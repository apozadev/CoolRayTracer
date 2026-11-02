# Benchmarks ray tracer targets against each other.
#
# Runs each target with -bench (render once, write drawtime.txt, exit), alternating
# targets every run so thermal/background drift affects all of them equally. Prints
# per-run times, min/median/mean/max per target, and whether all targets rendered
# the same image.
#
# -Spheres sweeps the scene size, to see what happens once the scene no longer fits in
# cache. Bigger scenes cover a bigger area at the same density. The image shrinks as the
# scene grows, so pixels x spheres (the amount of work) stays close to the default
# 1280x720 frame with 80 spheres, and tiles shrink so every thread still gets some.
# Compare targets within one sphere count: the work differs a little between counts.
#
# Build the preset first, e.g.:
#   cmake --build out/build/x64-release
#
# Usage:
#   .\bench.ps1
#   .\bench.ps1 -Runs 10
#   .\bench.ps1 -Threads 16          (0 = all logical processors)
#   .\bench.ps1 -Targets CoolRayTracer,CoolerRayTracer -Preset x64-release
#   .\bench.ps1 -Spheres 80,1000,10000,100000,1000000 -Runs 3 -Threads 0

param(
  [int]$Runs = 1,
  [string[]]$Targets = @("OkayRayTracer", "CoolRayTracer", "CoolerRayTracer", "SuperCoolRayTracer", "MegaCoolRayTracer"),
  [string]$Preset = "x64-release",
  [int]$Threads = 1,
  [int[]]$Spheres = @(80)
)

$root = Join-Path $PSScriptRoot "out\build\$Preset"
foreach ($t in $Targets) {
  $exe = Join-Path $root "$t\$t.exe"
  if (-not (Test-Path $exe)) { throw "Missing $exe - build the '$Preset' preset first." }
}

# Image and tile size for a sphere count: pixels x spheres stays close to 1280x720 x 80,
# and tiles shrink until there are at least 64 of them
function Get-RenderSize([int]$SphereCount) {
  $scale = [math]::Min(1.0, [math]::Sqrt(80.0 / [math]::Max(1, $SphereCount)))
  $width = [math]::Max(1, [int][math]::Round(1280 * $scale))
  $height = [math]::Max(1, [int][math]::Round(720 * $scale))
  $tile = 32
  while ($tile -gt 1 -and [math]::Ceiling($width / $tile) * [math]::Ceiling($height / $tile) -lt 64) {
    $tile = [int]($tile / 2)
  }
  return [pscustomobject]@{ Width = $width; Height = $height; Tile = $tile }
}

$sweep = @()
foreach ($n in $Spheres) {
  $size = Get-RenderSize $n
  $exeArgs = "-bench -threads $Threads -spheres $n -width $($size.Width) -height $($size.Height) -tile $($size.Tile)"
  if ($Spheres.Count -gt 1) {
    Write-Output ("=== {0} spheres: {1}x{2} image, {3} px tiles ===" -f $n, $size.Width, $size.Height, $size.Tile)
  }

  $results = @{}
  $hashes = @{}
  foreach ($t in $Targets) { $results[$t] = @() }

  for ($i = 1; $i -le $Runs; $i++) {
    foreach ($t in $Targets) {
      $dir = Join-Path $root $t
      $timeFile = Join-Path $dir "drawtime.txt"
      Remove-Item $timeFile -ErrorAction SilentlyContinue
      Start-Process -FilePath (Join-Path $dir "$t.exe") -ArgumentList $exeArgs -WorkingDirectory $dir -Wait
      $ms = [double]::Parse((Get-Content $timeFile), [Globalization.CultureInfo]::InvariantCulture)
      $results[$t] += $ms
      $hashes[$t] = (Get-FileHash (Join-Path $dir "output.bmp")).Hash
      Write-Output ("run {0} {1,-20} {2,10:F2} ms" -f $i, $t, $ms)
    }
  }

  Write-Output ""
  $medians = @{}
  foreach ($t in $Targets) {
    $s = @($results[$t] | Sort-Object)
    $median = $s[[int][math]::Floor($s.Count / 2)]
    $mean = ($s | Measure-Object -Average).Average
    $medians[$t] = $median
    Write-Output ("{0,-20} min {1,10:F2}  median {2,10:F2}  mean {3,10:F2}  max {4,10:F2}  (ms)" -f $t, $s[0], $median, $mean, $s[-1])
  }

  $sameImage = @($hashes.Values | Select-Object -Unique).Count -eq 1
  Write-Output ("Same image from every target: {0}" -f $(if ($sameImage) { "yes" } else { "NO" }))
  Write-Output ""

  $sweep += [pscustomobject]@{ Spheres = $n; Size = $size; Medians = $medians; SameImage = $sameImage }
}

if ($Spheres.Count -gt 1) {
  Write-Output ("Median ms per target (speedup vs {0})" -f $Targets[0])
  $header = "{0,9}  {1,-9}" -f "spheres", "image"
  foreach ($t in $Targets) { $header += "  {0,22}" -f $t }
  Write-Output $header
  foreach ($row in $sweep) {
    $line = "{0,9}  {1,-9}" -f $row.Spheres, ("{0}x{1}" -f $row.Size.Width, $row.Size.Height)
    $baseline = $row.Medians[$Targets[0]]
    foreach ($t in $Targets) {
      $line += "  {0,22}" -f ("{0:F1} ({1:F2}x)" -f $row.Medians[$t], ($baseline / $row.Medians[$t]))
    }
    if (-not $row.SameImage) { $line += "  IMAGES DIFFER" }
    Write-Output $line
  }
}
