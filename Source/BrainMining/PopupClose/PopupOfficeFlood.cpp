#include "PopupClose/PopupOfficeFlood.h"
#include "PopupClose/PopupLocalizedArtSubsystem.h"
#include "Engine/GameInstance.h"
#include "PopupClose/PopupCloseLayout.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "SceneInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeFlood,Log,All);
namespace
{
 constexpr int32 WindowBudget=192;
 constexpr int32 PaperBudget=48;
 constexpr int32 PaperFloorBudget=384;
 constexpr float WindowScale=1.4f;
 constexpr float AdditionalComputerDelay=2.f;
 constexpr float SlowEmissionSeconds=5.f;
 constexpr float RapidEmissionSeconds=10.f;
 constexpr float TotalEmissionSeconds=SlowEmissionSeconds+RapidEmissionSeconds;
 constexpr float WindowLayerSpacing=3.2f;
 constexpr int32 MaxWindowLayers=20;
 constexpr float PaperStackLayerSpacing=.6f;
 constexpr float PaperStackThickness=.55f;
 int32 WindowLayersAt(const FVector& P)
 {
  // Localized mounds leave the central pickup and the exit approach clear.
  static const FVector Centers[]={FVector(-520,100,0),FVector(-480,-180,0),FVector(350,-300,0),FVector(570,60,0),FVector(120,450,0),FVector(-30,-470,0)};
  static const float Radii[]={110.f,105.f,135.f,115.f,120.f,100.f};
  float Height=0.f;
  for(int32 I=0;I<UE_ARRAY_COUNT(Centers);++I)
  {
   const float Distance=FVector::Dist2D(P,Centers[I])/Radii[I];
   Height=FMath::Max(Height,FMath::Clamp(1.f-Distance,0.f,1.f));
  }
  return 1+FMath::FloorToInt(Height*(MaxWindowLayers-1));
 }
 float EmissionRate(float Seconds,bool bAdditionalComputer)
 {
  // The first phase should feel like a steady physical ejection. The second
  // phase keeps the same ten-second window, but eases into a restrained rate
  // so the room does not look like it is accelerating objects unnaturally.
  if(Seconds>=TotalEmissionSeconds)return 0.f;
  const float SlowRate=bAdditionalComputer?2.5f:3.5f;
  return Seconds<SlowEmissionSeconds ? SlowRate : FMath::Lerp(8.f,12.f,FMath::Clamp((Seconds-SlowEmissionSeconds)/RapidEmissionSeconds,0.f,1.f));
 }
}

