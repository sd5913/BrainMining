#include "MiningDesignerLibrary.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Operations/MeshBoolean.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/World.h"

using namespace UE::Geometry;
namespace
{
bool Editable(AActor* Actor)
{
    return IsValid(Actor) && Actor->GetWorld() && !Actor->GetWorld()->IsGameWorld();
}
bool Valid(const FDynamicMesh3& Mesh)
{
    return Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)
        && (Mesh.TriangleCount() == 0 || Mesh.IsClosed());
}
}
UDynamicMesh* UMiningDesignerLibrary::GetCampaignRockSource(AActor* Actor)
{
    if (!Editable(Actor)) return nullptr;
    FDynamicMesh3 Mesh;
    if (const auto* Rock = Cast<AGeneratedMiningRock>(Actor)) Rock->CopyEditorPreviewMesh(Mesh);
    else if (auto* Component = Actor->FindComponentByClass<UDynamicMeshComponent>())
        Component->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& Source){ Mesh = Source; });
    else return nullptr;
    UDynamicMesh* Result = NewObject<UDynamicMesh>();
    Result->SetMesh(MoveTemp(Mesh));
    return Result;
}
bool UMiningDesignerLibrary::SetCampaignRockSource(AActor* Actor, UDynamicMesh* Source)
{
    auto* Rock = Cast<AGeneratedMiningRock>(Actor);
    if (!Editable(Actor) || !Rock || !Source) return false;
    FDynamicMesh3 Mesh;
    Source->ProcessMesh([&](const FDynamicMesh3& Input){ Mesh = Input; });
    if (!Valid(Mesh)) return false;
    Mesh.DiscardAttributes();
    FMeshNormals::InitializeMeshToPerTriangleNormals(&Mesh);
    Rock->Modify();
    if (Rock->bChunked || Mesh.TriangleCount() > 12000)
    {
        // Large caves use the same bounded grid-size retries as region generation.
        // A fixed 700 cm grid can put a cell plane on a deformed terrain edge.
        if (Mesh.GetBounds().MaxDim() > 6000)
        {
            for (double Length : {1800.,1700.,1900.,1600.,2000.,1400.,700.})
                if (Rock->SetChunkedMesh(Mesh,Length,true,true)) return true;
            return false;
        }
        for (double Length : {700.,650.,750.,600.})
            if (Rock->SetChunkedMesh(Mesh,Length,true,true)) return true;
        return false;
    }
    Rock->SetGeneratedMesh(MoveTemp(Mesh));
    return true;
}
bool UMiningDesignerLibrary::SubtractCampaignLayers(AActor* Target, const TArray<AActor*>& Owners)
{
    if (!Editable(Target) || !Cast<AGeneratedMiningRock>(Target)) return false;
    UDynamicMesh* Input = GetCampaignRockSource(Target);
    if (!Input) return false;
    FDynamicMesh3 Mesh;
    Input->ProcessMesh([&](const FDynamicMesh3& Source){ Mesh = Source; });
    Mesh.DiscardAttributes();
    if (!Valid(Mesh)) return false;
    const FTransform Transform = Target->GetActorTransform();
    for (AActor* Owner : Owners)
    {
        if (!Editable(Owner) || Owner == Target) return false;
        UDynamicMesh* OwnerSource = GetCampaignRockSource(Owner);
        if (!OwnerSource) return false;
        FDynamicMesh3 Tool;
        OwnerSource->ProcessMesh([&](const FDynamicMesh3& Source){ Tool = Source; });
        Tool.DiscardAttributes();
        for (int32 V : Tool.VertexIndicesItr()) Tool.SetVertex(V, FVector3d(Transform.InverseTransformPosition(
            Owner->GetActorTransform().TransformPosition(FVector(Tool.GetVertex(V))))));
        if (!Mesh.GetBounds().Intersects(Tool.GetBounds())) continue;
        if (!Valid(Tool)) return false;
        FDynamicMesh3 Result;
        bool bSuccess = false;
        // Nearly coincident imported triangles can make an exact difference
        // ambiguous. A sub-millimetre clearance belongs to the hidden join,
        // never the visible soil face, and prevents coplanar blast caps.
        FMeshNormals ToolNormals(&Tool);
        ToolNormals.ComputeVertexNormals();
        for (double Clearance : {0., .01, .05})
        {
            FDynamicMesh3 Cutter(Tool);
            if (Clearance > 0.)
                for (int32 V : Cutter.VertexIndicesItr())
                    Cutter.SetVertex(V, Tool.GetVertex(V) + Clearance * ToolNormals.GetNormals()[V]);
            for (double Tolerance : {1.e-8, 1.e-7, 1.e-6})
            {
                Result.Clear();
                FMeshBoolean Operation(&Mesh,&Cutter,&Result,FMeshBoolean::EBooleanOp::Difference);
                Operation.bSimplifyAlongNewEdges = false;
                Operation.SnapTolerance = Tolerance;
                if (Operation.Compute() && Operation.CreatedBoundaryEdges.IsEmpty() && Valid(Result)) { bSuccess = true; break; }
            }
            if (bSuccess) break;
        }
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Error, TEXT("Layer partition failed: %s minus %s; target preserved"), *Target->GetName(), *Owner->GetName());
            return false;
        }
        Mesh = MoveTemp(Result);
    }
    Input->SetMesh(MoveTemp(Mesh));
    return SetCampaignRockSource(Target,Input);
}
