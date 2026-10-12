#include "MiningDesignerLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "GeneratedMiningRock.h"
#include "EngineUtils.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Implicit/Solidify.h"
#include "MeshSimplification.h"

bool UMiningDesignerLibrary::NaturalizeCampaignJunctions(AActor* Actor)
{
    using namespace UE::Geometry;
    auto* Hall=Cast<AMiningExhibitHall>(Actor);if(!Hall)return false;
    auto RepairThinSolids=[&]()
    {
        if(Hall->ActorHasTag(TEXT("BM_SolidJunctionRepairV3")))return true;
        for(TActorIterator<AGeneratedMiningRock> It(Hall->GetWorld());It;++It)
        {
            auto* R=*It;const FString Name=R->GetActorLabel();
            if(R->GetLevel()!=Hall->GetLevel() || !(Name.StartsWith(TEXT("Campaign_BuriedBank")) || Name.StartsWith(TEXT("Campaign_TunnelSoil"))))continue;
            FDynamicMesh3 M;R->CopyMiningMesh(M);if(!M.TriangleCount())continue;M.DiscardAttributes();
            TMeshAABBTree3<FDynamicMesh3> Tree(&M,true);TFastWindingTree<FDynamicMesh3> Winding(&Tree,true);
            TImplicitSolidify<FDynamicMesh3> Solid(&M,&Tree,&Winding);
            Solid.MeshCellSize=20;Solid.ExtendBounds=25;Solid.SurfaceSearchSteps=8;
            FDynamicMesh3 Clean(&Solid.Generate());
            if(!Clean.TriangleCount() || !Clean.IsClosed())return false;
            // Retain the solidifier topology here: QEM on thin fitted wedges can
            // fold an otherwise closed face across its neighbor.
            Clean.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Clean.Attributes()->PrimaryNormals());R->Modify();
            if(Clean.TriangleCount()>12000){if(!R->SetChunkedMesh(Clean,700,true,true))return false;}
            else R->SetGeneratedMesh(MoveTemp(Clean));
        }
        Hall->Modify();Hall->Tags.Add(TEXT("BM_SolidJunctionRepairV3"));return true;
    };
    const bool bInnerPass=Hall->ActorHasTag(TEXT("BM_NaturalJunctionsV2"));
    if(bInnerPass && Hall->ActorHasTag(TEXT("BM_InnerMouthFacetsV3")))return RepairThinSolids();
    if(bInnerPass)Hall->Tags.Remove(TEXT("BM_SolidJunctionRepairV3"));
    TArray<FVector> Centers;
    for(const auto& Area:Hall->Areas) if(Area.Kind==EMiningExperienceKind::Mining && Area.MapRoute.Num()>2)
    {
        // These are the original cave crop mouths, not the room's architectural doorway.
        if(bInnerPass)
        {
            // The other end of each adapter is 460 cm inside the cropped cave.
            // Its cap was outside the first deformation's full-strength region.
            Centers.Add(Area.MapRoute[1]-FVector(0,0,120));
            Centers.Add(Area.MapRoute[Area.MapRoute.Num()-3]-FVector(0,0,120));
        }
        else
        {
            Centers.Add(Area.MapRoute[0]-FVector(0,0,120));
            Centers.Add(FVector(Area.MiningBounds.Max.X-5,Area.MapRoute.Last().Y,Area.MapRoute.Last().Z-100));
        }
    }
    auto Weight=[bInnerPass](FVector D)
    {
        return (1-FMath::SmoothStep(bInnerPass?80.:160.,bInnerPass?340.:520.,FMath::Abs(D.X)))*
            (1-FMath::SmoothStep(650.,1200.,FMath::Abs(D.Y)))*
            (1-FMath::SmoothStep(650.,1100.,FMath::Abs(D.Z-180)));
    };
    auto Warp=[&](FVector P)
    {
        double W=0;FVector D=FVector::ZeroVector;
        for(FVector C:Centers)if(double Q=Weight(P-C);Q>W){W=Q;D=P-C;}
        if(W==0)return P;
        const double Upper=FMath::SmoothStep(150.,360.,D.Z);
        const double Near=FMath::Exp(-FMath::Square(D.Y/480.));
        const double Noise=FMath::PerlinNoise3D(P*.007+FVector(21,13,8));
        // One continuous world-space deformation for source cave, end cap and adapters.
        // It removes the ruler-straight cut plane without opening cracks between solids.
        P.X+=W*(95*FMath::Sin(D.Y*.009+D.Z*.006)+55*Noise);
        P.Y+=W*(D.Y*(.20-.38*Upper)*Near+35*FMath::Sin(D.Z*.012)+22*Noise);
        P.Z+=W*(150*FMath::Max(0.,1-FMath::Square(D.Y/400.))*Upper+18*Noise*FMath::SmoothStep(40.,180.,D.Z));
        return P;
    };
    for(TActorIterator<AGeneratedMiningRock> It(Hall->GetWorld());It;++It)
    {
        auto* Rock=*It;if(Rock->GetLevel()!=Hall->GetLevel())continue;
        const auto Transform=Rock->GetActorTransform();bool Changed=false;
        auto Deform=[&](FDynamicMesh3& Mesh)
        {
            if(!Mesh.TriangleCount())return;
            Mesh.DiscardAttributes();
            // Crop caps originally have only two giant triangles. Add actual facets before warping.
            for(int Pass=0;Pass<18;++Pass)
            {
                TArray<int> Edges;
                for(int E:Mesh.EdgeIndicesItr())
                {
                    const auto V=Mesh.GetEdgeV(E);const FVector A=Transform.TransformPosition(Mesh.GetVertex(V.A)),B=Transform.TransformPosition(Mesh.GetVertex(V.B));
                    if(FVector::DistSquared(A,B)<110*110)continue;
                    const FBox EdgeBounds(A.ComponentMin(B),A.ComponentMax(B));
                    for(FVector C:Centers)if(EdgeBounds.Intersect(FBox(C-FVector(530,1210,930),C+FVector(530,1210,1290)))){Edges.Add(E);break;}
                }
                if(Edges.IsEmpty())break;
                for(int E:Edges)if(Mesh.IsEdge(E)){FDynamicMesh3::FEdgeSplitInfo Info;Mesh.SplitEdge(E,Info);}
            }
            for(int V:Mesh.VertexIndicesItr())
            {
                const FVector P=Transform.TransformPosition(Mesh.GetVertex(V)),Q=Warp(P);
                if(!P.Equals(Q,.0001)){Mesh.SetVertex(V,Transform.InverseTransformPosition(Q));Changed=true;}
            }
            if(Changed && !Rock->bChunked && Rock->GetActorLabel().StartsWith(TEXT("Campaign_")))
            {
                // Earlier boolean fitting leaves sub-millimetre slivers in these thin end caps.
                // Solidify only the caps after bending, so those slivers cannot fold into
                // self-intersections and reject later mining booleans.
                TMeshAABBTree3<FDynamicMesh3> Tree(&Mesh,true);
                TFastWindingTree<FDynamicMesh3> Winding(&Tree,true);
                TImplicitSolidify<FDynamicMesh3> Solid(&Mesh,&Tree,&Winding);
                Solid.MeshCellSize=FMath::Max(20.,Mesh.GetBounds().MaxDim()/120.);Solid.ExtendBounds=Solid.MeshCellSize*1.25;Solid.SurfaceSearchSteps=8;
                FDynamicMesh3 Clean(&Solid.Generate());
                if(Clean.TriangleCount() && Clean.IsClosed())
                {
                    FQEMSimplification Simplifier(&Clean);Simplifier.bPreventTinyTriangles=true;
                    Simplifier.SimplifyToTriangleCount(FMath::Max(1500,Clean.TriangleCount()/3));
                    if(Clean.IsClosed())Mesh=MoveTemp(Clean);
                }
            }
            Mesh.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
        };
        Rock->Modify();
        if(Rock->bChunked)
        {
            for(auto& C:Rock->Chunks)if(C.InitialMesh)
            {
                FDynamicMesh3 M;C.InitialMesh->ProcessMesh([&](const FDynamicMesh3& Source){M=Source;});Deform(M);
                if(!M.TriangleCount())continue;
                const auto B=M.GetBounds();C.Bounds=FBox(B.Min,B.Max);C.InitialMesh->Modify();C.InitialMesh->SetMesh(MoveTemp(M));
            }
            Rock->ResetRock();
        }
        else {FDynamicMesh3 M;Rock->CopyMiningMesh(M);Deform(M);if(Changed){if(M.TriangleCount()>12000){if(!Rock->SetChunkedMesh(M,700,true,true))return false;}else Rock->SetGeneratedMesh(MoveTemp(M));}}
        if(Changed)UE_LOG(LogTemp,Display,TEXT("Natural junction: %s"),*Rock->GetActorLabel());
    }
    for(TActorIterator<AMiningTreasureField> It(Hall->GetWorld());It;++It)if(It->bUseAuthoredPlacements)
    {
        It->Modify();for(auto& P:It->AuthoredPlacements)
        {
            const FVector Old=P.Surface.GetLocation(),N=P.Surface.GetRotation().GetForwardVector();
            P.Surface.SetLocation(Warp(Old));P.Surface.SetRotation((Warp(Old+N)-Warp(Old)).Rotation().Quaternion());
            P.Transform.SetLocation(Warp(P.Transform.GetLocation()));
            P.Surface.SetRotation((P.Surface.GetLocation()-P.Transform.GetLocation()).Rotation().Quaternion());
            for(auto& F:P.Fragments)F.SetLocation(Warp(F.GetLocation()));
            for(auto& S:P.FragmentSupports)S=Warp(S);
        }
    }
    Hall->Modify();Hall->Tags.Add(bInnerPass?TEXT("BM_InnerMouthFacetsV3"):TEXT("BM_NaturalJunctionsV2"));
    return bInnerPass?RepairThinSolids():NaturalizeCampaignJunctions(Actor);
}
