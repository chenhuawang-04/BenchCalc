# BenchCalc 性能报告（中文，UTF-8）

默认报告为英文版：[`README.md`](./README.md)。  
本文件给出中文结论与关键数据摘要。

---

## 1) 本次分析基于哪些 CI 数据

- 最新主分析 run：`24816296924`（2026-04-23，success）
- 参考 run：`24815996080`、`24814250081`

使用产物目录：

- `cloud_artifacts_24816296924/expr-quick-*/expr_quick_*.csv`
- `cloud_artifacts_24816296924/expr-quick-*/mt_raw/*.csv`
- `cloud_artifacts_24816296924/expr-quick-*/mt_out/expr_matrix_summary.csv`

---

## 2) 当前测试包含什么

### quick 快照

- case：`expr_chunkpipe_64k_f64`
- 表达式：`((a + b) * c) - d`
- 类型：`double`
- 线程：`2`

### 多线程代表性场景（本次新增）

- case：`expr_chunkpipe_1m_f64`
- 线程：`1 / 2 / 4 / 8`（CI 会按机器条件扩展）
- 对照组与实验组并列，不替换旧方案

并且本仓库已接入子模块：

- `external/Melosyne_ThreadCenter`

---

## 3) quick 场景关键结论

在 `expr_chunkpipe_64k_f64` 下（`trimmed_mean_ms` 越小越好）：

- macOS / Ubuntu-Clang / Ubuntu-GCC：`hardcoded_plain_loop4` 仍是最快
- Windows-MSVC：`hardcoded_inline_exact_ams` 最快（约 1.47x 于 plain_loop4）

这说明不同编译器/平台下，最优硬编码路径会有差异，但整体上“硬编码对照组”仍是上界参考。

---

## 4) 多线程场景关键结论（`expr_chunkpipe_1m_f64`）

### 每个平台、每个线程的最佳方法（简表）

| 平台 | t1 | t2 | t4 | t8 |
|---|---|---|---|---|
| macos-clang | hardcoded_plain_loop4 | hardcoded_plain_loop4 | hardcoded_plain_inplace_ams | hardcoded_plain_loop4 |
| ubuntu-clang | hardcoded_inline_exact_ams | hardcoded_plain_loop4 | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams |
| ubuntu-gcc | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams |
| windows-msvc | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams | hardcoded_inline_exact_ams |

### 线程扩展性（几何平均，t1=1.0x）

| 方法 | t1 | t2 | t4 | t8 |
|---|---:|---:|---:|---:|
| hardcoded_plain_loop4 | 1.000x | 0.999x | 0.934x | 0.987x |
| hardcoded_inline_exact_ams | 1.000x | 1.075x | 1.466x | 1.481x |
| chunk_pipeline_nonjit_peak | 1.000x | 1.098x | 1.421x | 1.442x |
| vm_register | 1.000x | 1.237x | 2.265x | 2.209x |
| graph_fused_kernellib | 1.000x | 1.132x | 1.810x | 1.881x |

解释：

1. `hardcoded_plain_loop4` 是单循环硬编码对照，设计上不追求线程扩展。  
2. `hardcoded_inline_exact_ams` 在多线程下综合表现最强。  
3. `chunk_pipeline_nonjit_peak` 有可见扩展性，但绝对值仍落后于最优硬编码路径。  
4. VM / 图执行器扩展比率不差，但绝对时延仍偏高。

---

## 5) GPU 结果说明

本次 Windows artifact 中 GPU 两项均 unavailable：

- `gpu_dynamic_kernel_peak`
- `gpu_dynamic_kernel_e2e`
- reason：`no OpenCL platform`

因此本次无法给出 GPU 性能结论，这属于 runner 能力限制，不是功能被移除。

---

## 6) 严谨性说明（非常重要）

本次 `ci-multi-platform` 的多线程代表场景，每个 `(平台,线程)` 在 summary 里通常是 `samples=1`（进程级）。  
因此它更适合“趋势判断”，不适合做最终显著性结论。

若需要论文级/发布级结论，请跑：

- `benchmark-matrix.yml`
- `process_repeats >= 3`（建议 5）

---

## 7) UTF-8 约定

已统一：

- CSV 输出：UTF-8 + BOM（便于 Excel）
- 聚合读取：`utf-8-sig`
- README：UTF-8 编码

---

## 8) 复现命令

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j
.\build\expr_method_bench.exe --preset quick --case expr_chunkpipe_64k_f64 --threads 2 --seed 42 --csv local_quick.csv
```

多线程本地矩阵：

```powershell
powershell -ExecutionPolicy Bypass -File bench/run_expr_mt_matrix.ps1 -Preset mt -ProcessRepeats 3
```

