# BenchCalc: Expression Execution Performance Report (CI Baseline)

> Last updated: 2026-04-23  
> Scope: rigorous comparison of expression execution methods (hardcoded baselines, non-JIT dynamic engines, VM, graph executor, GPU dynamic kernel path)

---

## Framework Upgrade Status (Applied)

The benchmark framework has been upgraded with the following rigor/performance improvements:

- added `Melosyne_ThreadCenter` as a git submodule (`external/Melosyne_ThreadCenter`)
- multi-case/multi-thread **matrix workflow** (`.github/workflows/benchmark-matrix.yml`)
- independent **process-level repeats** in CI matrix runs
- per-method **95% CI** fields exported to CSV (`ci95_low_ms`, `ci95_high_ms`, `ci95_half_ms`)
- optional runtime controls: `--pin-cpu`, `--high-priority`, randomized method execution order
- reusable thread runtime for `parallel_for` (reduces thread create/join overhead)
- chunk tuning cache + VM plan cache + graph generic preallocation
- optimized plain in-place baseline (removed redundant copy path)

These upgrades are additive; old and new methods remain shown in parallel.

---

## 1) Data Source and Scope

This report is based on CI artifacts from:

- Primary benchmark run (contains the current method set): **`24814250081`**
- Reference run (workflow-only upgrade, no performance-code change): **`24814481683`** (success)

CSV artifacts used:

- `cloud_artifacts_248142/expr-quick-ubuntu-gcc/expr_quick_ubuntu-gcc.csv`
- `cloud_artifacts_248142/expr-quick-ubuntu-clang/expr_quick_ubuntu-clang.csv`
- `cloud_artifacts_248142/expr-quick-macos-clang/expr_quick_macos-clang.csv`
- `cloud_artifacts_248142/expr-quick-windows-msvc/expr_quick_windows-msvc.csv`

---

## 2) Test Case and Runtime Parameters

From CI workflow:

```bash
expr_method_bench --preset quick --case expr_chunkpipe_64k_f64 --seed 42 --threads 2
```

Case details (`bench/expr/cases.hpp`):

- `case_id`: `expr_chunkpipe_64k_f64`
- Expression: `((a + b) * c) - d`
- Value type: `double`
- Data size: `64000`
- Input profile: `signed` (SignedWide)

Quick preset (non-strict):

- `warmup=1`
- `iterations=5`
- `target_ms=25`
- `max_repeat=16`

Multi-thread preset:

- `--preset mt`
- representative thread counts in CI matrix: `1 / 2 / 4 / 8 / 16(max available)`
- cases emphasize throughput and mixed operator patterns for multi-thread scaling

---

## 3) Methods Included (all shown in parallel)

### Hardcoded / control groups

- `hardcoded_plain_loop4`  
  Plain one-pass loop: `out[i] = ((a op1 b) op2 c) op3 d`
- `hardcoded_plain_inplace_ams`  
  In-place AMS hardcoded loop: `a[i] = (a[i] + b[i]) * c[i] - d[i]`
- `hardcoded_inline_exact_ams`  
  Inline fixed-operator AMS upper-bound reference
- `hardcoded_inline`  
  Existing inline hardcoded variant

### Non-JIT dynamic methods

- `chunk_pipeline_nonjit`
- `chunk_pipeline_nonjit_fixed256`
- `chunk_pipeline_nonjit_peak`
- `graph_fused_kernellib`
- `vm_register`

### GPU dynamic kernel methods

- `gpu_dynamic_kernel_peak`
- `gpu_dynamic_kernel_e2e`

---

## 4) Core Results (steady-state, lower is better)

Metric: `trimmed_mean_ms`

| method | macos-clang | ubuntu-clang | ubuntu-gcc | windows-msvc |
|---|---:|---:|---:|---:|
| hardcoded_plain_loop4 | 0.028681 | 0.035048 | 0.048557 | 0.065225 |
| hardcoded_plain_inplace_ams | 0.052762 | 0.249060 | 0.229298 | 0.078519 |
| hardcoded_inline_exact_ams | 0.044464 | 0.073619 | 0.068873 | 0.140246 |
| chunk_pipeline_nonjit_peak | 0.051069 | 0.073708 | 0.075101 | 0.169756 |
| chunk_pipeline_nonjit | 0.096384 | 0.178949 | 0.075087 | 0.235181 |
| chunk_pipeline_nonjit_fixed256 | 0.112815 | 0.217533 | 0.075655 | 0.239608 |
| hardcoded_inline | 0.112023 | 0.123014 | 0.101635 | 0.210794 |
| vm_register | 0.440663 | 0.370745 | 0.342663 | 0.537617 |
| graph_fused_kernellib | 0.254997 | 0.961222 | 0.981621 | 1.567680 |
| gpu_dynamic_kernel_peak | — | — | — | N/A (`no OpenCL platform`) |
| gpu_dynamic_kernel_e2e | — | — | — | N/A (`no OpenCL platform`) |

