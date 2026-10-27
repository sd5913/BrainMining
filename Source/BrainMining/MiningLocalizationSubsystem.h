#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Components/Widget.h"
#include "MiningLocalizationSubsystem.generated.h"

class SWidget;

namespace MiningText
{
    BRAINMINING_API FText Get(const TCHAR* Key);
    BRAINMINING_API void RegisterTables();
}

DECLARE_MULTICAST_DELEGATE_OneParam(FMiningLanguageChanged, const FString&);

UCLASS()
class BRAINMINING_API UMiningLocalizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static FString ResolveCulture(const FString& Language);
    static bool IsSupportedPreference(const FString& Preference);
    UFUNCTION(BlueprintPure) FString GetLanguagePreference() const { return LanguagePreference; }
    UFUNCTION(BlueprintPure) FString GetLanguage() const;
    UFUNCTION(BlueprintCallable) bool SetLanguagePreference(const FString& Preference);
    TSharedRef<SWidget> MakeLanguageSelector();
    FMiningLanguageChanged OnLanguageChanged;
private:
    FString LanguagePreference = TEXT("auto");
    FString AutomaticCulture;
    bool bOwnsEditorPreview = false;
    bool ApplyLanguage();
};

UCLASS()
class BRAINMINING_API UMiningLanguageWidget : public UWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
