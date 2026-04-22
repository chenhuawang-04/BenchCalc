# 统一 Bench 框架（严谨重构版）

这个目录将原来分散在多个 `*_test.cpp` 的性能测试，统一为一套可复现、可比较、可导出的基准框架。

---

## 目标

1. **统一方法论**：固定 warmup、迭代数、统计口径（mean/median/p95/cv/trimmed mean）。
2. **统一输入生成**：固定随机种子，保证可复现。
3. **统一对比对象**：同一组 case 下比较 `original / improved / ultimate / smart` 四个预测算法。
4. **统一输出**：人类可读表格 + CSV 机器可读结果。
5. **质量门禁**：可选 `--strict`，将性能和稳定性阈值转成 CI 可判定结果。
6. **抗抖动测量**：采用 blocked round-robin + 自动 repeat（按目标样本时长）降低噪声。

---

## 文件结构

- `chunk_bench_core.h`
  - workload 建模（classic / addsub / poly / reduction / matrix-like）
  - 可复现实验场景（`VectorScenario<T>`）
  - 计时统计（`TimingStats`）
  - 自动 repeat 校准（`target_sample_ms`）
  - blocked round-robin 测量执行器（chunk 顺序可随机化）
  - 算法预测收集与候选块生成
  - 单 case 执行逻辑（`run_case<T>`）

- `chunk_bench_main.cpp`
  - CLI 入口
  - case matrix（quick / ci / full）
  - 结果打印、CSV 导出、质量门禁

---

## 构建

### CMake（推荐）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j
```

产物：

- `build/chunk_bench.exe`
- `build/expr_method_bench.exe`

可选 smoke 测试：

```powershell
ctest --test-dir build --output-on-failure
```

---

## 使用

### 快速模式

```powershell
.\build\chunk_bench.exe --preset quick
```

### 全量模式（默认）

```powershell
.\build\chunk_bench.exe --preset full
```

### 只跑部分 case

```powershell
.\build\chunk_bench.exe --case classic_1m
```

### 导出 CSV

```powershell
.\build\chunk_bench.exe --preset ci --csv bench_result.csv
```

### 严格门禁（适合 CI）

```powershell
.\build\chunk_bench.exe --preset ci --strict
```

### 一键脚本（PowerShell）

```powershell
.\bench\run_bench.ps1 -Preset ci -Strict -Csv bench_result_ci.csv
```

---

## 表达式引擎性能基准（新增）

用于验证“用户通过 JSON 表达式提交公式”的三种高性能执行路径：

- 图执行器 + 融合内核库（CPU）
- VM（寄存器解释执行）
- GPU 动态核（OpenCL，含 peak/e2e 两种口径）

### 运行

```powershell
.\build\expr_method_bench.exe --preset quick --csv expr_bench_quick.csv
.\build\expr_method_bench.exe --preset ci --strict --csv expr_bench_ci.csv
```

### 输出说明

- `gpu_dynamic_kernel_peak`：仅核执行（不含每轮回传），代表 GPU 峰值吞吐能力
- `gpu_dynamic_kernel_e2e`：包含每轮回传，代表端到端时延
- 若设备不支持某精度（例如缺少 `cl_khr_fp64`），会标记为 unavailable 并给出原因

### 调整测量强度（推荐在噪声环境）

```powershell
.\build\chunk_bench.exe --preset ci --target-ms 60 --max-repeat 16
```

---

## 统计口径

每个 chunk 的统计包含：

- `mean_ms`
- `median_ms`
- `p95_ms`
- `stdev_ms`
- `cv_percent`
- `trimmed_mean_ms`（去掉一个最小值和一个最大值）

**best chunk** 默认按 `trimmed_mean_ms` 选择，避免偶发尖峰影响结果。

此外，单个样本并不是“只跑 1 次 chunk”，而是按以下逻辑自动重复：

- 先用校准 chunk 估算单次耗时；
- 计算 `repeat_per_sample`，使样本总时长接近 `target_sample_ms`；
- 每个样本最终记录“平均每次耗时”。

这样可以显著降低短基准（<5ms）抖动。

---

## 严格模式门禁（当前阈值）

- checksum 一致性必须通过；
- smart 算法损失（默认）：
  - 常规 case `<= 20%`
  - `classic_6vec_5pass` 且 `data_size >= 1M` 的大规模场景 `<= 25%`
- best chunk 的 `cv <= 12%`。

可按团队要求在 `chunk_bench_main.cpp` 中调整阈值。

---

## 为什么这版更“严谨”

相比原来多份 ad-hoc 测试程序，这套框架统一了：

- 随机性控制；
- 计时流程（warmup + blocked measured loops）；
- 自动 repeat 时长校准（避免样本过短）；
- 候选块和算法对比逻辑；
- 输出与门禁格式；
- 复现路径（CLI + preset + seed）。

因此可以直接拿来做：

- 算法回归对比；
- CI 性能守护；
- 参数调优实验记录。
