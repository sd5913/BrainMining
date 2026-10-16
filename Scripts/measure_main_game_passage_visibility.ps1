$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$passageOutput = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/Validation/MainGamePassageExposure'
$passageRuntime = Get-Content -LiteralPath (Join-Path $passageOutput 'runtime.json') -Raw | ConvertFrom-Json
if (-not $passageRuntime.passed) { throw $passageRuntime.error }
$passageViews = foreach ($passageView in $passageRuntime.views) {
    $passageBitmap = [Drawing.Bitmap]::new($passageView.path)
    try {
        $passageSamples = 0; $passageLuma = 0.0; $passageBlack = 0; $passageWhite = 0
        for ($passageY = [int]($passageBitmap.Height * 0.20); $passageY -lt [int]($passageBitmap.Height * 0.65); $passageY += 6) {
            for ($passageX = [int]($passageBitmap.Width * 0.22); $passageX -lt [int]($passageBitmap.Width * 0.78); $passageX += 6) {
                $passagePixel = $passageBitmap.GetPixel($passageX,$passageY)
                $passageValue = 0.2126 * $passagePixel.R + 0.7152 * $passagePixel.G + 0.0722 * $passagePixel.B
                $passageLuma += $passageValue; $passageSamples++
                if ($passageValue -lt 4) { $passageBlack++ }
                if ($passagePixel.R -gt 245 -and $passagePixel.G -gt 245 -and $passagePixel.B -gt 245) { $passageWhite++ }
            }
        }
        [PSCustomObject]@{ id=$passageView.id; view=$passageView.view; mean_luma=$passageLuma/$passageSamples; black_fraction=$passageBlack/$passageSamples; clipped_fraction=$passageWhite/$passageSamples }
    } finally { $passageBitmap.Dispose() }
}
$passageFailed = @($passageViews | Where-Object { $_.mean_luma -lt 18 -or $_.black_fraction -gt 0.65 -or $_.clipped_fraction -gt 0.12 })
[PSCustomObject]@{ passed=$passageFailed.Count -eq 0; views=@($passageViews); failed=@($passageFailed); note='Central scene region excludes HUD. Both directions, before and after excavating each seam; dark distant cave openings are allowed.' } | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $passageOutput 'visibility.json') -Encoding utf8
$passageViews | Format-Table id,view,mean_luma,black_fraction,clipped_fraction
if ($passageFailed.Count -ne 0) { throw 'Some passage views are still black or overexposed.' }
