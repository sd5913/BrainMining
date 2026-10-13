import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
assert levels.save_current_level()
backup=root/'Saved/Backups/MainGame_BeforeSmoothRockRoughGuides';backup.mkdir(parents=True,exist_ok=True)
for relative in ['Content/MainGame/Maps/Lvl_MainGame.umap','Content/MainGame/Materials']:
    source=root/relative;destination=backup/source.name
    if not destination.exists():
        if source.is_dir():shutil.copytree(source,destination)
        else:shutil.copy2(source,destination)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
lights=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in actor.get_components_by_class(unreal.LightComponent):
        lights.append(dict(actor=actor.get_path_name(),label=actor.get_actor_label(),type=c.get_class().get_name(),
            intensity=c.intensity,color=str(c.light_color),cast_shadows=c.cast_shadows,
            location=list(actor.get_actor_location().to_tuple())))
regions=[]
for r in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion):
    regions.append(dict(actor=r.get_path_name(),label=r.get_actor_label(),tags=[str(t) for t in r.tags],
        rock=r.generated_rock.get_path_name(),rock_triangles=r.generated_rock.triangle_count,
        shell=r.generated_shell.get_path_name(),rock_material=r.rock_material.get_path_name(),
        guides=[dict(name=g.get_actor_label(),actor=g.get_path_name(),triangles=g.triangle_count,material=g.rock_material.get_path_name()) for g in r.generated_guide_layers],
        meshes=[l.mesh.get_path_name() for l in r.guide_layers]))
report=dict(map=world.get_path_name(),regions=regions,lights=lights,backup=str(backup))
(root/'Saved/Validation/main_game_surface_revision_before.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('SURFACE_REVISION_INSPECT '+json.dumps(dict(regions=len(regions),guides=sum(len(r['guides']) for r in regions),lights=lights),ensure_ascii=False))
