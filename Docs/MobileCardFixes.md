# Mobile card combat — 1.4

The card-room Slate view occupies viewport layer 20 and receives battle touches. The generic mobile controls occupy layer 0, so their confirm icon could be visible but could not receive touches through the full-screen card view.

Confirmation now lives in the card view alongside card selection, using the same virtual coordinates and aspect-ratio transform. Its 144-by-144 target displays a flat check icon when the hand is ready. In round four, the stacked-card icon selects all seventeen cards; the next tap plays them. No keyboard input is required. The unreachable lower-layer confirm icon is hidden, while keyboard and gamepad confirmation remain available.

The controller's mobile confirm entry calls its own active card interaction directly. Inactive card stages no longer affect its confirmation state.

`CardRoom.Input.TouchFullEncounter` exercises real touch events through all four rounds at four viewport sizes with an offset and DPI transform. It covers deselection, incomplete combinations, seventeen-card selection, repeated confirmation during resolution, victory, and inactive-room rejection. `BrainMining.Mobile.InputAndMenu` also checks controller dispatch and removal of the unreachable lower-layer button.

Android version: 1.4 / versionCode 5. This build includes the preceding mining fixes.

Validation: `Saved/Validation/Gamepad/20261020-110005/index.json` reports seven passing tests, zero errors and zero warnings. The initial broader run was stopped by the resource guard while loading the unrelated full-map start-room fixture; the focused card, mobile and gamepad-context run completed normally.

Build: `Saved/Validation/AndroidCardFix/UAT/Log.txt`. Native code was rebuilt and packaged with the preceding build's unchanged cooked assets. Signature, version, ARM64 native library, embedded data containers and all thirty mobile SVG files were verified. Artifact: `Builds/MobileVerticalSlice/AndroidCardFix/Share/挖哦-1.4-卡牌修复.apk`.
