"""Illuminate only the enclosed upper approach, above the unchanged dark echo maze."""
import unreal,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',root/'Saved/Backups'/('FeedbackEntranceLight_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap'))
aa=list(api.get_all_level_actors());echo=next(a for a in aa if isinstance(a,unreal.EchoPassageStage));frame=echo.get_actor_transform()
lamp=next((a for a in aa if a.get_actor_label()=='COL_Echo_UpperApproachLight'),None)
if not lamp:
 lamp=api.spawn_actor_from_class(unreal.PointLight,unreal.MathLibrary.transform_location(frame,unreal.Vector(-250,0,1150)))
 lamp.set_actor_label('COL_Echo_UpperApproachLight');lamp.set_editor_property('tags',['BM_MainGame_Collections','BM_CollectionFeedback','BM_Campaign_MainGameJourney']);lamp.set_folder_path('MainGame/Collections')
 light=lamp.light_component;light.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);light.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);light.set_intensity(180);light.set_light_color(unreal.LinearColor(.85,.9,1));light.set_editor_property('attenuation_radius',700.);light.set_cast_shadows(True)
assert levels.save_current_level()
path=root/'Scripts/capture_collection_feedback_rooms.py';exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'))
