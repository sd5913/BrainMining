# User-provided animal cell

- Source supplied by the user under `Art/Ending/CellModels`: `h092-动物细胞解剖_BLENDER.blend` and its `tex` directory. Other supplied formats remain untouched.
- No web download or license substitution was performed. The original files and textures are preserved.
- `Scripts/prepare_ending_cell.py` reads the Blender file, packs existing diffuse textures/colors into one atlas, and creates reduced ending-only FBX copies under `SourceAssets/EndingEmergence/Cell`.
- Near cutaway: 64,999 triangles. Distant cutaway: 6,460 triangles. Interior mitochondria, reticulum and nucleus: 2,500 / 6,499 / 6,999 triangles. Each uses one material slot.
- `Scripts/apply_ending_cell.py` imports these copies into `Content/EndingEmergence/Cell` and applies them to the existing ending map after making a backup. The playable interior collision, excavatable membrane and blood-vessel entrance are preserved.
- Per the subsequent user correction, the cutaway meshes are not used for universe-scale cells. Only interior organelles use the supplied geometry; the enclosing cell and the 666 surrounding cells use the original abstract translucent membranes and star fields.
