# Mobile remote sync — 1.8

Fast-forwarded `main` from `8869b9a5` to `b171fdb1` (14 remote commits). The pre-sync mobile working tree remains recoverable in the stash named `mobile-before-remote-sync-20261020`; it was reapplied and conflicts were resolved without discarding the earlier fixes.

The updated opening, collection widget, cell scenery, blood vessel art, city crowd, ending film and editable credits come from the remote version. The production ending map remains `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`.

Mobile integration:

- Preserve the existing flat icon controls, card touch/reward fixes, deferred mining fixes and pause catalogue entry.
- The floating cell room uses joystick movement along the viewing direction. Its unused jetpack button is hidden. The shared movement reader receives the analog touch axis, including gentle movements below the virtual keyboard threshold.
- The touch pause button opens the menu at credits instead of dispatching the desktop Escape-to-quit action. The collection entry remains available there, while normal modal and scripted-stage restrictions remain in place. The desktop keyboard prompt is hidden on touch devices.
- Keep the remote eight-float crowd animation/clothing data and moving ISM bounds on Android, without depending on the Windows-only OverCrowd plugin. Preserve the remote temporary scripted drill access for the final tunnel.
- Generate Android launcher icons from `SourceAssets/Branding/ApplicationIcon.png` using `Scripts/prepare_android_app_icon.py` (Pillow required).

Validation: the editor build and seven targeted tests passed in `Saved/Validation/Gamepad/20261020-194240/index.json`: mobile layout, menu lifecycle, ending controls, deferred ending excavation, catalogue navigation, ordinary deferred rock excavation, and crowd continuity. ResourceGuard remains active with one compile action and one Unreal workload at a time.

Android version is 1.8 / code 9. Packaging performs a fresh cook of both production maps; the earlier 1.7 cooked content is not reused via `-skipcook`. Build arguments and results are stored under `Saved/Validation/AndroidRemoteSync20261020/`.

The Android build and cook completed successfully. The standalone APK is `Builds/MobileVerticalSlice/AndroidRemoteSync20261020/Share/挖哦-1.8-同步更新.apk` (1,532,101,320 bytes), signed with the existing certificate and containing its game data. Validation confirms all five containers, thirty mobile SVGs, fourteen required updated runtime assets, seven launcher icon resolutions and the exact current ending movie. See `Saved/Validation/AndroidRemoteSync20261020/APK-validation.json`.

Device follow-up on 2026-10-20: the connected 23013RK75C (mondrian) has version 1.8 / code 9 installed. ADB touch smoke checks confirmed title rendering, entering the starting room, the pause menu, opening the collection catalogue from its gem icon, next-page navigation, returning to gameplay, subsequent look and joystick movement, look dragging from the mining button, settings open/back, and quit confirmation/cancel. Screenshots and package-scoped diagnostics are stored in `Saved/Validation/AndroidRemoteSync20261020/DeviceCheck/`.

The same app process remained alive throughout these checks. Its captured logs contain startup service/graphics allocation messages but no fatal crash marker; the package exit history has no new exit since this session started. This is a basic smoke check, not a performance benchmark or full campaign verification. Starting-room computer interaction was attempted but no visible outcome was confirmed, so it is not marked passed. Equipment pickup, sustained rock excavation, later room progression, ending video and credits still require device gameplay verification. Full device ending playthrough remains unverified.

Generated Windows OverCrowd binaries were backed up under the validation directory and the tracked files restored to their pulled versions. Mobile source changes remain uncommitted.
