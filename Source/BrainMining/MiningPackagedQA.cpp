#include "MiningPackagedQA.h"
#include "MiningSaveSubsystem.h"
#include "MiningGUIWidgets.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
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
#include "Subtitles/SubtitleSubsystem.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
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
    else if(Op==TEXT("upgrade"))Ok=Pawn->FindComponentByClass<UMiningProgressionComponent>()->Purchase(0,1);
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
    else if(Op==TEXT("sample_start")){SampleLabel=C->GetStringField(TEXT("label"));FrameSamples.Reset();}
    else if(Op==TEXT("sample_end"))
    {
        TArray<TSharedPtr<FJsonValue>> Samples;for(double Value:FrameSamples)Samples.Add(MakeShared<FJsonValueNumber>(Value));
        Result->SetStringField(TEXT("label"),SampleLabel);Result->SetArrayField(TEXT("frame_ms"),Samples);SampleLabel.Empty();FrameSamples.Reset();
    }
    else if(Op==TEXT("shot"))FScreenshotRequest::RequestScreenshot(Directory/(FPaths::MakeValidFileName(C->GetStringField(TEXT("name")))+TEXT(".png")),true,false);
    else if(Op==TEXT("quit"))FPlatformMisc::RequestExit(false);
    else if(Op!=TEXT("state") && Op!=TEXT("actors"))Ok=false;
    Result->SetStringField(TEXT("id"),LastId);Result->SetBoolField(TEXT("ok"),Ok);
    Result->SetNumberField(TEXT("area"),Hall?Hall->ActiveArea:-1);Result->SetNumberField(TEXT("unlocked"),Hall?Hall->UnlockedArea:-1);
    Result->SetBoolField(TEXT("opening"),AMiningOpening::IsOpening(W));
    Result->SetBoolField(TEXT("tutorial"),Save->TutorialActive);Result->SetNumberField(TEXT("completed"),Save->TutorialCompleted);
    Result->SetBoolField(TEXT("soil_passed"),Save->TutorialSoilPassed);
    Result->SetNumberField(TEXT("prototypes"),Save->PrototypesFound);Result->SetNumberField(TEXT("copper"),Wallet->GetMineralCount(EMiningMineral::Copper));
    Result->SetNumberField(TEXT("iron"),Wallet->GetMineralCount(EMiningMineral::Iron));Result->SetNumberField(TEXT("upgrade"),Wallet->GetUpgradeLevel(0));
    Result->SetBoolField(TEXT("has_save"),Save->HasSave());Result->SetArrayField(TEXT("position"),Vec(Pawn->GetActorLocation()));
    Result->SetNumberField(TEXT("collection"),Hall && Hall->Collection?Hall->Collection->UniqueCount():0);
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
    if(auto* Cards=First<ACardRoomStage>(W)){Result->SetNumberField(TEXT("card_phase"),int32(Cards->Run.Phase));Result->SetNumberField(TEXT("card_round"),Cards->Run.Round);}
    for(TObjectIterator<UMiningGUIHUDWidget> It;It;++It)
    {
        if(It->IsTemplate() || It->GetWorld()!=W)continue;
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
            if(auto* Rock=Cast<ADiggableRock>(*It)){A->SetNumberField(TEXT("cuts"),Rock->SuccessfulCuts);A->SetNumberField(TEXT("triangles"),Rock->TriangleCount);A->SetArrayField(TEXT("rock_size"),Vec(Rock->RockSize));}
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
