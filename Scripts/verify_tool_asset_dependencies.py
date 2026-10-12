"""UE commandlet/editor pre-push audit: tool assets must have complete tracked dependencies.

Stage repaired assets first. Raises if any /Game package in the tool dependency
closure is missing, is still a Git LFS pointer in the checkout, or is untracked.
"""
import json,subprocess
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
registry=unreal.AssetRegistryHelpers.get_asset_registry()
options=unreal.AssetRegistryDependencyOptions()
options.include_hard_package_references=True
options.include_soft_package_references=True
roots=['/Game/Drill/Stylized/SM_DrillStylized','/Game/Drill/Stylized/M_DrillStylized',
       '/Game/Drill/CF_DrillSpin','/Game/Bomb/SM_StickyBomb',
       '/Game/UI/MiningGUI/WBP_MiningHUD']
pending=list(roots);seen=set();failures=[]
while pending:
    package=pending.pop()
    if package in seen or not package.startswith('/Game/'):continue
    seen.add(package)
    file=root/'Content'/(package[len('/Game/'):]+'.uasset')
    if not file.exists():
        map_file=file.with_suffix('.umap')
        if map_file.exists():file=map_file
        else:
            failures.append('Missing package: '+package)
            continue
    if file.read_bytes()[:40].startswith(b'version https://git-lfs.github.com'):
        failures.append('Unhydrated LFS file: '+package)
    tracked=subprocess.run(['git','ls-files','--error-unmatch','--',str(file.relative_to(root)).replace('\\','/')],
        cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    if tracked.returncode:failures.append('Untracked dependency: '+package)
    pending.extend(str(dep) for dep in registry.get_dependencies(package,options))
material=unreal.load_asset('/Game/Drill/Stylized/M_DrillStylized')
bindings={}
for expression in unreal.ObjectIterator(unreal.MaterialExpressionTextureSampleParameter2D):
    if expression.get_outer()!=material:continue
    texture=expression.get_editor_property('texture')
    bindings[str(expression.get_editor_property('parameter_name'))]=texture.get_path_name() if texture else None
used=[path for path in bindings.values() if path]
for channel in ['BaseColor','Roughness','Metallic','Normal']:
    if not any('T_DrillStylized_'+channel+'.' in path for path in used):failures.append('Unbound drill channel: '+channel)
report={'passed':not failures,'packages_checked':len(seen),'packages':sorted(seen),'failures':failures,'drill_textures':used,'material_bindings':bindings}
out=root/'Saved/Validation/ToolDependencies';out.mkdir(parents=True,exist_ok=True)
(out/'result.json').write_text(json.dumps(report,indent=2))
assert not failures,'\n'.join(failures)
print(json.dumps({'passed':True,'packages_checked':len(seen),'drill_textures':used}))
