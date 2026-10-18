#include "Showcase/SMiningCampaignMap.h"
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
    auto Text=[&](FVector2D P,const FString& S,int32 Size,FLinearColor Color){FSlateDrawElement::MakeText(Out,Layer+5,G.ToPaintGeometry(FVector2D(1300,60),FSlateLayoutTransform(P)),S,FCoreStyle::GetDefaultFontStyle("Regular",Size),ESlateDrawEffect::None,Color);};
    auto Line=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width){FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width);};
    auto Mark=[&](FVector2D P,FLinearColor Color,const FString& Name){Box(P-FVector2D(5,5),FVector2D(10,10),Color);Text(P+FVector2D(10,-12),Name,15,Color);};
    Text({42,28},TEXT("矿道地图"),32,White);Text({1100,40},TEXT("已暂停  ·  M / Esc 返回"),18,Muted);
    Text({42,82},Area.Title.ToString(),22,Cyan);
    const TCHAR* Short[]={TEXT("挖掘 01"),TEXT("电脑房"),TEXT("挖掘 02"),TEXT("回声 / 卡牌"),TEXT("挖掘 03"),TEXT("博物馆"),TEXT("宇宙细胞")};
    for(int32 I=0;I<H->Areas.Num();++I)
    {
        const float X=42+I*190;
        Box({X,130},{178,43},I==H->ActiveArea?FLinearColor(.08f,.23f,.25f,1):Panel);
        Text({X+12,139},I<7?Short[I]:H->Areas[I].Title.ToString(),17,I==H->ActiveArea?Cyan:I<=H->UnlockedArea?White:Muted);
    }
    Box({32,199},{895,627},Panel);
    Text({955,211},TEXT("你在这里"),24,Gold);
    Text({955,253},TEXT("黄色箭头 = 位置与朝向"),18,White);
    Text({955,288},TEXT("青线 = 通往出口的主路"),18,Cyan);
    Text({955,323},TEXT("白点 = 入口   绿点 = 出口"),18,White);
    Text({955,368},H->Collection && H->Collection->bBuriedWithClues?TEXT("本段挖掘线索"):TEXT("本段藏品"),21,White);
    int32 Row=0;
    if(H->Collection) for(const auto& Item:H->Collection->AuthoredPlacements) if(Item.AreaTag==Area.Tag)
    {
        const bool Got=H->Collection->Inventory.IsValidIndex(Item.ItemId)&&H->Collection->Inventory[Item.ItemId]>0;
        const FString Clue=AMiningCollectible::Clue(Item.ItemId);
        const FString Name=H->Collection->bBuriedWithClues && !Got && !Clue.IsEmpty()?Clue:AMiningCollectible::ItemName(Item.ItemId);
        Text({955,409+Row++*36.f},FString::Printf(TEXT("%d  %s  %s"),Item.ItemId+1,*Name,Got?TEXT("已收集"):TEXT("")),17,Got?Muted:Gold);
    }
    Text({42,852},TEXT("路线示意，不显示挖开的洞形；矿道会向下延伸，请同时参考高度图。"),17,Muted);
    if(Area.MapRoute.Num()<2)
    {
        Text({80,280},TEXT("当前为玩法房间"),30,White);
        Text({80,340},TEXT("本页显示全流程位置；此房间暂无内部平面图。"),21,Muted);
        Text({80,402},Area.Instructions.ToString(),18,Cyan);
        Text({955,570},TEXT("完成当前玩法后"),20,White);Text({955,607},H->bContinuousCampaign?TEXT("出口打开，步行继续。"):TEXT("自动进入下一段。"),20,White);
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
    Text({60,213},TEXT("俯视  ·  上方为 +Y"),16,Muted);
    Text({60,632},TEXT("高度图  ·  左入口 → 右出口"),16,Muted);
    TArray<FVector2D> Plan,Profile;for(const auto& P:Route){Plan.Add(Top(P));Profile.Add(Depth(P));}
    Line(Plan,FLinearColor(.07f,.23f,.25f,1),15);Line(Plan,Cyan,3);Line(Profile,Cyan,3);
    Mark(Top(Route[0]),White,TEXT("入口"));Mark(Top(Route.Last()),FLinearColor(.3f,1,.5f),TEXT("出口"));
    if(H->Collection) for(const auto& Item:H->Collection->AuthoredPlacements) if(Item.AreaTag==Area.Tag)
    {
        const bool Got=H->Collection->Inventory.IsValidIndex(Item.ItemId)&&H->Collection->Inventory[Item.ItemId]>0;
        Mark(Top(H->Collection->bBuriedWithClues?Item.ClueTransform.GetLocation():Item.Transform.GetLocation()),Got?Muted:Gold,FString::FromInt(Item.ItemId+1));
    }
    const auto P=Top(Player); const FVector Forward=PC->GetControlRotation().Vector();
    const FVector2D D=FVector2D(Forward.X,-Forward.Y).GetSafeNormal(),Side(-D.Y,D.X);
    Line({P+D*16,P-D*10+Side*9,P-D*5,P-D*10-Side*9,P+D*16},Gold,3);
    Mark(Depth(Player),Gold,TEXT("你"));
    Text({955,570},FString::Printf(TEXT("沿主路剩余约 %.0f 米"),(Arcs.Last()-NearestArc(Player))/100),22,Cyan);
    Text({955,615},FString::Printf(TEXT("低于入口 %.0f 米"),(Route[0].Z-Player.Z)/100),19,White);
    Text({955,683},TEXT("向绿点前进，到出口"),18,White);Text({955,713},TEXT("即可进入下一房间。"),18,White);
    Text({955,770},TEXT("卡住可关闭地图后按 F6"),17,Muted);
    return Layer+5;
}
