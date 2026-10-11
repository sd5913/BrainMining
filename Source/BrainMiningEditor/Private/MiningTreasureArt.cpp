#include "MiningDesignerLibrary.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace MiningTreasureArt
{
    struct FFace
    {
        TArray<FVector> Points;
        float Shade;
        float Inclusion;
    };

    struct FMineral
    {
        TArray<FFace> Faces;
        FRandomStream Random;
        explicit FMineral(int32 Seed) : Random(Seed) {}
        void Face(TArray<FVector> Points, FVector Center, float Inclusion)
        {
            const FVector Normal = FVector::CrossProduct(Points[1]-Points[0], Points[2]-Points[0]);
            if (FVector::DotProduct(Normal, Points[0]-Center) < 0) Algo::Reverse(Points);
            Faces.Add({Points, float(Random.FRandRange(.58f,1.f)), Inclusion});
        }
        void Crystal(FVector Position, FVector Size, FRotator Rotation, int32 Sides, float Tip, float Inclusion, bool bRough = false)
        {
            TArray<FVector> Rings[3];
            for (int32 Ring = 0; Ring < 3; ++Ring)
                for (int32 Side = 0; Side < Sides; ++Side)
                {
                    const double Angle = Side * 2 * PI / Sides;
                    const double Radius = (Ring == 0 ? .68 : Ring == 1 ? 1. : .86) * (bRough ? Random.FRandRange(.78f,1.15f) : 1.);
                    const double Height = Ring == 0 ? -.48 : Ring == 1 ? -.25 : Tip;
                    const FVector Local(FMath::Cos(Angle)*Size.X*Radius, FMath::Sin(Angle)*Size.Y*Radius, Size.Z*Height);
                    Rings[Ring].Add(Position + Rotation.RotateVector(Local));
                }
            const FVector Peak = Position + Rotation.RotateVector(FVector(Size.X*.12, -Size.Y*.07, Size.Z*.62));
            Face(Rings[0], Position, Inclusion);
            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const int32 Next = (Side+1)%Sides;
                for (int32 Ring = 0; Ring < 2; ++Ring)
                    Face({Rings[Ring][Side], Rings[Ring][Next], Rings[Ring+1][Next], Rings[Ring+1][Side]}, Position, Inclusion);
                Face({Rings[2][Side], Rings[2][Next], Peak}, Position, Inclusion);
            }
        }
        void Rock(FVector Position, FVector Size, FRotator Rotation, float Inclusion = 0)
        {
            Crystal(Position, Size, Rotation, 7, .28f, Inclusion, true);
        }
        FMeshDescription Build()
        {
            FBox Bounds(ForceInit);
            for (const auto& FaceData : Faces) for (const FVector& Point : FaceData.Points) Bounds += Point;
            const FVector Center = Bounds.GetCenter();
            const double Scale = 48. / Bounds.GetExtent().GetMax();
            FMeshDescription Mesh;
            FStaticMeshAttributes Attributes(Mesh);
            Attributes.Register();
            Attributes.GetVertexInstanceUVs().SetNumChannels(1);
            const auto Group = Mesh.CreatePolygonGroup();
            Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Mineral");
            for (const auto& FaceData : Faces)
            {
                const FVector Normal = FVector::CrossProduct(FaceData.Points[1]-FaceData.Points[0], FaceData.Points[2]-FaceData.Points[0]).GetSafeNormal();
                const FVector Tangent = (FaceData.Points[1]-FaceData.Points[0]).GetSafeNormal();
                TArray<FVertexInstanceID> Instances;
                for (const FVector& Point : FaceData.Points)
                {
                    const auto Vertex = Mesh.CreateVertex();
                    Attributes.GetVertexPositions()[Vertex] = FVector3f((Point-Center)*Scale);
                    const auto Instance = Mesh.CreateVertexInstance(Vertex);
                    Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
                    Attributes.GetVertexInstanceTangents()[Instance] = FVector3f(Tangent);
                    Attributes.GetVertexInstanceBinormalSigns()[Instance] = 1;
                    Attributes.GetVertexInstanceColors()[Instance] = FVector4f(FaceData.Shade, FaceData.Shade, FaceData.Shade, FaceData.Inclusion);
                    Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(Point.X*.01, Point.Z*.01));
                    Instances.Add(Instance);
                }
                Mesh.CreatePolygon(Group, Instances);
            }
            return Mesh;
        }
    };
}

