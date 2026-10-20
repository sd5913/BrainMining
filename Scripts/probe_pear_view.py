from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());base=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8');head=base[:base.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/Playtest16/PearProbe')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc;yield 7
 H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.finished:yield .2
 pear=unreal.GameplayStatics.get_all_actors_with_tag(w,'StartRoomView_Pear')[0];target=pear.get_actor_bounds(False)[0]
 r['views']=[];r['shelf']=[]
 for i,(x,y) in enumerate([(-34640,210),(-34610,230),(-34580,230),(-34540,230),(-34490,230),(-34470,270)]):
  eye=unreal.Vector(x,y,11230);pose(eye,target);yield .4
  h=unreal.SystemLibrary.line_trace_single(w,eye,target,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pc.get_controlled_pawn()],unreal.DrawDebugTrace.NONE,True).to_tuple()
  outlined=pear.static_mesh_component.get_editor_property('render_custom_depth')
  r['views'].append(dict(eye=eye.to_tuple(),outline=outlined,hit=str(h)))
  H.capture_preview(pc,str(OUT/f'pear_{i}.png'))
  if outlined:
   H.send_preview_key(pc,'E',True);H.send_preview_key(pc,'E',False);yield .4;H.capture_preview(pc,str(OUT/'subtitle.png'))
 for x in [-34522,-34535]:
  h=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(x,288,11180),unreal.Vector(x,288,11100),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pc.get_controlled_pawn(),pear],unreal.DrawDebugTrace.NONE,True).to_tuple();r['shelf'].append(str(h))
 r['passed']=True;finish()
'''
exec(compile(head+run+base[base.index('def finish('):],'pear_probe_runtime','exec'))
