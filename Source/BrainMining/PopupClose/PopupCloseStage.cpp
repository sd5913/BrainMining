#include "PopupClose/PopupCloseStage.h"

#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseGameMode.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPopupClose, Log, All);

namespace
{
	constexpr float ScreenEnterSeconds = 0.45f;
	constexpr float ScreenRampSeconds = 14.f;
	constexpr float SlowInterval = 1.35f;
	constexpr float FastInterval = 0.06f;
	constexpr float ClimaxInterval = 0.08f;
	constexpr float ClimaxHoldSeconds = 1.15f;
	constexpr int32 MaxOpenPopups = 14;
	constexpr float SpraySeconds = 4.2f;
	constexpr float SprayInterval = 0.07f;
}

APopupCloseStage::APopupCloseStage()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;
	UStaticMesh* CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;

	auto MakeBox = [this, CubeMesh](const TCHAR* Name, const FVector& Location, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(SceneRoot);
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetRelativeLocation(Location);
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		return Mesh;
	};

	Floor = MakeBox(TEXT("Floor"), FVector(0.f, 0.f, -10.f), FVector(9.4f, 7.4f, 0.2f));
	Ceiling = MakeBox(TEXT("Ceiling"), FVector(0.f, 0.f, 310.f), FVector(9.4f, 7.4f, 0.2f));
	WallPositiveY = MakeBox(TEXT("WallPositiveY"), FVector(0.f, 360.f, 150.f), FVector(9.4f, 0.2f, 3.2f));
	WallNegativeY = MakeBox(TEXT("WallNegativeY"), FVector(0.f, -360.f, 150.f), FVector(9.4f, 0.2f, 3.2f));
	TableTop = MakeBox(TEXT("TableTop"), FVector(0.f, 0.f, 74.f), FVector(1.6f, 0.8f, 0.06f));
	MakeBox(TEXT("Leg1"), FVector(60.f, 28.f, 36.f), FVector(0.08f, 0.08f, 0.72f));
	MakeBox(TEXT("Leg2"), FVector(60.f, -28.f, 36.f), FVector(0.08f, 0.08f, 0.72f));
	MakeBox(TEXT("Leg3"), FVector(-60.f, 28.f, 36.f), FVector(0.08f, 0.08f, 0.72f));
	MakeBox(TEXT("Leg4"), FVector(-60.f, -28.f, 36.f), FVector(0.08f, 0.08f, 0.72f));
	Monitor = MakeBox(TEXT("Monitor"), FVector(48.f, 0.f, 122.f), FVector(0.08f, 0.72f, 0.5f));
	Screen = MakeBox(TEXT("Screen"), FVector(42.f, 0.f, 124.f), FVector(0.02f, 0.62f, 0.4f));
	Stand = MakeBox(TEXT("Stand"), FVector(48.f, 0.f, 92.f), FVector(0.08f, 0.08f, 0.22f));
	MakeBox(TEXT("StandBase"), FVector(40.f, 0.f, 80.f), FVector(0.28f, 0.16f, 0.03f));
	Keyboard = MakeBox(TEXT("Keyboard"), FVector(-10.f, 0.f, 80.f), FVector(0.34f, 0.14f, 0.03f));

	AimVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("AimVolume"));
	AimVolume->SetupAttachment(SceneRoot);
	AimVolume->SetRelativeLocation(FVector(42.f, 0.f, 124.f));
	AimVolume->SetBoxExtent(FVector(36.f, 48.f, 36.f));
	AimVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AimVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	AimVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	AimVolume->SetHiddenInGame(true);

	ScreenCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ScreenCamera"));
	ScreenCamera->SetupAttachment(SceneRoot);
	ScreenCamera->SetRelativeLocation(FVector(-8.f, 0.f, 124.f));
	ScreenCamera->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	ScreenCamera->SetFieldOfView(55.f);
	ScreenCamera->bUsePawnControlRotation = false;

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(SceneRoot);
	SpawnPoint->SetRelativeLocation(FVector(-430.f, 0.f, 92.f));

	SpawnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnArrow"));
	SpawnArrow->SetupAttachment(SpawnPoint);
	SpawnArrow->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-48.f, 25.f, 0.f));
	Sun->SetIntensity(10.f);
	Sun->SetAtmosphereSunLight(true);

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(SceneRoot);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(SceneRoot);

	RoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));
	RoomLight->SetupAttachment(SceneRoot);
	RoomLight->SetMobility(EComponentMobility::Movable);
	RoomLight->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
	RoomLight->SetIntensity(6000.f);
	RoomLight->SetAttenuationRadius(1400.f);
	RoomLight->SetCastShadows(false);
}

