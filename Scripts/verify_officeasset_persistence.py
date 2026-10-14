"""Read-only validation of the saved Close1 art and its imported dependencies."""
import unreal,json,pathlib
def verify_officeasset():
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world.get_path_name().startswith('/Game/PopupClose/Maps/Lvl_PopupClose1.')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();checks=[]
 def check(ok,label):checks.append({'pass':bool(ok),'label':label})
 office=next(a for a in actors if a.get_class().get_name()=='PopupCloseOffice');check(office.get_editor_property('use_imported_architecture'),'native blockout architecture remains disabled')
 art=[a for a in actors if a.actor_has_tag('OfficeAssetClose1')];check(len(art)==136,'136 modular architecture actors retained')
 devices=[a for a in actors if a.get_class().get_name()=='PopupOfficeDevice'];check(len(devices)==4,'four original gameplay devices')
 for d in devices:
  if d.get_editor_property('device_index')<2:
   check(d.get_editor_property('replacement_model').get_name()=='SM_Computer_02a_CRT_CustomScreen','working computer uses CRT')
   check(d.get_editor_property('screen_projection_material').get_name()=='M_CRTScreen','curved screen render-target material persisted')
   s=next(c for c in d.get_components_by_class(unreal.WidgetComponent) if c.get_name()=='DesktopSurface');loc=s.get_editor_property('RelativeLocation');check(abs(loc.z-102.947)<.1 and abs(loc.x+2.607)<.1,'screen aperture pose persisted')
  else:check(d.get_editor_property('replacement_model').get_name()=='SM_Copier','printer model and logic retained')
 check(len([a for a in actors if a.actor_has_tag('OfficeCRTPopupEmitter')])==6,'all six peripheral computers replaced')
 check(not any(isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_DeskMonitor' for a in actors),'old decorative monitors removed')
 check(len([a for a in actors if any(c.static_mesh and c.static_mesh.get_name().startswith('SM_Pillar') for c in a.get_components_by_class(unreal.StaticMeshComponent))])==4,'user pillars retained')
 registry=unreal.AssetRegistryHelpers.get_asset_registry();seen=set();todo=['/Game/PopupClose/Maps/Lvl_PopupClose1'];missing=[]
 while todo:
  p=todo.pop()
  if p in seen or not p.startswith('/Game/'):continue
  seen.add(p)
  if not unreal.EditorAssetLibrary.does_asset_exist(p):missing.append(p);continue
  todo.extend(str(n) for n in registry.get_dependencies(p,unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)))
 # The unrelated imported reference master still records its original absent texture.
 # The repaired Close1 glass must not carry that direct dependency.
 glass_deps=[str(n) for n in registry.get_dependencies('/Game/PopupClose/OfficeArt/Architecture/M_OfficeGlass',unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True))]
 check(not any('/Megascans/' in p for p in glass_deps),'repaired glass has no missing Megascans dependency')
 check(not missing,'saved level dependency closure resolves: '+str(missing))
 report={'passed':all(c['pass'] for c in checks),'checks':checks,'dependencies':len(seen),'missing':missing};path=pathlib.Path(unreal.Paths.project_saved_dir())/'OfficeAsset/persistence.json';path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('OFFICEASSET_PERSISTENCE',json.dumps(report,ensure_ascii=False))
verify_officeasset()