bool UMiningDesignerLibrary::BuildTreasureMeshes()
{
    using namespace MiningTreasureArt;
    for (int32 Kind = 0; Kind < 9; ++Kind)
    {
        FMineral Mineral(130 + Kind * 17);
        if (Kind == 0)
        {
            Mineral.Rock(FVector(0,0,0), FVector(35,28,52), FRotator(15,20,-10));
            Mineral.Rock(FVector(23,0,-8), FVector(22,24,38), FRotator(-20,-25,25));
            Mineral.Rock(FVector(-21,10,-5), FVector(17,20,37), FRotator(30,10,-12));
        }
        else if (Kind == 1 || Kind == 3)
        {
            Mineral.Rock(FVector(0,0,-9), FVector(38,29,42), FRotator(10,15,0), .85f);
            for (int32 Piece = 0; Piece < 5; ++Piece)
                Mineral.Rock(FVector((Piece-2)*12, -10+(Piece%2)*20, 8), FVector(12,13,30), FRotator(Piece*13,Piece*31,20));
        }
        else if (Kind == 2)
        {
            for (int32 Piece = 0; Piece < 6; ++Piece)
                Mineral.Crystal(FVector((Piece-2)*9,(Piece%2)*12,Piece*5-12), FVector(25-Piece*2,16,14), FRotator(5*Piece,15*Piece,12),4,.35f,0);
        }
        else if (Kind == 4)
        {
            for (int32 Piece = 0; Piece < 7; ++Piece)
            {
                const double Angle = Piece*2.4;
                Mineral.Crystal(FVector(FMath::Cos(Angle)*18,FMath::Sin(Angle)*17,Piece==0?12:0), FVector(16,15,Piece==0?62:40), FRotator(Piece*7,Piece*33,Piece*5),5,.25f,0);
            }
        }
        else if (Kind == 5 || Kind == 6)
        {
            Mineral.Rock(FVector(0,0,0), FVector(32,30,62), FRotator(-10,15,8));
            Mineral.Rock(FVector(20,-9,-9), FVector(21,20,39), FRotator(30,0,-24));
            Mineral.Rock(FVector(-23,12,-14), FVector(16,18,31), FRotator(-25,25,5));
        }
        else if (Kind == 7)
        {
            Mineral.Crystal(FVector(0,0,9), FVector(15,14,83), FRotator(-8,0,4),6,.28f,0);
            Mineral.Crystal(FVector(-18,0,-5), FVector(12,11,65), FRotator(-25,20,15),6,.24f,0);
            Mineral.Crystal(FVector(17,6,-7), FVector(11,10,58), FRotator(25,-10,-20),6,.3f,0);
            Mineral.Crystal(FVector(4,-15,-15), FVector(10,10,44), FRotator(5,30,28),6,.22f,0);
        }
        else
        {
            Mineral.Crystal(FVector(0,0,0), FVector(30,26,75), FRotator(-12,0,8),8,.12f,0);
            Mineral.Crystal(FVector(0,0,-31), FVector(20,18,46), FRotator(168,0,8),8,.03f,0);
        }
        const FString Name = FString::Printf(TEXT("SM_Treasure_%02d"), Kind+1);
        const FString PackageName = TEXT("/Game/Showcase/Treasure/Meshes/") + Name;
        UPackage* Package = CreatePackage(*PackageName);
        auto* Mesh = FindObject<UStaticMesh>(Package, *Name);
        const bool bNew = !Mesh;
        if (!Mesh) Mesh = NewObject<UStaticMesh>(Package, *Name, RF_Public|RF_Standalone);
        Mesh->bAllowCPUAccess = true;
        Mesh->GetStaticMaterials().Reset();
        Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Mineral")));
        FMeshDescription Description = Mineral.Build();
        UStaticMesh::FBuildMeshDescriptionsParams Build;
        Build.bAllowCpuAccess = true;
        Build.bBuildSimpleCollision = false;
        Build.bFastBuild = false;
        Mesh->BuildFromMeshDescriptions({&Description}, Build);
        Mesh->CreateBodySetup();
        Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
        Mesh->GetBodySetup()->InvalidatePhysicsData();
        Mesh->GetBodySetup()->CreatePhysicsMeshes();
        if (bNew) FAssetRegistryModule::AssetCreated(Mesh);
        Mesh->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public|RF_Standalone;
        if (!UPackage::SavePackage(Package, Mesh, *Filename, Args)) return false;
    }
    return true;
}
