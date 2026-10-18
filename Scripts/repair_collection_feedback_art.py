"""Rebuild only feedback materials and replace the exposed pipe with a small outlet."""
import unreal,ast,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/CollectionFeedback/art_repair.json';result={'passed':False}
try:
 code=(root/'Scripts/apply_collection_feedback.py').read_text(encoding='utf-8');env={};exec(code.split('\ntry:')[0],env)
 main=next(n for n in ast.parse(code).body if isinstance(n,ast.Try))
 for n in main.body:
  call=n.value if isinstance(n,(ast.Expr,ast.Assign)) else None
  if isinstance(call,ast.Call) and isinstance(call.func,ast.Name) and call.func.id=='material':exec(compile(ast.Module(body=[n],type_ignores=[]),'<feedback materials>','exec'),env)
 ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary
 purple=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_BlueViolet');ML.set_material_usage(purple,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES);ML.recompile_material(purple);EA.save_loaded_asset(purple)
 # UI-domain tone mapping for the HDR collection render target.
 path='/Game/CollectionPrototype/Materials/M_CollectionCataloguePreview'
 mat=unreal.load_asset(path) if EA.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CollectionCataloguePreview','/Game/CollectionPrototype/Materials',unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(mat);mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
 tex=ML.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D);tex.set_editor_property('parameter_name','Preview');tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
 custom=ML.create_material_expression(mat,unreal.MaterialExpressionCustom);custom.set_editor_property('code','return Color/(Color+1);');custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);inp=unreal.CustomInput();inp.set_editor_property('input_name','Color');custom.set_editor_property('inputs',[inp]);assert ML.connect_material_expressions(tex,'RGB',custom,'Color');ML.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);ML.recompile_material(mat);EA.save_loaded_asset(mat)
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);aa=list(api.get_all_level_actors());water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 for a in aa:
  if a.get_actor_label() in ['写实管道 · 路段00_1','写实弯管 · 转角1']:
   a.static_mesh_component.set_visibility(False);a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 original=next(a for a in aa if a.get_actor_label()=='写实弯管 · 转角1')
 outlet=next((a for a in aa if a.get_actor_label()=='COL_Heaven_SmallFaucet'),None)
 if not outlet:
  outlet=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.MathLibrary.transform_location(frame,unreal.Vector(-289,125,145)),original.get_actor_rotation());outlet.set_actor_label('COL_Heaven_SmallFaucet');outlet.set_editor_property('tags',['BM_MainGame_Collections','BM_CollectionFeedback','BM_Campaign_MainGame03']);outlet.set_actor_scale3d(original.get_actor_scale3d()*.6);outlet.static_mesh_component.set_static_mesh(original.static_mesh_component.static_mesh);outlet.static_mesh_component.set_material(0,original.static_mesh_component.get_material(0))
 assert unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'/Game/MainGame/Maps/Lvl_MainGame');result['passed']=True
except Exception:result['error']=traceback.format_exc();unreal.log_error(result['error'])
out.write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
