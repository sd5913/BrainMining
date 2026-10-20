"""Read-only inventory of the production ending's current saved dependencies."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
options=unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True,include_searchable_names=False,include_hard_management_references=False,include_soft_management_references=False)
seen=set();todo=['/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo']
while todo:
 package=todo.pop()
 if package in seen:continue
 seen.add(package)
 todo.extend(str(x) for x in registry.get_dependencies(package,options) if str(x).startswith(('/Game/','/Engine/','/CardRoom/','/EchoPassage/')))
assets=[]
for package in sorted(seen):
 if not package.startswith('/Game/'):continue
 for suffix in ['.uasset','.umap']:
  if (root/'Content'/package.removeprefix('/Game/')).with_suffix(suffix).exists():assets.append(package.removeprefix('/Game/')+suffix);break
out=root/'Saved/Validation/MainGamePackage';out.mkdir(parents=True,exist_ok=True)
(out/'ending-dependencies.json').write_text(json.dumps({'map':'/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo','assets':assets},ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('ENDING_PACKAGE_DEPENDENCIES '+str(len(assets)));unreal.SystemLibrary.quit_editor()
