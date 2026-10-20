import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback11'
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def v(p):return [p.x,p.y,p.z]
def row(a):
 p,e=a.get_actor_bounds(False)
 return dict(name=a.get_actor_label(),cls=a.get_class().get_name(),pos=v(a.get_actor_location()),bounds=[v(p),v(e)],tags=[str(t) for t in a.tags],parts=[dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else '',materials=[m.get_path_name() if m else '' for m in c.get_materials()],pos=v(c.get_world_location()),scale=v(c.get_world_scale())) for c in a.get_components_by_class(unreal.StaticMeshComponent)])
r={}
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
aa=A.get_all_level_actors()
office=next(a for a in aa if isinstance(a,unreal.PopupCloseStage))
r['office']=[row(a) for a in aa if (a.get_actor_location()-office.get_actor_location()).length()<2400]
r['soil']=[row(a) for a in aa if 'Soil' in a.get_actor_label() or ('Museum' in a.get_actor_label() and isinstance(a,unreal.DiggableRock))]
r['subtitles']={}
for p in unreal.EditorAssetLibrary.list_assets('/Game/Subtitles',recursive=True):
 if 'Office' in p:
  s=unreal.load_asset(p)
  if isinstance(s,unreal.SubtitleSegment):r['subtitles'][p]=[str(x.text) for x in s.lines]
assert L.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
r['cell']=[row(a) for a in A.get_all_level_actors() if a.actor_has_tag('EndingEditableCell')]
r['materials']={}
for name in ['CellNebula','NebulaField','VideoPortal','LivingMembrane']:
 m=unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_'+name)
 r['materials'][name]=[n.get_editor_property('code') for n in unreal.MaterialEditingLibrary.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom)]
(out/'inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
