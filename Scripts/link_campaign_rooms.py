"""Replace baked gameplay-room copies with always-loaded references to their source maps.

Only the campaign map is saved. Source maps and the authored mining terrain are untouched.
"""
import hashlib, json, re, shutil
from datetime import datetime
from pathlib import Path
import unreal


def link(world, hall):
    root=Path(unreal.Paths.project_dir())
    recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
    subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    areas=list(hall.get_editor_property('areas'))
    for index in (1,3,5,6):
        area=areas[index]
        if area.get_editor_property('source_level'):
            continue
        # Generated tunnel soil, doorway adapters, progress gates and signs stay in the host map.
        copies=[a for a in subsystem.get_all_level_actors()
                if a.actor_has_tag(area.tag) and str(a.get_folder_path()).startswith('Experiences/')
                and a!=hall]
        for actor in copies:
            subsystem.destroy_actor(actor)
        # Older builds inserted this orchestrator when the source had none; InitGame now supplies it.
        for actor in subsystem.get_all_level_actors():
            if actor.actor_has_tag(area.tag) and actor.get_actor_label()=='Campaign_JourneyEscape':
                subsystem.destroy_actor(actor)
        before=set(subsystem.get_all_level_actors())
        source=recipe['stages'][index]['source']
        stream=unreal.EditorLevelUtils.add_level_to_world_with_transform(
            world,source,unreal.LevelStreamingAlwaysLoaded,unreal.Transform(location=area.origin))
        assert stream,source
        imported=[a for a in subsystem.get_all_level_actors() if a not in before]
        assert imported,source
        area.set_editor_property('source_level',stream)
        area.set_editor_property('suppressed_source_actors',[
            a.get_name() for a in imported if index==5 and re.fullmatch(r'(BackWall|EndWall)\d*',a.get_actor_label())])
        unreal.log('CAMPAIGN_ROOM_REFERENCE '+source+' actors='+str(len(imported)))
    hall.set_editor_property('areas',areas)


if __name__=='__main__':
    root=Path(unreal.Paths.project_dir())
    recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
    backup=root/'Saved/CampaignBackups'/('room-references-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(root/'Content'/(recipe['map'][6:]+'.umap'),backup/'Previous.umap')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(recipe['map'])
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    hall=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if isinstance(a,unreal.MiningExhibitHall))
    source_hashes={s['source']:hashlib.sha256((root/'Content'/(s['source'][6:]+'.umap')).read_bytes()).hexdigest() for s in recipe['stages']}
    link(world,hall)
    assert unreal.MiningDesignerLibrary.build_crystal_clue_mesh()
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor,unreal.MiningTreasureField) and actor.get_editor_property('use_authored_placements'):
            actor.set_editor_property('crystal_clue_mesh',unreal.load_asset('/Game/Showcase/Treasure/Meshes/SM_CrystalClue'))
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,recipe['map'])
    assert all(hashlib.sha256((root/'Content'/(name[6:]+'.umap')).read_bytes()).hexdigest()==digest for name,digest in source_hashes.items())
    (root/'Saved/CampaignRoomReferences.json').write_text(json.dumps(source_hashes,indent=2),encoding='utf-8')
    unreal.log('CAMPAIGN_ROOM_REFERENCES_SAVED')
    unreal.SystemLibrary.quit_editor()
