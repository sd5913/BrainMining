# Mobile collection menu — 1.7

The mobile pause page now includes a gem icon beside Resume, connected to the existing `OpenPauseCollection` action. It opens the existing collection catalogue and retains the flat icon-only style. Resume remains the default focused action.

The entry follows the desktop pause menu's availability rule: it is disabled when opened over an existing room modal or scripted input lock, avoiding the catalogue's normal gameplay restore path clearing those locks. It is also disabled when the catalogue is already open.

The mobile toolbar's Back action closes an open catalogue directly. Simulating Escape on the player controller does not reach the paused catalogue widget's keyboard handler. Catalogue/map back actions also take priority over underlying room actions.

The final editor build passed all four selected checks with no failures in `Saved/Validation/Gamepad/20261020-151428/index.json`: mobile icon layout, input/menu lifecycle, ending controls, and catalogue navigation. The lifecycle regression opens the catalogue through the pause subsystem and verifies that mobile Back closes it, restores movement/look, hides the cursor, and resumes the mining tick.

Android version 1.7 / code 8 builds successfully with the existing signing certificate and embedded game data. Validation checks the native library, five containers, thirty mobile SVGs and ending movie. The independent APK is `Builds/MobileVerticalSlice/AndroidCollectionMenuFix/Share/挖哦-1.7-收集品入口.apk`.

Installed successfully on the connected 23013RK75C phone. Verified by touch: start game, open Pause, tap the gem, turn to the second catalogue page, tap toolbar Back, and drag to rotate the camera after returning to gameplay. Screenshots are saved in `Saved/Validation/AndroidCollectionMenuFix/device-*.png`.
