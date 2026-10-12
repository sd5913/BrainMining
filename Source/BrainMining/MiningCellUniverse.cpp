#include "MiningCellUniverse.h"
#include "BrainMiningCharacter.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningInteractionHUDWidget.h"
#include "DiggableRock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace CellUniverse
{
    const FName GeneratedTag(TEXT("CellUniverseGenerated"));
    const FLinearColor Gold(.95f, .51f, .12f);
    const FLinearColor Cyan(.04f, .65f, .58f);
    const FLinearColor Rose(.68f, .075f, .19f);
    const FLinearColor Violet(.33f, .16f, .75f);
    const TCHAR* Names[] = {TEXT("细胞内"), TEXT("血液"), TEXT("组织"), TEXT("生物体"), TEXT("大陆"),
        TEXT("星球"), TEXT("星系"), TEXT("宇宙"), TEXT("细胞膜"), TEXT("巨人的血液"), TEXT("巨人 · 念头的出口")};
}

class SCellUniverseView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCellUniverseView) {} SLATE_ARGUMENT(AMiningCellUniverse*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Owner = Args._Owner; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (!Owner.IsValid()) return Layer;
        const auto* D = Owner.Get();
        const FVector2D Size = G.GetLocalSize();
        const float S = FMath::Min(Size.X / 1600, Size.Y / 900);
        const FVector2D O = (Size - FVector2D(1600, 900) * S) * .5;
        const auto* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FLinearColor Cream(.9f, .94f, .91f), Muted(.47f, .62f, .65f), Dark(.007f, .016f, .024f, .84f);
        auto Geo = [&](float X, float Y, float W, float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S), FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))); };
        auto Box = [&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++Layer,Geo(X,Y,W,H),Brush,ESlateDrawEffect::None,C); };
        auto Text = [&](FString T,float X,float Y,int F,FLinearColor C) { FSlateDrawElement::MakeText(Out,++Layer,Geo(X,Y,1480,80),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(9,FMath::RoundToInt(F*S))),ESlateDrawEffect::None,C); };
        Box(36,32,390,110,Dark); Box(36,32,3,110,CellUniverse::Gold);
        Text(TEXT("涌现  /  尺度尽头"),57,47,22,Cream);
        Text(TEXT("CELL / COSMOS    ·    FIRST-PERSON STUDY"),58,87,11,Muted);
        for(int32 I=0;I<D->StageCount;++I) Box(58+I*31,118,24,3,I<=D->CurrentStage?CellUniverse::Gold:Muted*.3f);
        Box(1190,32,374,83,Dark);
        Text(FString::Printf(TEXT("%02d / %02d"),D->CurrentStage+1,D->StageCount),1210,45,12,CellUniverse::Gold);
        Text(D->StageName(),1210,71,18,Cream);
        const auto* M=D->GetMining();
        const FLinearColor Aim=M&&M->TargetRock?CellUniverse::Cyan:Cream;
        if (!UMiningInteractionHUDWidget::IsActiveFor(D->GetWorld()->GetFirstPlayerController()))
        { Box(798,441,3,7,Aim); Box(798,453,3,7,Aim); Box(791,449,6,2,Aim); Box(803,449,6,2,Aim); }
        if(!D->bAreaManaged)
        {
            Box(36,781,1528,86,Dark);
            Text(D->Hint(),58,795,18,Cream);
            Text(TEXT("WASD 移动   鼠标 观察   按住左键 挖掘   按住空格 上升 / 松开 下落   R 从头再来"),58,835,13,Muted);
        }
        const FString Sub=D->Subtitle();
        if(!Sub.IsEmpty())
        {
            const float SubtitleY=D->bAreaManaged?180:676;
            Box(280,SubtitleY,1040,62,Dark); Box(280,SubtitleY,3,62,CellUniverse::Gold);
            Text(Sub,306,SubtitleY+17,23,Cream);
        }
        if(D->bComplete)
        {
            Box(408,272,784,128,Dark);
            Text(TEXT("挖通了。"),685,292,34,CellUniverse::Gold);
            Text(TEXT("我们终于知道，要做一个怎样的游戏。"),527,354,21,Cream);
        }
        return Layer;
    }
private:
    TWeakObjectPtr<AMiningCellUniverse> Owner;
};

AMiningCellUniverseGameMode::AMiningCellUniverseGameMode() { HUDClass=AHUD::StaticClass(); }
AMiningCellUniverse::AMiningCellUniverse()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    ApplyExpandedPreset();
}

void AMiningCellUniverse::ApplyExpandedPreset()
{
    if(GetWorld() && GetWorld()->IsGameWorld()) return;
    // cm; pacing, navigable volume and mineable patches are authored independently.
    const float Lengths[]={3600,4800,5600,7000,10000,14500,17000,22000,6500,10500,7000};
    const float Widths[]={2600,3200,3800,5500,16000,26000,36000,48000,6500,4600,5000};
    const float Heights[]={1500,1800,2200,3000,8000,18000,22000,28000,4500,2800,3400};
    const float DepthBelow[]={1200,1800,2500,4000,9000,24000,28000,36000,6000,3600,4200};
    const float OpenW[]={400,480,600,800,1400,2200,3000,4200,650,550,850};
    const float OpenH[]={360,400,480,650,1100,1800,2400,3200,550,500,850};
    const float Depths[]={260,280,320,360,400,440,480,520,320,320,480};
    const float EndScale[]={.72f,.65f,.65f,.55f,.94f,1.f,1.f,1.f,1.f,1.f,1.f};
    Stages.SetNum(StageCount);
    for(int32 I=0;I<StageCount;++I)
    {
        auto& S=Stages[I]; S.Name=CellUniverse::Names[I]; S.Length=Lengths[I];
        S.Width=Widths[I]; S.Height=Heights[I]; S.BelowPathDepth=DepthBelow[I]; S.OpeningWidth=OpenW[I];
        S.OpeningHeight=OpenH[I]; S.Depth=Depths[I]; S.EndScale=EndScale[I];
        S.MinimumScaleSeconds=I>=4?40.f:18.f;
    }
}

float AMiningCellUniverse::GetStageStart(int32 Index) const
{
    return StageStarts.IsValidIndex(Index)?StageStarts[Index]:0.f;
}

