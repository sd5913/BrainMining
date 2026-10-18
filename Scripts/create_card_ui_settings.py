import unreal
folder='/Game/Rooms/CardDuel/UI'; path=folder+'/DA_CardRoomUISettings'; cls=unreal.load_class(None,'/Script/CardRoom.CardRoomUISettings')
data=unreal.load_asset(path)
if not data:
 factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
 data=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_CardRoomUISettings',folder,cls,factory)
assert data
for prop,name in [('round_art','T_RoundHeader'),('speech_bubble_art','T_SpeechBubble'),('player_health_art','T_Health_Player'),('boss_health_art','T_Health_Boss'),('card_laser_art','T_Card_Laser'),('card_poison_art','T_Card_Poison'),('card_fire_art','T_Card_Fire')]: data.set_editor_property(prop,unreal.load_asset(folder+'/'+name))
def box(prop,pos,size):
 b=unreal.CardRoomUIBox(); b.set_editor_property('position',unreal.Vector2D(*pos)); b.set_editor_property('size',unreal.Vector2D(*size)); data.set_editor_property(prop,b)
box('round',(48,103),(370,180));box('player_health',(46,205),(430,67));box('boss_health',(1120,205),(430,67));box('dialogue',(1018,283),(500,120));box('seated_panel',(465,625),(670,85));box('prompt',(48,596),(910,55));box('confirm',(1100,590),(430,50));box('select_all',(1100,538),(430,42));box('victory',(48,610),(870,160));box('bottom_hint',(48,850),(430,40));box('poison',(1355,220),(217,36))
for prop,val in [('card_normal_size',unreal.Vector2D(138,200)),('card_many_size',unreal.Vector2D(64,96)),('card_normal_step',160.0),('card_many_step',75.0),('card_top',672.0),('card_selected_lift',30.0),('card_hand_center_x',800.0)]: data.set_editor_property(prop,val)
unreal.EditorAssetLibrary.save_loaded_asset(data); print('CARD_UI_SETTINGS_CREATED',data.get_path_name())
