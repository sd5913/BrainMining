import json
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
mirrors=[]
for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorExhibit):
    if actor.get_editor_property('kind') in (10,11):
        capture=actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        target=capture.get_editor_property('texture_target')
        hidden=capture.get_editor_property('hidden_actors')
        mirrors.append({'kind':actor.get_editor_property('kind'),'width':target.get_editor_property('size_x'),'height':target.get_editor_property('size_y'),'miner_hidden':pawn in hidden})
front=next(m for m in mirrors if m['kind']==10)
assert len(mirrors)==1 and front['kind']==10,mirrors
assert front['miner_hidden'],front
assert front['width']==24 and front['height']==36,front
labels=[a.get_actor_label() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)]
assert not any(s.startswith(('StatueGlass_','StatueGlassBack_','StatueGlassFront_','StatueShowcaseTop_','ShowcaseFrame_')) for s in labels)
assert any(s.startswith('StatuePlinth_') for s in labels)
result={'passed':True,'mirrors':mirrors,'statue_enclosure_removed':True,'open_plinth_retained':True}
out=Path(unreal.Paths.project_saved_dir())/'Validation/WenSiMirror/polish_visual_checks.json'
out.write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('WENSI_POLISH_VISUAL_PASS')
