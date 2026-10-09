#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"
#include "MiningSpatialGeometry.h"

class FScopedTransaction;

struct FMiningSpatialSelection
{
	static TWeakObjectPtr<AMiningSpatialTunnel> Tunnel;
	static int32 Knot;
	static int32 Part;
	static bool Editable();
	static void Select(AMiningSpatialTunnel* Value, int32 Index = 0, int32 Handle = 0, FEditorViewportClient* Client = nullptr);
	static void Activate(FEditorViewportClient* Client = nullptr);
	static bool Insert();
	static void Delete();
};

class FMiningSpatialVisualizer : public FComponentVisualizer
{
public:
	virtual ~FMiningSpatialVisualizer() override;
	void Edit(AMiningSpatialTunnel* Source, int32 SelectedHandle);
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* Client, HComponentVisProxy* Proxy, const FViewportClick& Click) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* Client, FVector& Location) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* Client, FMatrix& Matrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* Client, FViewport* Viewport, FVector& Translate, FRotator& Rotate, FVector& Scale) override;
	virtual bool HandleInputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual void TrackingStarted(FEditorViewportClient* Client) override;
	virtual void TrackingStopped(FEditorViewportClient* Client, bool bDidMove) override;
	virtual UActorComponent* GetEditedComponent() const override;
	virtual void EndEditing() override;
private:
	TWeakObjectPtr<AMiningSpatialTunnel> Edited;
	int32 Handle = 0;
	TUniquePtr<FScopedTransaction> Transaction;
	mutable uint32 CachedHash = 0;
	mutable TWeakObjectPtr<AMiningSpatialTunnel> CachedSource;
	mutable TArray<MiningSpatial::FRing> CachedRings;
	bool Ring(MiningSpatial::FRing& Result) const;
	void Refresh(AMiningSpatialTunnel* Source) const;
};
