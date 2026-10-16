#include "EchoPassageStage.h"
#include "EchoPassageLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AEchoPassageStage::AEchoPassageStage()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("PassageOrigin"));
    auto Make=[&](const TCHAR* Name,bool Collision)
    {
        auto* M=CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        M->SetupAttachment(RootComponent); M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        M->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        M->SetCollisionResponseToAllChannels(ECR_Block); M->SetCastShadow(false);
        return M;
    };
    Rock=Make(TEXT("CollisionRock"),true); Outlines=Make(TEXT("EchoContours"),false);
    WarmWood=Make(TEXT("WarmWood"),true); WarmGlow=Make(TEXT("WarmLight"),false);
    TrapFloor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollapsingFloor")); TrapFloor->SetupAttachment(RootComponent);
    TrapFloor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    TrapFloor->SetRelativeLocation(FVector(10,0,888)); TrapFloor->SetRelativeScale3D(FVector(4.2,4,0.24));
    TrapFloor->SetCollisionResponseToAllChannels(ECR_Block); TrapFloor->SetCastShadow(false);
}
void AEchoPassageStage::Box(UInstancedStaticMeshComponent* Mesh,FVector P,FVector S)
{
    Mesh->AddInstance(FTransform(FQuat::Identity,P,S/100));
}
void AEchoPassageStage::Line(FVector A,FVector B,float Width)
{
    const FVector D=B-A;
    Outlines->AddInstance(FTransform(FRotationMatrix::MakeFromX(D).ToQuat(),(A+B)*.5,FVector(D.Size(),Width,Width)/100));
}
void AEchoPassageStage::OutlineBox(FVector P,FVector S)
{
    for(int Axis=0;Axis<3;++Axis) for(int A:{-1,1}) for(int B:{-1,1})
    {
        FVector L=P,R=P; L[Axis]-=S[Axis]*.5; R[Axis]+=S[Axis]*.5;
        L[(Axis+1)%3]+=A*S[(Axis+1)%3]*.5; R[(Axis+1)%3]=L[(Axis+1)%3];
        L[(Axis+2)%3]+=B*S[(Axis+2)%3]*.5; R[(Axis+2)%3]=L[(Axis+2)%3]; Line(L,R);
    }
}
void AEchoPassageStage::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    for(auto* M:{Rock.Get(),Outlines.Get(),WarmWood.Get(),WarmGlow.Get()}) M->ClearInstances();
    auto* Echo=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Passages/Echo/Materials/M_EchoContour.M_EchoContour"));
    auto* Wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Passages/Echo/Materials/M_WarmWood.M_WarmWood"));
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Passages/Echo/Materials/M_WarmGlow.M_WarmGlow"));
    if(Echo)
    {
        EchoMaterial=UMaterialInstanceDynamic::Create(Echo,this); RockMaterial=UMaterialInstanceDynamic::Create(Echo,this);
        EchoMaterial->SetScalarParameterValue(TEXT("Preview"),1); RockMaterial->SetScalarParameterValue(TEXT("Preview"),1);
        // Solid surfaces stay black; only the moving white ripple is drawn on them.
        RockMaterial->SetScalarParameterValue(TEXT("SurfaceMode"),1.f);
        Outlines->SetMaterial(0,EchoMaterial); Rock->SetMaterial(0,RockMaterial);
    }
    if(Wood) {WarmWood->SetMaterial(0,Wood);TrapFloor->SetMaterial(0,Wood);}
    if(Glow) WarmGlow->SetMaterial(0,Glow);
    const auto Cells=EchoPassageLayout::Cells(bUseMaze);
    for(int32 i=0;i<Cells.Num();++i)
    {
        const FIntPoint C=Cells[i]; FVector P(C.X*400,C.Y*400,0);
        Box(Rock,P+FVector(0,0,-12),FVector(400,400,24));
        if(i>0) Box(Rock,P+FVector(0,0,377),FVector(400,400,24));
        // Broken stone seams, with no filled illuminated grid or omniscient route overlay.
        Line(P+FVector(-200,-200,1.5),P+FVector(200,-200,1.5));
        Line(P+FVector(-200,200,1.5),P+FVector(200,200,1.5));
        Line(P+FVector(-200,-200,1.5),P+FVector(-200,200,1.5));
        const float J=(i*37%80)-40;
        Line(P+FVector(-80,-200,1.5),P+FVector(J,-40,1.5),.7f);
        Line(P+FVector(J,-40,1.5),P+FVector(-50,200,1.5),.7f);
        Line(P+FVector(J,-40,1.5),P+FVector(200,80+J,1.5),.7f);
        for(const FIntPoint D:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
        {
            if(Cells.Contains(C+D) || (C==EchoPassageLayout::ExitCell(bUseMaze) && D==FIntPoint(1,0))) continue;
            const FVector N(D.X,D.Y,0),T(-D.Y,D.X,0);
            Box(Rock,P+N*212+FVector(0,0,180),D.X?FVector(24,400,360):FVector(400,24,360));
            for(float H:{4.f,112.f,234.f,358.f})
            {
                const FVector A=P+N*199-T*200+FVector(0,0,H);
                const FVector B=P+N*198+T*J+FVector(0,0,H+(H==4||H==358?0:12));
                const FVector E=P+N*199+T*200+FVector(0,0,H);
                Line(A,B);Line(B,E);
            }
            for(int Band=0;Band<3;++Band)
            {
                float Along=(Band%2?-74.f:67.f)+J;
                Line(P+N*198+T*Along+FVector(0,0,Band*118+5),P+N*198+T*(Along+18)+FVector(0,0,(Band+1)*118+4));
            }
            Line(P+N*198-T*199+FVector(0,0,2),P+N*198-T*199+FVector(0,0,358));
        }
    }
    if(bUseMaze)
    {
        // Recognizable echo-only landmarks at bends; never permanent lights in the dark maze.
        for(const FVector Base:{FVector(1200,1200,0),FVector(2800,2400,0),FVector(4400,3600,0)})
        {
            for(int i=0;i<3;++i)
            {
                const FVector Size(28,28,36+i*18);
                const FVector P=Base+FVector(-145+i*38,-145,Size.Z*.5);
                Box(Rock,P,Size);OutlineBox(P,Size+FVector(1.5));
            }
        }
        // Shallow dead ends can be recognized before walking all the way in.
        for(const FVector Base:{FVector(400,-800,0),FVector(3600,1600,0)})
        {
            const FVector Size(110,50,65),P=Base+FVector(0,-145,32.5);
            Box(Rock,P,Size);OutlineBox(P,Size+FVector(1.5));
        }
    }
    // Shaft walls keep the opening readable during the drop, with a clear landing corridor to +X.
    for(int Side:{-1,1})
    {
        Box(Rock,FVector(0,Side*212,610),FVector(400,24,500));
        Line(FVector(-199,Side*199,360),FVector(-199,Side*199,860));
        Line(FVector(199,Side*199,360),FVector(199,Side*199,860));
    }
    Box(Rock,FVector(-212,0,450),FVector(24,400,900));
    // Small lit departure vestibule; the center floor collapses as the player steps onto it.
    Box(WarmWood,FVector(-480,0,888),FVector(560,400,24));
    for(int Side:{-1,1}) Box(WarmWood,FVector(-310,Side*222,1050),FVector(920,40,300));
    if(!bWalkThroughConnections)
    {
        Box(WarmWood,FVector(-760,0,1050),FVector(30,440,300));
        Box(WarmGlow,FVector(-747,0,1040),FVector(3,130,85));
    }
    for(int Side:{-1,1}) Box(WarmGlow,FVector(-500,Side*200,927),FVector(470,3,3));
    if(bRoomInSameLevel)
    {
        const FVector ExitShift=EchoPassageLayout::ExitOffset(bUseMaze);
        Box(Rock,ExitShift+FVector(3850,1600,-12),FVector(500,400,24));
        Box(Rock,ExitShift+FVector(3850,1600,377),FVector(500,400,24));
        for(int Side:{-1,1})Box(Rock,ExitShift+FVector(3850,1600+Side*212,180),FVector(500,24,360));
        return;
    }
    // The warm threshold moves with the exit of either layout.
    const FVector ExitShift=EchoPassageLayout::ExitOffset(bUseMaze);
    Box(WarmWood,ExitShift+FVector(4200,1600,-12),FVector(800,680,24));
    if(!bWalkThroughConnections) Box(WarmWood,ExitShift+FVector(4610,1600,175),FVector(22,680,350));
    for(int Side:{-1,1}) Box(WarmWood,ExitShift+FVector(4200,1600+Side*340,175),FVector(800,24,350));
    for(int Side:{-1,1})
    {
        Box(WarmWood,ExitShift+FVector(3820,1600+Side*250,175),FVector(40,180,350));
        Box(WarmGlow,ExitShift+FVector(3797,1600+Side*153,157),FVector(5,5,314));
    }
    Box(WarmWood,ExitShift+FVector(3820,1600,335),FVector(40,680,30));
    Box(WarmGlow,ExitShift+FVector(3797,1600,310),FVector(5,310,5));
    // Warm spill leads the eye to the doorway only after the final turn.
    for(int i=0;i<6;++i) Box(WarmGlow,ExitShift+FVector(3665+i*38,1600,1.8),FVector(18,170+i*10,2));
    Box(WarmWood,ExitShift+FVector(4320,1600,93),FVector(180,160,16));
    for(int X:{-1,1}) for(int Y:{-1,1}) Box(WarmWood,ExitShift+FVector(4320+X*65,1600+Y*55,45),FVector(14,14,90));
    Box(WarmGlow,ExitShift+FVector(4320,1600,120),FVector(30,30,40));
    Box(WarmWood,ExitShift+FVector(4470,1600,56),FVector(65,70,14));
    Box(WarmWood,ExitShift+FVector(4500,1600,104),FVector(12,70,95));
    for(int i=0;i<5;++i) Box(WarmGlow,ExitShift+FVector(4595,1450+i*65,210),FVector(3,25,90+(i%2)*35));
}
void AEchoPassageStage::BeginPlay()
{
    Super::BeginPlay();
    // Recreate per-session materials after loading a previously saved map.
    OnConstruction(GetActorTransform()); ResetPassage();
}
void AEchoPassageStage::ResetPassage()
{
    bCollapsed=bLanded=bCompleted=false; KnockCount=0; PulseIndex=0; LastEcho=LandingTime=-100;
    TrapFloor->SetVisibility(true);TrapFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    for(auto* M:{EchoMaterial.Get(),RockMaterial.Get()}) if(M)
    {
        M->SetScalarParameterValue(TEXT("Preview"),0);
        for(int i=0;i<4;++i) M->SetScalarParameterValue(*FString::Printf(TEXT("Start%d"),i),-1000);
    }
}
void AEchoPassageStage::Collapse()
{
    if(bCollapsed) return;
    bCollapsed=true; TrapFloor->SetVisibility(false);TrapFloor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if(AudioOverride.IsBound()) AudioOverride.Execute(TEXT("S_Crumble"),GetActorLocation(),.65f);
    else if(auto* S=LoadObject<USoundBase>(nullptr,TEXT("/Game/Passages/Echo/Audio/S_Crumble.S_Crumble"))) UGameplayStatics::PlaySound2D(this,S,.65f);
}
void AEchoPassageStage::EmitEcho(FVector Location,bool Landing)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if((bCompleted && !bRoomInSameLevel) || (!Landing && (Now-LastEcho<KnockCooldown || !bLanded))) return;
    LastEcho=Now;
    if(Landing) {bLanded=true;LandingTime=Now;} else ++KnockCount;
    for(auto* M:{EchoMaterial.Get(),RockMaterial.Get()}) if(M)
    {
        M->SetVectorParameterValue(*FString::Printf(TEXT("Origin%d"),PulseIndex),FLinearColor(Location.X,Location.Y,Location.Z,0));
        M->SetScalarParameterValue(*FString::Printf(TEXT("Start%d"),PulseIndex),Now);
        M->SetScalarParameterValue(*FString::Printf(TEXT("Radius%d"),PulseIndex),Landing?EchoRadius*1.4f:EchoRadius);
    }
    PulseIndex=(PulseIndex+1)%4;
    const TCHAR* Path=Landing?TEXT("/Game/Passages/Echo/Audio/S_Landing.S_Landing"):TEXT("/Game/Passages/Echo/Audio/S_Knock.S_Knock");
    if(AudioOverride.IsBound())AudioOverride.Execute(Landing?TEXT("S_Landing"):TEXT("S_Knock"),Location,Landing?.7f:.65f);
    else if(auto* S=LoadObject<USoundBase>(nullptr,Path)) UGameplayStatics::PlaySoundAtLocation(this,S,Location,Landing?.85f:.65f);
}
void AEchoPassageStage::Complete()
{
    if(bCompleted) return;
    bCompleted=true; OnPassageCompleted.Broadcast(PassageId);
    if(AudioOverride.IsBound())AudioOverride.Execute(TEXT("S_Exit"),GetActorLocation(),.55f);
    else if(auto* S=LoadObject<USoundBase>(nullptr,TEXT("/Game/Passages/Echo/Audio/S_Exit.S_Exit"))) UGameplayStatics::PlaySound2D(this,S,.55f);
}
void AEchoPassageStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    for(auto* M:{EchoMaterial.Get(),RockMaterial.Get()}) if(M)
    {
        M->SetScalarParameterValue(TEXT("Now"),GetWorld()->GetTimeSeconds());
        M->SetScalarParameterValue(TEXT("Speed"),FMath::Max(100.f,EchoSpeed));
        M->SetScalarParameterValue(TEXT("Fade"),FMath::Max(.3f,OutlineDuration));
    }
}

bool AEchoPassageStage::IsAtExit(const FVector& WorldPosition) const
{
    return EchoPassageLayout::IsAtExit(GetActorTransform().InverseTransformPosition(WorldPosition),bUseMaze);
}
