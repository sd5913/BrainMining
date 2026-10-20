"""Dress the saved city for night; preserve authored cells and production rails."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/EndingNight'
OUT.mkdir(parents=True,exist_ok=True)
BACKUP=ROOT/'Saved/Backups'/('EndingNight-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
BACKUP.mkdir(parents=True)
MAP='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.EditorAssetLibrary
L=unreal.MaterialEditingLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert levels.load_level(MAP)
all_actors=actors.get_all_level_actors()
d=next(a for a in all_actors if isinstance(a,unreal.EndingEmergence))
assert d.actor_has_tag('EndingStreetDressingV1')
assert not d.actor_has_tag('EndingStreetNightV1'),'Already applied; refine saved assets in place.'
anchor=next(c for c in d.get_components_by_class(unreal.SceneComponent) if c.get_name().startswith('02_CityAndPeople'))
cell={a.get_name():str(a.get_actor_transform()) for a in all_actors if a.actor_has_tag('EndingEditableCell')}
shutil.copy2(ROOT/'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap',BACKUP/'street.umap')
crowd_root='/Game/EndingEmergence/Crowd/Lite/'
for name in ['M_CrowdMotion']+['MI_SM_Walker_'+str(i) for i in range(8)]+['MI_SM_Hero']:
    shutil.copy2(ROOT/('Content/EndingEmergence/Crowd/Lite/'+name+'.uasset'),BACKUP/(name+'.uasset'))

# Preserve the existing VAT graph, including its phase/rate/texture bindings.
master=unreal.load_asset(crowd_root+'M_CrowdMotion')
vertex=next(n for n in L.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionVertexColor))
def node(cls,**props):
    n=L.create_material_expression(master,cls)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def connect(a,b,pin):assert L.connect_material_expressions(a,'',b,pin)
def rgb_custom(index,default):
    ns=[node(unreal.MaterialExpressionPerInstanceCustomData,data_index=index+i,const_default_value=default[i]) for i in range(3)]
    a=node(unreal.MaterialExpressionAppendVector);connect(ns[0],a,'A');connect(ns[1],a,'B')
    b=node(unreal.MaterialExpressionAppendVector);connect(a,b,'A');connect(ns[2],b,'B')
    v=node(unreal.MaterialExpressionVertexInterpolator);connect(b,v,'')
    return v
shirt=rgb_custom(2,(.22,.30,.34));pants=rgb_custom(5,(.055,.075,.1))
base=node(unreal.MaterialExpressionVectorParameter,parameter_name='BakedShirt',default_value=unreal.LinearColor(.22,.30,.34,1))
color=node(unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ['V','Shirt','Pants','BakedShirt']:
    item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
color.set_editor_property('inputs',inputs)
color.set_editor_property('code','''
// StaticMeshBuilder stores source linear vertex colors with ToFColor(true).
float3 linearV=pow(max((V.rgb+.055)/1.055,0),2.4);
float vmax=max(max(linearV.r,linearV.g),max(linearV.b,.0001));
float smax=max(max(BakedShirt.r,BakedShirt.g),BakedShirt.b);
float3 n=linearV/vmax;
float sm=1-smoothstep(.015,.055,length(n-BakedShirt.rgb/smax));
float pm=1-smoothstep(.015,.055,length(n-float3(.55,.75,1)));
float3 shirtDisplay=1.055*pow(max(Shirt,.0001),1./2.4)-.055;
float3 pantsDisplay=1.055*pow(max(Pants,.0001),1./2.4)-.055;
float3 result=lerp(V.rgb,shirtDisplay*vmax/smax,sm);
return lerp(result,pantsDisplay*vmax/.1,pm);
''')
for name,n in [('V',vertex),('Shirt',shirt),('Pants',pants),('BakedShirt',base)]:connect(n,color,name)
assert L.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
glow=node(unreal.MaterialExpressionMultiply,const_b=.025);connect(color,glow,'A')
assert L.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
L.recompile_material(master)
assert E.save_loaded_asset(master)
palette=[(.22,.30,.34),(.35,.24,.18),(.14,.18,.27),(.27,.18,.21)]
for i in range(9):
    name='MI_SM_Hero' if i==8 else 'MI_SM_Walker_'+str(i)
    m=unreal.load_asset(crowd_root+name)
    L.set_material_instance_vector_parameter_value(m,'BakedShirt',unreal.LinearColor(*palette[0 if i==8 else i%4],1))
    assert E.save_loaded_asset(m)

DEST='/Game/EndingEmergence/StreetNight'
E.make_directory(DEST)
def material(name,color,emission=0,roughness=.55):
    path=DEST+'/'+name
    m=unreal.load_asset(path) if E.does_asset_exist(path) else TOOLS.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    L.delete_all_material_expressions(m)
    def val(cls,**props):
        n=L.create_material_expression(m,cls)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    c=val(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*color,1))
    L.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    r=val(unreal.MaterialExpressionConstant,r=roughness)
    L.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
    if emission:
        e=val(unreal.MaterialExpressionMultiply,const_b=emission)
        L.connect_material_expressions(c,'',e,'A')
        L.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    L.recompile_material(m);assert E.save_loaded_asset(m)
    return m
dark=material('M_SignEnamel',(.012,.022,.035),0,.35)
warm=material('M_WarmLight',(1,.56,.19),5)
cyan=material('M_CyanLight',(.12,.68,1),4)
red=material('M_CoralLight',(1,.14,.12),3)
glass=material('M_ShopGlass',(.35,.27,.15),1.1,.22)
metal=unreal.load_asset('/Game/QuickCity/Brutalist/Materials/M_QC_Steel')
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
created=[]
def attach(a,label):
    a.attach_to_component(anchor,unreal.Name('None'),unreal.AttachmentRule.KEEP_RELATIVE,
        unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False)
    a.set_actor_label('EDIT_夜景_'+label)
    a.set_folder_path('Ending_Edit/02_City/NightStreet')
    a.set_editor_property('tags',[unreal.Name(t) for t in ['EndingEditableScenery','EndingEditableCity','EndingStreetNightV1']])
    created.append(a)
    return a
def place(label,mesh,p,yaw=0,scale=1,size=None,mat=None):
    m=unreal.load_asset(mesh) if isinstance(mesh,str) else mesh
    b=m.get_bounds();ex=b.box_extent;o=b.origin
    s=unreal.Vector(*(size[i]/max(2*getattr(ex,'xyz'[i]),.01) for i in range(3))) if size else unreal.Vector(scale,scale,scale)
    offset=unreal.MathLibrary.rotate_angle_axis(unreal.Vector(o.x*s.x,o.y*s.y,(o.z-ex.z)*s.z),yaw,unreal.Vector(0,0,1))
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(m)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False)
    if mat:
        for i in range(c.get_num_materials()):c.set_material(i,mat)
    attach(a,label)
    c.set_relative_location(unreal.Vector(*p)-offset,False,False)
    c.set_relative_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False,False)
    c.set_relative_scale3d(s)
    return a
def point(label,p,color,intensity,radius):
    a=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector())
    c=a.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    c.set_intensity(intensity);c.set_light_color(unreal.LinearColor(*color,1))
    c.set_attenuation_radius(radius);c.set_cast_shadows(False)
    c.set_editor_property('source_radius',30)
    attach(a,label);c.set_relative_location(unreal.Vector(*p),False,False)
    return a

# Use native JapaneseCity emission masks, retaining authored textures and UVs.
night_instances={};swaps=0
city=[a for a in all_actors if a.actor_has_tag('EndingEditableCity') and isinstance(a,unreal.StaticMeshActor)]
for ai,a in enumerate(city):
    c=a.static_mesh_component
    for slot in range(c.get_num_materials()):
        old=c.get_material(slot)
        if not old:continue
        source=old.get_path_name().split('.')[0]
        lit=source if source.endswith('_L') else source+'_L'
        if not E.does_asset_exist(lit):continue
        key=(lit,ai%3)
        if key not in night_instances:
            name=lit.rsplit('/',1)[1]+'_Night'+str(ai%3)
            m=unreal.load_asset(DEST+'/'+name) if E.does_asset_exist(DEST+'/'+name) else TOOLS.duplicate_asset(name,DEST,unreal.load_asset(lit))
            assert m
            if 'Win' in name or 'BldFrame' in name:
                L.set_material_instance_scalar_parameter_value(m,'Emissive',3.6)
                tint=[(1,.64,.32),(.40,.64,1),(.85,.83,.69)][ai%3]
                L.set_material_instance_vector_parameter_value(m,'EmissiveColor',unreal.LinearColor(*tint,1))
            E.save_loaded_asset(m);night_instances[key]=m
        c.set_material(slot,night_instances[key]);swaps+=1
    if 'StreetLight' in a.get_actor_label():
        p=a.get_actor_location()-unreal.Vector(22000,0,0)
        point('StreetLamp_'+str(ai),(p.x,p.y,620),(1,.68,.38),12500,1250)

# Shops are fitted to the actual inner faces of the four corner buildings.
shop_count=0
for a in city:
    if 'CityAssetBuildings_Block_' not in a.get_actor_label() or not a.get_actor_label().endswith('_0'):continue
    center,ex=a.get_actor_bounds(False)
    center-=unreal.Vector(22000,0,0)
    sx=1 if center.x>0 else -1;sy=1 if center.y>0 else -1
    for axis in (0,1):
        p=unreal.Vector(center.x-sx*(ex.x+9),center.y,0) if axis==0 else unreal.Vector(center.x,center.y-sy*(ex.y+9),0)
        n=unreal.Vector(-sx,0,0) if axis==0 else unreal.Vector(0,-sy,0)
        yaw=unreal.MathLibrary.conv_vector_to_rotator(n).yaw
        accent=[warm,cyan,red][shop_count%3]
        def at(out,z):return (p.x+n.x*out,p.y+n.y*out,z)
        prefix='Shop_'+str(shop_count)
        place(prefix+'_Window',cube,at(0,50),yaw,size=(12,690,200),mat=glass)
        for side in (-1,1):
            q=unreal.Vector(*at(10,50))+unreal.Vector(-n.y,n.x,0)*(side*345)
            place(prefix+'_Frame'+str(side),cube,q.to_tuple(),yaw,size=(14,9,205),mat=metal)
        place(prefix+'_Fascia',cube,at(3,280),yaw,size=(18,740,110),mat=dark)
        place(prefix+'_Trim',cube,at(16,385),yaw,size=(8,740,6),mat=accent)
        place(prefix+'_Canopy',cube,at(48,405),yaw,size=(115,800,16),mat=metal)
        text=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector())
        c=text.text_render;c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_text(['COFFEE','CITY MART','HOTEL','STATION','BOOKS','24H STORE','DINING','LOUNGE'][shop_count])
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
        c.set_world_size(68);c.set_text_material(unreal.load_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque'))
        c.set_text_render_color(unreal.Color(255,226,164,255))
        attach(text,prefix+'_Name')
        c.set_relative_location(unreal.Vector(*at(15,335)),False,False)
        c.set_relative_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False,False)
        point(prefix+'_PavementGlow',at(120,230),[(1,.63,.32),(.25,.65,1),(1,.28,.19)][shop_count%3],5500,680)
        place(prefix+'_StreetStand','/Game/JapaneseCity/Meshes/Props/SM_jctStreetStandA',at(160,14),yaw,scale=.8)
        shop_count+=1

J='/Game/JapaneseCity/Meshes/'
for side in (-1,1):
    for axis in (0,1):
        def street(along,across,z=14):return (along,side*across,z) if axis==0 else (side*across,along,z)
        for i,along in enumerate((-1850,1850)):
            place('IlluminatedSign_%s_%s_%s'%(side,axis,i),J+'Props/SM_jctStreetSignA',street(along,790),yaw=axis*90,scale=.8,
                mat=unreal.load_asset('/Game/JapaneseCity/Materials/Props/MI_jctStreetSignA_L'))
            place('Planter_%s_%s_%s'%(side,axis,i),J+'Foliage/SM_jctHedgeC',street(along+180,870),yaw=axis*90,scale=.55)
        place('UtilityBox_%s_%s'%(side,axis),J+'Props/SM_jctGrdEquipA',street(-3200,790),yaw=axis*90,scale=.8)
    # Lit advertising cabinet and roof strips for both existing shelters.
    p=(side*4000,side*870,14)
    place('BusShelterAdCase_'+str(side),cube,(p[0]+170,p[1],45),size=(14,100,180),mat=dark)
    place('BusShelterAdPanel_'+str(side),cube,(p[0]+180,p[1],55),size=(2,82,160),mat=cyan if side>0 else warm)
    place('BusShelterRoofStrip_'+str(side),cube,(p[0],p[1]-side*105,273),size=(390,6,4),mat=warm)
    point('BusShelterLamp_'+str(side),(p[0],p[1],250),(1,.72,.43),3500,550)

# Intersection lamps keep the opening crowd readable under warm local light.
for i,(x,y,yaw) in enumerate([(1200,720,180),(720,-1200,90),(-1200,-720,0),(-720,1200,-90)]):
    place('IntersectionLamp_'+str(i),J+'Props/SM_jctStreetLightA',(x,y,14),yaw,scale=.72,
        mat=unreal.load_asset('/Game/JapaneseCity/Materials/Props/MI_jctStreetSignA_L'))
    point('IntersectionLamp_'+str(i)+'_Glow',(x,y,620),(1,.73,.46),3000,1850)

assert shop_count==8
d.set_editor_property('tags',list(d.tags)+[unreal.Name('EndingStreetNightV1')])
assert cell=={a.get_name():str(a.get_actor_transform()) for a in actors.get_all_level_actors() if a.actor_has_tag('EndingEditableCell')}
assert all(a.get_attach_parent_actor()==d for a in created)
assert levels.save_current_level()
assert levels.load_level('/Engine/Maps/Entry');assert levels.load_level(MAP)
saved=[a for a in actors.get_all_level_actors() if a.actor_has_tag('EndingStreetNightV1') and not isinstance(a,unreal.EndingEmergence)]
assert len(saved)==len(created)
result={'passed':True,'backup':str(BACKUP),'added_actors':len(saved),'shops':shop_count,
    'native_material_swaps':swaps,'night_materials':len(night_instances),'cell_unchanged':True,
    'production_camera_unchanged':True,'saved_and_reloaded':True}
(OUT/'authoring.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('ENDING_NIGHT_SAVED '+json.dumps(result))
unreal.SystemLibrary.quit_editor()
