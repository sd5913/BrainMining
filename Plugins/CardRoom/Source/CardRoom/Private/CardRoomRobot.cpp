#include "CardRoomRobot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ACardRoomRobot::ACardRoomRobot()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("RobotRoot"));
    Head=CreateDefaultSubobject<USceneComponent>(TEXT("HeadJoint"));Head->SetupAttachment(RootComponent);Head->SetRelativeLocation(FVector(0,0,90));
    const TCHAR* Names[]={TEXT("Body"),TEXT("Head"),TEXT("ArmL"),TEXT("ArmR"),TEXT("FootL"),TEXT("FootR"),TEXT("BrowL"),TEXT("BrowR"),TEXT("EyeL"),TEXT("EyeR")};
    const FVector Pivots[]={FVector(0,0,65),FVector(0,0,90),FVector(0,36.5,62),FVector(0,-36.5,62),FVector(0,12.5,30),FVector(0,-12.5,30),FVector(-13.8515,21.232,128.2902),FVector(-13.8515,-22.7574,129.4159),FVector(-9.6565,20.7581,118.7902),FVector(-9.6565,-21.078,118.7902)};
    for(int32 I=0;I<10;++I)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(Names[I]);
        const bool OnHead=I==1 || I>=6;Part->SetupAttachment(OnHead?Head.Get():RootComponent.Get());
        Part->SetRelativeLocation(Pivots[I]-(OnHead?FVector(0,0,90):FVector::ZeroVector));
        Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Rooms/CardDuel/Robot/SM_Robot_%s.SM_Robot_%s"),Names[I],Names[I])));
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);Parts.Add(Part);
        if(I<6 && Part->GetStaticMesh())
        {
            // Six simple shapes follow the body, head and limbs of the floor toy.
            // The stage disables actor collision for its animated opponent.
            auto* Hull=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("%sHull"),Names[I]));
            Hull->SetupAttachment(Part);
            const FBox Bounds=Part->GetStaticMesh()->GetBoundingBox();
            Hull->SetRelativeLocation(Bounds.GetCenter());Hull->SetBoxExtent(Bounds.GetExtent()*.94f);
            Hull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Hull->SetCollisionObjectType(ECC_WorldStatic);
            Hull->SetCollisionResponseToAllChannels(ECR_Ignore);Hull->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
            Hull->SetGenerateOverlapEvents(false);Hull->SetCanEverAffectNavigation(false);
        }
    }
    Mouth=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExpressionMouth"));Mouth->SetupAttachment(Head);
    Mouth->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Mouth->SetRelativeLocation(FVector(-18.6,0,15));Mouth->SetRelativeScale3D(FVector(.009,.16,.018));
    Mouth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Singed=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SingedBrowInner"));
    Singed->SetupAttachment(Parts[6]);
    // The derived mesh only removes a short central section. Its remaining
    // vertices and UVs are unchanged, so the brow keeps its original pose.
    Singed->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Rooms/CardDuel/Robot/SM_Robot_BrowL_Singed.SM_Robot_BrowL_Singed")));
    Singed->SetCollisionEnabled(ECollisionEnabled::NoCollision);Singed->SetVisibility(false);BrokenBrow.Add(Singed);
}
void ACardRoomRobot::SetEyebrowSinged(bool bSinged)
{
    bEyebrowSinged=bSinged;
    // The replacement inherits the intact brow's exact transform and expression.
    Parts[6]->SetVisibility(!bSinged,false);
    for(const auto& Piece:BrokenBrow)Piece->SetVisibility(bSinged);
}
void ACardRoomRobot::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Mouth->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Robot/RobotGraphite")));
    Pose(TEXT("Idle"),0,1);
}
void ACardRoomRobot::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bEncounterDriven)Pose(TEXT("Idle"),GetWorld()->GetTimeSeconds(),Dt);
}
void ACardRoomRobot::Pose(FName State,float Time,float Dt,const FRotator& Look)
{
    SetEyebrowSinged(bEyebrowSinged);
    Expression=State;
    const bool Hurt=State==TEXT("Hurt"),Sick=State==TEXT("Sick"),Panic=State==TEXT("Panic"),Attack=State==TEXT("Attack"),Arrival=State==TEXT("Arrival"),Fly=State==TEXT("Fly");
    const float Wave=FMath::Sin(Time*1.8f),Flail=FMath::Sin(Time*7.f);
    auto Rotate=[&](USceneComponent* C,FRotator R){C->SetRelativeRotation(FMath::RInterpTo(C->GetRelativeRotation(),R,Dt,Hurt?18.f:8.f));};
    Rotate(Head,FRotator(Look.Pitch+(Hurt?-18:Sick?14:Attack?-8:Wave*2),Look.Yaw+FMath::Sin(Time*.8f)*(Sick?15:4),Hurt?Flail*7:Sick?Wave*12:Panic?Wave*7:Wave*2));
    const float Blink=FMath::Clamp(1-FMath::Abs(FMath::Fmod(Time+1.7f,4.1f)-.12f)/.10f,0.f,1.f);
    for(int32 Side=0;Side<2;++Side)
    {
        const float Sign=Side==0?1.f:-1.f;
        // Keep facial hardware just in front of the bevelled panel throughout articulation.
        FVector Brow=Parts[6+Side]->GetRelativeLocation();Brow.X=-20.5;Parts[6+Side]->SetRelativeLocation(Brow);
        FVector Eye=Parts[8+Side]->GetRelativeLocation();Eye.X=-17.65;Parts[8+Side]->SetRelativeLocation(Eye);
        const float ArmPitch=Fly?-100+Flail*35:Panic?-100+Flail*12:Hurt?-70:Attack?(Side==0?-115:-45):Arrival?-70+Wave*20:Sick?-20:Wave*1.5f;
        Rotate(Parts[2+Side],FRotator(ArmPitch,0,Sign*(Panic?22:Attack?16:5)));
        // The supplied feet point forwards in bind pose. Rest them on the floor
        // for the seated toy; hang them down over the chair during the duel.
        Parts[4+Side]->SetRelativeLocation(FVector(0,Sign*12.5f,bEncounterDriven?30.f:19.78f));
        Rotate(Parts[4+Side],FRotator(Fly?Sign*Flail*35:bEncounterDriven?65.f:0.f,0,0));
        // Neutralise the source's permanently angry brows; rotate them back for attacks.
        Rotate(Parts[6+Side],FRotator(0,0,Sign*(Attack?0:Panic?38:Hurt?32:Sick?34:20)));
        const float EyeZ=Hurt?.22f:Sick?.5f:(1-Blink*.92f)*(Panic?1.3f:1.f);
        Parts[8+Side]->SetRelativeScale3D(FVector(1,1,EyeZ));
    }
    Mouth->SetRelativeLocation(FVector(-21.5,0,15));
    Mouth->SetRelativeScale3D(FMath::VInterpTo(Mouth->GetRelativeScale3D(),FVector(.009,Panic?.09:Attack?.20:.16,Panic?.12:Hurt?.07:Attack?.045:Sick?.025:.018),Dt,12));
}
