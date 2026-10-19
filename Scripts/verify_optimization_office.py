"""Render the first office exit in the streamed MainGame, without saving the map."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
code = (root/'Scripts/capture_package_playtest.py').read_text(encoding='utf-8')
code = code.replace("'Saved/Validation/PackagePlaytest'", "'Saved/Validation/OptimizationRound2/OfficeVisual'")
old = "pose(unreal.Vector(-16810,15,6500),unreal.Vector(-16655,-115,6540));s.update(phase=2,next=now+2);return"
new = """stage=actors(unreal.PopupCloseStage)[0]
            meshes={c.get_name():c for c in stage.get_components_by_class(unreal.StaticMeshComponent)}
            for name in ['OfficeBlastDoor','OfficeBlastDoorArt']:
                mesh=meshes[name]
                assert mesh.is_visible() and not mesh.get_editor_property('hidden_in_game'),name+' hidden'
            assert meshes['OfficeBlastDoorArt'].static_mesh
            s['checks'].append('First exit door and original office door art visible in streamed MainGame')
            transform=stage.get_actor_transform()
            pose(unreal.MathLibrary.transform_location(transform,unreal.Vector(560,0,100)),unreal.MathLibrary.transform_location(transform,unreal.Vector(1000,0,145)))
            s.update(phase=2,next=now+2);return"""
assert old in code
code = code.replace(old,new)
code = code.replace("shot('office_restored_entry');area(2)", "shot('office-first-exit');finish();return\n            area(2)")
code = code.replace("unreal.SystemLibrary.quit_editor()", "levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()")
exec(compile(code,str(root/'Scripts/capture_package_playtest.py'),'exec'))
