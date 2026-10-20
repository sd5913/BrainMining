from pathlib import Path
from html import escape
import json
import sys

ROOT = Path(__file__).resolve().parent
INK = '#282A26'
CREAM = '#F4EBD6'
GOLD = '#E9BB58'
TEAL = '#8EB7AA'
MUTED = '#BDB7A6'

ICONS = {
    'pick': '<path d="M14 52 40 23M10 19Q34 0 56 25L43 21 32 27 25 17Z"/>',
    'drill': '<path d="m13 12 38-3-11 12-27 4Zm5 17 25-3-9 11-14 3Zm5 15 10-3-7 15Z"/>',
    'bomb': '<path d="m32 4 5 14 14-8-6 15 14 6-15 6 6 16-16-8-9 14-2-18-18 2 13-12-10-12 17 4Z"/><path d="m29 24-7 10 11-2-3 10 13-15-11 3 4-9"/>',
    'jet': '<rect x="14" y="19" width="13" height="27" rx="4"/><rect x="37" y="19" width="13" height="27" rx="4"/><path d="M27 24h10M27 39h10M20 51v8M44 51v8M25 11l7-7 7 7M32 5v13"/>',
    'hand': '<path d="M22 33V16a5 5 0 0 1 10 0v15-7a5 5 0 0 1 10 0v8-4a5 5 0 0 1 10 0v14c0 12-8 17-17 17-7 0-12-4-17-11L8 35q-2-6 4-7l10 10"/>',
    'pause': '<path d="M21 14v36M43 14v36" stroke-width="8"/>',
    'play': '<path d="m23 13 27 19-27 19Z"/>',
    'up': '<path d="m17 27 15-15 15 15M32 13v37M14 53h36"/>',
    'back': '<path d="m28 14-18 18 18 18M11 32h43"/>',
    'close': '<path d="m18 18 28 28M46 18 18 46"/>',
    'gem': '<path d="m19 13 26 0 12 17-25 26L7 30ZM7 30h50M19 13l-1 17 14 26 14-26-1-17"/>',
    'knock': '<path d="M8 51h48M17 44l6-8M32 44v-9M47 44l-6-8M20 8h21v18H20zM27 26v6h7v-6"/>',
    'thought': '<path d="M14 39a19 19 0 1 1 34 0l-4 6H20ZM22 51h20M27 57h10M32 15v13m-7-7 7 7 7-7"/>',
    'cursor': '<path d="m17 8 33 29-16 2-8 16Z"/>',
    'check': '<path d="m12 33 13 13 28-29"/>',
    'edit': '<path d="m12 42 29-29 11 11-29 29-15 4ZM36 18l11 11"/>',
    'sound': '<path d="m9 25 12 0 12-11v36L21 39H9ZM42 23q9 9 0 18M49 15q17 17 0 34"/>',
    'sliders': '<path d="M10 19h44M10 45h44"/><circle cx="24" cy="19" r="6" fill="currentColor"/><circle cx="42" cy="45" r="6" fill="currentColor"/>',
    'help': '<path d="M23 23a10 10 0 1 1 16 8q-7 3-7 10M32 50v1"/><circle cx="32" cy="32" r="27"/>',
    'door': '<path d="M12 54V10h29v44ZM22 33h1M46 23l9 9-9 9M36 32h19"/>',
}

def text(x, y, value, size=20, color=CREAM, weight=500, anchor='start', extra=''):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}" text-anchor="{anchor}" {extra}>{escape(value)}</text>'

def icon(name, x, y, size=40, color=CREAM):
    return f'<use href="#i-{name}" x="{x}" y="{y}" width="{size}" height="{size}" style="color:{color}"/>'

def cut(x, y, width, height, fill=INK, stroke='none', corner=14, extra=''):
    return f'<path d="M{x+corner} {y}H{x+width}V{y+height-corner}L{x+width-corner} {y+height}H{x}V{y+corner}Z" fill="{fill}" stroke="{stroke}" stroke-width="2" {extra}/>'

