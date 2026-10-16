#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "EndingComicRock.h"
#include "MiningComponent.h"
#include "MiningToolEffectsComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "UDynamicMesh.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace Finale
{
    const FVector Cosmos(95000,0,0),Tunnel(2200000,0,0),Desk(155000,0,0);
    const FVector TitleEye(-2300000,0,100000),Hole(0,0,-85000);
    constexpr float DrillEnd=7800;
    const FLinearColor Palette[]={FLinearColor(.48f,.25f,.10f),FLinearColor(.25f,.16f,.65f),FLinearColor(.40f,.045f,.055f),FLinearColor(1.04f,.55f,.13f),FLinearColor(1.05f,.94f,.65f),FLinearColor::White};
    int Stage(float Distance){return FMath::Clamp(FMath::FloorToInt(Distance/1300),0,5);}
    float Ease(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
    float Pace(float Distance){return FMath::Lerp(175.f,370.f,Ease(Distance/6500.f));}
    float Impact(float Distance){return FMath::Lerp(.32f,1.f,Ease(Distance/6500.f));}
}

void AEndingEmergence::BuildFinale()
{
    // Keep each membrane, nucleus and its stars together, including during replays.
    TArray<UInstancedStaticMeshComponent*> Components;GetComponents(Components);
    const TCHAR* Names[]={TEXT("DenseCellMembranes"),TEXT("DenseCellNuclei"),TEXT("DenseCellGalaxies"),TEXT("UniversesInOtherCells"),TEXT("WarmUniversesInOtherCells")};
    for(const TCHAR* Name:Names)for(auto* C:Components)if(C->GetName().StartsWith(Name))
    {
        TitleBatches.Add(C);TArray<FTransform>& Original=TitleOriginals.AddDefaulted_GetRef();
        for(int I=0;I<C->GetInstanceCount();++I){FTransform T;C->GetInstanceTransform(I,T);Original.Add(T);}
        if(TitleCenters.IsEmpty())for(const FTransform& T:Original)TitleCenters.Add(T.GetLocation());
        break;
    }
    const int Count=TitleCenters.Num();
    for(int I=0;I<Count;++I)
    {
        const int Glyph=FMath::Min(4,I*5/Count),Start=FMath::DivideAndRoundUp(Glyph*Count,5),End=FMath::DivideAndRoundUp((Glyph+1)*Count,5);
        const float U=float(I-Start)/FMath::Max(1,End-Start-1),Offset=(I%2?1:-1)*8500.f;
        FVector P(0,(Glyph-2)*445000.f,0);
        if(Glyph==0||Glyph==4)
        {
            const FVector2D Corners[]={{-180000,240000},{-95000,-240000},{0,65000},{95000,-240000},{180000,240000}};
            const float Along=U*4;const int Segment=FMath::Min(3,FMath::FloorToInt(Along));
            const FVector2D V=FMath::Lerp(Corners[Segment],Corners[Segment+1],Along-Segment);
            P+=FVector(0,V.X+Offset,V.Y);
        }
        else
        {
            const float A=U*2*PI,Ry=Glyph==2?135000:180000,Rz=Glyph==2?155000:240000;
            P+=FVector(0,FMath::Sin(A)*(Ry+Offset),FMath::Cos(A)*(Rz+Offset)+(Glyph==2?-85000:0));
        }
        P.X=FMath::Sin(I*2.399f)*3500;TitleTargets.Add(P);
    }
    // Neighbours choose different letters instead of moving as orderly rows.
    FRandomStream Gather(68237);
    for(int I=TitleTargets.Num()-1;I>0;--I)TitleTargets.Swap(I,Gather.RandRange(0,I));
    // Actual faceted geometry carries the ink; the tunnel is not a tiled pattern.
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ExitInk")))
    {ExitInkMaterial=UMaterialInstanceDynamic::Create(Base,this);Camera->GetCameraComponent()->AddOrUpdateBlendable(ExitInkMaterial,1);}
    FinaleTunnel=Group(TEXT("FinaleDrillTunnel"),Finale::Tunnel);FinaleTunnel->SetVisibility(false,true);
    for(int Stage=0;Stage<6;++Stage)
    {
        auto* Wall=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);Wall->SetupAttachment(FinaleTunnel);
        Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);Wall->SetCastShadow(false);Wall->SetEnableRaytracing(false);Wall->SetVisibleInRayTracing(false);
        Wall->SetCanEverAffectNavigation(false);Wall->SetMesh(EndingComic::Tunnel(Stage));
        auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ComicRock")),Wall);
        Mat->SetVectorParameterValue(TEXT("Tint"),Finale::Palette[Stage]*.72f);Mat->SetScalarParameterValue(TEXT("MeshInk"),1);Mat->SetScalarParameterValue(TEXT("WhiteInk"),Stage==5?1.f:0.f);
        Wall->SetMaterial(0,Mat);Wall->RegisterComponent();AddInstanceComponent(Wall);
    }
    FinaleImpactShards=Instances(TEXT("FinaleComicShards"),TEXT("Cube"),Finale::Palette[0],FinaleTunnel,TEXT("ComicRock"));
    if(EndingDebrisMeshes.Num()>2)FinaleImpactShards->SetStaticMesh(EndingDebrisMeshes[2]);
    if(auto* Mat=Cast<UMaterialInstanceDynamic>(FinaleImpactShards->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("MeshInk"),0);
    FinaleSpinCurve=LoadObject<UCurveFloat>(nullptr,TEXT("/Game/Drill/CF_DrillSpin"));
    FinaleImpactLines=Instances(TEXT("FinaleComicImpact"),TEXT("Cube"),FLinearColor(1,.83f,.48f),FinaleTunnel,TEXT("ExitStroke"));
    FinaleSpeedLines=Instances(TEXT("FinaleComicSpeed"),TEXT("Cube"),FLinearColor(.5f,.7f,.8f),FinaleTunnel,TEXT("ExitStroke"));
    FinaleImpactAges.Init(10,6);FinaleImpactOrigins.Init(FVector::ZeroVector,6);
    const FTransform Hidden(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);
    for(int I=0;I<144;++I)FinaleImpactShards->AddInstance(Hidden);
    for(int I=0;I<120;++I)FinaleImpactLines->AddInstance(Hidden);
    for(int I=0;I<64;++I)FinaleSpeedLines->AddInstance(Hidden);
    FinaleLight=Shape(TEXT("FinaleExitLight"),FVector(8400,0,0),FVector(24),FLinearColor(3,3,3),TEXT("Sphere"),FinaleTunnel,TEXT("Glow"));
    FinaleTunnel->SetVisibility(false,true);

    // The camera arrives at seated eye height. Four distinct bulbs frame the monitor.
    FinaleReality=Group(TEXT("FinaleRealityDesk"),Finale::Desk);
    const FLinearColor Wood(.24f,.12f,.058f),Cream(.42f,.38f,.30f),Dark(.018f,.025f,.035f);
    Shape(TEXT("FinaleFloor"),FVector(0,0,-5),FVector(1400,1300,10),Wood*.38f,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleRoomBack"),FVector(440,0,230),FVector(12,1100,470),Cream*.6f,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleRoomSide"),FVector(0,550,230),FVector(880,12,470),Cream*.75f,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleWindow"),FVector(160,540,240),FVector(310,5,250),FLinearColor(.65f,.44f,.22f),TEXT("Cube"),FinaleReality,TEXT("Glow"));
    for(int I=-1;I<=1;++I)Shape(TEXT("FinaleWindowFrame"),FVector(160+I*105,532,240),FVector(6,8,255),Wood,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleDesktop"),FVector(45,0,78),FVector(300,290,12),Wood,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleMonitorBase"),FVector(100,0,86),FVector(36,62,5),Dark,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleMonitorStem"),FVector(110,0,112),FVector(10,12,50),Dark,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleMonitor"),FVector(108,0,159),FVector(9,136,84),Dark,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleScreen"),FVector(102,0,159),FVector(1,125,73),FLinearColor(.02f,.065f,.09f),TEXT("Cube"),FinaleReality,TEXT("Glow"));
    // A level viewport and side panels establish a development workstation.
    Shape(TEXT("FinaleViewport"),FVector(101,-12,158),FVector(.5f,80,48),FLinearColor(.025f,.14f,.17f),TEXT("Cube"),FinaleReality,TEXT("Glow"));
    for(int I=0;I<9;++I)Shape(TEXT("FinaleEditorLines"),FVector(100,47,183-I*6),FVector(.4f,22-(I%3)*4,1),FLinearColor(.13f,.24f,.25f),TEXT("Cube"),FinaleReality,TEXT("Glow"));
    auto* Title=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);Title->SetupAttachment(FinaleReality);
    Title->SetRelativeLocationAndRotation(FVector(99,-12,161),FRotator(0,180,0));Title->SetHorizontalAlignment(EHTA_Center);
    Title->SetText(FText::FromString(TEXT("WOoOW")));Title->SetWorldSize(13);Title->SetTextRenderColor(FColor(240,211,133));Title->RegisterComponent();
    Shape(TEXT("FinaleKeyboard"),FVector(30,-5,87),FVector(34,95,4),Dark,TEXT("Cube"),FinaleReality);
    auto* Keys=Instances(TEXT("FinaleKeyboardKeys"),TEXT("Cube"),Cream*.55f,FinaleReality);
    for(int X=0;X<4;++X)for(int Y=0;Y<13;++Y)Keys->AddInstance(FTransform(FRotator::ZeroRotator,FVector(19+X*7,-46+Y*7,90),FVector(.05f,.05f,.02f)));
    Shape(TEXT("FinaleMousePad"),FVector(25,84,86),FVector(44,43,1),Dark*.7f,TEXT("Cube"),FinaleReality);
    Shape(TEXT("FinaleMouse"),FVector(25,84,89),FVector(14,9,6),Cream*.4f,TEXT("Sphere"),FinaleReality);
    Shape(TEXT("FinaleCup"),FVector(65,-106,101),FVector(20,20,30),Cream,TEXT("Cylinder"),FinaleReality);
    Shape(TEXT("FinaleCoffee"),FVector(65,-106,117),FVector(18,18,1),Wood*.1f,TEXT("Cylinder"),FinaleReality);
    for(int I=0;I<5;++I){auto* Note=Shape(TEXT("FinaleDeskNote"),FVector(-5+I*15,-100,86+I*.3),FVector(22,26,.4),Cream*1.4f,TEXT("Cube"),FinaleReality);Note->SetRelativeRotation(FRotator(0,I*13,0));}
    for(int Side:{-1,1})
    {
        Rod(FVector(-95,Side*45,98),FVector(-20,Side*34,92),15,FLinearColor(.04f,.11f,.14f),FinaleReality,TEXT("Surface"));
        Shape(TEXT("FinaleRestingHand"),FVector(-10,Side*33,93),FVector(26,15,8),FLinearColor(.42f,.25f,.14f),TEXT("Sphere"),FinaleReality);
    }
    for(int I=0;I<4;++I)
    {
        const FVector P(88,I%2?102:-102,I<2?155:235);
        Shape(TEXT("FinaleBulbGlass"),P,FVector(46,46,54),FLinearColor(.32f,.22f,.07f),TEXT("Sphere"),FinaleReality,TEXT("Atmosphere"));
        Shape(TEXT("FinaleBulbNeck"),P-FVector(0,0,30),FVector(21,21,20),FLinearColor(.25f,.19f,.09f),TEXT("Cylinder"),FinaleReality);
        for(int J=0;J<4;++J)Shape(TEXT("FinaleBulbScrew"),P-FVector(0,0,24+J*5),FVector(24,24,2),Dark*3,TEXT("Cylinder"),FinaleReality);
        auto* Glow=Shape(TEXT("FinaleBulbFilament"),P,FVector(20,20,26),FLinearColor(2,1.1f,.25f),TEXT("Sphere"),FinaleReality,TEXT("Glow"));
        FinaleBulbMaterials.Add(Cast<UMaterialInstanceDynamic>(Glow->GetMaterial(0)));
        auto* Light=NewObject<UPointLightComponent>(this,NAME_None,RF_Transient);Light->SetupAttachment(FinaleReality);Light->SetRelativeLocation(P);
        Light->SetLightColor(FLinearColor(1,.7f,.3f));Light->SetIntensity(250);Light->SetAttenuationRadius(320);Light->SetCastShadows(false);Light->RegisterComponent();FinaleBulbLights.Add(Light);
    }
    FinaleReality->SetVisibility(false,true);
    // A scaled copy of the real entrance occupies the o opening. At the end of
    // the dive its projected geometry matches the full-size excavation camera.
    FinaleEntrancePreview=Group(TEXT("FinaleEntrancePreview"),Finale::Cosmos+Finale::Hole);
    FinaleEntrancePreview->SetRelativeScale3D(FVector(350));
    auto AddPreview=[&](UE::Geometry::FDynamicMesh3 Mesh,FVector Location)
    {
        auto* Wall=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);Wall->SetupAttachment(FinaleEntrancePreview);
        Wall->SetRelativeLocation(Location);Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Wall->SetCastShadow(false);Wall->SetEnableRaytracing(false);Wall->SetVisibleInRayTracing(false);Wall->SetCanEverAffectNavigation(false);
        Wall->SetMesh(MoveTemp(Mesh));
        auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ComicRock")),Wall);
        Mat->SetVectorParameterValue(TEXT("Tint"),Finale::Palette[0]*.72f);Mat->SetScalarParameterValue(TEXT("MeshInk"),1);
        Mat->SetScalarParameterValue(TEXT("WorldScale"),350);Mat->SetScalarParameterValue(TEXT("Reveal"),0);
        Wall->SetMaterial(0,Mat);Wall->RegisterComponent();AddInstanceComponent(Wall);FinalePreviewMaterials.Add(Mat);
    };
    AddPreview(EndingComic::Tunnel(0),FVector::ZeroVector);
    auto Front=EndingComic::Volume(FVector(620,700,700));EndingComic::InkFacets(Front);AddPreview(MoveTemp(Front),FVector(610,0,0));
    FinaleEntrancePreview->SetVisibility(false,true);
    // Quiet neutral specks, at several depths, like matter suspended in a dish.
    auto* SpeckRoot=Group(TEXT("FinaleCellSpeckField"),Finale::Cosmos);
    FinaleCellSpecks=Instances(TEXT("FinaleCellSpecks"),TEXT("Sphere"),FLinearColor(.34f,.35f,.38f),SpeckRoot,TEXT("CellSpeck"));
    FRandomStream Specks(66271);
    for(int I=0;I<1900;++I)
    {
        const FVector P(Specks.FRandRange(-650000,900000),Specks.FRandRange(-2700000,2700000),Specks.FRandRange(-1700000,1700000));
        const float Size=Specks.FRandRange(4,18);
        FinaleCellSpecks->AddInstance(FTransform(FQuat::Identity,P,FVector(Size,Size,Size*Specks.FRandRange(.7f,1.3f))));
    }
    FinaleCellSpecks->SetVisibility(false);
}

