#include "Showcase/SMiningCampaignMap.h"
#include "BrainMiningFonts.h"
#include "MiningLocalizationSubsystem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

bool AMiningExhibitHall::ToggleMap()
{
    if(Map) { CloseMap(); return true; }
    if(!bReady || !bLinearCampaign || IsInteractionOpen() || !Controller->GetPawn()) return false;
    Map=SNew(SMiningCampaignMap).Hall(this);
    Controller->GetPawn()->FindComponentByClass<UMiningComponent>()->SetInputSuspended(true);
    Controller->SetIgnoreMoveInput(true); Controller->SetIgnoreLookInput(true); Controller->SetPause(true);
    GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(Map.ToSharedRef(),1200);
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Map); Controller->SetInputMode(Mode);
    Controller->bShowMouseCursor=false;
    return true;
}

void AMiningExhibitHall::CloseMap()
{
    if(!Map) return;
    if(GetGameInstance()) if(auto* Viewport=GetGameInstance()->GetGameViewportClient()) Viewport->RemoveViewportWidgetContent(Map.ToSharedRef());
    Map.Reset();
    if(Controller.IsValid())
    {
        Controller->SetPause(false); Controller->SetIgnoreMoveInput(false); Controller->SetIgnoreLookInput(false);
        Controller->SetInputMode(FInputModeGameOnly()); Controller->bShowMouseCursor=false;
        if(Controller->GetPawn()) Controller->GetPawn()->FindComponentByClass<UMiningComponent>()->SetInputSuspended(false);
    }
}

FReply SMiningCampaignMap::OnKeyDown(const FGeometry&,const FKeyEvent& Event)
{
    if((Event.GetKey()==EKeys::M || Event.GetKey()==EKeys::Escape) && !Event.IsRepeat() && Hall.IsValid()) Hall->CloseMap();
    return FReply::Handled();
}

