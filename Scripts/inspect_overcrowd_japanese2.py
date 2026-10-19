import unreal, json
OUT='E:/涌现taptap2026/BrainMining/Saved/Validation/OverCrowdJapaneseAssets2.json'
report={}
try:
    report['wardrobe_tags']=[str(x) for x in unreal.OverCrowdRuntimeLibrary.get_available_wardrobe_tags()]
except Exception as e: report['wardrobe_error']=repr(e)
try:
    ws=unreal.OverCrowdRuntimeLibrary.get_all_wardrobes([], [])
    report['wardrobes']=[]
    for w in ws:
        report['wardrobes'].append({'id':str(w.get_editor_property('wardrobe_id')),'compartments':str(w.get_editor_property('compartments')),'anim':str(w.get_editor_property('wardrobe_animation'))})
except Exception as e: report['wardrobes_error']=repr(e)
ar=unreal.AssetRegistryHelpers.get_asset_registry()
report['japanese_assets']=[]
for d in ar.get_assets_by_path('/Game/JapaneseCity', recursive=True):
    n=str(d.asset_name)
    c=str(getattr(d,'asset_class_path',getattr(d,'asset_class','')))
    if any(k in n.lower() or k in c.lower() for k in ('pcg','city','demo','building','map')):
        report['japanese_assets'].append({'name':n,'class':c,'package':str(d.package_name)})
report['maps']={}
for map_path in ['/Game/JapaneseCity/Maps/Demo_day','/Game/JapaneseCity/Maps/Demo_night','/Game/JapaneseCity/Maps/Overview']:
    try:
        unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        rows=[]
        for a in actors:
            cn=a.get_class().get_name()
            if any(k in cn.lower() or k in a.get_name().lower() for k in ('pcg','city','worldpartition','foliage','instanced','levelinstance')):
                rows.append({'name':a.get_name(),'class':cn,'path':a.get_path_name()})
        report['maps'][map_path]={'count':len(actors),'interesting':rows}
    except Exception as e: report['maps'][map_path]={'error':repr(e)}
with open(OUT,'w',encoding='utf-8') as f: json.dump(report,f,ensure_ascii=False,indent=2)
print('OVERCROWD_JAPANESE_ASSET_INSPECT2', json.dumps(report,ensure_ascii=False))

