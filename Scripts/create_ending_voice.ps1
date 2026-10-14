# Reproducible Chinese placeholder dialogue; replace with the team's recording later.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$endingAudio = Join-Path (Split-Path $PSScriptRoot -Parent) 'SourceAssets/EndingEmergence/Audio'
$endingVoice = New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
    $endingVoice.SelectVoice('Microsoft Huihui Desktop')
    $endingVoice.Rate = 0
    foreach ($line in @(
        @{ Name = 'BlockedThought'; Text = '唉，脑子堵住了，涌现不出来了。' },
        @{ Name = 'ClearMyHead'; Text = '好希望能有东西把我脑子疏通一下啊。' }
    )) {
        $endingVoice.SetOutputToWaveFile((Join-Path $endingAudio ('S_EE_' + $line.Name + '.wav')))
        $endingVoice.Speak($line.Text)
        $endingVoice.SetOutputToNull()
    }
} finally { $endingVoice.Dispose() }
