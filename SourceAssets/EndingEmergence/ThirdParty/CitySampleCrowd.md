# Ending street crowd

The user acquired and added **City Sample Crowds** and **Basic Walk Cycle Animation Pack** to this project through Fab. Refer to their acquired Fab licenses for distribution terms; these assets are not public-domain replacements.

- Original content remains under `Content/CitySampleCrowd` and `Content/Walk_cycle_AnimsSet`.
- Ending-specific references, duplicated in-place clips, IK rigs and retargeted Basic Walk clips live under `Content/EndingEmergence/Crowd`.
- `Scripts/retarget_ending_crowd.py` retargets Female Walk and Old Man Walk from the supplied mannequin to the corresponding City Sample body. It preserves the source animations.
- `Scripts/apply_ending_crowd.py` creates the four clothing presets and assigns `DA_EndingCrowd` only to the existing ending map, with a backup in Saved/Backups.
- Native City Sample walk/idle clips are duplicated before forcing root lock; originals remain untouched.
- Hair uses the supplied static helmet meshes. Facial meshes retain a neutral reference pose and follow the animated head bone; this distant crowd does not require groom simulation or facial animation.

## Current lightweight ending version

`Scripts/bake_ending_crowd.py` and the editor-only `UEndingCrowdBaker` merge lower-detail body/clothing/head/hair geometry and bake walking vertex offsets. The ending now references `Crowd/Lite/DA_EndingCrowdLite`: 1,200 walkers plus one central person in nine instanced batches, with no runtime skeletal pose drivers or original high-resolution skin/clothing materials. Near and far variants use different source LODs; animation phase and speed vary per walker. The hunched **Old Man Walk** is excluded from all active presets; its source asset is retained untouched.
