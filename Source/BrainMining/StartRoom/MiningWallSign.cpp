#include "StartRoom/MiningWallSign.h"
#include "BrainMiningFonts.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

AMiningWallSign::AMiningWallSign()
{
    Surface=CreateDefaultSubobject<UWidgetComponent>(TEXT("Sign"));
    SetRootComponent(Surface);
    Surface->SetDrawSize(FVector2D(1024,256));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetTwoSided(false);
    Surface->SetRelativeScale3D(FVector(.25f));
    Surface->SetCastShadow(false);
}
void AMiningWallSign::Refresh()
{
    Surface->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(STextBlock).Text(Words).Font(BrainMiningFonts::Body(64))
        .Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(.7f,.87f,.9f))]);
}
void AMiningWallSign::BeginPlay(){Super::BeginPlay();Refresh();}
void AMiningWallSign::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);Refresh();}