float AMiningCellUniverse::GetStageEnd(int32 Index) const
{
    return BuiltStages.IsValidIndex(Index)?GetStageStart(Index)+BuiltStages[Index].Length:0.f;
}

UStaticMeshComponent* AMiningCellUniverse::Shape(FString Name,FVector Position,FVector Size,FLinearColor Color,
    const TCHAR* Mesh,bool Collision,USceneComponent* Parent,const TCHAR* Surface)
{
    auto* C=NewObject<UStaticMeshComponent>(this,MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),FName(*Name)),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(CellUniverse::GeneratedTag);
    C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(Parent?Parent:RootComponent.Get());
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Engine/BasicShapes/"))+Mesh)));
    C->SetRelativeLocation(Position); C->SetRelativeScale3D(Size/100.0);
    C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
    C->SetGenerateOverlapEvents(false); C->SetCanEverAffectNavigation(false); C->SetCastShadow(false);
    auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/CellUniverse/Materials/"))+Surface));
    if(Base)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,C);
        M->SetVectorParameterValue(TEXT("Tint"),Color); C->SetMaterial(0,M);
    }
    C->RegisterComponent(); return C;
}

USceneComponent* AMiningCellUniverse::Group(FString Name,FVector Position)
{
    auto* C=NewObject<USceneComponent>(this,MakeUniqueObjectName(this,USceneComponent::StaticClass(),FName(*Name)),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(CellUniverse::GeneratedTag);
    C->SetupAttachment(RootComponent); C->SetRelativeLocation(Position); C->RegisterComponent(); return C;
}

void AMiningCellUniverse::Rod(FString Name,FVector A,FVector B,float Width,FLinearColor Color,USceneComponent* Parent)
{
    auto* C=Shape(Name,(A+B)*.5,FVector(Width,Width,(B-A).Size()),Color,TEXT("Cylinder"),false,Parent);
    C->SetRelativeRotation(FRotationMatrix::MakeFromZ(B-A).Rotator());
}

void AMiningCellUniverse::Ring(FString Name,FVector Center,float Radius,FLinearColor Color,USceneComponent* Parent)
{
    for(int32 I=0;I<24;++I)
    {
        const float A=I*2*PI/24, B=(I+1)*2*PI/24;
        Rod(Name,Center+FVector(0,FMath::Cos(A)*Radius,FMath::Sin(A)*Radius),
            Center+FVector(0,FMath::Cos(B)*Radius,FMath::Sin(B)*Radius),13,Color,Parent);
    }
}

void AMiningCellUniverse::BuildCell(int32 Stage)
{
    const float X=GetStageStart(Stage), Length=BuiltStages[Stage].Length;
    auto* L=Group(FString::Printf(TEXT("CellLandmark_%d"),Stage),FVector(X+1250,720,580)); Landmarks[Stage]=L;
    Shape(TEXT("Nucleus"),FVector::ZeroVector,FVector(650,580,650),CellUniverse::Violet,TEXT("Sphere"),false,L,TEXT("M_CU_Organic"));
    Shape(TEXT("Nucleolus"),FVector(-210,-150,30),FVector(180),CellUniverse::Gold,TEXT("Sphere"),false,L);
    FRandomStream R(Seed+Stage);
    for(int32 I=0;I<18;++I)
    {
        const FVector P(X+R.FRandRange(300,Length-500),R.FRandRange(-1100,1100),R.FRandRange(480,1350));
        auto* C=Shape(TEXT("Organelle"),P,FVector(100,230,100),I%2?CellUniverse::Cyan:CellUniverse::Rose,TEXT("Sphere"),false,nullptr,TEXT("M_CU_Organic"));
        C->SetRelativeRotation(FRotator(R.FRandRange(-40,40),R.FRandRange(-80,80),20));
        for(int32 J=0;J<3;++J) Shape(TEXT("OrganelleFold"),P+FVector(-25,J*40-40,0),FVector(85,12,80),CellUniverse::Gold*.35f,TEXT("Sphere"));
    }
    for(int32 I=0;I<5;++I)
        Ring(TEXT("Cytoskeleton"),FVector(X+250+I*(Length-650)/5,0,750),690,CellUniverse::Cyan*.3f);
}

void AMiningCellUniverse::BuildBlood(int32 Stage)
{
    const float X=GetStageStart(Stage), Length=BuiltStages[Stage].Length;
    FRandomStream R(Seed+Stage);
    for(int32 I=0;I<7;++I)
        Ring(TEXT("VesselRib"),FVector(X+130+I*(Length-400)/7,0,720),720,CellUniverse::Rose*.48f);
    for(int32 I=0;I<13;++I)
    {
        const FVector P(X+R.FRandRange(180,Length-280),I%2?R.FRandRange(560,1000):R.FRandRange(-1000,-560),R.FRandRange(260,1180));
        auto* C=Shape(TEXT("RedBloodCell"),P,FVector(240,240,80),FLinearColor(.7f,.035f,.075f),TEXT("Sphere"),false,nullptr,TEXT("M_CU_Blood"));
        C->SetRelativeRotation(FRotator(I*29,10,I*17)); BloodCells.Add(C); BloodOrigins.Add(P); BloodScales.Add(C->GetRelativeScale3D());
    }
    // A giant red cell directly beyond the universe membrane is the scale reversal.
    if(Stage==GiantBloodStage)
    {
        const FVector P(X+240,450,480);
        auto* C=Shape(TEXT("RevealRedCell"),P,FVector(520,160,520),CellUniverse::Rose,TEXT("Sphere"),false,nullptr,TEXT("M_CU_Blood"));
        BloodCells.Add(C); BloodOrigins.Add(P); BloodScales.Add(C->GetRelativeScale3D());
        auto* L=Group(TEXT("UniverseInsideCell"),FVector(X+2600,-1100,700)); Landmarks[GiantBloodStage]=L;
        Ring(TEXT("CellOutline"),FVector::ZeroVector,380,CellUniverse::Cyan,L);
        FRandomStream Stars(Seed);
        for(int32 I=0;I<65;++I)
        {
            const float A=I*.29f, D=30+I*4;
            Shape(TEXT("UniverseInCytoplasm"),FVector(Stars.FRandRange(-45,45),FMath::Cos(A)*D,FMath::Sin(A)*D),FVector(8),I%3?CellUniverse::Gold:CellUniverse::Violet,TEXT("Sphere"),false,L,TEXT("M_CU_Glow"));
        }
    }
}

void AMiningCellUniverse::BuildBug()
{
    const float X=GetStageStart(3), Length=BuiltStages[3].Length;
    auto* L=Group(TEXT("OrganismLandmark"),FVector(X+Length*.5f,1500,600)); Landmarks[3]=L;
    const FLinearColor Shell(.14f,.4f,.25f);
    Shape(TEXT("BeetleBody"),FVector::ZeroVector,FVector(650,430,350),Shell,TEXT("Sphere"),false,L);
    Shape(TEXT("BeetleHead"),FVector(-350,0,30),FVector(250,280,230),Shell*.65f,TEXT("Sphere"),false,L);
    Rod(TEXT("WingSeam"),FVector(-230,0,175),FVector(280,0,175),10,CellUniverse::Gold,L);
    for(int32 Side : {-1,1})
    {
        Shape(TEXT("Eye"),FVector(-440,Side*85,100),FVector(55),CellUniverse::Gold,TEXT("Sphere"),false,L,TEXT("M_CU_Glow"));
        Rod(TEXT("Antenna"),FVector(-435,Side*60,95),FVector(-570,Side*170,260),12,Shell,L);
        for(int32 I=0;I<3;++I)
        {
            const FVector A(-170+I*170,Side*135,0),B(-260+I*200,Side*350,-50),C(-330+I*240,Side*440,-260);
            Rod(TEXT("Leg"),A,B,24,Shell,L); Rod(TEXT("Leg"),B,C,18,Shell,L);
        }
    }
    FRandomStream R(Seed);
    for(int32 I=0;I<24;++I)
        Shape(TEXT("TerrainPebble"),FVector(X+R.FRandRange(150,Length-300),I%2?R.FRandRange(600,2200):R.FRandRange(-2200,-600),80),FVector(R.FRandRange(80,430)),FLinearColor(.14f,.12f,.075f),TEXT("Sphere"));
}

void AMiningCellUniverse::BuildTissue()
{
    const float X=GetStageStart(2), Length=BuiltStages[2].Length;
    auto* L=Group(TEXT("TissueLandmark"),FVector(X+Length*.55f,1100,900)); Landmarks[2]=L;
    for(int32 Row=0;Row<6;++Row)
        for(int32 Col=0;Col<5;++Col)
        {
            const FVector P(Row*340-850,(Col%2)*80,Col*300-600);
            Shape(TEXT("TissueCell"),P,FVector(370,220,330),FLinearColor(.18f,.33f,.24f),TEXT("Sphere"),false,L,TEXT("M_CU_Organic"));
            Shape(TEXT("TissueNucleus"),P+FVector(-40,-112,0),FVector(92,40,95),CellUniverse::Violet,TEXT("Sphere"),false,L);
        }
    for(int32 I=0;I<9;++I)
    {
        const float Y=-1450+I*130;
        Rod(TEXT("MuscleFiber"),FVector(X+300,Y,320+I*90),FVector(X+Length-600,Y+180,440+I*70),60,CellUniverse::Rose*.65f);
    }
    for(int32 I=0;I<12;++I)
        Rod(TEXT("Capillary"),FVector(X+300+I*380,-1500,180),FVector(X+700+I*370,-1100,500),22,CellUniverse::Rose);
}

void AMiningCellUniverse::BuildContinent()
{
    const float X=GetStageStart(4), Length=BuiltStages[4].Length;
    auto* L=Group(TEXT("ContinentLandmark"),FVector(X+Length*.55f,3600,-950)); Landmarks[4]=L;
    Shape(TEXT("ContinentalShelf"),FVector::ZeroVector,FVector(12000,6000,2000),FLinearColor(.045f,.14f,.08f),TEXT("Sphere"),false,L);
    Shape(TEXT("Ocean"),FVector(0,2500,-240),FVector(16000,9000,1800),FLinearColor(.012f,.07f,.16f),TEXT("Sphere"),false,L);
    FRandomStream R(Seed+4);
    for(int32 I=0;I<38;++I)
    {
        const FVector P(R.FRandRange(-4400,4400),R.FRandRange(-1600,1600),800);
        const float H=R.FRandRange(350,1500);
        Shape(TEXT("MountainRange"),P,FVector(H*.9f,H,H*1.3f),FLinearColor(.14f,.18f,.15f),TEXT("Cone"),false,L);
        if(I%3==0) Shape(TEXT("SnowPeak"),P+FVector(0,0,H*.5f),FVector(H*.21f,H*.24f,H*.35f),FLinearColor(.6f,.68f,.69f),TEXT("Cone"),false,L);
    }
    for(int32 I=0;I<15;++I)
        Rod(TEXT("River"),FVector(-4000+I*570,-1800+FMath::Sin(I*.65f)*450,1020),
            FVector(-4000+(I+1)*570,-1800+FMath::Sin((I+1)*.65f)*450,1020),34,CellUniverse::Cyan,L);
}

void AMiningCellUniverse::BuildPlanet()
{
    const float X=GetStageStart(5), Length=BuiltStages[5].Length;
    // 840 m planet against an unchanged ~2 m pawn. The horizon persists, never collapses into a toy.
    auto* L=Group(TEXT("PlanetLandmark"),FVector(X+Length*.52f,47000,-16000)); Landmarks[5]=L;
    Shape(TEXT("OceanAndContinents"),FVector::ZeroVector,FVector(84000),FLinearColor(.025f,.12f,.38f),TEXT("Sphere"),false,L,TEXT("M_CU_Planet"));
    Shape(TEXT("Atmosphere"),FVector::ZeroVector,FVector(86500),FLinearColor(.008f,.035f,.08f),TEXT("Sphere"),false,L,TEXT("M_CU_Nebula"));
    Shape(TEXT("Moon"),FVector(-12000,-57000,29000),FVector(7300),FLinearColor(.33f,.38f,.42f),TEXT("Sphere"),false,L);
    for(int32 I=0;I<70;++I)
    {
        const float A=I*2*PI/70;
        Shape(TEXT("OrbitMarker"),FVector(FMath::Cos(A)*52000,FMath::Sin(A)*52000,1000),FVector(85),CellUniverse::Cyan*.5f,TEXT("Sphere"),false,L,TEXT("M_CU_Glow"));
    }
}

void AMiningCellUniverse::Galaxy(USceneComponent* Parent,float Radius,int32 RandomSeed,int32 Count)
{
    Shape(TEXT("GalaxyCore"),FVector::ZeroVector,FVector(Radius*.13f,Radius*.075f,Radius*.075f),CellUniverse::Gold*.55f,TEXT("Sphere"),false,Parent,TEXT("M_CU_Glow"));
    Shape(TEXT("GalaxyHaze"),FVector::ZeroVector,FVector(Radius*.22f,Radius*2,Radius*1.25f),FLinearColor(.027f,.013f,.062f),TEXT("Sphere"),false,Parent,TEXT("M_CU_Nebula"));
    auto* Stars=NewObject<UInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UInstancedStaticMeshComponent::StaticClass(),TEXT("SpiralGalaxy")),RF_Transactional);
    AddInstanceComponent(Stars); Stars->ComponentTags.Add(CellUniverse::GeneratedTag); Stars->SetupAttachment(Parent);
    Stars->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")));
    Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision); Stars->SetCastShadow(false);
    Stars->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CellUniverse/Materials/M_CU_Stars"))); Stars->RegisterComponent();
    FRandomStream R(RandomSeed);
    for(int32 I=0;I<Count;++I)
    {
        const float D=R.FRandRange(.08f,1.f)*Radius,A=D/Radius*6.8f+(I%3)*2*PI/3+R.FRandRange(-.2f,.2f);
        const FVector P(R.FRandRange(-.035f,.035f)*Radius,FMath::Cos(A)*D,FMath::Sin(A)*D*.62f);
        Stars->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(.003f,.009f)*Radius/100)));
    }
}

