#include "MainGameStartMenuWidget.h"
#include "MiningUISounds.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MainGameDoor.h"
#include "DiggableRock.h"
#include "StartRoom/MiningOpening.h"
#include "Components/DynamicMeshComponent.h"

namespace
{
	static const FName TitleName(TEXT("MainGameTitle"));
	static const FName StartName(TEXT("MainGameStartButton"));
	static const FName ExitName(TEXT("MainGameExitButton"));

	static UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, Path));
	}
}

void UMainGameStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	TitleTexture = LoadTexture(TEXT("/Game/MainGame/UI/T_MainGameTitle.T_MainGameTitle"));
	StartTexture = LoadTexture(TEXT("/Game/MainGame/UI/T_MainGameStart.T_MainGameStart"));
	ExitTexture = LoadTexture(TEXT("/Game/MainGame/UI/T_MainGameExit.T_MainGameExit"));
	BuildWidgetTree();
}

void UMainGameStartMenuWidget::BuildWidgetTree()
{
	if (!WidgetTree) return;
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainGameCanvas"));
	WidgetTree->RootWidget = Canvas;

	TitleImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TitleName);
	if (TitleTexture) TitleImage->SetBrushFromTexture(TitleTexture, true);
	TitleImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Canvas->AddChild(TitleImage);

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), StartName);
	if (StartTexture)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StartImage"));
		Image->SetBrushFromTexture(StartTexture, true);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		StartButton->AddChild(Image);
	}
	auto StartStyle=StartButton->GetStyle();MiningUISounds::Apply(StartStyle);StartButton->SetStyle(StartStyle);
	StartButton->SetVisibility(ESlateVisibility::Visible);
	Canvas->AddChild(StartButton);
	StartButton->OnClicked.AddDynamic(this, &ThisClass::HandleStartClicked);

	ExitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ExitName);
	if (ExitTexture)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ExitImage"));
		Image->SetBrushFromTexture(ExitTexture, true);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		ExitButton->AddChild(Image);
	}
	ExitButton->SetVisibility(ESlateVisibility::Visible);
	Canvas->AddChild(ExitButton);
	auto ExitStyle=ExitButton->GetStyle();MiningUISounds::Apply(ExitStyle);ExitButton->SetStyle(ExitStyle);
	ExitButton->OnClicked.AddDynamic(this, &ThisClass::ExitGame);

	TitleSize = FVector2D(560.0f, 365.0f);
	TitlePosition = FVector2D(-280.0f, -230.0f);
}

void UMainGameStartMenuWidget::HandleStartClicked()
{
	if (GetWorld())
	{
		for (TActorIterator<AMiningOpening> Opening(GetWorld()); Opening; ++Opening)
		{
			Opening->StartGame();
			if (Opening->bStarted || Opening->bFinished)
			{
				bGameStarted = true;
				RemoveFromParent();
			}
			return;
		}
	}
	BeginGame();
}

void UMainGameStartMenuWidget::BeginGame()
{
	if (bGameStarted) return;
	bGameStarted = true;
	TitleElapsed = 0.0f;
	bMovingToEntrance = false;
	if (StartButton) StartButton->SetVisibility(ESlateVisibility::Collapsed);
	if (ExitButton) ExitButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainGameStartMenuWidget::SetEntranceTarget(ADiggableRock* Rock)
{
	EntranceTarget = Rock;
	bTargetWasObserved = false;
}

void UMainGameStartMenuWidget::BeginTitleToEntrance(AMainGameDoor* Door)
{
	if (!bGameStarted || bMovingToEntrance) return;
	if (!EntranceTarget.IsValid() && GetWorld())
	{
		for (TActorIterator<ADiggableRock> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("BM_MainGame_StartRoomJunction")))
			{
				EntranceTarget = *It;
				break;
			}
		}
	}
	bMovingToEntrance = true;
	TitleElapsed = 0.0f;
	TitleDuration = 0.8f;
}

void UMainGameStartMenuWidget::MoveTitleTo(const FVector2D& Target, float DeltaSeconds)
{
	TitleTarget = Target;
	TitleElapsed += DeltaSeconds;
	const float T = FMath::Clamp(TitleElapsed / FMath::Max(TitleDuration, 0.01f), 0.0f, 1.0f);
	const float Smooth = T * T * (3.0f - 2.0f * T);
	TitlePosition = FMath::Lerp(TitlePosition, TitleTarget, FMath::Clamp(DeltaSeconds * 10.0f, 0.0f, 1.0f));
	if (T >= 1.0f) TitlePosition = TitleTarget;
}

void UMainGameStartMenuWidget::UpdateLayout(const FVector2D& ViewportSize)
{
	if (!Canvas || ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f) return;
	const FVector2D StartButtonSize(390.0f, 150.0f);
	const FVector2D ExitButtonSize(390.0f, 150.0f);
	const FVector2D StartPos((ViewportSize.X - StartButtonSize.X) * 0.5f, ViewportSize.Y * 0.64f);
	const FVector2D ExitPos((ViewportSize.X - ExitButtonSize.X) * 0.5f, ViewportSize.Y * 0.79f);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(StartButton ? StartButton->Slot : nullptr))
	{
		CanvasSlot->SetAutoSize(false); CanvasSlot->SetSize(StartButtonSize); CanvasSlot->SetPosition(StartPos);
	}
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ExitButton ? ExitButton->Slot : nullptr))
	{
		CanvasSlot->SetAutoSize(false); CanvasSlot->SetSize(ExitButtonSize); CanvasSlot->SetPosition(ExitPos);
	}
	if (!bLayoutInitialized)
	{
		TitlePosition = FVector2D((ViewportSize.X - TitleSize.X) * 0.5f, ViewportSize.Y * 0.12f);
		bLayoutInitialized = true;
	}
	if (bGameStarted && !bMovingToEntrance)
		TitleTarget = FVector2D(ViewportSize.X - TitleSize.X - 38.0f, 38.0f);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TitleImage ? TitleImage->Slot : nullptr))
	{
		CanvasSlot->SetAutoSize(false); CanvasSlot->SetSize(TitleSize); CanvasSlot->SetPosition(TitlePosition);
	}
}

void UMainGameStartMenuWidget::HideTitle()
{
	bTitleArrivedAtEntrance = true;
	if (TitleImage) TitleImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainGameStartMenuWidget::ExitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainGameStartMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FVector2D ViewportSize = MyGeometry.GetLocalSize();
	UpdateLayout(ViewportSize);
	if (!bGameStarted || !TitleImage || TitleImage->GetVisibility() == ESlateVisibility::Collapsed) return;

	if (!bMovingToEntrance)
	{
		const FVector2D UpperRight(ViewportSize.X - TitleSize.X - 38.0f, 38.0f);
		MoveTitleTo(UpperRight, InDeltaTime);
	}
	else if (EntranceTarget.IsValid())
	{
		FVector2D Screen;
		if (GetOwningPlayer() && GetOwningPlayer()->ProjectWorldLocationToScreen(EntranceTarget->GetActorLocation() + FVector(0, 0, 110), Screen, true))
		{
			MoveTitleTo(Screen - TitleSize * 0.5f, InDeltaTime);
		}
		if (EntranceTarget->TriangleCount > 0) bTargetWasObserved = true;
		if (bTargetWasObserved && (EntranceTarget->TriangleCount <= 0 || EntranceTarget->RockMesh == nullptr || !EntranceTarget->RockMesh->IsVisible()))
			HideTitle();
	}
}
