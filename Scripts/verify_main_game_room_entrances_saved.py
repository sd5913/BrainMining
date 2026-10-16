"""Reload cave interface solids, hard backing and original map references."""
import unreal,json,hashlib,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as minerals
import main_game_room_entrances_geometry as geometry
OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());baseline=json.loads((OUT/'before.json').read_text());report={'passed':False,'checks':[],'connections':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
try:
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not ed.get_game_world();assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));named={a.get_actor_label():a for a in actors};world=ed.get_editor_world()
    for i,s in enumerate(cfg['generated_connections']):
        prefix='LINK_'+s['id'];check(not any(prefix+k in named for k in ['_Floor','_Roof','_North','_South']),'No rectangular adapter pieces remain: '+s['id'])
        rock=named[prefix+'_NaturalRock'];shell=named[prefix+'_BedrockShell'];soil=named[prefix+'_DiggableSoil'];c=shell.get_component_by_class(unreal.DynamicMeshComponent)
        check(isinstance(rock,unreal.GeneratedMiningRock) and rock.triangle_count>500 and soil.triangle_count>=48,'Saved faceted diggable rock and shaped clay: '+s['id'])
        check(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(c.get_dynamic_mesh()),'Saved unmineable shell is a closed solid: '+s['id'])
        report['connections'].append({'id':s['id'],'rock_triangles':rock.triangle_count,'soil_triangles':soil.triangle_count,'floor_samples':len(s['floor_samples']),'plug_position':list(soil.get_actor_location().to_tuple())})
        ignored=[a for a in actors if isinstance(a,unreal.DiggableRock)]
        x=(s['a'][0]+s['b'][0])/2;p=unreal.Vector(x,s['a'][1],geometry.floor_at(s,x)+100)
        for delta in [unreal.Vector(0,2000,0),unreal.Vector(0,-2000,0),unreal.Vector(0,0,1800),unreal.Vector(0,0,-1000)]:
            h=unreal.SystemLibrary.line_trace_single(world,p,p+delta,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True);h=h.to_tuple() if h else None;check(h and h[0] and not isinstance(h[9],unreal.DiggableRock),'Hard map boundary remains after excavating '+s['id']+' '+str(delta))
        if i%2==0:check(abs(hall.areas[s['mine']*2].exit.x-cfg['room_activation_x'][i//2])<.01,'Gameplay activation occurs at the original room doorway: '+s['id'])
    valid=minerals.validate(minerals.get_field(hall),hall);check(valid['ok'] and valid['deposits']==165 and valid['fragments']==187491,'All original mineral placements, fragments and hosts still valid')
    for source,digest in baseline['source_hashes'].items():check(hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest,'Original map unchanged: '+source)
    check(len(hall.areas)==7,'Seven referenced campaign sections preserved');report.update(mineral_layout_sha256=valid['layout_sha256'])
    original_counts=[74780,84182,82720,92184]
    for i,count in enumerate(original_counts):
        region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)))
        check(region.generated_rock.triangle_count==count,'Original mine '+str(i+1)+' diggable rock mesh remains intact')
    report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:(OUT/'saved_state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('NATURAL_ENTRANCES_SAVED '+json.dumps(report,ensure_ascii=False))
