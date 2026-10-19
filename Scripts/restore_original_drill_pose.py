"""Restore a low drill hold, calibrated for the campaign first-person camera."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/PackagePlaytest';out.mkdir(parents=True,exist_ok=True)
backup=root/'Saved/Backups'/('OriginalDrillPose_'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
source=unreal.get_default_object(unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter').generated_class())
names=['DrillRightMount','DrillLeftMount','DrillRightModel','DrillLeftModel']
props=['relative_location','relative_rotation','relative_scale3d']
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib=unreal.SubobjectDataBlueprintFunctionLibrary
report={'passed':False,'backup':str(backup),'pawns':{}}
for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/MainGame/Blueprints/BP_MainGameCharacter']:
    bp=unreal.load_asset(path);assert bp
    shutil.copy2(root/('Content/'+path.removeprefix('/Game/')+'.uasset'),backup/(bp.get_name()+'.uasset'))
    parts={}
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        c=lib.get_object_for_blueprint(lib.get_data(h),bp)
        if c and c.get_name() in names:parts[c.get_name()]=c
    assert set(parts)==set(names)
    report['pawns'][path]={}
    for name,c in parts.items():
        original=source.get_editor_property(name)
        values={p:original.get_editor_property(p) for p in props}
        # Keep the authored -10 degree model pitch, but adapt translation to
        # the campaign's 70-degree first-person projection. The old template's
        # near-camera offset otherwise pushes both drills outside the viewport.
        values['relative_location']=unreal.Vector(80,-28 if 'Left' in name else 28,-32) if name.endswith('Mount') else unreal.Vector()
        report['pawns'][path][name]={p:dict(before=list(c.get_editor_property(p).to_tuple()),after=list(values[p].to_tuple())) for p in props}
        for p in props:c.set_editor_property(p,values[p])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo=unreal.get_default_object(bp.generated_class())
    for name in names:
        for p in props:
            actual=cdo.get_editor_property(name).get_editor_property(p).to_tuple()
            expected=report['pawns'][path][name][p]['after']
            assert all(abs(a-b)<.001 for a,b in zip(actual,expected)),(name,p)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
report['passed']=True
(out/'restored-drill-pose.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