def defs():
    symbols = ''.join(f'<symbol id="i-{name}" viewBox="0 0 64 64"><g fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">{shape}</g></symbol>' for name, shape in ICONS.items())
    return f'<defs>{symbols}</defs><style>text{{font-family:"Microsoft YaHei","Noto Sans CJK SC",sans-serif}} .action{{cursor:pointer}} .action:focus{{outline:none}} .action:focus-visible>.surface{{stroke:#fff;stroke-width:5}} .action.held>.surface{{fill:#CF9D40;stroke:{CREAM};stroke-width:5}} .action.held use,.action.held text{{color:{INK}!important;fill:{INK}}} .pan-scene{{transition:transform .07s linear}}</style>'

def svg(content, title, height=720):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="{height}" viewBox="0 0 1600 {height}" role="img" aria-label="{title}"><title>{title}</title>{defs()}{content}</svg>'

def scene():
    return '''<g class="pan-scene">
    <path fill="#504838" d="M-80-80h1760v880H-80Z"/>
    <path fill="#70604A" d="M0 0h1600l-390 170-460-29-406 44Z"/>
    <path fill="#3E3D32" d="m0 0 344 185 89 281L0 720Z"/>
    <path fill="#62513C" d="m0 0 223 171-92 221L0 432ZM0 432l131-40 169 99L0 720Z"/>
    <path fill="#4A493A" d="m1600 0-390 170-44 304 434 246Z"/>
    <path fill="#68604A" d="m1600 0-186 261 186 190ZM1600 451l-186-190-110 236 296 223Z"/>
    <path fill="#726048" d="m0 720 433-254 733 8 434 246Z"/>
    <path fill="#887052" d="m363 720 269-243 355 0 234 243Z"/>
    <path fill="#9C8055" d="m681 720 58-159 95-9 134 168Z"/>
    <path fill="#262B27" d="m478 160 655 15 12 329-691 0Z"/>
    <path fill="#987449" d="m401 142 42-16 30 389-44 3ZM1137 147l40-3-4 373-43-11Z"/>
    <path fill="#BC9255" d="m402 125 777 4-1 47-773-8Z"/>
    <path fill="#342F27" d="m415 214 45-2 2 19-45 3ZM431 444l40-1 2 21-41 2ZM1134 214h41v20h-42ZM1132 443h42v21h-43Z"/>
    <path fill="#3A3D32" d="m514 230 185-70 186 47 163-10 38 269-562 18Z"/>
    <path fill="#646453" d="m598 286 126-73 150 37 93 111-49 139-208 17-157-80Z"/>
    <path fill="#7B7861" d="m598 286 126-73 49 152-220 72Z"/>
    <path fill="#565B4D" d="m724 213 150 37 93 111-194 4Z"/>
    <path fill="#6C6D58" d="m773 365 194-4-49 139-94-46Z"/>
    <path fill="#898267" d="m553 437 220-72 51 89-114 63Z"/>
    <path fill="#BD9555" d="m660 302 21-10 20 20-19 29-26-9Zm167 14 27-15 23 21-11 31-31-7Zm-90 117 22-13 17 20-11 21-24-7Z"/>
    <path fill="#E9BB58" d="m660 302 21-10 8 22-33 18Zm167 14 27-15-5 30-14 15Zm-90 117 22-13-7 23-11 11Z"/>
    <path fill="#31594E" d="m245 490-32-102 34-44 41 143Zm30 13 20-114 39-14 9 116Z"/>
    <path fill="#82AE9E" d="m247 344 7 121-41-77Zm48 45 39-14-18 101Z"/>
    <path fill="#474B3F" d="m1005 498 61-31 76 49-14 31-129-4ZM397 584l64-36 64 52-27 22-98-10Z"/>
    <path fill="#6F6C53" d="m1005 498 61-31 13 48-80 28ZM397 584l64-36-8 55-53 9Z"/>
    <path fill="#252A25" d="M783 163v40h37v-40Z"/><path fill="#F4D992" d="M790 169h23v24h-23Z"/>
    </g>'''

def action(name, cx, cy, radius, label, fill=INK, stroke=CREAM, active=False, action_id=None, secondary=None):
    foreground = INK if fill in (GOLD, CREAM, TEAL, '#CF9D40', '#D6D1C3') else CREAM
    points = ' '.join(f'{cx+dx*radius},{cy+dy*radius}' for dx, dy in [(-.58,-1),(.58,-1),(1,-.58),(1,.58),(.58,1),(-.58,1),(-1,.58),(-1,-.58)])
    identifier = action_id or name
    content = f'<g class="action" data-action="{identifier}" tabindex="0" role="button" aria-label="{label}"><polygon class="surface" points="{points}" fill="{fill}" fill-opacity="{1 if active else .92}" stroke="{stroke}" stroke-width="{3 if active else 2}"/>'
    size = 82 if radius > 65 else 54
    content += icon(name,cx-size/2,cy-size/2,size,foreground)
    if secondary:
        content += text(cx,cy+radius+25,secondary,15,CREAM,500,'middle')
    return content+'</g>'

