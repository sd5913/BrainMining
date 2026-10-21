"""Keep the victory instruction separate from the post-pickup narration."""
import unreal

def segment(name,key,text):
    path='/Game/Subtitles/DA_'+name
    s=unreal.load_asset(path)
    if not s:
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.SubtitleSegment)
        s=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_'+name,'/Game/Subtitles',unreal.SubtitleSegment,factory)
    s.set_editor_property('segment_id',key)
    line=unreal.SubtitleLine();line.set_editor_property('text',text)
    s.set_editor_property('lines',[line])
    assert unreal.EditorAssetLibrary.save_loaded_asset(s,False)

segment('CardsVictory','卡牌胜利提示','*恭喜你获得钻机！按V可切换使用*')
segment('Cards2','卡牌玩法2','这会不会太常见了呢？')
segment('RisingWater','水涌玩法','上涌的水...不断地往上跳...')
unreal.SystemLibrary.quit_editor()
