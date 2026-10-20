"""Exercise the actual card encounter and capture the mouse hover border."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/verify_playtest16_cards.py').read_text(encoding='utf8')
code=code.replace('Saved/Validation/Playtest16/Cards','Saved/Validation/Playtest17/Cards')
code=code.replace("step(6,1.7)","step(5.9,1.7)")
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
