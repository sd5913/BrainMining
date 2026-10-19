#include "MiningComponent.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningBombToolComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool UMiningComponent::ApplyPickaxeBalance(const FMiningPickaxeLevel& Level)
{
	FString Error;
	if (!Level.Validate(Error)) return false;
	PickaxeBalance = Level;
	bUsePickaxeBalance = true;
	return true;
}

float UMiningComponent::GetPickaxeImpactDelay() const
{
	const float Delay = FMath::Min(13.f / 30.f, GetMiningInterval()) * (.19f / (13.f / 30.f));
	return bMobileMiningInput ? FMath::Min(Delay, .06f) : Delay;
}

float UMiningComponent::GetPickaxeDepthMultiplier(const ADiggableRock* Rock) const
{
	return bUsePickaxeBalance && PickaxeBalance.bUseHardness && Rock ?
		UMiningPickaxeSettings::DepthFactor(PickaxeBalance.DestructionLevel,Rock->HardnessLevel):1.f;
}

float UMiningComponent::GetPickaxeDepth(const ADiggableRock* Rock) const
{
	return (bUsePickaxeBalance?PickaxeBalance.Depth*PickaxeSizeScale:Radius*(1+InsetFraction))*GetPickaxeDepthMultiplier(Rock);
}

void UMiningComponent::ResolvePickaxeStrike()
{
	const FHitResult SwingTarget = PickaxeSwingTarget;
	PickaxeSwingTarget = FHitResult();
	if (!GetWorld() || !GetOwner() || bDrillMode || bInputSuspended || IsEncounterBlocked()) return;
	if (const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>(); Bomb && Bomb->bEquipped) return;
	ADiggableRock* SwingRock = Cast<ADiggableRock>(SwingTarget.GetActor());
	RefreshTarget();
	if (IsValid(TargetRock) && (!IsValid(SwingRock) || TargetRock == SwingRock)) { ApplyPickaxeHit(TargetHit); return; }
	// Keep an already-started strike on a narrow edge through a few centimetres
	// of camera sway. Re-trace from the current eye: never use stale terrain,
	// mine through another object, extend reach, or stick to a deliberate turn.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && IsValid(SwingRock))
	{
		FVector Eye; FRotator View;
		PC->GetPlayerViewPoint(Eye, View);
		const FVector ToContact = SwingTarget.ImpactPoint - Eye;
		const double Along = FVector::DotProduct(ToContact, View.Vector());
		const double Margin = FMath::Max(12.f, GetMiningRadius() * .35f);
		if (Along > 0 && ToContact.Size() <= Reach &&
			(ToContact - View.Vector() * Along).SizeSquared() <= FMath::Square(Margin) &&
			FVector::DotProduct(ToContact.GetSafeNormal(), View.Vector()) >= FMath::Cos(FMath::DegreesToRadians(12.f)))
		{
			FHitResult Verified;
			if (GetWorld()->LineTraceSingleByChannel(Verified, Eye, SwingTarget.ImpactPoint + ToContact.GetSafeNormal() * 2,
				ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeSwingContact), true, GetOwner())) &&
				Verified.GetActor() == SwingTarget.GetActor() && Verified.Distance <= Reach &&
				FVector::DistSquared(Verified.ImpactPoint, SwingTarget.ImpactPoint) <= 4)
			{
				ApplyPickaxeHit(Verified); return;
			}
		}
		if (SwingTarget.bBlockingHit && SwingTarget.Distance <= Reach)
		{
			ApplyPickaxeHit(SwingTarget);
			return;
		}
	}
	Status = TEXT("Aim at rock within reach");
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
	if (bUsePickaxeBalance)
	{
		const float FrontDepth = CutRadius * (1.f - Inset);
		const float FinalDepth = GetPickaxeDepth(Cast<ADiggableRock>(Hit.GetActor()));
		Frame = FTransform(Rotation, Hit.ImpactPoint + Direction * ((FinalDepth - FrontDepth) * .5f) + TangentOffset.GetClampedToMaxSize(CutRadius * .8));
		HalfSize = FVector((FinalDepth + FrontDepth) * .5f, CutRadius, CutRadius);
		return;
	}
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
	if(Rock->ActorHasTag(TEXT("CardEscapeTelevision")))return MineTelevision(Hit);
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
	if (Rock->ActorHasTag(TEXT("BM_BombOnly")))
	{
		// Bomb-only layers deliberately stay silent under the mouse reticle.
		// Showing a mining failure prompt makes the cursor look actionable and
		// competes with the bomb interaction prompt.
		Status.Empty();
		TargetPickaxeHits = 0; bLastPickaxeBreak = false; return false;
	}
	if (GetPickaxeDepthMultiplier(Rock)<=0)
	{
		Status=FString::Printf(TEXT("破坏等级不足：矿镐 %d / 岩层硬度 %d，无法破坏"),PickaxeBalance.DestructionLevel,Rock->HardnessLevel);
		TargetPickaxeHits=0;bLastPickaxeBreak=false;return false;
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
		if (bUsePickaxeBalance)
		{
			Patch.Stages = PickaxeBalance.Stages;
			if(PickaxeBalance.bUseHardness){Patch.Stages.SetNum(1);Patch.Stages[0]=FMiningPickaxeStage();}
			Patch.Roughness = PickaxeBalance.Roughness;
		}
		else
		{
			const float Depths[] = {23, 56, 100};
			const float Widths[] = {82, 94, 100};
			for (int32 StageIndex = 0; StageIndex < 3; ++StageIndex)
			{
				auto& Stage = Patch.Stages.AddDefaulted_GetRef();
				Stage.DepthPercent = Depths[StageIndex];
				Stage.WidthPercent = Widths[StageIndex];
			}
		}
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
	const int32 RequiredStrikes = Request.Stages.Num();
	if (Request.Strikes < RequiredStrikes)
	{
		const auto& Stage = Request.Stages[Request.Strikes - 1];
		const FVector Direction = Frame.GetUnitAxis(EAxis::X);
		const FVector Anchor = Rock->GetActorTransform().TransformPosition(Request.LocalPoint);
		const float SurfaceOffset = FVector::DotProduct(Anchor - Frame.GetLocation(), Direction);
		const float End = SurfaceOffset + Request.EarlyDepthLimit * Stage.DepthPercent * .01f;
		const float Start = -HalfSize.X;
		Frame.AddToTranslation(Direction * ((Start + End) * .5f));
		HalfSize.X = (End - Start) * .5f;
		HalfSize.Y *= Stage.WidthPercent * .01f;
		HalfSize.Z *= Stage.WidthPercent * .01f;
	}
	OnPickaxeContact.Broadcast(Hit, Request.Strikes);
	const bool bAccepted = Rock->MinePickaxe(Frame, HalfSize, Request.Seed, Request.Roughness);
	Status = Rock->LastStatus;
	bLastPickaxeBreak = bAccepted && Request.Strikes == RequiredStrikes;
	TargetPickaxeHits = bAccepted ? (Request.Strikes < RequiredStrikes ? Request.Strikes : 0) : Request.Strikes - 1;
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
			if (Patch.bPending && Patch.Strikes >= Patch.Stages.Num()) OnPickaxeBreak.Broadcast(Anchor);
			const FTransform Frame = Patch.LocalFrame * Rock->GetActorTransform();
			const FVector Direction = Frame.GetUnitAxis(EAxis::X);
			FHitResult Surface;
			if (Patch.bPending && Patch.Strikes < Patch.Stages.Num())
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
