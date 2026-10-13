"""Verify rendered HUD backing and actual upgrade hit-testing after shear."""
import unreal,time,json,traceback
from pathlib import Path
output=Path(unreal.Paths.project_saved_dir())/'Validation/ImmersiveHUD';output.mkdir(parents=True,exist_ok=True)
state={'start':time.monotonic(),'phase':0,'next':0,'checks':[]}
helper=unreal.MiningGUIEditorLibrary
def check(value,label):
    assert value,label
    state['checks'].append(label)
def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(immersive_handle)
    (output/'runtime.json').write_text(json.dumps({'passed':ok,'checks':state['checks'],'error':error},ensure_ascii=False,indent=2),encoding='utf8')
    print('IMMERSIVE_HUD_RUNTIME',ok,len(state['checks']),error)
    unreal.EditorLevelLibrary.editor_end_play()
def tick(dt):
    try:
        now=time.monotonic()
        assert now-state['start']<100,'HUD preview timeout'
        if now<state['next']:return
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if not worlds:return
        world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not pawn:return
        phase=state['phase']
        if phase==0:
            hall=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_class().get_name()=='MiningExhibitHall')
            if not hall.get_editor_property('bReady'):return
            check(hall.enter_area(0,True),'Main mining area opens')
            state.update(phase=1,next=now+2)
        elif phase==1:
            helper.send_preview_key(pc,'V',True);state.update(phase=2,next=now+.6)
        elif phase==2:
            helper.send_preview_key(pc,'V',False)
            helper.capture_preview(pc,str(output/'hud_inset.png'))
            gui=pc.get_editor_property('MiningGUI');progression=pawn.get_component_by_class(unreal.MiningProgressionComponent)
            wallet=progression.get_wallet();wallet.add_coins(5000000)
            pc.set_upgrade_menu_held(True)
            state.update(phase=3,next=now+.8,gui=gui,progression=progression,wallet=wallet,index=0)
        elif phase==3:
            gui=state['gui'];images={w.get_name():w for w in unreal.ObjectIterator(unreal.Image) if w.get_outer().get_outer()==gui}
            for name in ['UpgradeBackplate']:
                image=images[name]
                check(image.slot.get_z_order()<0,name+' renders behind icons')
                check(image.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE,name+' cannot block pointer')
            for name in ['AbilityBackplate','CurrencyBackplate']:
                check(name not in images or images[name].get_parent() is None,'Ordinary HUD has no '+name)
            parent=images['UpgradeBackplate'].get_parent()
            check(parent==gui.upgrade_panel,'Upgrade plate is inside the animated popup')
            check(gui.upgrade_panel.slot.get_size().x==724,'Backdrop expands with the fourth upgrade card')
            for ability in [gui.ability_g,gui.ability_v]:
                material=ability.ability_icon.get_dynamic_material()
                check(material is not None,'Existing ability art keeps its rim material')
                ability.set_used(True)
                check(material.get_texture_parameter_value('Artwork')==ability.used_texture,'Used icon retains white rim and original artwork')
                ability.set_used(False)
                check(material.get_texture_parameter_value('Artwork')==ability.normal_texture,'Normal icon restores through the same material')
            helper.capture_preview(pc,str(output/'upgrade_backplate.png'))
            check(not gui.blast_upgrade.coin_button.get_is_enabled(),'Original story-locked bomb upgrade stays disabled')
            check(pawn.get_component_by_class(unreal.MiningBombToolComponent).unlock_skill(),'Unlock bomb in temporary test session through the gameplay API')
            state.update(phase=4,next=now+.4)
        elif phase==4:
            gui=state['gui'];cards=[gui.pickaxe_upgrade,gui.vortex_upgrade,gui.blast_upgrade,gui.drill_upgrade]
            index=state['index'];button=cards[index].coin_button
            state['before']=state['wallet'].balance;state['price']=state['progression'].get_next_price(index)
            check(helper.simulate_preview_button(button,True),'Skewed upgrade '+str(index)+' receives pointer press')
            check(helper.simulate_preview_button(button,False),'Skewed upgrade '+str(index)+' receives actual click')
            state.update(phase=5,next=now+.3)
        elif phase==5:
            index=state['index']
            check(state['progression'].get_level(index)==1,'Upgrade '+str(index)+' advances once')
            check(state['wallet'].balance==state['before']-state['price'],'Upgrade '+str(index)+' charges once')
            state['index']+=1
            if state['index']<4:state.update(phase=4,next=now+.1)
            else:
                pc.set_upgrade_menu_held(False);state.update(phase=6,next=now+.8)
        elif phase==6:
            check(not state['gui'].get_editor_property('bUpgradePanelOpen'),'Popup and its backing close together')
            check(not pc.show_mouse_cursor and not pc.is_look_input_ignored(),'Pointer and camera control restore')
            finish(True)
    except Exception:
        finish(False,traceback.format_exc())
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Showcase/Maps/Lvl_MainExperiences')
immersive_handle=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
