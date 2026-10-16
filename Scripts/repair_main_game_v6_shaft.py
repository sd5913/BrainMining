"""Trim duplicated slab faces where the starting shaft meets its passages."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV6'
def apply():
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    bounds={
        'STARTROOM_Shaft_North':((-33010,150,11100),(-32480,220,12300)),
        'STARTROOM_Shaft_South':((-33010,-220,11100),(-32480,-150,12300)),
        'STARTROOM_Shaft_TopRoof':((-32950,-220,12230),(-32480,220,12300)),
        'STARTROOM_LowerApproach_Roof':((-33010,-220,11400),(-32950,220,11470)),
        'STARTROOM_UpperPassage_Floor':((-32410,-220,11830),(-30700,220,11900)),
        'STARTROOM_UpperPassage_Roof':((-32480,-220,12230),(-30770,220,12300)),
        'STARTROOM_UpperPassage_North':((-32480,150,11900),(-30770,220,12230)),
        'STARTROOM_UpperPassage_South':((-32480,-220,11900),(-30770,-150,12230)),
    }
    changed=[]
    for label,(lo,hi) in bounds.items():
        a=next(a for a in actors if a.get_actor_label()==label)
        assert isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh.get_path_name().startswith('/Engine/BasicShapes/Cube')
        old_center,old_extent=a.get_actor_bounds(False);low=unreal.Vector(*lo);high=unreal.Vector(*hi)
        a.set_actor_location((low+high)/2,False,True);a.set_actor_scale3d((high-low)/100)
        changed.append(dict(label=label,old_center=list(old_center.to_tuple()),old_extent=list(old_extent.to_tuple()),min=lo,max=hi))
    return changed
if __name__=='__main__':
    report={'passed':False};ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    try:
        assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');report['slabs']=apply();assert levels.save_current_level();report['passed']=True
    except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
    finally:
        (OUT/'shaft.json').write_text(json.dumps(report,indent=2));unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
