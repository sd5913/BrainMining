#pragma once
#include "Engine/GameViewportClient.h"
#include "MiningGameViewportClient.generated.h"

UCLASS()
class BRAINMINING_API UMiningGameViewportClient : public UGameViewportClient
{
    GENERATED_BODY()
public:
    virtual void Draw(FViewport* InViewport,FCanvas* SceneCanvas) override;
};
