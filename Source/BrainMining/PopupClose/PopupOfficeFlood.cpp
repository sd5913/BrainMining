#include "PopupClose/PopupOfficeFlood.h"
#include "PopupClose/PopupCloseLayout.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseContent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeFlood,Log,All);
namespace
{
 constexpr int32 WindowBudget=192;
 constexpr int32 PaperBudget=48;
 constexpr int32 PaperFloorBudget=384;
 constexpr float WindowScale=1.4f;
 constexpr float AdditionalComputerDelay=3.f;
 constexpr float EmissionRampSeconds=12.f;
 float EmissionRate(float Seconds,bool bAdditionalComputer)
 {
  const float Start=bAdditionalComputer?AdditionalComputerDelay:0.f;
  const float Alpha=FMath::Clamp((Seconds-Start)/EmissionRampSeconds,0.f,1.f);
  return FMath::Lerp(bAdditionalComputer?0.f:1.f,10.f,Alpha);
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
  Mesh->RegisterComponent();Ground.Add(Mesh);
 }
 // Shuffled cells spread the persistent layer over the room instead of piling up centrally.
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
 // These match the eight monitors already built into the office desks.
 for(float X:{-820.f,-370.f,370.f,820.f})for(float Y:{-620.f,620.f})
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
 if(Slots.Num()>=WindowBudget+PaperBudget)return;
 WarmDebt=FMath::Min(WarmDebt+Dt,.06f);
 int32 Added=0;
 while(WarmDebt>=.02f && Slots.Num()<WindowBudget+PaperBudget && Added++<3)
 {
  WarmDebt-=.02f;
  FSlot Slot;Slot.bPaper=Slots.Num()>=WindowBudget;Slot.Variant=Slots.Num()%8;
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
 for(int32 I=0;I<4;++I){Sources[I].Debt=FMath::FRandRange(0.f,.5f);Emit(I);}
}
void FPopupOfficeFlood::Tick(float Dt)
{
 if(!bStarted)return;
 Time+=Dt;PeakFrame=FMath::Max(PeakFrame,Dt);
 for(auto& Slot:Slots)if(Slot.bActive && Time>=Slot.RetireTime)Retire(Slot,true);
 if(Time>=AdditionalComputerDelay && !bAllSources)
 {
  bAllSources=true;
  for(int32 I=4;I<Sources.Num();++I)Sources[I].Debt=FMath::FRandRange(0.f,.75f);
  UE_LOG(LogOfficeFlood,Display,TEXT("Eight office computers joined at %.2f s; their emission rate starts at zero and ramps over 12 seconds"),Time);
 }
 const int32 SourceCount=bAllSources?Sources.Num():4;
 for(int32 I=0;I<SourceCount;++I)
 {
  // Fractional emission credits preserve each source's phase as the rate changes.
  Sources[I].Debt=FMath::Min(Sources[I].Debt+Dt*EmissionRate(Time,I>=4),2.f);
  int32 Count=0;
  while(Sources[I].Debt>=1.f && Count++<2){Sources[I].Debt-=1.f;Emit(I);}
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
void FPopupOfficeFlood::Emit(int32 SourceIndex)
{
 const bool bPaper=SourceIndex==2 || SourceIndex==3;
 const int32 Start=bPaper?WindowBudget:0;
 const int32 Count=bPaper?FMath::Max(0,Slots.Num()-WindowBudget):FMath::Min(Slots.Num(),WindowBudget);
 if(Count==0)return;
 int32& Cursor=bPaper?PaperCursor:WindowCursor;
 FSlot& Slot=Slots[Start+Cursor++%Count];
 if(Slot.bActive)Retire(Slot,true);
 auto Actor=Slot.Actor.Get();if(!Actor)return;
 const auto& Source=Sources[SourceIndex];Slot.Cell=SelectCell(Source,bPaper);Slot.Target=Cells[Slot.Cell];
 const FTransform Room=Owner.GetActorTransform();
 const FVector Origin=Room.TransformPosition(Source.Origin);
 const FVector Target=Room.TransformPosition(Slot.Target);
 const float FlightSeconds=FMath::FRandRange(1.15f,1.40f);
 FVector Velocity=(Target-Origin)/FlightSeconds;Velocity.Z-=.5f*Owner.GetWorld()->GetGravityZ()*FlightSeconds;
 Actor->SetActorLocationAndRotation(Origin,FRotator(FMath::FRandRange(-20.f,20.f),Room.Rotator().Yaw+Source.Direction.Rotation().Yaw+180.f,FMath::FRandRange(-15.f,15.f)),false,nullptr,ETeleportType::TeleportPhysics);
 Actor->Launch(PopupCloseContent::Get(Slot.Variant).Color,Velocity);
 Slot.bActive=true;Slot.RetireTime=Time+FlightSeconds+.25f;++Sources[SourceIndex].Count;
}
void FPopupOfficeFlood::Retire(FSlot& Slot,bool bDeposit)
{
 if(auto Actor=Slot.Actor.Get())
 {
  if(Slot.bPaper)if(auto Body=Actor->FindComponentByClass<UStaticMeshComponent>())
   if(Body->CalcBounds(Body->GetComponentTransform()).GetBox().Min.Z<Owner.GetActorLocation().Z-.25f)++BelowFloorPapers;
  Actor->RetireToPool();
 }
 if(bDeposit)Deposit(Slot);
 Slot.bActive=false;++Recycled;
}
void FPopupOfficeFlood::AddGround(int32 Group,FVector P,FVector Size,FRotator Rotation)
{
 if(Ground.IsValidIndex(Group) && Ground[Group].IsValid())Ground[Group]->AddInstance(FTransform(Rotation,P,Size/100.f));
}
void FPopupOfficeFlood::Deposit(const FSlot& Slot)
{
 const float Yaw=FMath::FRandRange(-180.f,180.f);
 const FRotator R(-90,Yaw,0);
 FVector P=Slot.Target;
 P.X+=FMath::FRandRange(-8.f,8.f);P.Y+=FMath::FRandRange(-12.f,12.f);
 P.Z+=FMath::FRandRange(0.f,1.2f);
 if(Slot.bPaper)
 {
  if(FloorPapers>=PaperFloorBudget)return;
  P.Z=5.f+FMath::FRandRange(0.f,.6f);AddGround(10,P,FVector(.15f,21,29.7f),R);++FloorPapers;return;
 }
 if(CellState[Slot.Cell]==2)return;
 CellState[Slot.Cell]=2;++FloorWindows;
 const auto& Copy=PopupCloseContent::Get(Slot.Variant);
 const float W=Copy.Size.X*.13f*WindowScale,H=Copy.Size.Y*.13f*WindowScale;
 AddGround(Slot.Variant,P,FVector(2.5f,W,H),R);
 AddGround(8,P+R.RotateVector(FVector(-1.5f,0,H*.43f)),FVector(3.25f,W,H*.14f),R);
 AddGround(9,P+R.RotateVector(FVector(-2.75f,W*.43f,H*.43f)),FVector(3.75f,W*.10f,H*.11f),R);
}
void FPopupOfficeFlood::Pause()
{
 for(auto& Slot:Slots)if(Slot.bActive)Retire(Slot,true);
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
 return SourcesHealthy && Slots.Num()<=WindowBudget+PaperBudget && FloorWindows<=Cells.Num() && FloorPapers<=PaperFloorBudget && BelowFloorPapers==0;
}
int32 FPopupOfficeFlood::Emitted(int32 Source) const{return Sources.IsValidIndex(Source)?Sources[Source].Count:0;}
FString FPopupOfficeFlood::Describe() const
{
 int32 Active=0,Total=0;for(const auto& Slot:Slots)Active+=Slot.bActive?1:0;for(const auto& Source:Sources)Total+=Source.Count;
 FString Text=FString::Printf(TEXT("Flood time=%.2f sources=%d coverage=%.1f%% ground_windows=%d/%d ground_papers=%d/384 airborne=%d pool=%d/240 total_emitted=%d recycled=%d paper_below_floor=%d max_frame=%.2fms"),
 Time,bAllSources?12:4,100.f*FloorWindows/Cells.Num(),FloorWindows,Cells.Num(),FloorPapers,Active,Slots.Num(),Total,Recycled,BelowFloorPapers,PeakFrame*1000.f);
 for(int32 I=0;I<Sources.Num();++I)Text+=FString::Printf(TEXT("\nSource%d emitted=%d"),I,Sources[I].Count);
 Text+=FString::Printf(TEXT("\nCurrent emission rate=%.2f/s"),4.f*EmissionRate(Time,false)+(bAllSources?8.f*EmissionRate(Time,true):0.f));
 return Text;
}
