"""UE: verify Blender's real OBJ round trip on a temporary diggable actor."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
folder=root/'Saved/ExchangeValidation'
data=json.loads((folder/'roundtrip.json').read_text(encoding='utf-8'))
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
source=next(a for a in actors.get_all_level_actors() if a.get_path_name()==data['actor'])
temporary=actors.spawn_actor_from_class(unreal.GeneratedMiningRock,source.get_actor_location(),source.get_actor_rotation())
try:
    temporary.set_actor_scale3d(source.get_actor_scale3d())
    assert unreal.MiningDesignerLibrary.import_campaign_rock(temporary,str(folder/'roundtrip.obj'))
    location,extent=temporary.get_actor_bounds(False)
    original,original_extent=source.get_actor_bounds(False)
    error=(location-original-unreal.Vector(12.5,25,37.5)).length()
    assert error<.1,(location,original,error)
    assert (extent-original_extent).length()<.1
    assert temporary.mine_sphere(location,75),'Round-tripped terrain must still support excavation'
    (folder/'ue_validation.json').write_text(json.dumps(dict(actor=data['actor'],bounds_error_cm=error,triangles=temporary.triangle_count,excavation=True),indent=2))
    unreal.log('CAMPAIGN_EXCHANGE_ROUNDTRIP_VERIFIED')
finally:
    actors.destroy_actor(temporary)
