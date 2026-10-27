import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
report_path = root / 'Saved/Validation/LocalizationAchievements/achievement-collectibles.json'
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before auditing the release map'
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
fields = unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), unreal.MiningCollectionField)
placements = []
for field in fields:
    if not field.get_editor_property('use_authored_placements'):
        continue
    for placement in field.get_editor_property('authored_placements'):
        placements.append({'id': placement.item_id, 'area': str(placement.area_tag),
                           'host': placement.host_rock.get_path_name() if placement.host_rock else None,
                           'position': list(placement.transform.translation.to_tuple()), 'field': field.get_path_name()})
source = (root / 'Source/BrainMining/MiningAchievementState.cpp').read_text(encoding='utf-8')
required = set(map(int, re.search(r'TSet<int32> Required = \{([\d,]+)\}', source).group(1).split(',')))
mineral_rewards = {12: 25, 13: 15, 14: 10, 15: 5}
placed_ids = {entry['id'] for entry in placements}
escape_rooms = unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), unreal.MiningRoomEscape)
escape_source = (root / 'Source/BrainMining/MiningRoomEscapeStage.cpp').read_text(encoding='utf-8')
runtime_rewards = {11: 'MiningRoomEscape: exit celebration ribbon'} if escape_rooms and 'StageRibbon->BuildModel(11)' in escape_source and 'RegisterExternalCollectible(StageRibbon)' in escape_source else {}
missing = sorted(required - placed_ids - set(mineral_rewards) - set(runtime_rewards))
report = {'passed': not missing, 'required_ids': sorted(required), 'placed_ids': sorted(placed_ids),
          'mineral_reward_thresholds': mineral_rewards, 'runtime_rewards': runtime_rewards,
          'escape_rooms': [actor.get_path_name() for actor in escape_rooms], 'placements': placements, 'missing_ids': missing,
          'scope': 'Saved campaign placements and mineral reward rules; traversal and mining yields require playtesting.'}
report_path.parent.mkdir(parents=True, exist_ok=True)
report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
assert not missing, 'Release achievement includes unplaced collectibles: ' + str(missing)
unreal.log('ACHIEVEMENT_COLLECTION_AUDIT_PASSED ' + str(len(required)) + ' ids')
