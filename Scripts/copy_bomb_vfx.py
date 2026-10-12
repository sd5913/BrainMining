"""UE Python: copy the chosen explosion's actual package dependencies without overwriting art."""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = Path('E:/涌现taptap2026/Realistic_Starter_VFX_Pack_Niagara')
prefix = '/Game/Realistic_Starter_VFX_Pack_Niagara/'
registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
todo = [prefix + 'Niagara/Explosion/NS_Explosion_Big_2']
done, copied, preserved = set(), [], []
while todo:
    package = todo.pop()
    if package in done: continue
    name = package[len(prefix):]
    file = source / (name + '.uasset')
    assert file.exists(), 'Missing effect dependency: ' + package
    destination = root / 'Content/Realistic_Starter_VFX_Pack_Niagara' / (name + '.uasset')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        if hashlib.sha256(destination.read_bytes()).digest() != hashlib.sha256(file.read_bytes()).digest():
            # A current-engine re-save or an artist edit must survive a rerun.
            preserved.append(name)
    else:
        shutil.copy2(file, destination)
        copied.append(name)
    registry.scan_files_synchronous([str(destination)], force_rescan=True)
    done.add(package)
    todo.extend(str(dep) for dep in registry.get_dependencies(package, options) if str(dep).startswith(prefix))
report = {'assets': sorted(done), 'copied': copied, 'preserved_modified': preserved,
          'bytes': sum((source/(x[len(prefix):]+'.uasset')).stat().st_size for x in done)}
out = root/'Saved/Bomb'
out.mkdir(parents=True, exist_ok=True)
(out/'vfx-dependencies.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log(json.dumps({'assets': len(done), 'copied': len(copied), 'preserved_modified': len(preserved), 'bytes': report['bytes']}))
