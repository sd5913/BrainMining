#include "MiningFootprintComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"
#include "Engine/World.h"

UMiningFootprintComponent::UMiningFootprintComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=.04f;
}

void UMiningFootprintComponent::BeginPlay()
{
    Super::BeginPlay();
    FootprintMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/M_PlayerFootprint"));
    if(FootprintMaterial)
    {
        FootprintMaterial->EnsureIsComplete();
        LeftFootMaterial=UMaterialInstanceDynamic::Create(FootprintMaterial,this);
        RightFootMaterial=UMaterialInstanceDynamic::Create(FootprintMaterial,this);
        LeftFootMaterial->SetScalarParameterValue(TEXT("FootSide"),-1.f);
        RightFootMaterial->SetScalarParameterValue(TEXT("FootSide"),1.f);
    }
}

void UMiningFootprintComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt,Type,Fn);
    auto* Pawn=Cast<ACharacter>(GetOwner());
    if(!Pawn || !Pawn->IsLocallyControlled())return;
    const FVector P=Pawn->GetActorLocation();
    const float Scale=Pawn->GetActorScale3D().GetAbsMin();
    const bool Grounded=bEnabled && Pawn->GetCharacterMovement()->IsMovingOnGround() && !Pawn->IsMoveInputIgnored();
    const float Travel=bHavePrevious?FVector::Dist2D(P,Previous):0.f;
    Previous=P;bHavePrevious=true;
    // Scripted travel and teleports never paint a line across the destination.
    if(!Grounded || Travel>300.f*Scale){Distance=0;return;}
    Distance+=Travel;
    if(Travel<.1f || Distance<FMath::Max(10.f,StepDistance)*Scale)return;
    Distance=0;
    FVector Forward=Pawn->GetVelocity().GetSafeNormal2D();
    if(Forward.IsNearlyZero())return;
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
    const FVector Foot=P+Right*(bLeft?-10.f:10.f)*Scale;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FootprintGround),true,Pawn);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Foot,Foot-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+35.f*Scale),ECC_Visibility,Q))return;
    if(Hit.ImpactNormal.Z<.45f)return;
    if(!FootprintMaterial)FootprintMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/M_PlayerFootprint"));
    if(!FootprintMaterial)return;
    Forward=FVector::VectorPlaneProject(Forward,Hit.ImpactNormal).GetSafeNormal();
    const FRotator Rotation=FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal,Forward).Rotator();
    const float Seconds=FMath::Max(4.f,Lifetime);
    UMaterialInterface* Sole=bLeft?LeftFootMaterial.Get():RightFootMaterial.Get();
    if(auto* Mark=UGameplayStatics::SpawnDecalAttached(Sole?Sole:FootprintMaterial.Get(),FVector(9,8,17)*Scale,Hit.GetComponent(),NAME_None,Hit.ImpactPoint+Hit.ImpactNormal*.8f,Rotation,EAttachLocation::KeepWorldPosition,Seconds))
    {
        Mark->ComponentTags.Add(TEXT("MiningFootprint"));
        Mark->SetFadeOut(3.f,Seconds-3.f,false);
        Mark->FadeScreenSize=.0001f;
    }
    bLeft=!bLeft;
}
