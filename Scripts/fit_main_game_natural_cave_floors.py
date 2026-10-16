"""Blend the six new collars onto the existing, unmodified mine floors."""
import unreal,json,math,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));regions=[next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1))) for i in range(4)]
paths=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4'];report=[]
for i,s in enumerate(cfg['generated_connections']):
    base=cfg['mine_origins'][s['mine']];side=s['side'];mouth=base[0]+(-6500 if side=='entry' else 6500)
    s['start_x']=s['a'][0]-(25 if side=='entry' else 300);s['end_x']=s['b'][0]+(300 if side=='entry' else 25)
    if i==1:s['start_x']=cfg['rooms'][0]['origin'][0]+980
    lines=json.loads((ROOT/'Art/MainGame/RoutesV2'/paths[s['mine']]/'centerlines.json').read_text());points=next(p['points'] for p in lines if p['name']=='Main');points=points[:80] if side=='entry' else points[-80:]
    mine_start=mouth+5 if side=='entry' else s['start_x'];mine_end=s['end_x'] if side=='entry' else mouth-5
    count=math.ceil((mine_end-mine_start)/35);samples=[]
    for j in range(count+1):
        x=mine_start+(mine_end-mine_start)*j/count;p=min(points,key=lambda p:abs(base[0]+p[0]*100-x));z=base[2]+p[2]*100+20;hits=[]
        for y in [-60,-30,0,30,60]:
            start=unreal.Vector(x,s['a'][1]+y,z);end=start-unreal.Vector(0,0,2000);hit=unreal.MiningDesignerLibrary.find_campaign_wall(regions[s['mine']].generated_rock,start,end)
            if hit.distance(end)>1:hits.append(hit.z)
        if hits:samples.append([x,max(hits)+3])
    assert len(samples)>=2,(s['id'],'No overlap with original cave floor')
    height=samples[0][1] if side=='entry' else samples[-1][1];anchor=s['start_x'] if side=='entry' else s['end_x'];edge=samples[0][0] if side=='entry' else samples[-1][0]
    for j in range(17):
        t=j/16;x=anchor+(edge-anchor)*t;v=t*t*(3-2*t);samples.append([x,s['a'][2]+(height-s['a'][2])*v])
    s['floor_samples']=sorted({x:h for x,h in samples}.items());s['floor_samples']=[list(p) for p in s['floor_samples']]
    report.append({'id':s['id'],'mine_floor':height,'room_floor':s['a'][2],'original_mine_triangles':regions[s['mine']].generated_rock.triangle_count})
areas=list(hall.areas)
for s in cfg['generated_connections']:
    if s['side']=='entry':
        a=areas[s['mine']*2];entry=a.entry;p=entry.translation;p.z=geometry.floor_at(s,p.x)+96;entry.translation=p;a.set_editor_property('entry',entry)
hall.set_editor_property('areas',areas,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
cfg['mine_entry_positions']=[list(areas[i*2].entry.translation.to_tuple()) for i in range(4)]
(ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8');assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(ROOT/'Saved/Validation/MainGameRoomEntrances/floor_fit.json').write_text(json.dumps(report,indent=2));unreal.log('NATURAL_CAVE_FLOOR_FIT_SAVED')
