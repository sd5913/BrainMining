#include "MiningComponent.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningBombToolComponent.h"
#include "Engine/World.h"

float UMiningComponent::GetPickaxeImpactDelay() const
{
	return FMath::Min(13.f / 30.f, GetMiningInterval()) * (.19f / (13.f / 30.f));
}

void UMiningComponent::ResolvePickaxeStrike()
{
	if (!GetWorld() || !GetOwner() || bDrillMode || bInputSuspended || IsEncounterBlocked()) return;
	if (const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>(); Bomb && Bomb->bEquipped) return;
	RefreshTarget();
	if (IsValid(TargetRock)) ApplyPickaxeHit(TargetHit);
	else Status = TEXT("Aim at rock within reach");
}

int32 UMiningComponent::FindPickaxePatch(const FHitResult& Hit) const
{
	int32 Closest = INDEX_NONE;
	double BestDistance = MAX_dbl;
	for (int32 Index = 0; Index < PickaxePatches.Num(); ++Index)
	{
		const auto& Patch = PickaxePatches[Index];
		if (!Patch.Rock.IsValid() || Patch.Rock.Get() != Hit.GetActor()) continue;
		const FTransform Frame = Patch.LocalFrame * Patch.Rock->GetActorTransform();
		const FVector Direction = Frame.GetUnitAxis(EAxis::X);
		const FVector Anchor = Patch.Rock->GetActorTransform().TransformPosition(Patch.LocalPoint);
		const FVector Delta = Hit.ImpactPoint - Anchor;
		const double Depth = FVector::DotProduct(Delta, Direction);
		const double Distance = (Delta - Direction * Depth).SizeSquared();
		const double FullDepth = FVector::DotProduct(Frame.GetLocation() - Anchor, Direction) + Patch.HalfSize.X;
		if (Distance < BestDistance && Distance <= FMath::Square(Patch.Radius * .70f) &&
			Depth >= -Patch.Radius * .15f && Depth <= FullDepth + Patch.Radius * .1f &&
			FVector::DotProduct(Hit.ImpactNormal, -Direction) >= .05)
		{
			Closest = Index;
			BestDistance = Distance;
		}
	}
	return Closest;
}

void UMiningComponent::BuildPickaxeCut(const FHitResult& Hit, FTransform& Frame, FVector& HalfSize) const
{
	const FVector Direction = -Hit.ImpactNormal.GetSafeNormal();
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(Direction, FMath::Abs(Direction.Z) > .95 ? FVector::ForwardVector : FVector::UpVector).ToQuat();
	const float CutRadius = GetMiningRadius();
	const float Inset = FMath::IsFinite(InsetFraction) ? FMath::Clamp(InsetFraction, 0.f, .9f) : .3f;
	const float Lowering = FMath::IsFinite(DownwardOffset) ? FMath::Clamp(DownwardOffset, 0.f, 150.f) : 40.f;
	const float Depth = FMath::IsFinite(DepthMultiplier) ? FMath::Clamp(DepthMultiplier, 1.f, 4.f) : 1.f;
	const float ExtraDepth = (Depth - 1.f) * CutRadius * (1.f + Inset);
	const FVector Offset = -FVector::UpVector * Lowering;
	const FVector TangentOffset = Offset - Direction * FVector::DotProduct(Offset, Direction);
	const float AxialOffset = FMath::Clamp(FVector::DotProduct(Offset, Direction), -CutRadius * .5, CutRadius * (1 - Inset) * .8);
	const FVector Center = Hit.ImpactPoint + Direction * (CutRadius * Inset + ExtraDepth * .5f + AxialOffset) + TangentOffset.GetClampedToMaxSize(CutRadius * .8);
	Frame = FTransform(Rotation, Center);
	HalfSize = FVector(CutRadius + ExtraDepth * .5f, CutRadius, CutRadius);
}

