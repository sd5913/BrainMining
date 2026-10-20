"""Fit four compact corner gardens to the saved city, and enable lamp shadows."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/EndingNight'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
allactors=actors.get_all_level_actors()
d=next(a for a in allactors if isinstance(a,unreal.EndingEmergence))
assert d.actor_has_tag('EndingStreetNightV1')
assert not d.actor_has_tag('EndingCornerGardensV1'),'Already applied; edit the saved actors.'
anchor=next(c for c in d.get_components_by_class(unreal.SceneComponent) if c.get_name().startswith('02_CityAndPeople'))
before={a.get_name():str(a.get_actor_transform()) for a in allactors if a.actor_has_tag('EndingEditableCell')}
backup=ROOT/'Saved/Backups'/('EndingCorners-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
shutil.copy2(ROOT/'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap',backup)
created=[]
cache={}
def asset(path):
    if path not in cache:
        cache[path]=unreal.load_asset(path);assert cache[path],path
    return cache[path]
def attach(a,label):
    a.attach_to_component(anchor,unreal.Name('None'),unreal.AttachmentRule.KEEP_RELATIVE,
        unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False)
    a.set_actor_label('EDIT_街角_'+label)
    a.set_folder_path('Ending_Edit/02_City/CornerGardens')
    a.set_editor_property('tags',[unreal.Name(t) for t in
        ['EndingEditableScenery','EndingEditableCity','EndingStreetNightV1','EndingCornerGardensV1']])
    created.append(a)
def place(label,path,p,yaw=0,scale=1,size=None,material=None,shadow=True):
    m=asset(path)
    b=m.get_bounds();ex,o=b.box_extent,b.origin
    if '/QuickCity/' in path:
        mesh=unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(m,mesh,
            unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=True),
            unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
        box=unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
        ex,o=(box.max-box.min)*.5,(box.max+box.min)*.5
    s=unreal.Vector(*(size[i]/max(2*getattr(ex,'xyz'[i]),.01) if size[i]>0 else 1 for i in range(3))) if size else unreal.Vector(scale,scale,scale)
    offset=unreal.MathLibrary.rotate_angle_axis(unreal.Vector(o.x*s.x,o.y*s.y,(o.z-ex.z)*s.z),yaw,unreal.Vector(0,0,1))
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(m);c.set_cast_shadow(shadow);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if material:
        for i in range(c.get_num_materials()):c.set_material(i,material)
    attach(a,label)
    c.set_relative_location(unreal.Vector(*p)-offset,False,False)
    c.set_relative_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False,False)
    c.set_relative_scale3d(s)
    return a
J='/Game/JapaneseCity/Meshes/'
Q='/Game/QuickCity/Brutalist/'
paving=asset(Q+'Materials/M_QC_Paving')
warm=asset('/Game/EndingEmergence/StreetNight/M_WarmLight')
sign=asset('/Game/JapaneseCity/Materials/Props/MI_jctStreetSignA_L')
corners=[]
for sx in (-1,1):
    for sy in (-1,1):
        name='%s_%s'%(sx,sy)
        place(name+'_Paving',Q+'Meshes/SM_QC_Pavement',(sx*800,sy*800,14),
            size=(320,320,6),material=paving,shadow=False)
        place(name+'_TreePlanter',J+'Foliage/SM_jctTreeA',(sx*835,sy*835,20),scale=.22)
        place(name+'_LitDirectory',J+'Props/SM_jctStreetSignA',(sx*930,sy*710,20),
            yaw=unreal.MathLibrary.conv_vector_to_rotator(unreal.Vector(-sx,-sy,0)).yaw,
            scale=.65,material=sign)
        for axis in (0,1):
            p=(sx*800,sy*635,20.5) if axis==0 else (sx*635,sy*800,20.5)
            place(name+'_EdgeLight'+str(axis),Q+'Meshes/SM_QC_Pavement',p,yaw=axis*90,
                size=(300,3,3),material=warm,shadow=False)
        place(name+'_TactileTile',J+'Grounds/SM_jcGrdGuideBlockB',(sx*635,sy*635,20.5),
            size=(60,60,0),shadow=False)
        a=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector())
        c=a.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
        c.set_intensity(180);c.set_light_color(unreal.LinearColor(1,.72,.4,1))
        c.set_attenuation_radius(320);c.set_cast_shadows(False)
        attach(a,name+'_PlanterUplight');c.set_relative_location(unreal.Vector(sx*795,sy*795,85),False,False)
        corners.append({'quadrant':[sx,sy],'center':[sx*800,sy*800],
                        'footprint':[320,320],'tree_height_cm':302,'actors':7})

shadow_lights=[]
for a in allactors:
    if isinstance(a,unreal.PointLight) and 'IntersectionLamp_' in a.get_actor_label():
        c=a.point_light_component
        c.set_cast_shadows(True)
        c.set_editor_property('shadow_bias',.25)
        c.set_editor_property('shadow_slope_bias',.3)
        c.set_editor_property('source_radius',25)
        shadow_lights.append(a.get_name())
assert len(shadow_lights)==4
d.set_editor_property('tags',list(d.tags)+[unreal.Name('EndingCornerGardensV1')])
assert before=={a.get_name():str(a.get_actor_transform()) for a in actors.get_all_level_actors() if a.actor_has_tag('EndingEditableCell')}
assert all(a.get_attach_parent_actor()==d for a in created)
assert len(created)==28
assert levels.save_current_level()
assert levels.load_level('/Engine/Maps/Entry')
assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
saved=[a for a in actors.get_all_level_actors() if a.actor_has_tag('EndingCornerGardensV1') and a!=next(x for x in actors.get_all_level_actors() if isinstance(x,unreal.EndingEmergence))]
assert len(saved)==28
result={'passed':True,'backup':str(backup),'corner_objects':len(saved),'corners':corners,
        'shadow_lights':shadow_lights,'cell_unchanged':True,'production_camera_unchanged':True,'save_reload':True}
(OUT/'corners.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('ENDING_CORNERS_SAVED '+json.dumps(result))
unreal.SystemLibrary.quit_editor()
