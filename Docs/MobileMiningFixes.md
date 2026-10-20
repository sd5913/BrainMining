# Mobile mining fixes — 1.3

## Echo exit

Mobile action selection now follows the controller's active echo interaction component. Landing in an always-loaded echo stage is insufficient to keep the knock button active. The giant-room entrance disables echo exploration and permits mining before the passage is completed; the mobile HUD now follows this transition and exposes the mining button at the exit wall. The knock handler also checks the same active state.

## Ordinary rock cuts

Ordinary rock booleans previously stopped after a 35 ms game-thread budget, then repeated the same failed operation on subsequent strikes. Unfinished runtime cuts now retry on a worker with a bounded two-second boolean budget. Workers hold immutable geometry copies and share the existing global terrain-job limit with chunk cuts and simplification.

The game thread commits only valid, closed results within the triangle budget. Revision and transform checks reject stale results. Reset and teardown detach pending work. Pickaxe stages remain pending until terrain work completes, preventing duplicate requests or premature damage rollback. Collision, fog, and terrain listeners update together when the result commits.

`bm.Mining.RockBudgetMs=0` forces the deferred path for regression testing; the default remains 35 ms. The regression verifies repeated strikes at one location, completion of staged damage, duplicate-request rejection, and excavation after a reset.

## Android build

Version 1.3 / versionCode 4 is an ARM64 standalone APK with embedded data and the existing signing certificate. The latest Windows-only OverCrowd dependency is conditional; Android uses built-in instanced mesh components with the same baked meshes and animation instance data.

Build record: `Saved/Validation/AndroidMiningFix/UAT/Log.txt`.

Regression record: `Saved/Validation/Gamepad/20261020-071216/index.json` — 19 passing tests, including mobile echo-exit routing, pickaxe cuts/input, background task limits and Android compatibility profiles. The existing nonuniform-wall fixture emits world-context cleanup warnings.

APK verification: `Saved/Validation/AndroidMiningFix/APK-validation.json`. A connected phone receives an overwrite installation preserving application data. Automated coverage of the echo exit does not constitute a full manual phone playthrough.
