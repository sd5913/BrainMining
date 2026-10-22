import unreal
bp=unreal.load_asset('/Game/EndingEmergence/UI/WBP_EndingCredits')
found=0
for w in unreal.ObjectIterator(unreal.TextBlock):
 if w.get_path_name().startswith('/Game/EndingEmergence/UI/WBP_EndingCredits.') and w.get_name()=='ThankYou':
  w.set_text('恭喜你挖出来了！《挖哦》也因此诞生！感谢游玩！');found+=1
assert found>0,'Credits label missing'
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('POLISH23 credits labels saved '+str(found))
unreal.SystemLibrary.quit_editor()
