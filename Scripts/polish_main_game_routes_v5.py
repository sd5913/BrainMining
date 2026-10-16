"""Apply repeatable lighting and original neural-detail finishing to Routes V5."""
import unreal, json, traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()

def apply():
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors=list(actors.get_all_level_actors())
    hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall))
    assert hall.actor_has_tag('BM_RoutesV5')
    lights=[]
    for a in all_actors:
        if a.get_actor_label().startswith('MAIN_GAME_CrystalLight_'):
            n=int(a.get_actor_label().rsplit('_',1)[1])
            c=a.point_light_component
            c.set_intensity(180.)
            c.set_light_color(unreal.LinearColor(.42,.38,.8) if n%3 else unreal.LinearColor(.24,.55,.7))
            lights.append(a.get_actor_label())
    details=[]
    mat=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_DarkRed')
    for i in range(2):
        label=f'MAIN_GAME_NeuralDetail_{i}'
        a=next((a for a in all_actors if a.get_actor_label()==label),None)
        if a is None:
            a=actors.spawn_actor_from_class(unreal.GeneratedMiningRock,unreal.Vector(39750,11750,13900))
            a.set_actor_label(label)
        a.set_editor_property('tags',['BM_Campaign_MainGame03','BM_RoutesV5','BM_NeuralDetail'])
        a.set_editor_property('rock_material',mat)
        a.set_editor_property('hardness_level',2)
        assert unreal.MiningDesignerLibrary.import_campaign_rock(a,str(ROOT/f'Art/MainGame/RoutesV5/TunnelStalagmiteV3/NeuralDetail_{i}.obj'))
        for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mat)
        assert a.triangle_count>100
        details.append({'actor':label,'triangles':a.triangle_count})
    return {'lights':len(lights),'intensity':180,'neural_details':details}

if __name__=='__main__':
    report={'passed':False}
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    try:
        assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
        assert levels.set_current_level_by_name('Lvl_MainGame')
        report.update(apply())
        assert levels.save_current_level()
        report['passed']=True
    except Exception:
        report['error']=traceback.format_exc()
        unreal.log_error(report['error'])
    finally:
        (ROOT/'Saved/Validation/MainGameRoutesV5/polish.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'QUIT_EDITOR')
