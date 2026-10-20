"""Apply the screenshot review corrections without replacing authored rooms."""
import unreal,json,sys,traceback,shutil,datetime
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts'))
from scene_polish_materials import mat,node,prop,custom,wire,finish
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);EA=unreal.EditorAssetLibrary;DL=unreal.MiningDesignerLibrary
OUT=ROOT/'Saved/Validation/SceneFeedback';OUT.mkdir(parents=True,exist_ok=True)
report={'passed':False,'restored_materials':[]};I=unreal.Transform();opt=unreal.GeometryScriptPrimitiveOptions()
BACK=ROOT/'Saved/Backups'/('SceneFeedback_'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));BACK.mkdir(parents=True)
def box(center,size):
 m=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_box(m,opt,unreal.Transform(location=center),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER);return m
def subtract(mesh,frame,tool):
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,frame,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
def mark(a,label):
 a.set_actor_label(label);a.tags=['BM_Campaign_MainGame03','BM_MainGame_Collections','BM_SceneFeedback'];return a
def actor(label,cls,p):
 a=next((a for a in A.get_all_level_actors() if a.get_actor_label()==label),None) or A.spawn_actor_from_class(cls,p)
 a.set_actor_location(p,False,True);return mark(a,label)
try:
 originals=['/Game/MainGame/Materials/DirtTransition/MI_DirtWall_StartRoom','/Game/MainGame/Materials/DirtTransition/MI_DirtWall_01','/Game/Campaign/Materials/M_CampaignLimestone','/Game/MainGame/Materials/RoutesV5/M_Earth']
 restore={p.rsplit('/',1)[1]+'_Relief':unreal.load_asset(p) for p in originals}
 for level in ['/Game/StartRoom/Maps/Lvl_StartRoom','/Game/MainGame/Maps/Lvl_MainGame']:
  shutil.copy2(ROOT/'Content'/(level[6:]+'.umap'),BACK/(level.rsplit('/',1)[1]+'.umap'));assert L.load_level(level)
  for a in A.get_all_level_actors():
   if isinstance(a,(unreal.DiggableRock,unreal.MiningExternalRegion)):
    m=a.get_editor_property('rock_material')
    if m and m.get_name() in restore:a.set_editor_property('rock_material',restore[m.get_name()],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);report['restored_materials'].append(a.get_actor_label())
   for c in a.get_components_by_class(unreal.MeshComponent):
    for index,m in enumerate(c.get_materials()):
     if m and m.get_name() in restore:c.set_material(index,restore[m.get_name()]);report['restored_materials'].append(a.get_actor_label()+':'+c.get_name())
  assert L.save_current_level()
 aa=list(A.get_all_level_actors());water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 shell=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell');center=shell.get_actor_location()
 # Match the visible disk and water centers, and overlap the perimeter slightly.
 water.set_actor_location(center+unreal.Vector(0,0,2),False,True);water.radius=2000.;water.rebuild_water()
 for a in aa:
  if isinstance(a,unreal.HeavenToilet):a.duck_scale=.54;a.spray_duck.set_relative_scale3d(unreal.Vector(.54))
 # Retain the HDR cloud photograph without the previous compression/warping.
 cloud=unreal.load_asset('/Game/Fab/Toilet/citrus_orchard_puresky_2k');assert isinstance(cloud,unreal.TextureCube)
 sky=unreal.load_asset('/Game/MainGame/Materials/ScenePolish/M_HeavenCloudSky');unreal.MaterialEditingLibrary.delete_all_material_expressions(sky)
 sky.set_editor_property('is_sky',True);sky.set_editor_property('two_sided',True);sky.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 view=node(sky,unreal.MaterialExpressionCameraVectorWS)
 direction=custom(sky,'float3 d=-normalize(V); return float3(-.766044*d.x+.642788*d.y,-.642788*d.x-.766044*d.y,d.z);',{'V':(view,'')})
 sample=node(sky,unreal.MaterialExpressionTextureSampleParameterCube,parameter_name='Sky',texture=cloud,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR);wire(direction,sample,'UVs')
 color=custom(sky,'return Color*10.0;',{'Color':(sample,'RGB')});prop(color,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 p=node(sky,unreal.MaterialExpressionWorldPosition);origin=node(sky,unreal.MaterialExpressionActorPositionWS)
 opening=custom(sky,'float3 q=P-C;return (q.y<0 && abs(q.x+231)<320 && q.z<440)?0:1;',{'P':(p,'XYZ'),'C':(origin,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1);prop(opening,unreal.MaterialProperty.MP_OPACITY_MASK);finish(sky)
 exposure=next(a for a in aa if a.get_actor_label()=='COL_Heaven_LocalExposure');settings=exposure.settings
 settings.ambient_cubemap=cloud;settings.override_ambient_cubemap_intensity=True;settings.ambient_cubemap_intensity=.12;exposure.settings=settings
 # A six-metre collar overlaps the old branch and dome, with the same clear bore.
 collar=actor('COL_Heaven_SeamCollar',unreal.GeneratedMiningRock,center)
 mesh=box(unreal.MathLibrary.inverse_transform_location(collar.get_actor_transform(),unreal.MathLibrary.transform_location(frame,unreal.Vector(600,0,235))),unreal.Vector(940,1000,690))
 bore=box(unreal.MathLibrary.transform_location(frame,unreal.Vector(600,0,235)),unreal.Vector(640,1200,450));subtract(mesh,collar.get_actor_transform(),bore)
 cavity=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_cylinder(cavity,opt,unreal.Transform(location=center+unreal.Vector(0,0,2120)),radius=1980,height=4360,radial_steps=64,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER);subtract(mesh,collar.get_actor_transform(),cavity)
 collar.set_editor_property('rock_material',shell.rock_material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);assert DL.set_campaign_rock_source(collar,mesh)
 # Explicit water-drop geometry remains readable against dark rock at game exposure.
 drip=next(a for a in aa if a.get_actor_label()=='COL_Heaven_ExposedClueDrip');nozzle=drip.get_actor_location()
 for a in aa:
  if a.get_actor_label().startswith(('COL_Heaven_ClueDripExtra_','COL_Heaven_ExposedClueDrip')):A.destroy_actor(a)
 drip=actor('COL_Heaven_ExposedClueDrip',unreal.HeavenPipeDrip,nozzle);drip.fall_distance=184.
 # Restore the exact two authored Niagara placements and their original scale.
 for i,p in enumerate([(-10500,0,8000),(-6300,0,10400)]):
  a=actor('MAIN_GAME_CrystalMotes_'+str(i+1),unreal.NiagaraActor,unreal.Vector(*p));a.tags=['BM_Campaign_MainGame02','BM_RoutesV5'];a.set_actor_scale3d(unreal.Vector(3,3,3));a.get_component_by_class(unreal.NiagaraComponent).set_asset(unreal.load_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote'))
 # Original ceiling aperture is 440 x 440 cm; preserve the added digging depth.
 soil=next(a for a in aa if a.get_actor_label()=='MAIN_GAME_Museum_DownwardSoil')
 mesh=box(unreal.MathLibrary.inverse_transform_location(soil.get_actor_transform(),unreal.Vector(47200,11750,15220)),unreal.Vector(440,440,840));assert DL.set_campaign_rock_source(soil,mesh)
 # Put the guide on the player's side of the excavation face, below the ceiling.
 light=next(a for a in aa if a.get_actor_label()=='POLISH_StageSoil_Guide')
 light.set_actor_location(unreal.Vector(32900,11550,20200),False,True);light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(light.get_actor_location(),unreal.Vector(33450,11750,20120)),False)
 c=light.light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity_units(unreal.LightUnits.CANDELAS);c.set_intensity(900);c.set_attenuation_radius(1200);c.set_outer_cone_angle(50);c.set_inner_cone_angle(15);c.set_cast_shadows(False)
 report.update(passed=True,water_radius=water.radius,museum_bounds=str(soil.get_actor_bounds(False)),collar_triangles=DL.get_campaign_rock_source(collar).get_triangle_count(),sky_texture=cloud.get_path_name());assert L.save_current_level()
except Exception:report['error']=traceback.format_exc();report['passed']=False
(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
