"""Fast commit-tree check before pushing tool presentation assets.

Run with the normal Python interpreter. Checks the committed HEAD tree, not
untracked files or a developer's local cached DLL. UE dependency validation is
available separately in verify_tool_asset_dependencies.py.
"""
import subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.run(['git',*args],cwd=root,check=True,stdout=subprocess.PIPE).stdout
tracked=set(git('ls-tree','-r','--name-only','HEAD').decode().splitlines())
required=['Content/Drill/Stylized/SM_DrillStylized.uasset','Content/Drill/Stylized/M_DrillStylized.uasset',
          'Content/Drill/CF_DrillSpin.uasset','Content/Bomb/SM_StickyBomb.uasset',
          'Content/UI/MiningGUI/WBP_MiningHUD.uasset',
          'Source/BrainMining/MiningBombPresentationComponent.cpp','Source/BrainMining/MiningBombPresentationComponent.h']
required+=['Content/Drill/Stylized/T_DrillStylized_'+name+'.uasset' for name in ['BaseColor','Roughness','Metallic','Normal']]
required+=['Content/UI/MiningGUI/Reticles/'+prefix+tool+'.uasset' for prefix in ['M_Reticle','T_Reticle'] for tool in ['Pickaxe','Drill','Bomb']]
errors=['Not committed: '+path for path in required if path not in tracked]
for path,markers in {
    'Source/BrainMining/BrainMiningCharacter.cpp':['TEXT("BombMotion")','SM_DrillStylized','SM_StickyBomb'],
    'Source/BrainMining/MiningBombActor.cpp':['MiningBombPresentationComponent','bStuck = true'],
    'Source/BrainMining/MiningGUIWidgets.cpp':['ReticlePickaxe','ReticleDrill','ReticleBomb'],
}.items():
    text=git('show','HEAD:'+path).decode('utf-8')
    errors+=['Missing committed tool code: '+path+' / '+marker for marker in markers if marker not in text]
if errors:
    print('\n'.join(errors),file=sys.stderr)
    sys.exit(1)
print('Tool publish check passed: committed model, material, four textures, reticles and runtime code are complete.')
