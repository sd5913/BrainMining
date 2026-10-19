#include "MiningGamepadSubsystem.h"
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
        const float Lean[]={-.13f,.09f,-.065f,.12f,-.16f},Width[]={1.f,.90f,1.f,1.06f,.92f},Lift[]={-26000,20000,0,-14000,32000};
        const FVector Centre(0,(Glyph-2)*445000.f,Glyph==2?-85000.f:0.f);
        FVector Letter=P-Centre;Letter.Y*=Width[Glyph];
        Letter=FQuat(FVector::ForwardVector,Lean[Glyph]).RotateVector(Letter);
        P=Centre+Letter+FVector(FMath::Sin(I*2.399f)*3500,0,Lift[Glyph]);
        TitleTargets.Add(P);TitleGlyphs.Add(Glyph);
    }
    // Neighbours choose different letters instead of moving as orderly rows.
    FRandomStream Gather(68237);
    for(int I=TitleTargets.Num()-1;I>0;--I){const int J=Gather.RandRange(0,I);TitleTargets.Swap(I,J);TitleGlyphs.Swap(I,J);}
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
    FinaleLight=Shape(TEXT("FinaleExitLight"),FVector(9150,0,0),FVector(4,75,75),FLinearColor(10,10,10),TEXT("Sphere"),FinaleTunnel,TEXT("Glow"));
    FinaleTunnel->SetVisibility(false,true);

    BuildPaperFinale();
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
    TitleAnimatedCenters.SetNumUninitialized(TitleCenters.Num());
    TitleAnimatedScales.SetNumUninitialized(TitleCenters.Num());
    // Each cell owns 64 stars. Evaluate its drift once, not once per star.
    for(int OwnerCell=0;OwnerCell<TitleCenters.Num();++OwnerCell)
    {
        const float Phase=OwnerCell*2.39996f;
        const float Seed=.5f+.5f*FMath::Sin(Phase*7.13f),Delay=Seed*.23f;
        const float Progress=Finale::Ease((Alpha-Delay)/(1-Delay));
        const float Idle=Progress*(Shot==8?1-Finale::Ease(ShotTime/.95f):1);
        const float CellScale=FMath::Lerp(1.f,.17f,Progress)*(1+Idle*.045f*FMath::Sin(Age*1.2f+Phase));
        const float Rate=.12f+Seed*.18f;
        const FVector Float=FVector(
            .65f*FMath::Sin(Age*Rate+Phase)+.35f*FMath::Sin(Age*Rate*.43f+Phase*3),
            .65f*FMath::Sin(Age*Rate*.71f+Phase*1.3f)+.35f*FMath::Cos(Age*Rate*1.21f+Phase),
            .65f*FMath::Cos(Age*Rate*.87f+Phase)+.35f*FMath::Sin(Age*Rate*.57f+Phase*2))*6500*FMath::Lerp(.55f,1.f,FMath::Square(1-Alpha));
        const FVector Bend=FVector(FMath::Sin(Phase*1.7f)*.5f,FMath::Cos(Phase*.87f),FMath::Sin(Phase*1.31f))*(45000+Seed*125000)*FMath::Sin(PI*Progress);
        const int Glyph=TitleGlyphs[OwnerCell];const float Beat=Age*.85f+Glyph*1.43f;
        const FVector Pivot(0,(Glyph-2)*445000.f,Glyph==2?-85000.f:0.f);
        const FVector Target=Pivot+FQuat(FVector::ForwardVector,.045f*Idle*FMath::Sin(Beat)).RotateVector(TitleTargets[OwnerCell]-Pivot)
            +FVector(9000*FMath::Sin(Beat*.8f),4000*FMath::Cos(Beat),11000*FMath::Sin(Beat))*Idle;
        const FVector Center=FMath::Lerp(TitleCenters[OwnerCell],Target,Progress)+Bend+Float;
        TitleAnimatedCenters[OwnerCell]=Center;TitleAnimatedScales[OwnerCell]=CellScale;
    }
    TitleWorkingPoses.SetNum(TitleBatches.Num());
    for(int Batch=0;Batch<TitleBatches.Num();++Batch)
    {
        auto& Poses=TitleWorkingPoses[Batch];Poses=TitleOriginals[Batch];
        const int PerCell=Batch==3?42:Batch==4?22:1;
        for(int I=0;I<Poses.Num();++I)
        {
            const int OwnerCell=I/PerCell;if(!TitleCenters.IsValidIndex(OwnerCell))continue;
            const float CellScale=TitleAnimatedScales[OwnerCell];
            Poses[I].SetLocation(TitleAnimatedCenters[OwnerCell]+(Poses[I].GetLocation()-TitleCenters[OwnerCell])*CellScale);
            Poses[I].SetScale3D(Poses[I].GetScale3D()*CellScale);
        }
        TitleBatches[Batch]->BatchUpdateInstancesTransforms(0,Poses,false,true,true);
    }
    const FVector Main=TitleAnimatedCenters[0]+FVector(0,12000,0);
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
        Rock->RockMesh->SetCastShadow(false);Rock->SetActorHiddenInGame(true);
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
        // Keep the original cinematic framing. The shared gameplay pawn has a
        // different model offset; copying it lifts both drills across the lens.
        Mesh->SetRelativeTransform(FTransform(FRotator(-10,0,0),
            FVector(Left?-31.280714:-21.432637,0,Left?-14.638632:-16.375114),FVector(.72,Left?-.72:.72,.72)));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetVisibleInRayTracing(false);
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
    // Prepared tools may retain the exit fade after a preview or shot seek.
    // Every mining entrance must restore opaque, coloured drills.
    for(auto Mat:FinaleDrillMaterials)
    {
        Mat->SetScalarParameterValue(TEXT("Fade"),1);
        Mat->SetScalarParameterValue(TEXT("InkExit"),0);
        Mat->SetScalarParameterValue(TEXT("WhiteInk"),0);
    }
    for(auto Rock:FinaleRocks)if(IsValid(Rock))Rock->SetActorHiddenInGame(false);
    FinaleEntrancePreview->SetVisibility(false,true);
    for(auto Pivot:FinaleDrillPivots)Pivot->SetVisibility(true,true);
    FinaleTunnel->SetVisibility(true,true);
    FinaleLight->SetRelativeScale3D(FVector(.04f,.75f,.75f));
    ResetPaperFinale();
    // Reuse the tabletop aperture during drilling: same 75 cm diameter,
    // material and world position, so the on-screen circle cannot jump at exit.
    PaperScene->SetVisibility(false,true);PaperScene->SetVisibility(true);
    PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),37.5f);
    PaperSurface->SetVisibility(true);FinaleLight->SetVisibility(false);
    ToolLight->SetVisibility(true);ToolLight->SetIntensity(1800);
    Pose(Finale::Tunnel,Finale::Tunnel+FVector(1000,0,0),86);
    Mining->BeginMiningInput();SetAmbience(TEXT("Drill_Loop"),.24f);Sound(TEXT("DrillHit_01"),.16f,.82f);
}

