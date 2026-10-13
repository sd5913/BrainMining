"""Upgrade ONLY the dedicated Heaven Toilet map to the 60% shallow-water variant.

Idempotent migration: authored entrance/soil stays in place; already migrated room
actors are not scaled again. A pre-migration umap backup is saved under Saved.
Run in a dedicated editor commandlet after compiling HeavenToiletWater.
"""
import math
import shutil
from pathlib import Path
import unreal

ROOT='/Game/CollectionPrototype/HeavenToilet'
MAP=ROOT+'/Maps/Lvl_HeavenToilet'
CENTER=2580.0
RADIUS=1320.0
HEIGHT=48.0
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
assetlib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
root=Path(unreal.Paths.project_dir())
backup=root/'Saved/Backups/HeavenToilet_BeforeWater.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(root/'Content/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet.umap',backup)

def scalar(m,value,name=None):
    n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter if name else unreal.MaterialExpressionConstant)
    if name:
        n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value)
    else:n.set_editor_property('r',value)
    return n

def color(m,value,name=None):
    n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter if name else unreal.MaterialExpressionConstant3Vector)
    if name:n.set_editor_property('parameter_name',name)
    n.set_editor_property('default_value' if name else 'constant',unreal.LinearColor(*value,1))
    return n

def prop(m,n,target):assert lib.connect_material_property(n,'',target)

def custom(m,code,extra=None):
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sources={'P':(lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition),'XYZ'),
             'T':(lib.create_material_expression(m,unreal.MaterialExpressionTime),'')}
    for k,v in (extra or {}).items():sources[k]=(v,'')
    inputs=[]
    for key in sources:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',key);inputs.append(inp)
    n.set_editor_property('inputs',inputs);n.set_editor_property('code',code)
    for key,(src,pin) in sources.items():assert lib.connect_material_expressions(src,pin,n,key)
    return n

