"""Copy the specified pack's real water systems and their package dependencies."""
import unreal,pathlib,shutil,json
def water_assets():
 root=pathlib.Path(unreal.Paths.project_dir()).resolve();source=root.parent/'Realistic_Starter_VFX_Pack_Niagara';prefix='/Game/Realistic_Starter_VFX_Pack_Niagara'
 registry=unreal.AssetRegistryHelpers.get_asset_registry();copied=[];checked=set()
 options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
 def copy_package(package):
  if not package.startswith(prefix+'/'):return
  relative=package.removeprefix(prefix+'/');src=source/(relative+'.uasset');dst=root/'Content/Realistic_Starter_VFX_Pack_Niagara'/(relative+'.uasset')
  if not dst.exists():
   assert src.exists(),src;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);copied.append(str(dst.relative_to(root)))
  registry.scan_files_synchronous([str(dst)],force_rescan=True)
 pending=[prefix+'/Niagara/Water/'+name for name in ['NS_Water_Leak_1','NS_Water_Leak_2','NS_Water_Leak_3','NS_Water_Explosion','NS_Water_Splash']]
 while pending:
  package=pending.pop()
  if package in checked:continue
  checked.add(package);copy_package(package)
  for dep in registry.get_dependencies(package,options):
   if str(dep).startswith(prefix+'/'):pending.append(str(dep))
 systems=[unreal.load_asset(prefix+'/Niagara/Water/'+name) for name in ['NS_Water_Leak_1','NS_Water_Leak_2','NS_Water_Leak_3','NS_Water_Explosion','NS_Water_Splash']];assert all(systems)
 toilet=unreal.load_asset('/Game/Fab/Toilet/toilet1');assert toilet
 out=root/'Saved/HeavenPreview';out.mkdir(parents=True,exist_ok=True)
 export=unreal.AssetExportTask();export.object=toilet;export.filename=str(out/'toilet_source.fbx');export.exporter=unreal.StaticMeshExporterFBX();export.options=unreal.FbxExportOption();export.automated=True;export.prompt=False;export.replace_identical=True
 assert unreal.Exporter.run_asset_export_task(export)
 report={'systems':[s.get_path_name() for s in systems],'copied':copied,'dependencies':sorted(checked),'toilet_materials':[str(m.material_interface) for m in toilet.get_editor_property('static_materials')]}
 (out/'water_assets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('REAL_WATER_IMPORTED',len(copied),len(checked),'TOILET_EXPORTED')
water_assets()
