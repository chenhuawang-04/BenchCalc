#ifndef ADAPTIVE_CHUNK_SELECTOR_H
#define ADAPTIVE_CHUNK_SELECTOR_H

#include <vector>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/**
 * 自适应块大小选择器
 *
 * 核心思想：不依赖硬编码的经验值，而是在运行时动态测试多个候选块大小，
 * 选择实际性能最优的那个。
 *
 * 这个方法的优势：
 * 1. 适应任何硬件平台（Intel/AMD/ARM）
 * 2. 适应不同的编译器优化
 * 3. 考虑运行时的实际系统状态
 * 4. 不需要硬编码"魔法数字"
 */
class AdaptiveChunkSelector {
public:
    /**
     * 自动选择最优块大小
     *
     * @param compute_func  计算函数，签名: void(size_t start, size_t end)
     * @param data_size     数据规模（元素个数）
     * @param num_vectors   参与运算的向量数量
     * @param num_passes    每块内的遍历次数
     * @param element_size  元素字节数
     * @return 最优块大小
     */
    template<typename ComputeFunc>
    static size_t select_optimal(ComputeFunc compute_func,
                                  size_t data_size,
                                  size_t num_vectors,
                                  size_t num_passes,
                                  size_t element_size = 8) {

        // 步骤1：生成候选块大小
        std::vector<size_t> candidates = generate_candidates(
            data_size, num_vectors, num_passes, element_size
        );

        // 步骤2：测试每个候选块大小
        size_t best_chunk = candidates[0];
        double best_time = 1e9;

        // 使用小数据集快速测试
        size_t test_size = std::min(data_size, size_t(100000));

        for (size_t chunk : candidates) {
            double time = benchmark(compute_func, chunk, test_size);

            if (time < best_time) {
                best_time = time;
                best_chunk = chunk;
            }
        }

        return best_chunk;
    }

    /**
     * 获取理论建议的候选块大小列表
     *
     * 基于理论分析，不依赖实验数据
     */
    static std::vector<size_t> generate_candidates(size_t data_size,
                                                    size_t num_vectors,
                                                    size_t num_passes,
                                                    size_t element_size = 8) {
        std::vector<size_t> candidates;

        // 获取系统参数
        size_t L1_cache = get_L1_cache_size();
        size_t cache_line = 64;  // 几乎所有现代CPU

        // ===== 理论计算候选范围 =====

        // 1. 基于缓存容量的候选
        size_t working_set_per_element = num_vectors * element_size;
        size_t max_chunk_L1 = L1_cache / working_set_per_element / 2;  // 使用50%的L1

        // 2. 基于缓存行的候选
        size_t elements_per_line = cache_line / element_size;

        // 3. 根据复杂度调整范围
        size_t min_chunk, max_chunk;

        if (num_passes >= 5) {
            // 高复杂度：小块
            min_chunk = 16;
            max_chunk = std::min(size_t(256), max_chunk_L1);
        } else if (num_passes >= 3) {
            // 中等复杂度：中等块
            min_chunk = 32;
            max_chunk = std::min(size_t(512), max_chunk_L1);
        } else {
            // 低复杂度：可以使用大块
            min_chunk = 64;
            max_chunk = 2048;
        }

        // 根据数据规模进一步调整
        if (data_size >= 10000000) {      // > 10M
            max_chunk = std::min(max_chunk, size_t(128));
        } else if (data_size >= 1000000) { // > 1M
            max_chunk = std::min(max_chunk, size_t(256));
        }

        // ===== 生成候选列表 =====

        // 总是包含一些关键值
        std::vector<size_t> key_sizes = {16, 32, 48, 64, 96, 128, 192, 256, 384, 512};

        for (size_t size : key_sizes) {
            if (size >= min_chunk && size <= max_chunk) {
                candidates.push_back(size);
            }
        }

        // 确保至少有3个候选
        if (candidates.empty()) {
            candidates = {min_chunk, (min_chunk + max_chunk) / 2, max_chunk};
        }

        // 去重并排序
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        return candidates;
    }