void AEndingEmergence::UpdateCellTitle(float Alpha)
{
    if(TitleBatches.Num()!=5||TitleTargets.IsEmpty())return;
    const float Scale=FMath::Lerp(1.f,.17f,Alpha);
    for(int Batch=0;Batch<TitleBatches.Num();++Batch)
    {
        TArray<FTransform> Poses=TitleOriginals[Batch];
        const int PerCell=Batch==3?42:Batch==4?22:1;
        for(int I=0;I<Poses.Num();++I)
        {
            const int OwnerCell=I/PerCell;if(!TitleCenters.IsValidIndex(OwnerCell))continue;
            const float Phase=OwnerCell*2.39996f;
            const float Seed=.5f+.5f*FMath::Sin(Phase*7.13f),Delay=Seed*.23f;
            const float Progress=Finale::Ease((Alpha-Delay)/(1-Delay));
            const float CellScale=FMath::Lerp(1.f,.17f,Progress);
            const float Rate=.12f+Seed*.18f;
            const FVector Float=FVector(
                .65f*FMath::Sin(Age*Rate+Phase)+.35f*FMath::Sin(Age*Rate*.43f+Phase*3),
                .65f*FMath::Sin(Age*Rate*.71f+Phase*1.3f)+.35f*FMath::Cos(Age*Rate*1.21f+Phase),
                .65f*FMath::Cos(Age*Rate*.87f+Phase)+.35f*FMath::Sin(Age*Rate*.57f+Phase*2))*6500*FMath::Lerp(.1f,1.f,FMath::Square(1-Alpha));
            const FVector Bend=FVector(FMath::Sin(Phase*1.7f)*.5f,FMath::Cos(Phase*.87f),FMath::Sin(Phase*1.31f))*(45000+Seed*125000)*FMath::Sin(PI*Progress);
            const FVector Center=FMath::Lerp(TitleCenters[OwnerCell],TitleTargets[OwnerCell],Progress)+Bend+Float;
            Poses[I].SetLocation(Center+(Poses[I].GetLocation()-TitleCenters[OwnerCell])*CellScale);
            Poses[I].SetScale3D(Poses[I].GetScale3D()*CellScale);
        }
        TitleBatches[Batch]->BatchUpdateInstancesTransforms(0,Poses,false,true,true);
    }
    const FVector Main=TitleTargets[0]+FVector(0,12000,0);
    Cell->SetRelativeLocation(Main*Alpha);Cell->SetRelativeScale3D(FVector(32*Scale));
    Universe->SetRelativeLocation(Main*Alpha);Universe->SetRelativeScale3D(FVector(Scale));
    bTitleRestored=false;
}

