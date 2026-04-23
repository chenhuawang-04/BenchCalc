param(
    [string]$Preset = "mt",
    [int]$ProcessRepeats = 3,
    [int]$BaseSeed = 20260423,
    [string]$BuildDir = "build",
    [string]$OutDir = "mt_matrix_out"
)

$ErrorActionPreference = "Stop"

$bin = if (Test-Path "$BuildDir/expr_method_bench.exe") {
    "$BuildDir/expr_method_bench.exe"
} else {
    "$BuildDir/expr_method_bench"
}

if (-not (Test-Path $bin)) {
    throw "expr_method_bench not found under $BuildDir"
}

New-Item -ItemType Directory -Force -Path "$OutDir/raw" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir/out" | Out-Null

$cases = @(
    "expr_chunkpipe_1m_f64",
    "expr_chunkpipe_1m_f32",
    "expr_chunkpipe_div_1m_f64",
    "expr_chunkpipe_subdiv_1m_f32",
    "expr_chunkpipe_8m_f32"
)

$hw = [Environment]::ProcessorCount
$threadList = @(1, 2, 4, 8)
if ($hw -gt 8) { $threadList += [Math]::Min(16, $hw) }
$threadList = $threadList | Sort-Object -Unique

for ($r = 1; $r -le $ProcessRepeats; $r++) {
    foreach ($caseId in $cases) {
        foreach ($th in $threadList) {
            $seed = $BaseSeed + $r * 101 + $th
            $csv = "$OutDir/raw/expr_${caseId}_t${th}_r${r}.csv"
            & $bin --preset $Preset --case $caseId --threads $th --seed $seed `
                --warmup 3 --iterations 20 --target-ms 180 --max-repeat 64 `
                --randomize-order --pin-cpu --high-priority `
                --csv $csv
        }
    }
}

python bench/tools/aggregate_expr.py --input-glob "$OutDir/raw/*.csv" --output-dir "$OutDir/out" --platform "local-mt"
Write-Host "Done. Summary: $OutDir/out/expr_matrix_summary.md"

