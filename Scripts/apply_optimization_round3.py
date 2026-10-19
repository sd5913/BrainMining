"""Apply saved scene changes for the third packaged-playtest feedback round."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/OptimizationRound3';out.mkdir(parents=True,exist_ok=True)
backup=root/'Saved/Backups'/('OptimizationRound3-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def load_map(path):
    relative=Path('Content')/(path.removeprefix('/Game/')+'.umap')
    (backup/relative).parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/relative,backup/relative)
    assert levels.load_level(path)
load_map('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
stage=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.CardRoomStage))
stage.set_editor_property('opponent_base_local',unreal.MathLibrary.inverse_transform_location(stage.get_actor_transform(),unreal.Vector(15100,3600,1750)))
assert levels.save_current_level()
load_map('/Game/MainGame/Maps/Lvl_MainGame')
fields=[]
for index,position in enumerate([(-10500,0,7100),(-6300,0,9500)]):
    tag='BM_Round3_CavernDust_'+str(index+1)
    actor=next((a for a in actors.get_all_level_actors() if a.actor_has_tag(tag)),None)
    if not actor:actor=actors.spawn_actor_from_class(unreal.MiningCavernMotes,unreal.Vector(*position))
    actor.set_actor_label('MAIN_GAME_MushroomDust_'+str(index+1));actor.tags=['BM_Campaign_MainGame02',tag]
    actor.set_actor_location(unreal.Vector(*position),False,True)
    actor.half_extent=unreal.Vector(1100,650,1450);actor.particle_count=600
    fields.append(dict(label=actor.get_actor_label(),position=position,extent=[1100,650,1450],count=600))
assert levels.save_current_level()
(out/'authoring.json').write_text(json.dumps(dict(passed=True,backup=str(backup),robot_height=1750,dust_fields=fields),indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