void AMiningCellUniverse::BuildCosmos()
{
    auto* Main=Group(TEXT("GalaxyLandmark"),FVector(GetStageStart(6)+8500,85000,39000)); Landmarks[6]=Main;
    Main->SetRelativeRotation(FRotator(0,-65,12)); Galaxy(Main,71000,Seed,1700);
    // The sun and distant system are retained as intermediate scale references.
    Shape(TEXT("DistantSun"),FVector(GetStageStart(6)+2400,-10000,8500),FVector(3000),CellUniverse::Gold*.7f,TEXT("Sphere"),false,nullptr,TEXT("M_CU_Glow"));
    Shape(TEXT("FormerPlanet"),FVector(GetStageStart(6)+900,-1900,700),FVector(95),FLinearColor(.025f,.12f,.38f),TEXT("Sphere"),false,nullptr,TEXT("M_CU_Planet"));
    FRandomStream R(Seed+70);
    auto* Web=Group(TEXT("UniverseLandmark"),FVector(GetStageStart(7)+10000,0,0)); Landmarks[7]=Web;
    TArray<FVector> Centers;
    for(int32 I=0;I<9;++I)
    {
        const float Side=I%2?1.f:-1.f;
        const FVector P(R.FRandRange(-5000,110000),Side*R.FRandRange(35000,110000),(I%3==0?-1:1)*R.FRandRange(12000,80000));
        auto* G=Group(TEXT("IslandUniverse"),P);
        G->AttachToComponent(Web,FAttachmentTransformRules::KeepRelativeTransform);
        G->SetRelativeRotation(FRotator(R.FRandRange(-15,15),Side*-65,R.FRandRange(-45,45)));
        Galaxy(G,R.FRandRange(9000,28000),Seed+I*37,400); Centers.Add(P);
        if(I>1) Rod(TEXT("CosmicFilament"),Centers[I-2],P,180,FLinearColor(.025f,.016f,.065f),Web);
    }
    for(int32 Stage=5;Stage<=MembraneStage;++Stage)
    {
        auto* Dust=NewObject<UInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UInstancedStaticMeshComponent::StaticClass(),TEXT("StarField")),RF_Transactional);
        AddInstanceComponent(Dust); Dust->ComponentTags.Add(CellUniverse::GeneratedTag); Dust->SetupAttachment(RootComponent);
        Dust->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")));
        Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision); Dust->SetCastShadow(false);
        Dust->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CellUniverse/Materials/M_CU_Stars"))); Dust->RegisterComponent();
        const float X=GetStageStart(Stage),Length=BuiltStages[Stage].Length;
        for(int32 I=0;I<650;++I)
        {
            const float Distance=Stage==MembraneStage?2.f:22.f;
            FVector P(X+R.FRandRange(80,Length-200),R.FRandRange(-1250,1250)*Distance,R.FRandRange(-1450,1450)*Distance);
            if(FMath::Abs(P.Y)<1000 && FMath::Abs(P.Z)<1500) P.Z+=3000;
            Dust->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(.018f,.07f)*Distance)));
        }
        // Near rocks and thin route markers provide parallax against stationary immense bodies.
        if(Stage<MembraneStage) for(int32 I=0;I<32;++I)
        {
            const FVector P(X+R.FRandRange(150,Length-300),I%2?R.FRandRange(900,3500):R.FRandRange(-3500,-900),R.FRandRange(-4200,1300));
            auto* Rock=Shape(TEXT("NearAsteroid"),P,FVector(R.FRandRange(60,260)),FLinearColor(.095f,.11f,.16f),TEXT("Sphere"));
            Rock->SetRelativeScale3D(Rock->GetRelativeScale3D()*FVector(1,.7,1.6));
        }
    }
    // The universe's final boundary repeats the cell silhouette from stage 0.
    for(int32 I=0;I<4;++I)
        Ring(TEXT("UniverseMembraneContour"),FVector(GetStageStart(MembraneStage)+400+I*1500,0,1800),1800,CellUniverse::Violet*.5f);
    auto* Cell=Group(TEXT("HiddenCellNucleus"),FVector(GetStageStart(MembraneStage)+3400,1700,1300)); Landmarks[MembraneStage]=Cell;
    Shape(TEXT("RecognizableNucleus"),FVector::ZeroVector,FVector(1600),CellUniverse::Violet,TEXT("Sphere"),false,Cell,TEXT("M_CU_Organic"));
    Shape(TEXT("RecognizableNucleolus"),FVector(-620,-200,40),FVector(420),CellUniverse::Gold,TEXT("Sphere"),false,Cell);
}

