import unreal,json,shutil,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,custom,finish
out=root/'Saved/Validation/Playtest16';out.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
def protect(path):
 f=root/'Content'/(path.split('.')[0][6:]+'.uasset');dest=out/'Before'/f.relative_to(root)
 if f.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
def save(a):assert EA.save_loaded_asset(a,False)
path='/Game/MainGame/Materials/M_PlayerFootprint';protect(path);m=mat(path)
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
uv=node(m,unreal.MaterialExpressionTextureCoordinate);life=node(m,unreal.MaterialExpressionDecalLifetimeOpacity)
shape=custom(m,'return 1-smoothstep(.80,1.,length((UV*2-1)/float2(.83,.92)));',{'UV':(uv,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
alpha=custom(m,'return Shape*saturate(Life);',{'Shape':(shape,''),'Life':(life,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
prop(alpha,unreal.MaterialProperty.MP_OPACITY)
emissive=custom(m,'return float3(.7,.75,.78)*Shape*saturate(Life);',{'Shape':(shape,''),'Life':(life,'')})
prop(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
path='/Game/StartRoom/UI/M_OpeningTitleDecal';protect(path);m=unreal.load_asset(path)
tex=next(n for n in ML.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionTextureSample))
life=next(n for n in ML.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionDecalLifetimeOpacity))
# Mask the emitted light as well as decal opacity; transparent texels can never
# contribute the texture's white RGB background during the fade.
color=custom(m,'return RGB*Alpha*saturate(Fade);',{'RGB':(tex,'RGB'),'Alpha':(tex,'A'),'Fade':(life,'')})
prop(color,unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
for name,text in [('Pear','哦！美味被你发现~'),('Plant','祝我好运')]:
 path='/Game/Subtitles/DA_StartRoom'+name;protect(path)
 a=unreal.load_asset(path) if EA.does_asset_exist(path) else None
 if not a:
  factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.SubtitleSegment)
  a=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_StartRoom'+name,'/Game/Subtitles',unreal.SubtitleSegment,factory)
 a.set_editor_property('segment_id','StartRoomView_'+name)
 line=unreal.SubtitleLine();line.set_editor_property('text',text);line.set_editor_property('audio',None)
 a.set_editor_property('lines',[line]);save(a)
for path in ['/Game/CollectionPrototype/UI/WBP_CollectionInspect','/Game/EndingEmergence/UI/WBP_EndingCredits']:
 protect(path);bp=unreal.load_asset(path)
 for w in unreal.ObjectIterator(unreal.Widget):
  if not w.get_outer() or w.get_outer().get_outer()!=bp:continue
  name=w.get_name()
  if isinstance(w,unreal.TextBlock):
   texts={'CollectionTitle':'藏品','QuoteChinese':'找到出口的最好方式，就是向前挖。','Prompt':'[C] 查看藏品     [ESC] 退出游戏'}
   if name in texts:w.set_text(texts[name])
  offsets={'ItemName':(64,340,400,80),'ItemDescription':(70,458,388,225),'DescriptionDivider':(70,430,160,1)}
  if name in offsets and isinstance(w.slot,unreal.CanvasPanelSlot):
   w.slot.set_offsets(unreal.Margin(*offsets[name]))
  if name=='ItemDescription':w.set_auto_wrap_text(True)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
(out/'ui_materials.json').write_text(json.dumps(dict(passed=True,footprint_hold=3,footprint_fade=12,shape='single ellipse',quote='找到出口的最好方式，就是向前挖。'),ensure_ascii=False,indent=2),encoding='utf8')
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
rows=[]
for a in A.get_all_level_actors():
 p=a.get_actor_location()
 if isinstance(a,(unreal.GeneratedMiningRock,unreal.DynamicMeshActor)) or (-17100<p.x<-15800 and 'Wall' in a.get_actor_label()):
  rows.append(dict(name=a.get_name(),label=a.get_actor_label(),path=a.get_path_name(),pos=p.to_tuple(),bounds=str(a.get_actor_bounds(False)),tags=[str(t) for t in a.tags]))
(out/'geometry.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
oil=unreal.load_asset('/Game/MouseMuseum/MI_EndingOilPaint')
(out/'oil.txt').write_text(str(oil.get_editor_property('scalar_parameter_values')),encoding='utf8')
unreal.SystemLibrary.quit_editor()
