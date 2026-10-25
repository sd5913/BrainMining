# Arm ASR integration

Source: https://github.com/arm/accuracy-super-resolution-for-unreal
Revision: 83797e1a7cb334d54dd89a9666dcf7d7cc7809c7
License: upstream MIT notices and LICENSES/MIT.txt retained.
The license is a declared NonUFS runtime dependency so packaged distributions
include the upstream notice as well as the repository copy.

This is a local UE 5.8 compatibility port. Upstream documents support through
UE 5.5. The plugin defaults r.ArmASR.Enable to zero; the Android game subsystem
selects Auto (Quality) on eligible devices when no saved preference exists.
The view extension is limited to the Vulkan mobile renderer. Windows Editor and
Android Shipping compilation, fresh shader cooking and Redmi K60 Vulkan runs
passed on 2026-10-25. This is single-device validation, not certification of all
Android GPUs. See Docs/MobileUpscaling.md for measurements and limitations.

Local adaptations:

- Renderer Internal include path and separated postprocess input header for 5.8.
- Removed unused UnrealEd/PropertyEditor headers so runtime users of the plugin
  do not require undeclared editor dependencies. Uses the UE 5.8 post-engine-init
  delegate accessor.
- Updated history reference-count signatures and RDG atomic clear, without the
  removed texture bulk upload API or an extra leaked RHI texture reference.
- Checks the loaded float16 Vulkan extension, the enabled 16-bit-ops setting and
  physical shaderFloat16 feature instead of forcing extensions at startup. ASR's
  constant buffers use 32-bit values; 16-bit uniform/storage/push constants are
  not prerequisites. The Redmi K60 reports shaderFloat16 but no 16-bit uniform
  or push-constant support. UE's generic native-16-bit flag is therefore not a
  suitable ASR capability test. Detection is available during GameInstance init,
  before the later post-engine-init view-extension registration. Both module
  activation and settings share this capability decision; other RHIs retain the
  original renderer. Actual rendering and temporal history were confirmed on
  Redmi K60; OpenGL reports unsupported and leaves ASR disabled.
- Desktop Lumen history inspection and the global denoiser replacement are not
  used by this mobile integration. Shader permutations are restricted to mobile
  Vulkan. FX registration occurs once and is unregistered during shutdown.
- Per-frame resource cleanup remains on the render thread when toggling ASR.
- Render statistics record actual input/output rectangles and history reuse;
  changing presets requests a camera cut to discard incompatible history.
- The desktop reactive-mask path depends on desktop postprocess inputs which the
  mobile renderer never supplies. It defaults off to avoid an unused scene-color
  copy. Mobile translucency, particles and disocclusion still require visual QA;
  no reactive-mask quality claim is made.
