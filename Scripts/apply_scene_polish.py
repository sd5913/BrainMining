"""October 20 scene corrections; backups, targeted edits and saved assertions."""
import unreal,json,sys,shutil,traceback,math
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
OUT=ROOT/'Saved/Validation/ScenePolish';OUT.mkdir(parents=True,exist_ok=True)
BACKUP=ROOT/'Saved/Backups'/('ScenePolish_'+datetime.now().strftime('%Y%m%d_%H%M%S'))
MAIN='/Game/MainGame/Maps/Lvl_MainGame';HEAVEN='/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DL=unreal.MiningDesignerLibrary;EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
report={'passed':False,'backup':str(BACKUP),'removed':[],'saved':[]}
def V(p):return unreal.Vector(*p)
def xyz(p):return list(p.to_tuple())
def setp(a,k,v):a.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def backup(path,ext='uasset'):
 rel=Path('Content')/(path.split('.')[0].removeprefix('/Game/')+'.'+ext)
 if (ROOT/rel).exists() and not (BACKUP/rel).exists():
  (BACKUP/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/rel,BACKUP/rel)
def save_map():
 path=ed.get_editor_world().get_outermost().get_name() if hasattr(ed.get_editor_world(),'get_outermost') else ed.get_editor_world().get_path_name().split('.')[0]
 assert levels.save_current_level();report['saved'].append(path)
def mark(a,label,area):
 a.set_actor_label(label);a.tags=['BM_ScenePolish20261020',area];a.set_folder_path('MainGame/ScenePolish');return a
def cube(label,p,size,material,area):
 a=next((a for a in api.get_all_level_actors() if a.get_actor_label()==label),None) or api.spawn_actor_from_class(unreal.StaticMeshActor,V(p))
 mark(a,label,area);a.set_actor_location(V(p),False,True);a.set_actor_scale3d(V(size)/100)
 a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,material);a.static_mesh_component.set_collision_profile_name('BlockAll');return a
