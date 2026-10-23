"""Polish29: use HUD blast artwork, dark boundary rock, relocate pigeon and copper together."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Polish29';out.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'saved':[],'boundaries':[]}
def backup(path,ext='uasset'):
 rel=Path('Content')/(path.removeprefix('/Game/')+'.'+ext);src=root/rel;dst=out/'Before'/rel
 if src.exists() and not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def save(obj):
 assert EA.save_loaded_asset(obj,False);report['saved'].append(obj.get_path_name())
def setp(obj,k,v):obj.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def mat(path):
 backup(path)
 m=unreal.load_asset(path) if EA.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m);return m
def constant(m,value,prop):
 n=ML.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=value;assert ML.connect_material_property(n,'',prop)
def color(m,value,prop):
 n=ML.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*value);assert ML.connect_material_property(n,'',prop)
# Matte, slightly worn stencil: the same explosion silhouette as the ability button.
m=mat('/Game/MainGame/Materials/M_BombWallIcon');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
t=ML.create_material_expression(m,unreal.MaterialExpressionTextureSample);t.texture=unreal.load_asset('/Game/UI/MiningGUI/Art/T_IconBlast')
t.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
uv=ML.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
custom=ML.create_material_expression(m,unreal.MaterialExpressionCustom);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inputs=[]
for name in ['Mask','UV','Grunge']:
 i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
custom.set_editor_property('inputs',inputs);custom.set_editor_property('code',"""
float grain=frac(sin(dot(floor(UV*190),float2(12.9898,78.233)))*43758.5453);
float worn=smoothstep(.19,.73,Grunge*.78+grain*.22);
float edge=1.-smoothstep(.22,.70,length(UV-.5));
float wash=lerp(1.,.62,saturate(UV.y));
return Mask*(.09+.46*worn)*edge*wash;
""")
grunge=ML.create_material_expression(m,unreal.MaterialExpressionTextureSample)
grunge.texture=unreal.load_asset('/Game/DBStylizedSketchShader/Textures/T_GrungeA_MASK')
grunge.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
assert ML.connect_material_expressions(grunge,'R',custom,'Grunge')
assert ML.connect_material_expressions(t,'A',custom,'Mask');assert ML.connect_material_expressions(uv,'',custom,'UV');assert ML.connect_material_property(custom,'',unreal.MaterialProperty.MP_OPACITY)
color(m,(.66,.68,.70),unreal.MaterialProperty.MP_BASE_COLOR);constant(m,.95,unreal.MaterialProperty.MP_ROUGHNESS)
ML.recompile_material(m);save(m)
# A distinct charcoal surface, rather than the matching colored soil on an invincible boundary.
bedrock=mat('/Game/MainGame/Materials/M_BedrockDark');bedrock.set_editor_property('two_sided',True)
color(bedrock,(.009,.013,.021),unreal.MaterialProperty.MP_BASE_COLOR);constant(bedrock,.96,unreal.MaterialProperty.MP_ROUGHNESS);constant(bedrock,.12,unreal.MaterialProperty.MP_SPECULAR)
ML.recompile_material(bedrock);save(bedrock)
backup('/Game/MainGame/Maps/Lvl_MainGame','umap');assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
aa=list(api.get_all_level_actors());boundary=set()
for a in aa:
 if isinstance(a,unreal.MiningExternalRegion):
  setp(a,'shell_material',bedrock)
  if a.generated_shell:boundary.add(a.generated_shell)
 label=a.get_actor_label().lower()
 if '不可挖外壳' in label or ('bedrock' in label or 'uppershellseal' in label) and not isinstance(a,unreal.DiggableRock):boundary.add(a)
for a in boundary:
 for c in a.get_components_by_class(unreal.MeshComponent):
  for i in range(c.get_num_materials()):c.set_material(i,bedrock)
 report['boundaries'].append(a.get_actor_label())
field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.actor_has_tag('BM_MainGame_Collections'))
minerals=next(a for a in aa if isinstance(a,unreal.MiningTreasureField))
if not field.actor_has_tag('BM_Polish29_PigeonRelocated'):
 collection=list(field.authored_placements);bird=next(p for p in collection if p.item_id==3)
 deposits=list(minerals.get_editor_property('authored_placements'));copper=deposits[55]
 assert str(copper.treasure_id)=='2' and abs(copper.surface.translation.x+5013.716)<2
 old_clue=unreal.Transform();old_clue.import_text(bird.clue_transform.export_text())
 old_host=bird.host_rock
 surface=unreal.Vector(*copper.surface.translation.to_tuple());inside=unreal.Vector(*copper.transform.translation.to_tuple())
 inward=unreal.MathLibrary.find_look_at_rotation(surface,inside)
 new_clue=unreal.Transform(location=surface,rotation=inward)
 setp(bird,'clue_transform',new_clue)
 setp(bird,'transform',unreal.Transform(location=unreal.MathLibrary.transform_location(new_clue,unreal.Vector(360,0,0)),rotation=inward))
 setp(bird,'approach_transform',unreal.Transform(location=surface+unreal.Vector(-310,0,-170),rotation=inward))
 setp(bird,'host_rock',next(a for a in aa if a.get_name()=='GeneratedMiningRock_58'))
 # Swap the mineral into the previous pigeon bank, preserving its save placement ID.
 # Mineral surface frames face out; collectible clue frames face into the bank.
 old_surface=unreal.Transform();old_surface.import_text(copper.surface.export_text())
 target=unreal.Transform(location=old_clue.translation,rotation=unreal.MathLibrary.find_look_at_rotation(old_clue.translation,unreal.MathLibrary.transform_location(old_clue,unreal.Vector(-100,0,0))))
 def move(pos):return unreal.MathLibrary.transform_location(target,unreal.MathLibrary.inverse_transform_location(old_surface,pos))
 for name in ['transform','surface']:
  t=copper.get_editor_property(name);t.translation=move(t.translation)
  if name=='surface':t.rotation=target.rotation
  setp(copper,name,t)
 fragments=list(copper.fragments)
 for t in fragments:t.translation=move(t.translation)
 setp(copper,'fragments',fragments);setp(copper,'fragment_supports',[move(v) for v in copper.fragment_supports]);setp(copper,'hosts',[old_host])
 setp(minerals,'authored_placements',deposits);setp(field,'authored_placements',collection);setp(field,'tags',list(field.tags)+['BM_Polish29_PigeonRelocated'])
 report['pigeon']={'clue':list(surface.to_tuple()),'body':list(bird.transform.translation.to_tuple()),'copper_index':55,'moved_copper':list(copper.surface.translation.to_tuple())}
assert levels.save_current_level();report['saved'].append('/Game/MainGame/Maps/Lvl_MainGame')
(out/'scene-changes.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
