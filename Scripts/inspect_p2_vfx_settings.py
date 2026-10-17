import unreal,json
from pathlib import Path
names=['fx.Niagara.QualityLevel','fx.NiagaraAllowGPUParticles','r.GPUCrashDebugging','sg.EffectsQuality']
Path(unreal.Paths.project_dir(),'Saved/Automation/CountrysideP2/vfx-cvars.json').write_text(json.dumps({n:unreal.SystemLibrary.get_console_variable_int_value(n) for n in names}),encoding='utf-8')