def topbar():
    content = cut(48,32,365,64,INK,extra='fill-opacity=".83"')
    for offset, color, count in [(0,'#DFA175','12'),(88,'#B9B7AD','08'),(176,TEAL,'03'),(264,'#A7D4DE','01')]:
        content += icon('gem',61+offset,50,27,color)+text(96+offset,72,count,19,CREAM,700)
    content += '<g class="action" data-action="upgrade" tabindex="0" role="button" aria-label="升级装备">'+cut(1320,32,100,96,INK,CREAM,extra='class="surface"')+icon('up',1344,54,52,GOLD)+'</g>'
    content += '<g class="action" data-action="pause" tabindex="0" role="button" aria-label="暂停菜单">'+cut(1452,32,100,96,CREAM,extra='class="surface"')+icon('pause',1476,54,52,INK)+'</g>'
    return content

def joystick():
    return f'''<g opacity=".68"><circle cx="164" cy="570" r="83" fill="{INK}" fill-opacity=".35" stroke="{CREAM}" stroke-opacity=".45" stroke-width="2"/><path d="m156 474 8-8 8 8m-8 192 8 8-8 8m-96-120-8 8 8 8m192-16 8 8-8 8" fill="none" stroke="{CREAM}" stroke-width="2"/><circle cx="164" cy="570" r="35" fill="{CREAM}" fill-opacity=".85"/><circle cx="164" cy="570" r="7" fill="{INK}"/></g>'''

def hud():
    content = scene()+topbar()+joystick()
    content += '<g fill="none" stroke="#F4EBD6" stroke-width="2"><path d="M788 360h-7m31 0h7m-19-12v-7m0 31v7"/><circle cx="800" cy="360" r="2" fill="#F4EBD6"/></g>'
    content += action('hand',1138,572,52,'交互',action_id='interact')
    content += action('drill',1284,580,52,'钻机',secondary='82%')
    content += action('jet',1302,425,52,'喷气')
    content += '<path d="M1267 493h70" stroke="#282A26" stroke-width="5"/><path d="M1267 493h53" stroke="#8EB7AA" stroke-width="5"/>'
    content += action('bomb',1450,403,52,'炸弹',secondary='3 / 3')
    content += action('pick',1448,580,76,'挖矿',GOLD,CREAM,True,'mine')
    return content

def menu_button(x, y, width, height, name, label, target, fill=INK, foreground=CREAM):
    return f'<g class="action" data-action="{target}" tabindex="0" role="button" aria-label="{label}">'+cut(x,y,width,height,fill,extra='class="surface"')+icon(name,x+width/2-26,y+height/2-26,52,foreground)+'</g>'

def pause():
    content = scene()+f'<path fill="{INK}" opacity=".58" d="M0 0h1600v720H0Z"/>'
    content += cut(544,80,512,560,CREAM,corner=26)
    content += '<g class="action" data-action="resume" tabindex="0" role="button" aria-label="关闭暂停菜单"><rect x="960" y="88" width="88" height="88" fill="transparent"/>'+icon('close',984,112,40,INK)+'</g>'
    content += menu_button(592,184,416,104,'play','继续挖掘','resume',GOLD,INK)
    content += menu_button(592,320,96,96,'sound','声音与震动','sound')
    content += menu_button(752,320,96,96,'sliders','操作设置','controls')
    content += menu_button(912,320,96,96,'help','操作说明','help')
    content += menu_button(592,456,416,104,'door','返回主菜单','quit','#E8DFC9',INK)
    return content

