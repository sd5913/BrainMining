"""Offline authored-layout migration; PIE never randomizes these placements."""
import json, math, shutil, traceback, time
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
try:
    (root/'Saved/MiningInteriorBakeError.txt').unlink(missing_ok=True)
    backup=root/'Saved/CampaignBackups'/('mining-interior-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(root/'Content/Campaign/Maps/Lvl_FullJourney.umap',backup/'Lvl_FullJourney.umap')
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    field=next(a for a in actors if isinstance(a,unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements'))
    def vec(v):return (v.x,v.y,v.z)
    def transform(t):return (vec(t.translation),vec(t.scale3d),(t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w))
    def surface(p):
        return [(transform(t),vec(s)) for t,s in zip(p.fragments,p.fragment_supports) if (t.translation-s).length()>1]
    before=[(transform(p.transform),p.value,surface(p)) for p in field.get_editor_property('authored_placements')]
    assert unreal.MiningDesignerLibrary.refine_campaign_crystal_clues(field,True)
    after=field.get_editor_property('authored_placements')
    shells=[]
    for p,(core,value,outer) in zip(after,before):
        assert transform(p.transform)==core and p.value==value
        assert surface(p)==outer,'Existing surface clues moved'
        counts=[0,0,0]
        for q in p.fragment_supports:
            distance=(q-p.transform.translation).length()
            for j,(low,high) in enumerate(((0,80),(80,160),(160,225))):
                if low<=distance<high:counts[j]+=1
        density=[count/(4*math.pi/3*(high**3-low**3))*1000000 for count,(low,high) in zip(counts,((0,80),(80,160),(160,225)))]
        shells.append(dict(counts=counts,per_cubic_metre=density))
        assert density[0]>density[1]>density[2],f'Core density must rise toward the gem: {density}'
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    saved=False
    for attempt in range(3):
        if unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Campaign/Maps/Lvl_FullJourney'):
            saved=True
            break
        time.sleep(1)
    assert saved,'Could not save the validated authored layout'
    report=dict(deposits=len(after),surface_unchanged=True,core_unchanged=True,fragments=sum(len(p.fragments) for p in after),counts=[len(p.fragments) for p in after],core_density_shells=shells)
    (root/'Saved/MiningInteriorBake.json').write_text(json.dumps(report,indent=2))
    unreal.log('MINING_INTERIOR_BAKED '+json.dumps(report))
except Exception:
    (root/'Saved/MiningInteriorBakeError.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
