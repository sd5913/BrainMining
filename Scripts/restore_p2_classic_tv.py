"""Restore P1 TV geometry and its procedural tunnel, preserving P2 scenery/stage."""
import unreal,json,shutil,datetime,math
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
backup=root/'Saved/Backups'/('CardP2Classic_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True)
shutil.copy2(root/'Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap',backup/'Lvl_EchoMazeToCards_P2.umap')
actors=list(A.get_all_level_actors())
escape=next(a for a in actors if isinstance(a,unreal.MiningRoomEscape))
screen=escape.get_editor_property('authored_television_screen')
removed=[]
with unreal.ScopedEditorTransaction('Restore classic television and align seat ring'):
    escape.set_editor_property('authored_television_screen',None)
    if screen:
        removed.append(screen.get_actor_label());A.destroy_actor(screen)
    for a in actors:
        if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_P2_TV_Cabinet':
            removed.append(a.get_actor_label());A.destroy_actor(a)
    for name,value in dict(computer_entry=unreal.Vector(23520,11750,6700),computer_checkpoint_position=unreal.Vector(22840,11750,6100),tunnel_end=unreal.Vector(30140,11750,10230),exit=unreal.Vector(31300,11750,9996),auto_claim_victory_reward=False,preview_television=True).items():
        escape.set_editor_property(name,value)
    escape.set_actor_label('Classic Television / Green screen, safe tunnel and stage')
    ring=[a for a in actors if isinstance(a,unreal.StaticMeshActor) and 'Landing light ring' in a.get_actor_label()]
    assert ring,'Expected original dashed seat ring'
    for i,a in enumerate(ring):
        angle=i*2*math.pi/len(ring)
        a.set_actor_location(unreal.Vector(12450+800*math.cos(angle),3600+800*math.sin(angle),3007),False,True)
        a.set_actor_rotation(unreal.Rotator(yaw=math.degrees(angle)+90),False)
        a.set_actor_scale3d(unreal.Vector(2.8,.09,.03))
        a.set_actor_enable_collision(False)
    disabled=[]
    for a in actors:
        if a.get_actor_label() in ['Study V09 / Corner book pages 6','Study V09 / Corner book cover 6','Study V09 / Supporting hardback 6']:
            assert not a.static_mesh_component.is_visible(),'Do not remove collision from visible dressing'
            a.static_mesh_component.set_collision_profile_name('NoCollision')
            a.set_actor_enable_collision(False);disabled.append(a.get_actor_label())
    # This brush's coarse hull covers empty air above the ruler. Use a level-local
    # mesh copy so other maps sharing the countryside asset are unaffected.
    brush=next(a for a in actors if a.get_name()=='StaticMeshActor_338' and a.get_actor_label()=='SM_Brush')
    path='/Game/Passages/Echo/ArtP2/Collision/SM_Brush_Exact'
    mesh=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(brush.static_mesh_component.static_mesh.get_path_name(),path)
    assert mesh
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
    brush.static_mesh_component.set_static_mesh(mesh)
preview=[c for c in escape.get_components_by_class(unreal.ActorComponent) if c.component_has_tag('TelevisionEditorPreview')]
assert len(preview)>40,len(preview)
assert L.save_current_level()
out=root/'Saved/Validation/CardP2Flow'
(out/'classic-tv.json').write_text(json.dumps(dict(backup=str(backup),removed=removed,preview_parts=len(preview),seat_ring_parts=len(ring),disabled_hidden_collision=disabled,brush_collision=path),indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