def water_material(name,flow=False):
    path=ROOT+'/Materials/'+name
    # Preserve compiled custom outputs on subsequent migration runs.
    if assetlib.does_asset_exist(path):
        m=unreal.load_asset(path)
        if assetlib.get_metadata_tag(m,'HeavenWaterStableDepth')!='1':
            # Keep the depth prepass and surface identical. Dynamic-mesh WPO
            # caused stippled depth artifacts in the project's UE 5.8 water pass.
            prop(m,color(m,(0,0,0)),unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
            lib.recompile_material(m)
            assetlib.set_metadata_tag(m,'HeavenWaterStableDepth','1')
            assetlib.save_loaded_asset(m)
        return m
    m=assets.create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property('tangent_space_normal',False)
    m.set_editor_property('two_sided',True)
    prop(m,color(m,(.006,.018,.021)),unreal.MaterialProperty.MP_BASE_COLOR)
    prop(m,scalar(m,.10 if flow else .075),unreal.MaterialProperty.MP_ROUGHNESS)
    prop(m,scalar(m,.5),unreal.MaterialProperty.MP_SPECULAR)
    prop(m,scalar(m,.035),unreal.MaterialProperty.MP_OPACITY)
    strength=scalar(m,1 if flow else 0,'FlowStrength')
    wader=color(m,(0,0,0),'Wader')
    wake=scalar(m,0,'WakeStrength')
    normal=custom(m,'''
float2 q=P.xy;
float a=dot(q,float2(.032,.021))+T*.85;
float b=dot(q,float2(-.048,.027))-T*1.15;
float c=P.x*.11+sin(P.y*.065)*1.8+T*(4.0+Flow*5.0);
float2 n=float2(.048*cos(a)-.035*cos(b),.032*cos(a)+.038*cos(b));
n+=Flow*float2(.13*cos(c),.06*sin(c+q.y*.1));
float2 d=q-Wader.xy; float r=length(d);
n+=Wake*normalize(d+float2(.001,.001))*.15*sin(r*.19-T*7.0)*exp(-r*.013);
return normalize(float3(n,1));
''',{'Flow':strength,'Wader':wader,'Wake':wake})
    prop(m,normal,unreal.MaterialProperty.MP_NORMAL)
    # Animated physical water normals preserve a stable water depth prepass.
    w=lib.create_material_expression(m,unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    for key,n in [('ScatteringCoefficients',color(m,(.00008,.00017,.00020))),('AbsorptionCoefficients',color(m,(.0020,.00065,.00032))),('PhaseG',scalar(m,.18)),('ColorScaleBehindWater',color(m,(1,1,1)))]:
        assert lib.connect_material_expressions(n,'',w,key),key
    lib.recompile_material(m)
    assetlib.set_metadata_tag(m,'HeavenWaterStableDepth','1')
    assert assetlib.save_loaded_asset(m)
    return m

water_mat=water_material('M_HeavenShallowWater')
flow_mat=water_material('M_HeavenOutletWater',True)
assert levels.load_level(MAP)
actors=editor.get_all_level_actors()
marker=next((a for a in actors if a.get_actor_label()=='Heaven hemisphere - diameter 26.4m height 13.2m'),None)
if marker is None:
    for a in actors:
        label=a.get_actor_label()
        if label.startswith(('Heaven hemisphere','Heaven circular floor','Heaven dome latitude','Heaven curved meridian','Heaven lower cove')):
            p=a.get_actor_location()
            a.set_actor_location(unreal.Vector(1260+(p.x-1260)*.6,p.y*.6,p.z*.6),False,True)
            a.set_actor_scale3d(a.get_actor_scale3d()*.6)
        elif label.startswith(('Heaven dome wash','Heaven zenith','Heaven central key','Heaven central cool fill')):
            p=a.get_actor_location()
            a.set_actor_location(unreal.Vector(1260+(p.x-1260)*.6,p.y*.6,p.z*.6),False,True)
            c=a.light_component
            c.set_intensity(c.get_editor_property('intensity')*.36)
            c.set_editor_property('attenuation_radius',c.get_editor_property('attenuation_radius')*.6)
    dome=next(a for a in actors if a.get_actor_label().startswith('Heaven hemisphere'))
    dome.set_actor_label('Heaven hemisphere - diameter 26.4m height 13.2m')

# Rebuild only this room's pipe/center dressing, at full human prop scale.
for a in editor.get_all_level_actors():
    if a.get_actor_label().startswith(('Heaven continuous water pipe','Heaven pipe elbow','Heaven pipe coupling','Heaven floor pipe coupling',
            'Heaven central porcelain dais','Heaven central brass inlay','Heaven floor halo','Heaven suspended halo','Heaven dais step','Heaven underwater light')):
        editor.destroy_actor(a)

def mat(name):return unreal.load_asset(ROOT+'/Materials/'+name)
def mesh(label,kind,pos,size,material,collision=False,folder='Heaven/Pipe'):
    a=editor.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos))
    a.set_actor_label(label);a.set_folder_path(folder)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+kind))
    a.static_mesh_component.set_material(0,material)
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    a.set_actor_scale3d(unreal.Vector(*size));return a
def segment(label,p,q,r,material,folder='Heaven/Pipe'):
    p,q=unreal.Vector(*p),unreal.Vector(*q)
    d=q-p;c=(p+q)*.5
    a=mesh(label,'Cylinder',(c.x,c.y,c.z),(r/50,r/50,d.length()/100),material,folder=folder)
    a.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(d),False)
def ring(label,z,radius,tube,material):
    for i in range(64):
        a,b=i*math.tau/64,(i+1)*math.tau/64
        segment(label+' %02d'%i,(CENTER+radius*math.cos(a),radius*math.sin(a),z),(CENTER+radius*math.cos(b),radius*math.sin(b),z),tube,material,'Heaven/Trim')

# Pipe descends inside the soil. Every horizontal pipe segment in the hall is submerged.
points=[(-300,125,130),(-300,125,220),(1120,125,220),(1120,125,22),
        (CENTER+80,125,22),(CENTER+80,0,22),(CENTER+80,0,174),(CENTER+38,0,174)]
