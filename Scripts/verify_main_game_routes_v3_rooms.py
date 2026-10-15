"""Original-room workflows and six natural interfaces after the V3 relocation."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();out=ROOT/'Saved/Validation/MainGameRoutesV3/LinkedRooms';out.mkdir(exist_ok=True)
code=(ROOT/'Scripts/verify_main_game_linked_rooms.py').read_text(encoding='utf-8').replace("OUT=ROOT/'Saved/Validation/MainGameRooms'","OUT=ROOT/'Saved/Validation/MainGameRoutesV3/LinkedRooms'")
exec(compile(code,'regional_linked_rooms_audit','exec'),{'__name__':'__main__'})
