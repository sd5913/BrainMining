#include "MiningRoomEscape.h"
#include "MiningEncounterPlayer.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"

void AMiningRoomEscape::TickFloatingProps(float DeltaSeconds)
{
    if(Phase!=EMiningEscapePhase::Flooding || bEnteredComputer || !Player.IsValid())return;
    auto* Character=Cast<ACharacter>(Player->GetPawn());if(!Character)return;
    // Movement base identifies actual standing contact, not proximity while flying past a book.
    const auto* Base=Character->GetMovementBase();
    for(int32 Part=0;Part<FloatingProps.Num();++Part)
        if(Base && FloatingProps[Part]==Base && FloatingPadIndices.IsValidIndex(Part))
        {
            const int32 Pad=FloatingPadIndices[Part];
            if(FloatingSinkElapsed[Pad]<0)FloatingSinkElapsed[Pad]=0;
            break;
        }
    for(float& Age:FloatingSinkElapsed)if(Age>=0)Age+=DeltaSeconds;
}