def upgrade():
    content = scene()+f'<path fill="{INK}" opacity=".58" d="M0 0h1600v720H0Z"/>'
    content += cut(544,80,512,560,CREAM,corner=26)
    content += '<g class="action" data-action="resume" tabindex="0" role="button" aria-label="关闭升级预览"><rect x="960" y="88" width="88" height="88" fill="transparent"/>'+icon('close',984,112,40,INK)+'</g>'
    content += cut(592,172,144,144,INK)+icon('pick',615,196,96,GOLD)
    content += text(884,261,'2 → 3',44,INK,700,'middle')
    content += icon('gem',604,357,42,'#BA8257')+text(984,392,'12 / 8',28,INK,700,'end')
    content += icon('gem',604,427,42,'#898D85')+text(984,462,'8 / 4',28,INK,700,'end')
    content += menu_button(592,504,416,96,'up','升级镐子','purchase',GOLD,INK)
    return content

def components():
    content = f'<path fill="{CREAM}" d="M0 0h1600v900H0Z"/>'
    cards = [
        (64,64,'电脑房','cursor','点击弹窗','back','退出电脑'),
        (820,64,'盲人房','knock','敲地','hand','交互'),
        (64,344,'思维房','thought','右侧思维','edit','编辑'),
        (820,344,'卡牌房与收集','check','确认选择','hand','拾取'),
    ]
    for pos_x,pos_y,title,first,label,second,label2 in cards:
        content += f'<g role="group" aria-label="{title}">'+cut(pos_x,pos_y,716,240,'#DDD5C2')
        content += cut(pos_x+24,pos_y+24,668,192,'#45493F')
        content += action(first,pos_x+516,pos_y+120,52,label,GOLD,GOLD,True,secondary=None)
        content += action(second,pos_x+350,pos_y+120,52,label2)
        if first == 'thought':
            content += action('thought',pos_x+184,pos_y+120,52,'左侧思维')
        content += '</g>'
    for center,fill,stroke,label in [(560,GOLD,GOLD,'待机'),(800,'#CF9D40',INK,'按下'),(1040,'#D6D1C3','#AAA99A','不可用')]:
        content += action('pick',center,740,76,label,fill,stroke,fill==GOLD)
    return content

screens = {'hud': svg(hud(),'挖哦！移动端探索 HUD 概念'), 'pause': svg(pause(),'挖哦！移动端暂停菜单概念'), 'upgrade': svg(upgrade(),'挖哦！移动端升级面板概念'), 'contexts': svg(components(),'挖哦！移动端情境按钮与状态',900)}
for name, content in screens.items():
    (ROOT / f'{name}.svg').write_text(content,encoding='utf-8')
(ROOT / 'icons').mkdir(exist_ok=True)
for name, shape in ICONS.items():
    (ROOT / 'icons' / f'{name}.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64"><g fill="none" stroke="{CREAM}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">{shape}</g></svg>',encoding='utf-8')