void AEndingEmergence::FinaleDrillHit(const FHitResult& Hit)
{
    if(!bFinaleDrilling||!FinaleRocks.Contains(Cast<ADiggableRock>(Hit.GetActor())))return;
    ++FinaleDrillCuts;FinaleKick=Finale::Impact(DrillDistance);
    Sound(TEXT("DrillHit_01"),.28f,.90f+(FinaleDrillCuts%4)*.045f);
    if(FinaleDrillCuts%4==0)Sound(TEXT("DrillDebris_01"),.20f,.92f+(FinaleDrillCuts%3)*.04f);
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
        APawn* Pawn=Player?Player->GetPawn():nullptr;
        if(auto* Effects=Pawn?Pawn->FindComponentByClass<UMiningToolEffectsComponent>():nullptr)
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
    // Keep the reference's peripheral white strokes in the black flight, then
    // clear them before the camera reaches the physical white desktop.
    if(ExitInkMaterial)ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),.6f*(1-Finale::Ease((ShotTime-1.5f)/.8f)));
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
    if(FinaleTunnel)FinaleTunnel->SetVisibility(false,true);
    if(FinaleLight){FinaleLight->SetRelativeScale3D(FVector(.24f));FinaleLight->SetVisibility(false);}
    ResetPaperFinale();if(PaperScene)PaperScene->SetVisibility(false,true);
    DrillDistance=FinaleDrillSpeed=FinaleDrillCuts=0;TitleUpdateAt=-1;
}

void AEndingEmergence::UpdateFinale(float Dt)
{
    if(Shot==7)
    {
        for(auto Mat:FinalePreviewMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),0);
        const float T=Finale::Ease(ShotTime/10);
        if(Age-TitleUpdateAt>1.f/30){UpdateCellTitle(T);TitleUpdateAt=Age;}
        const float U=FMath::Clamp(ShotTime/Duration(7),0.f,1.f);
        const FVector Incoming=TitleCameraVelocity.GetClampedToMaxSize(FVector::Dist(TitleCameraStart,Finale::TitleEye)*2/Duration(7));
        Pose(Finale::Cosmos+FMath::CubicInterp(TitleCameraStart,Incoming*Duration(7),Finale::TitleEye,FVector::ZeroVector,U),Finale::Cosmos+FVector(0,0,30000),62);
        Camera->SetActorRotation(FQuat::Slerp(TitleLookStart.Quaternion(),Camera->GetActorQuat(),Finale::Ease(ShotTime/2)));
    }
    else if(Shot==8)
    {
        if(Age-TitleUpdateAt>1.f/30){UpdateCellTitle(1);TitleUpdateAt=Age;}
        for(auto Mat:FinalePreviewMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),Finale::Ease((ShotTime-2.15f)/1.65f));
        const float T=FMath::Clamp(ShotTime/Duration(8),0.f,1.f),Fall=T*T*T*(T*(T*6-15)+10);
        Pose(Finale::Cosmos+FMath::Lerp(Finale::TitleEye,Finale::Hole+FVector(-300,0,0),Fall),Finale::Cosmos+FMath::Lerp(FVector(0,0,30000),Finale::Hole,Finale::Ease(T*2)),FMath::Lerp(62.f,86.f,Finale::Ease(T)),bComfortMode?0:12*FMath::Sin(PI*T));
    }
    else if(Shot==9)
    {
        const float Boost=FMath::Lerp(1.f,1.7f,FMath::Max(0.f,float(UMiningGamepadSubsystem::ReadMove(Player).Y)));
        FinaleDrillSpeed=FMath::FInterpTo(FinaleDrillSpeed,Finale::Pace(DrillDistance)*Boost,Dt,4);
        Mining->DrillIntervalMultiplier=.27f*260/(Finale::Pace(DrillDistance)*Boost);
        if(!Mining->IsMiningActive())Mining->BeginMiningInput();
        const float Progress=DrillDistance/Finale::DrillEnd,Roll=bComfortMode?FMath::Sin(Progress*PI)*7:1080*FMath::Square(FMath::Max(0.f,(Progress-.18f)/.82f));
        const float Steering=UMiningGamepadSubsystem::ReadMove(Player).X;
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
    else if(Shot>=10)UpdatePaperFinale(Dt);
}
