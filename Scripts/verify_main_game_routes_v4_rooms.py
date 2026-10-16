"""Verify natural soil interfaces and the unchanged source-room workflows."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();out=ROOT/'Saved/Validation/MainGameRoutesV4/LinkedRooms';out.mkdir(exist_ok=True)
code=(ROOT/'Scripts/verify_main_game_linked_rooms.py').read_text(encoding='utf-8').replace("OUT=ROOT/'Saved/Validation/MainGameRooms'","OUT=ROOT/'Saved/Validation/MainGameRoutesV4/LinkedRooms'")
exec(compile(code,'lowpoly_room_interfaces','exec'),{'__name__':'__main__'})
