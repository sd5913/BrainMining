#include "EndingEmergence.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"

void AEndingEmergence::BuildSolarSystem()
{
    Shape(TEXT("SolarSun"),FVector::ZeroVector,FVector(1350),FLinearColor(1,.38f,.06f),TEXT("Sphere"),SolarSystem,TEXT("StellarSurface"));
    auto* Halo=Shape(TEXT("SolarCorona"),FVector(-700,0,0),FVector(5400,5400,1),FLinearColor(1,.43f,.10f),TEXT("Plane"),SolarSystem,TEXT("StellarCorona"));
    Halo->SetRelativeRotation(FRotator(90,0,0));
    const float Orbits[]={1250,1800,2400,3000,4000,5000,6200,7500};
    const float Sizes[]={120,200,225,160,650,500,350,320};
    const FLinearColor Colors[]={FLinearColor(.27f,.21f,.19f),FLinearColor(.72f,.43f,.15f),FLinearColor(.07f,.27f,.55f),FLinearColor(.55f,.12f,.055f),FLinearColor(.67f,.43f,.27f),FLinearColor(.68f,.52f,.28f),FLinearColor(.13f,.55f,.60f),FLinearColor(.11f,.20f,.62f)};
    auto* Arcs=Instances(TEXT("SolarOrbitArcs"),TEXT("Cylinder"),FLinearColor(.022f,.032f,.050f),SolarSystem,TEXT("Surface"));
    auto Orbit=[](float R,float A){return FVector(FMath::Cos(A)*R*.18f,FMath::Cos(A)*R,FMath::Sin(A)*R*.55f);};
    for(int I=0;I<8;++I)
    {
        const FVector P=Orbit(Orbits[I],I*.93f+.35f);
        auto* Planet=Shape(TEXT("SolarPlanet"),P,FVector(Sizes[I]),Colors[I],TEXT("Sphere"),SolarSystem,I==2?TEXT("Planet"):TEXT("SolarPlanet"));
        if(auto* Mat=Cast<UMaterialInstanceDynamic>(Planet->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("Variant"),I);
        if(I==5)
        {
            auto* Ring=Shape(TEXT("SaturnRings"),P,FVector(1150,1150,1),Colors[I],TEXT("Plane"),SolarSystem,TEXT("PlanetRing"));
            Ring->SetRelativeRotation(FRotator(68,0,23));
        }
        for(int J=0;J<96;++J)
        {
            const FVector A=Orbit(Orbits[I],J*2*PI/96),B=Orbit(Orbits[I],(J+1)*2*PI/96);
            Arcs->AddInstance(FTransform(FRotationMatrix::MakeFromZ(B-A).Rotator(),(A+B)*.5,FVector(.06,.06,(B-A).Size()/100)));
        }
    }
}

void AEndingEmergence::UpdateCosmos(float Reveal)
{
    if(!NebulaUniverse||!SolarSystem)return;
    SolarSystem->SetVisibility(true,true);
    // Visibility comes from crossing real world-space layers, never an alpha reveal.
    NebulaUniverse->SetVisibility(true,true);
    TArray<USceneComponent*> Parts;NebulaUniverse->GetChildrenComponents(true,Parts);
    for(auto* Part:Parts)if(auto* Mesh=Cast<UStaticMeshComponent>(Part))
    {
        if(auto* Mat=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("Reveal"),1);
        const FString Name=Mesh->GetName();
        if(Name.StartsWith(TEXT("ParallaxNebula")) || Name.StartsWith(TEXT("DeepStarField")) || Name.StartsWith(TEXT("GalaxyHaze")))
        {
            // These are enormous, almost zero-thickness background planes.
            // Keep stable LOD/bounds while the camera crosses their depth layers.
            Mesh->SetForcedLodModel(1);Mesh->SetBoundsScale(2.f);
            Mesh->bNeverDistanceCull=true;Mesh->bAllowCullDistanceVolume=false;
            Mesh->SetCastShadow(false);Mesh->SetTranslucentSortPriority(-50);
        }
    }
}

void AEndingEmergence::CacheCellMotion()
{
    CellBreathers.Empty();CellRestPoses.Empty();CellMotionPhases.Empty();
    TArray<USceneComponent*> Parts;Sets[0]->GetChildrenComponents(false,Parts);
    for(auto* Part:Parts)if(Part->GetName().StartsWith(TEXT("CellDress")) ||
        (Part->GetOwner()!=this && Part->GetOwner()->ActorHasTag(TEXT("EndingDemo2Organelle"))))
    {
        CellBreathers.Add(Part);CellRestPoses.Add(Part->GetRelativeTransform());
        const FString Name=Part->GetName();
        CellMotionPhases.Add(Name.Contains(TEXT("Nucle"))?.7f:Name.Contains(TEXT("Golgi"))?2.4f:CellBreathers.Num()*2.39996f);
    }
}

void AEndingEmergence::UpdateCellMotion()
{
    if(Shot>=0||GateIndex!=0)return;
    for(int I=0;I<CellBreathers.Num();++I)
    {
        const float Phase=CellMotionPhases[I],Breath=FMath::Sin(Age*.85f+Phase);
        FTransform T=CellRestPoses[I];
        T.AddToTranslation(FVector(FMath::Sin(Age*.37f+Phase)*5,FMath::Cos(Age*.31f+Phase)*6,Breath*14));
        T.SetScale3D(T.GetScale3D()*FVector(1+Breath*.012f,1+Breath*.012f,1+Breath*.022f));
        CellBreathers[I]->SetRelativeTransform(T);
    }
}

void AEndingEmergence::BuildBackgroundNeurons()
{
    auto* Bodies=Instances(TEXT("BackgroundNeuronBodies"),TEXT("FacetNeuronSoma"),FLinearColor(.045f,.24f,.55f),Sets[0],TEXT("NeuronGlass"));
    auto* Branches=Instances(TEXT("BackgroundDendrites"),TEXT("FacetNeuronBranch"),FLinearColor(.035f,.20f,.45f),Sets[0],TEXT("NeuralFilament"));
    auto* Tips=Instances(TEXT("BackgroundSynapses"),TEXT("FacetNeuronSoma"),FLinearColor(.09f,.33f,.60f),Sets[0],TEXT("NeuronGlass"));
    auto Link=[&](FVector A,FVector B,float Width){Branches->AddInstance(FTransform(FRotationMatrix::MakeFromZ(B-A).Rotator(),(A+B)*.5,FVector(Width,Width,(B-A).Size())/100));};
    FRandomStream R(8843);
    for(int I=0;I<1250;++I)
    {
        const FVector P=FVector(19700,0,1200)+R.VRand()*FVector(7400,4700,3300)*FMath::Pow(R.FRandRange(.12f,1.f),1.f/3);
        bool Clear=true;for(const FVector& Host:NeuronPositions)if(FVector::DistSquared(P,Host)<FMath::Square(430.f)){Clear=false;break;}
        if(!Clear)continue;
        const float Size=R.FRandRange(55,135);
        Bodies->AddInstance(FTransform(FRotator(R.FRandRange(-30,30),R.FRandRange(0,360),R.FRandRange(-25,25)),P,FVector(Size,Size*R.FRandRange(.8f,1.1f),Size*.9f)/100));
        for(int J=0;J<3+I%3;++J)
        {
            const float A=R.FRandRange(-PI,PI);
            const FVector Dir=FVector(FMath::Cos(A),FMath::Sin(A),R.FRandRange(-.45f,.45f)).GetSafeNormal(),Elbow=P+Dir*Size*2.1f,End=Elbow+Dir*Size*1.6f+R.VRand()*Size*.4f;
            Link(P,Elbow,5);Link(Elbow,End,2.8f);
            const FVector Tip=End+R.VRand()*FVector(Size,Size,Size*.35f);Link(End,Tip,1.4f);Tips->AddInstance(FTransform(FQuat::Identity,Tip,FVector(.08f)));
        }
    }
}

void AEndingEmergence::BuildBrainSurface()
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh(true,true,false,false);
    constexpr int Rows=160,Columns=256;
    auto Vertex=[&](float Latitude,float Longitude)
    {
        const FVector D(FMath::Sin(Latitude)*FMath::Cos(Longitude),FMath::Sin(Latitude)*FMath::Sin(Longitude),FMath::Cos(Latitude));
        const FVector P=D*FVector(8700,5800,4200);
        // A closed two-hemisphere envelope surrounds the entire playable route.
        // Fold relief changes the silhouette, not just a pattern on a sphere.
        const float Flow=FMath::Sin(P.X*.0038f+2*FMath::Sin(P.Y*.0025f)+FMath::Sin(P.Z*.002f))
            +.7f*FMath::Sin(P.Y*.0045f+2*FMath::Sin(P.Z*.003f)+FMath::Sin(P.X*.0018f))
            +.5f*FMath::Sin(P.Z*.005f+1.8f*FMath::Sin(P.X*.003f+P.Y*.0015f));
        const float Ridge=1-FMath::Exp(-FMath::Square(Flow)*3.2f);
        const float Cross=.88f+.12f*FMath::Sin(P.Z*.003f+FMath::Sin(P.Y*.002f)*1.5f);
        const float Fissure=FMath::Exp(-FMath::Square(P.Y/360.f))*FMath::Max(0.f,D.Z);
        const float Relief=-110+260*Ridge*Cross-480*Fissure;
        const int Id=Mesh.AppendVertex(FVector3d(P+D*Relief));
        Mesh.SetVertexColor(Id,FVector3f(Ridge*Cross,Fissure,1.f));
        return Id;
    };
    const int North=Vertex(0,0);
    for(int Row=1;Row<Rows;++Row)for(int Col=0;Col<Columns;++Col)Vertex(PI*Row/Rows,2*PI*Col/Columns);
    const int South=Vertex(PI,0);
    for(int Col=0;Col<Columns;++Col)
    {
        const int Next=(Col+1)%Columns;
        Mesh.AppendTriangle(North,1+Col,1+Next);
        for(int Row=0;Row<Rows-2;++Row)
        {
            const int A=1+Row*Columns+Col,B=1+Row*Columns+Next;
            Mesh.AppendTriangle(A,A+Columns,B);Mesh.AppendTriangle(B,A+Columns,B+Columns);
        }
        Mesh.AppendTriangle(1+(Rows-2)*Columns+Next,1+(Rows-2)*Columns+Col,South);
    }
    FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
    Mesh.Attributes()->EnablePrimaryColors();
    auto* Colors=Mesh.Attributes()->PrimaryColors();
    for(int V:Mesh.VertexIndicesItr())Colors->AppendElement(FVector4f(Mesh.GetVertexColor(V),1));
    for(int T:Mesh.TriangleIndicesItr())Colors->SetTriangle(T,Mesh.GetTriangle(T));
    auto* Surface=NewObject<UDynamicMeshComponent>(this,TEXT("SculptedCerebralCortex"),RF_Transactional);
    AddInstanceComponent(Surface);Surface->ComponentTags.Add(TEXT("EmergenceEndingGenerated"));
    Surface->SetupAttachment(Sets[0]);Surface->SetRelativeLocation(FVector(19700,0,1200));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);Surface->SetCastShadow(false);Surface->SetEnableRaytracing(false);
    Surface->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_Cortex")));
    Surface->SetMesh(MoveTemp(Mesh));Surface->RegisterComponent();

    // A substantial cortical bed immediately beneath the hosts. Its raised
    // lateral banks keep the player inside tissue, rather than in an empty globe.
    FDynamicMesh3 Bed(true,true,false,false);
    constexpr int LongRows=220,Across=120;
    for(int Row=0;Row<=LongRows;++Row)for(int Col=0;Col<=Across;++Col)
    {
        const float X=12400+14600.f*Row/LongRows,U=2.f*Col/Across-1;
        const float Width=4500*FMath::Sqrt(FMath::Max(.12f,1-FMath::Square((X-19700)/8600.f)));
        const float Y=U*Width;
        const float Ground=-8146.21125f+FMath::Sqrt(FMath::Max(0.f,10500*10500.f-FMath::Square(X-19700)-Y*Y));
        // Rounded winding gyri, separated by deep narrow sulci, not rock noise.
        const float Wave=FMath::Sin(Y*.0065f+1.6f*FMath::Sin(X*.0024f)+.65f*FMath::Sin(X*.0051f));
        const float Ridge=FMath::Sqrt(FMath::Max(0.f,1-Wave*Wave));
        const float Bank=FMath::SmoothStep(.48f,.95f,FMath::Abs(U));
        const float Midline=FMath::Exp(-FMath::Square(Y/160.f));
        const float Z=Ground-510+230*Ridge+1550*Bank-130*Midline;
        const int Id=Bed.AppendVertex(FVector3d(X,Y,Z));
        Bed.SetVertexColor(Id,FVector3f(Ridge,Bank,Midline));
    }
    for(int Row=0;Row<LongRows;++Row)for(int Col=0;Col<Across;++Col)
    {
        const int A=Row*(Across+1)+Col,B=A+Across+1;
        Bed.AppendTriangle(A,B,A+1);Bed.AppendTriangle(A+1,B,B+1);
    }
    FMeshNormals::QuickComputeVertexNormals(Bed);Bed.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Bed.Attributes()->PrimaryNormals());
    Bed.Attributes()->EnablePrimaryColors();auto* BedColors=Bed.Attributes()->PrimaryColors();
    for(int V:Bed.VertexIndicesItr())BedColors->AppendElement(FVector4f(Bed.GetVertexColor(V),1));
    for(int T:Bed.TriangleIndicesItr())BedColors->SetTriangle(T,Bed.GetTriangle(T));
    auto* Tissue=NewObject<UDynamicMeshComponent>(this,TEXT("InteriorCorticalBed"),RF_Transactional);
    AddInstanceComponent(Tissue);Tissue->ComponentTags.Add(TEXT("EmergenceEndingGenerated"));
    Tissue->SetupAttachment(Sets[0]);Tissue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tissue->SetCastShadow(false);Tissue->SetEnableRaytracing(false);
    Tissue->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_CorticalTissue")));
    Tissue->SetMesh(MoveTemp(Bed));Tissue->RegisterComponent();
}