void AMiningCellUniverse::BuildMouth()
{
    const float X=GetStageEnd(MouthStage)-2800;
    Shape(TEXT("Tongue"),FVector(X+1600,0,35),FVector(1850,900,120),FLinearColor(.5f,.09f,.12f),TEXT("Sphere"),false,nullptr,TEXT("M_CU_Organic"));
    for(int32 I=0;I<10;++I)
    {
        const float Y=(I-4.5f)*180;
        const float Arch=FMath::Square(Y/900)*280;
        Shape(TEXT("UpperTooth"),FVector(X+2200-Arch,Y,1000-Arch*.4f),FVector(190,155,300),FLinearColor(.82f,.72f,.51f),TEXT("Sphere"));
        // Lower teeth flank the passage; there is no undiggable obstruction at foot level.
        if(FMath::Abs(Y)>300) Shape(TEXT("LowerTooth"),FVector(X+2180-Arch,Y,145),FVector(175,150,240),FLinearColor(.8f,.7f,.49f),TEXT("Sphere"));
    }
    Ring(TEXT("MouthOutline"),FVector(X+2200,0,600),680,CellUniverse::Rose);
    // A small room beyond the mouth is the hand-off point for the later real footage.
    const float End=GetStageEnd(MouthStage);
    Shape(TEXT("RealityFloor"),FVector(End+600,0,-60),FVector(1700,2600,120),FLinearColor(.28f,.24f,.18f),TEXT("Cube"),true);
    Shape(TEXT("RealityBack"),FVector(End+1350,0,700),FVector(100,2600,1500),FLinearColor(.68f,.62f,.48f),TEXT("Cube"),true);
    Shape(TEXT("Desk"),FVector(End+1050,0,300),FVector(450,1400,80),FLinearColor(.3f,.16f,.075f),TEXT("Cube"),true);
    Shape(TEXT("Paper"),FVector(End+890,-120,344),FVector(200,280,4),FLinearColor(.92f,.88f,.72f));
    Shape(TEXT("Monitor"),FVector(End+1120,390,520),FVector(70,480,350),FLinearColor(.02f,.025f,.03f));
    Shape(TEXT("MonitorLight"),FVector(End+1082,390,530),FVector(4,425,290),CellUniverse::Cyan*.55f,TEXT("Cube"),false,nullptr,TEXT("M_CU_Glow"));
}

