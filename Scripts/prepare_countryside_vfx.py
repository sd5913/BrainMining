"""Import only the Niagara effects and transitive dependencies needed by P2."""
import unreal, shutil, json
from pathlib import Path
root=Path(unreal.Paths.project_dir());source=Path('E:/涌现taptap2026/Realistic_Starter_VFX_Pack_Niagara')
prefix='/Game/Realistic_Starter_VFX_Pack_Niagara/'
ar=unreal.AssetRegistryHelpers.get_asset_registry();opts=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
chosen={'ArcImpact':'Impact/NS_Impact_Electric','Poison':'Explosion/NS_Explosion_Color_Smoke','Ignition':'Fire/NS_Fire_Burst','Finale':'Explosion/NS_Explosion_Big_4','Spark':'Sparks/NS_Sparks_04','Confetti':'Explosion/NS_Explosion_Confetti','Muzzle':'MuzzleFlash/NS_MuzzleFlash_02'}
todo=[prefix+'Niagara/'+x for x in chosen.values()];done=set();copied=[]
while todo:
 p=todo.pop()
 if p in done:continue
 rel=p[len(prefix):]+'.uasset';src=source/rel;dst=root/'Content/Realistic_Starter_VFX_Pack_Niagara'/rel
 assert src.exists(),src
 dst.parent.mkdir(parents=True,exist_ok=True)
 if not dst.exists():shutil.copy2(src,dst);copied.append(rel)
 ar.scan_files_synchronous([str(dst)],force_rescan=True);done.add(p)
 todo.extend(str(x) for x in ar.get_dependencies(p,opts) if str(x).startswith(prefix))
out={};assets=[]
for k,rel in chosen.items():
 path='/Game/Passages/Echo/ArtP2/VFX/NS_P2_'+k
 s=unreal.load_asset(path)
 if not s:s=unreal.EditorAssetLibrary.duplicate_asset(prefix+'Niagara/'+rel,path)
 assert s,k
 unreal.EditorAssetLibrary.save_loaded_asset(s);out[k]=s.get_path_name();assets.append(s)
folder=root/'Saved/Automation/CountrysideP2';(folder/'vfx_import.json').write_text(json.dumps(dict(systems=out,dependencies=len(done),copied=copied),indent=2),encoding='utf-8')
unreal.log('P2_NIAGARA_IMPORTED '+str(len(done)))
