"""Author a compact street into the protected ending; never rebuild the director.

Run in a clean, exclusive UE editor process. Existing cell actors, stage anchors,
crowd paths and film cameras remain intact. Backups are kept under Saved/Backups.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
MAP = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
REPORT = ROOT / 'Saved/Validation/EndingStreet'
REPORT.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Use a clean editor.'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Use a clean editor.'
assert levels.load_level(MAP)
all_actors = actors.get_all_level_actors()
director = next(a for a in all_actors if isinstance(a, unreal.EndingEmergence))
assert director.get_editor_property('editable_scenery'), 'This edit requires the authored ending.'
if director.actor_has_tag('EndingStreetDressingV1'):
    raise RuntimeError('Street revision already applied; edit the saved actors directly.')
city = [a for a in all_actors if a.actor_has_tag('EndingEditableCity')]
anchor = next(c for c in director.get_components_by_class(unreal.SceneComponent)
              if c.get_name().startswith('02_CityAndPeople'))
cell_before = {a.get_name(): str(a.get_actor_transform()) for a in all_actors
               if a.actor_has_tag('EndingEditableCell')}
backup = ROOT / 'Saved/Backups' / ('EndingStreet-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.umap')
backup.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT / 'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap', backup)

J = '/Game/JapaneseCity/Meshes/'
Q = '/Game/QuickCity/Brutalist/'
cache = {}
geometry_bounds = {}
def mesh(path):
    if path not in cache:
        cache[path] = unreal.load_asset(path)
        assert isinstance(cache[path], unreal.StaticMesh), path
    return cache[path]

# Load every required asset before touching the original street.
paths = [J+'Buildings/'+n for n in ['SM_jctBld12_A_low','SM_jctBld12_C_low',
    'SM_jctBld12_6A_low','SM_jctBld24_12A_low','SM_jctBld24_A_btm_low',
    'SM_jctBld24_A_mdl_low','SM_jctBld24_A_top']]
paths += [J+'Props/'+n for n in ['SM_jctStreetLightA','SM_jctTrafficLightA',
    'SM_jctBenchA','SM_jctGuardRailA','SM_jctUtilityholeC','SM_jctGratingA',
    'SM_jctStreetSignB','SM_jctRoadConeA','SM_jctRoofEquipFanA']]
paths += [J+'Foliage/SM_jctTreeA',J+'Grounds/SM_jcGrdCrosswalkA',
          J+'Grounds/SM_jcGrdStopLineA',J+'Grounds/SM_jcGrdDashedLineA',
          Q+'Meshes/SM_QC_Road',Q+'Meshes/SM_QC_Pavement',
          Q+'Meshes/SM_QC_CivicCanopy',Q+'Meshes/SM_QC_RoofEquipment']
for path in paths: mesh(path)
road_mat = unreal.load_asset(Q+'Materials/M_QC_Road')
paving_mat = unreal.load_asset(Q+'Materials/M_QC_Paving')
concrete_mat = unreal.load_asset(Q+'Materials/M_QC_Concrete')
steel_mat = unreal.load_asset(Q+'Materials/M_QC_Steel')
assert road_mat and paving_mat and concrete_mat and steel_mat
for a in city:
    assert actors.destroy_actor(a)

created = []
crossings = []
buildings = []
def place(label, path, center, yaw=0, scale=1, size=None, category='Props', material=None):
    m = mesh(path)
    b = m.get_bounds()
    extent, origin = b.box_extent, b.origin
    if '/QuickCity/' in path:
        if path not in geometry_bounds:
            geo=unreal.DynamicMesh()
            unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(m,geo,
                unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=True),
                unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
            box=unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(geo)
            geometry_bounds[path]=((box.max-box.min)*.5,(box.max+box.min)*.5)
        extent,origin=geometry_bounds[path]
    if size:
        s = unreal.Vector(size[0]/max(2*extent.x,.01),size[1]/max(2*extent.y,.01),
                          size[2]/(2*extent.z) if extent.z>.01 else 1)
    else:
        s = unreal.Vector(scale,scale,scale)
    rotation = unreal.Rotator(pitch=0,yaw=yaw,roll=0)
    # Center XY at the requested point; requested Z is the bottom of the mesh.
    offset = unreal.MathLibrary.rotate_angle_axis(
        unreal.Vector(origin.x*s.x,origin.y*s.y,(origin.z-extent.z)*s.z),
        yaw,unreal.Vector(0,0,1))
    local = unreal.Vector(*center)-offset
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(m)
    c.set_editor_property('override_materials', [])
    if material:
        for slot in range(c.get_num_materials()): c.set_material(slot,material)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_editor_property('generate_overlap_events',False)
    c.set_cast_shadow(category=='Buildings')
    a.attach_to_component(anchor,unreal.Name('None'),unreal.AttachmentRule.KEEP_RELATIVE,
                          unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False)
    c.set_relative_location(local,False,False)
    c.set_relative_rotation(rotation,False,False)
    c.set_relative_scale3d(s)
    a.set_actor_label('EDIT_街景_'+label)
    a.set_folder_path('Ending_Edit/02_City/'+category)
    a.set_editor_property('tags',[unreal.Name(t) for t in
        ['EndingEditableScenery','EndingEditableCity','EndingStreetDressingV1']])
    c.set_visibility(True,True)
    created.append(a)
    return a

# 120m square of scenery, with just two 11.2m roads and four compact blocks.
road = Q+'Meshes/SM_QC_Road'
pavement = Q+'Meshes/SM_QC_Pavement'
place('CityGround_Asphalt',road,(0,0,-24),size=(12000,12000,20),category='Road',material=road_mat)
for side in (-1,1):
    for other in (-1,1):
        place('Sidewalk_%s_%s'%(side,other),pavement,(side*3300,other*3300,-14),
              size=(5400,5400,28),category='Road',material=paving_mat)
        # Short curb runs stop at each zebra crossing, leaving the corner readable.
        for axis in (0,1):
            p=(side*3500,other*600,0) if axis==0 else (side*600,other*3500,0)
            place('Curb_%s_%s_%s'%(side,other,axis),pavement,p,yaw=axis*90,
                  size=(5000,22,16),category='Road',material=concrete_mat)

# Four complete crossings centered on the arms of the same measured intersection.
for axis in (0,1):
    for side in (-1,1):
        center=(side*850,0,1) if axis==0 else (0,side*850,1)
        a=place('Crosswalk_%s_%s'%(axis,side),J+'Grounds/SM_jcGrdCrosswalkA',center,
                yaw=axis*90,size=(300,1120,0),category='Road')
        a.set_editor_property('tags',list(a.tags)+[unreal.Name('EndingAlignedCrosswalk')])
        crossings.append({'axis':axis,'side':side,'center':center,'road_width':1120})
        # White stop line sits behind each crossing rather than through its stripes.
        stop=(side*1120,0,1.2) if axis==0 else (0,side*1120,1.2)
        place('StopLine_%s_%s'%(axis,side),J+'Grounds/SM_jcGrdStopLineA',stop,
              yaw=axis*90,size=(24,470,0),category='Road')
        for step in range(5):
            p=(side*(1550+step*850),0,1.1) if axis==0 else (0,side*(1550+step*850),1.1)
            place('LaneDash_%s_%s_%s'%(axis,side,step),J+'Grounds/SM_jcGrdDashedLineA',
                  p,yaw=axis*90,size=(340,10,0),category='Road')

# Familiar flat-roofed apartment/office buildings; native pack proportions remain.
for sx in (-1,1):
    for sy in (-1,1):
        angle=90 if sx==sy else 0
        for number,(name,x,y,s) in enumerate([
            ('SM_jctBld12_A_low',1600,1600,1),
            ('SM_jctBld12_C_low',3300,1550,.95),
            ('SM_jctBld12_6A_low',1500,3450,1.05),
            ('SM_jctBld24_12A_low',4500,1700,.95),
        ]):
            a=place('CityAssetBuildings_Block_%s_%s_%s'%(sx,sy,number),
                    J+'Buildings/'+name,(sx*x,sy*y,14),yaw=90 if number==3 else angle,scale=s,category='Buildings')
            buildings.append(a.get_actor_label())
        # Pack A is a complete modular high-rise when its authored floor origins align.
        for part in ('btm_low','mdl_low','top'):
            place('CityAssetBuildings_Tower_%s_%s_%s'%(sx,sy,part),
                  J+'Buildings/SM_jctBld24_A_'+part,(sx*3450,sy*3850,14+
                    .94*{'btm_low':0,'mdl_low':600,'top':3600}[part]),
                  yaw=angle,scale=.94,category='Buildings')
        buildings.append('Tower_%s_%s'%(sx,sy))

# Small, grounded street details; all stay outside the crowd's +/-440cm lanes.
for side in (-1,1):
    for axis in (0,1):
        def street(along,across,z=14):
            return (along,side*across,z) if axis==0 else (side*across,along,z)
        for along in (-4300,-2300,2300,4300):
            place('CityDressProp_StreetLight_%s_%s_%s'%(side,axis,along),
                  J+'Props/SM_jctStreetLightA',street(along,720),
                  yaw=(0 if side<0 else 180)+axis*90,scale=.72)
        for along in (-2900,2900):
            place('CityDressProp_Tree_%s_%s_%s'%(side,axis,along),
                  J+'Foliage/SM_jctTreeA',street(along,880),scale=.8)
            place('CityDressProp_Bench_%s_%s_%s'%(side,axis,along),
                  J+'Props/SM_jctBenchA',street(along+330,800),yaw=axis*90,scale=1.05)
        for along in (-1550,1550):
            place('CityDressProp_GuardRail_%s_%s_%s'%(side,axis,along),
                  J+'Props/SM_jctGuardRailA',street(along,640),yaw=axis*90,scale=1)
        place('CityDressProp_Signal_%s_%s'%(side,axis),J+'Props/SM_jctTrafficLightA',
              street(side*1120,700),yaw=(180 if side>0 else 0)+axis*90,scale=.8)
        place('CityDressProp_Sign_%s_%s'%(side,axis),J+'Props/SM_jctStreetSignB',
              street(-side*1250,830),yaw=axis*90,scale=.8)
        for along in (-3500,3500):
            place('CityDressProp_Drain_%s_%s_%s'%(side,axis,along),
                  J+'Props/SM_jctGratingA',street(along,545,0),yaw=axis*90,scale=.35)
        place('CityDressProp_Manhole_%s_%s'%(side,axis),J+'Props/SM_jctUtilityholeC',
              street(1900,250,0),scale=1.25)

# Two restrained bus shelters use Quick City's concrete/steel canopy module.
for side in (-1,1):
    place('CityDressProp_QuickCityShelter_%s'%side,Q+'Meshes/SM_QC_CivicCanopy',
          (side*4000,side*870,274),size=(420,240,14),category='Props',material=concrete_mat)
    for x in (-180,180):
        for y in (-90,90):
            place('CityDressProp_ShelterPost_%s_%s_%s'%(side,x,y),Q+'Meshes/SM_QC_CivicCanopy',
                  (side*4000+x,side*870+y,14),size=(9,9,260),material=steel_mat)
    place('CityDressProp_ShelterSeat_%s'%side,J+'Props/SM_jctBenchA',
          (side*4000,side*920,14),scale=1.15)
    place('CityDressProp_RoofEquipment_%s'%side,Q+'Meshes/SM_QC_RoofEquipment',
          (side*1600,side*1600,2990),size=(170,120,95),category='Props')
    for number in range(3):
        place('CityDressProp_Cones_%s_%s'%(side,number),J+'Props/SM_jctRoadConeA',
              (side*(4700+number*90),side*690,14),scale=.75)

director.set_editor_property('tags',list(director.tags)+[unreal.Name('EndingStreetDressingV1')])
for camera in all_actors:
    if camera.actor_has_tag('EndingSceneryOverviewCity'):
        camera.set_actor_location(unreal.Vector(17200,-6600,7200),False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
            camera.get_actor_location(),unreal.Vector(22000,0,500)),False)
        camera.camera_component.set_editor_property('field_of_view',70)
assert all(a.get_attach_parent_actor()==director for a in created)
assert cell_before=={a.get_name():str(a.get_actor_transform()) for a in actors.get_all_level_actors()
                     if a.actor_has_tag('EndingEditableCell')}
assert levels.save_current_level()
# Validate the serialized level, not only the live editor state.
assert levels.load_level('/Engine/Maps/Entry')
assert levels.load_level(MAP)
saved=[a for a in actors.get_all_level_actors() if a.actor_has_tag('EndingStreetDressingV1')
       and isinstance(a,unreal.StaticMeshActor)]
assert len(saved)==len(created)
assert sum(a.actor_has_tag('EndingAlignedCrosswalk') for a in saved)==4
assert all(a.get_attach_parent_actor() and isinstance(a.get_attach_parent_actor(),unreal.EndingEmergence) for a in saved)
assert all('/EndingEmergence/City/' not in a.static_mesh_component.static_mesh.get_path_name() for a in saved)
result={'map':MAP,'backup':str(backup),'old_city_actors':len(city),'new_city_actors':len(saved),
        'building_count':len(buildings),'road_width_cm':1120,'street_extent_cm':12000,
        'crossings':crossings,'cell_unchanged':True,'save_reload':True,'attachments_preserved':True}
(REPORT/'authoring.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('ENDING_STREET_DRESSING_SAVED '+json.dumps(result))
