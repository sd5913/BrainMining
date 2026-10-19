#if WITH_EDITOR
#include "CardRoomStage.h"
#include "CardRoomPlayer.h"
#include "MiningEncounterPlayer.h"
#include "MiningRoomEscape.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "NiagaraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"

// Review the actual four-hand sequence at normal game speed, leaving time for
// Niagara simulation and recording rendered frames instead of skipping ticks.
static FAutoConsoleCommand ReviewP2(TEXT("BrainMining.ReviewP2"),TEXT("Play P2's four real hands and capture spell frames."),FConsoleCommandDelegate::CreateLambda([]
{
    UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W)return;
    auto* PC=Cast<AMiningEncounterPlayerController>(W->GetFirstPlayerController());if(!PC || !PC->GetPawn())return;
    ACardRoomStage* S=nullptr;for(TActorIterator<ACardRoomStage> It(W);It;++It){S=*It;break;}if(!S)return;
    PC->GetPawn()->SetActorLocation(S->GetApproachLocation()+FVector(0,0,100));
    PC->CardInteraction->SetRoomActive(true);PC->CardInteraction->Interact();S->Run.Match();
    // Only the review slows time, so short spell phases receive several real
    // rendered frames even on this machine. Shipping game timing is unchanged.
    UGameplayStatics::SetGlobalTimeDilation(W,.5f);
    const TWeakObjectPtr<ACardRoomStage> Weak(S);const double Began=FPlatformTime::Seconds();
    struct FReview{TSet<FString> Shots;FString Report;bool Collected=false;};auto State=MakeShared<FReview>();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Began,State](float)
    {
        if(!Weak.IsValid())return false;
        if(FPlatformTime::Seconds()-Began>120){UGameplayStatics::SetGlobalTimeDilation(Weak->GetWorld(),1);return false;}
        ACardRoomStage* Stage=Weak.Get();auto& Run=Stage->Run;
        if(Run.Phase==ECardRoomPhase::Choosing)
        {
            for(int32 I=0;I<Run.Hand.Num();++I)Run.Stage(I);
            Run.Confirm();
        }
        if(Run.Phase==ECardRoomPhase::Resolving)
        {
            const int32 Phase=Run.PhaseTime>=2.f?4:Run.PhaseTime>=1.35f?3:Run.PhaseTime>=.8f?2:Run.PhaseTime>=.4f?1:0;
            const FString Name=FString::Printf(TEXT("round%d_%d"),Run.Round,Phase);
            if(Phase>0 && !State->Shots.Contains(Name))
            {
                State->Shots.Add(Name);
                if(GEngine && GEngine->GameViewport && GEngine->GameViewport->GetGameViewportWidget())
                {
                    TArray<FColor> Pixels;FIntVector Size;
                    if(FSlateApplication::Get().TakeScreenshot(GEngine->GameViewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
                    {
                        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
                        FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Validation/IntroP2Office")/(Name+TEXT(".png"))));
                    }
                }
                TArray<UNiagaraComponent*> Effects;Stage->GetComponents(Effects);int32 Active=0;
                for(auto* FX:Effects)if(FX->IsActive() && FX->GetAsset())++Active;
                State->Report+=FString::Printf(TEXT("%s,time=%.3f,active_niagara=%d,player=%d,rival=%d\n"),*Name,Run.PhaseTime,Active,Run.PlayerHP,Run.RivalHP);
            }
        }
        if(Run.Phase==ECardRoomPhase::Victory && !State->Collected)
        {
            bool Auto=false;
            for(TActorIterator<AMiningRoomEscape> It(Stage->GetWorld());It;++It)Auto|=It->bAutoClaimVictoryReward;
            // Leave time to inspect the physical drill and exercise the normal E pickup.
            if(!Auto && Run.PhaseTime>4.f)Stage->CollectReward();
        }
        if(Run.Phase==ECardRoomPhase::Collected && !State->Collected)
        {State->Report+=TEXT("victory_reward_collected\n");State->Collected=true;}
        if(State->Collected)for(TActorIterator<AMiningRoomEscape> It(Stage->GetWorld());It;++It)
        {
            if(It->Phase==EMiningEscapePhase::Flooding)
            {
                State->Report+=FString::Printf(TEXT("flood_started,water=%d,tv=%d,sections=%d\n"),It->Water!=nullptr,It->TelevisionScreen!=nullptr,It->ComputerRocks.Num());
                FFileHelper::SaveStringToFile(State->Report,*(FPaths::ProjectSavedDir()/TEXT("Validation/IntroP2Office/presentation.csv")));
                UGameplayStatics::SetGlobalTimeDilation(Stage->GetWorld(),1);
                return false;
            }
        }
        return true;
    }),.03f);
}));
#endif
