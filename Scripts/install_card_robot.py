"""Install the supplied robot and move the clock away from the wall-impact opening."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/CardRobot'
dest='/Game/Rooms/CardDuel/Robot';lib=unreal.MaterialEditingLibrary;ats=unreal.AssetToolsHelpers.get_asset_tools()
materials=[]
for name,color,metal,rough in [('RobotIvory',(.64,.53,.36),.08,.46),('RobotGraphite',(.018,.024,.03),.65,.34)]:
    if unreal.EditorAssetLibrary.does_asset_exist(dest+'/'+name):
        materials.append(unreal.load_asset(dest+'/'+name));continue
    material=ats.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
    c=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector);c.constant=unreal.LinearColor(*color,1);lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop in [(metal,unreal.MaterialProperty.MP_METALLIC),(rough,unreal.MaterialProperty.MP_ROUGHNESS)]:
        c=lib.create_material_expression(material,unreal.MaterialExpressionConstant);c.r=value;lib.connect_material_property(c,'',prop)
    lib.recompile_material(material);unreal.EditorAssetLibrary.save_loaded_asset(material);materials.append(material)
for part in json.loads((root/'SourceAssets/CardRobot/Prepared/parts.json').read_text()):
    mesh=unreal.load_asset(dest+'/'+part['mesh'])
    for i in range(len(mesh.static_materials)):mesh.set_material(i,materials[1 if part['name'].startswith(('Eye','Brow')) else min(i,1)])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
backup=root/'Saved/Backups'/('CardRobot-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
map='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2'
shutil.copy2(root/'Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap',backup/'P2.umap')
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(map)
allactors=actors.get_all_level_actors();stage=next(a for a in allactors if isinstance(a,unreal.CardRoomStage))
stage.set_editor_property('rabbit_skeletal_mesh',None);stage.set_editor_property('bUseRobotOpponent',True)
stage.set_editor_property('opponent_scale',8.0);stage.set_editor_property('opponent_exit_center_offset',unreal.Vector(0,0,70))
stage.set_actor_label('P2 Countryside / Card opponent / Robot')
floor=[]
for a in allactors:
    if not isinstance(a,unreal.StaticMeshActor):continue
    mesh=a.static_mesh_component.static_mesh
    if (mesh and mesh.get_name()=='SM_Cat') or 'Floor/Handmade plush ' in a.get_actor_label() or 'Floor plush ' in a.get_actor_label():floor.append(a)
robot=next((a for a in allactors if a.actor_has_tag('CardRoomFloorRobot')),None)
assert floor or robot,'Missing authored floor cat or previously installed robot'
if not robot:robot=actors.spawn_actor_from_class(unreal.CardRoomRobot,unreal.Vector(9950,6550,30),unreal.Rotator(yaw=-35))
robot.set_actor_label('P2 Countryside / Floor / Supplied robot');robot.set_actor_scale3d(unreal.Vector(14,14,14))
robot.tags=[unreal.Name('CardRoomFloorRobot')]
for a in floor:actors.destroy_actor(a)
clock=next(a for a in allactors if a.get_actor_label() in ['clock','P2 Countryside / Wall clock clear of impact'])
before=clock.get_actor_location();clock.set_actor_location(unreal.Vector(before.x,-1500,10600),False,False)
clock.set_actor_label('P2 Countryside / Wall clock clear of impact')
assert levels.save_current_level()
(out/'installed.json').write_text(json.dumps(dict(passed=True,backup=str(backup),floor_replacements=len(floor),clock_before=str(before),clock_after=str(clock.get_actor_location()),stage_scale=stage.get_editor_property('opponent_scale')),indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
