#pragma once

// MoviePlayer owns a separate Slate thread, so the dots keep animating while
// synchronous terrain deserialization and collision rebuilding occupy the game thread.
class FMiningLoadingScreen
{
public:
    FMiningLoadingScreen();
    ~FMiningLoadingScreen();
private:
    bool bPlaying=false;
};
