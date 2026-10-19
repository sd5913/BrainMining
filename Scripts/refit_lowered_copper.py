"""Reproject the two lowered starter copper veins onto solid rock and rebuild their clues."""
import unreal,json,shutil,time
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/PackagePlaytest'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
field=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField))
placements=list(field.get_editor_property('authored_placements'));changed=[]
for i in [4,6]:
    p=placements[i];surface=p.surface.translation
    normal=unreal.MathLibrary.get_forward_vector(p.surface.rotation.rotator())
    endpoint=surface-normal*900
    wall=unreal.MiningDesignerLibrary.find_campaign_wall(p.hosts[0],surface+normal*500,endpoint)
    assert unreal.MathLibrary.vector_distance(wall,endpoint)>1
    t=p.surface;t.translation=wall;p.set_editor_property('surface',t)
    t=p.transform;t.translation=wall-normal*240;p.set_editor_property('transform',t)
    p.set_editor_property('fragments',[]);p.set_editor_property('fragment_supports',[])
    changed.append(p)
field.set_editor_property('authored_placements',changed)
assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(field)
report=json.loads(unreal.MiningDesignerLibrary.validate_campaign_minerals(field))
assert report['ok'],report['errors']
new=list(field.get_editor_property('authored_placements'))
for index,p in zip([4,6],new):placements[index]=p
field.set_editor_property('authored_placements',placements)
backup=root/'Saved/Backups'/('LoweredCopper_'+time.strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
assert levels.save_current_level()
report['positions']=[dict(core=list(p.transform.translation.to_tuple()),surface=list(p.surface.translation.to_tuple())) for p in new]
(out/'copper-refit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