void AMiningCellUniverse::BuildBelowPath()
{
    FRandomStream R(Seed+211);
    for(int32 Stage=0;Stage<StageCount;++Stage)
    {
        const auto& S=BuiltStages[Stage]; const float X=GetStageStart(Stage);
        if(Stage>=5 && Stage<=7) continue; // Planet, stars, galaxies and asteroids already surround the path in 3D.
        if(Stage==4)
        {
            Shape(TEXT("LandBelowBridge"),FVector(X+S.Length*.5f,-1700,-5500),FVector(15000,14000,4500),FLinearColor(.065f,.18f,.12f),TEXT("Sphere"));
            Shape(TEXT("OceanBelowBridge"),FVector(X+S.Length*.5f,3000,-6500),FVector(18000,17000,2600),FLinearColor(.015f,.06f,.18f),TEXT("Sphere"));
            for(int32 I=0;I<18;++I)
                Shape(TEXT("MountainBelowBridge"),FVector(X+R.FRandRange(600,S.Length-900),R.FRandRange(-4000,-600),-2900),FVector(1700,1900,R.FRandRange(1800,3500)),FLinearColor(.18f,.23f,.21f),TEXT("Cone"));
            continue;
        }
        for(int32 I=0;I<22;++I)
        {
            const FVector P(X+R.FRandRange(200,S.Length-650),R.FRandRange(-S.Width*.4f,S.Width*.4f),-R.FRandRange(350,S.BelowPathDepth*.8f));
            if(Stage==1||Stage==GiantBloodStage)
            {
                auto* C=Shape(TEXT("BloodBelowBridge"),P,FVector(310,310,100),CellUniverse::Rose,TEXT("Sphere"),false,nullptr,TEXT("M_CU_Blood"));
                C->SetRelativeRotation(FRotator(I*23,I*17,35)); BloodCells.Add(C); BloodOrigins.Add(P); BloodScales.Add(C->GetRelativeScale3D());
            }
            else if(Stage==0||Stage==2||Stage==MembraneStage)
            {
                Shape(TEXT("CellBelowBridge"),P,FVector(220,370,230),I%2?CellUniverse::Cyan*.6f:CellUniverse::Violet*.7f,TEXT("Sphere"),false,nullptr,TEXT("M_CU_Organic"));
                if(I%3==0) Rod(TEXT("FiberBelowBridge"),P,P+FVector(600,300,-280),24,CellUniverse::Cyan*.35f);
            }
            else Shape(TEXT("TerrainBelowBridge"),P,FVector(650,750,900),Stage==MouthStage?CellUniverse::Rose*.3f:FLinearColor(.18f,.14f,.08f),TEXT("Sphere"));
        }
    }
}

