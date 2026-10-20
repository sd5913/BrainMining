"""Scoped UI/effect fixes; preserves designer widget positions and unrelated assets."""
import json,shutil,sys
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,custom,finish
out=root/'Saved/Validation/TraversalFeedback';out.mkdir(parents=True,exist_ok=True)
backup=out/'Before';backup.mkdir(exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
report={'saved':[]}
def protect(path):
    f=root/'Content'/(path.split('.')[0][6:]+'.uasset');dest=backup/f.relative_to(root)
    if f.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
def save(a):
    assert EA.save_loaded_asset(a,False),a.get_path_name()
    report['saved'].append(a.get_path_name())

path='/Game/Subtitles/Collectibles/DA_CollectiblePickup_01';protect(path)
s=unreal.load_asset(path);lines=list(s.lines);lines[0].set_editor_property('text','蚂蚁一直挖 挖出了复杂的通路');lines[0].set_editor_property('audio',None);s.set_editor_property('lines',lines);save(s)

path='/Game/CollectionPrototype/Materials/M_CollectionFeatherFade';protect(path);m=mat(path)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.5,.5,.5,1))
prop(tint,unreal.MaterialProperty.MP_BASE_COLOR,'RGB')
prop(node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1.),unreal.MaterialProperty.MP_OPACITY)
prop(node(m,unreal.MaterialExpressionConstant,r=.9),unreal.MaterialProperty.MP_ROUGHNESS);finish(m);report['saved'].append(path)

path='/Game/MainGame/Materials/M_PlayerFootprint';protect(path);m=mat(path)
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
uv=node(m,unreal.MaterialExpressionTextureCoordinate)
shape=custom(m,'''float2 p=UV*2-1;
float toe=1-smoothstep(.87,1,length((p-float2(0,-.25))/float2(.76,.63)));
float heel=1-smoothstep(.85,1,length((p-float2(0,.62))/float2(.54,.27)));
float tread=lerp(.62,1,step(.30,frac((p.y+1)*5)));
return saturate(max(toe*tread,heel));''',{'UV':(uv,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
life=node(m,unreal.MaterialExpressionDecalLifetimeOpacity)
opacity=node(m,unreal.MaterialExpressionMultiply);wire(shape,opacity,'A');wire(life,opacity,'B');prop(opacity,unreal.MaterialProperty.MP_OPACITY)
glow=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Glow',default_value=unreal.LinearColor(5,5,5,1));prop(glow,unreal.MaterialProperty.MP_EMISSIVE_COLOR,'RGB')
finish(m);report['saved'].append(path)

path='/Game/UI/MiningGUI/Immersive/M_ArtworkOuterRim';protect(path);m=unreal.load_asset(path)
changed=False
for n in ML.get_material_expressions(m):
    if not isinstance(n,unreal.MaterialExpressionCustom):continue
    code=n.get_editor_property('code')
    if 'float filled=' not in code:continue
    start=code.index('float iconTop=') if 'float iconTop=' in code else code.index('float filled=');end=code.index('\nreturn float4',start)
    # T_BombAbility has its square at rows 114..774 of 1080, and G at 821..990.
    code=code[:start]+'''float iconTop=114.0/1080.0, iconBottom=774.0/1080.0;
float level=lerp(iconBottom,iconTop,saturate(RechargeProgress));
float inIcon=step(iconTop,UV.y)*step(UV.y,iconBottom);
float filled=step(level,UV.y);
float chargeEdge=(1-smoothstep(0,.012,abs(UV.y-level)))*step(.001,RechargeProgress)*step(RechargeProgress,.999);
float3 chargedIcon=lerp(color*.17,float3(.94,.27,.1),filled)+chargeEdge*.6;
float3 charge=lerp(color,chargedIcon,inIcon);'''+code[end:]
    n.set_editor_property('code',code);changed=True
assert changed;ML.recompile_material(m);save(m)

# Keep editable UMG controls and all layout positions, replace only their brushes/colors.
path='/Game/CollectionPrototype/UI/WBP_CollectionInspect';protect(path);bp=unreal.load_asset(path)
gold=unreal.LinearColor(1,.64,.015,1);cream=unreal.LinearColor(.94,.93,.88,1)
def brush(name,margin):
    return unreal.SlateBrush(resource_object=unreal.load_asset('/Game/UI/MenuStyle/T_Menu'+name),draw_as=unreal.SlateBrushDrawType.BOX,margin=margin)
for w in unreal.ObjectIterator(unreal.Widget):
    if not w.get_outer() or w.get_outer().get_outer()!=bp:continue
    name=w.get_name()
    if isinstance(w,unreal.Image):
        if name=='InfoBackground':w.set_brush(brush('Panel',unreal.Margin(.025,.0334,.025,.0334)))
        elif name.startswith('ItemArt'):w.set_brush(brush('Row',unreal.Margin(.02,.25,.02,.25)))
        elif name.startswith('ItemSelected'):w.set_brush(brush('Hover',unreal.Margin(.02,.25,.02,.25)))
        elif name=='DescriptionDivider':w.set_color_and_opacity(gold)
    if isinstance(w,unreal.TextBlock):
        if name in ['CollectionTitle','ItemName']:w.set_color_and_opacity(unreal.SlateColor(specified_color=gold))
        elif name=='ItemDescription':w.set_color_and_opacity(unreal.SlateColor(specified_color=cream))
    if isinstance(w,unreal.Button) and name in ['PreviousButton','NextButton','CloseButton']:
        style=w.get_editor_property('widget_style')
        normal=style.normal;normal.tint_color=unreal.SlateColor(specified_color=gold)
        style.normal=normal;style.hovered=normal;style.pressed=normal;style.disabled=normal;w.set_style(style)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)

# Minerals use Nanite models and instanced clue fragments in cooked games.
# Save their required shader permutations instead of relying on editor auto-compilation.
parents=set()
diagnosis=json.loads((out/'diagnosis.json').read_text(encoding='utf-8'))
paths={x['material'].split('.')[0] for x in diagnosis['terrain'] if x['material']}
paths.update(['/Game/Showcase/Treasure/Materials/M_TreasureMineral','/Game/MainGame/Materials/ScenePolish/M_CollectibleDetail'])
for path in sorted(paths):
    m=unreal.load_asset(path)
    while isinstance(m,unreal.MaterialInstanceConstant):m=m.get_editor_property('parent')
    if not isinstance(m,unreal.Material) or m.get_path_name() in parents:continue
    parents.add(m.get_path_name());protect(m.get_path_name())
    m.set_editor_property('used_with_instanced_static_meshes',True)
    m.set_editor_property('used_with_nanite',True)
    ML.recompile_material(m);save(m)
mineral=unreal.load_asset('/Game/Showcase/Treasure/Materials/M_TreasureMineral')
for path in EA.list_assets('/Game/Showcase/Treasure/Meshes'):
    mesh=unreal.load_asset(path)
    if not isinstance(mesh,unreal.StaticMesh):continue
    protect(path)
    slots=list(mesh.static_materials)
    for slot in slots:slot.material_interface=mineral
    mesh.set_editor_property('static_materials',slots);save(mesh)
report['passed']=True
(out/'assets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('TRAVERSAL_ASSETS_COMPLETE')
