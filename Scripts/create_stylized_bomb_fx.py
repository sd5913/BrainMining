"""Create small faceted fire/smoke assets; no photoreal explosion flipbook."""
import math
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
source=root/'SourceAssets/BombFX';source.mkdir(parents=True,exist_ok=True)
# Subdivided icosahedron: 80 triangular facets with flat normals.
phi=(1+math.sqrt(5))/2
vertices=[(-1,phi,0),(1,phi,0),(-1,-phi,0),(1,-phi,0),(0,-1,phi),(0,1,phi),(0,-1,-phi),(0,1,-phi),(phi,0,-1),(phi,0,1),(-phi,0,-1),(-phi,0,1)]
def normal(p):
    length=math.sqrt(sum(v*v for v in p));return tuple(v/length for v in p)
vertices=[normal(p) for p in vertices]
faces=[(0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),(1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),(3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),(4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1)]
mid={}
def midpoint(a,b):
    key=tuple(sorted((a,b)))
    if key not in mid:
        mid[key]=len(vertices);vertices.append(normal(tuple((vertices[a][i]+vertices[b][i])/2 for i in range(3))))
    return mid[key]
expanded=[]
for a,b,c in faces:
    ab,bc,ca=midpoint(a,b),midpoint(b,c),midpoint(c,a)
    expanded.extend([(a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)])
obj=['o BombPuff','s off']+['v '+' '.join(str(v*50) for v in p) for p in vertices]+['f '+' '.join(str(v+1) for v in f) for f in expanded]
path=source/'BombPuff.obj';path.write_text('\n'.join(obj)+'\n',encoding='ascii')
asset_path='/Game/Bomb/FX/SM_BombPuff'
assets=unreal.AssetToolsHelpers.get_asset_tools();editing=unreal.MaterialEditingLibrary
if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path='/Game/Bomb/FX';task.destination_name='SM_BombPuff';task.automated=True;task.save=True
    task.options=unreal.FbxImportUI();task.options.import_materials=False;task.options.import_textures=False;task.options.import_as_skeletal=False
    task.options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    task.options.static_mesh_import_data.import_uniform_scale=1.
    assets.import_asset_tasks([task])
    assert unreal.load_asset(asset_path),task.imported_object_paths
mesh=unreal.load_asset(asset_path)
for name,fire in [('M_BombFire',True),('M_BombSmoke',False)]:
    material=unreal.load_asset('/Game/Bomb/FX/'+name) or assets.create_asset(name,'/Game/Bomb/FX',unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('used_with_instanced_static_meshes',True)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT if fire else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    if not fire: material.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    editing.delete_all_material_expressions(material)
    def node(cls,**props):
        n=editing.create_material_expression(material,cls,0,0)
        for key,value in props.items():n.set_editor_property(key,value)
        return n
    def wire(a,b,socket,out=''):assert editing.connect_material_expressions(a,out,b,socket)
    def output(a,prop,out=''):assert editing.connect_material_property(a,out,prop)
    alpha=node(unreal.MaterialExpressionPerInstanceCustomData,data_index=0)
    interpolated=node(unreal.MaterialExpressionVertexInterpolator);wire(alpha,interpolated,'VS')
    depth=node(unreal.MaterialExpressionDepthFade,fade_distance_default=12.);wire(interpolated,depth,'Opacity');output(depth,unreal.MaterialProperty.MP_OPACITY)
    if fire:
        heat=node(unreal.MaterialExpressionPerInstanceCustomData,data_index=1)
        heat_pixel=node(unreal.MaterialExpressionVertexInterpolator);wire(heat,heat_pixel,'VS')
        orange=node(unreal.MaterialExpressionVectorParameter,parameter_name='FireOrange',default_value=unreal.LinearColor(1,.12,.018,1))
        yellow=node(unreal.MaterialExpressionVectorParameter,parameter_name='FireYellow',default_value=unreal.LinearColor(1,.65,.075,1))
        color=node(unreal.MaterialExpressionLinearInterpolate);wire(orange,color,'A','RGB');wire(yellow,color,'B','RGB');wire(heat_pixel,color,'Alpha')
        strength=node(unreal.MaterialExpressionScalarParameter,parameter_name='Brightness',default_value=2.5)
        energy=node(unreal.MaterialExpressionMultiply);wire(color,energy,'A');wire(strength,energy,'B');output(energy,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        color=node(unreal.MaterialExpressionVectorParameter,parameter_name='SmokeColor',default_value=unreal.LinearColor(.075,.065,.05,1))
        output(color,unreal.MaterialProperty.MP_BASE_COLOR,'RGB')
        ambient=node(unreal.MaterialExpressionMultiply,const_b=.45);wire(color,ambient,'A','RGB');output(ambient,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        rough=node(unreal.MaterialExpressionConstant,r=1.);output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    editing.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
# Explicitly clear a serialized old realistic system only on our two player
# blueprints. Preserve all model positions, artist materials and other defaults.
for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter']:
    bp=unreal.load_asset(path)
    if not bp:continue
    cdo=unreal.get_default_object(bp.generated_class());tool=cdo.get_component_by_class(unreal.MiningBombToolComponent)
    if tool:tool.set_editor_property('explosion_asset',None)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('STYLIZED_BOMB_ASSETS_READY: 80 facet mesh, compact fire and smoke, realistic system detached')