void AMiningCellUniverse::BuildDemo()
{
    if(GetWorld()->IsGameWorld()) return;
    for(ADiggableRock* Gate:Gates) if(IsValid(Gate)) Gate->Destroy();
    Gates.Reset(); Landmarks.Init(nullptr,StageCount); BloodCells.Reset(); BloodOrigins.Reset(); BloodScales.Reset();
    const TArray<UActorComponent*> Old=GetInstanceComponents();
    for(auto* C:Old) if(C && C->ComponentTags.Contains(CellUniverse::GeneratedTag)) C->DestroyComponent();
    if(Stages.Num()!=StageCount) ApplyExpandedPreset();
    BuiltStages=Stages; StageStarts.Reset(); float Start=0;
    for(auto& S:BuiltStages)
    {
        const auto Safe=[](float V,float Low,float High) {return FMath::IsFinite(V)?FMath::Clamp(V,Low,High):Low;};
        S.Length=Safe(S.Length,2600,100000); S.Width=Safe(S.Width,2600,100000);
        S.Height=Safe(S.Height,1500,60000); S.BelowPathDepth=Safe(S.BelowPathDepth,800,60000); S.Depth=Safe(S.Depth,180,650);
        S.OpeningWidth=Safe(S.OpeningWidth,300,S.Width-400);
        S.OpeningHeight=Safe(S.OpeningHeight,300,S.Height-200);
        S.EndScale=Safe(S.EndScale,.1f,1); S.MinimumScaleSeconds=Safe(S.MinimumScaleSeconds,5,180);
        StageStarts.Add(Start); Start+=S.Length;
    }
    Shape(TEXT("BackWall"),FVector(-80,0,(BuiltStages[0].Height-BuiltStages[0].BelowPathDepth)*.5f),FVector(100,BuiltStages[0].Width+200,BuiltStages[0].Height+BuiltStages[0].BelowPathDepth),FLinearColor(.016f,.03f,.04f),TEXT("Cube"),true);
    for(int32 I=0;I<StageCount;++I)
    {
        const auto& S=BuiltStages[I];
        const float X=GetStageStart(I),Length=S.Length,Width=S.Width,Height=S.Height,DepthBelow=S.BelowPathDepth;
        const float GateX=GetStageEnd(I)-350;
        const bool OpenVista=I>=4&&I<=7;
        const bool Blood=I==1||I==GiantBloodStage||I==MouthStage;
        const FLinearColor Color=Blood?CellUniverse::Rose:(I==3||I==4?CellUniverse::Gold:(I>=5&&I<=8?CellUniverse::Violet:CellUniverse::Cyan));
        const FLinearColor Wall=Blood?FLinearColor(.095f,.011f,.028f):FLinearColor(.006f,.014f,.028f);
        // Cosmic chambers have invisible safety bounds, with distant art outside them.
        // Only the narrow rock path is drawn; room ceilings cannot chop off the celestial vistas.
        Shape(TEXT("LowerSafetyFloor"),FVector(X+Length*.5f,0,-DepthBelow-55),FVector(Length+2,Width+200,110),Wall,TEXT("Cube"),true)->SetVisibility(!OpenVista);
        // This is the actual walkable surface: there is no full-width invisible floor at path height.
        Shape(TEXT("SuspendedWalkway"),FVector(X+Length*.5f,0,-40),FVector(Length+2,650,80),OpenVista?FLinearColor(.04f,.047f,.065f):Wall*2.f,TEXT("Cube"),true,nullptr,TEXT("M_CU_Rock"));
        Shape(TEXT("Ceiling"),FVector(X+Length*.5f,0,Height+60),FVector(Length+2,Width+200,120),Wall,TEXT("Cube"),true)->SetVisibility(!OpenVista);
        if(OpenVista)
        {
            for(int32 B=0;B<FMath::CeilToInt(Length/650);++B)
                Shape(TEXT("RidgeEdge"),FVector(X+200+B*650,B%2?380:-380,-120),FVector(640,270,310),FLinearColor(.07f,.08f,.11f),TEXT("Sphere"));
        }
        for(int32 Side:{-1,1})
        {
            Shape(TEXT("SideWall"),FVector(X+Length*.5f,Side*(Width*.5f+60),(Height-DepthBelow)*.5f),FVector(Length+2,120,Height+DepthBelow),Wall,TEXT("Cube"),true)->SetVisibility(!OpenVista);
            if(!OpenVista) Shape(TEXT("RouteThread"),FVector(X+Length*.5f,Side*260,3),FVector(Length,3,4),CellUniverse::Gold*.4f,TEXT("Cube"),false,nullptr,TEXT("M_CU_Glow"));
            const float FrameWidth=(Width-S.OpeningWidth)*.5f;
            Shape(TEXT("GateFrame"),FVector(GateX,Side*(S.OpeningWidth+FrameWidth)*.5f,(Height-DepthBelow)*.5f),FVector(S.Depth+20,FrameWidth,Height+DepthBelow),Color*.10f,TEXT("Cube"),true)->SetVisibility(!OpenVista);
            Shape(TEXT("GateSeam"),FVector(GateX-S.Depth*.5f-12,Side*(S.OpeningWidth*.5f+8),S.OpeningHeight*.5f),FVector(7,9,S.OpeningHeight),Color*.7f,TEXT("Cube"),false,nullptr,TEXT("M_CU_Glow"));
        }
        // A solid lintel limits the patch vertically. Small-scale exits are a crawl-sized seam, not a whole wall.
        Shape(TEXT("GateLintel"),FVector(GateX,0,(Height+S.OpeningHeight)*.5f),FVector(S.Depth+20,S.OpeningWidth,Height-S.OpeningHeight),Color*.10f,TEXT("Cube"),true)->SetVisibility(!OpenVista);
        Shape(TEXT("GateSill"),FVector(GateX,0,-DepthBelow*.5f),FVector(S.Depth+20,S.OpeningWidth,DepthBelow),Color*.10f,TEXT("Cube"),true)->SetVisibility(!OpenVista);
        Shape(TEXT("GateTopSeam"),FVector(GateX-S.Depth*.5f-12,0,S.OpeningHeight+6),FVector(7,S.OpeningWidth,9),Color*.7f,TEXT("Cube"),false,nullptr,TEXT("M_CU_Glow"));
        const int32 Marks=FMath::CeilToInt(Length/500);
        for(int32 Mark=0;Mark<Marks;++Mark)
            Shape(TEXT("ForwardTrail"),FVector(X+200+Mark*(Length-450)/Marks,0,3),FVector(65,6,5),CellUniverse::Gold*.5f,TEXT("Cube"),false,nullptr,TEXT("M_CU_Glow"));
        const FTransform T(GetActorRotation(),GetActorTransform().TransformPosition(FVector(GateX,0,S.OpeningHeight*.5f)));
        auto* Gate=GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(),T,this);
        Gate->RockSize=FVector(S.Depth,S.OpeningWidth,S.OpeningHeight);
        const TCHAR* Surface=(I==0||I==MembraneStage)?TEXT("M_CU_Membrane"):Blood||I==2?TEXT("M_CU_Organic"):TEXT("M_CU_Rock");
        Gate->RockMaterial=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/CellUniverse/Materials/"))+Surface));
        Gate->FinishSpawning(T); Gate->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
        Gate->Tags.Add(CellUniverse::GeneratedTag); Gates.Add(Gate);
#if WITH_EDITOR
        Gate->SetActorLabel(FString::Printf(TEXT("CU_%02d_DigThrough_%s"),I+1,CellUniverse::Names[I]));
        Gate->SetFolderPath(TEXT("Cell Universe/Diggable boundaries"));
#endif
        auto* Light=NewObject<UPointLightComponent>(this,MakeUniqueObjectName(this,UPointLightComponent::StaticClass(),TEXT("ChamberLight")),RF_Transactional);
        AddInstanceComponent(Light); Light->ComponentTags.Add(CellUniverse::GeneratedTag); Light->SetupAttachment(RootComponent);
        Light->SetRelativeLocation(FVector(X+Length*.55f,0,900)); Light->SetIntensityUnits(ELightUnits::Unitless);
        Light->SetUseInverseSquaredFalloff(false); Light->SetIntensity(3); Light->SetAttenuationRadius(FMath::Min(Length,6000.f));
        Light->SetLightColor(Color); Light->SetCastShadows(false); Light->RegisterComponent();
    }
    BuildCell(0); BuildBlood(1); BuildTissue(); BuildBug(); BuildContinent(); BuildPlanet(); BuildCosmos(); BuildBlood(GiantBloodStage); BuildMouth(); BuildBelowPath();
    MarkPackageDirty();
}

