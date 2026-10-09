"""Create a combined playthrough entry map; original independent prototypes are preserved."""
import unreal
SOURCE='/Game/Passages/Echo/Maps/Lvl_EchoPassage_P1'
MAP='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1'
DEST='/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1'
lib=unreal.EditorAssetLibrary
if lib.does_asset_exist(MAP):raise RuntimeError('Journey map exists; refusing overwrite')
if not lib.does_asset_exist(DEST):raise RuntimeError('Card room destination is missing')
if not lib.duplicate_asset(SOURCE,MAP):raise RuntimeError('Could not create journey entry map')
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level(MAP):raise RuntimeError('Could not open journey entry')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
stages=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.EchoPassageStage)]
if len(stages)!=1:raise RuntimeError('Expected one Echo passage actor')
stage=stages[0]
stage.set_editor_property('use_maze',True)
stage.set_editor_property('destination_level',unreal.load_asset(DEST))
stage.set_editor_property('passage_id','EchoMazeToCards_P1')
stage.set_actor_label('EchoMaze_To_CardRoom')
# Property changes rerun construction in-editor; explicit reconstruction also refreshes native ISM geometry.
stage.set_actor_transform(stage.get_actor_transform(),False,True)
if not levels.save_current_level():raise RuntimeError('Could not save journey map')
unreal.log('ECHO_CARD_JOURNEY_CREATED: '+MAP+' -> '+DEST)