bool UMiningComponent::ApplyPickaxeHit(const FHitResult& Hit)
{
	ADiggableRock* Rock = Cast<ADiggableRock>(Hit.GetActor());
	if (!IsValid(Rock) || !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
		Hit.ImpactNormal.IsNearlyZero() || bDrillMode || bInputSuspended || IsEncounterBlocked()) return false;
	if (const auto* Segment = Cast<AGeneratedMiningRock>(Rock))
	{
		if (Segment->SegmentIndex != INDEX_NONE && Segment->SegmentState != EMiningSegmentState::Active)
		{
			Status = TEXT("This segment is not available for mining");
			return false;
		}
		if (Segment->bCutPending)
		{
			Status = TEXT("Rock is breaking");
			return false;
		}
	}
	RefreshPickaxePatches();
	int32 Index = FindPickaxePatch(Hit);
	if (Index == INDEX_NONE)
	{
		if (PickaxePatches.Num() >= 64)
		{
			const int32 Oldest = PickaxePatches.IndexOfByPredicate([](const FMiningPickaxePatch& Patch) { return !Patch.bPending; });
			if (Oldest == INDEX_NONE) return false;
			PickaxePatches.RemoveAt(Oldest);
		}
		FTransform Frame;
		FVector HalfSize;
		BuildPickaxeCut(Hit, Frame, HalfSize);
		FMiningPickaxePatch Patch;
		Patch.Rock = Rock;
		Patch.LocalPoint = Rock->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
		Patch.LocalSurfacePoint = Patch.LocalPoint;
		Patch.LocalFrame = Frame.GetRelativeTransform(Rock->GetActorTransform());
		Patch.HalfSize = HalfSize;
		Patch.Radius = GetMiningRadius();
		Patch.Id = ++NextPickaxePatchId;
		Patch.Seed = HashCombine(GetTypeHash(Rock->GetFName()), GetTypeHash(FIntVector(
			FMath::RoundToInt(Patch.LocalPoint.X / 4), FMath::RoundToInt(Patch.LocalPoint.Y / 4), FMath::RoundToInt(Patch.LocalPoint.Z / 4))));
		const FVector Direction = Frame.GetUnitAxis(EAxis::X);
		const float FullDepth = FVector::DotProduct(Frame.GetLocation() - Hit.ImpactPoint, Direction) + HalfSize.X;
		Patch.EarlyDepthLimit = FullDepth;
		FHitResult BackSurface;
		if (Rock->ActorLineTraceSingle(BackSurface, Hit.ImpactPoint + Direction * (FullDepth + Patch.Radius * 2),
			Hit.ImpactPoint - Direction, ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeThickness), true)))
		{
			const float Thickness = FVector::DotProduct(BackSurface.ImpactPoint - Hit.ImpactPoint, Direction);
			if (Thickness > 1.f) Patch.EarlyDepthLimit = FMath::Min(FullDepth, Thickness * .85f);
		}
		Index = PickaxePatches.Add(Patch);
		const TWeakObjectPtr<ADiggableRock> WeakRock(Rock);
		if (!PickaxeObservers.Contains(WeakRock))
		{
			PickaxeObservers.Add(WeakRock, Rock->OnTerrainCommitted.AddWeakLambda(this, [this, WeakRock](const FBox& Bounds)
			{
				InvalidatePickaxePatches(WeakRock, Bounds);
			}));
		}
	}
	if (PickaxePatches[Index].bPending) return false;
	LastMinedRock = Rock;
	bLastPickaxeBreak = false;
	++PickaxePatches[Index].Strikes;
	PickaxePatches[Index].bPending = true;
	const FMiningPickaxePatch Request = PickaxePatches[Index];
	FTransform Frame = Request.LocalFrame * Rock->GetActorTransform();
	FVector HalfSize = Request.HalfSize;
	if (Request.Strikes < 3)
	{
		const FVector Direction = Frame.GetUnitAxis(EAxis::X);
		const FVector Anchor = Rock->GetActorTransform().TransformPosition(Request.LocalPoint);
		const float SurfaceOffset = FVector::DotProduct(Anchor - Frame.GetLocation(), Direction);
		const float End = SurfaceOffset + Request.EarlyDepthLimit * (Request.Strikes == 1 ? .23f : .56f);
		const float Start = -HalfSize.X;
		Frame.AddToTranslation(Direction * ((Start + End) * .5f));
		HalfSize.X = (End - Start) * .5f;
		HalfSize.Y *= Request.Strikes == 1 ? .82f : .94f;
		HalfSize.Z *= Request.Strikes == 1 ? .82f : .94f;
	}
	const bool bAccepted = Rock->MinePickaxe(Frame, HalfSize, Request.Seed);
	Status = Rock->LastStatus;
	bLastPickaxeBreak = bAccepted && Request.Strikes == 3;
	TargetPickaxeHits = bAccepted ? (Request.Strikes < 3 ? Request.Strikes : 0) : Request.Strikes - 1;
	if (!bAccepted)
	{
		for (auto& Patch : PickaxePatches)
		{
			if (Patch.Id != Request.Id) continue;
			--Patch.Strikes;
			Patch.bPending = false;
		}
	}
	else OnMiningHit.Broadcast(Hit);
	return bAccepted;
}

