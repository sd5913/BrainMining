"""Use the proven closed terrain-grid soil builder for real pickaxe booleans."""
import unreal,json,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.set_current_level_by_name('Lvl_MainGame')
cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());report=[]
for s in cfg['generated_connections']:
    a=next(a for a in actors if a.get_actor_label()=='LINK_'+s['id']+'_DiggableSoil');ring=geometry.cross(s,s['plug_x']);lo=[min(p[j] for p in ring) for j in range(3)];hi=[max(p[j] for p in ring) for j in range(3)]
    a.set_actor_location(unreal.Vector(s['plug_x'],(lo[1]+hi[1])/2,(lo[2]+hi[2])/2),False,True)
    assert unreal.MiningDesignerLibrary.build_campaign_soil(a,unreal.Vector(190,hi[1]-lo[1]+90,hi[2]-lo[2]+80),unreal.Vector(24,10,10));a.reset_rock();report.append({'id':s['id'],'triangles':a.triangle_count})
assert levels.save_current_level();Path(ROOT/'Saved/Validation/MainGameRoomEntrances/soil_refit.json').write_text(json.dumps(report,indent=2));unreal.log('ROOM_ENTRANCE_SOIL_REFIT_SAVED')
