import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());checks=[]
def check(value,label):
    assert value,label
    checks.append(label)
bp=unreal.load_asset('/Game/Showcase/Blueprints/BP_SharedMiningCharacter')
cdo=unreal.get_default_object(bp.generated_class())
mining=cdo.get_component_by_class(unreal.MiningComponent)
check(mining.get_editor_property('bUseDrillEnergy'),'Saved main character uses energy')
parts={p.get_name():p for p in cdo.get_components_by_class(unreal.SceneComponent)}
bomb=parts['BombModel'];p=bomb.get_editor_property('RelativeLocation');scale=bomb.get_editor_property('RelativeScale3D')
check(abs(p.x-12.8696985)<.001 and abs(p.y+.037528)<.001 and abs(p.z+.032153)<.001,'User bomb position retained')
check(scale.x==2 and scale.y==2 and scale.z==2,'User bomb scale retained')
for tool in ['DrillRightModel','DrillLeftModel']:
    check(parts[tool].get_material(0).get_name()=='M_DrillStylized',tool+' retains drill material')
hud=unreal.load_asset('/Game/UI/MiningGUI/WBP_MiningHUD')
images={i.get_name():i for i in unreal.ObjectIterator(unreal.Image) if i.get_outer().get_outer()==hud}
check('DrillEnergyBar' in images,'Energy bar saved in editable HUD')
for name in ['ReticlePickaxe','ReticleDrill','ReticleBomb']:
    size=images[name].slot.get_size()
    check(size.x==100 and size.y==100,name+' user size retained')
bar=images['DrillEnergyBar'];a=bar.slot.get_anchors();size=bar.slot.get_size()
check(a.minimum.x==1 and a.minimum.y==.5 and size.x==56 and size.y==196,'Energy bar saved at screen right')
check(bar.brush.resource_object.get_name()=='M_DrillEnergy','Energy bar material saved')
settings=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.UserInterfaceSettings'))
cursors=settings.get_editor_property('HardwareCursors')
check(len(cursors)==2,'Default and hand cursor shapes configured')
for shape,cursor in cursors.items():
    check(str(cursor.get_editor_property('CursorPath'))=='Slate/Cursors/PickaxeCursor','Saved cursor '+str(shape))
warning=unreal.load_asset('/Game/PopupClose/PopupArt/T_PendingWarning')
check(warning is not None,'Warning artwork survives restart')
out=root/'Saved/Validation/DrillEnergy';out.mkdir(parents=True,exist_ok=True)
(out/'assets.json').write_text(json.dumps({'passed':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf8')
print('ENERGY_CURSOR_PERSISTENCE_PASS',len(checks))
