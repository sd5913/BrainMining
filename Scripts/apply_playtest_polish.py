"""Fix the authored start door and overlap the stage/cave junction; preserve room art."""
import unreal,json,sys,shutil,datetime
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Scripts'))
import main_game_room_entrances_geometry as geometry
out=root/'Saved/Validation/PlaytestPolish';out.mkdir(parents=True,exist_ok=True)
backup=root/'Saved/Backups'/('PlaytestPolish-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
def load(path):
 shutil.copy2(root/'Content'/(path.removeprefix('/Game/')+'.umap'),backup/(path.rsplit('/',1)[-1]+'.umap'))
 assert L.load_level(path)
 return list(A.get_all_level_actors())
a=load('/Game/StartRoom/Maps/Lvl_StartRoom')
door=next(x for x in a if isinstance(x,unreal.MainGameDoor))
# The first door is the artist's thin timber panel between the bedroom and
# the little cave. Reuse that art and its handle; the far opening stays a rock wall.
panel=next((x for x in a if x.get_actor_label()=='South_ClosedWall5'),None)
if panel:
 mesh=panel.get_component_by_class(unreal.StaticMeshComponent)
 door.set_actor_transform(mesh.get_world_transform(),False,True)
 door.door_mesh.set_static_mesh(mesh.static_mesh)
 for i in range(mesh.get_num_materials()):door.door_mesh.set_material(i,mesh.get_material(i))
 A.destroy_actor(panel)
knob=next(x for x in a if x.get_actor_label()=='SM_Van_Wheel_Rear_Right')
knob.get_component_by_class(unreal.StaticMeshComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
knob.attach_to_actor(door,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
door.set_actor_label('STARTROOM_BedroomDoor_E_Open')
if not door.actor_has_tag('BM_StartRoom_InteractiveDoor'):door.set_editor_property('tags',list(door.tags)+['BM_StartRoom_InteractiveDoor'])
report['door']={'transform':str(door.get_actor_transform()),'handle':knob.get_actor_label(),'only_interactive_door':True}
assert L.save_current_level()
a=load('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
guides=[x for x in a if 'Landing light ring' in x.get_actor_label()]
report['guide_candidates']=[x.get_actor_label() for x in a if any(t in x.get_actor_label().lower() for t in ['ring','dash','outline','frame','mark'])]
for x in guides:
 if not x.actor_has_tag('CardSeatGuide'):x.set_editor_property('tags',list(x.tags)+['CardSeatGuide'])
report['tagged_guides']=len(guides)
assert guides and L.save_current_level()
a=load('/Game/MainGame/Maps/Lvl_MainGame');assert L.set_current_level_by_name('Lvl_MainGame')
cfgpath=root/'Config/MainGameRooms.json';shutil.copy2(cfgpath,backup/cfgpath.name)
cfg=json.loads(cfgpath.read_text(encoding='utf8'));spec=next(s for s in cfg['generated_connections'] if s['id']=='Journey_Mine03')
spec['start_x']=32650
spec['floor_samples']=[[32650,19900]]+[p for p in spec['floor_samples'] if p[0]>32650]
rock=next(x for x in a if x.get_actor_label()=='LINK_Journey_Mine03_NaturalRock')
obj=out/'stage-cave-collar.obj';geometry.write_obj(obj,*geometry.closed_tube(spec));assert unreal.MiningDesignerLibrary.import_campaign_rock(rock,str(obj));rock.reset_rock()
shell=next(x for x in a if x.get_actor_label()=='LINK_Journey_Mine03_BedrockShell');comp=shell.get_component_by_class(unreal.DynamicMeshComponent)
v,t=geometry.assert_closed(*geometry.closed_tube(spec,True));origin=shell.get_actor_location();dynamic=comp.get_dynamic_mesh();dynamic.reset()
unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic,unreal.GeometryScriptSimpleMeshBuffers(vertices=[unreal.Vector(*p)-origin for p in v],triangles=[unreal.IntVector(*i) for i in t]))
unreal.GeometryScript_Normals.set_per_face_normals(dynamic);assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dynamic)
comp.set_complex_as_simple_collision_enabled(True,True);comp.update_collision(False)
assert L.save_current_level();cfgpath.write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf8')
report['collar_start']=spec['start_x'];report['passed']=True
(out/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
