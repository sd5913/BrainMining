import unreal

MAP='/Game/CollectionPrototype/Maps/Lvl_Collection_V02'
MAT='/Game/CollectionPrototype/Materials'
assets=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.MaterialEditingLibrary

# The mask dilates ONLY into pixels outside the selected object's visible silhouette.
# Interior pixels retain PostProcessInput0 exactly. Scene depth prevents x-ray outlines.
path=MAT+'/M_CollectionSilhouette'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    m=assets.create_asset('M_CollectionSilhouette',MAT,unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['Base','Depth','Mask']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code','''
#if SHADING_PATH_MOBILE
return Base.rgb;
#else
float2 uv = GetDefaultSceneTextureUV(Parameters, 13);
float2 texel = View.BufferSizeAndInvSize.zw;
float center = (Mask.r > 0.0 && Mask.r < 10000000.0 && Mask.r <= Depth.r + 1.0) ? 1.0 : 0.0;
float edge = 0.0;
[unroll] for (int y=-1; y<=1; ++y) {
    [unroll] for (int x=-1; x<=1; ++x) {
        float2 p = uv + float2(x,y)*texel*2.0;
        float cd = SceneTextureLookup(p,13,false).r;
        float sd = SceneTextureLookup(p,1,false).r;
        float visible = (cd > 0.0 && cd < 10000000.0 && cd <= sd + 1.0 && cd <= Depth.r + 2.0) ? 1.0 : 0.0;
        edge = max(edge, visible);
    }
}
return lerp(Base.rgb, float3(1.0,1.0,1.0), edge*(1.0-center));
#endif
''')
    for name,texture in [('Base',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),('Depth',unreal.SceneTextureId.PPI_SCENE_DEPTH),('Mask',unreal.SceneTextureId.PPI_CUSTOM_DEPTH)]:
        node=lib.create_material_expression(m,unreal.MaterialExpressionSceneTexture);node.set_editor_property('scene_texture_id',texture)
        assert lib.connect_material_expressions(node,'Color',custom,name)
    assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    unreal.log('V02 map already exists; preserved existing map.')
else:
    assert levels.new_level(MAP)
    floor=unreal.load_asset(MAT+'/M_CollectionFloor');stone=unreal.load_asset(MAT+'/M_CollectionBedrock');gold=unreal.load_asset(MAT+'/M_CollectionAccent')
    def cube(label,p,size,material):
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.set_actor_label(label)
        a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,material)
        a.set_actor_scale3d(unreal.Vector(*(n/100 for n in size)));return a
    def sign(text,p,scale=34):
        a=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*p),unreal.Rotator(pitch=0,yaw=180,roll=0))
        a.set_actor_label('V02 '+text);c=a.get_component_by_class(unreal.TextRenderComponent);c.set_text(text);c.set_world_size(scale)
        c.set_text_render_color(unreal.Color(204,186,134,255));c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    cube('V02 safety floor',(950,0,-40),(4500,3100,80),floor)
    cube('V02 rear bedrock',(3160,0,580),(120,3100,1240),stone)
    cube('V02 ceiling',(1550,0,1160),(3320,3100,100),stone)
    for side in [-1,1]: cube('V02 side bedrock',(1000,side*1560,580),(4400,120,1240),stone)
    cube('V02 entrance boundary',(-1290,0,580),(100,3100,1240),stone)
    for y in [-1200,-600,0,600,1200]:
        cube('V02 dividing bedrock',(1760,y,580),(2700,55,1160),stone)
        cube('V02 mouth pillar',(425,y,580),(95,95,1160),stone)
        cube('V02 brass collar',(373,y,700),(12,105,60),gold)
    for i,y in enumerate([-900,-300,300,900]):
        cube('V02 threshold',(370,y,2),(12,520,4),gold)
        sign('0'+str(i+1),(414,y,1030),56)
        lamp=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(60,y,650))
        lamp.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);lamp.light_component.set_intensity(450)
        lamp.light_component.set_editor_property('attenuation_radius',1400);lamp.light_component.set_light_color(unreal.LinearColor(1,.83,.63,1))
    cube('V02 tutorial plinth',(-300,-330,10),(170,170,20),stone)
    sign('E  /  COLLECT',(-218,-330,175),19)
    actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-760,0,100),unreal.Rotator())
    field=actors.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector());field.set_actor_label('V02 collection rules')
    field.set_editor_property('deep_field',True);field.set_editor_property('excavation_depth',1100)
    pp=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    for name,value in [('auto_exposure_min_brightness',1.5),('auto_exposure_max_brightness',1.5),('auto_exposure_bias',0)]:
        settings.set_editor_property('override_'+name,True);settings.set_editor_property(name,value)
    pp.set_editor_property('settings',settings)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningCollectionGameMode)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-1150,-900,700),unreal.Rotator(pitch=-12,yaw=26,roll=0))
    assert levels.save_current_level()
    unreal.log('COLLECTION_V02_CREATED '+MAP)
