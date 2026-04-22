#ifndef ULTIMATE_CHUNK_PREDICTOR_H
#define ULTIMATE_CHUNK_PREDICTOR_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>

/**
 * 终极块大小预测器 v3.0
 *
 * 基于UltraThink深度分析的优化版本
 * 核心改进：
 * 1. 成本函数模型（循环+缓存+TLB）
 * 2. float/double专门处理
 * 3. 分段线性模型
 * 4. 硬件预取器感知
 */
class UltimateChunkPredictor {
public:
    struct SystemConfig {
        size_t L1_cache = 32 * 1024;
        size_t L2_cache = 256 * 1024;
        size_t cache_line = 64;
        size_t page_size = 4096;
        size_t tlb_entries = 64;

        double loop_overhead_cycles = 5.0;
        double L1_hit_cycles = 4.0;
        double L2_hit_cycles = 12.0;
        double mem_access_cycles = 200.0;
        double tlb_miss_cycles = 100.0;
    };

private:
    SystemConfig config;

    /**
     * 成本函数估计
     */
    double estimate_cost(size_t chunk_size,
                         size_t data_size,
                         size_t num_vectors,
                         size_t num_passes,
                         size_t element_size) const {

        // 组件1：外层循环开销
        double loop_cost = (static_cast<double>(data_size) / chunk_size)
                          * config.loop_overhead_cycles;

        // 组件2：缓存访问成本
        size_t working_set = chunk_size * num_vectors * element_size;
        double cache_cost = 0;

        if (working_set <= config.L1_cache / 2) {
            // L1命中
            cache_cost = data_size * num_passes * config.L1_hit_cycles;
        } else if (working_set <= config.L2_cache / 2) {
            // L2命中
            cache_cost = data_size * num_passes * config.L2_hit_cycles;
        } else {
            // 内存访问
            cache_cost = data_size * num_passes * config.mem_access_cycles * 0.5;
        }

        // 组件3：TLB成本
        double tlb_cost = 0;
        size_t bytes_per_vector = chunk_size * element_size;
        size_t pages_per_vector = (bytes_per_vector + config.page_size - 1)
                                  / config.page_size;
        size_t total_pages = pages_per_vector * num_vectors;

        if (total_pages > config.tlb_entries / 2) {
            // TLB失效
            double tlb_miss_rate = static_cast<double>(total_pages - config.tlb_entries/2)
                                  / total_pages;
            tlb_cost = data_size * num_passes * tlb_miss_rate * config.tlb_miss_cycles;
        }

        // 组件4：预取器效率惩罚
        double prefetch_penalty = 0;
        if (chunk_size < 16) {
            // 块太小，预取器效率低
            prefetch_penalty = data_size * num_passes * 2.0;
        } else if (chunk_size > 256) {
            // 块太大，时间局部性差
            prefetch_penalty = data_size * num_passes * 1.0;
        }

        return loop_cost + cache_cost + tlb_cost + prefetch_penalty;
    }

    /**
     * 基于成本函数选择最优块
     */
    size_t cost_based_selection(size_t data_size,
                                size_t num_vectors,
                                size_t num_passes,
                                size_t element_size) const {

        std::vector<size_t> candidates = {16, 24, 32, 48, 64, 96, 128, 192, 256};

        double min_cost = 1e20;
        size_t best_chunk = 64;

        for (size_t chunk : candidates) {
            double cost = estimate_cost(chunk, data_size, num_vectors,
                                       num_passes, element_size);
            if (cost < min_cost) {
                min_cost = cost;
                best_chunk = chunk;
            }
        }

        return best_chunk;
    }