void AEndingEmergence::PrepareFinaleRocks()
{
    if(!FinaleRocks.IsEmpty())return;
    for(int I=0;I<12;++I)
    {
        const float X=610+I*620+(I==11?30:0);
        const FTransform T(GetActorQuat(),GetActorTransform().TransformPosition(Finale::Tunnel+FVector(X,0,0)));
        auto* Rock=GetWorld()->SpawnActorDeferred<AEndingComicRock>(AEndingComicRock::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Rock->VisualSeed=I+137;Rock->bContinuousVolume=true;Rock->SetFlags(RF_Transient);
        Rock->RockSize=FVector(I==11?680:620,700,700);Rock->MaxTriangles=12000;
        auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ComicRock")),Rock);
        Mat->SetVectorParameterValue(TEXT("Tint"),Finale::Palette[Finale::Stage(X)]*.72f);Mat->SetScalarParameterValue(TEXT("MeshInk"),1);Mat->SetScalarParameterValue(TEXT("WhiteInk"),Finale::Stage(X)==5?1.f:0.f);
        Rock->RockMaterial=Mat;Rock->FinishSpawning(T);FinaleRocks.Add(Rock);
        Rock->RockMesh->SetCastShadow(false);
        if(I){auto* Previous=Cast<AEndingComicRock>(FinaleRocks[I-1]);Previous->Next=Rock;Rock->Previous=Previous;}
    }
    // Prepare tools while the title is still visible, not on the entrance frame.
    TArray<UStaticMeshComponent*> PawnParts;Player->GetPawn()->GetComponents(PawnParts);
    auto* ComicDrill=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ComicDrill"));
    for(const TCHAR* Name:{TEXT("DrillRightModel"),TEXT("DrillLeftModel")})for(auto* Part:PawnParts)if(Part->GetName()==Name)
    {
        const bool Left=Part->GetName()==TEXT("DrillLeftModel");
        auto* Pivot=NewObject<USceneComponent>(this,NAME_None,RF_Transient);Pivot->SetupAttachment(Camera->GetCameraComponent());
        Pivot->SetRelativeLocationAndRotation(FVector(Left?95:85,Left?-28:28,3),FRotator(14,Left?18:-18,0));
        Pivot->RegisterComponent();FinaleDrillPivots.Add(Pivot);
        auto* Mesh=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transient);Mesh->SetupAttachment(Pivot);Mesh->SetStaticMesh(Part->GetStaticMesh());
        Mesh->SetRelativeTransform(Part->GetRelativeTransform());Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetVisibleInRayTracing(false);
        auto* ToolMat=UMaterialInstanceDynamic::Create(ComicDrill?ComicDrill:Part->GetMaterial(0),Mesh);
        ToolMat->SetScalarParameterValue(TEXT("Fade"),1);ToolMat->SetScalarParameterValue(TEXT("InkExit"),0);FinaleDrillMaterials.Add(ToolMat);
        for(int M=0;M<Part->GetNumMaterials();++M)Mesh->SetMaterial(M,ToolMat);
        Mesh->RegisterComponent();FinaleDrills.Add(Mesh);Pivot->SetVisibility(false,true);
    }
}

