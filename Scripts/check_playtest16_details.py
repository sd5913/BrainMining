import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir());base=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8');head=base[:base.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/Playtest16/Details')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc;yield 7
 H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.finished:yield .2
 pear=unreal.GameplayStatics.get_all_actors_with_tag(w,'StartRoomView_Pear')[0];target=pear.get_actor_bounds(False)[0]
 pose(unreal.Vector(-34580,230,11230),target);yield 1
 assert pear.static_mesh_component.get_editor_property('render_custom_depth');H.capture_preview(pc,str(OUT/'pear.png'))
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 pick=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
 target=pick.get_actor_location();pose(target+unreal.Vector(-100,0,70),target);yield .3;assert room.interact(pc)
 H.prepare_collection_preview_area(pc,1,False);yield .6
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 for i,eye in enumerate([unreal.Vector(-16680,-650,6550),unreal.Vector(-16350,-650,6550),unreal.Vector(-16570,-550,6550)]):
  pose(eye,unreal.Vector(-16463,-418,6585));yield .4;H.capture_preview(pc,str(OUT/f'office_cap_{i}.png'))
 H.prepare_collection_preview_area(pc,4,False);hall.set_actor_tick_enabled(False);yield .5
 pawn=pc.get_controlled_pawn();mining=pawn.get_component_by_class(unreal.MiningComponent);mining.unlock_drill();assert mining.activate_drill()
 pose(unreal.Vector(33600,11750,20080),unreal.Vector(34500,11750,19800));yield .8
 assert mining.drill_mode;H.capture_preview(pc,str(OUT/'drill.png'))
 r['passed']=True;finish()
'''
exec(compile(head+run+base[base.index('def finish('):],'details_runtime','exec'))
