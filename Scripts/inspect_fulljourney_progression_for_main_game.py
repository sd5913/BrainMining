"""Read the saved FullJourney world as an asset; don't activate its rooms or terrain."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/main_game_fulljourney_reference.json'
def cls_info(path):
    cls=unreal.load_class(None,path);obj=unreal.get_default_object(cls)
    return obj
try:
    source=unreal.load_asset('/Game/Campaign/Maps/Lvl_FullJourney');assert source
    objects=list(unreal.GameplayStatics.get_all_actors_of_class(source,unreal.Actor))
    hall=next(a for a in objects if isinstance(a,unreal.MiningExhibitHall))
    field=next(a for a in objects if isinstance(a,unreal.MiningTreasureField))
    mode=source.get_world_settings().default_game_mode;defaults=unreal.get_default_object(mode)
    character=unreal.get_default_object(defaults.default_pawn_class);progression=character.get_component_by_class(unreal.MiningProgressionComponent)
    report=dict(passed=True,map=source.get_path_name(),actors=len(objects),game_mode=mode.get_path_name(),
        player=defaults.default_pawn_class.get_path_name(),controller=defaults.player_controller_class.get_path_name(),
        hall=hall.get_path_name(),linear=hall.linear_campaign,continuous=hall.continuous_campaign,
        areas=[dict(tag=str(a.tag),kind=str(a.kind),route_points=len(a.map_route)) for a in hall.areas],
        field=field.get_path_name(),authored=field.get_editor_property('use_authored_placements'),shared_wallet=field.get_editor_property('use_shared_wallet'),
        deposits=len(field.get_editor_property('authored_placements')),definitions=[dict(id=str(d.id),model=d.model.get_path_name(),values=list(d.values_by_size)) for d in field.definitions],
        treasure_material=field.treasure_material.get_path_name(),clue_mesh=field.get_editor_property('crystal_clue_mesh').get_path_name(),
        upgrade_tracks=[dict(name=str(t.name),prices=[s.price for s in t.steps],effects=[str(s.effect) for s in t.steps]) for t in progression.tracks])
    # Use the same source objects during the later editor integration (no source writes).
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('FULLJOURNEY_REFERENCE '+json.dumps(report,ensure_ascii=False))
except Exception:OUT.write_text(json.dumps(dict(passed=False,error=traceback.format_exc()),ensure_ascii=False,indent=2),encoding='utf-8');raise
