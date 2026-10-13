"""Restore original soil guides; reuse the fourth guide material on fourth-region ordinary walls."""
import unreal,json,shutil,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/main_game_mud_guides_stone_wall.json'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text())
def signature(r):
    rocks=[r.generated_rock]+list(r.generated_guide_layers)
    return dict(region=r.get_path_name(),rocks=[dict(path=g.get_path_name(),triangles=g.triangle_count,cuts=g.successful_cuts,chunks=len(g.chunks)) for g in rocks],
                cutters=[c.mesh.get_path_name() for c in r.cutters],guide_meshes=[g.mesh.get_path_name() for g in r.guide_layers],
                shell=r.generated_shell.get_path_name(),shell_material=r.shell_material.get_path_name())
def assign(rock,mat):
    rock.set_editor_property('rock_material',mat,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    for c in rock.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mat)
    assert rock.rock_material==mat and all(c.mesh.get_material(0)==mat for c in rock.chunks)
try:
    assert levels.save_current_level()
    backup=ROOT/'Saved/Backups/MainGame_BeforeMudGuideStoneWall';backup.mkdir(parents=True,exist_ok=True)
    if not (backup/'Lvl_MainGame.umap').exists():shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    mud=unreal.load_asset(recipe['guide_material']);wall=unreal.load_asset(recipe['folder']+'/'+recipe['themes'][3]['name']);assert mud and wall
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion));assert len(regions)==7
    before={r.get_path_name():signature(r) for r in regions}
    ordinary_before={r.get_path_name():r.rock_material for r in regions}
    guides=[g for r in regions for g in r.generated_guide_layers];assert len(guides)==64
    fourth=next(r for r in regions if 'BM_MainGame_NeuralV4' in [str(t) for t in r.tags])
    assert fourth.guide_material in [wall,mud],'Unexpected fourth guide material; inspect before swapping'
    for r in regions:
        r.set_editor_property('guide_material',mud,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        for g in r.generated_guide_layers:assign(g,mud)
    fourth.set_editor_property('rock_material',wall,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    assign(fourth.generated_rock,wall)
    for r in regions:
        assert signature(r)==before[r.get_path_name()],'Material assignment changed geometry'
        if r!=fourth:assert r.rock_material==ordinary_before[r.get_path_name()]
    assert levels.save_current_level()
    report=dict(passed=True,saved=True,map=world.get_path_name(),guide_count=len(guides),guide_material=mud.get_path_name(),
                fourth_region=fourth.get_actor_label(),fourth_wall_material=wall.get_path_name(),fourth_wall_components=len(fourth.generated_rock.get_components_by_class(unreal.DynamicMeshComponent)),
                geometry_cuts_shells_preserved=True,other_wall_materials_preserved=True,backup=str(backup))
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MUD_GUIDES_STONE_WALL '+json.dumps(report,ensure_ascii=False))
except Exception:OUT.write_text(json.dumps(dict(passed=False,error=traceback.format_exc()),ensure_ascii=False,indent=2),encoding='utf-8');raise
