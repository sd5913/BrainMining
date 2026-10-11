"""V11: overhead lamp and a bounded warm shaft tied to the lamp mouth.
Apply after V10; rerunning changes only lamp transforms and this shaft.
"""
import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
actors=list(A.get_all_level_actors());entrance=next(a for a in actors if isinstance(a,unreal.MiningGiantRoomEntrance))
assert unreal.Name('StudyV10LayoutApplied') in entrance.tags, 'Apply V10 first.'
TAG=unreal.Name('StudyLampBeamV11')
for a in actors:
    if TAG in a.tags:A.destroy_actor(a)
by={a.get_actor_label():a for a in A.get_all_level_actors()}
def lamp(name):return by['Study V10 / Lamp '+name]
def place(a,p,rot=None):
    a.set_actor_location(unreal.Vector(*p),False,False)
    if rot is not None:a.set_actor_rotation(rot,False)
opening=unreal.Vector(12450,3600,4400);target=unreal.Vector(12450,3600,3004);source=opening-unreal.Vector(0,0,28)
neck=opening+unreal.Vector(0,0,400);elbow=unreal.Vector(13100,4660,4770)
# Side base and hinged arms stay supported; the shade and light axis are exactly vertical.
arm=lamp('upper arm');d=neck-elbow;place(arm,((neck.x+elbow.x)/2,(neck.y+elbow.y)/2,(neck.z+elbow.z)/2),unreal.MathLibrary.make_rot_from_z(d));arm.set_actor_scale3d(unreal.Vector(.4,.4,d.length()/100))
for a in actors:
    if a.get_actor_label()=='Study V10 / Lamp joint':
        p=a.get_actor_location()
        if (p-unreal.Vector(12900,4900,3100)).length()>10 and (p-elbow).length()>10:place(a,(neck.x,neck.y,neck.z))
for name,z in [('shade',4600),('rim',4403),('diffuser',4390)]:place(lamp(name),(12450,3600,z),unreal.Rotator())
light=lamp('stage source');place(light,(source.x,source.y,source.z),unreal.Rotator(pitch=-90))
c=light.light_component;c.set_editor_property('inner_cone_angle',29.0);c.set_editor_property('outer_cone_angle',31.0);c.set_editor_property('source_radius',12.0);c.set_intensity(500000)
place(lamp('bulb bounce'),(12450,3600,4360))
# A cone with its apex hidden inside the fixture gives a 340cm mouth radius and
# 800cm floor radius. Shader clips the upper cone precisely below the diffuser.
base='/Game/Rooms/CardDuel/Materials/';name='M_StudyLampShaftV11';path=base+name
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,base.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
lib=unreal.MaterialEditingLibrary;lib.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
def expr(cls):return lib.create_material_expression(m,cls)
color=expr(unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(180,124,58,1));lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mask=expr(unreal.MaterialExpressionCustom);ins=[]
for n in ['P','N','V','Eye']:
    i=unreal.CustomInput();i.set_editor_property('input_name',n);ins.append(i)
mask.set_editor_property('inputs',ins);mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
mask.set_editor_property('code','''float edge=pow(1-abs(dot(normalize(N),normalize(V))),3);
float clipTop=1-smoothstep(4366,4372,P.z);
float t=saturate((4372-Eye.z)/1366);
float radius=lerp(340,800,t);
float inside=(length(Eye.xy-float2(12450,3600))<radius && Eye.z>3004 && Eye.z<4372)?0.18:1.0;
return clipTop*(0.055+0.16*edge)*inside;''')
for n,cls in [('P',unreal.MaterialExpressionWorldPosition),('N',unreal.MaterialExpressionPixelNormalWS),('V',unreal.MaterialExpressionCameraVectorWS),('Eye',unreal.MaterialExpressionCameraPositionWS)]:lib.connect_material_expressions(expr(cls),'',mask,n)
lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
height=1366/(1-340/800);bottom=3006
beam=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(12450,3600,bottom+height/2),unreal.Rotator())
beam.set_actor_label('Study V11 / Lamp visible shaft');beam.tags=[TAG];beam.set_folder_path('GiantRoom/StudyLampBeamV11')
comp=beam.static_mesh_component;comp.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cone'));comp.set_material(0,m);comp.set_cast_shadow(False)
beam.set_actor_scale3d(unreal.Vector(16,16,height/100));beam.set_actor_enable_collision(False)
assert abs(light.get_actor_rotation().pitch+90)<.01
assert abs(lamp('shade').get_actor_location().x-target.x)<.01 and abs(lamp('shade').get_actor_location().y-target.y)<.01
marker=unreal.Name('StudyV11LayoutApplied')
if marker not in entrance.tags:entrance.tags=list(entrance.tags)+[marker]
assert L.save_current_level()
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV11';out.mkdir(parents=True,exist_ok=True)
(out/'layout.json').write_text(json.dumps({'version':'V11','opening':[12450,3600,4400],'source':[12450,3600,4372],'target':[12450,3600,3004],'light_pitch':-90,'inner_outer_angles':[29,31],'shaft_radius_top_bottom':[340,800],'shaft_camera_inside_factor':.18,'stage_and_floor_props_unchanged':True},indent=2),encoding='utf-8')
unreal.log('STUDY_LAMP_SHAFT_V11_READY')