void AEndingEmergence::BeginFinaleDrill()
{
    if(bFinaleDrilling||!Mining)return;
    bFinaleDrilling=true;DrillDistance=FinaleDrillSpeed=FinaleSpin=0;FinaleDrillCuts=0;FinaleSide=FinaleSideSpeed=0;FinaleSideVector=FVector::ZeroVector;
    for(auto* Part:{FinaleImpactShards.Get(),FinaleImpactLines.Get(),FinaleSpeedLines.Get()})if(auto* Mat=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("Fade"),1);
    FinaleKick=0;FinaleImpactSlot=0;FinaleImpactAges.Init(10,6);
    if(auto* Effects=Player->GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>())
    {
        SavedEffectsEnabled=Effects->bEnabled;Effects->bEnabled=false;Effects->ResetEffects();
        SavedDrillSmoke=Effects->DrillSmokeStrength;SavedDrillDust=Effects->DrillDustStrength;SavedDrillSparks=Effects->DrillSparkStrength;
        Effects->DrillSmokeStrength=.18f;Effects->DrillDustStrength=.32f;Effects->DrillSparkStrength=.18f;
    }
    SavedInfiniteEnergy=Mining->bInfiniteDrillEnergy;SavedUseEnergy=Mining->bUseDrillEnergy;SavedEnergy=Mining->DrillEnergy;
    SavedRadius=Mining->Radius;SavedReach=Mining->Reach;SavedInterval=Mining->Interval;SavedDrillMultiplier=Mining->DrillIntervalMultiplier;
    SavedDepth=Mining->DepthMultiplier;SavedDownward=Mining->DownwardOffset;SavedInset=Mining->InsetFraction;SavedPreview=Mining->bShowPreview;
    Mining->bUseDrillEnergy=true;Mining->bInfiniteDrillEnergy=true;Mining->DrillEnergy=Mining->MaxDrillEnergy;
    Mining->Radius=145;Mining->Reach=420;Mining->Interval=.30f;Mining->DrillIntervalMultiplier=.27f*260/Finale::Pace(0);Mining->DepthMultiplier=1;
    Mining->DownwardOffset=0;Mining->InsetFraction=.05f;Mining->bShowPreview=false;
    Mining->SetInputSuspended(false);if(!Mining->bDrillMode)Mining->ActivateDrill();
    Mining->OnMiningHit.AddDynamic(this,&AEndingEmergence::FinaleDrillHit);
    PrepareFinaleRocks();
    FinaleEntrancePreview->SetVisibility(false,true);
    for(auto Pivot:FinaleDrillPivots)Pivot->SetVisibility(true,true);
    FinaleTunnel->SetVisibility(true,true);ToolLight->SetVisibility(true);ToolLight->SetIntensity(1800);
    Pose(Finale::Tunnel,Finale::Tunnel+FVector(1000,0,0),86);
    Mining->BeginMiningInput();SetAmbience(TEXT("Brain"),.32f);
}

