"""Add a smooth burn transition to the card room's Niagara cloud, preserving its source pack."""
import unreal,json,sys,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import node,wire,prop,custom,finish
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
system=unreal.load_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_Poison');assert system
out=root/'Saved/Validation/Playtest17';backup=out/'Before/NS_P2_Poison.uasset'
if not backup.exists():shutil.copy2(root/'Content/Passages/Echo/ArtP2/VFX/NS_P2_Poison.uasset',backup)
clones={};saved=[]
def copy_material(original):
 path=original.get_path_name()
 if path in clones:return clones[path]
 if '/PoisonBurn/' in path:return original
 dest='/Game/Rooms/CardDuel/Materials/PoisonBurn/'+original.get_name()
 existing=EA.does_asset_exist(dest)
 m=unreal.load_asset(dest) if existing else EA.duplicate_asset(path.split('.')[0],dest)
 assert m
 clones[path]=m
 if not existing:
  if isinstance(m,unreal.MaterialInstanceConstant):
   ML.set_material_instance_parent(m,copy_material(original.get_editor_property('parent')))
   assert EA.save_loaded_asset(m,False)
  else:
   burn=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='PoisonBurn',default_value=0.)
   for p in [unreal.MaterialProperty.MP_EMISSIVE_COLOR,unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_OPACITY,unreal.MaterialProperty.MP_OPACITY_MASK]:
    old=ML.get_material_property_input_node(m,p)
    if not old:continue
    output=ML.get_material_property_input_node_output_name(m,p)
    if p in [unreal.MaterialProperty.MP_EMISSIVE_COLOR,unreal.MaterialProperty.MP_BASE_COLOR]:
     code='float brightness=max(Old.r,max(Old.g,Old.b)); float3 heat=float3(3.2,.38,.012)*saturate(brightness*.5); return lerp(Old,heat,smoothstep(0,.3,Burn));'
     kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3
    else:
     code='return Old*(1-smoothstep(.15,1,Burn));'
     kind=unreal.CustomMaterialOutputType.CMOT_FLOAT1
    c=custom(m,code,{'Old':(old,output),'Burn':(burn,'')},kind);prop(c,p)
   finish(m)
  saved.append(dest)
 return m
renderers=[]
for renderer in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
 if 'NS_P2_Poison.NS_P2_Poison:' not in unreal.SystemLibrary.get_path_name(renderer):continue
 material=renderer.get_editor_property('material')
 if material:
  renderer.set_editor_property('material',copy_material(material));renderers.append(unreal.SystemLibrary.get_path_name(renderer))
count=unreal.MiningNiagaraEditorLibrary.bind_renderer_scalar(system,'PoisonBurn','User.PoisonBurn')
assert count>=12,count
assert unreal.MiningNiagaraEditorLibrary.compile_system(system)
assert EA.save_loaded_asset(system,False)
(out/'poison-burn-assets.json').write_text(json.dumps({'passed':True,'renderers':count,'saved':saved},indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
