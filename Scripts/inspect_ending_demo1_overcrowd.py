import unreal
import json

MAP = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo1'
OUT = 'E:/涌现taptap2026/BrainMining/Saved/Validation/EndingDemo1OverCrowd.json'

def obj_path(o):
    try:
        return o.get_path_name() if o else None
    except Exception:
        return None

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows = []
for a in actors:
    c = a.get_class()
    n = c.get_name()
    if any(k in n.lower() for k in ('crowd','ending','populate','niagara')) or 'Ending' in a.get_name():
        row = {'name': a.get_name(), 'class': n, 'path': obj_path(a), 'loc': [a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z]}
        for p in ('crowd_assets','CrowdAssets','CrowdEmitter','CrowdDataTexture','SortedBodyParts','PlacementGroup','SkeletalMesh','StaticMesh','CrowdCount','NiagaraSystem'):
            try:
                v = a.get_editor_property(p)
                if isinstance(v, unreal.Object):
                    v = obj_path(v)
                elif isinstance(v, unreal.Array):
                    v = [obj_path(x) if isinstance(x, unreal.Object) else str(x) for x in v]
                elif isinstance(v, unreal.Map):
                    v = str(v)
                else:
                    v = str(v)
                row[p] = v
            except Exception:
                pass
        comps = []
        for comp in a.get_components_by_class(unreal.SceneComponent):
            comps.append({'name': comp.get_name(), 'class': comp.get_class().get_name(), 'path': obj_path(comp)})
        row['components'] = comps
        rows.append(row)

data = {'map': MAP, 'actors': rows, 'all_actor_count': len(actors)}
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
print('ENDING_DEMO1_OVERCROWD_INSPECT', json.dumps(data, ensure_ascii=False))

