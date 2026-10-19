"""Give the terminal landing caps comfortable jetpack headroom below the soil."""
import unreal,sys,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
from gameplay_feedback_authoring import *
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
aa=list(actors.get_all_level_actors());path=ROOT/'Saved/Validation/GameplayFeedback/authoring.json';report=json.loads(path.read_text(encoding='utf-8'))
mat=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_ParkourBasalt')
for i in range(2):
    cap=next(a for a in aa if a.get_name()=='GeneratedMiningRock_'+str(25+i));lo,hi=bounds(cap)
    a=next(a for a in aa if a.get_actor_label()=='MAIN_GAME_Parkour_DigPlatform_%d'%(i+1))
    top=V([(lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z-450])
    if i==1:top.z-=50
    base=V([top.x,top.y,5110 if i==0 else 7510])
    report['platforms'][10+i]=platform(a,base,top,140,751+i,mat,'SM_RockMushroom_Dig_%d'%(i+1))
assert levels.save_current_level();report['terminal_headroom']=[450,500]
path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