FPopupOfficeFlood::FPopupOfficeFlood(AActor& InOwner):Owner(InOwner)
{
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 // Eight popup backgrounds, a title strip, a close button, and paper.
 for(int32 I=0;I<11;++I)
 {
  auto Mesh=NewObject<UInstancedStaticMeshComponent>(&Owner,*FString::Printf(TEXT("FloodFloor%d"),I));
  Owner.AddInstanceComponent(Mesh);Mesh->SetupAttachment(Owner.GetRootComponent());
  Mesh->SetStaticMesh(Cube);Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);
  const FLinearColor Color=I<8?PopupCloseContent::Get(I).Color:I==8?FLinearColor(.11f,.19f,.29f):I==9?FLinearColor(.85f,.12f,.10f):FLinearColor(.94f,.92f,.86f);
  auto Material=UMaterialInstanceDynamic::Create(Parent,Mesh);Material->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Material);
  if(I<PopupCloseContent::ArtCount)
  {
   auto* Instance=Owner.GetGameInstance();
   auto* Art=Instance?Instance->GetSubsystem<UPopupLocalizedArtSubsystem>()->GetArtwork(I):LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/PopupClose/PopupArt/M_Popup"))+PopupCloseContent::GetArt(I).AssetName));
   if(Art)Mesh->SetMaterial(0,Art);
  }
  Mesh->RegisterComponent();Ground.Add(Mesh);
 }
 // Shuffled cells preserve floor coverage underneath the localized mounds.
 for(float X=-968.f;X<=968.f;X+=32.f)for(float Y=-716.f;Y<=716.f;Y+=44.f)
  Cells.Add(FVector(X+FMath::FRandRange(-2.f,2.f),Y+FMath::FRandRange(-2.f,2.f),2.f));
 CellState.Init(0,Cells.Num());
 for(int32 I=0;I<Cells.Num();++I)CellOrder.Add(I);
 for(int32 I=CellOrder.Num()-1;I>0;--I)Swap(CellOrder[I],CellOrder[FMath::RandRange(0,I)]);
 for(int32 I=0;I<4;++I)
 {
  const FRotator R(0,PopupCloseLayout::DeviceYaw(I),0);
  FSource Source;Source.Origin=PopupCloseLayout::DevicePosition(I)+R.RotateVector(I>=2?FVector(-65,0,118):FVector(24,0,124));
  Source.Direction=-R.Vector();Sources.Add(Source);
 }
 // Follow the actual copier tray overrides, including translated / rotated room instances.
 const FTransform Room=Owner.GetActorTransform();
 for(TActorIterator<APopupOfficeDevice> It(Owner.GetWorld());It;++It)
 {
  auto Device=*It;const int32 I=Device->DeviceIndex;
  if(!Sources.IsValidIndex(I) || FVector::Dist2D(Device->GetActorLocation(),Owner.GetActorLocation())>1500.f)continue;
  Sources[I].Origin=Room.InverseTransformPosition(Device->GetActorTransform().TransformPosition(I>=2?Device->PaperOutputPosition+Device->PaperOutputDirection.GetSafeNormal()*16+FVector(0,0,6):Device->GetSurface()->GetRelativeLocation()+FVector(-15.5f,0,0)));
  Sources[I].Direction=Room.InverseTransformVectorNoScale(I>=2?Device->GetActorTransform().TransformVectorNoScale(Device->PaperOutputDirection).GetSafeNormal():-Device->GetActorForwardVector());
 }
 bool bCustomMonitors=false;
 for(TActorIterator<AActor> It(Owner.GetWorld());It;++It)
 {
  if(!It->ActorHasTag(TEXT("OfficePopupEmitter")) || FVector::Dist2D(It->GetActorLocation(),Owner.GetActorLocation())>1500.f)continue;
  FVector Center,Extent;It->GetActorBounds(false,Center,Extent);
  FSource Source;Source.Origin=Room.InverseTransformPosition(Center-It->GetActorRightVector()*8.f);
  if(It->ActorHasTag(TEXT("OfficeCRTPopupEmitter")))
   Source.Origin=Room.InverseTransformPosition(It->GetActorTransform().TransformPosition(FVector(0,-19.f,27.8f))-It->GetActorRightVector()*6.f);
  Source.Direction=Room.InverseTransformVectorNoScale(-It->GetActorRightVector());Sources.Add(Source);bCustomMonitors=true;
 }
 // Existing maps without an art layout retain their original eight monitor sources.
 if(!bCustomMonitors)for(float X:{-820.f,-370.f,370.f,820.f})for(float Y:{-620.f,620.f})
 {
  const FRotator R(0,Y<0?-90.f:90.f,0);
  FSource Source;Source.Origin=PopupCloseLayout::Compress(FVector(X,Y,0)+R.RotateVector(FVector(15,0,116)));
  Source.Direction=-R.Vector();Sources.Add(Source);
 }
 Slots.Reserve(WindowBudget+PaperBudget);
}
FPopupOfficeFlood::~FPopupOfficeFlood()
{
 for(auto& Slot:Slots)if(auto Actor=Slot.Actor.Get())Actor->Destroy();
 for(auto Mesh:Ground)if(Mesh.IsValid())Mesh->DestroyComponent();
}
void FPopupOfficeFlood::Warm(float Dt)
{
 for(auto& Slot:Slots)if(auto Actor=Slot.Actor.Get())Actor->IsReadyForLaunch();
 if(Slots.Num()>=WindowBudget+PaperBudget)return;
 WarmDebt=FMath::Min(WarmDebt+Dt,.06f);
 int32 Added=0;
 while(WarmDebt>=.02f && Slots.Num()<WindowBudget+PaperBudget && Added++<3)
 {
  WarmDebt-=.02f;
  FSlot Slot;Slot.bPaper=Slots.Num()>=WindowBudget;Slot.Variant=PopupCloseContent::FloodArtVariant(Slots.Num());
  FActorSpawnParameters Params;Params.Owner=&Owner;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto Actor=Owner.GetWorld()->SpawnActor<APopupCloseFlyingWindow>(Owner.GetActorLocation()+FVector(0,0,-10000-Slots.Num()*30),FRotator::ZeroRotator,Params);
  if(!Actor)break;
  Slot.Actor=Actor;Actor->PrepareForPool(Slot.Variant,Slot.bPaper,Slot.bPaper?1.f:WindowScale);
  Actor->SetDenseRendering();Slots.Add(Slot);
 }
}
void FPopupOfficeFlood::Start()
{
 bStarted=true;Time=0;
 for(int32 I=0;I<4;++I){Sources[I].Debt=FMath::FRandRange(0.f,.5f);Emit(I,true);}
}
void FPopupOfficeFlood::Tick(float Dt)
{
 if(!bStarted && !Slots.ContainsByPredicate([](const FSlot& Slot){return Slot.bActive;}))return;
 Time+=Dt;PeakFrame=FMath::Max(PeakFrame,Dt);
 for(auto& Slot:Slots)if(Slot.bActive && !Slot.bPhysical)
 {
  if(Time>=Slot.RetireTime)Retire(Slot,true);
  else if(auto Actor=Slot.Actor.Get())
  {
   const float Age=Time-Slot.Born;
   const float FlightDuration=FMath::Max(.01f,Slot.RetireTime-Slot.Born);
   // Keep the generated phase on the same constant-gravity ballistic curve as
   // the physical phase. The old hand-added arc lift made the midpoint speed
   // jump and read as an artificial acceleration.
   Actor->SetActorLocationAndRotation(Slot.Origin+Slot.Velocity*Age+FVector(0,0,.5f*Owner.GetWorld()->GetGravityZ()*Age*Age),
       Slot.Rotation+FRotator(25*Age,50*Age,40*Age),false,nullptr,ETeleportType::TeleportPhysics);
  }
 }
 // Stopping emission does not cancel the last batch's flight or floor deposits.
 if(!bStarted)return;
 if(Time>=AdditionalComputerDelay && !bAllSources)
 {
  bAllSources=true;
  for(int32 I=4;I<Sources.Num();++I)Sources[I].Debt=FMath::FRandRange(0.f,.75f);
  UE_LOG(LogOfficeFlood,Display,TEXT("%d additional office monitors joined at %.2f s; physical phase ends at %.1f s"),Sources.Num()-4,Time,SlowEmissionSeconds);
 }
 const int32 SourceCount=bAllSources?Sources.Num():4;
 for(int32 I=0;I<SourceCount;++I)
 {
  // Fractional emission credits preserve each source's phase as the rate changes.
  Sources[I].Debt=FMath::Min(Sources[I].Debt+Dt*EmissionRate(Time,I>=4),4.f);
  int32 Count=0;
  while(Sources[I].Debt>=1.f && Count++<4){Sources[I].Debt-=1.f;Emit(I,Time<SlowEmissionSeconds);}
 }
 if(Time>=TotalEmissionSeconds)
 {
  bStarted=false;
  int32 Active=0;for(const auto& Slot:Slots)Active+=Slot.bActive?1:0;
  UE_LOG(LogOfficeFlood,Display,TEXT("Popup eruption reached %.1f s; rapid instance phase complete, %d physical objects still airborne"),TotalEmissionSeconds,Active);
 }
 if(Time<=18.5f && FMath::FloorToInt(Time/3.f)>FMath::FloorToInt((Time-Dt)/3.f))
 {
  int32 Total=0;for(const auto& Source:Sources)Total+=Source.Count;
  const float Rate=4.f*EmissionRate(Time,false)+(bAllSources?8.f*EmissionRate(Time,true):0.f);
  UE_LOG(LogOfficeFlood,Display,TEXT("Emission ramp: t=%.2f nominal_rate=%.2f/s total_emitted=%d"),Time,Rate,Total);
 }
}
int32 FPopupOfficeFlood::SelectCell(const FSource& Source,bool bPaper)
{
 if(bPaper || FloorWindows==Cells.Num())return FMath::RandRange(0,Cells.Num()-1);
 for(int32 Attempt=0;Attempt<Cells.Num();++Attempt)
 {
  const int32 Cell=CellOrder[CellCursor++%CellOrder.Num()];
  if(CellState[Cell]!=0 || FVector::DotProduct(Cells[Cell]-Source.Origin,Source.Direction)<30.f)continue;
  CellState[Cell]=1;return Cell;
 }
 return FMath::RandRange(0,Cells.Num()-1);
}
void FPopupOfficeFlood::Emit(int32 SourceIndex,bool bPhysical)
{
 const bool bPaper=SourceIndex==2 || SourceIndex==3;
 const int32 Start=bPaper?WindowBudget:0;
 const int32 Count=bPaper?FMath::Max(0,Slots.Num()-WindowBudget):FMath::Min(Slots.Num(),WindowBudget);
 if(Count==0)return;
 int32& Cursor=bPaper?PaperCursor:WindowCursor;
 int32 Selected=INDEX_NONE;
 for(int32 Attempt=0;Attempt<Count;++Attempt)
 {
  const int32 Candidate=Start+Cursor++%Count;
  // Early bodies remain exactly where Chaos leaves them, including after 5 s.
  if(!Slots[Candidate].bActive && Slots[Candidate].Actor.IsValid() && Slots[Candidate].Actor->IsReadyForLaunch()){Selected=Candidate;break;}
 }
 // Never recycle a persistent physical body or interrupt a visible flight.
 if(Selected==INDEX_NONE)return;
 FSlot& Slot=Slots[Selected];
 const auto& Source=Sources[SourceIndex];Slot.Cell=SelectCell(Source,bPaper);Slot.Target=Cells[Slot.Cell];
 if(bPaper && Source.Count%4!=0)
 {
  // Three out of four sheets settle in loose stacks near their own printer.
  // Assign layers at launch so their ballistic destinations match the deposits.
  static const FVector Centers[]={FVector(510,260,0),FVector(660,180,0),FVector(-500,-230,0),FVector(-680,-100,0)};
  const int32 Stack=(SourceIndex==2?0:2)+(Source.Count/4)%2;
  const int32 Layer=(Source.Count/8)*3+Source.Count%4-1;
  Slot.Target=Centers[Stack]+FVector(FMath::FRandRange(-5.f,5.f),FMath::FRandRange(-6.f,6.f),7.f+PaperStackLayerSpacing*Layer);
 }
 auto Actor=Slot.Actor.Get();if(!Actor)return;
 const FTransform Room=Owner.GetActorTransform();
 const FVector Origin=Room.TransformPosition(Source.Origin);
 const FVector Target=Room.TransformPosition(Slot.Target);
 const float FlightSeconds=bPhysical?FMath::FRandRange(1.35f,1.65f):FMath::FRandRange(1.05f,1.35f);
 FVector Velocity=(Target-Origin)/FlightSeconds;Velocity.Z-=.5f*Owner.GetWorld()->GetGravityZ()*FlightSeconds;
 Actor->SetActorLocationAndRotation(Origin,FRotator(FMath::FRandRange(-20.f,20.f),Room.Rotator().Yaw+Source.Direction.Rotation().Yaw+180.f,FMath::FRandRange(-15.f,15.f)),false,nullptr,ETeleportType::TeleportPhysics);
 Slot.Rotation=Actor->GetActorRotation();Slot.Born=Time;Slot.Origin=Origin;Slot.Velocity=Velocity;Slot.bPhysical=bPhysical;
 if(bPhysical)Actor->Launch(PopupCloseContent::Get(Slot.Variant).Color,Velocity);
 else Actor->LaunchGenerated();
 Slot.bActive=true;Slot.RetireTime=Time+FlightSeconds;++Sources[SourceIndex].Count;
}
void FPopupOfficeFlood::Retire(FSlot& Slot,bool bDeposit)
{
 if(auto Actor=Slot.Actor.Get())
 {
  if(Slot.bPaper && Slot.bPhysical)if(auto Body=Actor->FindComponentByClass<UStaticMeshComponent>())
   if(Body->CalcBounds(Body->GetComponentTransform()).GetBox().Min.Z<Owner.GetActorLocation().Z-.25f)++BelowFloorPapers;
  Actor->RetireToPool();
 }
 if(bDeposit)Deposit(Slot);
 Slot.bActive=false;Slot.bPhysical=false;++Recycled;
}
void FPopupOfficeFlood::AddGround(int32 Group,FVector P,FVector Size,FRotator Rotation)
{
 if(Ground.IsValidIndex(Group) && Ground[Group].IsValid())Ground[Group]->AddInstance(FTransform(Rotation,P,Size/100.f));
}
void FPopupOfficeFlood::Deposit(const FSlot& Slot)
{
 FVector P=Slot.Target;
 if(Slot.bPaper)
 {
  if(FloorPapers>=PaperFloorBudget+(bStarted?0:PaperBudget))return;
  const bool bStacked=P.Z>4.f;
  const float Yaw=bStacked?FMath::FRandRange(-18.f,18.f):FMath::FRandRange(-180.f,180.f);
  if(!bStacked)P.Z=7.f+(WindowLayersAt(P)-1)*WindowLayerSpacing;
  else if(!bStarted && FloorPapers>=PaperFloorBudget)
  {
   // Earlier capped sheets leave no gaps underneath the final visible sheets.
   const int32 LastBaseLayer=PaperFloorBudget*3/16-1;
   const int32 TailLayer=1+(FloorPapers-PaperFloorBudget)/4;
   P.Z=FMath::Min(P.Z,7.f+PaperStackLayerSpacing*(LastBaseLayer+TailLayer));
  }
  AddGround(10,P,FVector(bStacked?PaperStackThickness:.15f,21,29.7f),FRotator(-90,Yaw,0));++FloorPapers;return;
 }
 const bool bCoveredCell=CellState[Slot.Cell]==2;
 if(bCoveredCell && bStarted)return;
 if(!bCoveredCell){CellState[Slot.Cell]=2;++FloorWindows;}
 // At most one extra persistent window per airborne slot survives the handoff.
 const int32 Layers=bCoveredCell?1:WindowLayersAt(P);
 const float BaseZ=bCoveredCell?7.f+(WindowLayersAt(P)-1)*WindowLayerSpacing:2.f;
 const float Yaw=FMath::FRandRange(-180.f,180.f);
 P.X+=FMath::FRandRange(-8.f,8.f);P.Y+=FMath::FRandRange(-12.f,12.f);
 P.Z=BaseZ+FMath::FRandRange(0.f,.6f);
 for(int32 Layer=0;Layer<Layers;++Layer)
 {
  const int32 Variant=PopupCloseContent::FloodArtVariant(Slot.Cell+Layer);
  const auto& Copy=PopupCloseContent::Get(Variant);
  const auto& Skin=PopupCloseContent::GetArt(Variant);
  const bool bArtwork=Ground[Variant]->GetMaterial(0) && Ground[Variant]->GetMaterial(0)->GetName().StartsWith(TEXT("M_Popup"));
  const FVector2D Size=bArtwork?Skin.Size:Copy.Size;
  const float W=Size.X*.13f*WindowScale,H=Size.Y*.13f*WindowScale;
  const FRotator R(-90,Yaw+Layer*31.f,FMath::FRandRange(-2.f,2.f));
  const FVector LayerP=P+FVector(FMath::FRandRange(-4.f,4.f),FMath::FRandRange(-4.f,4.f),Layer*WindowLayerSpacing);
  AddGround(Variant,LayerP,FVector(2.5f,W,H),R);
  if(!bArtwork)
  {
   AddGround(8,LayerP+R.RotateVector(FVector(-1.5f,0,H*.43f)),FVector(3.25f,W,H*.14f),R);
   AddGround(9,LayerP+R.RotateVector(FVector(-2.75f,W*.43f,H*.43f)),FVector(3.75f,W*.10f,H*.11f),R);
  }
 }
}
void FPopupOfficeFlood::Pause()
{
 for(auto& Slot:Slots)if(Slot.bActive && !Slot.bPhysical)Retire(Slot,true);
}
void FPopupOfficeFlood::Stop()
{
 if(!bStarted)return;
 bStarted=false;
 int32 Active=0;for(const auto& Slot:Slots)Active+=Slot.bActive?1:0;
 UE_LOG(LogOfficeFlood,Display,TEXT("Emission stopped; %d airborne popups/papers continue landing and remain on the floor"),Active);
}
void FPopupOfficeFlood::ClearVisuals()
{
 Stop();for(auto& Slot:Slots)if(Slot.bActive)Retire(Slot,false);for(auto Mesh:Ground)if(Mesh.IsValid())Mesh->ClearInstances();
}
void FPopupOfficeFlood::Reset()
{
 for(auto& Slot:Slots)if(Slot.bActive)Retire(Slot,false);
 for(auto Mesh:Ground)if(Mesh.IsValid())Mesh->ClearInstances();
 CellState.Init(0,Cells.Num());FloorWindows=FloorPapers=Recycled=BelowFloorPapers=CellCursor=WindowCursor=PaperCursor=0;
 Time=PeakFrame=0;bStarted=bAllSources=false;
 for(auto& Source:Sources){Source.Count=0;Source.Debt=0;}
}
bool FPopupOfficeFlood::IsFloorCovered() const{return FloorWindows>=Cells.Num()*.92f;}
bool FPopupOfficeFlood::IsHealthy() const
{
 bool SourcesHealthy=bAllSources;
 for(const auto& Source:Sources)SourcesHealthy &= Source.Count>0;
 return SourcesHealthy && Slots.Num()<=WindowBudget+PaperBudget && FloorWindows<=Cells.Num() && FloorPapers<=PaperFloorBudget+PaperBudget && BelowFloorPapers==0;
}
int32 FPopupOfficeFlood::Emitted(int32 Source) const{return Sources.IsValidIndex(Source)?Sources[Source].Count:0;}
FString FPopupOfficeFlood::Describe() const
{
 int32 Active=0,Total=0;for(const auto& Slot:Slots)Active+=Slot.bActive?1:0;for(const auto& Source:Sources)Total+=Source.Count;
 FString Text=FString::Printf(TEXT("Flood time=%.2f sources=%d coverage=%.1f%% ground_windows=%d/%d ground_papers=%d/432 airborne=%d pool=%d/240 total_emitted=%d recycled=%d paper_below_floor=%d max_frame=%.2fms"),
 Time,bAllSources?Sources.Num():4,100.f*FloorWindows/Cells.Num(),FloorWindows,Cells.Num(),FloorPapers,Active,Slots.Num(),Total,Recycled,BelowFloorPapers,PeakFrame*1000.f);
 int32 Missing=0,Compiling=0,PSOs=0,Ready=0;
 for(const auto& Slot:Slots)if(auto Actor=Slot.Actor.Get())
 {
  auto Mesh=Actor->FindComponentByClass<UStaticMeshComponent>();auto Material=Mesh->GetMaterial(0);
  auto Resource=Material && Owner.GetWorld()->Scene?Material->GetMaterialResource(Owner.GetWorld()->Scene->GetShaderPlatform()):nullptr;
  Missing+=Resource?0:1;Compiling+=Resource && !Resource->IsGameThreadShaderMapComplete()?1:0;
  PSOs+=Mesh->IsPSOPrecaching()?1:0;Ready+=Actor->IsReadyForLaunch()?1:0;
 }
 Text+=FString::Printf(TEXT("\nVisual readiness: ready=%d missing=%d compiling=%d pso=%d"),Ready,Missing,Compiling,PSOs);
 for(int32 I=0;I<Sources.Num();++I)Text+=FString::Printf(TEXT("\nSource%d emitted=%d"),I,Sources[I].Count);
 Text+=FString::Printf(TEXT("\nCurrent emission rate=%.2f/s"),bStarted?4.f*EmissionRate(Time,false)+(bAllSources?(Sources.Num()-4)*EmissionRate(Time,true):0.f):0.f);
 int32 VisibleWindows=0;float HighestCenter=0.f;
 for(int32 I=0;I<Ground.Num();++I)if(auto Mesh=Ground[I].Get())
 {
  if(I<8)VisibleWindows+=Mesh->GetInstanceCount();
  if(I<8 || I==10)for(int32 J=0;J<Mesh->GetInstanceCount();++J)
  {
   FTransform Transform;if(Mesh->GetInstanceTransform(J,Transform))HighestCenter=FMath::Max(HighestCenter,static_cast<float>(Transform.GetLocation().Z));
  }
 }
 Text+=FString::Printf(TEXT("\nGround heaps: visible_windows=%d extra_layers=%d highest_center=%.1fcm paper_stacks=4"),VisibleWindows,VisibleWindows-FloorWindows,HighestCenter);
 return Text;
}
