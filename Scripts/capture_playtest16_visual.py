from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
source=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8')
head=source[:source.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/Playtest16/Visual')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc
 yield 7
 H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.get_editor_property('bFinished'):yield .2
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 pear=unreal.GameplayStatics.get_all_actors_with_tag(w,'StartRoomView_Pear')[0]
 target=pear.get_actor_bounds(False)[0]
 r['pear_bounds']=str(pear.get_actor_bounds(False));r['areas']=[dict(index=i,tag=str(a.tag)) for i,a in enumerate(hall.areas)];r['pear_views']=[]
 for i,eye in enumerate([unreal.Vector(-34570,165,11230),unreal.Vector(-34510,160,11230),unreal.Vector(-34470,180,11230)]):
  pose(eye,target);yield .6
  H.capture_preview(pc,str(OUT/f'pear_{i}.png'))
  r['pear_views'].append(dict(eye=str(eye),outlined=pear.static_mesh_component.get_editor_property('render_custom_depth')))
  if pear.static_mesh_component.get_editor_property('render_custom_depth'):
   H.send_preview_key(pc,'E',True);H.send_preview_key(pc,'E',False);yield .3
   H.capture_preview(pc,str(OUT/'pear_subtitle.png'))
 plant=unreal.GameplayStatics.get_all_actors_with_tag(w,'StartRoomView_Plant')[0];target=plant.get_actor_bounds(False)[0]
 pose(target+unreal.Vector(100,90,45),target);yield .4
 H.send_preview_key(pc,'E',True);H.send_preview_key(pc,'E',False);yield .3
 H.capture_preview(pc,str(OUT/'plant_subtitle.png'))
 H.prepare_collection_preview_area(pc,2,False);yield 2
 pose(unreal.Vector(-16700,-400,6550),unreal.Vector(-16463,-418,6585));yield 1
 H.capture_preview(pc,str(OUT/'office_cap.png'))
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame02')
 H.prepare_collection_preview_area(pc,index,False);hall.set_actor_tick_enabled(False);yield 2
 particles=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NiagaraActor)
 r['niagara']=[dict(name=a.get_name(),pos=str(a.get_actor_location()),asset=str(a.get_component_by_class(unreal.NiagaraComponent).get_asset())) for a in particles if -13000<a.get_actor_location().x<-3000]
 assert not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCavernMotes)
 for i,eye in enumerate([unreal.Vector(-11000,-250,7300),unreal.Vector(-6800,-250,9700)]):
  pose(eye,eye+unreal.Vector(800,200,100));yield 6
  H.capture_preview(pc,str(OUT/f'niagara_{i}.png'))
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame03')
 H.prepare_collection_preview_area(pc,index,False);hall.set_actor_tick_enabled(False);yield 2
 pose(unreal.Vector(33600,11750,20080),unreal.Vector(34500,11750,19800));yield 2
 H.capture_preview(pc,str(OUT/'third_clear.png'))
 r['passed']=True;finish()
'''
exec(compile(head+run+source[source.index('def finish('):],'playtest16_visual_runtime','exec'))