void APopupCloseStage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyColors();
    Sun->SetVisibility(!bAreaManaged);
    Sky->SetVisibility(!bAreaManaged);
    Atmosphere->SetVisibility(!bAreaManaged);
}

void APopupCloseStage::SetAreaActive(bool bActive)
{
    if(!bActive) { CancelSession(); SetPromptVisible(GetWorld()->GetFirstPlayerController(),false); }
    bAreaActive=bActive;
}

void APopupCloseStage::ApplyColors()
{
	Paint(Floor, FLinearColor(0.72f, 0.72f, 0.7f));
	Paint(Ceiling, FLinearColor(0.82f, 0.82f, 0.8f));
	Paint(WallPositiveY, FLinearColor(0.9f, 0.9f, 0.88f));
	Paint(WallNegativeY, FLinearColor(0.9f, 0.9f, 0.88f));
	Paint(TableTop, FLinearColor(0.45f, 0.32f, 0.2f));
	Paint(Monitor, FLinearColor(0.08f, 0.08f, 0.09f));
	Paint(Screen, FLinearColor(0.16f, 0.42f, 0.95f));
	Paint(Stand, FLinearColor(0.12f, 0.12f, 0.13f));
	Paint(Keyboard, FLinearColor(0.18f, 0.18f, 0.2f));
}

void APopupCloseStage::Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color) const
{
	if (!Mesh || !BaseMaterial)
	{
		return;
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, const_cast<APopupCloseStage*>(this));
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Mesh->SetMaterial(0, Material);
}

FTransform APopupCloseStage::GetPlayerSpawnTransform() const
{
	return SpawnPoint ? SpawnPoint->GetComponentTransform() : GetActorTransform();
}

bool APopupCloseStage::IsComputerComponent(const UPrimitiveComponent* Component) const
{
	return Component == Screen || Component == Monitor || Component == Stand || Component == Keyboard || Component == AimVolume;
}

bool APopupCloseStage::CanInteract(APlayerController* Controller) const
{
	if (Phase != EPhase::Idle || !Controller)
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn || !Screen)
	{
		return false;
	}

	if (FVector::Dist(Pawn->GetActorLocation(), Screen->GetComponentLocation()) > 320.f)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector End = ViewLocation + ViewRotation.Vector() * 450.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PopupCloseAim), false, Pawn);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return false;
	}

	return Hit.GetActor() == this && IsComputerComponent(Hit.GetComponent());
}

void APopupCloseStage::SetPromptVisible(APlayerController* Controller, bool bVisible)
{
	if (!bVisible)
	{
		if (Prompt)
		{
			Prompt->RemoveFromParent();
			Prompt = nullptr;
		}
		return;
	}

	if (Prompt || !Controller)
	{
		return;
	}

	Prompt = CreateWidget<UPopupClosePromptWidget>(Controller);
	if (!Prompt)
	{
		return;
	}

	Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
	Prompt->AddToViewport(8);
	Prompt->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	Prompt->SetAlignmentInViewport(FVector2D::ZeroVector);
}

void APopupCloseStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
    if(bAreaManaged && !bAreaActive) return;

	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Phase == EPhase::Idle)
	{
		const bool bCanUse = CanInteract(Controller);
		SetPromptVisible(Controller, bCanUse);
		if (bCanUse && Controller->WasInputKeyJustPressed(EKeys::E))
		{
			BeginScreenSession(Controller, Controller->GetPawn());
		}
		return;
	}

	SetPromptVisible(Controller, false);
	if (Phase == EPhase::Entering)
	{
		EnterTime += DeltaSeconds;
		if (EnterTime >= ScreenEnterSeconds)
		{
			if (APlayerController* Locked = LockedController.Get())
			{
				Desktop = CreateWidget<UPopupCloseDesktopWidget>(Locked);
				if (Desktop)
				{
					Desktop->AddToViewport(20);
					Desktop->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
					Desktop->SetAlignmentInViewport(FVector2D::ZeroVector);
					FInputModeGameAndUI InputMode;
					InputMode.SetWidgetToFocus(Desktop->TakeWidget());
					InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
					InputMode.SetHideCursorDuringCapture(false);
					Locked->SetInputMode(InputMode);
					Locked->SetShowMouseCursor(true);
				}
			}
			Phase = EPhase::InScreen;
			ScreenTime = 0.f;
			SpawnDebt = CurrentSpawnInterval();
			ClimaxTime = 0.f;
		}
		return;
	}

	if (Phase == EPhase::InScreen)
	{
		TickScreen(DeltaSeconds);
		return;
	}

	TickSpray(DeltaSeconds);
}

void APopupCloseStage::BeginScreenSession(APlayerController* Controller, APawn* Pawn)
{
	if (!Controller || !Pawn)
	{
		return;
	}

	LockedController = Controller;
	LockedPawn = Pawn;
	Phase = EPhase::Entering;
	EnterTime = 0.f;

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->DisableMovement();
		bMoveSuppressed = true;
	}

	Controller->SetIgnoreLookInput(true);
	bLookSuppressed = true;
	Controller->SetViewTargetWithBlend(this, ScreenEnterSeconds, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
}

float APopupCloseStage::CurrentSpawnInterval() const
{
	const float Alpha = FMath::Clamp(ScreenTime / ScreenRampSeconds, 0.f, 1.f);
	return FMath::Lerp(SlowInterval, FastInterval, FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.4f));
}

void APopupCloseStage::TickScreen(float DeltaSeconds)
{
	ScreenTime += DeltaSeconds;
	const float Interval = CurrentSpawnInterval();
	SpawnDebt += DeltaSeconds;

	if (Desktop && Desktop->GetOpenCount() >= MaxOpenPopups)
	{
		SpawnDebt = FMath::Min(SpawnDebt, Interval);
	}
	else
	{
		int32 Spawned = 0;
		while (Desktop && SpawnDebt >= Interval && Desktop->GetOpenCount() < MaxOpenPopups && Spawned < 3)
		{
			SpawnDebt -= Interval;
			Desktop->SpawnPopup();
			++Spawned;
		}
	}

	if (Interval <= ClimaxInterval)
	{
		ClimaxTime += DeltaSeconds;
		if (ClimaxTime >= ClimaxHoldSeconds)
		{
			EjectFromScreen();
		}
	}
}

void APopupCloseStage::EjectFromScreen()
{
	if (Desktop)
	{
		Desktop->RemoveFromParent();
		Desktop = nullptr;
	}

	Phase = EPhase::Spraying;
	SprayTime = 0.f;
	SprayDebt = 0.f;

	if (APlayerController* Controller = LockedController.Get())
	{
		Controller->SetShowMouseCursor(false);
		Controller->SetInputMode(FInputModeGameOnly());
		if (APawn* Pawn = LockedPawn.Get())
		{
			Controller->SetViewTargetWithBlend(Pawn, 0.22f, EViewTargetBlendFunction::VTBlend_EaseIn, 2.5f);
		}
	}

	RestorePlayerControl(false);
}

void APopupCloseStage::TickSpray(float DeltaSeconds)
{
	SprayTime += DeltaSeconds;
	SprayDebt += DeltaSeconds;
	while (SprayDebt >= SprayInterval && SprayTime <= SpraySeconds)
	{
		SprayDebt -= SprayInterval;
		SpawnFlyingWindow();
	}

	if (bLookSuppressed && SprayTime >= 0.28f)
	{
		RestorePlayerControl(true);
	}

	if (SprayTime >= SpraySeconds)
	{
		++CompletedSessions;
		Phase = EPhase::Idle;
		RestorePlayerControl(true);
	}
}

