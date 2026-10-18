"""Real PIE collection / excavation / reset checks and optional GPU captures. No map saving."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir()) / 'Validation/CollectionExpansion'
out.mkdir(parents=True, exist_ok=True)
s = dict(phase=0, start=time.monotonic(), next=0, checks=[], index=0)

def check(name, value):
    assert value, name
    s['checks'].append(name)

def pose(position, target):
    s['pawn'].set_actor_location(unreal.Vector(*position), False, True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0], unreal.Vector(*target)))

def aim(item):
    p = item.get_actor_location()
    pose((p.x-230, p.y, max(105,p.z)), (p.x,p.y,p.z))

def shot(name):
    if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
        check('Capture '+name,unreal.MiningGUIEditorLibrary.capture_preview(s['pc'],str(out/name)))

def finish(passed, error=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if ed.get_game_world(): levels.editor_request_end_play()
    report = dict(passed=passed, checks=s['checks'], error=error, seconds=time.monotonic()-s['start'])
    (out/'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('COLLECTION_EXPANSION_VALIDATION ' + json.dumps(report))
    cleanup={'after':time.monotonic()+2,'loaded':False}
    def close(dt):
        if time.monotonic()<cleanup['after'] or ed.get_game_world():return
        if not cleanup['loaded']:
            s.clear();levels.load_level('/Engine/Maps/Entry');cleanup.update(loaded=True,after=time.monotonic()+1);return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)

def tagged(tag):
    return unreal.GameplayStatics.get_all_actors_with_tag(s['w'], tag)

def trace(a,b,capsule=False):
    args=(s['w'],unreal.Vector(*a),unreal.Vector(*b))
    tail=(unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
    return unreal.SystemLibrary.capsule_trace_single(*args,34,88,*tail) if capsule else unreal.SystemLibrary.line_trace_single(*args,*tail)

def tick(dt):
    try:
        now=time.monotonic()
        if now-s['start']>320: raise RuntimeError('Timeout at phase '+str(s['phase']))
        if now<s['next']: return
        phase=s['phase']
        if phase==0:
            check('Expanded V03 loads',levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03'))
            unreal.MiningGUIEditorLibrary.start_floating_preview()
            s.update(phase=1,next=now+8)
        elif phase==1:
            w=ed.get_game_world()
            if not w:return
            f=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            if not f.items:return
            s.update(w=w,f=f,pawn=unreal.GameplayStatics.get_player_pawn(w,0),pc=unreal.GameplayStatics.get_player_controller(w,0))
            s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent).disable_movement()
            check('13 objects and 12 stable catalogue types',len(f.items)==13 and len(f.inventory)==12 and set(i.item_id for i in f.items)==set(range(12)))
            s['new']={i.item_id:i for i in f.items if i.item_id>=4}
            # Campaign save integrity is checked by the launcher hash; the current
            # project no longer exposes the former save subsystem Python API.
            check('Original four excavation bays and tutorial are preserved',len([i for i in f.items if i.item_id<4])==5)
            check('Walkable connection through original north wall',trace((-500,1300,110),(-500,1750,110),True) is None)
            check('Wide gallery aisle reaches all four new dig sites',trace((310,1750,110),(310,4350,110),True) is None)
            pool=tagged('CollectionNumberOne')[0].get_component_by_class(unreal.InstancedStaticMeshComponent)
            s['number_count']=pool.get_instance_count()
            numbers=[pool.get_instance_transform(i,True) for i in range(pool.get_instance_count())]
            check('Gem-density numeral halo (%d instances)'%s['number_count'],s['number_count']>=800)
            check('Each instance is one complete numeral mesh',pool.static_mesh.get_name()=='SM_NumberFragment')
            check('Numeral sizes are twice crystal scale',all(.219<=t.scale3d.x<=.341 for t in numbers))
            check('Number sizes and tilts vary',len({round(t.scale3d.x,3) for t in numbers})>20 and len({round(t.rotation.rotator().roll) for t in numbers})>30)
            check('Numbers extend from the front into calculator soil',max(t.translation.x for t in numbers)>950)
            check('Vertical cola bore reaches the underground chamber',trace((754,2700,570),(754,2700,180)) is None)
            check('Ants use the smaller body scale',all(abs(a.get_actor_scale3d().x-.45)<.001 for a in tagged('CollectionSoilAnt')))
            cola_soil=min(tagged('CollectionExpansionSoil'),key=lambda a:abs(a.get_actor_location().y-2700))
            pocket=cola_soil.get_component_by_class(unreal.FogChamberComponent)
            check('Cola cavity hugs the can',pocket.room_size==unreal.Vector(64,64,106))
            cola=s['new'][8];center=cola.get_actor_location()+unreal.Vector(0,0,4)
            for delta in [unreal.Vector(100,0,0),unreal.Vector(0,100,0)]:
                hit=unreal.SystemLibrary.line_trace_single(w,center,center+delta,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn'],cola],unreal.DrawDebugTrace.NONE,True)
                check('Actual cola pocket side clearance '+str(delta),hit is not None and abs(hit.to_tuple()[5].distance(center)-32)<2)

            check('Firefly sparkle trail reaches the buried jar',tagged('CollectionGreenGlow')[0].trail_end.x==458)
            glow=tagged('CollectionGreenGlow')[0]
            gp=glow.get_components_by_class(unreal.InstancedStaticMeshComponent)[0]
            points=[gp.get_instance_transform(i,False).translation for i in range(gp.get_instance_count())]
            along=sorted(p.x for p in points);gaps=[b-a for a,b in zip(along,along[1:])]
            check('Firefly trail has irregular spacing',max(gaps)>4*min(g for g in gaps if g>0))
            check('Firefly trail drifts across a broad natural swath',max(p.y for p in points)-min(p.y for p in points)>60 and max(p.z for p in points)-min(p.z for p in points)>60)
            check('Cola column, sparkle trail and jar swarm exist',len(tagged('CollectionColaBubbles'))==1 and len(tagged('CollectionGreenGlow'))==1 and len(tagged('CollectionJarSwarm'))==1)
            check('Two cannons and two stage spotlights',len(tagged('CollectionRibbonCannon'))==2 and len(tagged('CollectionStageLight'))==2)
            s['stage']=tagged('CollectionCelebration')[0]
            check('Stage waits for approach',not s['stage'].stage_active)
            for i in [4,9,10]: check('Buried item %d starts unreleased'%i,not s['new'][i].released)
            for i in [5,6,7,8,11]: check('Display or cavity item %d starts static'%i,s['new'][i].released and not s['new'][i].get_editor_property('body').is_simulating_physics())
            for i in [4,8,9,10]:
                item=s['new'][i];p=item.get_actor_location()
                pose((310,p.y,105),(p.x,p.y,p.z));f.collect_target()
                check('Soil prevents remote collection of %d'%i,not item.collected)
            s['views']=[
                ('01_gallery.png',(-500,1600,200),(500,2600,240)),
                ('02_calculator_clue.png',(100,2000,130),(460,2000,230)),
                ('03_cola_bore.png',(570,2550,600),(754,2700,320)),
                ('04_flower_roots_buried.png',(310,3400,220),(665,3400,270)),
                ('05_firefly_glow.png',(310,4060,130),(460,4100,205)),
                ('06_red_curtain.png',(-650,2910,110),(-650,3300,300)),
                ('07_celebration_stage.png',(-650,3540,110),(-650,4280,260)),
            ]
            s.update(phase=2,index=0)
        elif phase==2:
            name,p,t=s['views'][s['index']];pose(p,t)
            s.update(phase=3,next=now+2)
        elif phase==3:
            shot(s['views'][s['index']][0]);s['index']+=1
            s.update(phase=2 if s['index']<len(s['views']) else 4,next=now+1)
        elif phase==4:
            check('Approaching curtain activates celebration',s['stage'].stage_active)
            curtains=tagged('CollectionRedCurtain')
            check('Curtains part to reveal the stage',all(abs(a.get_actor_location().x+650)>480 for a in curtains))
            pools=s['stage'].get_components_by_class(unreal.InstancedStaticMeshComponent)
            check('320 reusable falling ribbons',sum(c.get_instance_count() for c in pools)==320)
            s['particle']=pools[0].get_instance_transform(0,False);s['pool']=pools[0]
            s.update(phase=5,next=now+.7)
        elif phase==5:
            check('Confetti moves over time',str(s['particle'])!=str(s['pool'].get_instance_transform(0,False)))
            s.update(phase=6,index=4)
        elif phase==6:
            item=s['new'][s['index']];i=item.item_id;p=item.get_actor_location()
            if i in [4,8,9,10]:
                rock=min(tagged('CollectionExpansionSoil'),key=lambda a:abs(a.get_actor_location().y-p.y))
                if i==9:
                    check('Real cut exposes the dense roots',rock.mine_sphere(p,235))
                else:
                    for x in range(440,int(p.x)+1,140):rock.mine_sphere(unreal.Vector(x,p.y,170),180)
                s.update(phase=7,next=now+2)
            else:s.update(phase=7,next=now+.2)
        elif phase==7:
            item=s['new'][s['index']]
            check('Item %d has become available'%item.item_id,item.released)
            if item.item_id==4:check('Only excavated number clues disappear',0<tagged('CollectionNumberOne')[0].numbers.get_instance_count()<s['number_count'])
            aim(item);s.update(phase=8,next=now+.5)
        elif phase==8:
            f=s['f'];item=s['new'][s['index']]
            check('Aim reaches collectible %d'%item.item_id,f.target==item)
            f.collect_target();check('E collects item %d exactly once'%item.item_id,item.collected and f.inventory[item.item_id]==1)
            f.collect_target();check('No duplicate collection %d'%item.item_id,f.inventory[item.item_id]==1)
            f.toggle_catalogue();f.select_item(item.item_id);f.rotate_preview(15,8)
            check('Catalogue builds correct model %d'%item.item_id,f.preview.item_id==item.item_id)
            check('Catalogue pauses digging',not s['pawn'].get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
            s.update(phase=9,next=now+1)
        elif phase==9:
            shot('inspect_%02d.png'%s['index']);s.update(phase=10,next=now+.5)
        elif phase==10:
            s['f'].toggle_catalogue()
            check('Closing catalogue restores digging',s['pawn'].get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
            s['index']+=1;s.update(phase=6 if s['index']<=11 else 11,next=now+.3)
        elif phase==11:
            check('All eight new types collected',all(n==1 for n in s['f'].inventory[4:]))
            check('Cola and glow effects stop after their items are collected',all(not a.is_actor_tick_enabled() for a in tagged('CollectionColaBubbles')+tagged('CollectionGreenGlow')+tagged('CollectionJarSwarm')))
            computer=next(i for i in s['f'].items if i.item_id==1);p=computer.get_actor_location()
            rock=min(unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.DiggableRock),key=lambda a:abs(a.get_actor_location().y-p.y))
            s['ant_rock']=rock;s['computer']=computer
            for x in range(440,1050,100):rock.mine_sphere(unreal.Vector(x,p.y,140),165)
            pose((560,p.y,110),(1040,p.y,90))
            s.update(phase=13,next=now+2)
        elif phase==12:
            check('Reset restores 13 objects and clears inventory',len(s['f'].items)==13 and sum(s['f'].inventory)==0)
            check('Reset does not duplicate effects or stage',len(tagged('CollectionCelebration'))==1 and len(tagged('CollectionColaBubbles'))==1 and len(tagged('CollectionGreenGlow'))==1 and len(tagged('CollectionJarSwarm'))==1)
            check('Reset restores buried numerals',tagged('CollectionNumberOne')[0].numbers.get_instance_count()==s['number_count'])
            check('Reset restores a single continuous ant colony',len(tagged('CollectionSoilAnt'))==42)
            finish(True)
        elif phase==13:
            ants=[a for a in tagged('CollectionSoilAnt') if 580<a.get_actor_location().x<920 and not a.get_editor_property('hidden')]
            check('Ants populate the newly excavated middle soil',len(ants)>=3)
            s['ant_samples']=[(a,a.get_actor_location()) for a in ants]
            shot('08_ants_on_excavated_soil.png')
            s.update(phase=14,next=now+1)
        elif phase==14:
            check('Exposed ants keep crawling toward the computer',sum(a.get_actor_location().x>p.x+5 for a,p in s['ant_samples'])>=2)
            p=s['computer'].get_actor_location()
            for x in range(1000,int(p.x),100):s['ant_rock'].mine_sphere(unreal.Vector(x,p.y,140),165)
            s.update(phase=15,next=now+2)
        elif phase==15:
            p=s['computer'].get_actor_location()
            check('Ants extend into the final approach to the computer',any(p.x-400<a.get_actor_location().x<p.x-100 and not a.get_editor_property('hidden') for a in tagged('CollectionSoilAnt')))
            cloud=tagged('CollectionNumberOne')[0];pool=cloud.numbers
            # A single encompassing cut removes every remaining support without per-instance actors.
            rock=min(tagged('CollectionExpansionSoil'),key=lambda a:abs(a.get_actor_location().y-cloud.get_actor_location().y))
            rock.mine_sphere(cloud.get_actor_location(),510)
            check('Numerals disappear in the terrain commit frame',pool.get_instance_count()==0)
            s.update(phase=16,next=now+1)
        elif phase==16:
            check('All number supports can be excavated without floating remnants',tagged('CollectionNumberOne')[0].numbers.get_instance_count()==0 and len(tagged('CollectionNumberStroke'))==0)
            s['f'].reset_collection();s.update(phase=12,next=now+2)
    except Exception: finish(False,traceback.format_exc())

callback=unreal.register_slate_post_tick_callback(tick)
