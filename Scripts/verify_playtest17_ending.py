"""Check vessel transitions and earlier credits in the production ending map."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_playtest16_ending.py').read_text(encoding='utf8')
code=code.replace("code=code.replace('Saved/Validation/TraversalFeedback/Blood','Saved/Validation/Playtest16/Ending')", "code=code.replace('Saved/Validation/TraversalFeedback/Blood','Saved/Validation/Playtest17/Ending')")
code=code.replace("next=now+10", "next=now+5.5")
# Insert arrival captures into the underlying runtime script before its vessel fixture.
code=code.replace("a=code.index(\"            capture('vessel_lit')\")", '''code=code.replace("next=now+8", "next=now+.1")
code=code.replace("        if p=='ready':", "        if p=='arrival_mid':\\n            capture('00b_arrival_fading');s.update(phase='arrival_done',next=now+1.7);return\\n        if p=='arrival_done':\\n            capture('00c_arrival_visible');s['arrival_done']=True;s.update(phase='ready',next=now+.1);return\\n        if p=='ready':")
code=code.replace("            lights=[", "            if not s.get('arrival_done'):\\n                capture('00a_arrival_black');s.update(phase='arrival_mid',next=now+.6);return\\n            lights=[")
a=code.index("            capture('vessel_lit')")''')
code=code.replace("            capture('04_credits')", "            ui=H.read_preview_object(s['d'],'CreditsWidget');names=H.read_preview_object(ui,'Contributors');thanks=H.read_preview_object(ui,'ThankYou')\n            check(all(n in str(names.get_text()) for n in ['铁皮辣妹','空空道人','Seven','Rasimet']),'All developers appear in the credits')\n            check(names.get_render_opacity()>.9 and thanks.get_render_opacity()>.9,'Developers and thanks appear together')\n            check(names.slot.get_position().y-thanks.slot.get_position().y-thanks.slot.get_size().y>=40,'Developers have a clear gap below thanks')\n            capture('04_credits')")
code=code.replace("            capture('04_credits')", "            font=unreal.load_asset('/Game/Fonts/F_CreditsBilingual')\n            check(names.get_editor_property('default_text_style_override').font.font_object==font,'Developers use Hiragino W6 with Rockwell Latin')\n            for label in ['ThankYou','Prompt']:\n                check(H.read_preview_object(ui,label).get_editor_property('font').font_object==font,label+' uses the bilingual credits font')\n            capture('04_credits')")
exec(compile(code,'playtest17_ending','exec'))
