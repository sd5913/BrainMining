#if WITH_DEV_AUTOMATION_TESTS
#include "BrainMiningCharacter.h"
#include "MiningCharacterMovementComponent.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningJetpackMovementTest, "BrainMining.Flight.CharacterJumpCoastAndCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningJetpackMovementTest::RunTest(const FString& Parameters)
{
	UClass* PawnClass = LoadClass<ABrainMiningCharacter>(nullptr, TEXT("/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C"));
	if (!TestNotNull(TEXT("Main game character blueprint exists"), PawnClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	// Actor ProcessEvent intentionally ignores input functions until the world is initialized.
	World->InitializeActorsForPlay(FURL());
	const auto AddSlab = [World](float Z)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(2000, 2000, 25));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(FVector(0, 0, Z));
		return Actor;
	};
	AddSlab(-25);
	auto* Pawn = World->SpawnActor<ABrainMiningCharacter>(PawnClass, FVector(0, 0, 100), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Character spawns"), Pawn)) return false;
	auto* Controller = World->SpawnActor<AAIController>();
	Controller->Possess(Pawn);
	auto* Movement = CastChecked<UMiningCharacterMovementComponent>(Pawn->GetCharacterMovement());
	Movement->FlightSettings.bEnabled = true;
	const auto Step = [Movement](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame) Movement->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	};
	Movement->SetMovementMode(MOVE_Falling);
	Step(30);
	TestTrue(TEXT("Fixture settles on floor with real capsule collision"), Movement->IsMovingOnGround());
	const double GroundZ = Pawn->GetActorLocation().Z;
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	Step(5);
	TestTrue(TEXT("Short press really jumps on the first input"), Pawn->GetActorLocation().Z > GroundZ + 10 && Movement->Velocity.Z > 0);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr);
	TestFalse(TEXT("Short press never ignites jetpack"), Movement->IsFlightThrustActive());
	TestEqual(TEXT("Short jump generates no heat"), Movement->JetpackHeat, 0.f);
	Step(90);
	TestTrue(TEXT("Normal jump lands"), Movement->IsMovingOnGround());
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	Step(60);
	TestTrue(TEXT("Long hold reaches much higher than a normal jump"), Pawn->GetActorLocation().Z > GroundZ + 350);
	TestTrue(TEXT("Long hold reaches boost speed"), Movement->Velocity.Z > 800);
	const double ReleaseZ = Pawn->GetActorLocation().Z;
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr);
	Step(20);
	TestTrue(TEXT("Release coasts above the release point"), Pawn->GetActorLocation().Z > ReleaseZ && FMath::Abs(Movement->Velocity.Z) < 200);
	Step(35);
	TestTrue(TEXT("After coast ends descent becomes fast"), Movement->Velocity.Z < -800);
	Step(120);
	TestTrue(TEXT("Fast descent lands without tunnelling through floor"), Movement->IsMovingOnGround());
	TestEqual(TEXT("Landing clears hover"), Movement->JetpackHangRemaining, 0.f);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	Step(4);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr);
	Step(6);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	TestTrue(TEXT("Jump then second press ignites in the real character"), Movement->IsFlightThrustActive());
	Step(35);
	const double CeilingZ = Pawn->GetActorLocation().Z + 200;
	AActor* Ceiling = AddSlab(CeilingZ + 25);
	Step(45);
	TestTrue(TEXT("Boost cannot pass through ceiling"), Pawn->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() <= CeilingZ + 2);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr);
	Step(45);
	TestTrue(TEXT("Release leaves ceiling and falls"), Movement->Velocity.Z < 0);
	// Stage a separate long drop at the new maximum speed to check collision sweeps.
	Ceiling->Destroy();
	Movement->ResetFlightInput();
	Pawn->SetActorLocation(FVector(0, 0, 6000), false, nullptr, ETeleportType::TeleportPhysics);
	Movement->SetMovementMode(MOVE_Falling);
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.3f);
	Movement->Velocity = FVector(0, 0, -3250);
	Movement->SetFlightInputHeld(false);
	Step(120);
	TestTrue(TEXT("New high fall speed still lands on the floor without tunnelling"),
		Movement->IsMovingOnGround() && FMath::Abs(Pawn->GetActorLocation().Z - GroundZ) < 5.);
	// Reproduce a late correction near the floor through the actual character input.
	Movement->ResetFlightInput();
	Pawn->SetActorLocation(FVector(0, 0, GroundZ + 350), false, nullptr, ETeleportType::TeleportPhysics);
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(0, 0, -3250);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	TestTrue(TEXT("Character input immediately ignites during descent"), Movement->IsFlightThrustActive());
	Step(8);
	TestTrue(TEXT("Air catch brakes before reaching the nearby floor"),
		Movement->IsFalling() && Movement->Velocity.Z > -500 && Pawn->GetActorLocation().Z > GroundZ + 100);
	Step(16);
	TestTrue(TEXT("Keeping Space held recovers into ascent without landing"), Movement->IsFalling() && Movement->Velocity.Z > 400);
	TestTrue(TEXT("Aerial recovery uses the normal heat budget"), Movement->JetpackHeat > 0.f);
	Pawn->ProcessEvent(Pawn->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr);
	TestTrue(TEXT("Release after an aerial recovery can coast again"), Movement->JetpackHangRemaining > 0.f);
	Controller->UnPossess();
	TestFalse(TEXT("Losing possession clears input"), Movement->bFlightInputHeld);
	return true;
}
#endif
