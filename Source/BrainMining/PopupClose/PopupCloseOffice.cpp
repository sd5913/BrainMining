#include "PopupClose/PopupCloseOffice.h"
#include "PopupClose/PopupCloseLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "RenderUtils.h"
#include "UObject/ConstructorHelpers.h"

APopupCloseOffice::APopupCloseOffice()
{
 auto Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> NoGI(TEXT("/Game/PopupClose/OfficeArt/Architecture/MI_OfficeCeilingNoGI.MI_OfficeCeilingNoGI"));
 CeilingWithoutGI=NoGI.Object;
 for(int32 I=0;I<15;++I)
 {
  auto Group=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("OfficeGroup%d"),I));
  Group->SetupAttachment(Root);Group->SetStaticMesh(I==10?Cylinder.Object:(I==11?Sphere.Object:Cube.Object));
  Group->SetCollisionProfileName((I==0 || I==2 || I==3)?TEXT("BlockAll"):TEXT("NoCollision"));
  Group->SetCastShadow(I<4);Groups.Add(Group);
 }
 for(int32 X=0;X<3;++X)for(int32 Y=0;Y<3;++Y)
 {
  auto Light=CreateDefaultSubobject<URectLightComponent>(*FString::Printf(TEXT("OfficeLight%d%d"),X,Y));
  Light->SetupAttachment(Root);Light->SetRelativeLocation(PopupCloseLayout::Compress(FVector(-750+X*750,-500+Y*500,346)));
  Light->SetRelativeRotation(FRotator(-90,0,0));Light->SetMobility(EComponentMobility::Movable);
  Light->SetIntensity(6500);Light->SetAttenuationRadius(1000);Light->SetSourceWidth(130);Light->SetSourceHeight(60);
  Light->SetLightColor(FLinearColor(1.f,.94f,.84f));Light->SetCastShadows(false);CeilingLights.Add(Light);
 }
}

void APopupCloseOffice::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);BuildOffice();
}
void APopupCloseOffice::BeginPlay()
{
 Super::BeginPlay();BuildOffice();
 const auto* Lumen=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
 const auto* GI=IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
 const auto* HardwareLumen=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));
 const auto* SoftwareLumen=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.TraceMeshSDFs"));
 // This project's GI relies on hardware tracing; unsupported DX12 adapters
 // also need the fallback when mesh distance-field tracing is disabled.
 const bool bNoTracing=HardwareLumen && HardwareLumen->GetInt()!=0 && !IsRayTracingEnabled() && SoftwareLumen && SoftwareLumen->GetInt()==0;
 const bool bMissingIndirect=GetWorld()->GetFeatureLevel()<ERHIFeatureLevel::SM6 || (Lumen && Lumen->GetInt()==0) || (GI && GI->GetInt()!=1) || bNoTracing;
 int32 CompatibleTiles=0;
 if(bUseImportedArchitecture && bMissingIndirect && CeilingWithoutGI)
 {
  // Only the authored office tiles in this streamed level receive the fallback.
  // Neither the imported source asset nor any tunnel/other-room material changes.
  for(TActorIterator<AActor> It(GetWorld());It;++It)
  {
   if(It->GetLevel()!=GetLevel() || !It->ActorHasTag(TEXT("OfficeAssetClose1")))continue;
   TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
   for(auto* Mesh:Meshes)
    if(Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName()==TEXT("SM_Ceiling_300a"))
    {
     Mesh->SetMaterial(0,CeilingWithoutGI);++CompatibleTiles;
    }
  }
 }
 if(bUseImportedArchitecture)UE_LOG(LogTemp,Display,TEXT("Office ceiling lighting: fallback_tiles=%d feature_level=%d ray_tracing=%d missing_indirect=%d"),CompatibleTiles,int32(GetWorld()->GetFeatureLevel()),IsRayTracingEnabled()?1:0,bMissingIndirect?1:0);
}

