"""Check far / approach / opening / continuous water / exit / retrigger in PIE."""
import unreal,time,pathlib,json
toilet_test={'start':time.monotonic(),'phase':0,'checks':[]}
toilet_out=pathlib.Path(unreal.Paths.project_saved_dir())/'HeavenPreview'
def toilet_check(label,ok):toilet_test['checks'].append({'label':label,'pass':bool(ok)})
def toilet_tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn:return
  toilet=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeavenToilet)[0];age=time.monotonic()-toilet_test['start'];phase=toilet_test['phase']
  components=toilet.get_components_by_class(unreal.NiagaraComponent);jet=next(c for c in components if c.get_name()=='RealWaterJet')
  if phase==0 and age>2:
   expected='/Game/Fab/Toilet/Toilet.Toilet'
   toilet_check('reloaded body and lid retain original porcelain material',all(c.get_material(0) and c.get_material(0).get_path_name()==expected for c in toilet.get_components_by_class(unreal.StaticMeshComponent)))
   toilet_check('reloaded adapted water assets are bound',jet.get_editor_property('asset').get_path_name().startswith('/Game/HeavenPreview/VFX/NS_HeavenToiletWater.') and toilet.get_editor_property('opening_splash').get_editor_property('asset').get_path_name().startswith('/Game/HeavenPreview/VFX/NS_HeavenToiletOpeningSplash.'))
   pawn.set_actor_location(unreal.Vector(-390,-160,155),False,True);pc.set_control_rotation(unreal.Rotator(pitch=-6,yaw=23,roll=0));toilet_test['phase']=1
  elif phase==1 and age>4:
   toilet_check('far: lid closed and water off',toilet.get_editor_property('lid_open_amount')<.01 and not toilet.get_editor_property('water_running'))
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(toilet_out/'toilet_closed.png'));pawn.set_actor_location(unreal.Vector(-155,-80,135),False,True);pc.set_control_rotation(unreal.Rotator(pitch=-16,yaw=27,roll=0));toilet_test['phase']=2;toilet_test['near_at']=age
  elif phase==2 and age>toilet_test['near_at']+.22:
   amount=toilet.get_editor_property('lid_open_amount');toilet_check('approach: lid animates before water',.05<amount<.85 and not toilet.get_editor_property('water_running'));toilet_test['phase']=3
  elif phase==3 and age>toilet_test['near_at']+2.:
   toilet_test['first_activation']={'lid':toilet.get_editor_property('lid_open_amount'),'requested':toilet.get_editor_property('water_running'),'active':jet.is_active(),'nearby':toilet.get_editor_property('player_nearby'),'position':str(pawn.get_actor_location())}
   toilet_check('near: lid open and real Niagara active',toilet.get_editor_property('lid_open_amount')>.99 and toilet.get_editor_property('water_running') and jet.is_active())
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(toilet_out/'toilet_gushing.png'));toilet_test['phase']=4
  elif phase==4 and age>toilet_test['near_at']+7.:
   toilet_check('near: water remains running beyond initial burst',jet.is_active() and toilet.get_editor_property('water_running'))
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(toilet_out/'toilet_continuous.png'));pawn.set_actor_location(unreal.Vector(-440,-130,145),False,True);pc.set_control_rotation(unreal.Rotator(pitch=-6,yaw=17,roll=0));toilet_test['phase']=5;toilet_test['far_at']=age
  elif phase==5 and age>toilet_test['far_at']+2.:
   toilet_check('exit: lid closes and water emission stops',toilet.get_editor_property('lid_open_amount')<.01 and not toilet.get_editor_property('water_running'))
   pawn.set_actor_location(unreal.Vector(-150,-70,135),False,True);toilet_test['phase']=6;toilet_test['reenter_at']=age
  elif phase==6 and age>toilet_test['reenter_at']+1.5:
   toilet_check('re-enter: lid and water trigger again',toilet.get_editor_property('water_running') and jet.is_active())
   toilet_test['passed']=all(x['pass'] for x in toilet_test['checks']);(toilet_out/'interactive_checks.json').write_text(json.dumps(toilet_test,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_INTERACTIVE_CHECKS',toilet_test['passed']);unreal.unregister_slate_post_tick_callback(toilet_handle);unreal.EditorLevelLibrary.editor_end_play()
 except Exception as error:
  toilet_test['error']=repr(error);(toilet_out/'interactive_checks.json').write_text(json.dumps(toilet_test,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_INTERACTIVE_ERROR',repr(error));unreal.unregister_slate_post_tick_callback(toilet_handle);unreal.EditorLevelLibrary.editor_end_play()
toilet_handle=unreal.register_slate_post_tick_callback(toilet_tick);unreal.MiningGUIEditorLibrary.start_floating_preview()
