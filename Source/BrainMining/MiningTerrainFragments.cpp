#include "MiningTerrainFragments.h"
#include "Selections/MeshConnectedComponents.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMeshEditor.h"

int32 MiningTerrainFragments::RemoveSmallFloatingPieces(UE::Geometry::FDynamicMesh3& Mesh,const FBox& OriginalBounds,bool bProtectChunkBorders,float MaxDimension)
{
    using namespace UE::Geometry;
    if(Mesh.TriangleCount()==0 || MaxDimension<=0 || !OriginalBounds.IsValid)return 0;
    FMeshConnectedComponents Pieces(&Mesh);Pieces.FindConnectedTriangles();
    TArray<int32> Remove;
    TUniquePtr<FDynamicMeshAABBTree3> Tree;
    for(const auto& Piece:Pieces.Components)
    {
        FBox Bounds(ForceInit);
        for(int32 T:Piece.Indices)
        {
            FVector3d A,B,C;Mesh.GetTriVertices(T,A,B,C);
            Bounds+=FVector(A);Bounds+=FVector(B);Bounds+=FVector(C);
        }
        if(Bounds.GetSize().GetMax()>MaxDimension)continue;
        // Keep ground-resting pieces and anything potentially joined across a
        // terrain cell seam. Only complete isolated small solids are discarded.
        bool Anchored=Bounds.Min.Z<=OriginalBounds.Min.Z+1.f;
        if(bProtectChunkBorders)for(int32 Axis=0;Axis<3;++Axis)
            Anchored|=Bounds.Min[Axis]<=OriginalBounds.Min[Axis]+1.f || Bounds.Max[Axis]>=OriginalBounds.Max[Axis]-1.f;
        if(!Anchored)
        {
            if(!Tree)Tree=MakeUnique<FDynamicMeshAABBTree3>(&Mesh,true);
            TSet<int32> OwnTriangles;for(int32 T:Piece.Indices)OwnTriangles.Add(T);
            IMeshSpatial::FQueryOptions Options(1.1,[&OwnTriangles](int32 T){return !OwnTriangles.Contains(T);});
            // A separate mesh component can still physically rest on the cave
            // floor. Keep it when another solid is immediately underneath.
            for(const FVector2D Sample:{FVector2D(0,0),FVector2D(-.5,-.5),FVector2D(.5,.5),FVector2D(-.5,.5),FVector2D(.5,-.5)})
            {
                const FVector P(Bounds.GetCenter().X+Sample.X*Bounds.GetExtent().X,Bounds.GetCenter().Y+Sample.Y*Bounds.GetExtent().Y,Bounds.Min.Z+.05);
                if(Tree->FindNearestHitTriangle(FRay3d(FVector3d(P),FVector3d(0,0,-1)),Options)!=INDEX_NONE){Anchored=true;break;}
            }
        }
        if(!Anchored)Remove.Append(Piece.Indices);
    }
    if(Remove.IsEmpty())return 0;
    FDynamicMeshEditor Editor(&Mesh);Editor.RemoveTriangles(Remove,true);
    Mesh.CompactInPlace();
    return Remove.Num();
}
