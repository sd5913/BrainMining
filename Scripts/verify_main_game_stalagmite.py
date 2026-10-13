"""Saved-map reload checks of all third-region blockers and floor-attached stalagmites."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/verify_main_game_cavern.py').read_text()
code=code.replace("old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')",
    "old=find('BM_MainGame_CavernV2');new=find('BM_MainGame_StalagmiteV3');reserve=find('BM_MainGame_FutureRoom_02')")
for a,b in {'TunnelCavernV2':'TunnelStalagmiteV3','cavern_reload':'stalagmite_reload','CAVERN_RELOAD':'STALAGMITE_RELOAD',
    "ART/'pillars.json'":"ART/'stalagmites.json'","p=raw(spec['center_m'])+origin":"p=raw(spec['probe_m'])+origin",
    'pillars':'stalagmites','columns=':'stalagmites='}.items():code=code.replace(a,b)
extra="""    for spec in json.loads((ART/'stalagmites.json').read_text()):
        tip=raw(spec['tip_m'])+origin
        assert actor(trace(world,tip+unreal.Vector(0,0,80),tip+unreal.Vector(0,0,200),ignore)) is None,(spec['name'],'Tip unexpectedly connected to ceiling')
    spec=json.loads((ART/'stalagmites.json').read_text())[0]
    initial=new.generated_rock.triangle_count
    assert new.generated_rock.mine_sphere(raw(spec['probe_m'])+origin,70),str(new.generated_rock.last_status)
    new.reset_excavation();assert new.generated_rock.successful_cuts==0 and new.generated_rock.triangle_count==initial
    report.update(stalagmite_excavation_and_reset=True,stone_tips_have_free_air=True)
"""
code=code.replace('    report.update(passed=True',extra+'    report.update(passed=True')
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
