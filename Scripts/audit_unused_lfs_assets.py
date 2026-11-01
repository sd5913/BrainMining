"""Read-only conservative orphan inventory; runtime cook output is an additional veto."""
import json,re
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/LFSCleanup';out.mkdir(parents=True,exist_ok=True)
inventory=json.loads((root/'Saved/Validation/lfs-current-inventory.json').read_text(encoding='utf-8-sig'))['files']
source=[]
for folder in ['Source','Plugins','Config','Scripts']:
    for p in (root/folder).rglob('*'):
        if p.suffix.lower() in {'.cpp','.h','.cs','.ini','.py','.json','.ps1'} and not any(x in p.parts for x in ['Intermediate','Binaries','Saved']):
            source.append(p.read_text(encoding='utf-8-sig',errors='replace'))
text='\n'.join(source)
prefixes=set(re.findall(r'/Game/[A-Za-z0-9_/]+',text))
cooked=(root/'Saved/Validation/PCSize/PCSize-20261030-TVWall/Optimized/container-files.csv').read_text(encoding='utf-8-sig',errors='replace').replace('\\','/')
registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
classes={str(a.package_name):str(a.asset_class_path.asset_name) for a in registry.get_assets_by_path('/Game',recursive=True)}
options=unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True,include_searchable_names=True,include_hard_management_references=True,include_soft_management_references=True)
candidates=[];orphans=[]
for f in inventory:
    path=Path(f['name'])
    if not f['name'].startswith('Content/') or path.suffix not in ['.uasset','.umap']:continue
    package='/Game/'+path.with_suffix('').as_posix()[len('Content/'):]
    refs=[str(x) for x in registry.get_referencers(package,options) if str(x)!=package]
    if refs:continue
    protected=any(package==prefix or package.startswith(prefix+'/') for prefix in prefixes) or path.stem in text
    in_cook=f['name'][len('Content/'):] in cooked
    entry=dict(path=f['name'],package=package,asset_class=classes.get(package,''),bytes=f['size'],oid=f['oid'],source_reference=protected,in_cook=in_cook)
    orphans.append(entry)
    if not protected and not in_cook:candidates.append(entry)
report=dict(tracked_lfs_bytes=sum(f['size'] for f in inventory),tracked_lfs_files=len(inventory),candidates=sorted(candidates,key=lambda x:x['bytes'],reverse=True),orphans=orphans)
(out/'audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('LFS_ORPHAN_AUDIT candidates=%d bytes=%d'%(len(candidates),sum(x['bytes'] for x in candidates)))
unreal.SystemLibrary.quit_editor()
