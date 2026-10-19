# TapPlay startup compatibility

The October 19, 2026 report records startup failures on vivo Y70s (V2002A),
Redmi K30 Ultra (M2006J10C), and Huawei nova 5i (GLK-AL00), all on Android 10.
The other two devices passed (Android 13 and 15).

## Evidence and limits

- All three game processes receive SIGSEGV with fault address 0 on a foreground
  or background worker during startup.
- Redmi has twelve native frames in vendor `libGLES_mali.so`. This library name
  alone does not establish which graphics API was active.
- Y70s and nova 5i also report that SwappyDisplayManager's sandbox class loader
  cannot find `libUnreal.so`. The game library was already loaded elsewhere;
  this does not mean the APK is missing the library.
- The supplied logs lack usable native stacks for Y70s and nova 5i. The Redmi
  Unreal frame does not resolve to a credible graphics caller with the local
  symbols; the exact failing driver entry remains unconfirmed.
- The `libtap-log.so` exception belongs to the TapTap host process, not the game.

## Compatibility policy

`Config/DefaultDeviceProfiles.ini` selects a compatibility fragment for Android 10
Mali G76/G77 profiles and Android 10 Mali devices using the unknown/fallback
profiles (including G51). Matching rules must be present on each selected profile:
UE 5.8 does not traverse BaseProfileName when evaluating these rules.

The fragment applies before RHI initialization and overrides the profile's
Vulkan opt-in. It uses OpenGL ES, disables automatic PSO precaching, asynchronous
pipeline creation, and the separate pipeline-file-cache precompile path, and uses
UE frame pacing instead of Swappy.
This avoids the suspected startup driver compilation path and the observed
sandbox class-loader issue. This is a compatibility mitigation requiring device
retesting, not proof that every vendor crash has been eliminated. First-use shader
compilation may introduce hitches on affected devices.

Both ES and Vulkan shaders remain packaged. Newer Android versions and Adreno
devices retain their existing profiles. Android version code is 2, with display
version 1.1, to distinguish this candidate from the original version 1.

## Validation

Run `BrainMining.Android.CompatibilityProfiles` in editor automation. It uses
UE's Android profile selector and actual fragment evaluator to cover the three
affected GPU families, ES fallback, newer Android versions, and Adreno controls.

Build a fresh Android Shipping APK with embedded data. Check the signature,
version, embedded OBB, both maps, and packaged compatibility rules. Do not use
`-SkipCook` against data from before a source or content change.

On all five report devices, repeat three cold launches, enter MainGame, mine,
change rooms, reach the ending, and background/resume. Repeat in TapPlay as well
as a normal installation. An ordinary ADB install cannot validate TapPlay's
class-loader environment. Collect fresh logcat/tombstones if any startup fails.

## Local validation on October 19, 2026

- Editor and Android Shipping compilation succeeded.
- `BrainMining.Android.CompatibilityProfiles` passed all 12 device/version cases.
- Fresh cook completed 2,307 packages with zero errors; BuildCookRun succeeded.
- APK signature verification passed and its certificate matches the original APK.
- Manifest: package `com.YourCompany.BrainMining`, version code 2, version 1.1,
  minimum SDK 26, ARM64.
- Extracted the configuration from the APK's embedded `assets/main.obb.png` and
  verified all seven compatibility switches and the conditional matching rules.
- Embedded IoStore container sizes and CRCs match the inspected staged containers;
  the main map and ending map are present.
- Delivery: `Builds/MobileVerticalSlice/AndroidTapPlayCompatibility/Share/挖哦-1.1-兼容修复.apk`.
- Native build ID: `1223e38c09503f1655c9e1bb13c1f84fb2dede77`; matching symbols are
  archived with the build for subsequent crash analysis.
- No Android device was connected. Real-device and TapPlay regression results
  are still pending; the report's three failures are not yet confirmed resolved.

Logs, configuration extraction, container inventory, source provenance, and the
automation result are retained in `Saved/TapPlayCompatibility` and
`Saved/Automation/AndroidCompatibility`.