void AEndingEmergence::FinaleDrillHit(const FHitResult& Hit)
{
    if(!bFinaleDrilling||!FinaleRocks.Contains(Cast<ADiggableRock>(Hit.GetActor())))return;
    ++FinaleDrillCuts;FinaleKick=Finale::Impact(DrillDistance);
    FinaleImpactOrigins[FinaleImpactSlot]=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint)-Finale::Tunnel;
    FinaleImpactAges[FinaleImpactSlot]=0;FinaleImpactSlot=(FinaleImpactSlot+1)%6;
}

void AEndingEmergence::UpdateFinaleDrillEffects(float Dt,float Boost)
{
    const float Progress=DrillDistance/Finale::DrillEnd;
    // Keep readable, chunky fragments even at low speed. Only the opening's
    // emission strength is softened; camera recoil retains its gentler curve.
    const float Strength=FMath::Lerp(.8f,1.f,Finale::Ease((FinaleDrillSpeed-175.f)/100.f));
    const int ShardCount=FMath::RoundToInt(24.f*Strength);
    const float MeshScale=100.f/FMath::Max(1.f,static_cast<float>(FinaleImpactShards->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2));
    FinaleKick=FMath::Max(0.f,FinaleKick-Dt*7);
    auto* ShardMat=Cast<UMaterialInstanceDynamic>(FinaleImpactShards->GetMaterial(0));
    if(ShardMat){ShardMat->SetVectorParameterValue(TEXT("Tint"),Finale::Palette[Finale::Stage(DrillDistance)]*.95f);ShardMat->SetScalarParameterValue(TEXT("Stage"),Finale::Stage(DrillDistance));ShardMat->SetScalarParameterValue(TEXT("WhiteInk"),Finale::Ease((DrillDistance-6200)/650));}
    // Fixed instance pools: no particle actors or mesh allocations per contact.
    for(int Burst=0;Burst<6;++Burst)
    {
        float& T=FinaleImpactAges[Burst];T+=Dt;
        for(int J=0;J<24;++J)
        {
            const float A=J*2.39996f+Burst*.78f,Speed=(90+(J%7)*28)*(.7f+Strength*.3f);
            FVector P=FinaleImpactOrigins[Burst]+FVector(-T*(110+J%5*55),FMath::Cos(A)*(18+T*Speed),FMath::Sin(A)*(18+T*Speed)-T*T*80);
            // Cull fragments before they cross the camera instead of filling the lens.
            const float Size=J<ShardCount&&T<.65f&&P.X>DrillDistance+55?(1-Finale::Ease(T/.65f))*(.12f+(J%4)*.065f)*MeshScale*(.85f+Strength*.15f):0;
            FinaleImpactShards->UpdateInstanceTransform(Burst*24+J,FTransform(FRotator(T*330+J*31,T*210,T*440),P,FVector(Size,Size*.65f,Size*1.5f)),false,false,true);
        }
        for(int J=0;J<20;++J)
        {
            const float A=J*2*PI/20+Burst*.37f+FMath::Sin(J*7.13f)*.10f,Radius=22+T*(260+(J%5)*65),Life=FMath::Clamp(1-T/.28f,0.f,1.f);
            const FVector Radial(0,FMath::Cos(A),FMath::Sin(A));
            const FVector P=FinaleImpactOrigins[Burst]+FVector(-12-T*50,0,0)+Radial*Radius;
            const FQuat Rotation=FQuat::FindBetweenNormals(FVector::ForwardVector,Radial);
            const float Visible=P.X>DrillDistance+55&&J%3!=0?Life*Strength:0;
            FinaleImpactLines->UpdateInstanceTransform(Burst*20+J,FTransform(Rotation,P,FVector((.32f+Progress*.28f)*(1+(J%4)*.22f)*Visible,.026f*Visible,.035f*Visible)),false,false,true);
        }
    }
    // Peripheral longitudinal strokes give parallax and acceleration without a
    // screen-covering flash. Keep the central sightline clear.
    for(int I=0;I<64;++I)
    {
        const float A=I*2.39996f,R=185+(I%5)*22,Speed=FinaleDrillSpeed*(1.2f+Progress);
        const float X=FMath::Fmod(I*73.7f+FinaleSpin*Speed,900.f);
        const float Size=(bComfortMode?.28f:1)*Strength;
        FinaleSpeedLines->UpdateInstanceTransform(I,FTransform(FQuat::Identity,FVector(DrillDistance+950-X,FMath::Cos(A)*R,FMath::Sin(A)*R),FVector((.35f+Progress*.8f)*Boost*Size,.013f*Size,.013f*Size)),false,false,true);
    }
    FinaleImpactShards->MarkRenderStateDirty();FinaleImpactLines->MarkRenderStateDirty();FinaleSpeedLines->MarkRenderStateDirty();
}