void AMiningCellUniverse::BeginPlay()
{
    Super::BeginPlay();
    if (bAreaManaged) bAreaActive = false;
    ObservedCuts.Init(0,Gates.Num());
    StageElapsed.Init(0,StageCount);
    for(int32 I=0;I<Landmarks.Num();++I) if(Landmarks[I]) Landmarks[I]->SetVisibility(I==0,true);
    for(ADiggableRock* Gate:Gates)
    {
        if(!IsValid(Gate)) { GateMaterials.Add(nullptr); continue; }
        auto* M=UMaterialInstanceDynamic::Create(Gate->RockMaterial,this); GateMaterials.Add(M);
        Gate->RockMaterial=M; Gate->RockMesh->SetMaterial(0,M);
    }
}

bool AMiningCellUniverse::InitializePlayer()
{
    Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->GetPawn()) return false;
    Mining=Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    auto* Movement=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
    if(!Mining||!Movement) return false;
    // Retain the original pawn and every original input binding. Only this demo opts into flight.
    if (!bAreaManaged) { Movement->FlightSettings.bEnabled=true; Movement->FlightSettings.bEnableHeat=false; }
    float Deepest=2000;
    for(const auto& S:BuiltStages) Deepest=FMath::Max(Deepest,S.BelowPathDepth+2000);
    if (!bAreaManaged) Mining->FallResetDistance=Deepest;
    Mining->OnFieldReset.AddDynamic(this,&AMiningCellUniverse::ResetPresentation);
    if(!bAreaManaged) if(auto* Character=Cast<ABrainMiningCharacter>(Player->GetPawn())) Character->ConfigureMiningLighting(3.f,.3f);
    if(GEngine&&GEngine->GameViewport)
    {
        SAssignNew(View,SCellUniverseView).Owner(this); View->SetVisibility(EVisibility::HitTestInvisible);
        GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),20);
    }
    return true;
}

void AMiningCellUniverse::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bAreaManaged && !bAreaActive) return;
    if(!bInitialized) { bInitialized=InitializePlayer(); if(!bInitialized) return; }
    Elapsed+=DeltaSeconds; StageAge+=DeltaSeconds;
    if(StageElapsed.IsValidIndex(CurrentStage)) StageElapsed[CurrentStage]+=DeltaSeconds;
    const FVector P=GetActorTransform().InverseTransformPosition(Player->GetPawn()->GetActorLocation());
    if(Gates.Num()!=StageCount || BuiltStages.Num()!=StageCount || StageStarts.Num()!=StageCount) return;
    // Celestial art extends far beyond collision bounds. Reveal it through each mined boundary,
    // otherwise the 840 m planet would physically occlude the galaxy in the next section.
    // Use spatial location, not progression, so revisiting an earlier section still works.
    int32 SpatialStage=0;
    for(int32 I=1;I<StageCount;++I) if(P.X>=GetStageStart(I)) SpatialStage=I;
    for(int32 I=0;I<Landmarks.Num();++I)
        if(Landmarks[I] && Landmarks[I]->IsVisible()!=(I==SpatialStage)) Landmarks[I]->SetVisibility(I==SpatialStage,true);
    // Advance only after actual excavation AND physical passage. Flying over/around cannot skip a beat.
    if(!bComplete && IsValid(Gates[CurrentStage]) && Gates[CurrentStage]->SuccessfulCuts>0 && P.X>GetStageEnd(CurrentStage)+105)
    {
        if(CurrentStage<MouthStage) { ++CurrentStage; StageAge=0; }
        else bComplete=true;
    }
    for(int32 I=0;I<Gates.Num();++I)
    {
        if(!IsValid(Gates[I])) continue;
        if(ObservedCuts[I]!=Gates[I]->SuccessfulCuts)
        {
            ObservedCuts[I]=Gates[I]->SuccessfulCuts;
            if(GateMaterials.IsValidIndex(I)&&GateMaterials[I]) GateMaterials[I]->SetScalarParameterValue(TEXT("HitTime"),GetWorld()->GetTimeSeconds());
        }
    }
    MembraneCuts=IsValid(Gates[MembraneStage])?Gates[MembraneStage]->SuccessfulCuts:0;
    // Hold each landmark unchanged for the first 35% of its section. Even sprinting cannot
    // collapse it faster than the authored minimum time. Celestial heroes remain full size.
    auto ProgressFor=[&](int32 I)
    {
        const float Distance=FMath::Clamp((P.X-GetStageStart(I)-BuiltStages[I].Length*.35f)/(BuiltStages[I].Length*.6f),0.f,1.f);
        const float Time=FMath::Clamp((StageElapsed[I]-5.f)/BuiltStages[I].MinimumScaleSeconds,0.f,1.f);
        const float A=FMath::Min(Distance,Time);
        return A*A*(3-2*A);
    };
    ScaleProgress=ProgressFor(CurrentStage);
    for(int32 I=0;I<Landmarks.Num();++I)
    {
        if(!Landmarks[I]) continue;
        const float Target=FMath::Lerp(1.f,BuiltStages[I].EndScale,ProgressFor(I));
        const float Scale=FMath::FInterpTo(Landmarks[I]->GetRelativeScale3D().X,Target,DeltaSeconds,.65f);
        Landmarks[I]->SetRelativeScale3D(FVector(Scale));
    }
    for(int32 I=0;I<BloodCells.Num();++I)
    {
        UStaticMeshComponent* C=BloodCells[I]; if(!C) continue;
        const FVector Base=BloodOrigins[I];
        C->SetRelativeLocation(Base+FVector(FMath::Sin(Elapsed*.6f+I)*85,FMath::Sin(Elapsed*.8f+I)*45,FMath::Cos(Elapsed*.7f+I)*35));
        const bool SmallBlood=Base.X<GetStageEnd(1);
        const float Progress=SmallBlood?ProgressFor(1):0.f;
        const FVector Original=BloodScales[I];
        C->SetRelativeScale3D(Original*FMath::Lerp(1.f,BuiltStages[1].EndScale,Progress));
    }
}

