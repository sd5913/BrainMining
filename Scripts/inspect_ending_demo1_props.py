import unreal, json
MAP='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo1'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
demo=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
out={}
for p in ['b_editable_scenery','city_building_meshes','city_prop_meshes','crowd_assets','city','sets']:
    try:
        v=demo.get_editor_property(p)
        if isinstance(v,(list,tuple)): v=[x.get_path_name() if isinstance(x,unreal.Object) else str(x) for x in v]
        elif isinstance(v,unreal.Object): v=v.get_path_name()
        else: v=str(v)
        out[p]=v
    except Exception as e: out[p]=repr(e)
with open('E:/涌现taptap2026/BrainMining/Saved/Validation/EndingDemo1Props.json','w',encoding='utf-8') as f: json.dump(out,f,ensure_ascii=False,indent=2)
print('ENDING_DEMO1_PROPS',out)

