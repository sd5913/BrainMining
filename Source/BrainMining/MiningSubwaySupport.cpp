#include "MiningCollection.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void AMiningCollectible::UpdateSubwaySupport(float DeltaSeconds)
{
    if(ItemId!=21 || !bReleased || bCollected || !bSubwayPickupReady || ActorHasTag(TEXT("CollectionPreview"))
        || !GetWorld() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0.f)
    {
        SubwayFallSpeed=0.f;
        return;
    }

    // Use only the fixed carriage mesh and actor scale. Model's gentle visual
    // bob and the animated passengers must not change the support footprint.
    const auto Local=Model->GetDynamicMesh()->GetMeshRef().GetBounds();
    const FBox Bounds=FBox(FVector(Local.Min),FVector(Local.Max)).TransformBy(GetActorTransform());
    const FVector Extent=Bounds.GetExtent();
    if(Extent.IsNearlyZero())return;
    const FVector FootCenter(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z+1.f);
    const FVector FootExtent(FMath::Max(1.f,Extent.X*.8f),FMath::Max(1.f,Extent.Y*.8f),1.f);

    // A thin foot sweep allows the partially buried rear wall to remain in
    // place without treating side contact as permanent support underneath.
    // Never depenetrate sideways or impart an impulse when terrain is rebuilt.
    const float Dt=FMath::Min(DeltaSeconds,.1f);
    SubwayFallSpeed=FMath::Min(360.f,SubwayFallSpeed+420.f*Dt);
    const float Drop=FMath::Max(.5f,SubwayFallSpeed*Dt);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SubwaySupport),true,this);
    if(auto* PC=GetWorld()->GetFirstPlayerController())if(PC->GetPawn())Query.AddIgnoredActor(PC->GetPawn());
    FHitResult Hit;
    const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,FootCenter,FootCenter-FVector(0,0,Drop),
        FQuat::Identity,ECC_Visibility,FCollisionShape::MakeBox(FootExtent),Query);
    const float Travel=Blocked?(Hit.bStartPenetrating?0.f:FMath::Max(0.f,Drop*Hit.Time-.5f)):Drop;
    if(Travel>0.f)SetActorLocation(GetActorLocation()-FVector(0,0,Travel));
    if(Blocked)SubwayFallSpeed=0.f;
}
