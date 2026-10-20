# Mobile ending progression — 1.6

The ending hides the desktop inventory, but the mobile mining button incorrectly depended on that inventory flag. The cell membrane was therefore mineable in the world without an accessible touch action. Touch availability now follows the ending director's stage for its owning player.

- Cell: mining, walking, camera drag and jump. Holding the mining button continues to allow same-finger camera drag.
- Blood: joystick acceleration/steering and camera drag while the character itself remains immobilized.
- Neurons: camera drag and the existing jump icon, including dragging while pressing it. Space reaches the existing neuron possession/buffering logic.
- Brain arrival and automatic transitions: gameplay controls are released.
- Scale journey: joystick controls the existing retreat boost and lateral input.
- Final tunnel: joystick acceleration/steering; the real drill runs automatically. Its local cinematic mode no longer depends on campaign unlock or residual cooldown.
- Paper sequence/video/credits: no mining or movement controls.

Pause keeps the stage's original movement and mining locks. Hidden tools and axes are released when changing phases. No desktop inventory or extra text labels are introduced.

The final movie previously completed only through OnEndReached or a missing media-source reference. A present but unplayable source could leave the ending stuck indefinitely. Media-open errors, decoder errors, missing players, duration completion and a 20-second playback-progress timeout now lead to the existing credits. The timeout only accumulates in the ending's unpaused update; resets clear all watchdog state.

Regression coverage: BrainMining.Mobile.EndingProgression exercises touch handlers, real virtual keys, stage transitions, pause/resume, and failed/stalled video completion. BrainMining.Mobile.EndingExcavation exercises the circular cell membrane and all twelve linked finale rock segments with deferred Boolean cuts forced, checking collision clearance and mesh budgets.

Validation on 2026-10-20: all 10 selected tests passed with zero failures and zero warnings in `Saved/Validation/Gamepad/20261020-131620/index.json`. The tunnel excavation test cleared all twelve linked segments in 141 strikes. The selection also includes mobile layout/menu, deferred ordinary mining, gamepad release, and the complete card encounter/reward tests.

Android package target: version 1.6 / code 7, standalone APK, retaining the existing signing certificate and cooked content. Native Android code is rebuilt. Full physical-device ending playthrough is not claimed by the automated checks.

Build succeeded: `Saved/Validation/AndroidEndingFix/UAT/Log.txt`. Signature, version, ARM64 native library, all five data containers, thirty mobile SVGs and the embedded ending movie were verified in `Saved/Validation/AndroidEndingFix/APK-validation.json`. Artifact: `Builds/MobileVerticalSlice/AndroidEndingFix/Share/挖哦-1.6-结局修复.apk` (1,256,582,309 bytes). Installed successfully with `adb install -r` on the connected 23013RK75C; Android reports version 1.6 / code 7.

Device screenshots confirm both the title screen and entry into the playable opening room: `Saved/Validation/AndroidEndingFix/device-startup.png` and `device-gameplay.png`. The full ending route has not yet been manually completed on the phone.
