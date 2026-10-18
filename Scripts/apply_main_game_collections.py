"""Build an isolated MainGame preview from the latest saved terrain.

The JSON recipe is the merge unit. This entry point never saves MainGame or any
source room. Rebuilding replaces only our preview (after a timestamped backup).
"""
import unreal,json,math,hashlib,shutil,traceback,sys
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameCollections';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'Scripts'))
from main_game_collection_dressing import dress_heaven
CFG=ROOT/'Config/MainGameCollections.json';cfg=json.loads(CFG.read_text(encoding='utf-8'))
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.MiningDesignerLibrary;assets=unreal.EditorAssetLibrary;TAG=cfg['ownership_tag'];I=unreal.Transform()
report={'passed':False,'placements':[],'terrain_edits':[]}
terrain_sources={}
def V(p):return unreal.Vector(*p)
def xyz(p):return list(p.to_tuple())
def setp(a,k,v):a.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def point(t,p):return unreal.MathLibrary.transform_location(t,p)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def log(phase):
    report['phase']=phase;(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_COLLECTIONS '+phase)
def owned(a,area):
    assert a.get_level()==hall.get_level(),'Attempt to author into a shared source room'
    setp(a,'tags',list(a.tags)+[TAG,area]);a.set_folder_path('MainGame/Collections');return a
def trace(a,b,ignored=None):
    h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored or [],unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def placement(item,area,p,rot=None,scale=1,rock=None,clue=None):
    t=unreal.Transform(location=p,rotation=rot or unreal.Rotator(),scale=unreal.Vector(scale,scale,scale))
    a=unreal.MiningCollectionPlacement(item_id=item,area_tag=area,transform=t,clue_transform=clue or t,host_rock=rock)
    placements.append(a)
    report['placements'].append(dict(item_id=item,area_tag=area,position=xyz(p),clue=xyz(a.clue_transform.translation),host=rock.get_name() if rock else None))
    return a
def site(spec):
    area=next(a for a in hall.areas if str(a.tag)==spec['area_tag']);route=list(area.map_route);k=round((len(route)-1)*spec['route_fraction'])
    indices=sorted(range(max(2,k-12),min(len(route)-2,k+13)),key=lambda j:abs(j-k))
    for j in indices:
        p=route[j];floor=trace(p,p-unreal.Vector(0,0,3000))
        if not floor or floor[1]:continue
        ground=floor[5];height=240 if spec['item_id']==1 else spec.get('height_cm',160)
        start=unreal.Vector(p.x,p.y,ground.z+height);direction=unreal.Vector(0,spec['side'],0)
        if spec.get('floor'):
            start=p+direction*90;h=trace(start,start-unreal.Vector(0,0,3000))
        else:h=trace(start,start+direction*2300)
        if not h or not isinstance(h[9],unreal.DiggableRock) or h[1] or h[5].distance(start)<50:continue
        if not h[9].actor_has_tag(spec['area_tag']):continue
        if spec.get('floor') and start.z-h[5].z>600:continue
        if spec.get('floor') and h[7].z<.75:continue
        if not spec.get('floor') and abs(h[7].z)>.6:continue
        front=h[5];normal=h[7];rock=h[9]
        # Keep the recipe spatially separated from existing crystal seams.
        if any(front.distance(p.surface.translation)<180 for f in mineral_fields for p in f.get_editor_property('authored_placements')):continue
        return dict(front=front,normal=normal,rock=rock,ground=ground,route=p,index=j)
    raise RuntimeError('No safe diggable site near '+str(spec))
def cut_box(actor,center,size,rotation,cylinder=False):
    # Bounds reject unrelated actors before extracting/rebuilding any terrain.
    c,e=actor.get_actor_bounds(False);r=math.radians(rotation.yaw)
    extent=unreal.Vector(abs(math.cos(r))*size.x/2+abs(math.sin(r))*size.y/2,abs(math.sin(r))*size.x/2+abs(math.cos(r))*size.y/2,size.z/2)
    if any(abs(c.to_tuple()[i]-center.to_tuple()[i])>e.to_tuple()[i]+extent.to_tuple()[i] for i in range(3)):return
    if actor not in terrain_sources:terrain_sources[actor]=lib.get_campaign_rock_source(actor)
    source=terrain_sources[actor]
    if not source:return
    tool=unreal.DynamicMesh();transform=unreal.Transform(location=center,rotation=rotation)
    if cylinder:unreal.GeometryScript_Primitives.append_cylinder(tool,unreal.GeometryScriptPrimitiveOptions(),transform,radius=size.x/2,height=size.z,radial_steps=24,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    else:unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),transform,dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    before=source.get_triangle_count()
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(source,actor.get_actor_transform(),tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
    report['terrain_edits'].append(dict(actor=actor.get_name(),before=before,after=source.get_triangle_count()))
def cube(label,p,size,material,area,rot=None):
    a=owned(api.spawn_actor_from_class(unreal.StaticMeshActor,p,rot or unreal.Rotator()),area);a.set_actor_label(label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,material)
    a.static_mesh_component.set_collision_profile_name('BlockAll');a.set_actor_scale3d(size/100);return a
def heaven(spec,s):
    area=spec['area_tag'];front=s['front'];yaw=90 if spec['side']>0 else -90;rotation=unreal.Rotator(yaw=yaw)
    # Original exposed pipe at X=-300 meets the new tunnel wall; the soil begins 7.6m inside the branch.
    frame=unreal.Transform(location=unreal.Vector(front.x,front.y,s['ground'].z)+unreal.MathLibrary.get_forward_vector(rotation)*300,rotation=rotation)
    native=[a for a in api.get_all_level_actors() if a.get_level()==hall.get_level() and a.actor_has_tag(area) and (isinstance(a,unreal.DiggableRock) or isinstance(a,unreal.DynamicMeshActor))]
    for a in native:
        cut_box(a,point(frame,unreal.Vector(520,0,225)),unreal.Vector(1800,660,460),rotation)
        cut_box(a,point(frame,unreal.Vector(2580,0,670)),unreal.Vector(2680,2680,1350),rotation)
    copies=list(lib.copy_experience_actors('/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet',unreal.Vector(),area))
    source_field=next(a for a in copies if isinstance(a,unreal.MiningCollectionField));source_placement=source_field.authored_placements[0]
    toilet=source_placement.presentation_actor
    local_item=source_placement.transform.translation
    keep=[]
    for a in copies:
        if isinstance(a,(unreal.MiningCollectionField,unreal.PlayerStart,unreal.PostProcessVolume,unreal.SkyLight,unreal.DirectionalLight,unreal.ExponentialHeightFog)) or a.get_actor_label().startswith('Entrance '):
            api.destroy_actor(a);continue
        old=a.get_actor_transform();old.translation=point(frame,old.translation);old.rotation=unreal.MathLibrary.compose_rotators(old.rotation.rotator(),rotation).quaternion()
        a.set_actor_transform(old,False,True);owned(a,area);keep.append(a)
    water=next(a for a in keep if isinstance(a,unreal.HeavenToiletWater));setp(water,'spill_coordinate_frame',frame)
    soils=[a for a in keep if isinstance(a,unreal.DiggableRock)];setp(water,'retaining_soil',soils);water.rebuild_water()
    mat=unreal.load_asset('/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenBedrock')
    cube('COL_Heaven_BranchFloor',point(frame,unreal.Vector(510,0,-35)),unreal.Vector(1860,720,70),mat,area,rotation)
    cube('COL_Heaven_BranchRoof',point(frame,unreal.Vector(510,0,480)),unreal.Vector(1860,720,70),mat,area,rotation)
    for side in [-1,1]:cube('COL_Heaven_BranchWall',point(frame,unreal.Vector(510,side*350,225)),unreal.Vector(1860,40,450),mat,area,rotation)
    lamp=owned(api.spawn_actor_from_class(unreal.PointLight,point(frame,unreal.Vector(-80,-180,320))),area);lamp.set_actor_label('COL_Heaven_PipeLight')
    lamp.light_component.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);lamp.light_component.set_intensity(120);lamp.light_component.set_editor_property('attenuation_radius',1200);lamp.light_component.set_editor_property('cast_shadows',False)
    p=placement(0,area,point(frame,local_item),rotation,clue=unreal.Transform(location=point(frame,unreal.Vector(-300,125,130)),rotation=rotation))
    if toilet:setp(p,'presentation_actor',toilet)
    report['heaven']=dict(frame_position=xyz(frame.translation),yaw=yaw,soil=[a.get_name() for a in soils],actors=len(keep),water=water.get_name())
    report['heaven']['dressing']=dress_heaven(ed.get_editor_world(),TAG,frame)

try:
    assert not ed.get_game_world()
    protected=[ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap']+[ROOT/'Content'/(r['source'][6:]+'.umap') for r in json.loads((ROOT/'Config/MainGameRooms.json').read_text())['rooms']]+[ROOT/'Content/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet.umap']
    hashes={str(p):sha(p) for p in protected};report['source_hashes']=hashes
    preview=cfg['preview_map'];assert preview=='/Game/CollectionIntegration/Maps/Lvl_MainGame_CollectionsPreview'
    # After promotion, formal MainGame already owns the cuts, branch and placements.
    # Refuse a second bake before touching the existing preview.
    assert levels.load_level(cfg['source_map'])
    assert not any(f.authored_placements for f in unreal.GameplayStatics.get_all_actors_of_class(ed.get_editor_world(),unreal.MiningCollectionField)), 'MainGame already contains collectibles; edit its owned layout directly instead of baking a second time.'
    assert levels.load_level('/Engine/Maps/Entry')
    if assets.does_asset_exist(preview):
        backup=ROOT/'Saved/Backups'/('CollectionsPreview_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap');backup.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/'Content'/(preview[6:]+'.umap'),backup);assert assets.delete_asset(preview)
    # World assets need the editor's Save As path; generic asset duplication leaves
    # an orphan standalone UWorld and triggers the editor's GC leak assertion.
    log('creating preview with Save As')
    assert levels.load_level(cfg['source_map'])
    assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),preview)
    assert levels.load_level(preview)
    assert ed.get_editor_world().get_path_name().split('.')[0]==preview
    assert levels.set_current_level_by_name(Path(preview).name)
    aa=list(api.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.get_level()==hall.get_level())
    mineral_fields=[a for a in aa if isinstance(a,unreal.MiningTreasureField)];placements=[]
    assert not field.authored_placements,'Existing collectible layout needs explicit reconciliation'
    for spec in cfg['mines']:
        log('binding item '+str(spec['item_id']));s=site(spec);front,normal,rock=s['front'],s['normal'],s['rock'];item=spec['item_id']
        if spec.get('room'):heaven(spec,s);continue
        clue=unreal.Transform(location=front,rotation=unreal.MathLibrary.make_rot_from_x(normal*-1));position=front-normal*spec['depth_cm']
        rot=unreal.Rotator(yaw=90 if spec['side']>0 else -90)
        if item==8:
            rot=unreal.Rotator()
            position=front-unreal.Vector(0,0,spec['depth_cm'])
        if item==1:position.z-=80
        if item==9:position=front-unreal.Vector(0,0,75)
        p=placement(item,spec['area_tag'],position,rot,rock=rock,clue=clue)
        if item==8:
            # A small underground pocket and one continuous UPWARD bore, both in real mine terrain.
            cut_box(rock,position+unreal.Vector(0,0,4),unreal.Vector(64,64,106),unreal.Rotator())
            cut_box(rock,unreal.Vector(position.x-6,position.y,(position.z+front.z)/2+45),unreal.Vector(28,28,front.z-position.z+90),unreal.Rotator(),cylinder=True)
        report['placements'][-1].update(route_index=s['index'],ground=xyz(s['ground']),normal=xyz(normal))
    log('room displays')
    echo=next(a for a in aa if isinstance(a,unreal.EchoPassageStage))
    for spec in cfg['echo']:
        p=placement(spec['item_id'],'BM_Campaign_MainGameJourney',point(echo.get_actor_transform(),V(spec['local_position'])),unreal.Rotator(yaw=spec['yaw']),spec['scale'])
        # Runtime resolves this streamed actor by area; cross-map hard references cannot be saved.
    # The exact chair is resolved from the office's authored label, never from array order.
    cassette=cfg['cassette'];chair=next(a for a in aa if a.get_actor_label()==cassette['chair_label'] and 'PopupClose1' in a.get_level().get_path_name())
    p=point(chair.get_actor_transform(),V(cassette['local_position']))
    seat=trace(unreal.Vector(p.x,p.y,chair.get_actor_location().z+75),unreal.Vector(p.x,p.y,chair.get_actor_location().z+20))
    assert seat and seat[9]==chair,'Cassette seat anchor no longer matches the chair'
    p.z=seat[5].z+29*cassette['scale']+.5
    placement(5,'BM_Campaign_MainGameOffice',p,unreal.Rotator(yaw=chair.get_actor_rotation().yaw+cassette['yaw_offset']),cassette['scale'])
    report['cassette']=dict(chair=chair.get_actor_label(),seat=xyz(seat[5]))
    log('rebuilding edited terrain once per rock')
    for actor,source in terrain_sources.items():
        if not source:continue
        if isinstance(actor,unreal.GeneratedMiningRock):assert lib.set_campaign_rock_source(actor,source),actor.get_actor_label()
        else:
            c=actor.get_component_by_class(unreal.DynamicMeshComponent);c.get_dynamic_mesh().reset();unreal.GeometryScript_MeshEdits.append_mesh(c.get_dynamic_mesh(),source,I);c.update_collision(False)
    setp(field,'authored_placements',placements);setp(field,'use_authored_placements',True);setp(field,'buried_with_clues',True);setp(field,'display_version','MainGame / Collections')
    setp(field,'tags',list(field.tags)+[TAG])
    # MainGame's shared field already authors I as its catalogue key.
    assert len(placements)==11 and set(p.item_id for p in placements)==set(range(11))
    assert levels.save_current_level()
    assert all(sha(Path(p))==digest for p,digest in hashes.items()),'A shared source map changed during preview generation'
    report.update(passed=True,map=preview,shared_maps_unchanged=True,config_sha256=sha(CFG),ribbon='Existing TV stage, registered at escape')
    log('complete')
except Exception:
    report['error']=traceback.format_exc();log('failed');raise
finally:unreal.SystemLibrary.quit_editor()