HTML = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>挖哦！移动 UI · 方向 01</title><style>
*{box-sizing:border-box}body{margin:0;background:#F4EBD6;color:#282A26;font-family:'Microsoft YaHei',sans-serif}header{max-width:1664px;margin:auto;padding:18px 32px;display:flex;justify-content:flex-end}nav{display:flex;gap:8px}button{font:inherit;cursor:pointer;color:inherit}nav button{width:48px;height:48px;display:grid;place-items:center;border:1px solid #C6C0AE;background:transparent;border-radius:4px}nav button.selected{background:#282A26;color:#F4EBD6;border-color:#282A26}.ui-icon{width:28px;height:28px;display:block}main{max-width:1664px;margin:auto;padding:0 32px 32px}#stage{background:#282A26;overflow:hidden;line-height:0;border-radius:4px}#stage>svg{display:block;width:100%;height:auto;touch-action:none}#toast{position:fixed;left:50%;bottom:30px;transform:translateX(-50%);background:#282A26;color:#E9BB58;border:1px solid #E9BB58;padding:14px 24px;border-radius:5px;line-height:1.6;display:none;z-index:4;font-size:28px}dialog{border:0;background:#F4EBD6;padding:30px;border-radius:4px;max-width:420px;width:90vw;color:#282A26}dialog::backdrop{background:#151A17B0}dialog label{display:flex;align-items:center;gap:20px;margin:22px 0;min-height:48px}dialog input[type=range]{flex:1;min-width:0;accent-color:#B07F25}dialog input[type=checkbox]{width:24px;height:24px;accent-color:#B07F25}dialog button{width:64px;height:56px;display:inline-grid;place-items:center;background:#E9BB58;border:0;margin-top:20px}#dialog-close{float:right}button:focus-visible{outline:3px solid #AA7931;outline-offset:3px}@media(max-width:800px){header{padding:12px}main{padding:0 12px 12px}}
</style><header><nav aria-label="预览页面"><button data-screen="hud" class="selected" aria-label="探索 HUD">__ICON_PICK__</button><button data-screen="pause" aria-label="暂停菜单">__ICON_PAUSE__</button><button data-screen="upgrade" aria-label="升级面板">__ICON_UP__</button><button data-screen="contexts" aria-label="情境与状态">__ICON_SLIDERS__</button></nav></header><main><div id="stage"></div></main><div id="toast" role="status"></div><dialog id="dialog"><div id="dialog-body"></div><button id="dialog-close" aria-label="返回预览">__ICON_BACK__</button></dialog><script>
const screens=__SCREENS__;
const stage=document.querySelector('#stage');
const toast=document.querySelector('#toast');
let current='hud',timer,held=null;
const preferences={sensitivity:55,sound:75,vibration:true};
function show(name){held=null;current=name;stage.innerHTML=screens[name];document.querySelectorAll('[data-screen]').forEach(button=>button.classList.toggle('selected',button.dataset.screen===name));}
function notify(message){toast.textContent=message;toast.style.display='block';clearTimeout(timer);timer=setTimeout(()=>toast.style.display='none',3200);}
function openDialog(title,content){document.querySelector('#dialog').setAttribute('aria-label',title);document.querySelector('#dialog-body').innerHTML=content;document.querySelector('#dialog').showModal();}
function activate(action){
 if(action==='pause')show('pause');
 else if(action==='resume')show('hud');
 else if(action==='upgrade')show('upgrade');
 else if(action==='help')show('contexts');
 else if(action==='sound')openDialog('声音与震动',`<label>__ICON_SOUND__<input aria-label="音量" data-pref="sound" type="range" value="${preferences.sound}"></label><label><svg class="ui-icon" viewBox="0 0 64 64" aria-hidden="true"><g fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round"><rect x="22" y="12" width="20" height="40" rx="3"/><path d="m13 20-6 12 6 12m38-24 6 12-6 12"/></g></svg><input aria-label="震动反馈" data-pref="vibration" type="checkbox" ${preferences.vibration?'checked':''}></label>`);
 else if(action==='controls')openDialog('操作设置',`<label>__ICON_SLIDERS__<input aria-label="视角灵敏度" data-pref="sensitivity" type="range" min="10" max="100" value="${preferences.sensitivity}"></label>`);
 else if(action==='quit')openDialog('离开当前探索？','__ICON_DOOR__<button id="quit-confirm" aria-label="确认离开">__ICON_CHECK__</button>');
 else notify('✓');
}
document.querySelectorAll('[data-screen]').forEach(button=>button.onclick=()=>show(button.dataset.screen));
document.querySelector('#dialog-close').onclick=()=>document.querySelector('#dialog').close();
document.querySelector('#dialog-body').addEventListener('click',event=>{if(event.target.closest('#quit-confirm')){document.querySelector('#dialog').close();show('hud');}});
document.querySelector('#dialog-body').addEventListener('input',event=>{if(event.target.dataset.pref)preferences[event.target.dataset.pref]=event.target.type==='checkbox'?event.target.checked:Number(event.target.value);});
stage.addEventListener('pointerdown',event=>{const target=event.target.closest('[data-action]');if(!target)return;const action=target.dataset.action;if(current==='hud'&&['mine','drill','jet'].includes(action)){event.preventDefault();held={pointer:event.pointerId,x:event.clientX,y:event.clientY,target};target.classList.add('held');stage.setPointerCapture(event.pointerId);}});
stage.addEventListener('pointermove',event=>{if(!held||held.pointer!==event.pointerId)return;const scene=stage.querySelector('.pan-scene');if(scene){const scale=1600/stage.clientWidth;const gain=preferences.sensitivity/100;const deltaX=Math.max(-44,Math.min(44,(event.clientX-held.x)*scale*gain));const deltaY=Math.max(-28,Math.min(28,(event.clientY-held.y)*scale*gain));scene.setAttribute('transform',`translate(${deltaX} ${deltaY})`);}});
function release(event){if(!held||held.pointer!==event.pointerId)return;held.target.classList.remove('held');held=null;}
stage.addEventListener('pointerup',release);stage.addEventListener('pointercancel',release);stage.addEventListener('lostpointercapture',release);
stage.addEventListener('click',event=>{const target=event.target.closest('[data-action]');if(!target)return;if(['mine','drill','jet'].includes(target.dataset.action)&&current==='hud')return;activate(target.dataset.action);});
stage.addEventListener('keydown',event=>{if(event.key==='Enter'||event.key===' '){const target=event.target.closest('[data-action]');if(target){event.preventDefault();activate(target.dataset.action);}}});
window.addEventListener('keydown',event=>{if(event.key==='Escape'&&!document.querySelector('#dialog').open)show(current==='pause'?'hud':'pause');});
window.addEventListener('blur',()=>{if(held){held.target.classList.remove('held');held=null;}});
show('hud');
</script></html>'''
for name, shape in ICONS.items():
    markup = f'<svg class="ui-icon" viewBox="0 0 64 64" aria-hidden="true"><g fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">{shape}</g></svg>'
    HTML = HTML.replace(f'__ICON_{name.upper()}__',markup)
(ROOT / 'preview.html').write_text(HTML.replace('__SCREENS__',json.dumps(screens,ensure_ascii=False)),encoding='utf-8')
print(f'Generated {len(screens)} screens, {len(ICONS)} icons and preview.html in {ROOT}')

if '--game-assets' in sys.argv:
    destination = ROOT.parents[2] / 'Content' / 'UI' / 'Mobile'
    destination.mkdir(parents=True, exist_ok=True)
    for name, shape in ICONS.items():
        artwork = shape.replace('currentColor', '#FFFFFF')
        (destination / f'{name}.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 64 64"><g fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">{artwork}</g></svg>', encoding='utf-8')
    extra = {
        'plate': '<path fill="#FFFFFF" d="M28 1H72L99 28V72L72 99H28L1 72V28Z"/>',
        'rim': '<path fill="none" stroke="#FFFFFF" stroke-width="1.8" d="M28 1H72L99 28V72L72 99H28L1 72V28Z"/>',
        'panel': '<path fill="#FFFFFF" d="M4 0H100V96L96 100H0V4Z"/>',
        'left': '<path fill="none" stroke="#FFFFFF" stroke-width="6" stroke-linecap="round" stroke-linejoin="round" d="m47 26-24 24 24 24M24 50h55"/>',
        'right': '<path fill="none" stroke="#FFFFFF" stroke-width="6" stroke-linecap="round" stroke-linejoin="round" d="m53 26 24 24-24 24M76 50H21"/>',
        'vibration': '<g fill="none" stroke="#FFFFFF" stroke-width="6" stroke-linecap="round"><rect x="34" y="18" width="32" height="64" rx="5"/><path d="m20 32-10 18 10 18m60-36 10 18-10 18"/></g>',
        'coin': '<g fill="none" stroke="#FFFFFF" stroke-width="5"><circle cx="50" cy="50" r="35"/><path d="m50 27 16 23-16 23-16-23Z"/></g>',
        'music': '<path fill="none" stroke="#FFFFFF" stroke-width="6" stroke-linecap="round" d="M35 73V24l44-9v49M35 29l44-9"/><ellipse fill="#FFFFFF" cx="24" cy="73" rx="14" ry="10"/><ellipse fill="#FFFFFF" cx="68" cy="64" rx="14" ry="10"/>',
        'voice': '<g fill="none" stroke="#FFFFFF" stroke-width="6" stroke-linecap="round"><rect x="38" y="14" width="24" height="43" rx="12"/><path d="M25 43a25 25 0 0 0 50 0M50 69v17M35 86h30"/></g>',
        'ambient': '<path fill="none" stroke="#FFFFFF" stroke-width="5" stroke-linecap="round" d="M12 35h49q24 0 19-16-4-9-14-3M12 50h65q18 0 14 15-5 10-15 2M12 65h30q20 0 15 15-4 9-13 2"/>',
    }
    for name, artwork in extra.items():
        (destination / f'{name}.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="192" height="192" viewBox="0 0 100 100">{artwork}</svg>',encoding='utf-8')
