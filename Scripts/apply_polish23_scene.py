"""Bind the authored props and bake a wall symbol from the actual sticky-bomb mesh."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Polish23';out.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def backup(rel):
 src=root/rel;dst=out/'Before'/rel
 if src.exists() and not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
backup(Path('Content/StartRoom/Maps/Lvl_StartRoom.umap'))
assert levels.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
bindings={'StaticMeshActor_100':'StartRoomView_BookRight','StaticMeshActor_129':'StartRoomView_BookLeft','StaticMeshActor_104':'StartRoomView_Toilet'}
for a in actors.get_all_level_actors():
 if a.get_name() in bindings:
  a.tags=[t for t in a.tags if not str(t).startswith('StartRoomView_')]+[unreal.Name(bindings[a.get_name()])]
  for c in a.get_components_by_class(unreal.PrimitiveComponent):
   if c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION:c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
   c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY,unreal.CollisionResponseType.ECR_BLOCK)
assert levels.save_current_level()
# Render the supplied gameplay mesh and its actual materials into a reusable wall icon.
assert levels.load_level('/Engine/Maps/Entry')
mesh=unreal.load_asset('/Game/Bomb/SM_StickyBomb');assert mesh
a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0));a.static_mesh_component.set_static_mesh(mesh)
for position,intensity in [((-70,-80,120),16),((70,70,80),8)]:
 light=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*position));lc=light.get_component_by_class(unreal.PointLightComponent);lc.set_intensity(intensity);lc.set_editor_property('cast_shadows',False);lc.set_attenuation_radius(500)
center,extent=a.get_actor_bounds(False)
cap=actors.spawn_actor_from_class(unreal.SceneCapture2D,center+unreal.Vector(-200,-150,120));c=cap.get_component_by_class(unreal.SceneCaptureComponent2D)
cap.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(cap.get_actor_location(),center),False)
c.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC);c.set_editor_property('ortho_width',max(extent.x,extent.y,extent.z)*3)
c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
c.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST);c.show_only_actor_components(a,False)
c.set_editor_property('capture_every_frame',False);c.set_editor_property('capture_on_movement',False)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rt=unreal.RenderingLibrary.create_render_target2d(w,512,512,unreal.TextureRenderTargetFormat.RTF_RGBA8,unreal.LinearColor(0,0,0,1))
c.texture_target=rt;c.capture_scene()
tex=unreal.load_asset('/Game/MainGame/UI/T_BombWallIcon')
if tex:unreal.RenderingLibrary.convert_render_target_to_texture2d_editor_only(w,rt,tex)
else:tex=unreal.RenderingLibrary.render_target_create_static_texture2d_editor_only(rt,'/Game/MainGame/UI/T_BombWallIcon',unreal.TextureCompressionSettings.TC_DEFAULT,unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
assert tex
tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT);tex.set_editor_property('srgb',False);EA.save_loaded_asset(tex)
task=unreal.AssetExportTask();task.object=tex;task.filename=str(out/'bomb-icon.png');task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.TextureExporterPNG()
assert unreal.Exporter.run_asset_export_task(task)
path='/Game/MainGame/Materials/M_BombWallIcon'
m=EA.load_asset(path)
if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_BombWallIcon','/Game/MainGame/Materials',unreal.Material,unreal.MaterialFactoryNew())
ML.delete_all_material_expressions(m);m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
t=ML.create_material_expression(m,unreal.MaterialExpressionTextureSample);t.texture=tex;t.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
inv=ML.create_material_expression(m,unreal.MaterialExpressionOneMinus);assert ML.connect_material_expressions(t,'A',inv,'')
ML.connect_material_property(inv,'',unreal.MaterialProperty.MP_OPACITY)
ML.connect_material_property(t,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
ML.connect_material_property(t,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ML.recompile_material(m);assert EA.save_loaded_asset(m)
(out/'assets.json').write_text(json.dumps({'bindings':bindings,'icon':tex.get_path_name(),'material':m.get_path_name(),'bounds':str((center,extent))},ensure_ascii=False,indent=2),encoding='utf-8')



bp=unreal.load_asset('/Game/EndingEmergence/UI/WBP_EndingCredits')
backup(Path('Content/EndingEmergence/UI/WBP_EndingCredits.uasset'))
for widget in unreal.ObjectIterator(unreal.TextBlock):
 if widget.get_path_name().startswith('/Game/EndingEmergence/UI/WBP_EndingCredits.') and widget.get_name()=='ThankYou':
  widget.set_text('恭喜你挖出来了！《挖哦》也因此诞生！感谢游玩！')
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert EA.save_loaded_asset(bp,False)
unreal.SystemLibrary.quit_editor()
