# Mobile GM tools

The touch pause menu has a **GM 工具** button beside Quit. Open the pause menu with the top-right pause button, then select GM tools to access the existing developer panel: room jumps, tool grants, mineral unlocks and standalone test maps. Desktop Ctrl+F10 behavior is unchanged.

Opening GM closes the pause panel before opening the developer panel so the existing menu toggle does not merely dismiss pause. Closing GM restores gameplay input without restarting a held mining action. If the room already owns an input lock, the developer panel remains unavailable and the touch pause menu is restored. Quit confirmation cannot be bypassed through this entry point.

This entry exposes the existing developer actions and does not isolate their gameplay or save effects. Use a test save when exercising room jumps and unlocks.

## Validation

- Windows Editor and Android Shipping native builds succeeded on 2026-10-25.
- `BrainMining.Mobile.InputAndMenu` passed with zero warnings or failures. It checks the GM entry, pause-to-GM transition, input restoration, quit confirmation and the pre-existing modal input lock.
- Automation evidence: `Saved/Validation/MobileGM-20261025/Automation/index.json`.
- Android package and device acceptance are recorded separately in `Saved/Validation/SizeOptimization/AndroidSizeOptimized-20261025-MobileGM/` and `Saved/Validation/MobileGM-20261025/`.
- The 949,909,876-byte APK passed container and required-asset checks, then installed on Redmi K60 / Android 15 with `adb install -r`. The installed APK SHA-256 matches `A11E4A45D9A26167250997ACEDFAFA342CE3FF3D355CF9DE9DCAF4D72D7872EE`.
- Actual touch checks passed: pause entry visible, GM panel opens, the list scrolls, Continue returns to gameplay, and camera/movement input responds. Screenshots `04-pause-gm.png` through `07-resume-movement.png` record these steps. No fatal exception, fatal signal, ANR or out-of-memory match appeared in the captured process log.
- This acceptance covers menu access and input restoration, not every developer action or a complete gameplay/performance run. The package retains version 1.12 / code 13; identify this test build by its path and hash.

The later ASR-03 build also passed the same automation suite and a native touch
check of pause → GM tools → return to gameplay. It includes the mobile graphics
settings under the pause menu's sliders icon. The installed APK and final
verification are recorded in `Docs/MobileUpscaling.md`; GM screenshots are in
`Saved/Validation/MobilePerformance-20261025/final-asr-03/`.
