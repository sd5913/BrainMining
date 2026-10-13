#include "MiningDesignerLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "GeneratedMiningRock.h"
#include "EngineUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"

bool UMiningDesignerLibrary::ExportCampaignLayout(AActor* Actor,const FString& Directory)
{
    using namespace UE::Geometry;
    auto* Hall=Cast<AMiningExhibitHall>(Actor);if(!Hall)return false;
    IFileManager::Get().MakeDirectory(*Directory,true);
    TArray<TSharedPtr<FJsonValue>> Records;
    auto Write=[&](AActor* A,const FString& Id,const TArray<FVector>& Points,const TArray<FIntVector>& Faces,bool Editable)
    {
        if(Faces.IsEmpty())return true;
        FString Obj=TEXT("# BrainMining: world metres, X east, Y flipped from UE, Z up\no ")+Id+TEXT("\n");
        for(FVector P:Points)Obj+=FString::Printf(TEXT("v %.8f %.8f %.8f\n"),P.X*.01,-P.Y*.01,P.Z*.01);
        for(FIntVector T:Faces)Obj+=FString::Printf(TEXT("f %d %d %d\n"),T.X+1,T.Z+1,T.Y+1);
        if(!FFileHelper::SaveStringToFile(Obj,*(Directory/Id+TEXT(".obj")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("file"),Id+TEXT(".obj"));R->SetStringField(TEXT("id"),Id);
        R->SetStringField(TEXT("actor"),A->GetPathName());R->SetStringField(TEXT("label"),A->GetActorLabel());
        R->SetStringField(TEXT("source_level"),A->GetLevel()->GetOutermost()->GetName());R->SetBoolField(TEXT("editable_rock"),Editable);
        R->SetNumberField(TEXT("triangles"),Faces.Num());Records.Add(MakeShared<FJsonValueObject>(R));return true;
    };
    for(TActorIterator<AActor> It(Hall->GetWorld());It;++It)
    {
        AActor* A=*It;bool Suppressed=false;
        for(const auto& Area:Hall->Areas)if(Area.SourceLevel && Area.SourceLevel->GetLoadedLevel()==A->GetLevel())
            Suppressed|=Area.SuppressedSourceActors.Contains(A->GetFName());
        if(Suppressed)continue;
        if(auto* Rock=Cast<AGeneratedMiningRock>(A))
        {
            FDynamicMesh3 M;Rock->CopyMiningMesh(M);TArray<FVector> P;TArray<FIntVector> F;TMap<int,int> Map;
            for(int V:M.VertexIndicesItr()){Map.Add(V,P.Num());P.Add(A->GetActorTransform().TransformPosition(M.GetVertex(V)));}
            for(int T:M.TriangleIndicesItr()){auto V=M.GetTriangle(T);F.Add(FIntVector(Map[V.A],Map[V.B],Map[V.C]));}
            if(!Write(A,A->GetName(),P,F,A->GetLevel()==Hall->GetLevel()))return false;
            continue;
        }
        TArray<UStaticMeshComponent*> Components;A->GetComponents(Components);
        for(auto* C:Components)
        {
            const UStaticMesh* Mesh=C->GetStaticMesh();if(!Mesh || !Mesh->GetRenderData() || !Mesh->GetRenderData()->LODResources.Num())continue;
            const auto& L=Mesh->GetRenderData()->LODResources[0];auto* Instances=Cast<UInstancedStaticMeshComponent>(C);
            const int Count=Instances?Instances->GetInstanceCount():1;
            for(int I=0;I<Count;++I)
            {
                FTransform T=C->GetComponentTransform();if(Instances)Instances->GetInstanceTransform(I,T,true);
                TArray<FVector> P;TArray<FIntVector> F;
                for(uint32 V=0;V<L.VertexBuffers.PositionVertexBuffer.GetNumVertices();++V)P.Add(T.TransformPosition(FVector(L.VertexBuffers.PositionVertexBuffer.VertexPosition(V))));
                const auto Indices=L.IndexBuffer.GetArrayView();
                for(int V=0;V+2<Indices.Num();V+=3)F.Add(FIntVector(Indices[V],Indices[V+2],Indices[V+1]));
                if(!Write(A,FString::Printf(TEXT("Ref_%08x_"),GetTypeHash(A->GetPathName()))+C->GetName()+TEXT("_Instance_")+FString::FromInt(I),P,F,false))return false;
            }
        }
    }
    auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("meshes"),Records);Root->SetStringField(TEXT("coordinates"),TEXT("metres: Blender=(UE.X,-UE.Y,UE.Z)/100"));
    FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    return FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("layout.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UMiningDesignerLibrary::ImportCampaignRock(AActor* Actor,const FString& Filename)
{
    using namespace UE::Geometry;
    auto* Rock=Cast<AGeneratedMiningRock>(Actor);if(!Rock)return false;
    TArray<FString> Lines;if(!FFileHelper::LoadFileToStringArray(Lines,*Filename))return false;
    FDynamicMesh3 Mesh;TArray<int> Vertices;
    for(const FString& Line:Lines)
    {
        TArray<FString> Parts;Line.ParseIntoArrayWS(Parts);
        if(Parts.IsEmpty() || Parts[0].StartsWith(TEXT("#")))continue;
        if(Parts[0]==TEXT("v") && Parts.Num()==4)
        {
            FVector P(FCString::Atod(*Parts[1])*100,-FCString::Atod(*Parts[2])*100,FCString::Atod(*Parts[3])*100);
            if(P.ContainsNaN())return false;
            Vertices.Add(Mesh.AppendVertex(Rock->GetActorTransform().InverseTransformPosition(P)));
        }
        else if(Parts[0]==TEXT("f"))
        {
            if(Parts.Num()!=4)return false;int V[3];
            for(int I=0;I<3;++I){V[I]=FCString::Atoi(*Parts[I+1])-1;if(!Vertices.IsValidIndex(V[I]))return false;}
            if(Mesh.AppendTriangle(Vertices[V[0]],Vertices[V[2]],Vertices[V[1]])<0)return false;
        }
    }
    // A terrain solid must remain closed for winding coverage and excavation booleans.
    if(!Mesh.TriangleCount() || !Mesh.IsClosed())return false;
    Mesh.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    Rock->Modify();
    if(Mesh.TriangleCount()>12000)return Rock->SetChunkedMesh(Mesh,700,true,true);
    Rock->SetGeneratedMesh(MoveTemp(Mesh));return true;
}
