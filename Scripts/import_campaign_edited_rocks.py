"""Run in UE after exporting selected terrain with blender_campaign_exchange.py.
Applies to matching existing actors; leaves map unsaved for review/Undo.
"""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Art/Mining/FullJourneyExchange/Edited'
rows=json.loads((root/'edits.json').read_text(encoding='utf-8'))
actors={a.get_path_name():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
for row in rows:
    assert row['actor'] in actors,'Load FullJourney before importing: '+row['actor']
    assert (root/row['file']).is_file()
with unreal.ScopedEditorTransaction('Import Blender campaign terrain'):
    for row in rows:
        assert unreal.MiningDesignerLibrary.import_campaign_rock(actors[row['actor']],str(root/row['file'])), 'Invalid or non-closed terrain: '+row['actor']
unreal.log('Blender terrain applied. Review excavation and mineral coverage before saving the level.')
