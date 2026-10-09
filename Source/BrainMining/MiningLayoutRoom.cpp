#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "MiningSpatialEditBridge.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"

AMiningLayoutRoom::AMiningLayoutRoom()
{
	PrimaryActorTick.bCanEverTick = false;
	RoomOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("FloorOrigin"));
	SetRootComponent(RoomOrigin);
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBounds"));
	Bounds->SetupAttachment(RoomOrigin);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetHiddenInGame(true);
	Bounds->ShapeColor = FColor(60, 220, 210);
	Bounds->SetLineThickness(4.0f);
	RoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));
	RoomLight->SetupAttachment(Bounds);
	RoomLight->SetMobility(EComponentMobility::Movable);
	RoomLight->SetIntensityUnits(ELightUnits::Candelas);
	RoomLight->SetIntensity(1300.0f);
	RoomLight->SetLightColor(FLinearColor(0.75f, 0.87f, 1.0f));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RoomLabel"));
	Label->SetupAttachment(Bounds);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(100.0f);
	Label->SetTextRenderColor(FColor(100, 240, 220));
	Label->SetHiddenInGame(true);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshGuides();
}

void AMiningLayoutRoom::RefreshGuides()
{
	const FVector SafeSize = RoomSize.ComponentMax(FVector(250));
	if (GetRootComponent() != RoomOrigin)
	{
		const FTransform PreviousTransform = GetActorTransform();
		SetRootComponent(RoomOrigin);
		RoomOrigin->SetWorldTransform(PreviousTransform);
		Bounds->AttachToComponent(RoomOrigin, FAttachmentTransformRules::KeepRelativeTransform);
	}
	Bounds->SetRelativeScale3D(FVector::OneVector);
	Bounds->SetRelativeRotation(FRotator::ZeroRotator);
	Bounds->SetRelativeLocation(bFloorAnchored ? FVector(0, 0, SafeSize.Z * 0.5) : FVector::ZeroVector);
	Bounds->SetBoxExtent(SafeSize * 0.5);
	RoomLight->SetRelativeLocation(FVector(0, 0, SafeSize.Z * 0.5 - 90));
	RoomLight->SetAttenuationRadius(SafeSize.GetMax() * GetActorScale3D().GetAbs().GetMax() * 1.5);
	Label->SetRelativeLocation(FVector(0, 0, SafeSize.Z * 0.5 + 75));
	Label->SetRelativeRotation(FRotator(0, 90, 0));
	Label->SetText(FText::FromString(FString::Printf(TEXT("%02d %s"), ConnectionOrder, bStartRoom ? TEXT("START") : TEXT("ROOM"))));
}

void AMiningLayoutRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshGuides();
}

FBox AMiningLayoutRoom::GetRoomBounds() const
{
	const FVector Extent = RoomSize.ComponentMax(FVector(250)) * GetActorScale3D().GetAbs() * 0.5;
	const FVector Center = GetActorLocation() + (bFloorAnchored ? FVector(0, 0, Extent.Z) : FVector::ZeroVector);
	return FBox(Center - Extent, Center + Extent);
}

void AMiningLayoutRoom::UpgradeFloorAnchor()
{
	if (bFloorAnchored)
	{
		return;
	}
	const FBox Previous = GetRoomBounds();
	TArray<AActor*> AttachedChildren;
	GetAttachedActors(AttachedChildren);
	TArray<FTransform> Transforms;
	for (AActor* Child : AttachedChildren) Transforms.Add(Child->GetActorTransform());
	Modify();
	RoomSize = Previous.GetSize();
	bFloorAnchored = true;
	SetActorScale3D(FVector::OneVector);
	SetActorLocation(FVector(Previous.GetCenter().X, Previous.GetCenter().Y, Previous.Min.Z));
	RefreshGuides();
	for (int32 Index = 0; Index < AttachedChildren.Num(); ++Index) AttachedChildren[Index]->SetActorTransform(Transforms[Index]);
}

void AMiningLayoutRoom::SetAsStartRoom()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningLayoutRoom> Room(GetWorld()); Room; ++Room)
	{
		if (Room->GetLevel() == GetLevel() && Room->LayoutId == LayoutId)
		{
			Room->Modify();
			Room->bStartRoom = *Room == this;
			Room->RefreshGuides();
		}
	}
	NotifyGenerators(false);
}

void AMiningLayoutRoom::NotifyGenerators(bool bImmediate)
{
	if (!GetWorld() || GetWorld()->IsGameWorld())
	{
		return;
	}
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Generator->GetLevel() == GetLevel() && Generator->LayoutId == LayoutId)
		{
			Generator->RequestRebuild();
			if (bImmediate && Generator->bAutoRebuild)
			{
				Generator->RebuildTunnels();
			}
		}
	}
}

#if WITH_EDITOR
void AMiningLayoutRoom::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (MiningSpatialEditing::Handle(this)) { RefreshGuides(); return; }
	RefreshGuides();
	NotifyGenerators(bFinished);
}

void AMiningLayoutRoom::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	if (MiningSpatialEditing::Handle(this)) { RefreshGuides(); return; }
	RefreshGuides();
	NotifyGenerators((Event.ChangeType & EPropertyChangeType::Interactive) == 0);
}

void AMiningLayoutRoom::PostEditUndo()
{
	Super::PostEditUndo();
	if (MiningSpatialEditing::Handle(this)) { RefreshGuides(); return; }
	RefreshGuides();
	NotifyGenerators(false);
}
#endif
