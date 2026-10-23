#include "MiningCollection.h"
#include "MiningMineralRewards.h"
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

int32 AMiningCollectible::UnlockThreshold(int32 Id)
{
    constexpr int32 Counts[]={25,15,10,5};
    return IsMineralReward(Id)?Counts[Id-12]:0;
}

FString AMiningCollectionField::UnlockCondition(int32 Id) const
{
    if(!AMiningCollectible::IsMineralReward(Id))return FString();
    const auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    const int32 Count=Wallet->CollectedMineralTotals.FindRef(MineralRewardType(Id));
    return FString::Printf(TEXT("解锁条件：累计收集%d个%s\n当前进度：%d / %d\n\n本轮实际收集的矿石均会计入，升级消耗不扣进度。达成条件后自动收入藏品。"),
        AMiningCollectible::UnlockThreshold(Id),*FMiningMineralCost::Name(MineralRewardType(Id)),Count,AMiningCollectible::UnlockThreshold(Id));
}

void AMiningCollectionField::CheckMineralRewards(bool bAnimate)
{
    if(bCatalogueOnly || !GetGameInstance())return;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Inventory.SetNumZeroed(AMiningCollectible::ItemTypeCount);
    for(int32 Id=12;Id<16;++Id)
        if(Inventory[Id]==0 && Wallet->CollectedMineralTotals.FindRef(MineralRewardType(Id))>=AMiningCollectible::UnlockThreshold(Id))
        {
            Inventory[Id]=1;
            GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection=Inventory;
            GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->RecordCollectible(Id);
            if(bAnimate)QueuePickup(Id);
        }
    if(bCatalogueOpen && CatalogueWidget)SelectItem(SelectedId);
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
