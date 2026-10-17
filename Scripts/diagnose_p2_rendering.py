import unreal,json
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=E.get_game_world() or E.get_editor_world()
a=next(x for x in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage))
d={'world':w.get_name(),'scale':list(a.get_actor_scale3d().to_tuple()),'styled':a.get_editor_property('styled_effects'),'spell_scale':a.get_editor_property('spell_presentation_scale'),'opponent_scale':a.get_editor_property('opponent_scale'),'components':[]}
for c in a.get_components_by_class(unreal.StaticMeshComponent):
 if c.get_name() in ['VelvetBody','RoundHead','Nose','BowTie','Laser','SpellOrb','Tail0']:
  d['components'].append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()],visible=c.is_visible(),world=str(c.get_world_location()),scale=str(c.get_world_scale()),relative=str(c.get_relative_transform())))
Path(unreal.Paths.project_dir(),'Saved/Validation/IntroP2Office/render_diagnosis.json').write_text(json.dumps(d,indent=2),encoding='utf8')