def delete(a):report['removed'].append(a.get_path_name());api.destroy_actor(a)
def load(path):backup(path,'umap');assert levels.load_level(path)
try:
 # The pulled map regressed to pre-collection/P1 assembly. Restore the last integrated
 # map only when this precise regression is present; keep current source room assets.
 load(MAIN)
 fields=[a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningCollectionField)]
 if len(fields)==1 and len(fields[0].authored_placements)==0:
  assert levels.load_level('/Game/MainGame/Maps/Lvl_ScenePolishBaseline')
  f=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningCollectionField))
  assert len(f.authored_placements)==11,'Baseline must contain the complete integrated collection'
  assert any(isinstance(a,unreal.HeavenToilet) for a in api.get_all_level_actors())
  assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),MAIN)
  report['recovered_prepull_integration']=True
  assert levels.load_level(MAIN)
 from scene_polish_materials import create_materials,FOLDER
 relief=create_materials(backup)
 aa=list(api.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField))
 assert len(field.authored_placements)==11
 water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 report['heaven_frame']={'position':xyz(frame.translation),'yaw':frame.rotation.rotator().yaw}
 red=water.retaining_soil[0].rock_material
 # Keep the carved branch, red enclosing rock and collection placements, replace
 # the imported source dressing as a unit so authored pipes/water/sky stay aligned.
 old=[a for a in aa if (a.actor_has_tag('BM_MainGame_Collections') and a.actor_has_tag('BM_Campaign_MainGame03') and not a.get_actor_label().startswith('COL_')) or a.actor_has_tag('BM_ScenePolishHeaven')]
 assert water in old
 for a in old:delete(a)
 copies=list(DL.copy_experience_actors(HEAVEN,unreal.Vector(),'BM_Campaign_MainGame03'))
 sourcefield=next(a for a in copies if isinstance(a,unreal.MiningCollectionField));placement=sourcefield.authored_placements[0]
 presentation=placement.presentation_actor;item_pose=placement.transform.copy()
 poses={a:a.get_actor_transform() for a in copies}
 kept=[]
 for a in copies:
  label=a.get_actor_label()
  if isinstance(a,(unreal.MiningCollectionField,unreal.PlayerStart,unreal.PostProcessVolume,unreal.SkyLight,unreal.DirectionalLight,unreal.ExponentialHeightFog)) or label.startswith(('Entrance ','Heaven water droplet','Heaven spout catch basin')):
   delete(a);continue
  t=poses[a];t.translation=unreal.MathLibrary.transform_location(frame,t.translation);t.rotation=unreal.MathLibrary.compose_rotators(t.rotation.rotator(),frame.rotation.rotator()).quaternion()
  a.set_actor_transform(t,False,True);a.tags=list(a.tags)+['BM_MainGame_Collections','BM_ScenePolishHeaven'];a.set_folder_path('MainGame/Collections/Heaven20261020');kept.append(a)
  # A source HDRI skylight is global; the room's local authored keys illuminate it.
  for sky in a.get_components_by_class(unreal.SkyLightComponent):sky.set_intensity(0)
 water=next(a for a in kept if isinstance(a,unreal.HeavenToiletWater))
 setp(water,'spill_coordinate_frame',frame);setp(water,'retaining_soil',[a for a in kept if isinstance(a,unreal.DiggableRock)]);setp(water,'outer_x',-300.)
 for rock in water.retaining_soil:
  setp(rock,'rock_material',red)
  for c in rock.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,red)
 water.rebuild_water();assert presentation in kept;presentation.continuous_spray=True
 pp=list(field.authored_placements);p=next(p for p in pp if p.item_id==0)
 item_pose.translation=unreal.MathLibrary.transform_location(frame,item_pose.translation);item_pose.rotation=unreal.MathLibrary.compose_rotators(item_pose.rotation.rotator(),frame.rotation.rotator()).quaternion()
 setp(p,'transform',item_pose);setp(p,'presentation_actor',presentation);setp(field,'authored_placements',pp)
 drip=api.spawn_actor_from_class(unreal.NiagaraActor,unreal.MathLibrary.transform_location(frame,unreal.Vector(-300,125,184)),frame.rotation.rotator())
 mark(drip,'COL_Heaven_ExposedClueDrip','BM_Campaign_MainGame03');drip.tags=list(drip.tags)+['BM_ScenePolishHeaven']
 c=drip.get_component_by_class(unreal.NiagaraComponent);c.set_asset(unreal.load_asset('/Game/HeavenPreview/VFX/NS_HeavenPipeDrip'));c.set_auto_activate(True)
 report['heaven_copied']=len(kept)
 # Existing large Niagara crystal bubbles are redundant with the much finer motes.
 for a in list(api.get_all_level_actors()):
  if a.get_actor_label().startswith('MAIN_GAME_CrystalMotes_'):delete(a)
  if a.get_actor_label()=='Office_Entry_OpenDoor':delete(a)
 # Relief is private to the start room/tunnel assets; preserve their original palette.
 changed=0
 for a in api.get_all_level_actors():
  if a.get_level()!=hall.get_level():continue
  if a.get_actor_location().x>-18000:continue
  if isinstance(a,unreal.DiggableRock) and a.rock_material:
   key=a.rock_material.get_path_name().split('.')[0]
   if key in relief:
    setp(a,'rock_material',relief[key]);changed+=1
    for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,relief[key])
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   for i,m in enumerate(c.get_materials()):
    if m and m.get_path_name().split('.')[0] in relief:c.set_material(i,relief[m.get_path_name().split('.')[0]]);changed+=1
 report['relief_components']=changed
 # A warm local pool points at the next excavatable face after the TV stage.
 light=next((a for a in api.get_all_level_actors() if a.get_actor_label()=='POLISH_StageSoil_Guide'),None) or api.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(32750,11480,20280))
 mark(light,'POLISH_StageSoil_Guide','BM_Campaign_MainGameJourney')
 light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(light.get_actor_location(),unreal.Vector(33200,11750,20050)),False)
 c=light.light_component;c.set_intensity_units(unreal.LightUnits.CANDELAS);c.set_intensity(850);c.set_light_color(unreal.LinearColor(1,.68,.40));c.set_attenuation_radius(1800);c.set_outer_cone_angle(42);c.set_inner_cone_angle(22)
 # Fill the original roof aperture down to its ceiling underside. Bake the existing
 # actor so references and mining ownership survive, with a continuous 8.4 m plug.
 soil=next(a for a in api.get_all_level_actors() if a.get_actor_label()=='MAIN_GAME_Museum_DownwardSoil')
 mesh=unreal.DynamicMesh();local=unreal.MathLibrary.inverse_transform_location(soil.get_actor_transform(),unreal.Vector(47200,11750,15220))
 unreal.GeometryScript_Primitives.append_box(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=620,dimension_y=620,dimension_z=840,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 assert DL.set_campaign_rock_source(soil,mesh)
 report['museum_soil_bounds']=[xyz(x) for x in soil.get_actor_bounds(False)]
 ceiling=unreal.load_asset(FOLDER+'/M_MuseumCeiling')
 for a in api.get_all_level_actors():
  if a.get_level()==hall.get_level() and a.get_actor_label().startswith('MAIN_GAME_RoofRim_'):
   a.static_mesh_component.set_material(0,ceiling)
 save_map()
 # Source office: two misplaced floating sofas and one-sided partition returns.
 load('/Game/PopupClose/Maps/Lvl_PopupClose1')
 for a in list(api.get_all_level_actors()):
  if a.get_name() in ['StaticMeshActor_246','StaticMeshActor_247']:
   assert any(c.static_mesh and 'Sofa' in c.static_mesh.get_name() for c in a.get_components_by_class(unreal.StaticMeshComponent));delete(a)
 wall=unreal.load_asset('/Game/Office/Materials/Mi_Wall_01a')
 # Source coordinates are the campaign positions minus the office translation.
 for label,p,size in [('PartitionFront',(-712.825475,166,184.93),(30,4,393.86)),('EntryLeft',(-1005,-110,189.38),(30,4,378.76)),('EntryRight',(-1005,110,189.38),(30,4,378.76))]:
  cube('POLISH_Office_'+label,p,size,wall,'BM_Campaign_MainGameOffice')
 save_map()
 load('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1')
 count=0
 for a in api.get_all_level_actors():
  if not isinstance(a,unreal.StaticMeshActor):continue
  if a.get_actor_label().startswith('Ceiling_') or 'SM_ModularCeiling' in a.get_actor_label():
   for i in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(i,ceiling)
   count+=1
 assert count>10;report['museum_ceiling_parts']=count;save_map()
 # Starting source room walls use the same rough rock treatment as the tutorial.
 load('/Game/StartRoom/Maps/Lvl_StartRoom')
 for a in api.get_all_level_actors():
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   for i,m in enumerate(c.get_materials()):
    key=m.get_path_name().split('.')[0] if m else ''
    if key in relief and any(k in a.get_actor_label().lower() for k in ['wall','passage','bedrock','junction']):c.set_material(i,relief[key])
 save_map()
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
 (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
