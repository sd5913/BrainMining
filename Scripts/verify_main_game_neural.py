"""Reload-only verification of closed neural region, all blockers and sculpted rock excavation."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/verify_main_game_cavern.py').read_text()
code=code.replace("old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')",
    "old=find('BM_MainGame_StalagmiteV3');new=find('BM_MainGame_NeuralV4');reserve=find('BM_MainGame_FutureRoom_03')")
for a,b in {'TunnelCavernV2':'TunnelNeuralV4','cavern_reload':'neural_reload','CAVERN_RELOAD':'NEURAL_RELOAD',
    "ART/'pillars.json'":"ART/'neural_rocks.json'","p=raw(spec['center_m'])+origin":"p=raw(spec['probe_m'])+origin",
    'pillars':'neural_rocks','columns=':'neural_rocks='}.items():code=code.replace(a,b)
extra="""    spec=json.loads((ART/'neural_rocks.json').read_text())[0]
    initial=new.generated_rock.triangle_count
    assert new.generated_rock.mine_sphere(raw(spec['probe_m'])+origin,90),str(new.generated_rock.last_status)
    new.reset_excavation();assert new.generated_rock.successful_cuts==0 and new.generated_rock.triangle_count==initial
    report.update(neural_rock_excavation_and_reset=True)
"""
code=code.replace('    report.update(passed=True',extra+'    report.update(passed=True')
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
