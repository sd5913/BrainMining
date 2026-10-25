#include "MiningAudioComponent.h"
#include "MiningRoomEscape.h"
#include "Showcase/MiningExhibitHall.h"
#include "CardRoomStage.h"
#include "EchoPassageStage.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"

TArray<FName> UMiningAudioComponent::RoomMusicAssetNames()
{
	return {TEXT("M_Room_Office_Loop"), TEXT("M_Room_CardExplore_Loop"),
		TEXT("M_Room_CardBattle_Loop"), TEXT("M_Room_Escape_Loop"), TEXT("M_Room_Museum_Loop")};
}

FName UMiningAudioComponent::FindRoomMusic()
{
	if (!Hall.IsValid() || !Hall->bReady || !Hall->Areas.IsValidIndex(Hall->ActiveArea)) return NAME_None;
	const auto Kind = Hall->AreaKind(Hall->ActiveArea);
	if (Kind == EMiningExperienceKind::Office) return TEXT("M_Room_Office_Loop");
	if (Kind == EMiningExperienceKind::Museum) return TEXT("M_Room_Museum_Loop");
	if (Kind != EMiningExperienceKind::Journey) return NAME_None;
	const auto* Escape = Hall->Journey.Get();
	if (!Escape || !Escape->bAreaActive) return NAME_None;
	if (Escape->Phase == EMiningEscapePhase::Warning || Escape->Phase == EMiningEscapePhase::Flooding)
		return TEXT("M_Room_Escape_Loop");
	if (Escape->Phase != EMiningEscapePhase::Idle || !Escape->CardStage) return NAME_None;
	if (!RoomMusicEcho.IsValid())
		for (TActorIterator<AEchoPassageStage> Actor(GetWorld()); Actor; ++Actor)
		{
			RoomMusicEcho = *Actor;
			break;
		}
	if (RoomMusicEcho.IsValid() && !RoomMusicEcho->bCompleted) return NAME_None;
	switch (Escape->CardStage->Run.Phase)
	{
	case ECardRoomPhase::Exploring:
	case ECardRoomPhase::Seated:
	case ECardRoomPhase::Matching:
	case ECardRoomPhase::Arrival:
		return TEXT("M_Room_CardExplore_Loop");
	case ECardRoomPhase::Choosing:
	case ECardRoomPhase::Resolving:
		return TEXT("M_Room_CardBattle_Loop");
	default:
		return NAME_None;
	}
}

void UMiningAudioComponent::UpdateRoomMusic(float DeltaTime, float Gain)
{
	const FName Selected = FindRoomMusic();
	if (!Selected.IsNone() && Gain > 0 && !Sounds.Contains(Selected))
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/RoomMusic/%s.%s"), *Selected.ToString(), *Selected.ToString());
		Sounds.Add(Selected, LoadObject<USoundBase>(nullptr, *Path));
		if (!Sounds.FindRef(Selected)) UE_LOG(LogTemp, Warning, TEXT("Room music asset missing: %s"), *Path);
	}
	for (const FName Name : RoomMusicAssetNames())
	{
		const bool bAvailable = Sounds.FindRef(Name) != nullptr;
		const float Balance = Name == TEXT("M_Room_Office_Loop") ? 2.4f :
			Name == TEXT("M_Room_CardExplore_Loop") ? 1.8f :
			Name == TEXT("M_Room_Museum_Loop") ? 2.5f : 1.f;
		SetSoundscapeLayer(Name, Name == Selected && bAvailable ? Gain * Balance : 0.f, DeltaTime);
	}
}
