#include "MiningTunnelShapeSection.h"
#include "MiningTunnelRoute.h"
#include "MiningTunnelGenerator.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

AMiningTunnelShapeSection::AMiningTunnelShapeSection()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;
	Frame = CreateDefaultSubobject<UBoxComponent>(TEXT("EditableCrossSection"));
	SetRootComponent(Frame);
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Frame->SetGenerateOverlapEvents(false);
	Frame->SetHiddenInGame(true);
	Frame->SetLineThickness(5);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SectionLabel"));
	Label->SetupAttachment(Frame);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(65);
	Label->SetHiddenInGame(true);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector2D AMiningTunnelShapeSection::GetWorldSize(int32 Wall) const
{
	const int32 Axis = Wall / 2;
	const FVector Scale = GetActorScale3D();
	return FVector2D(Width * Scale[Axis == 0 ? 1 : 0], Height * Scale[Axis == 2 ? 1 : 2]);
}

uint32 AMiningTunnelShapeSection::GetGeometryHash() const
{
	uint32 Hash = HashCombineFast(GetTypeHash(GetPathName()), GetTypeHash(Tunnel.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(bEnabled));
	Hash = HashCombineFast(Hash, GetTypeHash(GetActorTransform().ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(Width));
	return HashCombineFast(Hash, GetTypeHash(Height));
}

void AMiningTunnelShapeSection::RefreshGuides()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || !FMath::IsFinite(Width) || !FMath::IsFinite(Height)) return;
	const int32 Axis = IsValid(Tunnel) ? Tunnel->ResolvedStartWall / 2 : 0;
	FVector Extent(15);
	Extent[Axis == 0 ? 1 : 0] = FMath::Max(1.f, Width) * 0.5;
	Extent[Axis == 2 ? 1 : 2] = FMath::Max(1.f, Height) * 0.5;
	Frame->SetBoxExtent(Extent, false);
	Frame->ShapeColor = bEnabled ? FColor(70, 220, 255) : FColor(110, 110, 110);
	Frame->MarkRenderStateDirty();
	Label->SetRelativeLocation(FVector(0, 0, Extent.Z + 100));
	Label->SetRelativeRotation(FRotator(0, 90, 0));
	Label->SetTextRenderColor(Frame->ShapeColor);
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s %.0f x %.0f"), bEnabled ? TEXT("SHAPE") : TEXT("OFF"), Width, Height)));
#endif
}

void AMiningTunnelShapeSection::SetGuideVisibility(bool bVisible)
{
	Frame->SetVisibility(bVisible);
	Label->SetVisibility(bVisible);
}

void AMiningTunnelShapeSection::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshGuides();
}

void AMiningTunnelShapeSection::NotifyGenerators()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Generator->GetLevel() == GetLevel()) Generator->RequestRebuild();
	}
}

void AMiningTunnelShapeSection::RebuildTunnel()
{
	if (IsValid(Tunnel))
	{
		if (AMiningTunnelGenerator* Generator = Cast<AMiningTunnelGenerator>(Tunnel->GetOwner())) Generator->RebuildTunnels();
	}
}

void AMiningTunnelShapeSection::AddAnotherSection()
{
	if (IsValid(Tunnel)) Tunnel->AddShapeSection();
}

void AMiningTunnelShapeSection::SelectTunnel()
{
#if WITH_EDITOR
	if (GEditor && IsValid(Tunnel) && GetWorld() && !GetWorld()->IsGameWorld())
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Tunnel, true, true, true);
	}
#endif
}

#if WITH_EDITOR
void AMiningTunnelShapeSection::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	bInteractiveEdit = !bFinished;
	RefreshGuides();
	NotifyGenerators();
}

void AMiningTunnelShapeSection::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	bInteractiveEdit = (Event.ChangeType & EPropertyChangeType::Interactive) != 0;
	RefreshGuides();
	NotifyGenerators();
}

void AMiningTunnelShapeSection::PostEditUndo()
{
	Super::PostEditUndo();
	bInteractiveEdit = false;
	RefreshGuides();
	NotifyGenerators();
}
#endif
