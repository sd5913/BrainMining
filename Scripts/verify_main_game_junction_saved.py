"""Read-only collision acceptance for the widened, saved entrance enclosure."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameJunction';report={'passed':False,'checks':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
try:
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not ed.get_game_world()
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());world=ed.get_editor_world()
    pawn=unreal.get_default_object(unreal.load_class(None,'/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C'));capsule=pawn.get_component_by_class(unreal.CapsuleComponent)
    radius=capsule.get_scaled_capsule_radius();height=capsule.get_scaled_capsule_half_height()
    ignored=[a for a in actors if isinstance(a,unreal.DiggableRock)]
    manifest=json.loads((ROOT/'Saved/Validation/MainGameStartRoom/integration.json').read_text());offset=unreal.Vector(*manifest.get('junction_world_offset',[0,0,0]))
    for y in [-240,-120,0,120,240]:
        h=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(-6800,y,6110)+offset,unreal.Vector(-6270,y,6110)+offset,radius,height,unreal.TraceTypeQuery.ECC_VISIBILITY,False,ignored,unreal.DrawDebugTrace.NONE,True);h=h.to_tuple() if h else None
        check(not h or not h[0],'No fixed pipe or bedrock blocks the wide mouth at y='+str(y))
    for end in [(-6410,-500,6240),(-6410,500,6240),(-6410,0,5800),(-6410,0,6800)]:
        h=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(-6410,0,6240)+offset,unreal.Vector(*end)+offset,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True);h=h.to_tuple() if h else None
        check(h and h[0] and h[9] and not isinstance(h[9],unreal.DiggableRock),'Unmineable outside enclosure remains at '+str(end))
    r=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_V1'))
    path=OUT/'authoring.json';journal=json.loads(path.read_text());journal.update(shell_triangles_after=r.generated_shell.get_component_by_class(unreal.DynamicMeshComponent).get_dynamic_mesh().get_triangle_count(),shell_lower_edge_cleared=True,upper_shell_sealed=any(a.get_actor_label()=='STARTROOM_Junction_UpperShellSeal' for a in actors));path.write_text(json.dumps(journal,indent=2),encoding='utf-8')
    report.update(passed=True,capsule_radius_cm=radius,capsule_half_height_cm=height)
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'saved_collision.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('WIDE_JUNCTION_SAVED '+json.dumps(report))
