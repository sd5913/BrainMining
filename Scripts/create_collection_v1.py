"""Create only the isolated V0.1 collection map and its private materials."""
import unreal

ROOT='/Game/CollectionPrototype'
MAP=ROOT+'/Maps/Lvl_Collection_V01'
assets=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Collection V0.1 already exists; refusing to overwrite a played/edited map.')

def expression(mat,cls): return lib.create_material_expression(mat,cls)
def constant(mat,value):
    c=expression(mat,unreal.MaterialExpressionConstant); c.set_editor_property('r',value); return c
def material(name,color):
    path=ROOT+'/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    m=assets.create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    c=expression(m,unreal.MaterialExpressionVectorParameter); c.set_editor_property('parameter_name','Tint'); c.set_editor_property('default_value',unreal.LinearColor(*color,1))
    lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(constant(m,.88),'',unreal.MaterialProperty.MP_ROUGHNESS)
    multiply=expression(m,unreal.MaterialExpressionMultiply)
    lib.connect_material_expressions(c,'',multiply,'A'); lib.connect_material_expressions(constant(m,.12),'',multiply,'B')
    lib.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m

unreal.EditorAssetLibrary.make_directory(ROOT+'/Materials')
unreal.EditorAssetLibrary.make_directory(ROOT+'/Maps')
tint=material('M_CollectionTint',(.7,.75,.65))
earth=material('M_CollectionEarth',(.27,.145,.06))
floor=material('M_CollectionFloor',(.055,.075,.071))
bedrock=material('M_CollectionBedrock',(.12,.17,.16))
accent=material('M_CollectionAccent',(.66,.38,.08))
outline=assets.create_asset('M_CollectionOutline',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
outline.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
outline.set_editor_property('two_sided',True)
outline.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
lib.connect_material_property(constant(outline,3),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
sign=expression(outline,unreal.MaterialExpressionTwoSidedSign)
mask=expression(outline,unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(sign,'',mask,'A'); lib.connect_material_expressions(constant(outline,-1),'',mask,'B')
lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY_MASK)
normal=expression(outline,unreal.MaterialExpressionVertexNormalWS)
offset=expression(outline,unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(normal,'',offset,'A'); lib.connect_material_expressions(constant(outline,2.2),'',offset,'B')
lib.connect_material_property(offset,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
lib.recompile_material(outline); unreal.EditorAssetLibrary.save_loaded_asset(outline)

if not levels.new_level(MAP): raise RuntimeError('Could not create standalone level')

def cube(label,p,size,mat,rotation=0):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),unreal.Rotator(0,rotation,0))
    a.set_actor_label(label); a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,mat); a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    return a
def sign(text,p,size=27):
    a=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*p),unreal.Rotator(0,180,0)); a.set_actor_label('Collection sign '+text)
    c=a.get_component_by_class(unreal.TextRenderComponent); c.set_text(text); c.set_world_size(size)
    c.set_text_render_color(unreal.Color(224,197,134,255)); c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)

cube('Collection safety floor',(200,0,-40),(2500,2600,80),floor)
cube('Back wall',(1050,0,530),(180,2600,1140),bedrock)
for s in [-1,1]:
    cube('Side boundary',(200,s*1325,480),(2500,90,1040),bedrock)
    cube('Entry boundary',(-1080,s*720,230),(90,1150,540),bedrock)
    cube('Entry safety rail',(-1080,s*180,70),(90,260,140),accent)
cube('Entry wall',(-1080,0,30),(90,280,60),bedrock)
for y in [-1040,-520,0,520,1040]:
    cube('Faceted stone pillar',(930,y,520),(220,90,1080),bedrock,12 if y%3 else -12)
for bay in range(4):
    y=-780+bay*520
    cube('Bay stripe',(335,y,2),(10,390,4),accent)
    sign('0'+str(bay+1),(422,y,860),44)
    cube('Bay upper beam',(920,y,870),(90,450,50),accent)
for x in [-850,-500,-150,200]:
    cube('Path guide',(x,0,1),(4,2400,2),bedrock)
sign('THE UNDERGROUND ARCHIVE',(-1010,0,480),44)
sign('EXCAVATE / DISCOVER / COLLECT',(-1008,0,405),22)
cube('Tutorial plinth',(-300,-330,10),(170,170,20),bedrock)
sign('START HERE  /  E',(-218,-330,175),19)

actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-720,0,100),unreal.Rotator(0,0,0)).set_actor_label('Collection player start')
field=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/BrainMining.MiningCollectionField'),unreal.Vector())
field.set_actor_label('Collection rules - seed and exposure')
field.set_editor_property('exposure_threshold',.65)
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500),unreal.Rotator(-58,-135,0))
sun.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE); sun.light_component.set_intensity(3)
sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1300))
sky.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE); sky.light_component.set_editor_property('real_time_capture',True); sky.light_component.set_intensity(.65)
actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
for y in [-780,-260,260,780]:
    lamp=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(180,y,450))
    lamp.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    lamp.light_component.set_intensity(2500); lamp.light_component.set_editor_property('attenuation_radius',1050)
    lamp.light_component.set_light_color(unreal.LinearColor(1,.77,.49,1))
pp=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
pp.set_editor_property('unbound',True); settings=pp.get_editor_property('settings')
for key,value in [('auto_exposure_min_brightness',1),('auto_exposure_max_brightness',1),('auto_exposure_bias',0)]:
    settings.set_editor_property('override_'+key,True); settings.set_editor_property(key,value)
pp.set_editor_property('settings',settings)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/BrainMining.MiningCollectionGameMode'))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-1350,-1250,1250),unreal.Rotator(-25,40,0))
assert levels.save_current_level()
unreal.log('COLLECTION_V01_CREATED '+MAP)
