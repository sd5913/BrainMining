"""Replace the uneven imported rotation take with one constant-speed full turn."""
import json,math
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());path='/Game/Drill/CF_DrillSpin'
curve=unreal.load_asset(path)
times=[i/60. for i in range(31)]
before=[curve.get_float_value(t) for t in times]
csv=root/'SourceAssets/Drill/CF_DrillSpin.csv'
csv.write_text('0.000000000,0.000000000\n0.500000000,360.000000000\n',encoding='utf-8')
factory=unreal.CSVImportFactory();settings=unreal.CSVImportSettings()
settings.import_type=unreal.CSVImportType.ECSV_CURVE_FLOAT
settings.import_curve_interp_mode=unreal.RichCurveInterpMode.RCIM_LINEAR
factory.automated_import_settings=settings
task=unreal.AssetImportTask();task.filename=str(csv);task.destination_path='/Game/Drill';task.destination_name='CF_DrillSpin'
task.factory=factory;task.automated=True;task.replace_existing=True;task.save=False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
curve=unreal.load_asset(path)
after=[curve.get_float_value(t) for t in times]
assert all(abs(value-t*720)<.002 for t,value in zip(times,after))
assert all(abs((b-a)*60-720)<.1 for a,b in zip(after,after[1:]))
# Verify the runtime's phase/cycle accumulation at and on both sides of wraps.
def runtime_angle(t):
 cycles=math.floor(t/.5);phase=t% .5
 return cycles*(curve.get_float_value(.5)-curve.get_float_value(0))+curve.get_float_value(phase)-curve.get_float_value(0)
loop_samples=[(t,runtime_angle(t)) for t in [.499,.5,.501,.999,1.,1.001]]
assert all(abs(angle-t*720)<.003 for t,angle in loop_samples)
assert unreal.EditorAssetLibrary.save_loaded_asset(curve,False)
report={'passed':True,'curve':curve.get_path_name(),'seconds_per_turn':.5,'degrees_per_second':720,
        'before':before,'after':after,'loop_samples':loop_samples,'blueprints_saved':False,'runtime_code_changed':False}
out=root/'Saved/Drill/UniformSpin';out.mkdir(parents=True,exist_ok=True)
(out/'result.json').write_text(json.dumps(report,indent=2))
print(json.dumps({key:report[key] for key in ['passed','curve','seconds_per_turn','degrees_per_second']}))
