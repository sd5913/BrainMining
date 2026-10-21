"""Exercise the actual card encounter and capture the mouse hover border."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/verify_playtest16_cards.py').read_text(encoding='utf8')
code=code.replace('Saved/Validation/Playtest16/Cards','Saved/Validation/Playtest17/Cards')
code=code.replace("            # Recreate the saved MainGame collar", "            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.MainGameSubtitleDirector,unreal.Vector())\n            # Recreate the saved MainGame collar")
code=code.replace("        if p==17:", "        if p==17:\n            director=actors(unreal.MainGameSubtitleDirector)[0]\n            check(director.has_triggered('卡牌胜利提示') and not director.has_triggered('卡牌玩法2'),'Victory announces the drill, delaying narration until pickup')")
code=code.replace("        if p==18:", "        if p==18:\n            check(actors(unreal.MainGameSubtitleDirector)[0].has_triggered('卡牌玩法2'),'Post-pickup narration starts after collecting the drill')")
code=code.replace("step(6,1.7)","step(5.9,1.7)")
code=code.replace("step(11,3.6)","step(10.5,8)")
code=code.replace("        if p==11:","        if p==10.5:\n            cap('06b-poison-persistent');step(11,6);return\n        if p==11:")
code=code.replace("        if p==6:","        if p==5.9:\n            s['pc'].set_mouse_location(640,620);step(6,.35);return\n        if p==6:")
start=code.index('        if p==12:space()');end=code.index('        if p==14:',start)
code=code[:start]+'''        if p==12:
            space();s['burn_frame']=0;step(12.5,.75);return
        if p==12.5:
            cap('burn-%02d'%s['burn_frame']);s['burn_frame']+=1
            if s['burn_frame']<14:step(12.5,.14)
            else:step(14,2)
            return
'''+code[end:]
exec(compile(code,'playtest17_cards','exec'))
