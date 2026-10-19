"""Edit each level in its own coordinates; preserve streamed-room transforms."""
import unreal,json,re
from pathlib import Path
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rows=[];changed=[]
for package in ['/Game/MainGame/Maps/Lvl_MainGame','/Game/StartRoom/Maps/Lvl_StartRoom']:
    assert levels.load_level(package)
    dirty=False
    for a in list(api.get_all_level_actors()):
        if not a.get_level().get_path_name().startswith(package+'.'):continue
        label=a.get_actor_label()
        if label=='STARTROOM_JetpackInstruction':
            sign=api.spawn_actor_from_class(unreal.MiningWallSign,a.get_actor_location(),a.get_actor_rotation())
            sign.set_editor_property('words','按住空格起飞');sign.set_actor_scale3d(unreal.Vector(.25,.25,.25))
            sign.set_editor_property('tags',a.tags);sign.set_actor_label('STARTROOM_JetpackInstruction_Chinese')
            sign.set_folder_path(a.get_folder_path());api.destroy_actor(a);dirty=True;rows.append({'label':label,'text':'按住空格起飞'});continue
        for c in a.get_components_by_class(unreal.TextRenderComponent):
            text=str(c.text).strip()
            if re.search(r'(?i)\b(MAIN|MINE ACCESS|ROOM|REGION|SHIFT)\b',text) and text.isascii():
                rows.append({'label':label,'removed':text,'package':package})
                c.set_text('');c.set_visibility(False);c.set_hidden_in_game(True);dirty=True
    if dirty:
        assert levels.save_current_level(),package;changed.append(package)
out=Path(unreal.Paths.project_saved_dir())/'Validation/CardUIFeedback'
(out/'corridor.json').write_text(json.dumps({'passed':True,'changed':rows,'packages':changed},ensure_ascii=False,indent=2),encoding='utf8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
