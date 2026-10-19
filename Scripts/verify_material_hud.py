"""Render the production material HUD in an unsaved lightweight PIE fixture."""
import json
import time
import traceback
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/MaterialProgression'
OUT.mkdir(parents=True, exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper=unreal.MiningGUIEditorLibrary
state={'phase':'wait','start':time.monotonic(),'next':0,'busy':False}
report={'passed':False,'fixture':'unsaved lightweight scene; production HUD and controller','checks':[]}
def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def shot(name):
    check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'Captured '+name)
def press():
    helper.send_preview_key(state['pc'],'Tab',True)
    helper.send_preview_key(state['pc'],'Tab',False)

def verify_rows(level, amounts):
    gui=state['gui']
    labels=[label for label in unreal.ObjectIterator(unreal.TextBlock)
            if label.get_path_name().startswith(gui.get_path_name())
            and isinstance(label.slot, unreal.CanvasPanelSlot)]
    level_labels=[label for label in labels if label.get_name()=='PickaxeCurrentLevel']
    check(len(level_labels)==1 and str(level_labels[0].get_text())=='等级 '+str(level), 'HUD displays level '+str(level))
    panel=level_labels[0].get_parent()
    rows=sorted([label for label in labels if label.get_parent()==panel and label.get_name().startswith('PickaxeRequirementCount')],
                key=lambda label: label.get_name())
    check(len(rows)==4, 'HUD owns four independent material requirement rows')
    for index,row in enumerate(rows):
        if index>=len(amounts):
            check(not str(row.get_text()), 'Unused material row is empty '+str(index))
            continue
        owned,required=amounts[index]
        check(str(row.get_text()).endswith(f'{owned} / {required}'), 'Material quantities refresh '+str(index))
        color=row.get_editor_property('color_and_opacity').get_editor_property('specified_color')
        green=color.g>.9 and color.r<.5 and color.b<.7
        check(green==(owned>=required), 'Each material turns green independently '+str(index))
    check(not any(str(label.get_text()) in ('矿石背包','矿镐','升级材料') for label in labels),'HUD omits the three redundant headings')
    hint=next(label for label in labels if label.get_name()=='PickaxeTabUpgrade')
    check('材料不足' not in str(hint.get_text()),'Upgrade hint omits insufficient-material tutorial')
    check(str(hint.get_text())==('已满级' if level==4 else '[ Tab ] 升级'),'Upgrade hint uses the concise label')
    panels={w.get_name():w for w in unreal.ObjectIterator(unreal.CanvasPanel) if w.get_path_name().startswith(gui.get_path_name())}
    inventory=panels['MineralInventoryPanel']; pickaxe=panels['PickaxeSummaryPanel']
    check(inventory.slot.get_anchors().minimum==unreal.Vector2D(0,0) and inventory.slot.get_position().x>0,'Inventory anchors to the top left')
    check(pickaxe.slot.get_anchors().minimum==unreal.Vector2D(1,0) and pickaxe.slot.get_position().x<0,'Upgrade anchors to the top right')
    check(inventory.get_editor_property('RenderTransform').shear.y>0 and pickaxe.get_editor_property('RenderTransform').shear.y<0,'Opposite perspective shears point inward')
    check(gui.pickaxe_upgrade.get_editor_property('RenderTransform').scale.x<.7,'Pickaxe disc is smaller')
    images=[image for image in unreal.ObjectIterator(unreal.Image) if image.get_path_name().startswith(gui.get_path_name())]
    for name in ['MineralInventoryBackplate','PickaxeSummaryBackplate']:
        plate=next(image for image in images if image.get_name()==name)
        check(plate.slot.get_z_order()<0 and plate.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE,
              name+' stays behind content without blocking input')
    for index in range(4):
        icon=next(image for image in images if image.get_name()=='PickaxeRequirementIcon'+str(index))
        check((icon.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE)==(index<len(amounts)),
              'Recipe icon follows row visibility '+str(index))
def finish(error=None):
    report.update(passed=error is None,error=error,elapsed=time.monotonic()-state['start'])
    (OUT/'hud.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play()
    state.update(phase='done',next=time.monotonic()+2)
def tick(dt):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done':
            if not ed.get_game_world():
                unreal.unregister_slate_post_tick_callback(callback)
                state.clear()
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
                unreal.SystemLibrary.quit_editor()
            return
        assert now-state['start']<120,'HUD fixture timed out'
        world=ed.get_game_world()
        if not world:return
        if state['phase']=='wait':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not pc or not pawn or not pc.mining_gui:return
            if not state.get('hall_created'):
                progress=pawn.get_component_by_class(unreal.MiningProgressionComponent)
                progress.get_wallet().set_editor_property('use_mineral_inventory',True)
                hall=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)[0]
                area=unreal.MiningExperienceArea()
                area.set_editor_property('title','HUD 预览')
                area.set_editor_property('kind',unreal.MiningExperienceKind.MINING)
                area.set_editor_property('entry',unreal.Transform(location=unreal.Vector(0,0,200)))
                hall.set_editor_property('areas',[area])
                state['hall_created']=True
                return
            if not pc.mining_gui.get_editor_property('bMineralProgression'):return
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            progress=pawn.get_component_by_class(unreal.MiningProgressionComponent)
            state.update(pc=pc,gui=pc.mining_gui,progress=progress,wallet=progress.get_wallet(),phase='empty',next=now+2)
        elif state['phase']=='empty':
            verify_rows(0,[(0,4),(0,2)])
            shot('hud_01_empty')
            state['wallet'].add_mineral('2',4)
            state.update(phase='partial',next=now+.5)
        elif state['phase']=='partial':
            verify_rows(0,[(4,4),(0,2)])
            shot('hud_01_partial')
            press()
            check(state['progress'].get_level(0)==0, 'Partial recipe cannot upgrade')
            state['wallet'].add_mineral('4',2)
            state.update(phase='ready',next=now+.5)
        elif state['phase']=='ready':
            verify_rows(0,[(4,4),(2,2)])
            shot('hud_02_ready')
            # Route through Slate/game viewport just like a physical keyboard event.
            helper.send_preview_focused_key(state['pc'],'Tab',True)
            state.update(phase='focused',next=now+.3)
        elif state['phase']=='focused':
            helper.send_preview_focused_key(state['pc'],'Tab',False)
            check(state['progress'].get_level(0)==1,'Focused Slate Tab input buys exactly one level')
            verify_rows(1,[(0,6),(0,4)])
            shot('hud_02_spent')
            for mineral,count in [('2',24),('4',18),('6',6),('9',2)]:state['wallet'].add_mineral(mineral,count)
            press();press()
            state.update(phase='four_materials',next=now+.8)
        elif state['phase']=='four_materials':
            check(state['progress'].get_level(0)==3,'Level three displays four material requirements')
            verify_rows(3,[(10,10),(8,8),(4,4),(2,2)])
            shot('hud_03_four_materials');press()
            state.update(phase='max',next=now+.8)
        elif state['phase']=='max':
            check(state['progress'].get_level(0)==4,'Rendered HUD reaches maximum level')
            verify_rows(4,[])
            shot('hud_04_max');finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False

world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningExhibitGameMode)
collection=actors.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector())
collection.set_editor_property('area_managed',True)
collection.set_editor_property('use_authored_placements',True)
collection.set_editor_property('show_exploration_hud',False)
hall=actors.spawn_actor_from_class(unreal.MiningExhibitHall,unreal.Vector())
hall.set_editor_property('linear_campaign',True)
# Leave areas empty until the PIE callback has selected the material economy.
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