for i,(p,q) in enumerate(zip(points,points[1:])):segment('Heaven continuous water pipe %02d'%i,p,q,10,mat('M_HeavenPipe'))
for i,p in enumerate(points[1:-1]):mesh('Heaven pipe elbow %02d'%i,'Sphere',p,(.26,.26,.26),mat('M_HeavenPipe'))
for x in range(-240,1110,150):segment('Heaven pipe coupling',(x-6,125,220),(x+6,125,220),15,mat('M_HeavenBrass'))
for x in range(1300,2600,190):segment('Heaven floor pipe coupling',(x-6,125,22),(x+6,125,22),14,mat('M_HeavenBrass'))

# Three 30cm steps, top at 90cm; original toilet base sits 42cm above the water.
for radius,height in [(245,30),(205,60),(165,90)]:
    mesh('Heaven dais step %03d'%height,'Cylinder',(CENTER,0,height*.5),(radius/50,radius/50,height/100),mat('M_HeavenPorcelain'),True,'Heaven/Room')
ring('Heaven central brass inlay',91,160,2.5,mat('M_HeavenBrass'))
ring('Heaven floor halo',31,240,3,mat('M_HeavenWarmLight'))
ring('Heaven suspended halo',440,130,4,mat('M_HeavenWarmLight'))

field=next(a for a in editor.get_all_level_actors() if isinstance(a,unreal.MiningCollectionField))
placement=unreal.MiningCollectionPlacement()
placement.set_editor_property('item_id',0)
placement.set_editor_property('area_tag','HeavenToilet')
placement.set_editor_property('transform',unreal.Transform(location=unreal.Vector(CENTER,0,144)))
field.set_editor_property('authored_placements',[placement])
assert field.get_editor_property('authored_placements')[0].get_editor_property('transform').translation==unreal.Vector(CENTER,0,144)

# Extra small underwater accents give visible refraction landmarks and reflected highlights.
for i in range(6):
    a=i*math.tau/6
    mesh('Heaven underwater light %02d'%i,'Sphere',(CENTER+1050*math.cos(a),1050*math.sin(a),20),(.32,.32,.12),mat('M_HeavenWarmLight'),False,'Heaven/Lighting')

water=next((a for a in editor.get_all_level_actors() if isinstance(a,unreal.HeavenToiletWater)),None)
if water is None:water=editor.spawn_actor_from_class(unreal.HeavenToiletWater,unreal.Vector(CENTER,0,0))
water.set_actor_label('Heaven water - 48cm basin and breach outflow');water.set_folder_path('Heaven/Water')
for key,value in [('radius',RADIUS),('water_height',HEIGHT),('water_material',water_mat),('spill_material',flow_mat),
                  ('retaining_soil',[a for a in editor.get_all_level_actors() if isinstance(a,unreal.DiggableRock)]),
                  ('inner_x',1400),('outer_x',420),('mouth_x',1260),('corridor_half_width',170)]:
    water.set_editor_property(key,value)
water.rebuild_water()
# Scope stable screen-space water reflections to this map. The project's
# unfiltered hardware Lumen reflections produce horizontal noise on shallow water.
pp=next(a for a in editor.get_all_level_actors() if isinstance(a,unreal.PostProcessVolume))
settings=pp.get_editor_property('settings')
for name,value in [('reflection_method',unreal.ReflectionMethod.SCREEN_SPACE),
                   ('screen_space_reflection_quality',100.0),('screen_space_reflection_intensity',100.0)]:
    settings.set_editor_property('override_'+name,True)
    settings.set_editor_property(name,value)
pp.set_editor_property('settings',settings)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ed.set_level_viewport_camera_info(unreal.Vector(1500,-420,220),unreal.Rotator(pitch=12,yaw=21,roll=0))
assert levels.save_current_level()
unreal.log('HEAVEN_WATER_UPGRADE_SAVED diameter=2640 height=1320 water=48 toilet=144')
