# Mobile remote sync — 1.11

Fast-forwarded `main` from `14eff176` to `7424d01e`, including six remote commits fetched on 2026-10-21. The updated campaign includes collectible interaction/feedback, computer click feedback, poison ignition effects, the cell doorway and ending presentation, plus bilingual credits using the supplied Hiragino W6 and Rockwell fonts.

During packaging, a further remote commit `1e65312e` restored the opening camera motion and moved the player's landing position away from the bed. It was fetched, then fast-forwarded after the running build finished. It changes native opening code and validation scripts only; the final native rebuild therefore reuses this run's freshly cooked content. The target source revision is `1e65312e`.

The existing mobile camera, WenSi thought/painting interactions, card selection/reward, deferred mining, ending controls and pause catalogue remain in place. The mobile menu now also responds to externally requested quit confirmation, matching the new credits Escape path. Confirmation/cancellation is covered by inspecting the actual touch menu actions, not just its state flag.

The combined automation run exposed a test fixture that destroyed its collection actor before BeginPlay without invoking UI cleanup. The fixture now calls EndPlay explicitly in that case so its preview cannot retain a discarded world and crash the next map-based test. This is test teardown, not a change to campaign collection behavior.

The editor build and all 14 selected tests passed in `Saved/Validation/Gamepad/20261021-083442/index.json`: mobile layout/input/ending/WenSi, catalogue and pickup exposure, collection audio routing, opening menu lifecycle, card mouse/touch input, deferred excavation and floating fragments. The first run's test teardown failure is retained in `Saved/Validation/Gamepad/20261021-082513/`.

Android version is 1.11 / code 12. Packaging rebuilds Android native code and cooks MainGame and the production ending again. It does not reuse the previous APK's cooked content. ResourceGuard, one compile action, limited cook/shader concurrency and one Unreal workload remain enabled. This run's temporary files and derived data use the E drive. Build arguments, resource logs and validation evidence are recorded in `Saved/Validation/AndroidRemoteSync20261021/`.

The fresh cook processed all 2,781 packages successfully. Both OpenGL ES 3.1 and Vulkan ES 3.1 shader archives were generated. The initial build completed in 40 minutes, including the new E-drive Android derived-data cache. The final native rebuild is recorded under `Saved/Validation/AndroidRemoteSync20261021/NativeRefresh/`.
