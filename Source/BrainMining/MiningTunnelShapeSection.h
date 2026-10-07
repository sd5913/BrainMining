#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningTunnelShapeSection.generated.h"

class AMiningTunnelRoute;
class UBoxComponent;
class UTextRenderComponent;

UCLASS(Blueprintable, PrioritizeCategories=("Tunnel Shape"))
class BRAINMINING_API AMiningTunnelShapeSection : public AActor
{
	GENERATED_BODY()
public:
	AMiningTunnelShapeSection();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tunnel Shape")
	TObjectPtr<AMiningTunnelRoute> Tunnel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tunnel Shape")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tunnel Shape", meta=(ClampMin="250", ClampMax="10000", Units="cm", ToolTip="Cross-section width. Move this actor to bend the tunnel. Endpoints remain attached to the complete room faces."))
	float Width = 1400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tunnel Shape", meta=(ClampMin="250", ClampMax="10000", Units="cm", ToolTip="Cross-section height about the actor center. No rotation; positive actor scale is supported."))
	float Height = 1200;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tunnel Shape")
	FString SectionStatus;

	UPROPERTY(VisibleAnywhere, Category="Tunnel Shape")
	TObjectPtr<UBoxComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category="Tunnel Shape")
	TObjectPtr<UTextRenderComponent> Label;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Tunnel Shape")
	void RebuildTunnel();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Tunnel Shape")
	void AddAnotherSection();

	UFUNCTION(CallInEditor, Category="Tunnel Shape")
	void SelectTunnel();

	uint32 GetGeometryHash() const;
	FVector2D GetWorldSize(int32 Wall) const;
	void RefreshGuides();
	void SetGuideVisibility(bool bVisible);
	bool IsInteractiveEdit() const { return bInteractiveEdit; }

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif

private:
	void NotifyGenerators();
	bool bInteractiveEdit = false;
};
