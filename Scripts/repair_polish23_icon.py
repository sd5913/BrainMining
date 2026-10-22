import unreal
m=unreal.load_asset('/Game/MainGame/Materials/M_BombWallIcon');ml=unreal.MaterialEditingLibrary
nodes=ml.get_material_expressions(m)
t=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionTextureSample));inv=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionOneMinus))
assert ml.connect_material_expressions(t,'A',inv,'')
ml.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
unreal.SystemLibrary.quit_editor()
