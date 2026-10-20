"""Use only fine Niagara particles in both mushroom chambers; preserve the source effect.
Run with Run_Feedback11.ps1 -Script apply_mushroom_niagara.py -Render (GPU emitter)."""
import unreal,json
from pathlib import Path
EA=unreal.EditorAssetLibrary;H=unreal.MiningNiagaraEditorLibrary
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Playtest16'
path='/Game/MainGame/VFX/NS_MushroomCrystalMotes'
system=EA.load_asset(path) if EA.does_asset_exist(path) else EA.duplicate_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote',path)
assert system
objects={o.get_class().get_name():o for o in unreal.ObjectIterator(unreal.Object) if unreal.SystemLibrary.get_path_name(o).startswith(system.get_path_name()+':Minimal_1')}
changes=[]
def setp(cls,p,value):
 o=objects[cls];assert H.set_object_property(o,p,value),(cls,p,value)
 changes.append([cls,p,json.loads(H.inspect_object(o))[p]])
def scalar(lo,hi):return f'(Mode=UniformRange,Min={lo},Max={hi},ChannelConstantsAndRanges=({lo},{hi}))'
def vec(x,y,z):return f'(X={x},Y={y},Z={z})'
shape='NiagaraStatelessModule_ShapeLocation';init='NiagaraStatelessModule_InitializeParticle';emitter='NiagaraStatelessEmitter'
setp(shape,'ShapePrimitive','Box')
setp(shape,'bBoxSurfaceOnly','False')
setp(shape,'BoxSize',f'(Mode=NonUniformConstant,Min={vec(2200,1300,2900)},Max={vec(2200,1300,2900)},ChannelConstantsAndRanges=(2200,1300,2900))')
setp(init,'SpriteSizeDistribution','(Mode=UniformRange,Min=(X=0.3,Y=0.3),Max=(X=1.6,Y=1.6),ChannelConstantsAndRanges=(0.3,1.6))')
setp(init,'LifetimeDistribution',scalar(3,5))
setp(emitter,'SpawnInfos','((Type=Rate,Rate=(Mode=UniformConstant,Min=300,Max=300,ChannelConstantsAndRanges=(300)),bEnabled=True))')
setp(emitter,'FixedBounds',f'(Min={vec(-1300,-900,-1700)},Max={vec(1300,900,1700)},IsValid=True)')
setp('NiagaraStatelessModule_CurlNoiseForce','NoiseFrequency','0.006')
setp('NiagaraStatelessModule_CurlNoiseForce','NoiseStrength','2.5')
setp('NiagaraStatelessModule_AddVelocity','bModuleEnabled','True')
setp('NiagaraStatelessModule_AddVelocity','LinearVelocityDistribution',f'(Mode=NonUniformRange,Min={vec(-3,-3,1)},Max={vec(3,3,5)},ChannelConstantsAndRanges=(-3,-3,1,3,3,5))')
assert H.compile_system(system)
assert EA.save_loaded_asset(system,False)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
removed=[]
for a in list(A.get_all_level_actors()):
 if isinstance(a,unreal.MiningCavernMotes):removed.append(a.get_actor_label());assert A.destroy_actor(a)
placed=[]
for i,pos in enumerate([unreal.Vector(-10500,0,7100),unreal.Vector(-6300,0,9500)],1):
 label=f'MAIN_GAME_CrystalMotes_{i}'
 a=next((a for a in A.get_all_level_actors() if a.get_actor_label()==label),None) or A.spawn_actor_from_class(unreal.NiagaraActor,pos)
 a.tags=['BM_Campaign_MainGame02','BM_RoutesV5'];a.set_actor_label(label);a.set_actor_location(pos,False,True);a.set_actor_rotation(unreal.Rotator(),False);a.set_actor_scale3d(unreal.Vector(1,1,1))
 c=a.get_component_by_class(unreal.NiagaraComponent);c.set_asset(system);c.set_auto_activate(True)
 placed.append(dict(label=label,position=pos.to_tuple(),asset=c.get_asset().get_path_name()))
assert L.save_current_level()
(out/'niagara_applied.json').write_text(json.dumps(dict(passed=True,removed=removed,placed=placed,changes=changes),indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
