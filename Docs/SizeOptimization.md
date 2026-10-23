# Size optimization

## Baseline and scope

The 2026-10-21 Android 1.12 baseline APK is 1,542,144,800 bytes. Its archived directory is `Builds/MobileVerticalSlice/AndroidRemoteSync20261021v112`. Preserve this build until the optimized package passes validation.

The implementation covers generated build retention, isolated test cache retention, Android texture and mesh cooking budgets, redundant runtime content, and measured package validation. Source art, recovery backups, Git LFS objects and shared engine assets are not cleanup targets.

## Storage retention review

Latest check at 2026-10-22 01:40 local time: all 37 listed builds remain absent; all 49 listed caches still exist at their baseline sizes (7.13 GiB). The user's latest deletion report does not correspond to any additional missing manifest entry. E: has 141.02 GiB free, up 1.35 GiB since the previous observation. All previously recorded report paths remain present and the r6 APK hash still matches. Evidence: `Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r3.json`. Earlier observations below are historical.

Run `python Scripts/plan_project_storage.py` to produce a read-only cleanup manifest in `Saved/Validation/SizeOptimization/storage_plan.json`. It retains the two newest builds, the 1.11/1.12 baselines, and the initial Android, V15, V29 and TapPlay compatibility milestones. Unknown output types are preserved. No files are deleted by this script.

The user explicitly approved the frozen 86-directory manifest on 2026-10-22. Tool policy rejected the automated deletion command before it started. The user subsequently performed manual deletion. The latest verification at 01:28 local time found all 37 listed build directories absent (57.90 GiB of baseline file lengths); all 49 caches remain (7.13 GiB). E: free space is 139.67 GiB, up 5.36 GiB since the previous check and 71.21 GiB since the initial observation. These volume-wide changes cannot be attributed exclusively to the manifest. Do not regenerate or expand the approved manifest. Latest evidence: `Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r2.json`; the earlier snapshot remains in `storage_cleanup_manual_verification.json`.

The retained AndroidMainGameV15 build is also missing from its original path; no matching Recycle Bin item was found. The other five retained baseline builds, cell backup and r6 APK exist. The r6 APK length and SHA-256 still match the validated package. The V15 discrepancy remains unresolved; see `storage_cleanup_v15_recovery_check.json` in the same evidence directory.

New gamepad tests use `-NoAssetRegistryCache`, preserving their isolated user directory without writing another approximately 152 MiB asset-index cache each time. All 49 listed old caches remain at their baseline sizes. Each corresponding run directory still contains report files; report contents were not compared byte for byte.

## Android content budgets

The Android base device profile caps world and weapon color/normal textures at 2048, character normals and specular groups at 1024, and skybox textures at 1024. UI, lightmaps, desktop profiles and original imported pixels are unchanged. Textures that disable mip generation can bypass group limits and must be checked in the cooked report.

The cell retains its 1,355,517-triangle desktop LOD0 and adds a 100,890-triangle LOD1 selected by the Android MinLOD override. `r.StaticMesh.StripMinLodDataDuringCooking=1` removes render LODs below the platform minimum from cooked data. This does not imply that collision data is reduced by the same ratio; collision settings are preserved. The pre-change asset is backed up under `Saved/Backups/SizeOptimization20261021` and also exists in the unchanged Git baseline.

The optimized Android wrapper selects `-CustomConfig=AndroidSize`, which loads `Config/Custom/AndroidSize/DefaultGame.ini` in the host cooker. This removes the complete MainGame/Materials directory from AlwaysCook and excludes the HDRIBackdrop Blueprint, Material, Mesh and Texture folders. A platform-only AndroidGame.ini does not control these host editor packaging settings. Excluding the plugin mount root also triggers a duplicate-domain ensure in UE 5.8's AssetReferenceRestrictions, so the explicit content subfolders are used. A referenced-never-cook error remains a validation failure.

