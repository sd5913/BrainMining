"""Real pickaxe/drill input with normal lazy terrain initialization enabled."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/inspect_main_game_mining_fx_runtime.py').read_text(encoding='utf-8')
code=code.replace("OUT=ROOT/'Saved/Validation/MainGameMiningFX'", "OUT=ROOT/'Saved/Validation/MainGameRoutesV4/FX'")
code=code.replace("ROOT/'Art/MainGame/RoutesV3'", "ROOT/'Art/MainGame/RoutesV4'")
code=code.replace("if STRICT and not s.get('upgraded'):", "if STRICT and s['index']>=4 and not s.get('upgraded'):")
# Scouting must follow normal player proximity loading, rather than eagerly
# loading the entire mine and concealing lazy component registration problems.
a=code.index("            h=unreal.SystemLibrary.line_trace_single(world,p,end,")
code=code[:a]+"""            s.update(scout_p=p,scout_end=end,ignored=ignored,scout_region=region,scout_stage=stage,scout_guide=guide)
            move(p-unreal.Vector(0,0,64));aim(end);s.update(phase='scout',next=now+1.0)
        elif phase=='scout':
            p=s['scout_p'];end=s['scout_end'];ignored=s['ignored'];region=s['scout_region'];stage=s['scout_stage'];guide=s['scout_guide']
"""+code[a:]
code=code.replace("phase='observe',next=now+.65", "phase='observe',next=now+.40")
code=code.replace("if STRICT and cases[s['index']][2]:s.update(phase='motion',next=now+.12)", "if STRICT:s.update(phase='motion',next=now+.12)")
a=code.index("        if cases[s['index']][2]:");b=code.index("    (OUT/REPORT_FILE)",a)
code=code[:a]+"""        current=[a for a in unreal.GameplayStatics.get_all_actors_of_class(s['world'],unreal.MiningDrillDustBurst) if a.get_actor_location().distance(s['point'])<200]
        check(bool(current) and all(a.is_actor_tick_enabled() and not a.get_editor_property('hidden') and a.sprites.get_instance_count()>0 for a in current),'Visible moving dust on the actual mined surface '+str(cases[s['index']]))
        if cases[s['index']][2]:
            check(any(a.smoke_sprites.get_instance_count()>0 and a.spark_sprites.get_instance_count()>0 for a in current),'Drill keeps its smoke and sparks '+str(cases[s['index']]))
        else:
            check(any(a.actor_has_tag('BM_PickaxeImpact') and a.smoke_sprites.get_instance_count()==0 and a.spark_sprites.get_instance_count()==0 for a in current),'Pickaxe emits a short dust-only puff '+str(cases[s['index']]))
        s['cloud_probes']=[(a,a.sprites.get_instance_transform(0,True).translation) for a in current]
"""+code[b:]
code=code.replace("'bm.Mining.LazyRuntimeChunks 0'", "'bm.Mining.LazyRuntimeChunks 1'")
exec(compile(code,'default_streamed_mining_fx','exec'),dict(__name__='__main__',STRICT=True,cases=[case for i in range(4) for case in [(i,.23,False),(i,.23,False,'guide'),(i,.8,True),(i,.8,True,'guide')]]))
