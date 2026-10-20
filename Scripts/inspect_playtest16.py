import unreal,json
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Playtest16';out.mkdir(parents=True,exist_ok=True)
rows=[]
for a in A.get_all_level_actors():
 p=a.get_actor_location(); label=a.get_actor_label()
 if p.x < -30000 or 32500<p.x<38000 or 'Office' in label or 'Mushroom' in label or isinstance(a,(unreal.CardRoomStage,unreal.MiningExhibitHall)):
  row=dict(name=a.get_name(),label=label,cls=a.get_class().get_name(),pos=p.to_tuple(),rot=str(a.get_actor_rotation()),scale=a.get_actor_scale3d().to_tuple(),tags=[str(t) for t in a.tags],bounds=str(a.get_actor_bounds(False)))
  mesh=a.get_component_by_class(unreal.StaticMeshComponent)
  if mesh and mesh.static_mesh:row['mesh']=mesh.static_mesh.get_path_name()
  if isinstance(a,unreal.MiningExhibitHall):row['areas']=[dict(tag=str(x.tag),entry=str(x.entry),exit=str(x.exit)) for x in a.areas]
  if isinstance(a,unreal.CardRoomStage):row['reward']=str(a.reward_local)
  rows.append(row)
(out/'actors.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
widgets={}
for path in ['/Game/CollectionPrototype/UI/WBP_CollectionInspect','/Game/EndingEmergence/UI/WBP_EndingCredits']:
 bp=unreal.load_asset(path);ws=[]
 for w in unreal.ObjectIterator(unreal.Widget):
  if not w.get_outer() or w.get_outer().get_outer()!=bp:continue
  row={'name':w.get_name(),'class':w.get_class().get_name()}
  if isinstance(w,unreal.TextBlock):row['text']=str(w.get_text())
  if isinstance(w.slot,unreal.CanvasPanelSlot):row['offset']=str(w.slot.get_offsets());row['anchors']=str(w.slot.get_anchors())
  ws.append(row)
 widgets[path]=ws
(out/'widgets.json').write_text(json.dumps(widgets,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
