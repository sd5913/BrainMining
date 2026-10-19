"""Align the existing P2 television to P1 gameplay anchors. Run on P2 after compiling.
Preserves the authored cabinet/glass and all other level dressing; backs up edited assets.
"""
import unreal, json, shutil, datetime, sys
from pathlib import Path

root=Path(unreal.Paths.project_dir()).resolve()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out=root/'Saved/Validation/CardP2Flow';out.mkdir(parents=True,exist_ok=True)
backup=root/'Saved/Backups'/('CardP2Flow_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True)
for relative in ['Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap','Content/Rooms/CardDuel/Materials/M_EscapeTVInterior.uasset']:
    shutil.copy2(root/relative,backup/Path(relative).name)
all_actors=actors.get_all_level_actors()
escapes=[a for a in all_actors if isinstance(a,unreal.MiningRoomEscape)]
assert len(escapes)==1
escape=escapes[0];screen=escape.get_editor_property('authored_television_screen')
assert screen and screen.get_editor_property('authored_rest_mesh')
cabinets=[a for a in all_actors if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_P2_TV_Cabinet']
assert len(cabinets)==1
cabinet=cabinets[0]
entry=unreal.Vector(23520,11750,6700);target=entry-unreal.Vector(308,0,0)
center,extent=screen.get_actor_bounds(False)
# Match the P1 opening and leave the new cabinet's lower edge just above the P1 shelf.
# Use measured bounds, not the imported model's offset pivot.
desired_half_height=448.1941695
factor=desired_half_height/extent.z
with unreal.ScopedEditorTransaction('P2 television at P1 escape anchors'):
    for actor in [cabinet,screen]:
        old_location=actor.get_actor_location()
        actor.set_actor_scale3d(actor.get_actor_scale3d()*factor)
        actor.set_actor_location(target+(old_location-center)*factor,False,True)
    cabinet.static_mesh_component.set_collision_profile_name('NoCollision')
    screen.set_actor_enable_collision(True)
    for component in screen.get_components_by_class(unreal.DynamicMeshComponent):
        component.set_collision_profile_name('BlockAll')
        component.set_complex_as_simple_collision_enabled(True,True)
        component.update_collision(False)
    screen.reset_rock()
    for name,value in {'computer_entry':entry,'computer_checkpoint_position':unreal.Vector(22840,11750,6100),'tunnel_end':unreal.Vector(30140,11750,10230),'exit':unreal.Vector(31300,11750,9996),'preview_television':False}.items():
        escape.set_editor_property(name,value)
    cabinet.set_actor_label('P2 Television / P1 aligned escape cabinet')
    escape.set_actor_label('P2 Television / Flood, safe tunnel and celebration stage')
assert (screen.get_actor_bounds(False)[0]-target).length()<2
assert levels.save_current_level()
sys.path.insert(0,str(root/'Scripts'))
from card_tv_material import build_tv_material
build_tv_material()
(out/'aligned.json').write_text(json.dumps(dict(backup=str(backup),screen_center=str(screen.get_actor_bounds(False)[0]),cabinet_bounds=str(cabinet.get_actor_bounds(False)),entry=str(entry)),indent=2),encoding='utf-8')
unreal.log('CARD_P2_ALIGNED_AND_MATERIAL_READY')