`Content/Packaging/PAL_NativeRuntimeMaterials` records all six material paths found in C++ under both Source and Plugins, including EchoPassage's M_BlueViolet. Because a label alone did not retain three unreferenced native materials in the measured cook, the wrapper also scans current C++ paths and explicitly passes them as `-PACKAGE=` cook roots. Asset references from maps and other cooked assets still provide their normal dependency closure. Ordinary desktop packaging keeps its previous broad directory rule.

Run `pwsh -NoProfile -File Scripts/Package_Android_SizeOptimized.ps1` after the asset preparation script succeeds, with Python 3.11 or newer available as `python`. Every invocation requires new cook and archive directories and performs a full cook. Staging uses `Saved/StagedBuilds/Android`: UEDeployAndroid reads the embedded OBB from this standard location even when UAT stages to a custom directory. The wrapper hashes every packaged pak/utoc/ucas against the staged file, verifies the required native assets/maps and exclusions, and refuses success if any differ or are missing. Small utoc evidence files, the APK hash, byte count and validation result are retained per run.

The wrapper reuses the existing Shipping executable; rebuild it separately after C++ changes. It does not create a second Share APK copy. Install and device rendering still require a connected target device.

The resource guard tolerates failure of optional WMI working-set trimming while keeping critical-memory and timeout termination active. PowerShell 7 uses .NET process-tree termination; older shells retain taskkill and now report failure explicitly. The guard regression suite passed 11 checks, and an injected WMI failure test additionally verified that its helper actually exited.

## Rollback

Revert only the Android TextureLODGroups additions in DefaultDeviceProfiles.ini and the StripMinLodDataDuringCooking addition in DefaultEngine.ini to restore previous platform budgets. Restore `Content/Fab/Cell/cell/StaticMeshes/cell.uasset` from `Saved/Backups/SizeOptimization20261021/cell.uasset` to restore its original single LOD. Stop selecting AndroidSize to restore the broad cooking rules; the primary asset label can then be removed if no longer needed. These actions do not require rewriting Git history or changing imported art.

## Validation ledger

- [x] Verify manual removal of all 37 historical builds in the approved manifest (57.90 GiB of baseline file lengths).
- [ ] Cache cleanup deferred at the user's request: 49 cache directories remain (7.13 GiB); missing retained V15 remains a separate issue.
- [x] Record manual cleanup observations, remaining directories, retained APK hash and volume free space.
- [x] Prevent isolated gamepad asset-index caches from accumulating; completed targeted run created none.
- [x] Set Android texture budgets while preserving UI and desktop assets.
- [x] Add the reduced Android LOD and verify cooked size; visual acceptance remains pending.
- [x] Remove verified redundant cooking inputs and verify all six native material paths in r6.
- [x] Perform a fresh Android cook, stage and package.
- [x] Compare actual container resources and APK size against the baseline: 38.43% smaller.
- [ ] Run relevant regressions and inspect important scenes.
- [x] Record results, remaining limitations and rollback instructions in SizeOptimizationResults.md.
- [x] Verify APK signing, manifest metadata, ZIP alignment, native ELF alignment and baseline signing identity; prepare AndroidSizeAcceptance.md.
- [x] Install and verify r6 on Xiaomi 23013RK75C / Android 15; check two launches, start-room rendering, look/move touch controls, pause and collection navigation.
- [ ] Complete Office/Museum/cell/ending visual checks, remaining gameplay and save tests, and matched device performance comparison. Investigate first-launch shader compiler/linker log messages; full device acceptance remains open.

Two completed logic-test batches total 19 successes and one failure (Gamepad.Haptics.LifecycleAndEvents: jet ignition cue). Main-scene and isolated-cell rendered previews were stopped by the physical-memory guard, so visual acceptance remains open. No Android device was connected. These limitations are separate from the passing package-integrity and size comparisons.

A subsequent commandlet scene-capture attempt avoided editor UI/PIE overhead. The first run hit its 240-second bound; the project-cache retry was stopped at approximately 536 seconds with 1.29 GiB physical memory available. Neither produced images. See Saved/Validation/SizeOptimization/offscreen_attempts.json. No memory thresholds were relaxed.