void AEndingEmergence::PoseFinaleTools()
{
    float Angle=FinaleSpin*720;
    if(FinaleSpinCurve){float A,B;FinaleSpinCurve->GetTimeRange(A,B);Angle=FinaleSpinCurve->GetFloatValue(A+FMath::Fmod(FinaleSpin,FMath::Max(.001f,B-A)));}
    for(int I=0;I<FinaleDrills.Num();++I)
    {
        auto* Mesh=FinaleDrills[I].Get();auto* Pivot=FinaleDrillPivots[I].Get();
        const FTransform Spin=UMiningToolEffectsComponent::BuildDrillSpinTransform(Mesh->GetRelativeTransform(),FVector::ForwardVector,FVector::ZeroVector,I?-Angle:Angle);
        const FQuat Base=FRotator(14,I?18:-18,0).Quaternion();
        Pivot->SetRelativeRotation(Base*Spin.GetRotation());
        Pivot->SetRelativeLocation(FVector((I?95:85)-FinaleKick*1.3f,I?-28:28,FMath::Lerp(-32.f,3.f,Shot==10?1.f:Finale::Ease(ShotTime/.8f)))+Base.RotateVector(Spin.GetTranslation()));
    }
}

void AEndingEmergence::EndFinaleDrill(bool KeepVisuals)
{
    if(bFinaleDrilling&&Mining)
    {
        if(auto* Effects=Player->GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>())
        {
            Effects->bEnabled=SavedEffectsEnabled;
            Effects->DrillSmokeStrength=SavedDrillSmoke;Effects->DrillDustStrength=SavedDrillDust;Effects->DrillSparkStrength=SavedDrillSparks;
            Effects->ResetEffects();
        }
        Mining->EndMiningInput();Mining->OnMiningHit.RemoveDynamic(this,&AEndingEmergence::FinaleDrillHit);Mining->SelectPickaxe();Mining->SetInputSuspended(true);
        Mining->bInfiniteDrillEnergy=SavedInfiniteEnergy;Mining->bUseDrillEnergy=SavedUseEnergy;Mining->DrillEnergy=SavedEnergy;
        Mining->Radius=SavedRadius;Mining->Reach=SavedReach;Mining->Interval=SavedInterval;Mining->DrillIntervalMultiplier=SavedDrillMultiplier;
        Mining->DepthMultiplier=SavedDepth;Mining->DownwardOffset=SavedDownward;Mining->InsetFraction=SavedInset;Mining->bShowPreview=SavedPreview;
    }
    bFinaleDrilling=false;
    if(KeepVisuals)return;
    if(FinaleImpactShards)FinaleImpactShards->SetVisibility(false);
    if(FinaleImpactLines)FinaleImpactLines->SetVisibility(false);
    if(FinaleSpeedLines)FinaleSpeedLines->SetVisibility(false);
    for(auto C:FinaleDrills)if(C)C->DestroyComponent();FinaleDrills.Empty();
    for(auto C:FinaleDrillPivots)if(C)C->DestroyComponent();FinaleDrillPivots.Empty();FinaleDrillMaterials.Empty();
    if(ToolLight){ToolLight->SetVisibility(false);ToolLight->SetIntensity(900);}
}

