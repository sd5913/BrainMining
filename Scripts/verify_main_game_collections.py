"""PIE excavation, real E input, echo material and room integration checks."""
import unreal,json,time,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameCollections';OUT.mkdir(parents=True,exist_ok=True)
cfg=json.loads((ROOT/'Config/MainGameCollections.json').read_text());auth=json.loads((OUT/'authoring.json').read_text());assert auth['passed']
promotion=json.loads((OUT/'promotion.json').read_text()) if (OUT/'promotion.json').exists() else {}
test_map=cfg['source_map'] if promotion.get('passed') else cfg['preview_map']
source_hashes={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in auth['source_hashes']}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s=dict(phase=0,start=time.monotonic(),next=0,checks=[],index=0)
def check(name,v):
    assert v,name
    if name not in s['checks']:s['checks'].append(name);unreal.log('COLLECTION_CHECK '+name)
def xyz(v):return list(v.to_tuple())
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def tagged(tag):return unreal.GameplayStatics.get_all_actors_with_tag(s['w'],tag)
def trace(p,q):return unreal.SystemLibrary.line_trace_single(s['w'],p,q,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
def pose(p,target):
    s['pawn'].set_actor_location(p,False,True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target))
def shot(name):
    if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
        check('Screenshot '+name,unreal.MiningGUIEditorLibrary.capture_preview(s['pc'],str(OUT/name)))
def key(k):
    unreal.MiningGUIEditorLibrary.send_preview_key(s['pc'],k,True)
    s.update(release_key=k,release_at=time.monotonic()+.15)
def area(tag):
    index=next(i for i,a in enumerate(s['hall'].areas) if str(a.tag)==tag)
    check('Area activation '+tag,unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],index,False))
def finish(ok,error=''):
    if s.get('done'):return
    s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
    if ed.get_game_world():levels.editor_request_end_play()
    report=dict(passed=ok,error=error,map=test_map,checks=s['checks'],seconds=time.monotonic()-s['start'])
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_COLLECTION_RUNTIME '+json.dumps(report))
    cleanup={'after':time.monotonic()+2,'loaded':False}
    def close_when_pie_ended(dt):
        if time.monotonic()<cleanup['after'] or ed.get_game_world():return
        if not cleanup['loaded']:
            s.clear();levels.load_level('/Engine/Maps/Entry');cleanup.update(loaded=True,after=time.monotonic()+1);return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close_when_pie_ended)
