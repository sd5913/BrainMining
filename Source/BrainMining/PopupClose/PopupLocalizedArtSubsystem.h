#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PopupLocalizedArtSubsystem.generated.h"

class UMaterialInterface;
class UTexture2D;
class UTextureRenderTarget2D;
class FWidgetRenderer;

UCLASS()
class BRAINMINING_API UPopupLocalizedArtSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UMaterialInterface* GetArtwork(int32 Variant);
private:
    friend class FMiningPopupArtworkTest;
    void RefreshLanguage(const FString& Culture);
    void RenderArtwork(int32 Variant);
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UMaterialInterface>> Materials;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTexture2D>> Backgrounds;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTextureRenderTarget2D>> Targets;
    FWidgetRenderer* Renderer = nullptr;
};