    /**
     * 保守预测（不需要运行时测试）
     *
     * 基于理论分析给出一个"安全"的块大小，保证性能不会太差
     */
    static size_t conservative_predict(size_t data_size,
                                       size_t num_vectors,
                                       size_t num_passes,
                                       size_t element_size = 8) {

        size_t L1_cache = get_L1_cache_size();

        // 计算理论最大块大小（工作集适配L1的50%）
        size_t working_set_per_element = num_vectors * element_size;
        size_t theoretical_max = L1_cache / working_set_per_element / 2;

        // 基于复杂度的安全系数
        double safety_factor;
        if (num_passes >= 7) {
            safety_factor = 0.3;  // 超高复杂度：非常保守
        } else if (num_passes >= 5) {
            safety_factor = 0.4;  // 高复杂度：保守
        } else if (num_passes >= 3) {
            safety_factor = 0.6;  // 中等复杂度
        } else {
            safety_factor = 1.0;  // 低复杂度：可以激进
        }

        // 基于数据规模的调整
        if (data_size >= 10000000) {
            safety_factor *= 0.5;  // 超大数据：更保守
        } else if (data_size >= 1000000) {
            safety_factor *= 0.7;  // 大数据：稍保守
        }

        size_t chunk = static_cast<size_t>(theoretical_max * safety_factor);

        // 对齐到缓存行
        size_t cache_line = 64;
        size_t elements_per_line = cache_line / element_size;
        if (elements_per_line > 0) {
            chunk = ((chunk + elements_per_line - 1) / elements_per_line) * elements_per_line;
        }

        // 限制范围
        chunk = std::max(size_t(16), std::min(size_t(2048), chunk));

        // 调整到常用值
        std::vector<size_t> preferred = {16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048};
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

private:
    // 性能测试
    template<typename ComputeFunc>
    static double benchmark(ComputeFunc compute_func, size_t chunk_size, size_t test_size) {
        const int WARMUP = 1;
        const int ITERATIONS = 5;

        // 预热
        for (int i = 0; i < WARMUP; ++i) {
            for (size_t block = 0; block < test_size; block += chunk_size) {
                size_t end = std::min(block + chunk_size, test_size);
                compute_func(block, end);
            }
        }

        // 计时
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i) {
            for (size_t block = 0; block < test_size; block += chunk_size) {
                size_t end = std::min(block + chunk_size, test_size);
                compute_func(block, end);
            }
        }

        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = finish - start;

        return elapsed.count() / ITERATIONS;
    }

    // 获取L1缓存大小
    static size_t get_L1_cache_size() {
#ifdef _WIN32
        // Windows: 使用默认值
        return 32 * 1024;
#elif defined(__linux__)
        // Linux: 尝试从sysfs读取
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
#elif defined(__APPLE__)
        // macOS: sysctl
        size_t L1_size = 0;
        size_t size = sizeof(L1_size);
        if (sysctlbyname("hw.l1dcachesize", &L1_size, &size, NULL, 0) == 0) {
            return L1_size;
        }
        return 32 * 1024;
#else
        return 32 * 1024;  // 默认32KB
#endif
    }
};

/**
 * 便捷函数：自动选择最优块大小
 */
template<typename ComputeFunc>
inline size_t auto_select_chunk_size(ComputeFunc compute_func,
                                     size_t data_size,
                                     size_t num_vectors,
                                     size_t num_passes) {
    return AdaptiveChunkSelector::select_optimal(
        compute_func, data_size, num_vectors, num_passes
    );
}

/**
 * 便捷函数：获取保守预测（不需要运行时测试）
 */
inline size_t quick_chunk_size(size_t data_size,
                               size_t num_vectors,
                               size_t num_passes) {
    return AdaptiveChunkSelector::conservative_predict(
        data_size, num_vectors, num_passes
    );
}

#endif // ADAPTIVE_CHUNK_SELECTOR_H
