"""Exercise the migrated prop, real E pickup and connected entrance outflow in PIE."""
import unreal,time,pathlib,json
state={'phase':0,'start':time.monotonic(),'checks':[]}
out=pathlib.Path(unreal.Paths.project_saved_dir())/'HeavenPreview';out.mkdir(exist_ok=True)
def check(label,ok):
 state['checks'].append({'label':label,'pass':bool(ok)})
def shot(pc,name):unreal.MiningGUIEditorLibrary.capture_preview(pc,str(out/name))
def tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  w=worlds[0];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
  if not pawn:return
  age=time.monotonic()-state['start'];phase=state['phase'];toilet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HeavenToilet) if a.get_actor_location().z>-1000]
  if phase<5 and not toilet:return
  t=toilet[0] if toilet else None;field=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0];water=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HeavenToiletWater)[0]
  def pose(p,target):
   pawn.set_actor_location(p,False,True);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target))
  if phase==0 and age>3 and field.items:
   pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
   item=field.items[0];check('single original collectible uses the placed toilet',len(field.items)==1 and t.get_owner()==item and not item.get_editor_property('model').is_visible())
   check('original diggable entrance and fixed basin preserved',len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock))==3 and water.radius==1320 and water.water_height==48 and not water.spilling)
   pose(unreal.Vector(2050,-170,180),unreal.Vector(2580,0,155));state.update(phase=1,at=age)
  elif phase==1 and age>state['at']+2:
   check('far: migrated toilet closed and water off',t.lid_open_amount<.01 and not t.water_running);shot(pc,'hall_closed.png')
   pose(unreal.Vector(2430,-80,180),unreal.Vector(2580,0,158));state.update(phase=2,at=age)
  elif phase==2 and age>state['at']+.22:
   check('approach opens lid before water starts',.05<t.lid_open_amount<.85 and not t.water_running);state['phase']=3
  elif phase==3 and age>state['at']+2:
   check('near: lid fully opens with original adapted Niagara',t.lid_open_amount>.99 and t.water_running and t.get_editor_property('water_jet').is_active());shot(pc,'hall_gushing.png')
   state.update(phase=4,at=age)
  elif phase==4 and age>state['at']+4:
   check('water remains continuous without stretched actor scale',t.water_running and t.get_editor_property('water_jet').get_world_scale().x<1)
   # The real aim trace hits the authored body and resolves its original collectible owner.
   pose(unreal.Vector(2420,0,180),t.get_actor_location()+unreal.Vector(0,0,50))
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',True);state.update(phase=5,at=age)
  elif phase==5 and age>state['at']+.4:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',False)
   check('E pickup collects original ID and removes lid/body/water together',field.inventory[0]==1 and not [a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HeavenToilet) if a.get_actor_location().z>-1000])
   # A high tunnel and blind pocket must not create water outflow.
   soil=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock)
   for r in soil:
    for x in range(420,1460,65):r.mine_sphere(unreal.Vector(x,0,180),65)
   water.refresh_breach();check('high tunnel does not leak basin water',not water.spilling and not water.get_editor_property('outlet_spray').is_active())
   for r in soil:r.reset_rock()
   for r in soil:
    for x in range(420,1460,65):r.mine_sphere(unreal.Vector(x,0,38),80)
   water.refresh_breach();check('low connected breach opens the basin outlet',water.spilling)
   pose(unreal.Vector(115,-180,120),unreal.Vector(420,0,45));state.update(phase=6,at=age)
  elif phase==6 and age>state['at']+3:
   shot(pc,'entrance_flow.png');check('entrance spray remains active',water.get_editor_property('outlet_spray').is_active())
   for r in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock):r.reset_rock()
   water.refresh_breach();state.update(phase=7,at=age)
  elif phase==7 and age>state['at']+2:
   check('resealed entrance stops spray after existing drops finish',not water.spilling and not water.get_editor_property('outlet_spray').is_active())
   state['passed']=all(x['pass'] for x in state['checks']);(out/'migration_validation.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf8');print('MIGRATION_VALIDATION',state['passed']);unreal.unregister_slate_post_tick_callback(handle);unreal.EditorLevelLibrary.editor_end_play()
 except Exception as error:
  state['error']=repr(error);(out/'migration_validation.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf8');print('MIGRATION_ERROR',repr(error));unreal.unregister_slate_post_tick_callback(handle);unreal.EditorLevelLibrary.editor_end_play()
handle=unreal.register_slate_post_tick_callback(tick);unreal.MiningGUIEditorLibrary.start_floating_preview()
