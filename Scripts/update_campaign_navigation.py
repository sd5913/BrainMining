"""Bake map routes into the existing campaign without rebuilding its terrain."""
import bisect
import json
import math
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(recipe['map'])
all_actors=actors.get_all_level_actors()
hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall))
areas=list(hall.areas)
points=json.loads((root/recipe['mine_centerlines']).read_text())[0]['points']
arcs=[0.0]
for a,b in zip(points,points[1:]): arcs.append(arcs[-1]+math.dist(a,b))
for i,spec in enumerate(recipe['stages']):
    if spec['kind']!='MINING': continue
    region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_'+spec['id']))
    def position(distance):
        j=max(1,min(len(points)-1,bisect.bisect_left(arcs,distance)))
        t=(distance-arcs[j-1])/(arcs[j]-arcs[j-1])
        p=[a+(b-a)*t for a,b in zip(points[j-1],points[j])]
        return unreal.MathLibrary.transform_location(region.get_actor_transform(),unreal.MathLibrary.transform_location(region.cutters[0].local_transform,unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)))
    start,end=spec['start_arc_m'],spec['end_arc_m']
    n=math.ceil((end-start)/2)
    path=[position(start+(end-start)*j/n) for j in range(n+1)]
    if recipe.get('continuous'):path=[areas[i].entry.translation+unreal.Vector(0,0,24)]+path+[areas[i].exit]
    areas[i].set_editor_property('map_route',path)
hall.set_editor_property('areas',areas)
backup=root/'Saved/CampaignBackups'/('navigation-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
shutil.copy2(root/'Content'/(recipe['map'][6:]+'.umap'),backup/'Lvl_FullJourney.umap')
assert levels.save_current_level()
unreal.log('CAMPAIGN_NAVIGATION_SAVED '+str([len(a.map_route) for a in areas]))
unreal.SystemLibrary.quit_editor()
