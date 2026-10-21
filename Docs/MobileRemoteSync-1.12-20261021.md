# Mobile remote sync — 1.12

Fast-forwarded main from `af1a2ca0` to `79c1cafd` on 2026-10-21. This imports the latest playtest fixes for maximum-tier pickaxe materials, collectible presentation across the ending transition, robot singed eyebrows, pickup and water subtitles, heaven entry audio, footprints, and ending music fade-in.

The newly persistent finale drill prompt now reads “上推摇杆加速” when touch controls are active. Desktop keeps its W-key prompt. The existing mobile joystick drives this stage through the shared movement input.

Android version is 1.12 / code 13. The standalone APK embeds its game data. Packaging rebuilds native Android code and cooks the updated source assets; it does not reuse the previous APK. Builds run one at a time with ResourceGuard, one compile action and constrained shader/cook concurrency. Temporary files and derived data stay on the E drive.

Build and verification evidence is recorded under `Saved/Validation/AndroidRemoteSync20261021v112/`.

The Windows Editor build passed. The targeted regression report `Saved/Validation/Gamepad/20261021-103009/index.json` records 21 passed tests (17 clean, four with warnings), zero failures and zero not-run tests. Coverage includes mobile input/menu/ending/WenSi, collectible presentation and audio, pickaxe progression, room audio, opening lifecycle, robot eyebrows and card rules. Warnings concern test-world cleanup, test-generated collision data and a network connectivity probe.

The earlier combined run was stopped by ResourceGuard when MainGameTravel loaded the full campaign, and the separate rendered ending music check was likewise stopped during startup for physical-memory headroom. Neither is counted as passing. Resource thresholds were not lowered. No complete phone campaign or ending playthrough is claimed.

Android packaging succeeded in 6 minutes 46 seconds. The current-source cook processed 2,783 packages: 60 cooked, 2,586 reused through the cooker's up-to-date checks, and 137 skipped by platform. Cook reported zero errors and two warnings (plugin license notice and an existing FString-backed text localization warning).

Deliverable: `Builds/MobileVerticalSlice/AndroidRemoteSync20261021v112/Share/挖哦-1.12-同步更新.apk`, 1,542,144,800 bytes. Validation confirms the expected signing certificate, version 1.12/code 13, matching staged native binary and five data containers, 89 required runtime assets, 30 mobile SVGs, seven launcher icons and the ending movie. The source snapshot still matches the build, and remote main remained `79c1cafd` at the final check.

Installed with `adb install -r` on device `f8ec7df0`, preserving app data. Android reports the update at 2026-10-21 10:45:12 local time. Screenshots verify startup, movement away from the bed, touch camera rotation, pause, catalogue entry and restored gameplay/camera after returning. PID 20279 remained alive, and the captured process log contains no fatal signal, fatal exception, assertion failure or fatal error matches.
