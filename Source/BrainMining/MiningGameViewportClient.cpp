#include "MiningGameViewportClient.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Widgets/SOverlay.h"
#include "Slate/SGameLayerManager.h"

void UMiningGameViewportClient::Draw(FViewport* InViewport,FCanvas* SceneCanvas)
{
    const auto* Session=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>():nullptr;
    // Keep GM accessible so recording mode can be restored through its button.
    const bool Hidden=Session && Session->bAllUIHidden && !Session->IsMenuOpen();
    if(auto Layers=GetGameLayerManager())Layers->AsWidget()->SetVisibility(Hidden?EVisibility::Hidden:EVisibility::SelfHitTestInvisible);
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    auto* HUD=PC?PC->GetHUD():nullptr;
    const bool OldHUD=HUD && HUD->bShowHUD;
    const bool OldWidgets=EngineShowFlags.WidgetComponents;
    if(Hidden){if(HUD)HUD->bShowHUD=false;EngineShowFlags.SetWidgetComponents(false);}
    Super::Draw(InViewport,SceneCanvas);
    if(HUD)HUD->bShowHUD=OldHUD;
    EngineShowFlags.SetWidgetComponents(OldWidgets);
}
