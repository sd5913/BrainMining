"""Scoped October package feedback: preserve map ownership and back up every changed asset."""
import json, math, shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/PackagePlaytest';OUT.mkdir(parents=True,exist_ok=True)
BACKUP=ROOT/'Saved/Backups'/('PackagePlaytest_'+datetime.now().strftime('%Y%m%d-%H%M%S'))
BACKUP.mkdir(parents=True)
assets=unreal.EditorAssetLibrary;ml=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib=unreal.MiningDesignerLibrary
report={'backup':str(BACKUP),'saved':[]}
def backup(path,ext='uasset'):
    rel=Path('Content')/(path.split('.')[0].removeprefix('/Game/')+'.'+ext)
    if (ROOT/rel).exists() and not (BACKUP/rel).exists():
        (BACKUP/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/rel,BACKUP/rel)
def save(obj):
    assert assets.save_loaded_asset(obj,False),obj.get_path_name()
    report['saved'].append(obj.get_path_name())
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def V(v):return unreal.Vector(*v)
def xyz(v):return list(v.to_tuple())
def material(path,code,params):
    backup(path)
    mat=unreal.load_asset(path) if assets.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
    ml.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    custom=ml.create_material_expression(mat,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    nodes={'T':(ml.create_material_expression(mat,unreal.MaterialExpressionTime),''),'UV':(ml.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate),'')}
    for key,value in params.items():
        vector=isinstance(value,tuple)
        n=ml.create_material_expression(mat,unreal.MaterialExpressionVectorParameter if vector else unreal.MaterialExpressionScalarParameter)
        n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',unreal.LinearColor(*value) if vector else value)
        nodes[key]=(n,'')
    inputs=[]
    for k in nodes:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',k);inputs.append(inp)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code',code)
    for key,(node,out) in nodes.items():assert ml.connect_material_expressions(node,out,custom,key)
    assert ml.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    ml.recompile_material(mat);save(mat);return mat

material('/Game/WenSi/Mirror/Materials/M_PoweredCable',
    'float pulse=pow(saturate(.5+.5*cos((UV.y*5-T*1.7)*6.283185)),12); return lerp(float3(.10,.018,.012),float3(.025,.55,.10)*(1.3+6*pulse),saturate(Powered));',{'Powered':0.})
material('/Game/Rooms/CardDuel/Materials/M_EscapeCRTScreen',
    'float scan=.88+.12*sin(UV.y*630+T*3); float glass=1-.25*length(UV-.5); return Color.rgb*(22*scan*glass);',{'Color':(.06,.38,.23,1.)})

# Runtime instancing must never depend on editor-only automatic usage recompilation.
report['ending_usage_fixed']=[]
for path in assets.list_assets('/Game/EndingEmergence/Materials',recursive=True):
    m=unreal.load_asset(path)
    if isinstance(m,unreal.Material) and m.material_domain==unreal.MaterialDomain.MD_SURFACE and not m.get_editor_property('used_with_instanced_static_meshes'):
        backup(path);m.set_editor_property('used_with_instanced_static_meshes',True)
        ml.recompile_material(m);save(m);report['ending_usage_fixed'].append(path)

assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
assert levels.set_current_level_by_name('Lvl_MainGame')
backup('/Game/MainGame/Maps/Lvl_MainGame','umap')
aa=list(api.get_all_level_actors())
hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall))
field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField))
minerals=next(a for a in aa if isinstance(a,unreal.MiningTreasureField))
tag='BM_PackagePlaytest20261019'
def mark(a,label,area):
    a.set_actor_label(label);a.set_folder_path('MainGame/PlaytestFixes')
    setp(a,'tags',list(a.tags)+[tag,area]);return a
def rock(label,center,size,mat,area):
    old=next((a for a in aa if a.get_actor_label()==label),None)
    a=old or api.spawn_actor_from_class(unreal.DiggableRock,center)
    a.set_actor_location(center,False,True);mark(a,label,area)
    setp(a,'rock_size',size);setp(a,'rock_material',mat);setp(a,'hardness_level',1);a.reset_rock()
    return a
red=next(a for a in aa if a.get_name()=='GeneratedMiningRock_33').rock_material

