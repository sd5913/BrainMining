"""Replace box adapters with mineable cave solids and sealed rock backing."""
import unreal,json,hashlib,sys,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import main_game_room_entrances_geometry as geometry
import campaign_minerals_api as minerals
OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());baseline=json.loads((OUT/'before.json').read_text());report={'passed':False,'backup':baseline['backup'],'connections':[]}
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def V(p):return unreal.Vector(*p)
def tag(a,label,area):
    assert a.get_level()==hall.get_level();a.set_actor_label(label);a.set_folder_path('MainGame/LinkedRooms/NaturalEntrances');a.set_editor_property('tags',[area,'BM_MainGame_RoomConnection','BM_MainGame_NaturalRoomConnection']);return a
def import_rock(label,spec,data,material):
    a=tag(actors.spawn_actor_from_class(unreal.GeneratedMiningRock,V([spec['plug_x'],spec['a'][1],spec['a'][2]+180])),label,spec['tag']);a.set_editor_property('rock_material',material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    path=OUT/(label+'.obj');geometry.write_obj(path,*data);assert unreal.MiningDesignerLibrary.import_campaign_rock(a,str(path));a.reset_rock();assert a.triangle_count>0;return a
try:
    assert not ed.get_game_world();assert ed.get_editor_world().get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame';assert levels.set_current_level_by_name('Lvl_MainGame')
    existing=list(actors.get_all_level_actors());assert not any(a.actor_has_tag('BM_MainGame_NaturalRoomConnection') for a in existing),'Already refined'
    hall=next(a for a in existing if isinstance(a,unreal.MiningExhibitHall));field=minerals.get_field(hall);before=minerals.validate(field,hall);assert before['ok']
    regions=[next(a for a in existing if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1))) for i in range(4)]
    solids=[(r.generated_rock.get_path_name(),r.generated_rock.triangle_count) for r in regions]
    specs=[]
    for i,original in enumerate(cfg['generated_connections']):
        s=geometry.setup(original,i,cfg['rooms']);prefix='LINK_'+s['id'];region=regions[s['mine']]
        # The short original room vestibule is covered by an irregular rock
        # throat. No artificial rectangular adapter remains visible.
        for a in existing:
            if a.get_actor_label() in [prefix+k for k in ['_Floor','_Roof','_North','_South','_DiggableSoil']]:assert actors.destroy_actor(a)
        rock=import_rock(prefix+'_NaturalRock',s,geometry.closed_tube(s),region.rock_material)
        vertices,triangles=geometry.assert_closed(*geometry.closed_tube(s,True));origin=V([s['plug_x'],s['a'][1],s['a'][2]+180])
        shell=tag(actors.spawn_actor_from_class(unreal.DynamicMeshActor,origin),prefix+'_BedrockShell',s['tag']);c=shell.get_component_by_class(unreal.DynamicMeshComponent)
        buffers=unreal.GeometryScriptSimpleMeshBuffers(vertices=[V(p)-origin for p in vertices],triangles=[unreal.IntVector(*t) for t in triangles]);mesh=c.get_dynamic_mesh();unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(mesh,buffers)
        unreal.GeometryScript_Normals.set_per_face_normals(mesh);assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
        c.set_material(0,region.shell_material);c.set_collision_profile_name('BlockAll');c.set_complex_as_simple_collision_enabled(True,True);c.update_collision(False)
        ring=geometry.cross(s,s['plug_x']);lo=[min(p[j] for p in ring) for j in range(3)];hi=[max(p[j] for p in ring) for j in range(3)]
        plug=tag(actors.spawn_actor_from_class(unreal.GeneratedMiningRock,unreal.Vector(s['plug_x'],(lo[1]+hi[1])/2,(lo[2]+hi[2])/2)),prefix+'_DiggableSoil',s['tag'])
        plug.set_editor_property('rock_material',unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage'),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        assert unreal.MiningDesignerLibrary.build_campaign_soil(plug,unreal.Vector(190,hi[1]-lo[1]+90,hi[2]-lo[2]+80),unreal.Vector(24,10,10));plug.reset_rock()
        # The old crop collar stays as an outer map seal, behind the new cave.
        mouth=V(cfg['mine_origins'][s['mine']])+unreal.Vector(-6500 if s['side']=='entry' else 6500,0,6000 if s['side']=='entry' else 500)
        oldprefix='LINK_'+s['tag']+'_'+str(round(mouth.x))+'_Collar_'
        for a in existing:
            name=a.get_actor_label()
            if not name.startswith(oldprefix):continue
            p,e=a.get_actor_bounds(False);lo=p-e;hi=p+e;side=name.removeprefix(oldprefix);y=s['a'][1];f=s['a'][2]
            if side=='South':hi.y=y-500
            if side=='North':lo.y=y+500
            if side in ['Above','Below']:lo.y=y-500;hi.y=y+500
            if side=='Above':lo.z=f+900
            if side=='Below':hi.z=f
            assert all(v>0 for v in (hi-lo).to_tuple());a.set_actor_location((lo+hi)*.5,False,True);a.set_actor_scale3d((hi-lo)/100.)
        s.update(plug=plug.get_path_name(),rock=rock.get_path_name(),shell=shell.get_path_name(),height=s['room_height']);specs.append(s)
        report['connections'].append({'id':s['id'],'rock_triangles':rock.triangle_count,'soil_triangles':plug.triangle_count,'plug_x':s['plug_x']})
    after=minerals.validate(field,hall);assert after['ok'] and after['layout_sha256']==before['layout_sha256']
    assert solids==[(r.generated_rock.get_path_name(),r.generated_rock.triangle_count) for r in regions]
    for source,digest in baseline['source_hashes'].items():assert hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest,source
    hall.set_editor_property('tags',list(hall.tags)+['BM_MainGame_NaturalRoomConnections'])
    assert levels.save_current_level();cfg.update(entrances_revision='natural-room-entrances-v2',generated_connections=specs);(ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8')
    report.update(passed=True,saved=True,mineral_layout_unchanged=True,original_regions_unchanged=True,source_maps_unchanged=True)
except Exception:report['error']=traceback.format_exc();raise
finally:(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('NATURAL_ROOM_ENTRANCES '+json.dumps(report,ensure_ascii=False))