void UMiningComponent::InvalidatePickaxePatches(TWeakObjectPtr<ADiggableRock> Rock, const FBox& Bounds)
{
	for (int32 Index = PickaxePatches.Num() - 1; Index >= 0; --Index)
	{
		auto& Patch = PickaxePatches[Index];
		if (Patch.Rock != Rock) continue;
		if (Bounds.IsValid && Rock.IsValid())
		{
			const FVector Anchor = Rock->GetActorTransform().TransformPosition(Patch.LocalPoint);
			if (Bounds.ComputeSquaredDistanceToPoint(Anchor) > FMath::Square(Patch.HalfSize.Size() * 1.12)) continue;
			const FTransform Frame = Patch.LocalFrame * Rock->GetActorTransform();
			const FVector Direction = Frame.GetUnitAxis(EAxis::X);
			FHitResult Surface;
			if (Patch.bPending && Patch.Strikes < 3)
			{
				if (Rock->ActorLineTraceSingle(Surface, Frame.GetLocation() - Direction * (Patch.HalfSize.X + 1),
					Frame.GetLocation() + Direction * (Patch.HalfSize.X + 1), ECC_Visibility,
					FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeStageSurface), true)))
				{
					Patch.LocalSurfacePoint = Rock->GetActorTransform().InverseTransformPosition(Surface.ImpactPoint);
					Patch.bPending = false;
					continue;
				}
			}
			else if (!Patch.bPending)
			{
				const FVector PreviousSurface = Rock->GetActorTransform().TransformPosition(Patch.LocalSurfacePoint);
				if (Rock->ActorLineTraceSingle(Surface, PreviousSurface - Direction, PreviousSurface + Direction,
					ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeDamageInvalidation), true))) continue;
			}
		}
		PickaxePatches.RemoveAt(Index);
	}
	TargetPickaxeHits = 0;
}

void UMiningComponent::RefreshPickaxePatches()
{
	for (int32 Index = PickaxePatches.Num() - 1; Index >= 0; --Index)
	{
		auto& Patch = PickaxePatches[Index];
		if (!Patch.Rock.IsValid())
		{
			PickaxePatches.RemoveAt(Index);
			continue;
		}
		if (!Patch.bPending) continue;
		const auto* Segment = Cast<AGeneratedMiningRock>(Patch.Rock.Get());
		if (Segment && Segment->bCutPending) continue;
		Patch.bPending = false;
		Patch.Strikes = FMath::Max(0, Patch.Strikes - 1);
	}
	for (auto Observer = PickaxeObservers.CreateIterator(); Observer; ++Observer)
	{
		if (PickaxePatches.ContainsByPredicate([&](const FMiningPickaxePatch& Patch) { return Patch.Rock == Observer.Key(); })) continue;
		if (Observer.Key().IsValid()) Observer.Key()->OnTerrainCommitted.Remove(Observer.Value());
		Observer.RemoveCurrent();
	}
}

void UMiningComponent::ClearPickaxeDamage()
{
	PickaxePatches.Reset();
	for (const auto& Observer : PickaxeObservers) if (Observer.Key.IsValid()) Observer.Key->OnTerrainCommitted.Remove(Observer.Value);
	PickaxeObservers.Reset();
	TargetPickaxeHits = 0;
	bLastPickaxeBreak = false;
}
