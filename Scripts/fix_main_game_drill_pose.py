"""Restore MainGame's off-screen drills to the existing shared drill hold pose."""
import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameDrill';OUT.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
path='/Game/MainGame/Blueprints/BP_MainGameCharacter';bp=unreal.load_asset(path)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
backup=ROOT/'Saved/Backups'/('MainGame_DrillPose_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
shutil.copy2(ROOT/'Content/MainGame/Blueprints/BP_MainGameCharacter.uasset',backup/'BP_MainGameCharacter.uasset')
protected=['Content/MainGame/Maps/Lvl_MainGame.umap','Content/Showcase/Blueprints/BP_SharedMiningCharacter.uasset','Content/FirstPerson/Blueprints/BP_FirstPersonCharacter.uasset']
def sha(p):return hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
hashes={p:sha(p) for p in protected}
source=unreal.get_default_object(unreal.load_asset('/Game/Showcase/Blueprints/BP_SharedMiningCharacter').generated_class())
subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
names=['DrillRightMount','DrillLeftMount','DrillRightModel','DrillLeftModel'];components={}
for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    c=lib.get_object_for_blueprint(lib.get_data(h),bp)
    if c and c.get_name() in names:components[c.get_name()]=c
assert set(components)==set(names),list(components)
report={'passed':False,'backup':str(backup),'components':[]}
for name,c in components.items():
    original=source.get_editor_property(name)
    record={'name':name,'template':c.get_path_name(),'before':{},'after':{}}
    for prop in ['relative_location','relative_rotation','relative_scale3d']:
        record['before'][prop]=list(c.get_editor_property(prop).to_tuple())
        value=original.get_editor_property(prop);c.set_editor_property(prop,value);record['after'][prop]=list(value.to_tuple())
    report['components'].append(record)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
for name in names:
    for prop in ['relative_location','relative_rotation','relative_scale3d']:
        a=cdo.get_editor_property(name).get_editor_property(prop);b=source.get_editor_property(name).get_editor_property(prop)
        assert all(abs(x-y)<.001 for x,y in zip(a.to_tuple(),b.to_tuple())),(name,prop)
assert all(c.get_editor_property('hidden_in_game') for c in cdo.get_components_by_class(unreal.SkeletalMeshComponent))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
assert all(sha(p)==digest for p,digest in hashes.items()),'Protected source asset was changed'
report.update(passed=True,protected_hashes=hashes)
(OUT/'authoring.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_DRILL_POSE_SAVED')
