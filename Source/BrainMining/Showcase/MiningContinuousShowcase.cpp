#include "Showcase/MiningShowcase.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningEncounterPlayer.h"
#include "MiningHUD.h"
#include "MiningGUIWidgets.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"

int32 AMiningShowcaseDirector::FindRegion(const FVector& Position) const
{
    for (int32 Index = 0; Index < Exhibits.Num(); ++Index)
        if (FBox(Exhibits[Index].Center - Exhibits[Index].Extent, Exhibits[Index].Center + Exhibits[Index].Extent).IsInsideOrOn(Position)) return Index;
    return INDEX_NONE;
}

void AMiningShowcaseDirector::UpdateContinuousRegion()
{
    auto* Controller = Player();
    if (!Controller || !Controller->GetPawn()) return;
    if (auto* MiningController = Cast<ABrainMiningPlayerController>(Controller); MiningController && MiningController->MiningGUI && TreasureField)
        MiningController->MiningGUI->SetCoinDisplay(FText::AsNumber(TreasureField->Coins));
    if (!bContinuousReady)
    {
        for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) if (Field->Items.IsEmpty()) return;
        if (auto* Character = Cast<ACharacter>(Controller->GetPawn()))
        {
            Character->GetCapsuleComponent()->SetCapsuleSize(32, 88);
            Character->GetCharacterMovement()->MaxWalkSpeed = 600;
            Character->GetCharacterMovement()->MaxStepHeight = 20;
        }
        if (auto* Mining = Controller->GetPawn()->FindComponentByClass<UMiningComponent>())
        {
            Mining->bScopedReset = true;
            Mining->FallResetDistance = 0;
        }
        for (TActorIterator<AEchoPassageStage> Stage(GetWorld()); Stage; ++Stage)
        {
            Stage->DestinationLevel.Reset();
            Stage->OnPassageCompleted.AddUniqueDynamic(this, &AMiningShowcaseDirector::OnEchoCompleted);
        }
        for (TActorIterator<ACardRoomStage> Stage(GetWorld()); Stage; ++Stage)
        {
            Stage->OnRoomCompleted.AddUniqueDynamic(this, &AMiningShowcaseDirector::OnCardCompleted);
            Stage->OnCollectibleClaimed.AddUniqueDynamic(this, &AMiningShowcaseDirector::OnCardReward);
        }
        bContinuousReady = true;
        for (auto& Exhibit : Exhibits)
            if (Exhibit.Kind == EMiningShowcaseKind::Popup) Exhibit.Extent = Exhibit.Extent.ComponentMax(FVector(1250,950,650));
        TArray<UPostProcessComponent*> LegacyExposure;
        GetComponents(LegacyExposure);
        for (auto* Exposure : LegacyExposure) Exposure->bEnabled = false;
        LastContinuousSafePosition = Controller->GetPawn()->GetActorLocation();
        SwitchContinuousRegion(FindRegion(Controller->GetPawn()->GetActorLocation()));
    }
    const int32 Next = FindRegion(Controller->GetPawn()->GetActorLocation());
    if (Next != ActiveIndex) SwitchContinuousRegion(Next);
    if (auto* Character = Cast<ACharacter>(Controller->GetPawn()))
    {
        if (Character->GetCharacterMovement()->IsMovingOnGround() && !IsModalActive()) LastContinuousSafePosition = Character->GetActorLocation();
        if (Character->GetActorLocation().Z < -1200)
        {
            CloseContinuousInteraction();
            Character->SetActorLocation(LastContinuousSafePosition + FVector(0,0,10), false, nullptr, ETeleportType::TeleportPhysics);
            Character->GetCharacterMovement()->StopMovementImmediately();
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Status = FText::FromString(TEXT("已从地图外恢复到最近落脚点，玩法进度未重置。"));
        }
    }
}

void AMiningShowcaseDirector::CloseContinuousInteraction()
{
    if (auto* Controller = Cast<ABrainMiningPlayerController>(Player())) Controller->SetUpgradeMenuHeld(false);
    for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) if (Field->bCatalogueOpen) Field->ToggleCatalogue();
    for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle) if (Puzzle->bEditing) Puzzle->ToggleEdit();
    for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) Stage->CancelSession();
    if (CardInteraction) CardInteraction->LeaveSeat();
    Status = FText::FromString(TEXT("已退出交互。沿步道步行前往其他区域。"));
}

