import unreal,json,sys,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts'))
from scene_polish_materials import mat,node,prop,finish,custom,wire
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);EA=unreal.EditorAssetLibrary
report={'passed':False,'windows':[]}
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 sky=next((a for a in A.get_all_level_actors() if 'HDRIBackdrop' in a.get_class().get_name()),None)
 source='/Game/Office/Textures/TX_Clouds_HDRI';target='/Game/MainGame/Materials/ScenePolish/T_HeavenClouds'
 cloud=unreal.load_asset(target) if EA.does_asset_exist(target) else EA.duplicate_asset(source,target)
 assert isinstance(cloud,unreal.TextureCube);cloud.set_editor_property('max_texture_size',1024);assert EA.save_loaded_asset(cloud,False)
 report['sky_before']=str(sky.get_editor_property('Cubemap')) if sky else 'Local cloud shell'
 frame=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.HeavenToiletWater)).spill_coordinate_frame
 # Preserve the latest source dome dimensions. The matching rock clearance is
 # rebuilt by fit_scene_polish_heaven.py, rather than shrinking this room.
 if sky:A.destroy_actor(sky)
 sky_mat=mat('/Game/MainGame/Materials/ScenePolish/M_HeavenCloudSky');sky_mat.set_editor_property('two_sided',True);sky_mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 p=node(sky_mat,unreal.MaterialExpressionWorldPosition);origin=node(sky_mat,unreal.MaterialExpressionActorPositionWS)
 direction=custom(sky_mat,'float3 d=normalize(P-C);d.z=abs(d.z)*.7+.2;return normalize(d);',{'P':(p,'XYZ'),'C':(origin,'')})
 sample=node(sky_mat,unreal.MaterialExpressionTextureSampleParameterCube,parameter_name='Sky',texture=cloud,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR);wire(direction,sample,'UVs')
 color=custom(sky_mat,'return 24*Color/(1+Color);',{'Color':(sample,'RGB')});prop(color,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 sky_mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
 opening=custom(sky_mat,'float3 q=P-C;return (q.y<0 && abs(q.x+231)<360 && q.z<470)?0:1;',{'P':(p,'XYZ'),'C':(origin,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 prop(opening,unreal.MaterialProperty.MP_OPACITY_MASK);finish(sky_mat)
 label='COL_Heaven_InteriorCloudSky';a=next((a for a in A.get_all_level_actors() if a.get_actor_label()==label),None) or A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
 a.set_actor_label(label);a.tags=['BM_MainGame_Collections','BM_Campaign_MainGame03','BM_ScenePolishHeaven'];a.set_actor_location(unreal.MathLibrary.transform_location(frame,unreal.Vector(2641,-231,9)),False,True);a.set_actor_scale3d(unreal.Vector(38.414,38.414,82.412))
 c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));c.set_material(0,sky_mat);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False);c.set_cast_shadow(False)
 c.set_editor_property('visible_in_ray_tracing',False)
 for floor in A.get_all_level_actors():
  if floor.get_actor_label().startswith('Heaven circular floor'):floor.static_mesh_component.set_visibility(True)
 for a in A.get_all_level_actors():
  if isinstance(a,unreal.HeavenToilet):
   report['duck_before']={k:str(a.get_editor_property(k)) for k in ['duck_forward_speed','duck_up_speed','duck_gravity','duck_scale']}
   a.duck_forward_speed=480.;a.duck_up_speed=300.;a.duck_gravity=620.
 assert L.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc()
(ROOT/'Saved/Validation/ScenePolish/lighting_finish.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
