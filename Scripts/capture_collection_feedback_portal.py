"""Capture the shared E prompt and travel through it to the stronger cell shader."""
from pathlib import Path
import unreal
common=(Path(unreal.Paths.project_dir())/'Scripts/capture_collection_feedback_rooms.py').read_text(encoding='utf-8')
exec(compile(common.split('\ndef tick(dt):')[0].replace("'rooms.json'","'portal.json'"),'<shared capture helpers>','exec'))
def tick(dt):
 if s.get('busy') or s.get('done'):return
 s['busy']=True
 try:
  now=time.monotonic();ph=s['phase']
  if now-s['start']>240:raise RuntimeError('Portal timeout '+str(ph))
  if now<s['next']:return
  if ph==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0');unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,next=now+5);return
  if ph==1:
   w=ed.get_game_world()
   if not w:return
   s['w']=w;hall=actors(unreal.MiningExhibitHall)[0]
   if not hall.ready:return
   for a in actors(unreal.MiningOpening):
    if not a.finished:a.start_game();s['next']=now+.5;return
   s.update(hall=hall,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0))
   area(5);hall.set_actor_tick_enabled(False);puzzle=actors(unreal.WenSiMirrorPuzzle)[0];check('Museum skip stops at painting',puzzle.skip_to_reward());s.update(phase=2,next=now+2);return
  if ph==2:
   hud=next(h for h in unreal.ObjectIterator(unreal.MiningInteractionHUDWidget) if h.get_owning_player()==s['pc']);check('Painting uses shared E action',hud.interaction_action=='进入画中的圆圈');shot('museum_common_e_prompt')
   unreal.MiningGUIEditorLibrary.send_preview_key(s['pc'],'E',True);s.update(phase=3,next=now+.15);return
  if ph==3:
   unreal.MiningGUIEditorLibrary.send_preview_key(s['pc'],'E',False);s.update(phase=4,next=now+.5);return
  if ph==4:
   w=ed.get_game_world()
   if not w or 'Lvl_EndingEmergence_Demo' not in w.get_name():s['next']=now+.5;return
   s.update(w=w,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=5,next=now+8);return
  if ph==5:
   ending=actors(unreal.EndingEmergence)[0];check('E enters the ending cell',ending is not None);check('Cell painting is at full weight',ending.cell_paint_weight>=.99);shot('ending_stronger_cell_paint');finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