void AMiningShowcaseDirector::SwitchContinuousRegion(int32 Index)
{
    CloseContinuousInteraction();
    if (EchoInteraction) { EchoInteraction->DestroyComponent(); EchoInteraction = nullptr; }
    if (CardInteraction) { CardInteraction->DestroyComponent(); CardInteraction = nullptr; }
    Policy->Release();
    ActiveIndex = Index;
    bModalWasActive = false;
    const bool bHasRegion = Exhibits.IsValidIndex(Index);
    const EMiningShowcaseKind Kind = GetActiveKind();
    if (TreasureField) TreasureField->SetAreaActive(bHasRegion && Kind == EMiningShowcaseKind::Treasure);
    for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) Field->SetAreaActive(bHasRegion && Kind == EMiningShowcaseKind::Collection);
    for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle) Puzzle->SetAreaActive(bHasRegion && Kind == EMiningShowcaseKind::WenSi);
    for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) Stage->SetAreaActive(bHasRegion && Kind == EMiningShowcaseKind::Popup);
    auto* Controller = Player();
    if (!Controller || !Controller->GetPawn()) return;
    if (!bHasRegion || (Kind != EMiningShowcaseKind::Mining && Kind != EMiningShowcaseKind::Collection && Kind != EMiningShowcaseKind::Treasure && Kind != EMiningShowcaseKind::Popup)) Policy->Apply(Controller->GetPawn());
    if (auto* MiningController = Cast<ABrainMiningPlayerController>(Controller))
        MiningController->SetMiningGUIEnabled(bHasRegion && (Kind == EMiningShowcaseKind::Mining || Kind == EMiningShowcaseKind::Treasure || Kind == EMiningShowcaseKind::Popup));
    if (auto* Movement = Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())
    {
        Movement->ResetFlightInput();
        Movement->FlightSettings.bEnabled = bHasRegion && (Kind == EMiningShowcaseKind::Collection || Kind == EMiningShowcaseKind::Mining || Kind == EMiningShowcaseKind::Treasure);
        Movement->FlightSettings.bEnableHeat = Kind == EMiningShowcaseKind::Mining || Kind == EMiningShowcaseKind::Treasure;
        Movement->MaxWalkSpeed = bHasRegion && Kind == EMiningShowcaseKind::WenSi ? 260 : 600;
    }
    if (auto* Mining = Controller->GetPawn()->FindComponentByClass<UMiningComponent>())
    {
        Mining->CancelMining();
        Mining->ResetBounds = bHasRegion ? FBox(Exhibits[Index].Center - Exhibits[Index].Extent, Exhibits[Index].Center + Exhibits[Index].Extent) : FBox(ForceInit);
    }
    Controller->ClientSetHUD(bHasRegion && Kind == EMiningShowcaseKind::Mining ? AMiningHUD::StaticClass() : AHUD::StaticClass());
    if (auto* Camera = Controller->GetPawn()->FindComponentByClass<UCameraComponent>())
    {
        Camera->PostProcessBlendWeight = 1;
        Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
        Camera->PostProcessSettings.AutoExposureBias = bHasRegion && Kind == EMiningShowcaseKind::Cards ? -6.f : 0.f;
    }
    if (bHasRegion && Kind == EMiningShowcaseKind::Echo)
    {
        EchoInteraction = NewObject<UEchoPassageInteractionComponent>(Controller);
        EchoInteraction->RegisterComponent();
    }
    if (bHasRegion && Kind == EMiningShowcaseKind::Cards)
    {
        CardInteraction = NewObject<UCardRoomInteractionComponent>(Controller);
        CardInteraction->RegisterComponent();
    }
    if (auto* Subtitles = USubtitleSubsystem::Get(this)) Subtitles->StopAll();
    if (bHasRegion) Visited.Add(Exhibits[Index].Id);
    Status = FText::FromString(TEXT("Ctrl + F10 打开 GM · F2 路线提示 · F8 退出交互 · 本地图内进度保留"));
}

void AMiningShowcaseDirector::RestartContinuousRegion()
{
    if (!Exhibits.IsValidIndex(ActiveIndex) || !Player() || !Player()->GetPawn()) return;
    CloseContinuousInteraction();
    if (GetActiveKind() == EMiningShowcaseKind::Mining)
        if (auto* Mining = Player()->GetPawn()->FindComponentByClass<UMiningComponent>()) Mining->ResetField();
    if (GetActiveKind() == EMiningShowcaseKind::Treasure && TreasureField) TreasureField->ResetExhibit();
    if (GetActiveKind() == EMiningShowcaseKind::Collection)
        for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) Field->ResetCollection();
    if (EchoInteraction) EchoInteraction->Restart();
    if (GetActiveKind() == EMiningShowcaseKind::Popup)
        for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) Stage->ResetCompletedSessions();
    if (GetActiveKind() == EMiningShowcaseKind::WenSi)
    {
        for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle)
        {
            if (Puzzle->GetActorTransform().InverseTransformPosition(Player()->GetPawn()->GetActorLocation()).X > 0)
            {
                Status = FText::FromString(TEXT("请走回文思入口再按 F5，避免重置障碍物时夹住角色。"));
                return;
            }
            Puzzle->bComplete = false;
            Puzzle->CloseThought(0); Puzzle->CloseThought(1);
            for (AWenSiObject* Object : Puzzle->Objects)
            {
                Object->SetActorLocation(Object->Rest);
                Object->Rule = Object->ObjectKind == 0 ? EWenSiRule::Heavy : Object->ObjectKind == 1 ? EWenSiRule::Light : EWenSiRule::Intangible;
                Object->ApplyRule();
            }
        }
    }
    Completed.Remove(Exhibits[ActiveIndex].Id);
    Status = FText::FromString(TEXT("已重置当前区域，其他区域进度不变。"));
}
