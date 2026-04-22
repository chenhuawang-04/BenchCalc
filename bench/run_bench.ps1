param(
    [ValidateSet("quick","ci","full")]
    [string]$Preset = "ci",
    [switch]$Strict = $false,
    [string]$Csv = "",
    [int]$Warmup = -1,
    [int]$Iterations = -1,
    [double]$TargetMs = -1,
    [int]$MaxRepeat = -1
)

$ErrorActionPreference = "Stop"

Write-Host "==> Configure"
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++

Write-Host "==> Build"
cmake --build build -j

$args = @("--preset", $Preset)
if ($Strict) { $args += "--strict" }
if ($Csv -ne "") { $args += @("--csv", $Csv) }
if ($Warmup -ge 0) { $args += @("--warmup", $Warmup) }
if ($Iterations -ge 0) { $args += @("--iterations", $Iterations) }
if ($TargetMs -gt 0) { $args += @("--target-ms", $TargetMs) }
if ($MaxRepeat -gt 0) { $args += @("--max-repeat", $MaxRepeat) }

Write-Host "==> Run: .\build\chunk_bench.exe $($args -join ' ')"
& .\build\chunk_bench.exe @args
exit $LASTEXITCODE

