$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$roomRoot = Split-Path $PSScriptRoot -Parent
$roomOutput = Join-Path $roomRoot 'Saved/Validation/MainGameRoomEntrances'
$roomReport = Get-Content -LiteralPath (Join-Path $roomOutput 'exposure_runtime.json') -Raw | ConvertFrom-Json
if (-not $roomReport.passed) { throw 'The runtime exposure fixture has not passed.' }
function Measure-RoomView([string]$ImagePath) {
    $roomBitmap = [System.Drawing.Bitmap]::new($ImagePath)
    try {
        $roomSamples = 0; $roomLuma = 0.0; $roomWhite = 0
        for ($roomY = [int]($roomBitmap.Height * 0.18); $roomY -lt [int]($roomBitmap.Height * 0.62); $roomY += 6) {
            for ($roomX = [int]($roomBitmap.Width * 0.18); $roomX -lt [int]($roomBitmap.Width * 0.82); $roomX += 6) {
                $roomPixel = $roomBitmap.GetPixel($roomX, $roomY)
                $roomLuma += 0.2126 * $roomPixel.R + 0.7152 * $roomPixel.G + 0.0722 * $roomPixel.B
                if ($roomPixel.R -gt 245 -and $roomPixel.G -gt 245 -and $roomPixel.B -gt 245) { $roomWhite++ }
                $roomSamples++
            }
        }
        return [PSCustomObject]@{ mean_luma = $roomLuma / $roomSamples; clipped_fraction = $roomWhite / $roomSamples; samples = $roomSamples }
    } finally { $roomBitmap.Dispose() }
}
$roomComparisons = foreach ($roomPair in $roomReport.pairs) {
    $roomBefore = Measure-RoomView $roomPair.before
    $roomAfter = Measure-RoomView $roomPair.after
    [PSCustomObject]@{ id = $roomPair.id; before = $roomBefore; after = $roomAfter; luma_ratio = $roomBefore.mean_luma / [Math]::Max($roomAfter.mean_luma, 1); clipped_difference = $roomBefore.clipped_fraction - $roomAfter.clipped_fraction }
}
$roomImagesPassed = @($roomComparisons | Where-Object { [Math]::Abs($_.luma_ratio - 1) -gt 0.05 -or $_.before.clipped_fraction -gt 0.05 -or $_.after.clipped_fraction -gt 0.05 }).Count -eq 0
[PSCustomObject]@{ passed = $roomImagesPassed; comparisons = @($roomComparisons); note = 'Identical camera; mean brightness changes at most 5 percent and white clipping stays below 5 percent. Source lighting and encounter policy are preserved.' } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $roomOutput 'image_comparison.json') -Encoding utf8
if (-not $roomImagesPassed) { throw 'Room exposure image comparison failed.' }
$roomComparisons | Format-Table id,luma_ratio,clipped_difference
