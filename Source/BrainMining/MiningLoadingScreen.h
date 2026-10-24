#pragma once
#include "CoreMinimal.h"

// MoviePlayer owns a separate Slate thread, so the dots keep animating while
// synchronous terrain deserialization and collision rebuilding occupy the game thread.
class FMiningLoadingScreen
{
public:
    explicit FMiningLoadingScreen(const TCHAR* Label=TEXT("加载中"));
    ~FMiningLoadingScreen();
    void ContinueThroughLevelTravel();
private:
    bool bPlaying=false;
};
