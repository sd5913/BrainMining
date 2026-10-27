#include "MiningMineralRewards.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningCollection.h"
#include "MiningWalletSubsystem.h"
#include "MiningSaveSubsystem.h"
#include "MiningExperienceSubsystem.h"
#include "MiningGUIWidgets.h"
#include "BrainMiningPlayerController.h"
#include "MiningAudioComponent.h"
#include "MiningHapticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HeavenToilet.h"
#include "Subtitles/SubtitleLibrary.h"

int32 AMiningCollectible::UnlockThreshold(int32 Id)
{
    constexpr int32 Counts[]={25,15,10,5};
    return IsMineralReward(Id)?Counts[Id-12]:0;
}

FString AMiningCollectionField::UnlockCondition(int32 Id) const
{
    if(!AMiningCollectible::IsMineralReward(Id))return MiningText::Get(TEXT("UI.Collection.Locked")).ToString();
    const auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    const int32 Count=Wallet->CollectedMineralTotals.FindRef(MineralRewardType(Id));
    return FText::Format(MiningText::Get(TEXT("UI.Collection.MineralCondition")),FFormatNamedArguments{
        {TEXT("Target"),AMiningCollectible::UnlockThreshold(Id)},{TEXT("Mineral"),FMiningMineralCost::NameText(MineralRewardType(Id))},{TEXT("Count"),Count}}).ToString();
}

void AMiningCollectionField::CheckMineralRewards(bool bAnimate)
{
    if(bCatalogueOnly || !GetGameInstance())return;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    for(int32 Id=12;Id<16;++Id)
        if(Wallet->CollectedMineralTotals.FindRef(MineralRewardType(Id))>=AMiningCollectible::UnlockThreshold(Id))
            GrantMineralReward(Id,bAnimate);
    if(bCatalogueOpen && CatalogueWidget)SelectItem(SelectedId);
}

bool AMiningCollectionField::GrantMineralReward(int32 Id, bool bAnimate)
{
    if(bCatalogueOnly || !GetGameInstance() || !AMiningCollectible::IsMineralReward(Id))return false;
    // Append missing slots only; never clear or truncate existing unlocks.
    if(Inventory.Num()<AMiningCollectible::ItemTypeCount)Inventory.AddZeroed(AMiningCollectible::ItemTypeCount-Inventory.Num());
    if(Inventory[Id]>0)return false;
    Inventory[Id]=1;
    GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection=Inventory;
    GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->RecordCollectible(Id);
    if(bAnimate)
    {
        QueuePickup(Id);
        USubtitleLibrary::PlaySubtitleSegment(this,FName(*FString::Printf(TEXT("收集物拾取_%02d"),Id)));
    }
    if(bCatalogueOpen && CatalogueWidget)SelectItem(SelectedId);
    return true;
}

void AMiningCollectionField::QueuePickup(int32 Id)
{
    if(auto* PC=Cast<ABrainMiningPlayerController>(GetWorld()->GetFirstPlayerController());PC && PC->MiningGUI)
        PC->MiningGUI->QueueCollectionPickup(this,Id);
}

void AMiningCollectionField::BeginPickupPreview(int32 Id)
{
    if(!Preview)return;
    PickupPreviewId=Id;SelectItem(Id);
    if(!PickupCapture)
    {
        PickupTexture=NewObject<UTextureRenderTarget2D>(this);PickupTexture->ClearColor=FLinearColor(0,0,0,1);
        PickupTexture->InitAutoFormat(512,384);
        PickupCapture=NewObject<USceneCaptureComponent2D>(this);AddInstanceComponent(PickupCapture);PickupCapture->RegisterComponent();
        PickupCapture->bCaptureEveryFrame=false;PickupCapture->bCaptureOnMovement=false;
        PickupCapture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        PickupCapture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
        PickupCapture->TextureTarget=PickupTexture;PickupCapture->FOVAngle=Capture->FOVAngle;
        PickupCapture->ShowFlags=Capture->ShowFlags;PickupCapture->PostProcessSettings=Capture->PostProcessSettings;
        PickupCapture->ShowOnlyActors=Capture->ShowOnlyActors;
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/UI/Redesign/M_CollectionPreview.M_CollectionPreview")))
        {PickupMaterial=UMaterialInstanceDynamic::Create(Material,this);PickupMaterial->SetTextureParameterValue(TEXT("Preview"),PickupTexture);}
    }
    PickupCapture->SetWorldTransform(Capture->GetComponentTransform());
    UpdatePickupPreview(0);
    if(AMiningCollectible::IsMineralReward(Id))
    {
        if(Player && Player->GetPawn())if(auto* Audio=Player->GetPawn()->FindComponentByClass<UMiningAudioComponent>())Audio->PlayCollectible(Id);
        UMiningHapticsSubsystem::Emit(Player,EMiningHapticCue::Collectible);
    }
}

void AMiningCollectionField::UpdatePickupPreview(float Time)
{
    if(!Preview || !PickupCapture || bCatalogueOpen)return;
    if(Preview->ItemId!=PickupPreviewId)SelectItem(PickupPreviewId);
    Preview->SetActorHiddenInGame(PickupPreviewId==0 && PreviewToilet);
    if(PreviewToilet)PreviewToilet->SetActorHiddenInGame(PickupPreviewId!=0);
    // Only capture during the short reveal. Flying uses the frozen texture.
    Preview->AnimateEmergence(Time+2.f);
    PickupCapture->CaptureScene();
}

void AMiningCollectionField::EndPickupPreview()
{
    PickupPreviewId=INDEX_NONE;
    if(!bCatalogueOpen)
    {
        if(Preview)Preview->SetActorHiddenInGame(true);
        if(PreviewToilet)PreviewToilet->SetActorHiddenInGame(true);
    }
}
