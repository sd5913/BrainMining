"""Saved clods protrude, can be mined/reset, and retain the original clay material."""
import unreal,json,sys,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
OUT=ROOT/'Saved/Validation/MainGameRoutesV5/ClayRelief';report={'passed':False,'checks':[]}
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
try:
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world,hall,actors=api._context();all_actors=actors.get_all_level_actors()
    clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage')
    records=json.loads((ROOT/'Art/MainGame/RoutesV5/ClayRelief/manifest.json').read_text())['regions']
    for i,item in enumerate(records):
        region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}'))
        check(all(g.rock_material==clay for g in region.generated_guide_layers if not g.actor_has_tag('BM_BombOnly')),'Original clay material on region '+str(i+1))
        clods=[a for a in all_actors if a.get_actor_label().startswith(f'MAIN_GAME_ClayWallClods_{i+1}_')]
        check(len(clods)==len(item['clod_meshes']),'Saved clay clod groups '+str(i+1))
        hits=[]
        for rec in item['samples']:
            p=raw(rec['position']);n=unreal.Vector(rec['normal'][0],-rec['normal'][1],rec['normal'][2]);side=n.cross(unreal.Vector(0,0,1))*.7
            h=unreal.SystemLibrary.line_trace_single(world,p+side+n*65,p+side-n*35,unreal.TraceTypeQuery.ECC_VISIBILITY,True,region.generated_guide_layers,unreal.DrawDebugTrace.NONE,True)
            if not h:continue
            h=h.to_tuple()
            if h[0] and h[9] in clods:
                relief=(h[5]-p).dot(n)
                if relief>3:hits.append((h[9],h[5],relief))
            if len(hits)>=6:break
        check(len(hits)>=6,'Six independently traced protruding clay chunks '+str(i+1))
        a,p,depth=hits[0];before=a.triangle_count
        check(a.mine_sphere(p,65) and a.successful_cuts>0,'Wall clay chunks are really excavatable '+str(i+1))
        a.reset_rock();check(a.triangle_count==before and a.successful_cuts==0,'Reset restores wall clay chunks '+str(i+1))
    valid=api.validate(api.get_field(hall),hall);check(valid['ok'],'All 139 mineral placements retain valid hosts')
    report.update(passed=True,minerals=valid)
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'saved.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
