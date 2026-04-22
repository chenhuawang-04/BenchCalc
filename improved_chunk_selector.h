#ifndef IMPROVED_CHUNK_SELECTOR_H
#define IMPROVED_CHUNK_SELECTOR_H

#include <vector>
#include <algorithm>
#include <cstddef>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

/**
 * 改进版块大小选择器 v2.0
 *
 * 修复问题：
 * 1. float类型预测不准（基于字节数而非元素数）
 * 2. 1M规模过于保守（更激进的策略）
 * 3. 添加TLB约束检查
 */
class ImprovedChunkSelector {
public:
    /**
     * 改进的保守预测
     */
    static size_t predict(size_t data_size,
                         size_t num_vectors,
                         size_t num_passes,
                         size_t element_size = 8) {

        // 步骤1：获取L1缓存大小
        size_t L1_cache = get_L1_cache_size();

        // 步骤2：计算理论最大块大小
        size_t working_set_per_element = num_vectors * element_size;
        size_t theoretical_max = L1_cache / working_set_per_element / 2;

        // 步骤3：基于复杂度的安全系数
        double safety_factor;
        if (num_passes >= 10) {
            safety_factor = 0.25;  // 极高复杂度
        } else if (num_passes >= 7) {
            safety_factor = 0.3;
        } else if (num_passes >= 5) {
            safety_factor = 0.4;
        } else if (num_passes >= 3) {
            safety_factor = 0.6;
        } else {
            safety_factor = 1.0;
        }

        // 步骤4：基于总内存占用的调整（修复：使用字节数而非元素数）
        size_t total_bytes = data_size * element_size * num_vectors;

        if (total_bytes >= 100 * 1024 * 1024) {        // >= 100MB
            safety_factor *= 0.3;
        } else if (total_bytes >= 50 * 1024 * 1024) {  // >= 50MB
            safety_factor *= 0.4;
        } else if (total_bytes >= 10 * 1024 * 1024) {  // >= 10MB
            safety_factor *= 0.5;
        } else if (total_bytes >= 5 * 1024 * 1024) {   // >= 5MB
            safety_factor *= 0.6;
        } else if (total_bytes >= 1 * 1024 * 1024) {   // >= 1MB
            safety_factor *= 0.7;
        } else if (total_bytes < 100 * 1024) {         // < 100KB
            safety_factor *= 1.5;  // 小数据，可以用大块
        }

        size_t chunk = static_cast<size_t>(theoretical_max * safety_factor);

        // 步骤5：TLB约束检查（新增）
        const size_t PAGE_SIZE = 4096;
        const size_t TLB_BUDGET = 32;  // 使用32个TLB条目

        size_t bytes_per_vector = chunk * element_size;
        size_t pages_per_vector = (bytes_per_vector + PAGE_SIZE - 1) / PAGE_SIZE;
        size_t total_pages = pages_per_vector * num_vectors;

        if (total_pages > TLB_BUDGET) {
            // 缩小块大小以满足TLB约束
            size_t max_bytes_per_vector = (TLB_BUDGET / num_vectors) * PAGE_SIZE;
            size_t max_chunk_from_tlb = max_bytes_per_vector / element_size;
            chunk = std::min(chunk, max_chunk_from_tlb);
        }

        // 步骤6：特殊规则优化（基于测试结果）

        // 规则1：大数据集高复杂度，强制使用小块
        if (data_size >= 1000000 && num_passes >= 5) {
            chunk = std::min(chunk, size_t(48));
        }

        // 规则2：多向量场景，考虑缓存关联性
        if (num_vectors >= 8) {
            chunk = static_cast<size_t>(chunk * 0.8);
        }

        // 规则3：极高复杂度，不要过度保守
        if (num_passes >= 10 && chunk < 48) {
            chunk = 48;  // 最小48，避免循环开销过大
        }

        // 步骤7：对齐到缓存行
        size_t cache_line = 64;
        size_t elements_per_line = cache_line / element_size;
        if (elements_per_line > 0) {
            chunk = ((chunk + elements_per_line - 1) / elements_per_line) * elements_per_line;
        }

        // 步骤8：范围限制
        chunk = std::max(size_t(16), std::min(size_t(2048), chunk));

        // 步骤9：调整到常用值
        return round_to_preferred(chunk);
    }

private:
    static size_t get_L1_cache_size() {
#ifdef _WIN32
        return 32 * 1024;
#elif defined(__linux__)
        FILE* f = fopen("/sys/devices/system/cpu/cpu0/cache/index0/size", "r");
        if (f) {
            size_t size;
            char unit;
            if (fscanf(f, "%zu%c", &size, &unit) == 2) {
                fclose(f);
                if (unit == 'K') return size * 1024;
                if (unit == 'M') return size * 1024 * 1024;
            }
            fclose(f);
        }
        return 32 * 1024;
#else
        return 32 * 1024;
#endif
    }

    static size_t round_to_preferred(size_t chunk) {
        static const std::vector<size_t> preferred = {
            16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048
        };

        size_t best = preferred[0];
        size_t min_diff = std::abs(static_cast<long>(chunk) - static_cast<long>(best));

        for (size_t p : preferred) {
            size_t diff = std::abs(static_cast<long>(chunk) - static_cast<long>(p));
            if (diff < min_diff) {
                min_diff = diff;
                best = p;
            }
        }

        return best;
    }
};

/**
 * 便捷函数
 */
inline size_t improved_chunk_size(size_t data_size,
                                  size_t num_vectors,
                                  size_t num_passes,
                                  size_t element_size = 8) {
    return ImprovedChunkSelector::predict(data_size, num_vectors, num_passes, element_size);
}

#endif // IMPROVED_CHUNK_SELECTOR_H
