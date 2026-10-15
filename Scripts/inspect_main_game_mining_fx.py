"""Inspect authored mining effects references and counts on existing characters."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/MainGameMiningFX';out.mkdir(parents=True,exist_ok=True)
report={}
for path in ['/Game/MainGame/Blueprints/BP_MainGameCharacter','/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/ExternalMining/Blueprints/BP_ExternalMiningTestCharacter']:
    bp=unreal.load_asset(path);cdo=unreal.get_default_object(bp.generated_class());fx=cdo.get_component_by_class(unreal.MiningToolEffectsComponent);values={}
    for p in ['bEnabled','DebrisClass','DebrisMeshAssets','SwingCurveAsset','DrillDustMaterialAsset','DrillSmokeMaterialAsset','DrillSparkMaterialAsset','DebrisChunkCount','DrillDebrisChunkCount','DrillDustParticleCount','DrillSmokeParticleCount','DrillSparkParticleCount','DrillDustStrength','DrillSmokeStrength','DrillSparkStrength']:
        value=fx.get_editor_property(p)
        if p=='DebrisMeshAssets':value=[str(v) for v in value]
        elif not isinstance(value,(int,float,bool,str)):value=str(value)
        values[p]=value
    report[path]=values
(out/'authored_before.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log(json.dumps(report))