---

## 5) Relative Speed vs Plain Hardcoded Baseline

Baseline: `hardcoded_plain_loop4 = 1.0x`

| method | macos-clang | ubuntu-clang | ubuntu-gcc | windows-msvc |
|---|---:|---:|---:|---:|
| hardcoded_plain_loop4 | 1.000x | 1.000x | 1.000x | 1.000x |
| hardcoded_inline_exact_ams | 0.645x | 0.476x | 0.705x | 0.465x |
| chunk_pipeline_nonjit_peak | 0.562x | 0.475x | 0.647x | 0.384x |
| hardcoded_plain_inplace_ams | 0.544x | 0.141x | 0.212x | 0.831x |
| chunk_pipeline_nonjit | 0.298x | 0.196x | 0.647x | 0.277x |
| chunk_pipeline_nonjit_fixed256 | 0.254x | 0.161x | 0.642x | 0.272x |
| hardcoded_inline | 0.256x | 0.285x | 0.478x | 0.309x |
| vm_register | 0.065x | 0.095x | 0.142x | 0.121x |
| graph_fused_kernellib | 0.112x | 0.036x | 0.049x | 0.042x |

Cross-platform geometric mean (vs `hardcoded_plain_loop4`):

- `hardcoded_inline_exact_ams`: **0.563x**
- `chunk_pipeline_nonjit_peak`: **0.508x**
- `hardcoded_plain_inplace_ams`: **0.341x**
- `chunk_pipeline_nonjit`: **0.320x**
- `hardcoded_inline`: **0.322x**
- `chunk_pipeline_nonjit_fixed256`: **0.291x**
- `vm_register`: **0.101x**
- `graph_fused_kernellib`: **0.054x**

---

## 6) End-to-End (including prepare) Result

For all 4 platforms, `e2e_n1_ms / e2e_n10_ms / e2e_n100_ms` are consistently won by:

- **`hardcoded_plain_loop4`**

Why:

1. very low prepare overhead;
2. shortest hot loop path;
3. chunk/peak variants may approach in steady-state in some environments, but prepare costs are significantly higher.

---

## 7) Why Chunk Is Not the Winner in This CI Case

In this standardized CI case (`64k`, `f64`, `((a+b)*c)-d`), chunk methods are not the fastest.

Main reasons:

1. The baseline is truly hardcoded operator chain, enabling stronger compiler optimization.
2. `hardcoded_plain_loop4` performs one dispatch and then runs a very direct loop.
3. Chunk methods introduce extra scheduling/state overhead that is not fully amortized at `64k`.
4. `hardcoded_plain_inplace_ams` restores `a` before each timed run for reproducibility, so memory-copy overhead can dominate on some platforms.

---

## 8) GPU Status in Current CI

GPU methods are integrated into the framework but not benchmarked in this CI result set:

- Linux/macOS jobs do not run the Windows OpenCL dynamic-kernel path.
- Windows job reports:
  - `available=0`
  - `reason=no OpenCL platform`

So this CI result indicates **runner capability limitation**, not removal of the GPU method.

---

## 9) Practical Takeaways

For this CI baseline (`expr_chunkpipe_64k_f64`):

1. **CPU absolute upper bound**: `hardcoded_plain_loop4`
2. **Closest non-JIT dynamic method**: `chunk_pipeline_nonjit_peak` (still behind hardcoded baseline)
3. **VM and graph executor (current implementation)**: clearly slower than chunk/hardcoded group
4. **GPU dynamic kernel**: framework-ready, but needs a runner with actual OpenCL GPU support

---

## 10) Next Steps for More Scientific Coverage

To avoid one-case bias, expand the benchmark matrix:

1. Data scale: `64k / 1M / 8M`
2. Data type: `f32 / f64`
3. Expression family: no-division vs division-heavy
4. Threads: `1 / 2 / 4 / 8`
5. Keep all three CPU controls permanently:
   - `hardcoded_plain_loop4`
   - `hardcoded_plain_inplace_ams`
   - `hardcoded_inline_exact_ams`
6. Add a dedicated GPU-capable CI lane for `gpu_dynamic_kernel_*`

---

## 11) Reproduction Commands

Local:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j
./build/expr_method_bench --preset quick --case expr_chunkpipe_64k_f64 --seed 42 --threads 2 --csv local_expr.csv
```

Trigger CI:

```bash
gh workflow run ci-multi-platform.yml --ref master
```

Run full matrix benchmark workflow (recommended for scientific comparison):

```bash
gh workflow run benchmark-matrix.yml --ref master \
  -f process_repeats=3 -f preset=mt -f run_gpu_lane=false
```
