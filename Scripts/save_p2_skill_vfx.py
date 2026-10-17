import unreal
for key in ['ArcImpact','Poison','Ignition','Finale','Spark','Confetti']:
 asset=unreal.load_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_'+key)
 assert asset and unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False),key
unreal.log('P2_SKILL_VFX_ASSETS_SAVED')
exec(compile(open('E:/BrainMining_CodexBuild/Scripts/save_p2_before_restart.py',encoding='utf-8').read(),'save_p2_before_restart.py','exec'))