if not hall.actor_has_tag(tag):
    placements=list(minerals.get_editor_property('authored_placements'))
    # The first two copper deposits stay in their original seams, lowered together with their crystal clues.
    early=sorted([i for i,p in enumerate(placements) if str(p.area_tag)=='BM_Campaign_MainGame01' and str(p.treasure_id)=='2'],key=lambda i:placements[i].transform.translation.x)[:2]
    report['lowered_copper']=[]
    def move_deposit(p,delta):
        for name in ['transform','surface']:
            t=p.get_editor_property(name);t.translation=t.translation+delta;setp(p,name,t)
        fragments=list(p.fragments)
        for t in fragments:t.translation=t.translation+delta
        setp(p,'fragments',fragments);setp(p,'fragment_supports',[v+delta for v in p.fragment_supports])
    for i in early:
        move_deposit(placements[i],unreal.Vector(0,0,-180));report['lowered_copper'].append(i)

    # Place the flower near the floor of the branch beside the first mushroom climb.
    region=next(a for a in aa if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame02'))
    origin=region.get_actor_location()+region.cutters[0].local_transform.translation
    branches=json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelCavernV2/centerlines.json').read_text())
    branch=next(b for b in branches if b['name']=='Branch_02')
    p=branch['points'][-9];probe=origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
    h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),probe,probe-unreal.Vector(0,0,1800),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
    assert h[0] and isinstance(h[9],unreal.DiggableRock),'Flower requires a diggable floor'
    collection=list(field.get_editor_property('authored_placements'));flower=next(p for p in collection if p.item_id==9)
    t=flower.transform;t.translation=h[5]-unreal.Vector(0,0,45);setp(flower,'transform',t)
    setp(flower,'clue_transform',unreal.Transform(location=h[5]))
    setp(flower,'approach_transform',unreal.Transform(location=probe+unreal.Vector(0,0,100)))
    setp(flower,'host_rock',h[9]);setp(field,'authored_placements',collection)
    report['flower']={'branch':'Branch_02','position':xyz(t.translation),'floor':xyz(h[5]),'host':h[9].get_name()}

    # Fit rock directly outside the hemisphere rather than leaving a square void around it.
    dome=next(a for a in aa if a.get_actor_label().startswith('Heaven hemisphere'))
    center=dome.get_actor_location()
    mesh=unreal.DynamicMesh();identity=unreal.Transform()
    unreal.GeometryScript_Primitives.append_box(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=unreal.Vector(0,0,700)),dimension_x=2820,dimension_y=2820,dimension_z=1620,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    sphere=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_sphere_lat_long(sphere,unreal.GeometryScriptPrimitiveOptions(),identity,radius=1330,steps_phi=32,steps_theta=64)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,identity,sphere,identity,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True))
    mouth=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_box(mouth,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=unreal.Vector(0,-1370,225)),dimension_x=660,dimension_y=900,dimension_z=460,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,identity,mouth,identity,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True))
    shell=mark(api.spawn_actor_from_class(unreal.GeneratedMiningRock,center),'COL_Heaven_DiggableDomeShell','BM_Campaign_MainGame03')
    setp(shell,'rock_material',red);setp(shell,'hardness_level',1)
    assert lib.set_campaign_rock_source(shell,mesh)
    # Replace only collection-owned red blockers, retaining the blue sky and its trim.
    report['converted_rock']=[]
    for a in aa:
        if not a.actor_has_tag('BM_MainGame_Collections') or not isinstance(a,unreal.StaticMeshActor):continue
        materials=a.static_mesh_component.get_materials()
        is_red=any(m and m==red for m in materials) or a.get_actor_label().startswith('COL_Heaven_Branch')
        if not is_red:continue
        c,e=a.get_actor_bounds(False)
        rock(a.get_actor_label()+'_Diggable',c,e*2,red,'BM_Campaign_MainGame03')
        report['converted_rock'].append(a.get_actor_label());api.destroy_actor(a)
    # A ceiling seam above the shrine is moved to a genuine outer red-rock wall.
    for i,p in enumerate(placements):
        q=p.transform.translation-center
        if str(p.area_tag)=='BM_Campaign_MainGame03' and q.z>950 and math.hypot(q.x,q.y)<1900:
            target=center+unreal.Vector(1570,0,630);move_deposit(p,target-p.transform.translation)
            report['relocated_heaven_mineral']=i
    setp(minerals,'authored_placements',placements)
    # Overlap the stage and mine roof by 60 cm on each side of their join.
    rock('LINK_Stage_RedRoofSeal',unreal.Vector(32790,11750,20425),unreal.Vector(940,1200,220),red,'BM_Campaign_MainGame03')

    # Restore the authored office door as a visibly open leaf beside the entrance.
    door_mesh=unreal.load_asset('/Game/PopupClose/Maps/_GENERATED/Lenovo/office_doors_Submesh_704FAA0D')
    assert door_mesh
    door=mark(api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-16698,-95,6540),unreal.Rotator(yaw=85)),'Office_Entry_OpenDoor','BM_Campaign_MainGameOffice')
    component=door.static_mesh_component;component.set_static_mesh(door_mesh)
    b=door_mesh.get_bounding_box();extent=(b.max-b.min);scale=unreal.Vector(12/max(extent.x,1),180/max(extent.y,1),280/max(extent.z,1))
    component.set_world_scale3d(scale);component.set_collision_profile_name('BlockAll')
    bounds_center,_=door.get_actor_bounds(False);door.add_actor_world_offset(unreal.Vector(-16655,-115,6540)-bounds_center,False,True)
    setp(hall,'tags',list(hall.tags)+[tag])
    assert levels.save_current_level()

# Lower the seated opponent's resting body onto the existing chair, retaining its arrival motion.
cardmap='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2'
backup(cardmap,'umap');assert levels.load_level(cardmap)
stage=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.CardRoomStage))
if not stage.actor_has_tag(tag):
    pos=stage.opponent_base_local;pos.z-=80
    setp(stage,'opponent_base_local',pos);setp(stage,'tags',list(stage.tags)+[tag])
    assert levels.save_current_level()
report['passed']=True
(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PACKAGE_PLAYTEST_AUTHORING '+json.dumps(report,ensure_ascii=False))
unreal.SystemLibrary.quit_editor()
