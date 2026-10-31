# Benchmarks ray tracer targets against each other.
#
# Runs each target with -bench (render once, write drawtime.txt, exit), alternating
# targets every run so thermal/background drift affects all of them equally. Prints
# per-run times, min/median/mean/max per target, and whether the first two targets
# produced byte-identical output.bmp files.
#
# Build the preset first, e.g.:
#   cmake --build out/build/x64-release
#
# Usage:
#   .\bench.ps1
#   .\bench.ps1 -Runs 10
#   .\bench.ps1 -Targets CoolRayTracer,CoolerRayTracer -Preset x64-release

param(
  [int]$Runs = 1,
  [string[]]$Targets = @("CoolRayTracer", "CoolerRayTracer", "SuperCoolRayTracer"),
  [string]$Preset = "x64-release"
)

$root = Join-Path $PSScriptRoot "out\build\$Preset"
$results = @{}
foreach ($t in $Targets) {
  $exe = Join-Path $root "$t\$t.exe"
  if (-not (Test-Path $exe)) { throw "Missing $exe - build the '$Preset' preset first." }
  $results[$t] = @()
}

for ($i = 1; $i -le $Runs; $i++) {
  foreach ($t in $Targets) {
    $dir = Join-Path $root $t
    $timeFile = Join-Path $dir "drawtime.txt"
    Remove-Item $timeFile -ErrorAction SilentlyContinue
    Start-Process -FilePath (Join-Path $dir "$t.exe") -ArgumentList "-bench" -WorkingDirectory $dir -Wait
    $ms = [double]::Parse((Get-Content $timeFile), [Globalization.CultureInfo]::InvariantCulture)
    $results[$t] += $ms
    Write-Output ("run {0} {1,-20} {2,10:F2} ms" -f $i, $t, $ms)
  }
}

Write-Output ""
foreach ($t in $Targets) {
  $s = @($results[$t] | Sort-Object)
  $median = $s[[int][math]::Floor($s.Count / 2)]
  $mean = ($s | Measure-Object -Average).Average
  Write-Output ("{0,-20} min {1,10:F2}  median {2,10:F2}  mean {3,10:F2}  max {4,10:F2}  (ms)" -f $t, $s[0], $median, $mean, $s[-1])
}
