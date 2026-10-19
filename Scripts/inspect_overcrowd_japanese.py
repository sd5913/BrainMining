import unreal, json
OUT='E:/涌现taptap2026/BrainMining/Saved/Validation/OverCrowdJapaneseAssets.json'
report={}
try:
    tags=[]
    unreal.OverCrowdRuntimeLibrary.get_available_wardrobe_tags(tags)
    report['wardrobe_tags']=[str(x) for x in tags]
except Exception as e: report['wardrobe_error']=repr(e)
try:
    ws=[]
    unreal.OverCrowdRuntimeLibrary.get_all_wardrobes(ws, [], [])
    report['wardrobes']=[]
    for w in ws:
        report['wardrobes'].append({'id':str(w.get_editor_property('wardrobe_id')),'compartments':str(w.get_editor_property('compartments')),'anim':str(w.get_editor_property('wardrobe_animation'))})
except Exception as e: report['wardrobes_error']=repr(e)
ar=unreal.AssetRegistryHelpers.get_asset_registry()
for path in ['/Game/JapaneseCity','/Game/EndingEmergence','/OverCrowd']:
    assets=ar.get_assets_by_path(path, recursive=True)
    report[path]=[]
    for d in assets:
        n=str(d.asset_name)
        try: c=str(d.asset_class)
        except: c=''
        if path.endswith('JapaneseCity') or any(k in n.lower() or k in c.lower() for k in ('wardrobe','vat','niagara','pcg','city')):
            report[path].append({'name':n,'class':c,'package':str(d.package_name)})
with open(OUT,'w',encoding='utf-8') as f: json.dump(report,f,ensure_ascii=False,indent=2)
print('OVERCROWD_JAPANESE_ASSET_INSPECT', json.dumps(report,ensure_ascii=False))