void APopupCloseStage::SpawnFlyingWindow()
{
	if (!Screen)
	{
		return;
	}

	const FVector OutDir = -GetActorForwardVector();
	const FVector Origin = Screen->GetComponentLocation() + OutDir * 30.f + FVector(
		0.f,
		FMath::FRandRange(-24.f, 24.f),
		FMath::FRandRange(-16.f, 16.f));
	const FRotator Rotation(
		FMath::FRandRange(-18.f, 24.f),
		FMath::FRandRange(-35.f, 35.f),
		FMath::FRandRange(-25.f, 25.f));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APopupCloseFlyingWindow* Window = GetWorld()->SpawnActor<APopupCloseFlyingWindow>(Origin, Rotation, Params);
	if (!Window)
	{
		return;
	}

	const FVector Velocity = OutDir * FMath::FRandRange(520.f, 1100.f)
		+ FVector::UpVector * FMath::FRandRange(180.f, 620.f)
		+ GetActorRightVector() * FMath::FRandRange(-380.f, 380.f);
    if (bAreaManaged) Window->SetLifeSpan(15.f);
	static const FLinearColor Colors[] = {
		FLinearColor(0.93f, 0.94f, 0.96f),
		FLinearColor(1.f, 0.95f, 0.82f),
		FLinearColor(1.f, 0.86f, 0.84f),
		FLinearColor(0.86f, 0.93f, 1.f),
		FLinearColor(0.9f, 0.96f, 0.9f)
	};
	Window->Launch(Colors[FMath::RandRange(0, UE_ARRAY_COUNT(Colors) - 1)], Velocity);
}

void APopupCloseStage::RestorePlayerControl(bool bRestoreLook)
{
	APlayerController* Controller = LockedController.Get();
	APawn* Pawn = LockedPawn.Get();
	if (bMoveSuppressed)
	{
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		bMoveSuppressed = false;
	}

	if (bRestoreLook && bLookSuppressed && Controller)
	{
		Controller->SetIgnoreLookInput(false);
		bLookSuppressed = false;
	}
}

void APopupCloseStage::CancelSession()
{
	if (Desktop) { Desktop->RemoveFromParent(); Desktop = nullptr; }
	if (Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
	RestorePlayerControl(true);
	if (auto* Controller = LockedController.Get())
	{
		if (auto* Pawn = LockedPawn.Get()) Controller->SetViewTarget(Pawn);
		Controller->SetShowMouseCursor(false);
		Controller->SetInputMode(FInputModeGameOnly());
	}
	LockedPawn.Reset();
	LockedController.Reset();
	Phase = EPhase::Idle;
}

void APopupCloseStage::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelSession();
	Super::EndPlay(Reason);
}

#if WITH_EDITOR
void APopupCloseStage::CreateLevel()
{
	if (!GEditor)
	{
		return;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World)
	{
		UE_LOG(LogPopupClose, Error, TEXT("Failed to create a blank map for the popup-close level."));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<APopupCloseStage>(APopupCloseStage::StaticClass(), FTransform::Identity, Params);

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		Settings->DefaultGameMode = APopupCloseGameMode::StaticClass();
	}

	const FString AssetPath = TEXT("/Game/PopupClose/Maps/Lvl_PopupClose");
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, AssetPath))
	{
		UE_LOG(LogPopupClose, Error, TEXT("Failed to save %s."), *AssetPath);
		return;
	}

	UE_LOG(LogPopupClose, Display, TEXT("Saved popup-close level to %s."), *AssetPath);
}

static FAutoConsoleCommand GCreatePopupCloseLevel(
	TEXT("PopupClose.CreateLevel"),
	TEXT("Create and save Content/PopupClose/Maps/Lvl_PopupClose."),
	FConsoleCommandDelegate::CreateStatic(&APopupCloseStage::CreateLevel));
#endif
