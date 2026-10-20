# Mobile WenSi camera input — 1.9

The WenSi puzzle and mirror museum HUDs sit above the mobile controls. Their full-screen Slate scale box, size box and overlay were hit-testable even in exploration mode. The upper widget path therefore intercepted touch starts before they reached the camera/joystick layer.

The full-screen layout containers now use `SelfHitTestInvisible`; decorative hint text uses `HitTestInvisible`. Thought cards retain their existing exploration/edit visibility, so exploration lets touches pass through and editing still accepts word interaction. The standalone mirror HUD also receives the same outer visibility as the campaign HUD.

`BrainMining.Mobile.WenSiTouchRouting` paints the real puzzle and mirror widgets over the mobile controls into a Slate hit-test grid at 1600x900 and 2400x1080. It checks empty/populated thought layouts, touch camera yaw, editable-card hit testing, empty-space routing during editing and camera unlock after editing. The focused test passed in `Saved/Validation/Gamepad/20261020-222028/`. All six mobile/catalogue regression tests passed in `Saved/Validation/Gamepad/20261020-223452/`.

Android version is 1.9 / code 10. This is a native UI change; packaging rebuilds the Android executable and reuses the validated cooked MainGame/ending assets from 1.8. ResourceGuard and single-action compilation remain enabled. Packaging, APK checks and device evidence are recorded under `Saved/Validation/AndroidWenSiTouchFix/`.

The standalone APK is `Builds/MobileVerticalSlice/AndroidWenSiTouchFix/Share/挖哦-1.9-文思视角修复.apk` (1,532,101,384 bytes). Signature, version, native binary, embedded data containers, mobile SVGs and movie checks passed. ADB replacement installation succeeded on the connected 23013RK75C at 22:50:43 on 2026-10-20, and package inspection confirms version 1.9 / code 10. The opening screen renders after launch. WenSi input was verified by the real-widget automation described above; a full WenSi playthrough on the phone has not been performed.
