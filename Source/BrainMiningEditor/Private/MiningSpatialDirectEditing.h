#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "MiningSpatialGeometry.h"

class FCanvas;
class FSceneView;
class FPrimitiveDrawInterface;
class FEditorViewportClient;
class FViewport;
class HHitProxy;

namespace MiningSpatialDirectEditing
{
	enum class ETool { Path, Boundary, Move };
	enum class EHandle { None, Knot, InsertKnot, Profile, InsertProfile, SectionSide, RoomFace };
	struct FHandle
	{
		TWeakObjectPtr<AActor> Actor;
		EHandle Kind = EHandle::None;
		int32 Knot = 0;
		int32 Index = 0;
		bool operator==(const FHandle& Other) const { return Actor == Other.Actor && Kind == Other.Kind && Knot == Other.Knot && Index == Other.Index; }
	};
	struct FControl
	{
		FHandle Handle;
		FVector Position = FVector::ZeroVector;
		FString Label;
		bool bLocked = false;
	};
	bool IsEnabled();
	ETool Tool();
	void SetTool(ETool Value);
	bool& AutoSmooth();
	FString Hint();
	void Reset();
	void Controls(AActor* Actor, TArray<FControl>& Result);
	bool Frame(const AMiningSpatialTunnel& Tunnel, int32 Knot, MiningSpatial::FRing& Result);
	bool Location(const FHandle& Handle, FVector& Position);
	bool Translate(const FHandle& Handle, const FVector& Delta);
	bool Insert(FHandle& Handle);
	bool Delete(const FHandle& Handle);
	void Select(const FHandle& Handle);
	void Begin();
	void End(bool bCancel = false);
	bool HasSelection();
	FVector WidgetLocation();
	bool InputDelta(const FVector& Delta, const FRotator& Rotation, const FVector& Scale);
	bool InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event);
	bool MouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY);
	void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawHUD(const FSceneView* View, FCanvas* Canvas);
	bool FaceSection();
}
