"""Read-only dependency inventory for the MainGame Windows package."""
import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root / 'Saved/Validation/MainGamePackage'
out.mkdir(parents=True, exist_ok=True)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
config = (root / 'Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
maps = re.findall(r'^\+MapsToCook=\(FilePath="([^"]+)"\)', config, re.M)
directories = re.findall(r'^\+DirectoriesToAlwaysCook=\(Path="([^"]+)"\)', config, re.M)
seeds = set(maps)
for directory in directories:
    assets = registry.get_assets_by_path(directory, recursive=True)
    assert assets, 'Empty runtime cook directory: ' + directory
    seeds.update(str(a.package_name) for a in assets)
options = unreal.AssetRegistryDependencyOptions(
    include_hard_package_references=True, include_soft_package_references=True,
    include_searchable_names=False, include_hard_management_references=False,
    include_soft_management_references=False)
seen = set()
pending = list(seeds)
while pending:
    package = pending.pop()
    if package in seen or not package.startswith(('/Game/', '/Engine/', '/CardRoom/', '/EchoPassage/')):
        continue
    seen.add(package)
    pending.extend(str(p) for p in registry.get_dependencies(package, options))
game_packages = sorted(p for p in seen if p.startswith('/Game/'))
files = []
for package in game_packages:
    base = root / 'Content' / package.removeprefix('/Game/')
    for suffix in ('.uasset', '.umap'):
        file = base.with_suffix(suffix)
        if file.exists():
            files.append((package, file.stat().st_size, suffix))
            break
report = dict(maps=maps, runtime_directories=directories,
              referenced_maps=[p for p, size, suffix in files if suffix == '.umap'],
              game_package_count=len(game_packages), source_bytes=sum(size for p, size, suffix in files),
              largest_packages=sorted(files, key=lambda entry: entry[1], reverse=True)[:25],
              game_packages=game_packages)
(out / 'dependencies.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('MAIN_GAME_PACKAGE_AUDIT ' + json.dumps({k: v for k, v in report.items() if k not in ('game_packages', 'largest_packages')}, ensure_ascii=False))
