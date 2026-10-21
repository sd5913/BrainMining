"""Start the production game and walk away from the restored bedside landing."""
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
source=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8')
head=source[:source.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/OpeningRestore')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc
 yield 8;H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0]
 assert H.capture_preview(pc,str(OUT/'01_menu_unchanged.png'))
 r['menu_eye']=str(opening.get_editor_property('SeatEye'))
 opening.start_game()
 while not opening.finished:yield .1
 pawn=pc.get_controlled_pawn();landing=pawn.get_actor_location()
 expected=opening.get_editor_property('StandingLocation')
 assert ((landing-expected)*unreal.Vector(1,1,0)).length()<10,'Player lands beside the bed'
 assert not pc.is_move_input_ignored() and not pc.is_look_input_ignored(),'Opening restores player controls'
 assert pawn.get_actor_enable_collision(),'Normal player collision is restored'
 assert H.capture_preview(pc,str(OUT/'02_landing.png'))
 H.send_preview_key(pc,'W',True);yield 1.2;H.send_preview_key(pc,'W',False)
 end=pawn.get_actor_location()
 r['walk_distance_cm']=((end-landing)*unreal.Vector(1,1,0)).length()
 r['landing']=str(landing);r['after_walking']=str(end)
 assert r['walk_distance_cm']>60,'Player must walk freely away from the bed after starting'
 assert abs(end.z-landing.z)<50,'Player remains on the bedroom floor'
 assert H.capture_preview(pc,str(OUT/'03_walked_away.png'))
 r['passed']=True;finish()
'''
exec(compile(head+run+source[source.index('def finish('):],'opening_restore','exec'))
