"""Dedicated PIE validation and rendered captures; never saves runtime excavation.
Run with -ExecutePythonScript, -RenderOffscreen and a GPU (or -nullrhi for checks only).
"""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=Path(unreal.Paths.project_saved_dir())/'Validation/HeavenToiletWater'
out.mkdir(parents=True,exist_ok=True)
s={'phase':0,'start':time.monotonic(),'next':0,'checks':[],'cuts':0,'attempts':0}

def check(label,value):
    assert value,label
    s['checks'].append(label)

def trace(p,q):
    return unreal.SystemLibrary.line_trace_single(s['w'],unreal.Vector(*p),unreal.Vector(*q),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)

def pose(p,target):
    s['pawn'].set_actor_location(unreal.Vector(*p),False,True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],unreal.Vector(*target)))

def shot(name):
    if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
        command='Shot' if '-WaterTemporalCapture' in unreal.SystemLibrary.get_command_line() else 'HighResShot 1600x900'
        unreal.SystemLibrary.execute_console_command(s['w'],command+' filename="'+str(out/name)+'"')

def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if ed.get_game_world():levels.editor_request_end_play()
    report={'passed':ok,'checks':s['checks'],'cuts':s['cuts'],'seconds':time.monotonic()-s['start'],'error':error}
    (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('HEAVEN_TOILET_VALIDATION '+json.dumps(report))
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    try:
        now=time.monotonic()
        if now-s['start']>300:raise RuntimeError('Validation timed out in phase '+str(s['phase']))
        if now<s['next']:return
        p=s['phase']
        if p==0:
            check('Independent level loads',levels.load_level('/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'))
            levels.editor_request_begin_play()
            s.update(phase=1,next=now+12)
        elif p==1:
            w=ed.get_game_world()
            if not w:return
            field=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            if not field.items:return
            s.update(w=w,f=field,pawn=unreal.GameplayStatics.get_player_pawn(w,0),pc=unreal.GameplayStatics.get_player_controller(w,0))
            s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent).disable_movement()
            s['mining']=s['pawn'].get_component_by_class(unreal.MiningComponent)
            check('Exactly one collectible, the original Heaven Toilet',len(field.items)==1 and field.items[0].item_id==0)
            s['item']=field.items[0]
            check('Toilet centered and raised 90cm above floor',s['item'].get_actor_location()==unreal.Vector(2580,0,144))
            check('Toilet released without physics',s['item'].released and not s['item'].get_editor_property('body').is_simulating_physics())
            check('Solid earth blocks entrance before mining',trace((200,-60,130),(1500,-60,130)) is not None)
            check('Three real diggable soil layers',len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock))==3)
            # Interior rays reach the curved walls; no solid sphere fills the room.
            for angle in [0,math.pi/2,math.pi,math.pi*1.5]:
                q=(2580+1600*math.cos(angle),1600*math.sin(angle),700)
                check('Hemisphere wall collision angle %.2f'%angle,trace((2580,0,700),q) is not None)
            check('Dome crown collision at 13.2 metres',trace((2580,0,1100),(2580,0,1600)) is not None)
            check('Room floor supports player',trace((2100,0,100),(2100,0,-100)) is not None)
            check('Interior is hollow',trace((1600,-60,130),(2240,-60,130)) is None)
            s['water']=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HeavenToiletWater)[0]
            water=s['water']
            check('Room radius exactly 60 percent and knee-deep 48cm water',water.radius==1320 and water.water_height==48)
            check('Sealed earth prevents any spill',not water.spilling and not water.spill.is_visible())
            check('Water uses Single Layer Water shading',water.water_material.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
            soil=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock)
            for rock in soil:
                for x in range(420,1460,65):rock.mine_sphere(unreal.Vector(x,0,180),65)
            water.refresh_breach()
            check('Negative test really opens a high passage below the doorway lintel',trace((410,0,180),(1470,0,180)) is None)
            check('Tunnel above waterline does not trigger spill',not water.spilling)
            for rock in soil:rock.reset_rock()
            for rock in soil:rock.mine_sphere(unreal.Vector(1280,0,90),160)
            water.refresh_breach()
            check('Blind pocket reaching the basin does not leak through solid entrance',not water.spilling)
            for rock in soil:rock.reset_rock()
            water.refresh_breach()
            pose((-700,-110,100),(460,50,235))
            s.update(phase=2,next=now+3)
        elif p==2:
            shot('01_entrance.png')
            s.update(phase=20,next=now+1)
        elif p==20:
            # Real MiningComponent call, from reachable positions, advances down the pipe.
            pose((200,-60,100),(460,-60,130))
            s.update(phase=3,next=now+1)
        elif p==3:
            hit=unreal.SystemLibrary.capsule_trace_single(s['w'],unreal.Vector(200,-60,105),unreal.Vector(1580,-60,105),34,88,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
            if hit is not None:
                data=hit.to_tuple()
                impact=data[5]
                check_actor=data[9]
                assert isinstance(check_actor,unreal.DiggableRock),'Blocked by '+str(check_actor)+' at '+str(impact)
                assert s['attempts']<70,'Too many mining attempts: '+str(impact)
                # Sweep the aim around the capsule obstruction, as a player clears
                # remaining edges; do not keep resubmitting an identical tangent cut.
                offsets=[(0,0),(-25,18),(25,-18),(-18,-15),(18,30)]
                dy,dz=offsets[s['attempts']%len(offsets)]
                pose((max(200,impact.x-190),-60,105),(impact.x+8,impact.y+dy,impact.z+25+dz))
                if s['mining'].try_mine():s['cuts']+=1
                unreal.log('HEAVEN_DIG attempt=%d impact=%s cuts=%d'%(s['attempts'],impact,s['cuts']))
                s['attempts']+=1
                s['next']=now+.7
            else:
                check('Original mining component cuts the soil',s['cuts']>=5)
                check('Player capsule can pass excavated soil and dome portal',hit is None)
                s['water'].refresh_breach()
                check('Excavated low passage triggers actual outlet water',s['water'].spilling and s['water'].spill.is_visible())
                check('Flow mesh follows connected air cells',s['water'].connected_cells>50 and s['water'].spill.get_dynamic_mesh().get_triangle_count()>20)
                pose((120,-260,100),(460,-40,40))
                s.update(phase=30,next=now+4)
        elif p==30:
            shot('06_outflow.png')
            s.update(phase=31,next=now+1)
        elif p==31:
            pose((1480,-240,115),(2580,0,450))
            s.update(phase=4,next=now+4)
        elif p==4:
            shot('02_hemisphere_reveal.png')
            s.update(phase=40,next=now+1)
        elif p==40:
            pose((1590,-510,160),(2580,0,490))
            s.update(phase=5,next=now+3)
        elif p==5:
            shot('03_room.png')
            s.update(phase=50,next=now+1)
        elif p==50:
            pose((2290,-70,105),(2580,0,160))
            s.update(phase=6,next=now+3)
        elif p==6:
            check('Water level remains fixed during sustained spill',s['water'].water_height==48 and s['water'].spilling)
            check('Initial gush relaxes into a small steady visual flow',.34<s['water'].spill_strength<.8)
            check('Original E interaction targets the central toilet',s['f'].target==s['item'])
            shot('04_toilet.png')
            s.update(phase=60,next=now+1)
        elif p==60:
            s['f'].collect_target()
            check('Collecting stores exactly one toilet',s['item'].collected and list(s['f'].inventory)==[1,0,0,0])
            s['f'].toggle_catalogue()
            check('Catalogue opens and pauses excavation',s['f'].catalogue_open and not s['mining'].is_component_tick_enabled())
            s['f'].select_item(0)
            s.update(phase=7,next=now+3)
        elif p==7:
            if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
                unreal.SystemLibrary.execute_console_command(s['w'],'Shot SHOWUI filename="'+str(out/'05_catalogue.png')+'" -nosuffix')
            s.update(phase=70,next=now+1)
        elif p==70:
            s['f'].toggle_catalogue()
            check('Closing catalogue restores excavation',s['mining'].is_component_tick_enabled())
            for rock in unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.DiggableRock):rock.reset_rock()
            s['water'].refresh_breach()
            check('Restored soil shuts off the outflow',not s['water'].spilling and not s['water'].spill.is_visible())
            levels.editor_request_end_play()
            s.update(phase=8,next=now+2)
        elif p==8:
            if ed.get_game_world():return
            check('Original collection V03 still loads',levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03'))
            levels.editor_request_begin_play()
            s.update(phase=9,next=now+3)
        elif p==9:
            w=ed.get_game_world()
            if not w:return
            field=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            check('Original collection field still spawns all five items',len(field.items)==5 and not field.use_authored_placements)
            finish(True)
    except Exception:
        finish(False,traceback.format_exc())

callback=unreal.register_slate_post_tick_callback(tick)
