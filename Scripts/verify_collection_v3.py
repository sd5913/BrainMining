"""Dedicated PIE integration run. No editor map is modified or saved."""
import unreal,time,json,traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=Path(unreal.Paths.project_saved_dir())/'Validation';out.mkdir(parents=True,exist_ok=True)
s={'phase':0,'start':time.monotonic(),'next':0,'checks':[],'index':0,'attempts':0,'cuts':{},'positions':{}}
def check(name,result):
    assert result,name
    s['checks'].append(name)
def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if ed.get_game_world():levels.editor_request_end_play()
    report={'passed':ok,'checks':s['checks'],'cuts':s['cuts'],'error':error,'seconds':time.monotonic()-s['start']}
    (out/'collection_v03_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('COLLECTION_V03_RUNTIME '+json.dumps(report,ensure_ascii=False));unreal.SystemLibrary.quit_editor()
def aim(item,from_x=None):
    p=item.get_actor_location();z=450 if item.item_id==3 else 100
    s['pawn'].set_actor_location(unreal.Vector(p.x-240 if from_x is None else from_x,p.y,z),False,True)
    eye=s['pawn'].get_actor_eyes_view_point()[0]
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,p))
def tick(dt):
    try:
        now=time.monotonic()
        if now-s['start']>300:raise RuntimeError('V03 verification timed out at phase '+str(s['phase']))
        if now<s['next']:return
        p=s['phase']
        if p==0:
            check('Independent V03 map loads',levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03'))
            levels.editor_request_begin_play();s['phase']=1;s['next']=now+2
        elif p==1:
            w=ed.get_game_world()
            if not w:return
            f=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            if len(f.items)!=5:return
            s.update(f=f,w=w,pawn=unreal.GameplayStatics.get_player_pawn(w,0),pc=unreal.GameplayStatics.get_player_controller(w,0))
            s['mining']=s['pawn'].get_component_by_class(unreal.MiningComponent)
            s['movement']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent);s['movement'].disable_movement()
            check('V03 uses demonstration depth 750 cm',f.deep_field and f.excavation_depth==750)
            for item in f.items[:4]:
                check('Item '+str(item.item_id)+' uses shortened demonstration layout',700<item.get_actor_location().x-460<1080)
                if item.item_id in [0,1,3]:
                    check('Cave item '+str(item.item_id)+' starts exposed without physics',item.released and not item.get_editor_property('body').is_simulating_physics())
                    s['positions'][item.item_id]=item.get_actor_location()
                else: check('Test tubes remain buried',not item.released)
                aim(item,200);f.collect_target();check('Solid entrance blocks item '+str(item.item_id),not item.collected)
            check('Ant trail actors exist',f.ambient_ant_count==18)
            ants=unreal.GameplayStatics.get_all_actors_with_tag(w,'CollectionAmbientAnt');s['ant']=ants[0];s['ant_pos']=s['ant'].get_actor_location()
            check('Birds stay inside sealed cave',f.flock_bursts==0)
            s['feathers']=unreal.GameplayStatics.get_all_actors_with_tag(w,'CollectionFeather')
            check('Seven recognizable feather actors exist',len(s['feathers'])==7)
            s['feather_positions']=[a.get_actor_location() for a in s['feathers']]
            chemical=next(i for i in f.items if i.item_id==2 and i!=f.items[4])
            cr=min(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-chemical.get_actor_location().y))
            check('Chemical stain lives on soil material',cr.rock_mesh.get_material(0).get_editor_property('parent').get_name()=='M_CollectionStainedEarth')
            loose=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor) if a.get_actor_location().x>440 and abs(a.get_actor_location().y-chemical.get_actor_location().y)<150 and 80<a.get_actor_location().z<270]
            check('No loose colored clue props in chemical tunnel',not loose)
            f.toggle_catalogue();check('Empty catalogue opens',f.catalogue_open);f.toggle_catalogue()
            aim(f.items[4]);s['phase']=2;s['next']=now+.6
        elif p==2:
            f=s['f'];check('Ants crawl over time',(s['ant'].get_actor_location()-s['ant_pos']).length()>1)
            for item in f.items[:4]:
                if item.item_id in [0,1]:check('Cave item '+str(item.item_id)+' remains stationary',item.get_actor_location()==s['positions'][item.item_id])
            check('Aim enables custom-depth silhouette',f.items[4].get_editor_property('model').get_editor_property('render_custom_depth'))
            hull=[c for c in f.items[4].get_components_by_class(unreal.DynamicMeshComponent) if c.get_name()=='WhiteSilhouette'][0]
            check('Old full-surface hull remains hidden',not hull.is_visible())
            f.collect_target();check('Exposed tutorial still collects',sum(f.inventory)==1)
            f.toggle_catalogue();f.select_item(2);f.rotate_preview(45,20)
            check('Large catalogue model rotates',abs(f.preview.get_actor_rotation().yaw-15)<1)
            check('Catalogue pauses excavation',not s['mining'].is_component_tick_enabled())
            f.toggle_catalogue();check('Catalogue resumes excavation',s['mining'].is_component_tick_enabled())
            check('Feathers remain fixed while soil supports them',all(a.get_actor_location()==pos for a,pos in zip(s['feathers'],s['feather_positions'])))
            s['feather']=s['feathers'][0];s['feather_start']=s['feather'].get_actor_location()
            bird=next(i for i in f.items if i.item_id==3)
            s['birdrock']=min(unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-bird.get_actor_location().y))
            check('Remove only feather support with real soil cut',s['birdrock'].mine_sphere(s['feather_start']+unreal.Vector(3,0,0),50))
            s['phase']=20;s['next']=now+.7
        elif p==20:
            check('Unsupported feather starts fluttering',s['feather'].actor_has_tag('FeatherFalling'))
            check('Unsupported feather descends',s['feather'].get_actor_location().z<s['feather_start'].z-5)
            check('Adjacent supported feather stays attached',s['feathers'][1].get_actor_location()==s['feather_positions'][1])
            s['phase']=21;s['next']=now+8
        elif p==21:
            check('Falling feather settles on actual floor',s['feather'].actor_has_tag('FeatherLanded') and 1<s['feather'].get_actor_location().z<10)
            s['f'].reset_collection()
            check('Reset restores seven supported feathers',len(unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'CollectionFeather'))==7 and not unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'FeatherFalling'))
            aim(s['f'].items[4]);s['f'].collect_target()
            s['phase']=3;s['next']=now+.4
        elif p==3:
            f=s['f'];item=f.items[s['index']]
            if f.flock_bursts and not s.get('bird_motion_verified'):
                flying=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'CollectionFlyingBird')
                if flying and 'flying_bird' not in s:
                    s['flying_bird']=flying[0];s['bird_pos']=flying[0].get_actor_location()
                elif flying and s.get('flying_bird') in flying:
                    check('Flock birds fly along the excavated passage',(s['flying_bird'].get_actor_location()-s['bird_pos']).length()>1)
                    s['bird_motion_verified']=True
            aim(item,min(200+s['attempts']*65,item.get_actor_location().x-240))
            f.collect_target()
            if item.collected:
                rock=min(unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-item.get_actor_location().y))
                count=rock.successful_cuts
                s['cuts'][str(item.item_id)]=count
                check('Item '+str(item.item_id)+' needs sustained excavation',4<=count<=8)
                s['index']+=1;s['attempts']=0;s['next']=now+.4
                if s['index']==4:s['phase']=4;s['next']=now+1
                return
            s['mining'].try_mine();s['attempts']+=1
            if s['attempts']>45:raise RuntimeError('Cannot reach item '+str(item.item_id)+' at '+str(item.get_actor_location())+' status='+s['mining'].status)
            s['next']=now+.34
        elif p==4:
            f=s['f'];check('All four kinds collected',all(n>0 for n in f.inventory))
            check('Opening actual passage triggers one flock',f.flock_bursts==1)
            f.set_editor_property('random_seed',9137);f.reset_collection()
            first=[(i.item_id,str(i.get_actor_location())) for i in f.items]
            f.reset_collection();second=[(i.item_id,str(i.get_actor_location())) for i in f.items]
            check('Fixed seed still reproduces deep layout',first==second)
            check('Reset clears inventory and rearms bird event',sum(f.inventory)==0 and f.flock_bursts==0)
            check('Reset does not duplicate ants',len(unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'CollectionAmbientAnt'))==18)
            levels.editor_request_end_play();s['phase']=5;s['next']=now+1
        elif p==5:
            if ed.get_game_world():return
            check('V01 comparison level still loads',levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V01'))
            levels.editor_request_begin_play();s['phase']=6;s['next']=now+2
        elif p==6:
            w=ed.get_game_world()
            if not w:return
            f=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            check('V01 retains short layout',not f.deep_field and all(i.get_actor_location().x<650 for i in f.items))
            levels.editor_request_end_play();s['phase']=7;s['next']=now+1
        elif p==7:
            if ed.get_game_world():return
            check('Original MiningLab loads',levels.load_level('/Game/MiningPrototype/Maps/Lvl_MiningLab'))
            levels.editor_request_begin_play();s['phase']=8;s['next']=now+2
        elif p==8:
            w=ed.get_game_world()
            if not w:return
            check('Original demo has no collection field',not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField))
            rock=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock)[0];pos=rock.get_actor_location();size=rock.rock_size
            check('Original digging works',rock.mine_sphere(unreal.Vector(pos.x-size.x*.5,pos.y,pos.z),120))
            unreal.GameplayStatics.get_player_pawn(w,0).get_component_by_class(unreal.MiningComponent).reset_field()
            check('Original reset works',rock.successful_cuts==0);finish(True)
    except Exception:finish(False,traceback.format_exc())
callback=unreal.register_slate_post_tick_callback(tick)
