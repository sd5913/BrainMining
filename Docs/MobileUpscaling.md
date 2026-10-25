# Mobile upscaling

Android uses Arm ASR temporal upscaling on eligible mobile Vulkan devices.
Open the touch pause menu, choose the sliders icon, then select **自动 / 关闭 /
画质 / 均衡 / 性能**. Changes apply immediately and persist in
`Saved/Config/BrainMiningMobileGraphics.ini`.

New installations without a saved preference default to Auto, which selects
Quality on supported hardware. Existing choices are preserved. Corrupt numeric
settings fall back to Off. Off and unsupported devices restore the device
profile's original scene-resolution and anti-aliasing settings. The subsystem
is Android-only. PC settings are unaffected.

| Mode | Input dimension ratio | Redmi K60 verified input → output |
| --- | ---: | --- |
| Auto / Quality | 1 / 1.5 | 1067 × 481 → 1600 × 720 |
| Balanced | 1 / 1.7 | 942 × 424 → 1600 × 720 |
| Performance | 1 / 2 | 800 × 360 → 1600 × 720 |

The scene output is 1600 × 720 on the tested profile, not the phone's physical
2400 × 1080 display resolution. UI uses the existing separate UI composition
path. Presets invalidate temporal history; office AA ownership is coordinated
with the graphics subsystem. Auto is capability-based Quality selection, not a
dynamic FPS controller or a thermal controller.

## Device results, 2026-10-25

Redmi K60, Android 15, Adreno 730, Vulkan. ASR-02 was freshly cooked and installed;
the installed APK hash was checked before comparisons. Four idle fixtures were
sampled twice for 20 seconds per mode after five seconds of warmup, alternating
mode order. All modes used the same APK and camera fixtures.

| Scene | Off mean interval, two runs | Quality mean interval, two runs | Interpretation |
| --- | --- | --- | --- |
| Starting room | 16.67 / 16.67 ms | 16.67 / 16.67 ms | Both about 60 FPS |
| Office | 16.67 / 18.62 ms | 16.67 / 16.67 ms | Quality held about 60 FPS in these samples |
| Third mine | 18.90 / 18.87 ms | 16.67 / 16.67 ms | About 53 → 60 FPS |
| Museum | 25.30 / 25.03 ms | 24.98 / 24.64 ms | Both about 40 FPS; no substantial gain |

These FPS equivalents derive from game-loop intervals, not presentation timing.
Third-mine GPU time decreased from 17.69 / 17.70 ms to 13.44 / 13.44 ms;
game-loop p95 decreased from 35.16 / 34.74 ms to 17.83 / 17.62 ms. The museum's
remaining limitation needs separate profiling. Lower GPU time alone does not
establish its bottleneck. Thermal/frequency scaling was not controlled; battery
snapshots are retained. These results do not establish battery-life savings.

The raw `first-mine` label actually denotes the starting room, not active mining.
The reusable verification script now names it `start-room`.

## Validation and limits

- Windows Editor and Android Shipping builds, fresh mobile Vulkan shader cooking,
  packaged asset checks, and three automation tests passed: GraphicsPolicy,
  GraphicsPersistence and InputAndMenu under `BrainMining.Mobile`.
- Device telemetry confirms advancing ASR frames, valid temporal history, the
  actual input/output dimensions above, Off restoration and restart persistence.
- Touch checks cover opening the GM panel, returning to gameplay, selecting
  graphics settings, camera movement, pickaxe/drill use and actual excavation.
  Starting-room blocker cuts increased from 0 to 2; third-mine terrain cuts
  increased from 0 to 3. Camera, excavation and
  particle screenshots are retained in `interaction-asr-02`.
- Scene transitions and UI remained usable in the tested paths. Drill effects
  have bright jagged edges in some captures. Screenshot checks do not establish
  artifact-free motion, dense-smoke quality or a complete gameplay pass.
- OpenGL was tested on the same phone: ASR is unsupported, no ASR frames render,
  and selecting Quality retains the original resolution. No unsupported GPU or
  additional physical handset has been tested.
- The desktop reactive-mask path is disabled because mobile does not supply its
  desktop inputs. Translucent exhibits, dense particles and long play sessions
  remain useful follow-up image-quality coverage before a broad release.

## Reproducing checks

`Scripts/verify_mobile_upscaling.py --serial <serial> --apk <fresh-apk> --output
<new-directory>` runs repeated alternating-order comparisons. It verifies the
installed APK hash, isolates saves with `-MiningQA -UserDir=...`, and removes only
its own unchanged command-line override in a finally block before normal launch.
Do not clear player data or substitute an APK repacked from stale cooked content.

The opt-in QA transport accepts `mobile_graphics` modes 0..4. State includes
capabilities, CVars, actual ASR dimensions/frame count/history and GPU samples.
An empty GPU sample array means unavailable data, not zero cost.

Evidence root: `Saved/Validation/MobilePerformance-20261025/`:

- `comparison-asr-02/report.json`: 32 repeated samples and persistence result.
- `smoke-asr-02/`: actual rendering checks for every preset.
- `interaction-asr-02/`: touch, mining and camera checks; `restored.json` confirms
  normal startup restoration without clearing player data.
- `fallback-asr-02/`: OpenGL telemetry; the optional pause capture raced the
  opening animation, so use the final fallback pass for menu acceptance.
- `final-asr-03/`: fresh-profile Auto activation, native touch GM entry and
  graphics selection, restart persistence and a final third-mine comparison.
  Off / Auto mean game-loop intervals were 18.63 / 16.74 ms; GPU times were
  17.35 / 13.51 ms in these 15-second samples. This is a targeted regression
  check after UI/default changes, not a replacement for the repeated benchmark.
- `final-fallback-asr-03/`: OpenGL fallback, disabled preset buttons, unchanged
  resolution and AA even after a direct Quality request. Both final runs have
  successful `report.json` and `restored.json` records.
- `Tests-*/Automation/index.json`: automation reports.

Final APK: `Builds/MobileVerticalSlice/AndroidSizeOptimized-20261025-ASR-03/BrainMining-Android-Shipping-arm64.apk`,
950,063,476 bytes (906.05 MiB). SHA-256:
`E13C90A1668C90536E79855460924D4E9FEBA752A2332495BF585A36E1CBA1E5`.
Fresh cook and all package-integrity checks passed; `adb install -r` retained
player data, and the installed APK hash matched. Package evidence is under
`Saved/Validation/SizeOptimization/AndroidSizeOptimized-20261025-ASR-03/`.
Final editor/native build logs and `Tests-20261025-110806/Automation/index.json`
are under the evidence root; all three tests passed without warnings/failures.
The owned QA override was removed and normal startup restored.

Upstream revision, MIT license and UE 5.8 port details are recorded in
`Plugins/ArmASR/UPSTREAM.md`.
