"""Check the actual collection widget, rich-text styles and all 12 ID bindings in PIE.

Uses an unsaved fixture, restores the original map, and leaves player saves alone.
"""
import json, re, time, traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/Validation/CollectionEmergence'
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not ed.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
original = ed.get_editor_world().get_path_name().split('.')[0]
source = (root/'Source/BrainMining/MiningCollectible.cpp').read_text(encoding='utf-8-sig')
body = source.split('const TCHAR* Descriptions[]={', 1)[1].split('};', 1)[0]
quotes = source.split('const TCHAR* Quotes[]={', 1)[1].split('};', 1)[0]
expected_a = re.findall(r'TEXT\("(.*?)"\)', body)
expected_b = re.findall(r'TEXT\("(.*?)"\)', quotes)
assert len(expected_a) == len(expected_b) == 12
report = {'passed':False, 'checks':[], 'entries':[]}
state = {'phase':'wait', 'start':time.monotonic(), 'at':0, 'id':0}

def check(value, label):
    assert value, label
    report['checks'].append(label)

styles = unreal.load_asset('/Game/CollectionPrototype/UI/DT_CollectionDescriptionStyles')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(styles))
for row in rows:
    font = row['TextStyle']['Font']
    check('F_SourceHanSansCN_Medium' in font['FontObject'], row['Name']+' uses project font')
    check(abs(font['SkewAmount']-(.18 if row['Name']=='Quote' else 0))<.001, row['Name']+' font skew')

def finish(error=None):
    report['error'] = error
    report['passed'] = error is None and len(report['entries']) == 12
    (out/'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    levels.editor_request_end_play()
    state.update(phase='restore', at=time.monotonic())

def tick(dt):
    try:
        now = time.monotonic()
        if state['phase']=='restore':
            if not ed.get_game_world() and now-state['at']>1:
                unreal.unregister_slate_post_tick_callback(handle)
                levels.load_level(original)
                unreal.log('COLLECTION_EMERGENCE_VERIFIED '+str(report['passed']))
            return
        if now-state['start']>100:
            finish('Timed out'); return
        world = ed.get_game_world()
        if not world: return
        if state['phase']=='wait':
            fields = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningCollectionField)
            if not fields: return
            field = fields[0]
            widget = field.get_editor_property('catalogue_widget')
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not widget or not pc: return
            state.update(field=field, widget=widget, pc=pc)
            assert unreal.MiningGUIEditorLibrary.write_preview_state(field, 'Inventory', '('+','.join(['1']*12)+')')
            field.toggle_catalogue()
            widget.select_collection_item(2)
            unreal.MiningGUIEditorLibrary.resize_preview_viewport(pc, 1600, 900)
            state.update(phase='inspect', at=now)
            return
        field, widget, pc = state['field'], state['widget'], state['pc']
        control = lambda name: unreal.CollectionUMGEditorLibrary.find_inspect_control(widget, name)
        if state['phase']=='inspect' and now-state['at']>.8:
            text = control('ItemDescription')
            check(isinstance(text, unreal.RichTextBlock), 'Uses designer RichTextBlock')
            check(isinstance(control('DescriptionScroll'), unreal.ScrollBox), 'Long descriptions have a scroll area')
            check(control('ResponsiveScale').get_class()==unreal.ScaleBox.static_class(), 'Responsive designer scale retained')
            for item_id in range(12):
                widget.select_collection_item(item_id)
                expected = expected_a[item_id]+'\n\n<Quote>“'+expected_b[item_id]+'”</>'
                actual = str(text.get_text())
                check(actual==expected, 'Correct A, blank line and italic quote for ID '+str(item_id))
                name = str(control('ItemName').get_text())
                if item_id==10: check(name=='罐子里的萤火虫', 'Existing firefly name retained')
                report['entries'].append({'id':item_id, 'name':name, 'text':actual})
            widget.select_collection_item(2)
            state.update(phase='capture', at=now)
        elif state['phase']=='capture' and now-state['at']>.5:
            check(unreal.MiningGUIEditorLibrary.capture_preview(pc, str(out/'description-1600x900.png')), 'Capture long reagent description')
            unreal.MiningGUIEditorLibrary.resize_preview_viewport(pc, 1280, 720)
            widget.select_collection_item(1)
            state.update(phase='small', at=now)
        elif state['phase']=='small' and now-state['at']>.6:
            check(unreal.MiningGUIEditorLibrary.capture_preview(pc, str(out/'description-1280x720.png')), 'Capture 720p ant description')
            scroll = control('DescriptionScroll')
            scroll.set_scroll_offset(80)
            widget.select_collection_item(5)
            check(scroll.get_scroll_offset()==0, 'Changing item resets description scroll')
            assert unreal.MiningGUIEditorLibrary.write_preview_state(field, 'Inventory', '('+','.join(['0']*12)+')')
            widget.refresh_collection()
            check(str(control('ItemDescription').get_text())=='', 'Locked item has no description')
            finish()
    except Exception:
        finish(traceback.format_exc())

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode', unreal.MiningCollectionGameMode)
field = actors.spawn_actor_from_class(unreal.MiningCollectionField, unreal.Vector())
field.set_editor_property('use_authored_placements', True)
field.set_editor_property('show_exploration_hud', False)
field.set_editor_property('authored_placements', [])
handle = unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
