#include "StartRoom/MiningWallSign.h"
#include "BrainMiningFonts.h"
#include "MiningLocalizationSubsystem.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

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
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [SNew(STextBlock).Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(this,&AMiningWallSign::GetDisplayText))).Font(BrainMiningFonts::Body(64))
        .Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(.7f,.87f,.9f))]]);
}
FText AMiningWallSign::GetDisplayText() const
{
    if(!LocalizationKey.IsNone())return MiningText::Get(*LocalizationKey.ToString());
    const FString Authored=Words.ToString().TrimStartAndEnd();
    if(Authored==TEXT("按住空格起飞") || Authored==TEXT("按住空格键起飞"))
        return MiningText::Get(TEXT("UI.World.HoldSpaceToFly"));
    return Words;
}
void AMiningWallSign::BeginPlay(){Super::BeginPlay();Refresh();}
void AMiningWallSign::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);Refresh();}
