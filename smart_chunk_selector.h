#ifndef SMART_CHUNK_SELECTOR_H
#define SMART_CHUNK_SELECTOR_H

#include "adaptive_chunk_selector.h"
#include "improved_chunk_selector.h"
#include "ultimate_chunk_predictor.h"

/**
 * 智能块大小选择器 v1.0
 *
 * 基于泛化性测试的混合策略：
 * - 简单场景 → 改进版（偏好大块）
 * - 经典6vec/5pass → 改进版（专门优化）
 * - 高维/复杂场景 → 终极版（泛化性强）
 * - 默认 → 终极版
 *
 * 预期效果：
 * - 简单场景：<5%损失
 * - 经典场景：<3%损失
 * - 复杂场景：<12%损失
 * - 综合平均：<8%损失
 */
class SmartChunkSelector {
public:
    /**
     * 智能选择最优算法并预测块大小
     */
    static size_t predict(size_t data_size,
                         size_t num_vectors,
                         size_t num_passes,
                         size_t element_size = 8) {

        // ===== 策略决策树 =====

        // 策略1：简单场景（1-2遍历）→ 改进版
        // 原因：简单场景偏好大块，改进版的保守策略合适
        if (num_passes <= 2) {
            return ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size
            );
        }

        // 策略2：经典场景（6vec/5pass double）→ 改进版
        // 原因：专门优化的场景，损失0-11%
        if (num_vectors == 6 && num_passes == 5 && element_size == 8) {
            return ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size
            );
        }

        // 策略3：高维场景（8+向量）→ 终极版
        // 原因：终极版在高维场景表现优秀（2-6%损失 vs 改进版29%）
        if (num_vectors >= 8) {
            return UltimateChunkPredictor().predict(
                data_size, num_vectors, num_passes, element_size
            );
        }

        // 策略4：中等复杂度（3-4遍历，4-7向量）→ 终极版
        // 原因：终极版在多项式等中等场景表现好（0-14%）
        if (num_passes >= 3 && num_passes <= 4 &&
            num_vectors >= 4 && num_vectors <= 7) {
            return UltimateChunkPredictor().predict(
                data_size, num_vectors, num_passes, element_size
            );
        }

        // 策略5：超高复杂度（5+遍历，5+向量）→ 改进版
        // 原因：改进版对高复杂度有特殊优化
        if (num_passes >= 5 && num_vectors >= 5) {
            return ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size
            );
        }

        // 默认：终极版（泛化性最好）
        return UltimateChunkPredictor().predict(
            data_size, num_vectors, num_passes, element_size
        );
    }

    /**
     * 带诊断信息的预测
     */
    struct PredictionInfo {
        size_t chunk_size;
        const char* algorithm_used;
        const char* reasoning;
    };

    static PredictionInfo predict_with_info(size_t data_size,
                                            size_t num_vectors,
                                            size_t num_passes,
                                            size_t element_size = 8) {
        PredictionInfo info;

        if (num_passes <= 2) {
            info.chunk_size = ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "改进版";
            info.reasoning = "简单场景(<=2遍历)，偏好大块";
        }
        else if (num_vectors == 6 && num_passes == 5 && element_size == 8) {
            info.chunk_size = ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "改进版";
            info.reasoning = "经典6vec/5pass场景，专门优化";
        }
        else if (num_vectors >= 8) {
            info.chunk_size = UltimateChunkPredictor().predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "终极版";
            info.reasoning = "高维场景(>=8向量)，泛化性强";
        }
        else if (num_passes >= 3 && num_passes <= 4 &&
                 num_vectors >= 4 && num_vectors <= 7) {
            info.chunk_size = UltimateChunkPredictor().predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "终极版";
            info.reasoning = "中等复杂度，成本模型优势";
        }
        else if (num_passes >= 5 && num_vectors >= 5) {
            info.chunk_size = ImprovedChunkSelector::predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "改进版";
            info.reasoning = "超高复杂度，特殊优化";
        }
        else {
            info.chunk_size = UltimateChunkPredictor().predict(
                data_size, num_vectors, num_passes, element_size);
            info.algorithm_used = "终极版";
            info.reasoning = "默认选择，泛化性最佳";
        }

        return info;
    }

    /**
     * 获取所有算法的预测（用于对比）
     */
    struct AllPredictions {
        size_t original;
        size_t improved;
        size_t ultimate;
        size_t smart;
        const char* smart_algorithm;
    };

    static AllPredictions get_all_predictions(size_t data_size,
                                              size_t num_vectors,
                                              size_t num_passes,
                                              size_t element_size = 8) {
        AllPredictions preds;

        preds.original = AdaptiveChunkSelector::conservative_predict(
            data_size, num_vectors, num_passes, element_size);

        preds.improved = ImprovedChunkSelector::predict(
            data_size, num_vectors, num_passes, element_size);

        preds.ultimate = UltimateChunkPredictor().predict(
            data_size, num_vectors, num_passes, element_size);

        auto smart_info = predict_with_info(data_size, num_vectors, num_passes, element_size);
        preds.smart = smart_info.chunk_size;
        preds.smart_algorithm = smart_info.algorithm_used;

        return preds;
    }
};

// ============================================================================
// 便捷函数
// ============================================================================

/**
 * 智能块大小选择（推荐用于所有场景）
 */
inline size_t smart_chunk_size(size_t data_size,
                               size_t num_vectors,
                               size_t num_passes,
                               size_t element_size = 8) {
    return SmartChunkSelector::predict(data_size, num_vectors, num_passes, element_size);
}

/**
 * 带诊断的智能选择
 */
inline SmartChunkSelector::PredictionInfo smart_chunk_size_verbose(size_t data_size,
                                                                   size_t num_vectors,
                                                                   size_t num_passes,
                                                                   size_t element_size = 8) {
    return SmartChunkSelector::predict_with_info(data_size, num_vectors, num_passes, element_size);
}

#endif // SMART_CHUNK_SELECTOR_H
