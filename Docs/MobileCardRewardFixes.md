# Mobile card reward — 1.5

The full-screen card view remains active after victory while the player is seated. The previous mobile fix put confirmation in this view, but left reward pickup on the covered gameplay layer. Victory now exposes a large hand icon in the card view at the same action position. It calls the existing interaction path, including the collectible notification that unlocks and equips the drill and starts the escape warning.

The pickup reply does not request focus for the card view after it has been dismissed. Seat restoration returns the player to the entry position, restores collision and movement, and releases camera input. Covered gameplay interaction buttons are hidden during the card encounter and return after collection. Rewards still require explicit pickup and cannot be claimed twice.

`CardRoom.Input.TouchFullEncounter` now runs through victory and reward pickup at four viewport sizes, including modal dismissal, focus, position, collision, walking, and repeated-touch checks. `BrainMining.Mobile.InputAndMenu` also exercises the real escape reward binding and verifies drill ownership, activation, and escape progression.

Android version: 1.5 / versionCode 6. Includes the preceding mining and card combat fixes.

Validation: `Saved/Validation/Gamepad/20261020-115127/index.json` reports seven passing tests after the final Android build. The reward integration fixture initializes actor script execution before exercising the production dynamic reward delegate.

The blocked pickup was also reproduced on the connected phone running version 1.4: tapping the visible gameplay hand button left the drill reward floating and the player seated. Evidence: `Saved/Validation/AndroidCardRewardFix/before-install.png` and `before-pickup-tap.png`. These are pre-fix observations, not a post-fix phone playthrough.

Build: `Saved/Validation/AndroidCardRewardFix/UAT/Log.txt`. Android compilation also required replacing the first-stage loop-and-break with an equivalent iterator check to satisfy Clang's unreachable-loop-increment diagnostic. Signature, version, ARM64 native library, embedded data containers and all thirty mobile SVG files were verified. Artifact: `Builds/MobileVerticalSlice/AndroidCardRewardFix/Share/挖哦-1.5-钻机拾取修复.apk`.
