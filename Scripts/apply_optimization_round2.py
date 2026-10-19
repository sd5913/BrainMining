"""Repair authored P2 walking surfaces and the seated floor robot; back up touched assets."""
import unreal,json,shutil,math
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/OptimizationRound2';out.mkdir(parents=True,exist_ok=True)
backup=root/'Saved/Backups'/('OptimizationRound2-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets=unreal.EditorAssetLibrary
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
def back(path,ext):
    rel=Path('Content')/(path.removeprefix('/Game/')+'.'+ext)
    if (root/rel).exists():
        (backup/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/rel,backup/rel)
map='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2'
back(map,'umap');assert levels.load_level(map)
changed=[];copies={}
for a in actors.get_all_level_actors():
    if not isinstance(a,unreal.StaticMeshActor):continue
    c=a.static_mesh_component;m=c.static_mesh;p=a.get_actor_location();label=a.get_actor_label()
    # Actual left-hand furniture, not hidden legacy scaffolding or ruler markings.
    if not m or not c.is_visible() or not (7900<p.x<11700 and -2400<p.y<1800 and -50<p.z<3900):continue
    if any(s in label.lower() for s in ['tick','tape','fold','label','tab']):continue
    if m.get_path_name().startswith('/Game/'):
        src=m.get_path_name().split('.')[0]
        dest='/Game/Passages/Echo/ArtP2/Collision/'+m.get_name().removesuffix('_Walkable').removesuffix('_Exact')+'_Walkable'
        if src not in copies:
            back(dest,'uasset')
            clone=unreal.load_asset(dest) if assets.does_asset_exist(dest) else assets.duplicate_asset(src,dest)
            assert clone
            clone.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            clone.get_editor_property('body_setup').set_editor_property('double_sided_geometry',True)
            for lod in range(mesh_editor.get_lod_count(clone)):
                for section in range(clone.get_num_sections(lod)):
                    mesh_editor.enable_section_collision(clone,True,lod,section)
            assert assets.save_loaded_asset(clone,False)
            copies[src]=clone
        c.set_static_mesh(copies[src])
    c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    a.set_actor_enable_collision(True)
    changed.append(dict(label=label,mesh=c.static_mesh.get_path_name(),position=list(p.to_tuple())))
floor=next(a for a in actors.get_all_level_actors() if a.actor_has_tag('CardRoomFloorRobot'))
p=floor.get_actor_location();p.z=.2
floor.set_actor_location(p,False,True)
floor.set_actor_rotation(unreal.Rotator(yaw=math.degrees(math.atan2(6550-3600,9950-6700))),False)
floor.set_editor_property('bEncounterDriven',False)
stage=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.CardRoomStage))
# The old book riser was moved into the left route. Seat the pelvis on the
# 1800 cm cushion, at its front edge, instead of leaving it at the old riser height.
stage.set_editor_property('opponent_scale',10.)
body_scale=stage.get_actor_scale3d().z*stage.get_editor_property('opponent_scale')
pelvis=unreal.Vector(15100,3600,1800-30*body_scale)
stage.set_editor_property('opponent_base_local',unreal.MathLibrary.inverse_transform_location(stage.get_actor_transform(),pelvis))
stage.set_editor_property('seat_pitch',7.)
assert levels.save_current_level()
report=dict(passed=True,backup=str(backup),surfaces=changed,meshes=[m.get_path_name() for m in copies.values()],floor_position=str(p),floor_rotation=str(floor.get_actor_rotation()))
(out/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