void APopupCloseOffice::BuildOffice()
{
 if(bReplacePerimeterWorkstations)
 {
  TInlineComponentArray<URectLightComponent*> Lights(this);
  for(auto Light:Lights)
  {
   const bool bCorner=Light->GetFName()==TEXT("OfficeLight02") || Light->GetFName()==TEXT("OfficeLight20");
   Light->SetIntensityUnits(ELightUnits::Lumens);
   Light->SetIntensity(bCorner?CornerLightLumens:InteriorLightLumens);
   Light->SetLightColor(FLinearColor::White);Light->SetUseTemperature(true);
   Light->SetTemperature(InteriorLightTemperature);Light->SetCastShadows(true);
  }
 }
 auto Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 const FLinearColor Colors[]={
  FLinearColor(.8f,.82f,.79f),FLinearColor(.12f,.19f,.24f),FLinearColor(.38f,.25f,.14f),
  FLinearColor(.065f,.08f,.1f),FLinearColor(.2f,.42f,.58f),FLinearColor(.9f,.88f,.79f),
  FLinearColor(.12f,.26f,.37f),FLinearColor(.71f,.46f,.17f),FLinearColor(.1f,.15f,.2f),
  FLinearColor(.55f,.23f,.18f),FLinearColor(.5f,.32f,.21f),FLinearColor(.12f,.31f,.16f),
  FLinearColor(.94f,.96f,1.f)
 };
 Surfaces.Reset();
 for(int32 I=0;I<Groups.Num();++I)
 {
  Groups[I]->ClearInstances();
  // Imported office art owns the visible shell. The procedural groups still
  // rebuild their instances for editor compatibility, but must not leave a
  // second, invisible collision shell inside that art.
  const bool bUseProceduralShell=!bUseImportedArchitecture;
  Groups[I]->SetVisibility(bUseProceduralShell);
  Groups[I]->SetCollisionEnabled(bUseProceduralShell && (I==0 || I==2 || I==3)?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  Groups[I]->SetGenerateOverlapEvents(bUseProceduralShell);
  auto Surface=UMaterialInstanceDynamic::Create(Parent,this);Surface->SetVectorParameterValue(TEXT("Color"),Colors[I==13?1:I==14?0:I]);
  UMaterialInterface* Override=SurfaceOverrides.IsValidIndex(I)?SurfaceOverrides[I].Get():nullptr;
  if(I==13)Override=CarpetMaterial.Get();
  if(I==14)Override=CeilingTileMaterial.Get();
  Groups[I]->SetMaterial(0,Override?Override:Surface);Surfaces.Add(Surface);
 }
 auto Box=[&](int32 Group,FVector P,FVector Size,FRotator R=FRotator::ZeroRotator)
 {
  Groups[Group]->AddInstance(FTransform(R,PopupCloseLayout::Compress(P),PopupCloseLayout::Compress(Size)/100.f));
 };
 // Keep the smaller office and bridge its doors to the existing campaign tunnels.
 // The positive-X doorway is the bomb exit, including in the standalone map.
 {
  for(float Y:{-510.f,510.f}) Box(0,FVector(1200,Y,180),FVector(20,780,360));
  Box(0,FVector(1200,0,325),FVector(20,240,70));
  for(float Side:{-1.f,1.f})
  {
   if(Side<0 && !bWalkThroughConnections)continue;
   // Box applies the 5/6 room compression: these join x=1000 to x=1200.
   Box(0,FVector(Side*1320,0,-10),FVector(240,264,20));
   Box(0,FVector(Side*1320,0,370),FVector(240,264,20));
   for(float Y:{-132.f,132.f})Box(0,FVector(Side*1320,Y,180),FVector(240,24,360));
  }
  if(!bWalkThroughConnections)
  {
   Box(0,FVector(1770,0,-10),FVector(660,264,20));
   Box(0,FVector(1770,0,370),FVector(660,264,20));
   for(float Y:{-132.f,132.f})Box(0,FVector(1770,Y,180),FVector(660,24,360));
  }
 }
 Box(0,FVector(-1200,-510,180),FVector(20,780,360));
 Box(0,FVector(-1200,510,180),FVector(20,780,360));
 Box(0,FVector(-1200,0,325),FVector(20,240,70));
 if(!bWalkThroughConnections) Box(2,FVector(-1206,0,140),FVector(16,208,280));
 for(float Y:{-104.f,104.f})Box(7,FVector(-1188,Y,145),FVector(5,6,290));
 Box(7,FVector(-1187,75,110),FVector(10,4,20));
 if(!bUseImportedArchitecture)for(float Y:{-885.f,885.f})
 {
  Box(1,FVector(0,Y,45),FVector(2380,8,90));Box(7,FVector(0,Y,93),FVector(2380,5,3));
  for(float X:{-830.f,-280.f,280.f,830.f})
  {
   Box(4,FVector(X,Y,230),FVector(420,5,170));
   for(float Offset:{-215.f,0.f,215.f})Box(3,FVector(X+Offset,Y-(Y>0?4:-4),230),FVector(5,6,178));
   for(float Z:{141.f,230.f,319.f})Box(3,FVector(X,Y-(Y>0?4:-4),Z),FVector(435,6,5));
   for(int32 Slat=0;Slat<6;++Slat)Box(5,FVector(X,Y-(Y>0?10:-10),288+Slat*5),FVector(425,9,2));
  }
 }
 if(!bUseImportedArchitecture)for(int32 X=0;X<12;++X)for(int32 Y=0;Y<9;++Y)
 {
  if(!CarpetMaterial)Box((X+Y)%2?1:8,FVector(-1100+X*200,-800+Y*200,.3),FVector(199,199,.5));
  if(!CeilingTileMaterial)Box(14,FVector(-1100+X*200,-800+Y*200,357),FVector(196,196,2));
 }
 if(CarpetMaterial && !bUseImportedArchitecture)Box(13,FVector(0,0,.3f),FVector(2380,1780,.5f));
 if(CeilingTileMaterial && !bUseImportedArchitecture)
  for(int32 X=0;X<33;++X)for(int32 Y=0;Y<25;++Y)
   Box(14,FVector(-1152+X*72,-864+Y*72,357),FVector(71.8f,71.8f,2));
 if(!bUseImportedArchitecture)for(float X:{-750.f,0.f,750.f})for(float Y:{-500.f,0.f,500.f})
 {
  Box(3,FVector(X,Y,352),FVector(148,78,6));Box(12,FVector(X,Y,348),FVector(138,68,2));
 }
 auto Chair=[&](FVector P,float Yaw)
 {
  if(bReplacePerimeterWorkstations)return;
  const FRotator R(0,Yaw,0);
  auto Part=[&](int32 G,FVector L,FVector Size){Box(G,P+R.RotateVector(L),Size,R);};
  Part(3,FVector(0,0,48),FVector(46,46,10));Part(6,FVector(0,0,54),FVector(43,43,4));
  Part(3,FVector(22,0,77),FVector(8,46,52));Part(6,FVector(17,0,79),FVector(3,40,40));
  Part(3,FVector(0,0,24),FVector(7,7,42));
  Part(3,FVector(0,0,5),FVector(58,8,7));Part(3,FVector(0,0,5),FVector(8,58,7));
  for(float Y:{-27.f,27.f}){Part(3,FVector(4,Y,64),FVector(35,5,5));Part(3,FVector(14,Y,56),FVector(5,5,17));}
 };
 auto Desk=[&](FVector P,float Yaw)
 {
  const FRotator R(0,Yaw,0);
  auto Part=[&](int32 G,FVector L,FVector Size){Box(G,P+R.RotateVector(L),Size,R);};
  Part(2,FVector(0,0,76),FVector(150,78,7));
  for(float X:{-62.f,62.f})for(float Y:{-27.f,27.f})Part(3,FVector(X,Y,36),FVector(6,6,72));
  Part(1,FVector(52,0,139),FVector(7,153,115));
  Part(3,FVector(23,0,115),FVector(7,55,38));Part(4,FVector(18.5f,0,116),FVector(1,47,29));
  Part(3,FVector(23,0,90),FVector(5,5,20));Part(3,FVector(20,0,81),FVector(22,17,3));
  Part(3,FVector(-20,0,82),FVector(30,13,3));Part(3,FVector(-20,23,82),FVector(10,6,3));
  Part(5,FVector(-37,-22,82),FVector(26,21,3));Part(9,FVector(-38,-22,84),FVector(24,19,1));
  Part(3,FVector(47,52,38),FVector(28,18,65));
  for(int32 Book=0;Book<5;++Book)Part(Book%2?5:9,FVector(10+Book*7,-25,87),FVector(5,17,17+Book%3*2));
  Box(10,P+R.RotateVector(FVector(-38,26,87)),FVector(8,8,12),R);
  Chair(P+R.RotateVector(FVector(-95,0,0)),Yaw);
 };
 if(!bReplacePerimeterWorkstations)
  for(float X:{-820.f,-370.f,370.f,820.f})for(float Y:{-620.f,620.f})Desk(FVector(X,Y,0),Y<0?-90:90);
 // Meeting area and office services stay clear of the central interaction and spray corridor.
 if(!bReplacePerimeterWorkstations)
 {
 Box(2,FVector(740,-170,76),FVector(220,105,8));
 for(float X:{660.f,820.f})Box(3,FVector(X,-170,36),FVector(10,65,72));
 for(float X:{660.f,820.f})for(float Y:{-265.f,-75.f})Chair(FVector(X,Y,0),Y<-170?90:-90);
 for(int32 Paper=0;Paper<3;++Paper)Box(5,FVector(670+Paper*60,-170,82),FVector(29,22,2));
 Box(3,FVector(1130,290,80),FVector(110,120,160));
 for(int32 Row=0;Row<4;++Row)
 {
  Box(0,FVector(1072,290,25+Row*38),FVector(4,112,34));
  Box(7,FVector(1068,290,30+Row*38),FVector(4,25,4));
 }
 // The two functioning printers are separate PopupOfficeDevice actors.
 // Bookshelf, notice board, coffee station, bins, plants and desktop clutter.
 Box(2,FVector(1125,-420,136),FVector(110,270,272));
 for(int32 Shelf=0;Shelf<4;++Shelf)
 {
  Box(8,FVector(1068,-420,42+Shelf*63),FVector(7,250,54));
  for(int32 Book=0;Book<9;++Book)Box(Book%3==0?9:(Book%3==1?5:6),FVector(1060,-525+Book*25,33+Shelf*63),FVector(24,17,35+Book%3*5));
 }
 Box(2,FVector(-840,290,77),FVector(180,72,154));
 Box(0,FVector(-840,290,157),FVector(190,82,6));Box(3,FVector(-808,290,182),FVector(45,43,45));
 Box(10,FVector(-850,290,168),FVector(18,18,18));
 for(int32 Cup=0;Cup<4;++Cup)Box(10,FVector(-905+Cup*14,270,166),FVector(8,8,12));
 const float NoticeY=-500.f;
 Box(2,FVector(1185,NoticeY,236),FVector(5,200,118));Box(9,FVector(1181,NoticeY,236),FVector(2,184,103));
 for(int32 Note=0;Note<7;++Note)Box(Note%2?5:4,FVector(1178,NoticeY-74+Note%4*45,215+Note/4*40),FVector(1,30,29));
 for(FVector P:{FVector(-1070,-750,0),FVector(-1070,750,0),FVector(1050,-750,0),FVector(1030,750,0)})
 {
  Box(10,P+FVector(0,0,24),FVector(45,45,48));
  Box(2,P+FVector(0,0,62),FVector(5,5,55));
  for(int32 Leaf=0;Leaf<5;++Leaf)
   Box(11,P+FVector(FMath::Cos(Leaf*1.25f)*22,FMath::Sin(Leaf*1.25f)*22,75+Leaf*8),FVector(38,25,43),FRotator(0,Leaf*72,25));
 }
 Box(10,FVector(-780,365,25),FVector(30,30,50));Box(3,FVector(-780,365,49),FVector(26,26,4));
 }
 // Central workstation: wood slab, inset edging and powder-coated U frames.
 if(bReplacePerimeterWorkstations)return; // Detailed art actors provide the furniture; devices keep their own logic.
 Box(3,FVector(0,0,69.5f),FVector(183,93,3));
 for(float Y:{-48.f,48.f})Box(7,FVector(0,Y,73),FVector(188,1.5f,3));
 for(float X:{-93.f,93.f})Box(7,FVector(X,0,73),FVector(1.5f,96,3));
 for(float Y:{-42.f,42.f})
 {
  for(float X:{-75.f,75.f})
  {
   Box(3,FVector(X,Y,36),FVector(5,5,68));
   Box(8,FVector(X,Y,2),FVector(8,8,4));
  }
  Box(3,FVector(0,Y,5),FVector(155,6,4));
  Box(3,FVector(0,Y,67),FVector(155,5,5));
 }
 Box(3,FVector(77,0,43),FVector(3,78,46)); // Back modesty panel; front knee space remains open.
 Box(3,FVector(58,0,62),FVector(16,74,5)); // Cable tray.
 // Pedestal with three distinct drawer fronts and inset pull handles.
 Box(2,FVector(25,-34,34),FVector(49,26,66));
 for(int32 Drawer=0;Drawer<3;++Drawer)
 {
  Box(8,FVector(-.5f,-34,13+Drawer*21),FVector(1,24,20));
  Box(2,FVector(-1.3f,-34,13+Drawer*21),FVector(1,22,18));
  Box(7,FVector(-2.3f,-34,17+Drawer*21),FVector(2,10,1.5f));
 }
 // Desktop mat, individual keyboard keys, mouse pad and office supplies.
 Box(6,FVector(-15,9,77.4f),FVector(60,45,.7f));
 for(int32 Row=0;Row<4;++Row)for(int32 Key=0;Key<11;++Key)
  Box(8,FVector(-15.1f+Row*3.1f,-14.1f+Key*2.8f,81.65f),FVector(2.4f,2.2f,.4f));
 Box(5,FVector(-16.3f,0,81.7f),FVector(1.4f,12,.4f)); // Space bar.
 Box(8,FVector(-14,30,78),FVector(21,17,.5f));
 Box(3,FVector(-14,30,80),FVector(10,6,3));
 Box(7,FVector(-16,30,81.7f),FVector(2,.7f,.4f));
 Box(5,FVector(-54,-24,79),FVector(27,22,3));Box(6,FVector(-54,-24,81),FVector(26,21,1));
 Box(10,FVector(9,32,84),FVector(9,9,13));Box(8,FVector(9,32,90.7f),FVector(7,7,.5f));
 Box(7,FVector(9,38,85),FVector(5,2,6)); // Mug handle.
 Box(10,FVector(-54,33,84),FVector(8,8,13));
 for(int32 Pen=0;Pen<3;++Pen)Box(Pen==1?7:3,FVector(-56+Pen*2,33,92),FVector(1,1,16),FRotator(0,0,Pen*5-5));
 // Tower, recessed front, ventilation slots and simple cable runs below the desk.
 Box(3,FVector(35,32,31),FVector(34,18,58));
 Box(8,FVector(17.5f,32,31),FVector(1,15,53));
 for(int32 Vent=0;Vent<7;++Vent)Box(7,FVector(16.8f,32,12+Vent*3),FVector(.5f,12,.6f));
 Box(4,FVector(16.6f,32,51),FVector(.6f,2,2));
 Box(8,FVector(59,32,35),FVector(1,1,51));
 Box(8,FVector(52,32,62),FVector(14,1,1));
 Box(8,FVector(52,0,86),FVector(1,1,20));
 Box(8,FVector(55,0,67),FVector(6,1,18));
 Chair(FVector(-135,105,0),-25);
}

int32 APopupCloseOffice::GetDetailCount() const
{
 int32 Count=0;for(auto Group:Groups)Count+=Group->GetInstanceCount();return Count;
}