    /**
     * 决策树模型（基于实验数据）
     */
    size_t decision_tree_predict(size_t data_size,
                                 size_t num_vectors,
                                 size_t num_passes,
                                 size_t element_size) const {

        size_t total_bytes = data_size * element_size * num_vectors;

        // float vs double 分支
        if (element_size == 4) {  // float
            if (total_bytes < 1 * 1024 * 1024) {  // < 1MB
                return (num_passes >= 5) ? 96 : 128;
            } else if (total_bytes < 10 * 1024 * 1024) {  // 1-10MB
                return (num_passes >= 5) ? 128 : 192;
            } else {  // > 10MB
                return (num_passes >= 7) ? 96 : 128;
            }
        } else {  // double (8字节)
            if (total_bytes < 500 * 1024) {  // < 500KB
                if (num_passes <= 2) return 192;
                return (num_passes >= 5) ? 96 : 128;
            } else if (total_bytes < 5 * 1024 * 1024) {  // 500KB-5MB
                if (num_passes <= 2) return 96;
                return (num_passes >= 5) ? 48 : 64;
            } else if (total_bytes < 50 * 1024 * 1024) {  // 5-50MB
                return (num_passes >= 5) ? 32 : 48;
            } else {  // > 50MB
                return (num_passes >= 7) ? 16 : 32;
            }
        }
    }

public:
    UltimateChunkPredictor() = default;
    explicit UltimateChunkPredictor(const SystemConfig& cfg) : config(cfg) {}

    /**
     * 主预测函数 - 混合策略
     *
     * 策略：成本模型 + 决策树的加权组合
     */
    size_t predict(size_t data_size,
                   size_t num_vectors,
                   size_t num_passes,
                   size_t element_size = 8) const {

        // 方法1：成本函数预测
        size_t cost_pred = cost_based_selection(data_size, num_vectors,
                                                num_passes, element_size);

        // 方法2：决策树预测
        size_t tree_pred = decision_tree_predict(data_size, num_vectors,
                                                 num_passes, element_size);

        // 混合策略：取几何平均（避免极端值）
        double geometric_mean = std::sqrt(static_cast<double>(cost_pred * tree_pred));
        size_t hybrid = static_cast<size_t>(geometric_mean);

        // 调整到标准值
        return round_to_preferred(hybrid);
    }

    /**
     * 保守预测（单一方法，更快）
     */
    size_t predict_conservative(size_t data_size,
                               size_t num_vectors,
                               size_t num_passes,
                               size_t element_size = 8) const {
        return decision_tree_predict(data_size, num_vectors, num_passes, element_size);
    }

    /**
     * 精确预测（评估多个候选）
     */
    std::vector<size_t> get_top_candidates(size_t data_size,
                                           size_t num_vectors,
                                           size_t num_passes,
                                           size_t element_size = 8) const {

        std::vector<std::pair<double, size_t>> scored_candidates;
        std::vector<size_t> all_candidates = {16, 24, 32, 48, 64, 96, 128, 192, 256, 384};

        for (size_t chunk : all_candidates) {
            double cost = estimate_cost(chunk, data_size, num_vectors,
                                       num_passes, element_size);
            scored_candidates.push_back({cost, chunk});
        }

        // 按成本排序
        std::sort(scored_candidates.begin(), scored_candidates.end());

        // 返回前3名
        std::vector<size_t> top3;
        for (size_t i = 0; i < std::min(size_t(3), scored_candidates.size()); ++i) {
            top3.push_back(scored_candidates[i].second);
        }

        return top3;
    }

private:
    size_t round_to_preferred(size_t chunk) const {
        static const std::vector<size_t> preferred = {
            16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512
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

// ============================================================================
// 便捷函数
// ============================================================================

/**
 * 快速预测（推荐用于生产环境）
 */
inline size_t ultimate_chunk_size(size_t data_size,
                                  size_t num_vectors,
                                  size_t num_passes,
                                  size_t element_size = 8) {
    static UltimateChunkPredictor predictor;
    return predictor.predict_conservative(data_size, num_vectors, num_passes, element_size);
}

/**
 * 精确预测（推荐用于性能关键代码）
 */
inline size_t ultimate_chunk_size_precise(size_t data_size,
                                          size_t num_vectors,
                                          size_t num_passes,
                                          size_t element_size = 8) {
    static UltimateChunkPredictor predictor;
    return predictor.predict(data_size, num_vectors, num_passes, element_size);
}

/**
 * 获取候选列表（用于运行时测试）
 */
inline std::vector<size_t> get_chunk_candidates(size_t data_size,
                                                size_t num_vectors,
                                                size_t num_passes,
                                                size_t element_size = 8) {
    static UltimateChunkPredictor predictor;
    return predictor.get_top_candidates(data_size, num_vectors, num_passes, element_size);
}

#endif // ULTIMATE_CHUNK_PREDICTOR_H
