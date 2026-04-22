param(
    [ValidateSet("quick","ci","full")]
    [string]$Preset = "ci",
    [switch]$Strict = $false,
    [string]$Csv = "expr_bench_result.csv"
)

$ErrorActionPreference = "Stop"

Write-Host "==> Configure"
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++

Write-Host "==> Build"
cmake --build build -j

$args = @("--preset", $Preset, "--csv", $Csv)
if ($Strict) { $args += "--strict" }

Write-Host "==> Run: .\build\expr_method_bench.exe $($args -join ' ')"
& .\build\expr_method_bench.exe @args
exit $LASTEXITCODE