void AEndingEmergence::UpdateFinaleExit(float Dt)
{
    const float Fade=1-Finale::Ease(ShotTime/1.65f);
    FinaleSpin+=Dt*.65f;PoseFinaleTools();UpdateFinaleDrillEffects(Dt,1);
    for(auto Mat:FinaleDrillMaterials){Mat->SetScalarParameterValue(TEXT("Fade"),Fade);Mat->SetScalarParameterValue(TEXT("InkExit"),1-Fade);}
    for(auto* Part:{FinaleImpactShards.Get(),FinaleImpactLines.Get(),FinaleSpeedLines.Get()})
    {
        if(auto* Mat=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
        {Mat->SetScalarParameterValue(TEXT("Fade"),Fade);Mat->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.8f,.8f,.8f));}
        Part->SetVisibility(Fade>0);
    }
    for(auto Pivot:FinaleDrillPivots)Pivot->SetVisibility(Fade>0,true);
    if(ToolLight)ToolLight->SetIntensity(1800*Fade);
    if(ExitInkMaterial)ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),FMath::Sin(PI*Finale::Ease(ShotTime/Duration(10)))*.9f);
}

void AEndingEmergence::ResetFinale()
{
    EndFinaleDrill();if(ExitInkMaterial)ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),0);
    for(auto R:FinaleRocks)if(IsValid(R))R->Destroy();FinaleRocks.Empty();
    if(!bTitleRestored)
    {
        for(int B=0;B<TitleBatches.Num();++B)TitleBatches[B]->BatchUpdateInstancesTransforms(0,TitleOriginals[B],false,true,true);
        Cell->SetRelativeLocation(FVector::ZeroVector);Cell->SetRelativeScale3D(FVector(32));Universe->SetRelativeLocation(FVector::ZeroVector);Universe->SetRelativeScale3D(FVector(1));
        bTitleRestored=true;
    }
    if(FinaleEntrancePreview)FinaleEntrancePreview->SetVisibility(false,true);
    for(auto Mat:FinalePreviewMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),0);
    if(FinaleCellSpecks)FinaleCellSpecks->SetVisibility(false);
    if(FinaleTunnel)FinaleTunnel->SetVisibility(false,true);if(FinaleReality)FinaleReality->SetVisibility(false,true);
    if(FinaleLight)FinaleLight->SetRelativeScale3D(FVector(.24f));
    DrillDistance=FinaleDrillSpeed=FinaleDrillCuts=0;TitleUpdateAt=-1;
}

