import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Polish29';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
aa=list(api.get_all_level_actors());report={'actors':[],'collections':[],'minerals':[]}
def path(o):return o.get_path_name() if o else None
def v(x):return list(x.to_tuple())
for a in aa:
 if isinstance(a,unreal.MiningCollectionField):
  for p in a.authored_placements:report['collections'].append({'field':a.get_name(),'id':p.item_id,'transform':p.transform.export_text(),'clue':p.clue_transform.export_text(),'approach':p.approach_transform.export_text(),'host':path(p.host_rock),'presentation':path(p.presentation_actor)})
 if isinstance(a,unreal.MiningTreasureField):
  for i,p in enumerate(a.get_editor_property('authored_placements')):
   if str(p.area_tag)=='BM_Campaign_MainGame02':report['minerals'].append({'index':i,'id':str(p.treasure_id),'transform':p.transform.export_text(),'surface':p.surface.export_text(),'hosts':[path(h) for h in p.hosts],'fragments':[t.export_text() for t in p.fragments]})
 label=a.get_actor_label()
 if any(t in label.lower() for t in ['shell','bedrock','mushroom','parkour','region']):
  comps=a.get_components_by_class(unreal.MeshComponent)
  report['actors'].append({'name':a.get_name(),'label':label,'tags':[str(t) for t in a.tags],'pos':v(a.get_actor_location()),'bounds':[v(x) for x in a.get_actor_bounds(False)],'materials':list(set(path(c.get_material(i)) for c in comps for i in range(c.get_num_materials())))})
for name in ['T_AbilityG','T_IconBlast']:
 tex=unreal.load_asset('/Game/UI/MiningGUI/Art/'+name)
 task=unreal.AssetExportTask();task.object=tex;task.filename=str(out/(name+'.png'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.TextureExporterPNG();unreal.Exporter.run_asset_export_task(task)
(out/'scene-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
