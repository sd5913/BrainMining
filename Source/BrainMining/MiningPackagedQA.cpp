#include "MiningPackagedQA.h"
#include "MiningExperienceSubsystem.h"
#include "MiningComponent.h"
#include "MiningBombToolComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MiningSaveSubsystem.h"
#include "MiningGUIWidgets.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/RichTextBlock.h"
#include "MiningCollectionWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "UObject/UObjectIterator.h"
#include "MiningWalletSubsystem.h"
#include "MiningProgressionComponent.h"
#include "MiningCollection.h"
#include "MiningRoomEscape.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/MouseMuseumPortal.h"
#include "EndingEmergence.h"
#include "GeneratedMiningRock.h"
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "StartRoom/MiningOpening.h"
#include "StartRoom/MiningStartRoom.h"
#include "PopupClose/PopupCloseStage.h"
#include "CardRoomStage.h"
#include "CardRoomPlayer.h"
#include "NiagaraComponent.h"
#include "HeavenToilet.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
    template<typename T> T* First(UWorld* W){for(TActorIterator<T> It(W);It;++It)return *It;return nullptr;}
    TArray<TSharedPtr<FJsonValue>> Vec(FVector V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
    FVector ReadVec(const FJsonObject& J,const TCHAR* Name,FVector Default=FVector::ZeroVector)
    {
        const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
        return J.TryGetArrayField(Name,A)&&A->Num()==3?FVector((*A)[0]->AsNumber(),(*A)[1]->AsNumber(),(*A)[2]->AsNumber()):Default;
    }
    TSharedPtr<SWidget> FindVisibleTag(const TSharedRef<SWidget>& Widget,FName Tag)
    {
        if(!Widget->GetVisibility().IsVisible())return nullptr;
        if(Widget->GetTag()==Tag)return Widget;
        auto* Children=Widget->GetChildren();
        for(int32 I=0;I<Children->Num();++I)if(auto Found=FindVisibleTag(Children->GetChildAt(I),Tag))return Found;
        return nullptr;
    }
    void ReadMenuTags(const TSharedRef<SWidget>& Widget,TArray<TSharedPtr<FJsonValue>>& Tags)
    {
        if(!Widget->GetVisibility().IsVisible())return;
        const FString Tag=Widget->GetTag().ToString();
        if(Tag.StartsWith(TEXT("PauseAction_"))||Tag.StartsWith(TEXT("OpeningQuit")))Tags.Add(MakeShared<FJsonValueString>(Tag));
        auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)ReadMenuTags(Children->GetChildAt(I),Tags);
    }
}
void UMiningPackagedQA::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString UserDir;
    // The separate user directory is mandatory: QA must never touch a player's slot.
    bEnabled=FParse::Param(FCommandLine::Get(),TEXT("MiningQA")) &&
        FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) && !UserDir.IsEmpty();
    if(bEnabled){Directory=FPaths::ProjectSavedDir()/TEXT("PackagedQA");IFileManager::Get().MakeDirectory(*Directory,true);}
}
void UMiningPackagedQA::Tick(float)
{
    const double Now=FPlatformTime::Seconds();
    if(!SampleLabel.IsEmpty() && LastFrame>0)FrameSamples.Add((Now-LastFrame)*1000.0);
    LastFrame=Now;
    if(!SampleLabel.IsEmpty())
        if(auto* Controller=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;Controller && Controller->PlayerCameraManager)
        {
            const FVector Camera=Controller->PlayerCameraManager->GetCameraLocation();
            if(bCameraSampleStarted)MaxCameraStep=FMath::Max(MaxCameraStep,FVector::Dist(Camera,SampleCamera));
            SampleCamera=Camera;bCameraSampleStarted=true;
        }
    if(Now<NextPoll)return;NextPoll=Now+.1;
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if(!GetWorld() || (Hall && !Hall->bReady) || !GetWorld()->GetFirstPlayerController() || !GetWorld()->GetFirstPlayerController()->GetPawn())return;
    FString Text;TSharedPtr<FJsonObject> Command;
    if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("command.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Command) || !Command)return;
    FString Id;if(!Command->TryGetStringField(TEXT("id"),Id) || Id==LastId)return;
    LastId=Id;Execute(Command);
}
void UMiningPackagedQA::Execute(const TSharedPtr<FJsonObject>& C)
{
    UWorld* W=GetWorld();auto* PC=W->GetFirstPlayerController();auto* Pawn=Cast<ACharacter>(PC->GetPawn());
    auto* Hall=AMiningExhibitHall::Find(W);auto* Save=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    auto Result=MakeShared<FJsonObject>();bool Ok=true;
    FString Op;C->TryGetStringField(TEXT("op"),Op);
    FString Name;C->TryGetStringField(TEXT("actor"),Name);
    AActor* Actor=nullptr;
    if(!Name.IsEmpty())for(TActorIterator<AActor> It(W);It;++It)if(It->GetPathName()==Name || It->GetName()==Name || It->ActorHasTag(FName(*Name))){Actor=*It;break;}
    auto Pose=[&](FVector Eye,FVector Target)
    {
        FVector OldEye;FRotator R;Pawn->GetActorEyesViewPoint(OldEye,R);
        const FVector P=Eye-(OldEye-Pawn->GetActorLocation());
        AGeneratedMiningRock::PrepareTerrainAt(W,P,1400);
        Pawn->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        PC->SetViewTarget(Pawn);PC->SetControlRotation((Target-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.01f);
    };
    if(Op==TEXT("start")){if(auto* Opening=First<AMiningOpening>(W))Opening->StartGame();else Ok=false;}
    else if(Op==TEXT("continue")){if(auto* Opening=First<AMiningOpening>(W))Opening->ContinueSavedGame();else Ok=false;}
    else if(Op==TEXT("menu_pointer"))
    {
        Ok=false;
        for(TObjectIterator<UMiningOpeningMenu> It;It;++It)
        {
            if(It->IsTemplate() || It->GetWorld()!=W || !It->IsInViewport())continue;
            const FString Action=C->GetStringField(TEXT("action")),Phase=C->GetStringField(TEXT("phase"));
            UButton* Button=Action==TEXT("continue")?It->ContinueButton.Get():(Action==TEXT("start")?It->StartButton.Get():(Action==TEXT("exit")?It->ExitButton.Get():nullptr));
            auto& Slate=FSlateApplication::Get();
            const FVector2D Point=Button?Button->GetCachedGeometry().LocalToAbsolute(Button->GetCachedGeometry().GetLocalSize()*.5f):It->GetCachedGeometry().LocalToAbsolute(FVector2D(20,20));
            const FVector2D Previous=Slate.GetCursorPos();Slate.SetCursorPos(Point);
            TSet<FKey> Held;if(Phase==TEXT("down"))Held.Add(EKeys::LeftMouseButton);
            const FPointerEvent Event(0,Point,Previous,Held,Phase==TEXT("move")?EKeys::Invalid:EKeys::LeftMouseButton,0,FModifierKeysState());
            // Route through Slate hit testing and UButton handlers, never set a
            // hover/pressed flag or broadcast a click to test pointer feedback.
            if(Phase==TEXT("move")){Slate.ProcessMouseMoveEvent(Event);Ok=true;}
            else if(Phase==TEXT("down"))
            {auto Window=Slate.FindWidgetWindow(It->TakeWidget());if(Window){Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Event);Ok=true;}}
            else if(Phase==TEXT("up")){Slate.ProcessMouseButtonUpEvent(Event);Ok=true;}
            break;
        }
    }
    else if(Op==TEXT("slate_pointer"))
    {
        auto& Slate=FSlateApplication::Get();auto View=W->GetGameViewport()->GetGameViewportWidget();
        auto Window=View?Slate.FindWidgetWindow(View.ToSharedRef()):nullptr;
        auto Button=Window?FindVisibleTag(Window.ToSharedRef(),FName(*C->GetStringField(TEXT("tag")))):nullptr;Ok=Button.IsValid();
        if(Button)
        {
            const FString Phase=C->GetStringField(TEXT("phase"));const auto& Geometry=Button->GetCachedGeometry();
            const FVector2D Point=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f),Previous=Slate.GetCursorPos();Slate.SetCursorPos(Point);
            TSet<FKey> Held;if(Phase==TEXT("down"))Held.Add(EKeys::LeftMouseButton);
            const FPointerEvent Event(0,Point,Previous,Held,Phase==TEXT("move")?EKeys::Invalid:EKeys::LeftMouseButton,0,FModifierKeysState());
            if(Phase==TEXT("move"))Slate.ProcessMouseMoveEvent(Event);
            else if(Phase==TEXT("down"))Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Event);
            else if(Phase==TEXT("up"))Slate.ProcessMouseButtonUpEvent(Event);
            else Ok=false;
        }
    }
    else if(Op==TEXT("slate_key"))
    {
        bool Ctrl=false;C->TryGetBoolField(TEXT("ctrl"),Ctrl);
        const FKeyEvent Event(FKey(*C->GetStringField(TEXT("key"))),FModifierKeysState(false,false,Ctrl,false,false,false,false,false,false),0,false,0,0);
        FSlateApplication::Get().ProcessKeyDownEvent(Event);FSlateApplication::Get().ProcessKeyUpEvent(Event);
    }
    else if(Op==TEXT("slate_focus") || Op==TEXT("slate_activate"))
    {
        auto& Slate=FSlateApplication::Get();auto View=W->GetGameViewport()->GetGameViewportWidget();
        auto Window=View?Slate.FindWidgetWindow(View.ToSharedRef()):nullptr;
        auto Button=Window?FindVisibleTag(Window.ToSharedRef(),FName(*C->GetStringField(TEXT("tag")))):nullptr;Ok=Button.IsValid();
        if(Button)
        {
            if(auto Scroll=FindVisibleTag(Window.ToSharedRef(),TEXT("GM_Scroll")))
                StaticCastSharedPtr<SScrollBox>(Scroll)->ScrollDescendantIntoView(Button,false,EDescendantScrollDestination::Center);
            Slate.SetKeyboardFocus(Button,EFocusCause::Navigation);
            if(Op==TEXT("slate_activate"))
            {
                const FKeyEvent Event(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
                Slate.ProcessKeyDownEvent(Event);Slate.ProcessKeyUpEvent(Event);
            }
        }
    }
    else if(Op==TEXT("menu_click"))
    {
        Ok=false;
        for(TObjectIterator<UMiningOpeningMenu> It;It;++It)
        {
            if(It->IsTemplate() || It->GetWorld()!=W || !It->IsInViewport())continue;
            const FString Action=C->GetStringField(TEXT("action"));
            UButton* Button=Action==TEXT("continue")?It->ContinueButton.Get():(Action==TEXT("start")?It->StartButton.Get():nullptr);
            if(Button && Button->IsVisible() && Button->GetIsEnabled()){Button->OnClicked.Broadcast();Ok=true;}
            break;
        }
    }
    else if(Op==TEXT("confirm_new")){if(auto* Opening=First<AMiningOpening>(W))Opening->ConfirmNewGame(C->GetBoolField(TEXT("keep")));else Ok=false;}
    else if(Op==TEXT("return_menu"))GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->ReturnToMainMenu();
    else if(Op==TEXT("pause"))Ok=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->TogglePauseMenu();
    else if(Op==TEXT("toggle_ui"))GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->ToggleAllUI();
    else if(Op==TEXT("gm_tool"))
    {auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();Ok=Session->ToggleDeveloperMenu();if(Ok)Ok=Session->DeveloperGrantTool(int32(C->GetNumberField(TEXT("tool"))));Session->CloseMenu();}
    else if(Op==TEXT("gm_mineral"))
    {auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();Ok=Session->ToggleDeveloperMenu();if(Ok)Ok=Session->DeveloperUnlockMineral(int32(C->GetNumberField(TEXT("item"))));Session->CloseMenu();}
    else if(Op==TEXT("gm_menu"))Ok=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->ToggleDeveloperMenu();
    else if(Op==TEXT("gm_jump"))
    {auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();Ok=Session->ToggleDeveloperMenu();if(Ok)Ok=Session->JumpToDeveloperArea(int32(C->GetNumberField(TEXT("area"))));}
    else if(Op==TEXT("gm_map")){auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();Ok=Session->ToggleDeveloperMenu();if(Ok)Ok=Session->OpenDeveloperExperience(int32(C->GetNumberField(TEXT("index"))));}
    else if(Op==TEXT("throw_bomb")){auto* Bomb=Pawn->FindComponentByClass<UMiningBombToolComponent>();Ok=Bomb && Bomb->ThrowBomb();}
    else if(Op==TEXT("save"))Ok=Save->SaveProgress();
    else if(Op==TEXT("load"))Ok=Save->LoadProgress();
    else if(Op==TEXT("area_fixture") && Hall)
    {
        // Traversal setup only, deliberately distinct from naturally completing a room.
        const int32 Area=int32(C->GetNumberField(TEXT("area")));Hall->UnlockedArea=FMath::Max(Hall->UnlockedArea,Area);Ok=Hall->EnterArea(Area,true);
    }
    else if(Op==TEXT("pose"))
    {
        FVector Target=Actor?Actor->GetActorLocation():ReadVec(*C,TEXT("target"));
        bool Bounds=false;if(Actor && C->TryGetBoolField(TEXT("bounds"),Bounds) && Bounds)Target=Actor->GetComponentsBoundingBox(true).GetCenter();
        Target+=ReadVec(*C,TEXT("target_offset"));
        Pose(C->HasField(TEXT("eye"))?ReadVec(*C,TEXT("eye")):Target+ReadVec(*C,TEXT("offset"),FVector(-150,0,40)),Target);
    }
    else if(Op==TEXT("key"))
    {
        const FKey Key(*C->GetStringField(TEXT("key")));bool Down=true;C->TryGetBoolField(TEXT("down"),Down);
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,Down?IE_Pressed:IE_Released,Down?1:0));
    }
    else if(Op==TEXT("aim"))
    {
        FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);PC->SetControlRotation((ReadVec(*C,TEXT("target"))-Eye).Rotation());
    }
    else if(Op==TEXT("film_fixture")){if(auto* Ending=First<AEndingEmergence>(W))Ending->PreviewFilm();else Ok=false;}
    else if(Op==TEXT("interact"))Ok=AMiningStartRoom::Find(W)->Interact(PC);
    else if(Op==TEXT("mineral_fixture"))Ok=Wallet->AddMineral(FName(*C->GetStringField(TEXT("mineral"))),int32(C->GetNumberField(TEXT("amount"))));
    else if(Op==TEXT("upgrade"))Ok=Pawn->FindComponentByClass<UMiningProgressionComponent>()->Purchase(0,Wallet->GetUpgradeLevel(0)+1);
    else if(Op==TEXT("trace"))
    {
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(PackagedQATrace),true,Pawn);
        bool Capsule=false;C->TryGetBoolField(TEXT("capsule"),Capsule);
        const bool HitSomething=Capsule
            ? W->SweepSingleByChannel(Hit,ReadVec(*C,TEXT("from")),ReadVec(*C,TEXT("to")),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Query)
            : W->LineTraceSingleByChannel(Hit,ReadVec(*C,TEXT("from")),ReadVec(*C,TEXT("to")),ECC_Visibility,Query);
        Result->SetBoolField(TEXT("hit"),HitSomething);
        if(HitSomething){Result->SetStringField(TEXT("hit_actor"),Hit.GetActor()?Hit.GetActor()->GetName():TEXT(""));Result->SetArrayField(TEXT("hit_position"),Vec(Hit.ImpactPoint));Result->SetBoolField(TEXT("diggable"),Cast<ADiggableRock>(Hit.GetActor())!=nullptr);}
    }
    else if(Op==TEXT("mine"))
    {
        auto* Rock=Cast<ADiggableRock>(Actor);Ok=Rock!=nullptr;
        if(Rock)Ok=Rock->MineSphere(C->HasField(TEXT("point"))?ReadVec(*C,TEXT("point")):Rock->GetComponentsBoundingBox(true).GetCenter(),C->GetNumberField(TEXT("radius")));
    }
    else if(Op==TEXT("collect") && Hall){Ok=Hall->Collection->TryCollectTarget();}
    else if(Op==TEXT("treasure") && Hall){Ok=Hall->Treasure->CollectTarget(PC);}
    else if(Op==TEXT("office")){if(auto* Office=First<APopupCloseStage>(W))Office->StartOfficeRound();else Ok=false;}
    else if(Op==TEXT("card"))
    {
        auto* Cards=PC->FindComponentByClass<UCardRoomInteractionComponent>();Ok=Cards!=nullptr;
        if(Cards)
        {
            const FString Action=C->GetStringField(TEXT("action"));
            if(Action==TEXT("interact"))Cards->Interact();
            else if(Action==TEXT("leave"))Cards->LeaveSeat();
            else if(Action==TEXT("confirm"))Cards->ConfirmCards();
            else if(Action==TEXT("select"))Cards->ToggleCardSelection(int32(C->GetNumberField(TEXT("index"))));
            else if(Action==TEXT("all"))Cards->SelectAllCards();
            else if(Action==TEXT("restart") && Cards->Stage)Cards->Stage->RestartEncounter();else Ok=false;
        }
    }
    else if(Op==TEXT("credits_fixture"))
    {if(auto* Ending=First<AEndingEmergence>(W))Ending->bComplete=true;else Ok=false;}
    else if(Op==TEXT("walking_fixture")){Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);}
    else if(Op==TEXT("catalogue_rotate"))
    {if(auto* Field=First<AMiningCollectionField>(W)){Field->SelectItem(C->HasField(TEXT("item"))?int32(C->GetNumberField(TEXT("item"))):0);Field->RotatePreview(C->GetNumberField(TEXT("yaw")),C->GetNumberField(TEXT("pitch")));}else Ok=false;}
    else if(Op==TEXT("catalogue_owned_fixture") && Hall && Hall->Collection)
    {
        // Isolated -MiningQA only: preview setup, never a player shortcut.
        const int32 Id=int32(C->GetNumberField(TEXT("item")));Ok=Id>=0&&Id<AMiningCollectible::ItemTypeCount;
        if(Ok){Hall->Collection->Inventory[Id]=1;Hall->Collection->SelectItem(Id);}
    }
    else if(Op==TEXT("catalogue_hover"))
    {
        auto* Field=First<AMiningCollectionField>(W);
        auto* B=Field&&Field->CatalogueWidget?Cast<UButton>(Field->CatalogueWidget->GetWidgetFromName(FName(*C->GetStringField(TEXT("name"))))):nullptr;
        Ok=B!=nullptr;if(B)
        {
            auto S=B->TakeWidget();const auto G=S->GetCachedGeometry();const FVector2D P=G.GetAbsolutePosition()+G.GetAbsoluteSize()*.5f;
            const FPointerEvent E(0,P,P,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
            if(C->GetBoolField(TEXT("hover")))S->OnMouseEnter(G,E);else S->OnMouseLeave(E);
        }
    }
    else if(Op==TEXT("catalogue_button"))
    {
        auto* Field=First<AMiningCollectionField>(W);
        auto* Button=Field && Field->CatalogueWidget?Cast<UButton>(Field->CatalogueWidget->GetWidgetFromName(FName(*C->GetStringField(TEXT("name"))))):nullptr;
        Ok=Button && Button->GetIsEnabled() && Button->IsVisible();
        if(Ok)
        {
            // Authored cards are mouse-only/non-focusable. Exercise the actual
            // SButton press/release handlers without moving the desktop cursor.
            auto SlateButton=Button->TakeWidget();
            const FKeyEvent Key(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
            Ok=SlateButton->OnKeyDown(SlateButton->GetCachedGeometry(),Key).IsEventHandled();
            Ok=SlateButton->OnKeyUp(SlateButton->GetCachedGeometry(),Key).IsEventHandled() && Ok;
        }
    }
    else if(Op==TEXT("reward_check_fixture") && Hall && Hall->Collection){Hall->Collection->CheckMineralRewards();}
    else if(Op==TEXT("pickup_replay_fixture") && Hall && Hall->Collection){Hall->Collection->QueuePickup(int32(C->GetNumberField(TEXT("item"))));}
    else if(Op==TEXT("pickup_debug") && Hall && Hall->Collection)
    {
        auto* Field=Hall->Collection.Get();
        if(Field->PickupTexture)
        {
            TArray<FLinearColor> Pixels;Field->PickupTexture->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels);
            int32 Visible=0;float Bright=0;for(const auto& P:Pixels){Visible+=P.A<.99f;Bright=FMath::Max(Bright,P.GetMax());}
            Result->SetNumberField(TEXT("pickup_visible_pixels"),Visible);Result->SetNumberField(TEXT("pickup_brightness"),Bright);
            Result->SetArrayField(TEXT("pickup_camera"),Vec(Field->PickupCapture->GetComponentLocation()));
            Result->SetBoolField(TEXT("pickup_preview_hidden"),Field->Preview->IsHidden());
        }
    }
    else if(Op==TEXT("legacy_collection_save_fixture"))
    {
        Ok=Save->SaveProgress();
        auto* Legacy=Ok?Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("BrainMiningProgress_v1"),0)):nullptr;
        Ok=Legacy!=nullptr;
        if(Legacy)
        {
            Legacy->Version=4;Legacy->CollectedMineralTotals.Reset();Legacy->Collection.SetNum(12);
            for(int32 Id=12;Id<16;++Id)Legacy->RunCollectedItems.Remove(Id);
            Ok=UGameplayStatics::SaveGameToSlot(Legacy,TEXT("BrainMiningProgress_v1"),0);
        }
    }
    else if(Op==TEXT("mineral_progress_fixture"))
    {const auto Mineral=EMiningMineral(int32(C->GetNumberField(TEXT("mineral"))));Ok=FMiningMineralCost::IsSupported(Mineral);if(Ok)Wallet->CollectedMineralTotals.FindOrAdd(Mineral)=FMath::Max(0,int32(C->GetNumberField(TEXT("amount"))));}
    else if(Op==TEXT("mineral_pickup_fixture") && Actor)
    {
        // Expose/reposition a real ore actor in this isolated QA profile. The
        // normal treasure command must still aim, award, count and hide it.
        auto* Ore=Cast<AMiningTreasure>(Actor);Ok=Ore && !Ore->bCollected;
        if(Ok){Ore->CoveringRocks.Reset();Ore->Exposure=1;Ore->bReleased=true;Ore->SetDropActive(false);Ore->SetActorLocation(ReadVec(*C,TEXT("point")));Ore->SetActorHiddenInGame(false);Ore->SetActorEnableCollision(true);}
    }
    else if(Op==TEXT("sample_start")){SampleLabel=C->GetStringField(TEXT("label"));FrameSamples.Reset();MaxCameraStep=0;bCameraSampleStarted=false;}
    else if(Op==TEXT("sample_end"))
    {
        TArray<TSharedPtr<FJsonValue>> Samples;for(double Value:FrameSamples)Samples.Add(MakeShared<FJsonValueNumber>(Value));
        Result->SetStringField(TEXT("label"),SampleLabel);Result->SetArrayField(TEXT("frame_ms"),Samples);SampleLabel.Empty();FrameSamples.Reset();
        Result->SetNumberField(TEXT("max_camera_step_cm"),MaxCameraStep);
    }
    else if(Op==TEXT("shot"))FScreenshotRequest::RequestScreenshot(Directory/(FPaths::MakeValidFileName(C->GetStringField(TEXT("name")))+TEXT(".png")),true,false);
    else if(Op==TEXT("quit"))FPlatformMisc::RequestExit(false);
    else if(Op!=TEXT("state") && Op!=TEXT("actors") && Op!=TEXT("neurons") && Op!=TEXT("slate_ui"))Ok=false;
    Result->SetStringField(TEXT("id"),LastId);Result->SetBoolField(TEXT("ok"),Ok);
    Result->SetNumberField(TEXT("autosaves"),Save->AutosavesCompleted);
    Result->SetBoolField(TEXT("save_in_flight"),Save->IsSaveInFlight());
    Result->SetNumberField(TEXT("snapshot_ms"),Save->LastSnapshotMilliseconds);
    Result->SetStringField(TEXT("map"),UGameplayStatics::GetCurrentLevelName(W));
    TArray<TSharedPtr<FJsonValue>> DebugMaps;
    for(const auto& Map:UMiningExperienceSubsystem::DeveloperMapPaths())DebugMaps.Add(MakeShared<FJsonValueString>(Map));
    Result->SetArrayField(TEXT("gm_maps"),DebugMaps);
    if(auto* Mining=Pawn->FindComponentByClass<UMiningComponent>()){Result->SetNumberField(TEXT("drill_capacity"),Mining->MaxDrillEnergy);Result->SetNumberField(TEXT("drill_energy"),Mining->DrillEnergy);Result->SetNumberField(TEXT("drill_recovery_per_second"),Mining->DrillRecoveryRate*.6f);}
    if(auto* Museum=First<AWenSiMirrorPuzzle>(W))
    {Result->SetBoolField(TEXT("aim_reset"),Museum->bAimReset);Result->SetNumberField(TEXT("reset_hold"),Museum->ResetHold.Progress());Result->SetNumberField(TEXT("reset_count"),Museum->ResetCount);}
    if(auto* Toilet=First<AHeavenToilet>(W))Result->SetBoolField(TEXT("toilet_outline"),Toilet->Body->bRenderCustomDepth);
    Result->SetNumberField(TEXT("area"),Hall?Hall->ActiveArea:-1);Result->SetNumberField(TEXT("unlocked"),Hall?Hall->UnlockedArea:-1);
    Result->SetBoolField(TEXT("opening"),AMiningOpening::IsOpening(W));
    if(auto* Camera=PC->PlayerCameraManager.Get())
    {
        Result->SetArrayField(TEXT("camera"),Vec(Camera->GetCameraLocation()));
        Result->SetNumberField(TEXT("camera_fade"),Camera->FadeAmount);
        Result->SetNumberField(TEXT("camera_fov"),Camera->GetFOVAngle());
    }
    if(auto* Viewport=W->GetGameViewport())
    {
        Result->SetBoolField(TEXT("world_render_disabled"),Viewport->bDisableWorldRendering);
        Result->SetBoolField(TEXT("rendering"),Viewport->EngineShowFlags.Rendering);
        Result->SetBoolField(TEXT("game_show"),Viewport->EngineShowFlags.Game);
    }
    Result->SetStringField(TEXT("view_target"),GetNameSafe(PC->GetViewTarget()));

    Result->SetBoolField(TEXT("collection_choice"),First<AMiningOpening>(W) && First<AMiningOpening>(W)->IsChoosingCollection());
    Result->SetBoolField(TEXT("ui_hidden"),GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->bAllUIHidden);
    Result->SetBoolField(TEXT("quit_confirmation"),GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->bQuitConfirmation);
    Result->SetBoolField(TEXT("menu_open"),GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->IsMenuOpen());
    Result->SetBoolField(TEXT("move_ignored"),PC->IsMoveInputIgnored());Result->SetBoolField(TEXT("look_ignored"),PC->IsLookInputIgnored());
    if(Op==TEXT("slate_ui"))
    {
        auto View=W->GetGameViewport()->GetGameViewportWidget();auto Window=View?FSlateApplication::Get().FindWidgetWindow(View.ToSharedRef()):nullptr;
        TArray<TSharedPtr<FJsonValue>> Tags;if(Window)ReadMenuTags(Window.ToSharedRef(),Tags);Result->SetArrayField(TEXT("slate_tags"),Tags);
    }
    if(auto* Mining=Pawn->FindComponentByClass<UMiningComponent>())Result->SetBoolField(TEXT("mining_suspended"),Mining->IsInputSuspended());
    TArray<UStaticMeshComponent*> ToolParts;Pawn->GetComponents(ToolParts);
    for(auto* Part:ToolParts)if(Part->GetName()==TEXT("PickaxeModel"))Result->SetBoolField(TEXT("pickaxe_visible"),Part->IsVisible());
    TArray<TSharedPtr<FJsonValue>> VisibleTools;
    for(auto* Part:ToolParts)if(Part->IsVisible() && (Part->GetName().Contains(TEXT("Drill")) || Part->GetName()==TEXT("PickaxeModel") || Part->GetName()==TEXT("BombModel")))VisibleTools.Add(MakeShared<FJsonValueString>(Part->GetName()));
    Result->SetArrayField(TEXT("visible_tools"),VisibleTools);
    TArray<TSharedPtr<FJsonValue>> MirrorParts;
    for(TActorIterator<AWenSiMirrorExhibit> It(W);It;++It)if(It->MirrorPresence)
        for(auto Part:It->PresenceParts)MirrorParts.Add(MakeShared<FJsonValueString>(Part->GetName()));
    Result->SetArrayField(TEXT("mirror_presence_parts"),MirrorParts);
    TArray<TSharedPtr<FJsonValue>> RunItems;for(int32 I=0;I<AMiningCollectible::ItemTypeCount;++I)if(Save->WasCollectedThisRun(I))RunItems.Add(MakeShared<FJsonValueNumber>(I));
    Result->SetArrayField(TEXT("run_items"),RunItems);
    Result->SetBoolField(TEXT("tutorial"),Save->TutorialActive);Result->SetNumberField(TEXT("completed"),Save->TutorialCompleted);
    Result->SetBoolField(TEXT("soil_passed"),Save->TutorialSoilPassed);
    Result->SetNumberField(TEXT("prototypes"),Save->PrototypesFound);Result->SetNumberField(TEXT("copper"),Wallet->GetMineralCount(EMiningMineral::Copper));
    Result->SetNumberField(TEXT("iron"),Wallet->GetMineralCount(EMiningMineral::Iron));Result->SetNumberField(TEXT("upgrade"),Wallet->GetUpgradeLevel(0));
    Result->SetBoolField(TEXT("has_save"),Save->HasSave());Result->SetArrayField(TEXT("position"),Vec(Pawn->GetActorLocation()));
    Result->SetNumberField(TEXT("collection"),Hall && Hall->Collection?Hall->Collection->UniqueCount():0);
    TArray<TSharedPtr<FJsonValue>> Totals,Owned,Conditions;
    for(auto M:{EMiningMineral::Copper,EMiningMineral::Iron,EMiningMineral::Malachite,EMiningMineral::Diamond})Totals.Add(MakeShared<FJsonValueNumber>(Wallet->CollectedMineralTotals.FindRef(M)));
    Result->SetArrayField(TEXT("mineral_totals"),Totals);
    if(Hall && Hall->Collection)
    {for(int32 V:Hall->Collection->Inventory)Owned.Add(MakeShared<FJsonValueNumber>(V));for(int32 Id=12;Id<16;++Id)Conditions.Add(MakeShared<FJsonValueString>(Hall->Collection->UnlockCondition(Id)));}
    Result->SetArrayField(TEXT("collection_counts"),Owned);Result->SetArrayField(TEXT("unlock_conditions"),Conditions);
    if(auto* Field=First<AMiningCollectionField>(W);Field && Field->CatalogueWidget)
    {
        Result->SetNumberField(TEXT("selected_item"),Field->SelectedId);
        auto* Widget=Field->CatalogueWidget.Get();
        if(auto* Count=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("CollectionCount"))))Result->SetStringField(TEXT("catalogue_count"),Count->GetText().ToString());
        if(auto* Text=Cast<URichTextBlock>(Widget->GetWidgetFromName(TEXT("ItemDescription"))))Result->SetStringField(TEXT("catalogue_description"),Text->GetText().ToString());
        if(auto* Preview=Widget->GetWidgetFromName(TEXT("PreviewImage")))Result->SetBoolField(TEXT("catalogue_preview_visible"),Preview->IsVisible());
        TArray<TSharedPtr<FJsonValue>> Labels,Buttons;
        for(const TCHAR* WidgetName:{TEXT("ItemName"),TEXT("ItemLabel0"),TEXT("ItemLabel1"),TEXT("ItemLabel2"),TEXT("ItemLabel3"),TEXT("ItemCondition0"),TEXT("ItemCondition1"),TEXT("ItemCondition2"),TEXT("ItemCondition3")})
            if(auto* T=Cast<UTextBlock>(Widget->GetWidgetFromName(WidgetName))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),WidgetName);J->SetStringField(TEXT("text"),T->GetText().ToString());Labels.Add(MakeShared<FJsonValueObject>(J));}
        for(const TCHAR* WidgetName:{TEXT("PreviousButton"),TEXT("NextButton"),TEXT("CloseButton"),TEXT("ItemButton0"),TEXT("ItemButton3")})
            if(auto* B=Cast<UButton>(Widget->GetWidgetFromName(WidgetName)))
            {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),WidgetName);const auto G=B->GetCachedGeometry();J->SetArrayField(TEXT("position"),Vec(FVector(G.GetAbsolutePosition(),0)));J->SetArrayField(TEXT("size"),Vec(FVector(G.GetAbsoluteSize(),0)));J->SetBoolField(TEXT("hovered"),B->IsHovered());J->SetNumberField(TEXT("scale"),B->GetRenderTransform().Scale.X);Buttons.Add(MakeShared<FJsonValueObject>(J));}
        Result->SetArrayField(TEXT("catalogue_labels"),Labels);Result->SetArrayField(TEXT("catalogue_buttons"),Buttons);
        if(Field->Preview)
        {
            TArray<TSharedPtr<FJsonValue>> Motion;Motion.Add(MakeShared<FJsonValueString>(Field->Preview->Model->GetRelativeTransform().ToString()));
            for(auto Part:Field->Preview->EmergenceParts)Motion.Add(MakeShared<FJsonValueString>(Part->GetRelativeTransform().ToString()));
            Result->SetArrayField(TEXT("catalogue_motion"),Motion);
        }
    }
    if(auto* MPC=Cast<ABrainMiningPlayerController>(PC);MPC && MPC->MiningGUI){Result->SetNumberField(TEXT("pickup_id"),MPC->MiningGUI->ActivePickupId);Result->SetNumberField(TEXT("pickup_age"),MPC->MiningGUI->PickupAge);Result->SetNumberField(TEXT("pickup_flights"),MPC->MiningGUI->CompletedPickupFlights);}
    TArray<TSharedPtr<FJsonValue>> Areas;
    for(int32 I=0;Hall && I<Hall->Areas.Num();++I)
    {
        auto A=MakeShared<FJsonObject>();A->SetArrayField(TEXT("entry"),Vec(Hall->Areas[I].Entry.GetLocation()));
        A->SetArrayField(TEXT("exit"),Vec(Hall->Areas[I].Exit));A->SetBoolField(TEXT("completed"),Hall->GetCompletedAreas().Contains(I));
        Areas.Add(MakeShared<FJsonValueObject>(A));
    }
    Result->SetArrayField(TEXT("areas"),Areas);
    if(auto* Room=AMiningStartRoom::Find(W)){Result->SetBoolField(TEXT("pickaxe"),Room->bPickaxeUnlocked);Result->SetBoolField(TEXT("jetpack"),Room->bJetpackUnlocked);}
    Result->SetBoolField(TEXT("bomb"),Wallet->bBombSkillUnlocked);Result->SetBoolField(TEXT("drill"),Wallet->bDrillSkillUnlocked);
    if(auto* Journey=Hall?Hall->Journey.Get():nullptr)
    {
        Result->SetNumberField(TEXT("journey_phase"),int32(Journey->Phase));Result->SetNumberField(TEXT("nodes"),Journey->ClearedNodes);
        Result->SetBoolField(TEXT("inside_computer"),Journey->bEnteredComputer);Result->SetBoolField(TEXT("tv_checkpoint"),Journey->bComputerCheckpoint);
        Result->SetArrayField(TEXT("tv_entry"),Vec(Journey->ComputerEntry));Result->SetArrayField(TEXT("tv_checkpoint_position"),Vec(Journey->ComputerCheckpointPosition));
        Result->SetArrayField(TEXT("journey_exit"),Vec(Journey->Exit));
        TArray<TSharedPtr<FJsonValue>> Rocks;for(const auto& Rock:Journey->ComputerRocks)if(Rock)Rocks.Add(MakeShared<FJsonValueString>(Rock->GetPathName()));
        Result->SetArrayField(TEXT("computer_rocks"),Rocks);
    }
    if(auto* Museum=First<AWenSiMirrorPuzzle>(W))
    {
        Result->SetStringField(TEXT("museum"),Museum->MirrorStatus());
        if(Museum->MousePortal)Result->SetArrayField(TEXT("painting_cell"),Vec(Museum->MousePortal->CellCenter()));
    }
    if(auto* Ending=First<AEndingEmergence>(W))
    {
        Result->SetNumberField(TEXT("ending_gate"),Ending->GateIndex);Result->SetNumberField(TEXT("ending_shot"),Ending->Shot);
        Result->SetNumberField(TEXT("neuron"),Ending->CurrentNeuron);Result->SetBoolField(TEXT("ending_complete"),Ending->bComplete);
        Result->SetStringField(TEXT("ending_hint"),Ending->Hint());
        Result->SetNumberField(TEXT("target_neuron"),Ending->TargetNeuron);
        Result->SetNumberField(TEXT("travelling_neuron"),Ending->TravellingToNeuron);
        if(Op==TEXT("neurons"))
        {
            TArray<USceneComponent*> SceneParts;Ending->GetRootComponent()->GetChildrenComponents(true,SceneParts);
            int32 VisibleBlood=0,VisibleCell=0;
            for(auto* Part:SceneParts)if(Part->IsVisible())
            {
                if(Part->ComponentHasTag(TEXT("EndingBloodVisual")))++VisibleBlood;
                if(Part->ComponentHasTag(TEXT("EndingCellVisual"))||Part->GetName().StartsWith(TEXT("EllipsoidalCellInterior"))||Part->GetName().StartsWith(TEXT("CellDress")))++VisibleCell;
            }
            Result->SetNumberField(TEXT("visible_blood_parts"),VisibleBlood);Result->SetNumberField(TEXT("visible_cell_parts"),VisibleCell);
            TArray<UStaticMeshComponent*> Meshes;Ending->GetComponents(Meshes);
            TArray<TSharedPtr<FJsonValue>> Neurons;
            for(int32 I=0;I<Ending->NeuronPositions.Num();++I)
            {
                auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("index"),I);
                const FVector Position=Ending->NeuronWorldPosition(I);Item->SetArrayField(TEXT("position"),Vec(Position));
                Item->SetBoolField(TEXT("reachable"),Ending->CanPossess(I));
                for(auto* Mesh:Meshes)
                    if((Mesh->GetName().StartsWith(TEXT("PossessableNeuron"))||Mesh->GetName().StartsWith(TEXT("LargestNeuron")))&&Mesh->GetComponentLocation().Equals(Position,1.f))
                        if(auto* Material=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
                        {const auto Tint=Material->K2_GetVectorParameterValue(TEXT("Tint"));Item->SetArrayField(TEXT("tint"),Vec(FVector(Tint.R,Tint.G,Tint.B)));break;}
                Neurons.Add(MakeShared<FJsonValueObject>(Item));
            }
            Result->SetArrayField(TEXT("neurons"),Neurons);
        }
    }
    if(auto* Sub=USubtitleSubsystem::Get(this))Result->SetStringField(TEXT("subtitle"),Sub->GetActiveText().ToString());
    TArray<TSharedPtr<FJsonValue>> MenuButtons;
    for(TObjectIterator<UMiningOpeningMenu> It;It;++It)
    {
        if(It->IsTemplate() || It->GetWorld()!=W || !It->IsInViewport())continue;
        for(UButton* Button:{It->ContinueButton.Get(),It->StartButton.Get(),It->ExitButton.Get()})
        {
            if(!Button)continue;
            auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("name"),Button->GetName());
            if(auto* Label=Cast<UTextBlock>(Button->GetContent()))Item->SetStringField(TEXT("text"),Label->GetText().ToString());
            Item->SetBoolField(TEXT("visible"),Button->IsVisible());Item->SetBoolField(TEXT("focused"),Button->HasKeyboardFocus());
            Item->SetBoolField(TEXT("hovered"),Button->IsHovered());Item->SetBoolField(TEXT("pressed"),Button->IsPressed());
            const auto Transform=Button->GetRenderTransform();
            Item->SetNumberField(TEXT("angle"),Transform.Angle);Item->SetNumberField(TEXT("shear"),Transform.Shear.X);Item->SetNumberField(TEXT("scale"),Transform.Scale.X);
            const FVector2D Position=Button->GetCachedGeometry().GetAbsolutePosition();
            Item->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(Position.X),MakeShared<FJsonValueNumber>(Position.Y)});
            MenuButtons.Add(MakeShared<FJsonValueObject>(Item));
        }
        break;
    }
    Result->SetArrayField(TEXT("menu_buttons"),MenuButtons);
    if(auto* Office=First<APopupCloseStage>(W)){Result->SetStringField(TEXT("office"),Office->GetSessionStatus());Result->SetStringField(TEXT("office_exit"),Office->GetExitStatus());}
    if(auto* Cards=First<ACardRoomStage>(W))
    {
        Result->SetNumberField(TEXT("card_phase"),int32(Cards->Run.Phase));Result->SetNumberField(TEXT("card_round"),Cards->Run.Round);
        TArray<UNiagaraComponent*> FX;Cards->GetComponents(FX);
        for(auto* Part:FX)if(Part->GetFName()==TEXT("P2Fx_PoisonHit"))
        {Result->SetBoolField(TEXT("poison_paused"),Part->IsPaused());Result->SetArrayField(TEXT("poison_scale"),Vec(Part->GetRelativeScale3D()));}
    }
    for(TObjectIterator<UMiningGUIHUDWidget> It;It;++It)
    {
        if(It->IsTemplate() || It->GetWorld()!=W)continue;
        if(auto* Shortcut=Cast<UTextBlock>(It->GetWidgetFromName(TEXT("CollectionShortcut"))))Result->SetStringField(TEXT("collection_shortcut"),Shortcut->GetText().ToString());
        if(auto* Toast=Cast<UTextBlock>(It->GetWidgetFromName(TEXT("TaskCompletionToast"))))
        {Result->SetStringField(TEXT("task_toast"),Toast->GetText().ToString());Result->SetBoolField(TEXT("task_toast_visible"),Toast->IsVisible());}
        if(auto* Panel=It->GetWidgetFromName(TEXT("GuidancePanel")))Result->SetNumberField(TEXT("guide_opacity"),Panel->GetRenderOpacity());
        if(auto* Panel=It->GetWidgetFromName(TEXT("PickaxeSummaryPanel")))Result->SetNumberField(TEXT("upgrade_opacity"),Panel->GetRenderOpacity());
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 I=0;I<5;++I)
            if(auto* Row=Cast<UTextBlock>(It->GetWidgetFromName(*FString::Printf(TEXT("GuidanceRow%d"),I))))
            {
                auto Value=MakeShared<FJsonObject>();Value->SetNumberField(TEXT("index"),I);
                Value->SetStringField(TEXT("text"),Row->GetText().ToString());Value->SetBoolField(TEXT("visible"),Row->IsVisible());
                Rows.Add(MakeShared<FJsonValueObject>(Value));
            }
        Result->SetArrayField(TEXT("guide_rows"),Rows);break;
    }
    if(Op==TEXT("actors"))
    {
        FString Filter;C->TryGetStringField(TEXT("filter"),Filter);TArray<TSharedPtr<FJsonValue>> Actors;
        for(TActorIterator<AActor> It(W);It;++It)
        {
            if(!Filter.IsEmpty() && !It->GetClass()->GetName().Contains(Filter) && !It->GetName().Contains(Filter) && !It->ActorHasTag(FName(*Filter)))continue;
            auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("name"),It->GetName());A->SetStringField(TEXT("path"),It->GetPathName());A->SetStringField(TEXT("class"),It->GetClass()->GetName());
            A->SetArrayField(TEXT("position"),Vec(It->GetActorLocation()));A->SetArrayField(TEXT("center"),Vec(It->GetComponentsBoundingBox(true).GetCenter()));
            A->SetArrayField(TEXT("extent"),Vec(It->GetComponentsBoundingBox(true).GetExtent()));A->SetArrayField(TEXT("forward"),Vec(It->GetActorForwardVector()));
            if(auto* Exhibit=Cast<AWenSiMirrorExhibit>(*It)){A->SetNumberField(TEXT("kind"),Exhibit->Kind);A->SetNumberField(TEXT("word"),int32(Exhibit->Word));}
            if(auto* Rock=Cast<ADiggableRock>(*It)){A->SetBoolField(TEXT("collision"),Rock->GetActorEnableCollision());A->SetNumberField(TEXT("cuts"),Rock->SuccessfulCuts);A->SetNumberField(TEXT("triangles"),Rock->TriangleCount);A->SetArrayField(TEXT("rock_size"),Vec(Rock->RockSize));}
            if(auto* Item=Cast<AMiningTreasure>(*It))
            {
                A->SetStringField(TEXT("mineral"),Item->TreasureId.ToString());A->SetBoolField(TEXT("released"),Item->bReleased);A->SetBoolField(TEXT("collected"),Item->bCollected);
                A->SetNumberField(TEXT("placement"),Item->SavePlacementIndex);
                TArray<TSharedPtr<FJsonValue>> Hosts;for(const auto& Host:Item->CoveringRocks)if(Host)Hosts.Add(MakeShared<FJsonValueString>(Host->GetPathName()));
                A->SetArrayField(TEXT("hosts"),Hosts);
            }
            if(auto* Item=Cast<AMiningCollectible>(*It)){A->SetNumberField(TEXT("item"),Item->ItemId);A->SetBoolField(TEXT("released"),Item->bReleased);A->SetBoolField(TEXT("collected"),Item->bCollected);A->SetStringField(TEXT("host"),Item->HostRock?Item->HostRock->GetName():TEXT(""));}
            Actors.Add(MakeShared<FJsonValueObject>(A));
        }
        Result->SetArrayField(TEXT("actors"),Actors);
    }
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("response.tmp")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    IFileManager::Get().Move(*(Directory/TEXT("response.json")),*(Directory/TEXT("response.tmp")),true);
}