def tick(dt):
    if s.get('busy') or s.get('done'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if s.get('release_key') and now>=s['release_at']:
            unreal.MiningGUIEditorLibrary.send_preview_key(s['pc'],s.pop('release_key'),False)
        if now-s['start']>480:raise RuntimeError('Timeout in phase '+str(s['phase']))
        if now<s['next']:return
        ph=s['phase']
        if ph==0:
            check('Collection map loads '+test_map,levels.load_level(test_map))
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,next=now+4);return
        if ph==1:
            w=ed.get_game_world()
            if not w:return
            s['w']=w;h=actors(unreal.MiningExhibitHall)[0]
            if not h.ready:return
            for opening in actors(unreal.MiningOpening):opening.start_game()
            s.update(hall=h,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),field=actors(unreal.MiningCollectionField)[0],phase=2,next=now+5);return
        if ph==2:
            for opening in actors(unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['next']=now+.2;return
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            s['items']={i.item_id:i for i in s['field'].items}
            s['hosts']={p.item_id:p.host_rock for p in s['field'].authored_placements}
            check('Eleven authored collectibles, no duplicate stage ribbon',set(s['items'])==set(range(11)))
            check('One shared collection inventory',len(actors(unreal.MiningCollectionField))==1)
            check('All seven excavation clues created',tagged('CollectionNumberOne')[0].numbers.get_instance_count()>=800 and len(tagged('CollectionSoilAnt'))==42 and len(tagged('CollectionGreenGlow'))==1 and len(tagged('CollectionColaBubbles'))==1)
            s['number_count']=tagged('CollectionNumberOne')[0].numbers.get_instance_count()
            check('Reagent stain does not recolor the entire mine',len(tagged('CollectionReagentStain'))>0)
            tape=s['items'][5]
            check('Cassette is half previous size and upright',abs(tape.get_actor_scale3d().x-.25)<.001 and abs(tape.get_actor_rotation().pitch)<.01)
            check('Cassette rests on the chair seat',abs(tape.get_actor_location().z-29*.25-auth['cassette']['seat'][2]-.5)<.1)
            check('Firefly jar contains its glowing swarm',len(tagged('CollectionJarSwarm'))==1)
            s['echo']=actors(unreal.EchoPassageStage)[0]
            for item in [s['items'][6],s['items'][7]]:
                outline=next(c for c in item.get_components_by_class(unreal.DynamicMeshComponent) if c.get_name()=='WhiteSilhouette');c=next(c for c in s['echo'].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='EchoContours')
                check('Echo shared material for '+str(item.item_id),outline.get_material(0)==c.get_material(0))
            s['views']=list(auth['placements']);s.update(phase=3,index=0);return
        if ph==3:
            row=s['views'][s['index']];item=s['items'][row['item_id']];area(row['area_tag'])
            p=item.get_actor_location();front=unreal.Vector(*row['clue']);normal=unreal.Vector(*row.get('normal',[0,1,0]))
            if row['item_id']==0:
                frame=unreal.Transform(location=unreal.Vector(*auth['heaven']['frame_position']),rotation=unreal.Rotator(yaw=auth['heaven']['yaw']))
                s['heaven_frame']=frame
                pose(unreal.MathLibrary.transform_location(frame,unreal.Vector(-450,0,110)),front)
            elif row['item_id'] in [6,7]:pose(p+unreal.Vector(0,220,45),p);s['echo'].emit_echo(p+unreal.Vector(0,70,0),True)
            elif row['item_id']==5:pose(p+unreal.Vector(35,125,65),p)
            elif row['item_id'] in [8,9]:pose(front+unreal.Vector(-160,80,170),front)
            else:pose(front+normal*230+unreal.Vector(0,0,-30),front)
            s.update(phase=4,next=now+1.6);return
        if ph==4:
            row=s['views'][s['index']];shot('%02d_item_%02d_before.png'%(s['index']+1,row['item_id']))
            if row['item_id']==8:
                cola=s['items'][8];center=cola.get_actor_location()+unreal.Vector(0,0,4)
                for delta in [unreal.Vector(100,0,0),unreal.Vector(0,100,0)]:
                    hit=unreal.SystemLibrary.line_trace_single(s['w'],center,center+delta,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn'],cola],unreal.DrawDebugTrace.NONE,True)
                    check('Actual cola pocket side clearance '+str(delta),hit is not None and abs(hit.to_tuple()[5].distance(center)-32)<2)
            s['index']+=1;s.update(phase=3 if s['index']<len(s['views']) else 5,index=s['index'],next=now+.5);return
        if ph==5:
            s['dig_ids']=[2,1,4,3,9,10];s.update(index=0,phase=6);return
        if ph==6:
            item=s['items'][s['dig_ids'][s['index']]];row=next(r for r in auth['placements'] if r['item_id']==item.item_id);area(row['area_tag'])
            front=unreal.Vector(*row['clue']);normal=unreal.Vector(*row['normal']);pose(front+normal*230,item.get_actor_location())
            s.update(dig_item=item,dig_row=row,phase=7,next=now+1);return
        if ph==7:
            item=s['dig_item'];check('Buried item %d cannot be collected before digging'%item.item_id,not item.released)
            key('E');s.update(phase=70,next=now+.3);return
        if ph==70:
            item=s['dig_item'];check('Soil blocks E pickup %d'%item.item_id,not item.collected)
            front=unreal.Vector(*s['dig_row']['clue']);p=item.get_actor_location();steps=math.ceil(front.distance(p)/90)
            for i in range(steps+1):s['hosts'][item.item_id].mine_sphere(front+(p-front)*(i/max(1,steps)),225 if item.item_id==9 else 165)
            s.update(phase=8,next=now+2,dig_started=now);return
        if ph==8:
            item=s['dig_item'];check('Real excavation releases %d'%item.item_id,item.released)
            body=item.get_component_by_class(unreal.BoxComponent)
            if body.get_physics_linear_velocity().distance(unreal.Vector())>20 and now-s['dig_started']<12:
                s['next']=now+.4;return
            if item.item_id==1:
                ants=[a for a in tagged('CollectionSoilAnt') if not a.get_editor_property('hidden')]
                check('Excavation reveals ants on soil',len(ants)>=3)
                s['ant_samples']=[(a,a.get_actor_location()) for a in ants]
            p=item.get_actor_location();normal=unreal.Vector(*s['dig_row']['normal']);pose(p+normal*190+unreal.Vector(0,0,30),p)
            s.update(phase=9,next=now+.8);return
        if ph==9:
            if s['dig_item'].item_id==1:
                check('Exposed ants continue crawling',sum(a.get_actor_location().distance(p)>5 for a,p in s['ant_samples'])>=2)
            item=s['dig_item'];shot('dug_%02d.png'%item.item_id);s.update(phase=91,next=now+.2);return
        if ph==91:
            key('E');s.update(phase=10,next=now+.3);return
        if ph==10:
            item=s['dig_item'];check('Real E input collects %d'%item.item_id,item.collected and s['field'].inventory[item.item_id]==1)
            if item.item_id==4:check('Numbers disappear with their excavated supports',0<tagged('CollectionNumberOne')[0].numbers.get_instance_count()<s['number_count'])
            s['index']+=1;s.update(phase=6 if s['index']<len(s['dig_ids']) else 11,next=now+.3);return
        if ph==11:
            area('BM_Campaign_MainGame03');frame=s['heaven_frame'];local=lambda x,y,z:unreal.MathLibrary.transform_location(frame,unreal.Vector(x,y,z))
            water=actors(unreal.HeavenToiletWater)[0];s['water']=water
            check('Heaven branch is sealed by real soil',trace(local(400,0,130),local(1470,0,130)) is not None)
            check('Relocated water remains retained before digging',not water.spilling)
            pose(local(250,0,110),local(470,0,220))
            for rock in water.retaining_soil:
                for x in range(420,1460,65):rock.mine_sphere(local(x,0,180),65)
            water.refresh_breach();check('Rotated room: high passage cannot leak below it',not water.spilling)
            for rock in water.retaining_soil:
                for x in range(420,1460,90):rock.mine_sphere(local(x,0,110),170)
            water.refresh_breach();check('Rotated room: connected excavation releases water',water.spilling)
            check('Heaven entrance has a continuous excavated passage',trace(local(400,0,140),local(1470,0,140)) is None)
            pose(local(1540,-130,170),local(2580,0,250));s.update(phase=111,next=now+2);return
        if ph==111:
            shot('heaven_room_after_excavation.png');s.update(direct_ids=[5,6,7,8,0],index=0,phase=12);return
        if ph==12:
            item=s['items'][s['direct_ids'][s['index']]];row=next(r for r in auth['placements'] if r['item_id']==item.item_id);area(row['area_tag']);p=item.get_actor_location()
            if item.item_id==8:
                front=unreal.Vector(*row['clue']);s['hosts'][item.item_id].mine_sphere(front,210);s['hosts'][item.item_id].mine_sphere(p,170)
                pose(p+unreal.Vector(-180,0,100),p)
            elif item.item_id==0:
                frame=s['heaven_frame'];pose(unreal.MathLibrary.transform_location(frame,unreal.Vector(2320,0,220)),p)
            elif item.item_id==5:pose(p+unreal.Vector(35,125,65),p)
            else:pose(p+unreal.Vector(0,190,20),p);s['echo'].emit_echo(p,True)
            s.update(phase=13,next=now+1);return
        if ph==13:
            key('E');s.update(phase=14,next=now+.4);return
        if ph==14:
            item=s['items'][s['direct_ids'][s['index']]];check('Room display E pickup %d'%item.item_id,item.collected)
            s['index']+=1;s.update(phase=12 if s['index']<len(s['direct_ids']) else 15);return
        if ph==15:
            area('BM_Campaign_MainGameJourney');escape=actors(unreal.MiningRoomEscape)[0]
            unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],s['hall'].active_area,True)
            check('TV completion fixture applied',escape.phase==unreal.MiningEscapePhase.ESCAPED)
            check('Existing stage ribbon was built',escape.stage_ribbon is not None)
            pose(escape.stage_ribbon.get_actor_location()+unreal.Vector(-160,0,110),escape.stage_ribbon.get_actor_location())
            s.update(escape=escape,phase=16,next=now+1);return
        if ph==16:
            ribbon=s['escape'].stage_ribbon
            check('TV stage provides exactly one ribbon',ribbon is not None and len([i for i in s['field'].items if i.item_id==11])==1)
            pose(ribbon.get_actor_location()+unreal.Vector(-160,0,110),ribbon.get_actor_location());s.update(ribbon=ribbon,phase=17,next=now+1);return
        if ph==17:
            shot('12_existing_tv_stage.png');key('E');s.update(phase=18,next=now+.4);return
        if ph==18:
            check('Existing TV ribbon E pickup',s['ribbon'].collected)
            check('All twelve collectible types share one inventory',all(n==1 for n in s['field'].inventory[:12]))
            for p,digest in source_hashes.items():check('Source map unchanged '+Path(p).name,hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest)
            finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
import math
handle=unreal.register_slate_post_tick_callback(tick)
