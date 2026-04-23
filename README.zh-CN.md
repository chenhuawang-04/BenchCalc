# BenchCalc 性能对比报告（中文）

中文版本请见本文件，默认英文版请见 [`README.md`](./README.md)。

---

## 概述

本仓库用于对比多种表达式执行路径（硬编码对照、chunk 非 JIT、VM、图执行器、GPU 动态核）在统一输入下的性能。

本次报告基于 CI 产物：

- 主要数据 run：`24814250081`
- 参考 run（workflow 升级，无性能代码变更）：`24814481683`

场景：

- `expr_chunkpipe_64k_f64`
- 表达式：`((a + b) * c) - d`
- 数据规模：`64000`
- `double` + `signed` 输入
- `threads=2`，`seed=42`

已完成框架补强（并未替换旧方案，而是并列增强）：

- 已接入子模块：`external/Melosyne_ThreadCenter`
- 新增 `benchmark-matrix.yml`（多 case / 多线程 / 多进程重复）
- CSV 增加 95% 置信区间字段
- 支持 `--pin-cpu` / `--high-priority` / 方法执行顺序随机化
- 并行执行运行时改为可复用线程池
- chunk/VM/graph 路径增加缓存与预分配优化
- `hardcoded_plain_inplace_ams` 去除冗余复制路径

新增多线程场景集：

- 预设：`--preset mt`
- 代表性线程数：`1 / 2 / 4 / 8 / 16(按机器上限裁剪)`
- 保持对照组与实验组并列，重点观察多线程扩展性与稳定性

---

## 关键结论

1. 本次 CI 场景下，**`hardcoded_plain_loop4` 为稳态与 E2E 双第一**。  
2. 非 JIT 动态方案中，`chunk_pipeline_nonjit_peak` 最接近硬编码上限，但仍有明显差距。  
3. `vm_register`、`graph_fused_kernellib` 在当前实现中明显慢于硬编码/chunk。  
4. GPU 动态核方法已纳入统一框架，但当前 CI runner 无 OpenCL 平台，因此显示 unavailable。  

详细表格、指标与方法学说明请查看英文默认报告：[`README.md`](./README.md)。