void AEndingEmergence::UpdateFinale(float Dt)
{
    if(Shot==7)
    {
        for(auto Mat:FinalePreviewMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),0);
        const float T=Finale::Ease(ShotTime/10);
        if(Age-TitleUpdateAt>1.f/30){UpdateCellTitle(T);TitleUpdateAt=Age;}
        Pose(Finale::Cosmos+FMath::Lerp(TitleCameraStart,Finale::TitleEye,Finale::Ease(ShotTime/12)),Finale::Cosmos+FVector(0,0,30000),62);
        Camera->SetActorRotation(FQuat::Slerp(TitleLookStart.Quaternion(),Camera->GetActorQuat(),Finale::Ease(ShotTime/2)));
    }
    else if(Shot==8)
    {
        for(auto Mat:FinalePreviewMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),Finale::Ease((ShotTime-.25f)/2.9f));
        const float T=FMath::Clamp(ShotTime/Duration(8),0.f,1.f),Fall=T*T*T*(T*(T*6-15)+10);
        Pose(Finale::Cosmos+FMath::Lerp(Finale::TitleEye,Finale::Hole+FVector(-300,0,0),Fall),Finale::Cosmos+FMath::Lerp(FVector(0,0,30000),Finale::Hole,Finale::Ease(T*2)),FMath::Lerp(62.f,86.f,Finale::Ease(T)),bComfortMode?0:12*FMath::Sin(PI*T));
    }
    else if(Shot==9)
    {
        const float Boost=Player->IsInputKeyDown(EKeys::W)?1.7f:1;
        FinaleDrillSpeed=FMath::FInterpTo(FinaleDrillSpeed,Finale::Pace(DrillDistance)*Boost,Dt,4);
        Mining->DrillIntervalMultiplier=.27f*260/(Finale::Pace(DrillDistance)*Boost);
        if(!Mining->IsMiningActive())Mining->BeginMiningInput();
        const float Progress=DrillDistance/Finale::DrillEnd,Roll=bComfortMode?FMath::Sin(Progress*PI)*7:1080*FMath::Square(FMath::Max(0.f,(Progress-.18f)/.82f));
        const float Steering=(Player->IsInputKeyDown(EKeys::D)?1.f:0.f)-(Player->IsInputKeyDown(EKeys::A)?1.f:0.f);
        FinaleSideSpeed=FMath::FInterpTo(FinaleSideSpeed,Steering*45,Dt,7);
        FinaleSide=FMath::Clamp(FinaleSide+FinaleSideSpeed*Dt,-55.f,55.f);
        FinaleSideVector=FRotator(0,0,Roll).RotateVector(FVector(0,FinaleSide,0));
        const FVector From=GetActorTransform().TransformPosition(Finale::Tunnel+FVector(DrillDistance,0,0)+FinaleSideVector);
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(EndingDrillRail),true,this);Query.AddIgnoredActor(Player->GetPawn());
        float Travel=FinaleDrillSpeed*Dt;
        if(GetWorld()->LineTraceSingleByChannel(Hit,From,From+GetActorForwardVector()*FMath::Max(300.f,Travel+80),ECC_Visibility,Query))Travel=FMath::Min(Travel,FMath::Max(0.f,Hit.Distance-80));
        DrillDistance=FMath::Min(Finale::DrillEnd,DrillDistance+Travel);

        UpdateFinaleDrillEffects(Dt,Boost);
        const float Kick=bComfortMode?0:FinaleKick;
        Pose(Finale::Tunnel+FVector(DrillDistance,0,0)+FinaleSideVector,Finale::Tunnel+FVector(DrillDistance+1000,0,0)+FinaleSideVector,86+Progress*8+Kick*1.8f,Roll+FMath::Sin(Age*73)*Kick*.55f);
        FinaleSpin+=Dt*Boost*(.45f+Progress*.85f);
        PoseFinaleTools();
        for(auto Mat:FinaleDrillMaterials)Mat->SetScalarParameterValue(TEXT("WhiteInk"),Finale::Ease((DrillDistance-6200)/650));
        for(auto* Part:{FinaleImpactLines.Get(),FinaleSpeedLines.Get()})if(auto* Mat=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
            Mat->SetVectorParameterValue(TEXT("Tint"),FMath::Lerp(FLinearColor(.8f,.75f,.65f),FLinearColor(.015f,.015f,.02f),Finale::Ease((DrillDistance-6200)/650)));
        TransitEffects(bComfortMode?0:Progress*.2f);
        if(DrillDistance>=Finale::DrillEnd)SetShot(10);
    }
    else if(Shot==10)
    {
        const float T=FMath::Clamp(ShotTime/Duration(10),0.f,1.f);
        Pose(Finale::Tunnel+FVector(FMath::Lerp(Finale::DrillEnd,8390.f,T*T),0,0)+FinaleSideVector*(1-Finale::Ease(T)),Finale::Tunnel+FVector(8500,0,0),FMath::Lerp(94.f,45.f,T),bComfortMode?0:1080+540*T*T);
        UpdateFinaleExit(Dt);
        FinaleLight->SetRelativeScale3D(FVector(.24f+Finale::Ease(T)*.5f));TransitEffects(bComfortMode?0:.2f+.5f*T);
    }
    else if(Shot==11)
    {
        Pose(Finale::Desk+FVector(-225,0,180+FMath::Sin(ShotTime*.8f)*.35f),Finale::Desk+FVector(105,0,170),68);
        for(int I=0;I<FinaleBulbMaterials.Num();++I)
        {
            const float Pulse=.15f+.85f*Finale::Ease((FMath::Sin(ShotTime*2.6f-I*1.3f)+.4f)/1.2f);
            FinaleBulbMaterials[I]->SetVectorParameterValue(TEXT("Tint"),FLinearColor(2.8f,1.5f,.38f)*Pulse);
            FinaleBulbLights[I]->SetIntensity(60+300*Pulse);
        }
    }
}
