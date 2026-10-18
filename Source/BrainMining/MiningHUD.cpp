#include "MiningHUD.h"
#include "BrainMiningFonts.h"
#include "MiningCharacterMovementComponent.h"
#include "BrainMiningCharacter.h"
#include "MiningComponent.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningTunnelGenerator.h"
#include "EngineUtils.h"
#include "FogChamberComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "MiningDeveloperMapComponent.h"

AMiningHUD::AMiningHUD()
{
	DeveloperMap = CreateDefaultSubobject<UMiningDeveloperMapComponent>(TEXT("DeveloperMap"));
}

void AMiningHUD::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (bShowDeveloperOverlay && PlayerOwner && PlayerOwner->IsLocalController())
	{
		EnableInput(PlayerOwner);
		DeveloperMap->BindInput(InputComponent);
	}
#endif
}

void AMiningHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner || !PlayerOwner->GetPawn())
	{
		return;
	}
	const UMiningComponent* Mining = PlayerOwner->GetPawn()->FindComponentByClass<UMiningComponent>();
	if (!Mining)
	{
		return;
	}
	const FLinearColor Accent(0.2f, 0.9f, 0.8f);
	if (!bShowDeveloperOverlay) return;
	const AMiningTunnelGenerator* Layout = nullptr;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Generator->bGeneratedSegmented) { Layout = *Generator; break; }
	}
	DrawRect(FLinearColor(0.015f, 0.025f, 0.04f, 0.8f), 20, 20, 530, Layout ? 205 : 180);
	DrawText(TEXT("BRAIN / MINING LAB"), Accent, 36, 30, BrainMiningFonts::Asset(true), 1.0f);
	DrawText(Mining->Status, FLinearColor::White, 36, 59, BrainMiningFonts::Asset());
	const ABrainMiningCharacter* Character = Cast<ABrainMiningCharacter>(PlayerOwner->GetPawn());
	DrawText(FString::Printf(TEXT("Scale %.0f%%  |  Reach %.1fm  |  Radius %.2fm"), Character ? Character->AppliedPlayerScale * 100 : 100,
		Mining->Reach / 100.0f, Mining->GetMiningRadius() / 100.0f), FLinearColor::White, 36, 81, BrainMiningFonts::Asset());
	const ADiggableRock* Rock = IsValid(Mining->TargetRock) ? Mining->TargetRock.Get() : Mining->LastMinedRock.Get();
	if (Layout)
	{
		Rock = Layout->GetActiveSegment();
		int32 TotalCuts = 0;
		int32 RetainedTriangles = 0;
		int32 Recycled = 0;
		for (const AGeneratedMiningRock* Segment : Layout->GeneratedSegments)
		{
			if (!IsValid(Segment)) continue;
			TotalCuts += Segment->SuccessfulCuts;
			RetainedTriangles += Segment->TriangleCount;
			Recycled += Segment->SegmentState == EMiningSegmentState::Recycled ? 1 : 0;
		}
		DrawText(FString::Printf(TEXT("Room %d | Active segment %d/%d | Total cuts %d"), Layout->CurrentRoomIndex,
			Layout->ActiveSegmentIndex == INDEX_NONE ? 0 : Layout->ActiveSegmentIndex + 1, Layout->GeneratedSegments.Num(), TotalCuts), Accent, 36, 150, BrainMiningFonts::Asset());
		DrawText(FString::Printf(TEXT("Retained triangles %d (not a shared budget) | Recycled %d"), RetainedTriangles, Recycled), FLinearColor::White, 36, 173, BrainMiningFonts::Asset());
	}
	if (IsValid(Rock))
	{
		const AGeneratedMiningRock* Segment = Cast<AGeneratedMiningRock>(Rock);
		const FString MeshStats = Segment && Segment->bChunked
			? FString::Printf(TEXT("Cuts %d | Chunks %d | Largest %d / %d | Reduced %d"), Rock->SuccessfulCuts, Segment->Chunks.Num(), Segment->LargestChunkTriangles, Rock->MaxTriangles, Segment->SimplifiedTriangles)
			: FString::Printf(TEXT("Cuts %d  |  Triangles %d / %d"), Rock->SuccessfulCuts, Rock->TriangleCount, Rock->MaxTriangles);
		DrawText(MeshStats, Accent, 36, 103, BrainMiningFonts::Asset());
		DrawText(FString::Printf(TEXT("Boolean %.1fms  Collision %.1fms  Total %.1fms"), Rock->LastBooleanMilliseconds, Rock->LastCollisionMilliseconds, Rock->LastTotalMilliseconds), FLinearColor::White, 36, 125, BrainMiningFonts::Asset());
		if (!Layout) DrawText(Rock->FogChamber->GetStateLabel(), Accent, 36, 150, BrainMiningFonts::Asset());
	}
	const UMiningCharacterMovementComponent* Movement = Character ? Cast<UMiningCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	const bool bFlightEnabled = Movement && Movement->FlightSettings.bEnabled;
	if (bFlightEnabled)
	{
		const TCHAR* FlightState = Movement->IsFlightThrustActive() ? TEXT("LIFT") : Movement->JetpackHangRemaining > 0.f ? TEXT("COAST") : Movement->IsFalling() ? TEXT("FALL") : TEXT("GROUND");
		DrawText(FString::Printf(TEXT("Flight: %s | Vertical %.1f m/s"), FlightState, Movement->Velocity.Z / 100), Accent, 28, Canvas->ClipY - 64, BrainMiningFonts::Asset());
	}
	DrawText(bFlightEnabled ? TEXT("WASD Move   Hold SPACE Rise / Release Fall   Hold LMB Mine   R Reset")
		: TEXT("WASD Move   SPACE Jump   Hold LMB Mine   R Reset   F1 Cut preview"), FLinearColor::White, 28, Canvas->ClipY - 38, BrainMiningFonts::Asset(), 1.1f);
	DeveloperMap->Draw(this, Canvas);
}
