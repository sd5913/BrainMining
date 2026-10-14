import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();data={}
reference=json.loads((ROOT/'Saved/Validation/main_game_fulljourney_reference.json').read_text())
for name,path in [('source',reference['player']),('main','/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C')]:
    obj=unreal.get_default_object(unreal.load_class(None,path));values={}
    for k in ['jump_action','move_action','look_action','mouse_look_action']:
        v=obj.get_editor_property(k);values[k]=v.get_path_name() if v else None
    data[name]=values
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
    move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
    data['runtime']=dict(position=list(pawn.get_actor_location().to_tuple()),flight_input_held=move.get_editor_property('bFlightInputHeld'),
        mode=str(move.movement_mode),velocity=list(move.velocity.to_tuple()),creative=move.get_editor_property('bCreativeFlight'),
        upgrade_held=pc.get_editor_property('bUpgradeMenuHeld'),move_ignored=pc.is_move_input_ignored(),look_ignored=pc.is_look_input_ignored())
(ROOT/'Saved/Validation/main_game_player_input.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