int32 SMiningCampaignMap::OnPaint(const FPaintArgs&,const FGeometry& Allotted,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    const auto* H=Hall.Get(); if(!H || !H->Areas.IsValidIndex(H->ActiveArea)) return Layer;
    const auto& Area=H->Areas[H->ActiveArea];
    auto* PC=H->GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC?PC->GetPawn():nullptr;
    if(!Pawn) return Layer;
    const FVector Player=Pawn->GetActorLocation();
    const FLinearColor Bg(.012f,.022f,.03f,1),Panel(.025f,.044f,.053f,1),Cyan(.22f,.83f,.81f,1),Gold(1,.74f,.25f,1),Muted(.45f,.59f,.63f,1),White(.9f,.95f,.95f,1);
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out,Layer,Allotted.ToPaintGeometry(),Brush,ESlateDrawEffect::None,Bg);
    const float Scale=FMath::Min(Allotted.GetLocalSize().X/1400,Allotted.GetLocalSize().Y/900);
    const auto G=Allotted.MakeChild(FVector2D(1400,900),FSlateLayoutTransform(Scale,(Allotted.GetLocalSize()-FVector2D(1400,900)*Scale)*.5));
    auto Box=[&](FVector2D P,FVector2D Size,FLinearColor Color){FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Color);};
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto Text=[&](FVector2D P,const FString& S,int32 Size,FLinearColor Color,bool bTitle=false,float MaxWidth=0.f)
    {
        const auto Font=BrainMiningFonts::Style(bTitle?TEXT("Bold"):TEXT("Medium"),Size);
        const float Available=MaxWidth>0?MaxWidth:FMath::Max(1.f,1360.f-static_cast<float>(P.X));
        TArray<FString> Lines;
        S.ParseIntoArrayLines(Lines,false);
        float Offset=0;
        for(const FString& TextLine:Lines)
        {
            const float TextWidth=Measure->Measure(TextLine,Font).X;
            const float TextScale=TextWidth>Available?Available/TextWidth:1.f;
            FSlateDrawElement::MakeText(Out,Layer+5,G.ToPaintGeometry(FVector2D(Available/TextScale,60),FSlateLayoutTransform(TextScale,P+FVector2D(0,Offset))),TextLine,Font,ESlateDrawEffect::None,Color);
            Offset+=Size*1.6f;
        }
    };
    auto Line=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width){FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width);};
    auto Mark=[&](FVector2D P,FLinearColor Color,const FString& Name){Box(P-FVector2D(5,5),FVector2D(10,10),Color);Text(P+FVector2D(10,-12),Name,15,Color);};
    Text({42,28},MiningText::Get(TEXT("UI.Map.Title")).ToString(),32,White,true);Text({1040,40},MiningText::Get(TEXT("UI.Map.Back")).ToString(),18,Muted);
    Text({42,82},Area.Title.ToString(),22,Cyan,true);
    const FString Short[]={MiningText::Get(TEXT("UI.Map.Mine1")).ToString(),MiningText::Get(TEXT("UI.Map.Office")).ToString(),MiningText::Get(TEXT("UI.Map.Mine2")).ToString(),MiningText::Get(TEXT("UI.Map.EchoCards")).ToString(),MiningText::Get(TEXT("UI.Map.Mine3")).ToString(),MiningText::Get(TEXT("UI.Map.Museum")).ToString(),MiningText::Get(TEXT("UI.Map.Universe")).ToString()};
    for(int32 I=0;I<H->Areas.Num();++I)
    {
        const float X=42+I*190;
        Box({X,130},{178,43},I==H->ActiveArea?FLinearColor(.08f,.23f,.25f,1):Panel);
        Text({X+12,139},I<UE_ARRAY_COUNT(Short)?Short[I]:H->Areas[I].Title.ToString(),17,I==H->ActiveArea?Cyan:I<=H->UnlockedArea?White:Muted,false,154);
    }
    Box({32,199},{895,627},Panel);
    Text({955,211},MiningText::Get(TEXT("UI.Map.YouAreHere")).ToString(),24,Gold,true);
    Text({955,253},MiningText::Get(TEXT("UI.Map.PlayerLegend")).ToString(),18,White);
    Text({955,288},MiningText::Get(TEXT("UI.Map.RouteLegend")).ToString(),18,Cyan);
    Text({955,323},MiningText::Get(TEXT("UI.Map.EndpointsLegend")).ToString(),18,White);
    Text({955,368},H->Collection && H->Collection->bBuriedWithClues?MiningText::Get(TEXT("UI.Map.Clues")).ToString():MiningText::Get(TEXT("UI.Map.Collectibles")).ToString(),21,White,true);
    int32 Row=0;
    int32 ItemCount=0;
    if(H->Collection)for(const auto& Item:H->Collection->AuthoredPlacements)if(Item.AreaTag==Area.Tag)++ItemCount;
    const float RowHeight=FMath::Min(30.f,175.f/FMath::Max(1,ItemCount));
    const int32 RowFont=FMath::Min(17,FMath::Max(10,FMath::FloorToInt(RowHeight-6)));
    if(H->Collection) for(const auto& Item:H->Collection->AuthoredPlacements) if(Item.AreaTag==Area.Tag)
    {
        const bool Got=H->Collection->Inventory.IsValidIndex(Item.ItemId)&&H->Collection->Inventory[Item.ItemId]>0;
        const FString Clue=AMiningCollectible::Clue(Item.ItemId);
        const FString Name=H->Collection->bBuriedWithClues && !Got && !Clue.IsEmpty()?Clue:AMiningCollectible::ItemName(Item.ItemId);
        Text({955,409+Row++*RowHeight},FString::Printf(TEXT("%d  %s  %s"),Item.ItemId+1,*Name,Got?*MiningText::Get(TEXT("UI.Map.Collected")).ToString():TEXT("")),RowFont,Got?Muted:Gold);
    }
    Text({42,852},MiningText::Get(TEXT("UI.Map.SchematicHint")).ToString(),17,Muted);
    if(Area.MapRoute.Num()<2)
    {
        Text({80,280},MiningText::Get(TEXT("UI.Map.Room")).ToString(),30,White,true);
        Text({80,340},MiningText::Get(TEXT("UI.Map.NoFloorPlan")).ToString(),21,Muted,false,815);
        Text({80,402},Area.Instructions.ToString(),18,Cyan,false,815);
        Text({955,610},MiningText::Get(TEXT("UI.Map.AfterCompletion")).ToString(),20,White);Text({955,650},H->bContinuousCampaign?MiningText::Get(TEXT("UI.Map.WalkOn")).ToString():MiningText::Get(TEXT("UI.Map.NextArea")).ToString(),20,White);
        return Layer+5;
    }
    const auto& Route=Area.MapRoute;
    TArray<float> Arcs; Arcs.Add(0);
    FBox Bounds(ForceInit); for(const auto& P:Route) Bounds+=P; Bounds+=Player;
    for(int32 I=1;I<Route.Num();++I) Arcs.Add(Arcs.Last()+FVector::Dist(Route[I-1],Route[I]));
    auto NearestArc=[&](FVector P){float Best=MAX_flt,Arc=0;for(int32 I=1;I<Route.Num();++I){const FVector D=Route[I]-Route[I-1];const float T=FMath::Clamp(FVector::DotProduct(P-Route[I-1],D)/FMath::Max(D.SizeSquared(),1.),0.,1.);const float Dist=FVector::DistSquared(P,Route[I-1]+D*T);if(Dist<Best){Best=Dist;Arc=FMath::Lerp(Arcs[I-1],Arcs[I],T);}}return Arc;};
    const auto Center=Bounds.GetCenter();const auto Size=Bounds.GetSize();
    const float Zoom=FMath::Min(760./FMath::Max(Size.X,500.),330./FMath::Max(Size.Y,500.));
    auto Top=[&](FVector P){return FVector2D(470+(P.X-Center.X)*Zoom,421-(P.Y-Center.Y)*Zoom);};
    auto Depth=[&](FVector P){return FVector2D(90+NearestArc(P)/FMath::Max(Arcs.Last(),1.f)*760,780-(P.Z-Bounds.Min.Z)/FMath::Max(Size.Z,300.)*106);};
    Text({60,213},MiningText::Get(TEXT("UI.Map.Plan")).ToString(),16,Muted);
    Text({60,632},MiningText::Get(TEXT("UI.Map.Elevation")).ToString(),16,Muted);
    TArray<FVector2D> Plan,Profile;for(const auto& P:Route){Plan.Add(Top(P));Profile.Add(Depth(P));}
    Line(Plan,FLinearColor(.07f,.23f,.25f,1),15);Line(Plan,Cyan,3);Line(Profile,Cyan,3);
    Mark(Top(Route[0]),White,MiningText::Get(TEXT("UI.Map.Entrance")).ToString());Mark(Top(Route.Last()),FLinearColor(.3f,1,.5f),MiningText::Get(TEXT("UI.Map.Exit")).ToString());
    if(H->Collection) for(const auto& Item:H->Collection->AuthoredPlacements) if(Item.AreaTag==Area.Tag)
    {
        const bool Got=H->Collection->Inventory.IsValidIndex(Item.ItemId)&&H->Collection->Inventory[Item.ItemId]>0;
        Mark(Top(H->Collection->bBuriedWithClues?Item.ClueTransform.GetLocation():Item.Transform.GetLocation()),Got?Muted:Gold,FString::FromInt(Item.ItemId+1));
    }
    const auto P=Top(Player); const FVector Forward=PC->GetControlRotation().Vector();
    const FVector2D D=FVector2D(Forward.X,-Forward.Y).GetSafeNormal(),Side(-D.Y,D.X);
    Line({P+D*16,P-D*10+Side*9,P-D*5,P-D*10-Side*9,P+D*16},Gold,3);
    Mark(Depth(Player),Gold,MiningText::Get(TEXT("UI.Map.You")).ToString());
    Text({955,610},FText::Format(MiningText::Get(TEXT("UI.Map.Remaining")),FMath::RoundToInt((Arcs.Last()-NearestArc(Player))/100)).ToString(),22,Cyan);
    Text({955,650},FText::Format(MiningText::Get(TEXT("UI.Map.BelowEntrance")),FMath::RoundToInt((Route[0].Z-Player.Z)/100)).ToString(),19,White);
    Text({955,710},MiningText::Get(TEXT("UI.Map.FollowExit")).ToString(),18,White);Text({955,740},MiningText::Get(TEXT("UI.Map.EnterNext")).ToString(),18,White);
    Text({955,790},MiningText::Get(TEXT("UI.Map.Recover")).ToString(),17,Muted);
    return Layer+5;
}