void AMiningCellUniverse::ResetPresentation()
{
    if (bAreaManaged && !bAreaActive) return;
    CurrentStage=0; bComplete=false; MembraneCuts=0; ScaleProgress=0;
    Elapsed=0; StageAge=0; StageElapsed.Init(0,StageCount); ObservedCuts.Init(0,Gates.Num());
    for(int32 I=0;I<Landmarks.Num();++I) if(Landmarks[I])
    { Landmarks[I]->SetRelativeScale3D(FVector(1)); Landmarks[I]->SetVisibility(I==0,true); }
    for(int32 I=0;I<BloodCells.Num();++I) if(BloodCells[I])
    {
        BloodCells[I]->SetRelativeLocation(BloodOrigins[I]);
        BloodCells[I]->SetRelativeScale3D(BloodScales[I]);
    }
    for(UMaterialInstanceDynamic* M:GateMaterials) if(M) M->SetScalarParameterValue(TEXT("HitTime"),-1000);
}

void AMiningCellUniverse::SetAreaActive(bool bActive)
{
    bAreaActive = bActive;
    if (View) View->SetVisibility(bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

FVector AMiningCellUniverse::GetGateApproach(int32 Index) const
{
    Index=FMath::Clamp(Index,0,StageCount-1);
    const float Depth=BuiltStages.IsValidIndex(Index)?BuiltStages[Index].Depth:260;
    return GetActorTransform().TransformPosition(FVector(GetStageEnd(Index)-350-Depth*.5f-240,0,100));
}
FString AMiningCellUniverse::StageName() const { return BuiltStages.IsValidIndex(CurrentStage)?BuiltStages[CurrentStage].Name:FString(CellUniverse::Names[FMath::Clamp(CurrentStage,0,StageCount-1)]); }
FString AMiningCellUniverse::Hint() const
{
    if(bComplete) return TEXT("演示完成 · R 可重玩，最后的出口留给实拍转场。");
    static const TCHAR* Hints[]={
        TEXT("在细胞里停留一会儿。出口是前方那一小块发亮的薄膜。"),
        TEXT("血液流经身体。沿金色痕迹，寻找血管壁上的小块堵塞。"),
        TEXT("细胞聚成组织。右侧是细胞群，左侧是纤维和毛细血管。"),
        TEXT("整片组织，属于这一只小虫。继续走向外面的土地。"),
        TEXT("山脉、河流、海岸……留一点时间看看这片大陆。"),
        TEXT("抬头看看。脚下的小岩脊，正经过一整颗星球。空格可以升高视角。"),
        TEXT("那颗星球已经是一点微光。前方的星系仍远得几乎不动。"),
        TEXT("无数星系之间，只有脚下这一条细细的路。沿金色痕迹继续。"),
        TEXT("宇宙的尽头，为什么像一层细胞膜？挖开它。"),
        TEXT("红细胞又变得巨大了。左侧那颗细胞，装着刚才的星空。"),
        TEXT("声音越来越近。挖通最后的堵塞，让这个念头出去。")};
    return Hints[FMath::Clamp(CurrentStage,0,StageCount-1)];
}
FString AMiningCellUniverse::Subtitle() const
{
    if(bComplete) return TEXT("“等等……我们就做一个挖矿游戏！”");
    if(CurrentStage==MembraneStage && MembraneCuts>0) return TEXT("宇宙外面，怎么会有血液？");
    if(CurrentStage==GiantBloodStage)
    {
        if(StageAge<5) return TEXT("“看完这视频没感觉，我们就去吃饭。”");
        if(StageAge<8) return TEXT("“好。”");
        if(StageAge<13) return TEXT("“看完了，有感觉吗？”");
        if(StageAge<17) return TEXT("“……”");
        return TEXT("“呃，走吧，吃饭。”");
    }
    if(CurrentStage==MouthStage)
    {
        const int32 Cuts=Gates.IsValidIndex(MouthStage)&&Gates[MouthStage]?Gates[MouthStage]->SuccessfulCuts:0;
        if(Cuts>0) return Cuts==1?TEXT("“……等等，挖。”"):Cuts==2?TEXT("“挖！”"):TEXT("“挖！！”");
        return TEXT("“脑子干了，堵死了……好想把脑子挖通畅啊……”");
    }
    return FString();
}

void AMiningCellUniverse::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Mining) Mining->OnFieldReset.RemoveDynamic(this,&AMiningCellUniverse::ResetPresentation);
    if(View.IsValid()&&GEngine&&GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    View.Reset(); Super::EndPlay(Reason);
}
