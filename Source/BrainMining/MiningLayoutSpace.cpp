#include "MiningLayoutSpace.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "MiningSpaceGeometry.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

AMiningLayoutSpace::AMiningLayoutSpace()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SpaceFloorOrigin")));
	ContentRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ContentRoot"));
	ContentRoot->SetupAttachment(RootComponent);
	ContentRoot->SetAbsolute(false, false, true);
	Preview = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("SpacePreview"));
	Preview->SetupAttachment(RootComponent);
	Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Preview->SetGenerateOverlapEvents(false);
	Preview->SetCanEverAffectNavigation(false);
	Preview->SetCastShadow(false);
	Preview->SetHiddenInGame(true);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SpaceLabel"));
	Label->SetupAttachment(RootComponent);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHiddenInGame(true);
	Label->SetWorldSize(60);
	Label->SetHorizontalAlignment(EHTA_Center);
}

FBox AMiningLayoutSpace::GetSpaceBounds() const
{
	const FVector Size = SpaceSize * GetActorScale3D();
	const FVector Center = GetActorLocation() + FVector(0, 0, Size.Z * 0.5);
	return FBox(Center - Size * 0.5, Center + Size * 0.5);
}

uint32 AMiningLayoutSpace::GetGeometryHash() const
{
	uint32 Hash = GetTypeHash(GetPathName());
	Hash = HashCombineFast(Hash, GetTypeHash(bEnabled));
	Hash = HashCombineFast(Hash, GetTypeHash(FromRoom.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(LayoutId));
	Hash = HashCombineFast(Hash, GetTypeHash(GetActorTransform().ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(SpaceSize));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Shape)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Contents)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Connection)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Entrance)));
	Hash = HashCombineFast(Hash, GetTypeHash(BranchWidth));
	Hash = HashCombineFast(Hash, GetTypeHash(BranchHeight));
	Hash = HashCombineFast(Hash, GetTypeHash(bAlignBranchFloors));
	return HashCombineFast(Hash, GetTypeHash(EntranceRockThickness));
}

void AMiningLayoutSpace::RefreshGuides()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || SpaceSize.ContainsNaN()) return;
	const FVector Size = SpaceSize.ComponentMax(FVector(1));
	Preview->SetMesh(MiningSpaces::ShapeMesh(Shape, FBox(FVector(-Size.X * 0.5, -Size.Y * 0.5, 0), FVector(Size.X * 0.5, Size.Y * 0.5, Size.Z))));
	if (!PreviewMaterial) PreviewMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MiningPrototype/Materials/SpaceEditing/M_SpacePreview.M_SpacePreview"), nullptr, LOAD_NoWarn);
	const FLinearColor Color = !bEnabled ? FLinearColor(0.35, 0.35, 0.35) : Contents == EMiningSpaceContents::DiggableRock ? FLinearColor(1, 0.5, 0.08) : FLinearColor(0.12, 0.7, 1);
	if (PreviewMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Preview->SetMaterial(0, Material);
	}
	Label->SetRelativeLocation(FVector(0, 0, Size.Z + 90));
	Label->SetRelativeRotation(FRotator(0, 90, 0));
	Label->SetTextRenderColor(Color.ToFColor(true));
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s | %s"), bEnabled ? TEXT("") : TEXT("OFF "), *SpaceName, Contents == EMiningSpaceContents::Empty ? TEXT("EMPTY") : TEXT("DIGGABLE"))));
#endif
}

void AMiningLayoutSpace::SetGuideVisibility(bool bVisible)
{
	Preview->SetVisibility(bVisible);
	Label->SetVisibility(bVisible);
}

void AMiningLayoutSpace::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshGuides();
}

void AMiningLayoutSpace::NotifyGenerators()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Generator->GetLevel() == GetLevel()) Generator->RequestRebuild();
	}
}

void AMiningLayoutSpace::RebuildLayout()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Generator->GetLevel() == GetLevel() && Generator->LayoutId == LayoutId) Generator->RebuildTunnels();
	}
}

void AMiningLayoutSpace::AssignNearestTunnel()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	TArray<AMiningLayoutRoom*> Rooms;
	for (TActorIterator<AMiningLayoutRoom> Room(GetWorld()); Room; ++Room)
	{
		if (Room->GetLevel() == GetLevel() && Room->LayoutId == LayoutId) Rooms.Add(*Room);
	}
	Rooms.Sort([](const AMiningLayoutRoom& First, const AMiningLayoutRoom& Second)
	{
		if (First.bStartRoom != Second.bStartRoom) return First.bStartRoom;
		return First.ConnectionOrder < Second.ConnectionOrder;
	});
	double Nearest = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index + 1 < Rooms.Num(); ++Index)
	{
		const FVector Closest = FMath::ClosestPointOnSegment(GetSpaceBounds().GetCenter(), Rooms[Index]->GetRoomBounds().GetCenter(), Rooms[Index + 1]->GetRoomBounds().GetCenter());
		const double Distance = FVector::DistSquared(Closest, GetSpaceBounds().GetCenter());
		if (Distance < Nearest)
		{
			Modify();
			FromRoom = Rooms[Index];
			Nearest = Distance;
		}
	}
	NotifyGenerators();
}

#if WITH_EDITOR
void AMiningLayoutSpace::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	bInteractiveEdit = !bFinished;
	RefreshGuides();
	NotifyGenerators();
}

void AMiningLayoutSpace::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	bInteractiveEdit = (Event.ChangeType & EPropertyChangeType::Interactive) != 0;
	RefreshGuides();
	if (Event.GetPropertyName() != GET_MEMBER_NAME_CHECKED(AMiningLayoutSpace, SpaceName) && Event.GetPropertyName() != GET_MEMBER_NAME_CHECKED(AMiningLayoutSpace, PreviewMaterial)) NotifyGenerators();
}

void AMiningLayoutSpace::PostEditUndo()
{
	Super::PostEditUndo();
	bInteractiveEdit = false;
	RefreshGuides();
	NotifyGenerators();
}
#endif
