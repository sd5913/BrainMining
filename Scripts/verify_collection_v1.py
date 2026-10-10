"""PIE integration test. Uses real pawn sight traces and MiningComponent.TryMine.
Run in a dedicated editor process with -ExecutePythonScript=... . Never saves a map.
"""
import unreal, time, json, traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=Path(unreal.Paths.project_saved_dir())/'Validation'
out.mkdir(parents=True,exist_ok=True)
state={'phase':0,'start':time.monotonic(),'next':0,'checks':[],'item':0,'cuts':0}

def check(name,condition):
    assert condition,name
    state['checks'].append(name)

def aim(item):
    p=item.get_actor_location()
    # Teleport only to set up sight tests; excavation uses the unchanged player component.
    state['pawn'].set_actor_location(unreal.Vector(p.x-260,p.y,p.z+20),False,True)
    eye=state['pawn'].get_actor_eyes_view_point()[0]
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,p))

def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world(): levels.editor_request_end_play()
    report={'passed':ok,'checks':state['checks'],'error':error,'elapsed_seconds':time.monotonic()-state['start']}
    (out/'collection_v01_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('COLLECTION_RUNTIME '+json.dumps(report,ensure_ascii=False))
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    try:
        now=time.monotonic()
        if now-state['start']>240: raise RuntimeError('Collection runtime validation timed out: '+str(state['phase']))
        if now<state['next']: return
        phase=state['phase']
        if phase==0:
            check('Separate level loads',levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V01'))
            levels.editor_request_begin_play(); state['phase']=1; state['next']=now+2
        elif phase==1:
            w=editor.get_game_world()
            if not w: return
            fields=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)
            if not fields or len(fields[0].items)!=5: return
            f=fields[0]; state['f']=f
            state['pc']=unreal.GameplayStatics.get_player_controller(w,0); state['pawn']=unreal.GameplayStatics.get_player_pawn(w,0)
            state['mining']=state['pawn'].get_component_by_class(unreal.MiningComponent)
            state['movement']=state['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent)
            check('Existing pawn has original mining component',state['mining'] is not None)
            check('Existing hold-to-fly movement reused',state['movement'].flight_settings.enabled)
            check('Four unique buried kinds plus exposed tutorial',sorted(i.item_id for i in f.items[:4])==[0,1,2,3] and f.items[4].released)
            check('Buried collectibles begin unreleased',all(not i.released for i in f.items[:4]))
            f.toggle_catalogue(); check('Empty catalogue opens',f.catalogue_open)
            check('Opening catalogue suspends mining ticks',not state['mining'].is_component_tick_enabled())
            f.toggle_catalogue(); check('Closing catalogue resumes mining',not f.catalogue_open and state['mining'].is_component_tick_enabled())
            state['movement'].disable_movement()
            aim(f.items[0]); f.collect_target(); check('Cannot collect through solid rock',sum(f.inventory)==0)
            aim(f.items[4]); state['phase']=2; state['next']=now+.5
        elif phase==2:
            f=state['f']; f.collect_target()
            check('Exposed tutorial collected through real aim trace',sum(f.inventory)==1 and f.items[4].collected)
            f.collect_target(); check('Repeated E cannot duplicate an item',sum(f.inventory)==1)
            check('Collected object hidden with collision off',f.items[4].get_editor_property('hidden') and not f.items[4].get_actor_enable_collision())
            f.toggle_catalogue(); f.select_item(2)
            before=f.preview.get_actor_rotation(); f.rotate_preview(40,25); after=f.preview.get_actor_rotation()
            check('3D preview responds to yaw and pitch',abs(before.yaw-after.yaw)>30 and abs(after.pitch)>20)
            f.rotate_preview(0,1000); check('Preview pitch stays bounded',abs(f.preview.get_actor_rotation().pitch)<=80.1)
            f.reset_preview(); f.toggle_catalogue()
            state['phase']=3; state['next']=now+.4
        elif phase==3:
            f=state['f']; item=f.items[state['item']]
            if item.released:
                state['next']=now+1.2; state['phase']=4; return
            aim(item)
            state['mining'].try_mine(); state['cuts']+=1
            if state['cuts']>30: raise RuntimeError('Failed to uncover '+str(item.item_id)+' exposure='+str(item.exposure))
            state['next']=now+.4
        elif phase==4:
            f=state['f']; item=f.items[state['item']]
            check('Item '+str(item.item_id)+' releases after real excavation',item.released and item.get_editor_property('body').is_simulating_physics())
            check('Item '+str(item.item_id)+' falls and stays above safety floor',item.get_actor_location().z>=40)
            aim(item); state['phase']=5; state['next']=now+.3
        elif phase==5:
            f=state['f']; item=f.items[state['item']]; f.collect_target()
            check('Excavated item '+str(item.item_id)+' can be collected',item.collected)
            state['item']+=1; state['cuts']=0; state['phase']=3 if state['item']<4 else 6
        elif phase==6:
            f=state['f']; check('All four catalogue entries unlocked',all(n>0 for n in f.inventory) and sum(f.inventory)==5)
            f.set_editor_property('random_seed',8128); f.reset_collection()
            a=[(i.item_id,str(i.get_actor_location())) for i in f.items]
            f.reset_collection(); b=[(i.item_id,str(i.get_actor_location())) for i in f.items]
            check('Fixed seed reproduces identical positions and identities',a==b)
            check('Reset clears inventory and restores buried states',sum(f.inventory)==0 and all(not i.released for i in f.items[:4]))
            f.set_editor_property('random_seed',8129); f.reset_collection()
            c=[(i.item_id,str(i.get_actor_location())) for i in f.items]
            check('Different seeds change the layout',a!=c)
            levels.editor_request_end_play(); state['phase']=7; state['next']=now+1
        elif phase==7:
            if editor.get_game_world(): return
            check('Original mining lab still loads',levels.load_level('/Game/MiningPrototype/Maps/Lvl_MiningLab'))
            levels.editor_request_begin_play(); state['phase']=8; state['next']=now+2
        elif phase==8:
            w=editor.get_game_world()
            if not w: return
            pawn=unreal.GameplayStatics.get_player_pawn(w,0); mining=pawn.get_component_by_class(unreal.MiningComponent)
            rocks=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock)
            check('Original demo has no collection field',len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField))==0)
            rock=rocks[0]; center=rock.get_actor_location(); size=rock.rock_size
            check('Original rock still excavates',rock.mine_sphere(unreal.Vector(center.x-size.x*.5,center.y,center.z),120))
            mining.reset_field(); check('Original demo reset still works',rock.successful_cuts==0)
            finish(True)
    except Exception: finish(False,traceback.format_exc())

callback=unreal.register_slate_post_tick_callback(tick)
