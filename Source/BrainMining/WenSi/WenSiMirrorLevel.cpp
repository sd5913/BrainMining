#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiMirrorMechanismLayout.h"
#include "WenSi/WenSiPuzzle.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

void AWenSiMirrorPuzzle::CreateMirrorLevel()
{
 const FString Map=TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum");
 if(!GEditor || GEditor->PlayWorld || FPackageName::DoesPackageExist(Map)){UE_LOG(LogTemp,Warning,TEXT("WenSi mirror creation refused: PIE or existing map"));return;}
 UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return;
 auto Material=[](const TCHAR* Name){return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name));};
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto Cream=Material(TEXT("MI_Limestone"));auto Dark=Material(TEXT("MI_Charcoal"));auto Brass=Material(TEXT("MI_Brass"));auto Blue=Material(TEXT("MI_DeepBlue"));auto Tile=Material(TEXT("MI_FloorStone"));auto LightTile=Material(TEXT("MI_LightStone"));auto Glass=Material(TEXT("M_DisplayGlass"));
 int32 Index=0;
 auto Block=[&](const TCHAR* Name,FVector P,FVector Size,UMaterialInterface* Mat,bool Solid=true,UStaticMesh* Mesh=nullptr)->AStaticMeshActor*
 {
  auto Actor=World->SpawnActor<AStaticMeshActor>();Actor->SetActorLabel(FString::Printf(TEXT("%s_%03d"),Name,++Index));Actor->SetFolderPath(TEXT("MuseumMirror/Architecture"));
  auto Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Mesh?Mesh:Cube);Component->SetMaterial(0,Mat);Component->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));Component->SetCastShadow(Solid);Actor->SetActorLocation(P);Actor->SetActorScale3D(Size/100.f);return Actor;
 };
 auto Room=[&](float Center,float Length)
 {
  Block(TEXT("Floor"),FVector(Center,0,-15),FVector(Length,1320,30),Tile);
  Block(TEXT("Ceiling"),FVector(Center,0,430),FVector(Length,1320,20),Dark);
  for(float Y:{-660.f,660.f})
  {
   Block(TEXT("Wall"),FVector(Center,Y,210),FVector(Length,20,420),Cream);
   Block(TEXT("Wainscot"),FVector(Center,Y+(Y<0?12:-12),55),FVector(Length-20,8,110),Blue,false);
   Block(TEXT("BrassTrim"),FVector(Center,Y+(Y<0?18:-18),114),FVector(Length-20,5,5),Brass,false);
   Block(TEXT("Cornice"),FVector(Center,Y+(Y<0?16:-16),397),FVector(Length-20,12,16),Cream,false);
   for(float X=Center-Length*.5f+100;X<Center+Length*.5f;X+=270)
   {
    Block(TEXT("Column"),FVector(X,Y+(Y<0?18:-18),207),FVector(38,35,390),Cream);
    Block(TEXT("Frame"),FVector(X+105,Y+(Y<0?14:-14),263),FVector(135,8,138),Brass,false);
    Block(TEXT("Art"),FVector(X+105,Y+(Y<0?20:-20),263),FVector(120,4,124),Dark,false);
    auto Accent=Block(TEXT("AbstractArt"),FVector(X+90,Y+(Y<0?24:-24),267),FVector(28,2,75),Cream,false);Accent->SetActorRotation(FRotator(0,0,22));
   }
  }
  for(float X=Center-Length*.5f+50;X<Center+Length*.5f;X+=100)
   for(float Y=-600;Y<=600;Y+=100)Block(TEXT("FloorTile"),FVector(X,Y,.4),FVector(99,99,.8),(FMath::RoundToInt(X/100)+FMath::RoundToInt(Y/100))%2?Tile:LightTile,false);
 };
 Room(-225,1350);Room(1725,1450);
 Block(TEXT("BackWall"),FVector(-910,0,210),FVector(20,1340,420),Cream);
 Block(TEXT("EndWall"),FVector(2460,0,210),FVector(20,1340,420),Cream);
 for(float X:{450.f,1000.f})
 {
  for(float Y:{-390.f,390.f})Block(TEXT("Partition"),FVector(X,Y,210),FVector(20,540,420),Cream);
  Block(TEXT("PortalHeader"),FVector(X,0,330),FVector(20,240,180),Cream);
 }
 Block(TEXT("CorridorFloor"),FVector(725,0,-15),FVector(550,260,30),Tile);
 for(float Y:{-130.f,130.f})Block(TEXT("CorridorWall"),FVector(725,Y,210),FVector(550,20,420),Cream);
 // The stone rises into its own shaft. Low approach ceilings prevent a flying bypass.
 Block(TEXT("LowApproachCeiling"),FVector(526,0,330),FVector(152,260,180),Dark);
 Block(TEXT("LowExitCeiling"),FVector(849,0,330),FVector(302,260,180),Dark);
 Block(TEXT("LiftPocketCeiling"),FVector(650,0,430),FVector(96,260,20),Dark);

 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto Exhibit=[&](int32 Kind,FVector P,EWenSiMirrorWord Word,FRotator R=FRotator::ZeroRotator)
 {
  auto Actor=World->SpawnActor<AWenSiMirrorExhibit>(P,R,Params);Actor->Kind=Kind;Actor->Word=Word;Actor->Configure();Actor->SetActorLabel(Actor->Noun());Actor->SetFolderPath(TEXT("MuseumMirror/InteractiveExhibits"));return Actor;
 };
 Exhibit(0,FVector(650,0,95),EWenSiMirrorWord::Heavy);
 Exhibit(1,FVector(650,0,120),EWenSiMirrorWord::Heavy);
 Exhibit(2,FVector(-230,360,155),EWenSiMirrorWord::Ordinary);
 Exhibit(3,FVector(-450,-350,170),EWenSiMirrorWord::Floating);
 Exhibit(4,FVector(160,-330,88),EWenSiMirrorWord::Passable);
 Exhibit(5,FVector(970,0,118),EWenSiMirrorWord::Heavy);
 Exhibit(6,FVector(1500,-430,WenSiMuseumMechanisms::BlockRestZ),EWenSiMirrorWord::Ordinary);
 Exhibit(7,FVector(1340,320,130),EWenSiMirrorWord::Heavy);
 Exhibit(8,FVector(1340,320,115),EWenSiMirrorWord::Conductive);
 Exhibit(9,FVector(1900,120,257),EWenSiMirrorWord::Big);
 Exhibit(10,FVector(1050,-340,135),EWenSiMirrorWord::Ordinary);
 Exhibit(12,FVector(1110,480,104),EWenSiMirrorWord::Small);

 // An open plinth displays the sculpture without a glass enclosure.
 Block(TEXT("StatuePlinth"),FVector(-230,360,33),FVector(170,170,66),Blue);
 Block(TEXT("BalloonStand"),FVector(-450,-350,32),FVector(75,75,64),Blue);
 Block(TEXT("BalloonString"),FVector(-450,-350,87),FVector(1.5,1.5,46),Brass,false,Cylinder);
 Block(TEXT("SmokeStand"),FVector(160,-330,32),FVector(100,100,64),Blue);
 Block(TEXT("SmokeNozzle"),FVector(160,-330,70),FVector(36,36,14),Brass,false,Cylinder);
 Block(TEXT("SmallExhibitPlinth"),FVector(1110,480,40),FVector(80,80,80),Blue);
 Block(TEXT("MetalPlinth"),FVector(1340,320,39),FVector(70,70,78),Blue);
 // Four sloping wedges form the hopper. Its 32 cm opening only admits the small ball.
 Block(TEXT("HopperPedestal"),FVector(1900,120,16),FVector(180,180,32),Blue);
 Block(TEXT("CatchTray"),FVector(1900,120,39),FVector(160,160,10),Brass);
 for(float X:{1815.f,1985.f})for(float Y:{35.f,205.f})Block(TEXT("HopperLeg"),FVector(X,Y,118),FVector(10,10,160),Brass);
 for(int32 I=0;I<4;++I)
 {
  FVector P(1900,120,195);P[I<2?0:1]+=(I%2?1:-1)*53.f;
  auto Wedge=Block(TEXT("HopperSlope"),P,I<2?FVector(74,180,12):FVector(32,74,12),Brass);Wedge->SetActorRotation(I<2?FRotator(I%2?22:-22,0,0):FRotator(0,0,I%2?-22:22));
 }
 for(float Y:{92.f,148.f})Block(TEXT("CircuitContact"),FVector(1900,Y,80),FVector(22,8,64),Brass);
 WenSiMuseumMechanisms::Build(World);
 for(int32 I=0;I<2;++I)
 {
  auto Lamp=Block(TEXT("LockLamp"),FVector(2180,I?-305.f:-395.f,145),FVector(22),Material(TEXT("MI_Terracotta")),false,Sphere);Lamp->Tags.Add(I?TEXT("MirrorPowerLamp"):TEXT("MirrorPressureLamp"));
  Lamp->GetStaticMeshComponent()->SetOverlayMaterial(Material(TEXT("M_InteractiveRim")));
 }
 // Fully enclosed alcove: flight cannot bypass the 40 x 64 cm reward aperture.
 for(float Y:{-490.f,-210.f})Block(TEXT("RewardSide"),FVector(2320,Y,210),FVector(260,20,420),Cream);
 for(float Y:{-427.f,-273.f})Block(TEXT("RewardFront"),FVector(2190,Y,210),FVector(18,114,420),Blue);
 Block(TEXT("RewardHeader"),FVector(2190,-350,242),FVector(18,40,356),Blue);
 auto Shutter=Block(TEXT("RewardShutter"),FVector(2190,-350,32),FVector(12,40,64),Brass);Shutter->Tags.Add(TEXT("MirrorRewardShutter"));
 auto Prize=Block(TEXT("RewardCrystal"),FVector(2300,-350,32),FVector(30,30,40),Brass,false,Sphere);Prize->Tags.Add(TEXT("MirrorReward"));
 Prize->GetStaticMeshComponent()->SetOverlayMaterial(Material(TEXT("M_InteractiveRim")));
 Block(TEXT("RewardPlinth"),FVector(2300,-350,8),FVector(60,60,16),Blue);
 for(FVector P:{FVector(-720,300,0),FVector(1060,150,0)})
 {auto Reset=World->SpawnActor<AWenSiResetButton>(P,FRotator::ZeroRotator,Params);Reset->Configure();Reset->SetActorLabel(TEXT("重置按钮"));Reset->SetFolderPath(TEXT("MuseumMirror/Controls"));}
 // Benches and noninteractive exhibits enrich the room without adding written hints.
 for(FVector P:{FVector(-570,490,0),FVector(1670,510,0)})
 {
  Block(TEXT("BenchSeat"),P+FVector(0,0,47),FVector(170,52,14),Material(TEXT("MI_Walnut")));
  for(float X:{-65.f,65.f})Block(TEXT("BenchLeg"),P+FVector(X,0,20),FVector(12,40,40),Brass);
 }
 for(FVector P:{FVector(-710,-550,0),FVector(350,530,0),FVector(2050,500,0)})
 {
  Block(TEXT("DecorPlinth"),P+FVector(0,0,50),FVector(80,80,100),Blue);
  auto Art=Block(TEXT("DecorSculpture"),P+FVector(0,0,140),FVector(42,42,70),Cream);Art->SetActorRotation(FRotator(0,35,15));
 }
 for(FVector P:{FVector(-650,-200,355),FVector(150,200,355),FVector(1220,-150,355),FVector(1950,240,355)})
 {
  auto A=World->SpawnActor<AActor>();auto Light=NewObject<UPointLightComponent>(A);A->SetRootComponent(Light);A->AddInstanceComponent(Light);Light->SetMobility(EComponentMobility::Movable);Light->SetIntensity(5200);Light->SetLightColor(FLinearColor(1,.87,.72));Light->SetAttenuationRadius(1100);Light->SetCastShadows(false);Light->RegisterComponent();A->SetActorLocation(P);A->SetFolderPath(TEXT("MuseumMirror/Lights"));
  Block(TEXT("CeilingLamp"),P+FVector(0,0,40),FVector(75,75,8),Brass,false);
 }
 auto ExposureActor=World->SpawnActor<AActor>();auto Exposure=NewObject<UPostProcessComponent>(ExposureActor);ExposureActor->SetRootComponent(Exposure);ExposureActor->AddInstanceComponent(Exposure);Exposure->bUnbound=true;Exposure->Settings.bOverride_AutoExposureMinBrightness=true;Exposure->Settings.bOverride_AutoExposureMaxBrightness=true;Exposure->Settings.AutoExposureMinBrightness=0;Exposure->Settings.AutoExposureMaxBrightness=0;Exposure->RegisterComponent();
 auto Start=World->SpawnActor<APlayerStart>(FVector(-680,0,89),FRotator::ZeroRotator,Params);Start->SetActorLabel(TEXT("PlayerStart"));
 World->SpawnActor<AWenSiMirrorPuzzle>();World->GetWorldSettings()->DefaultGameMode=AWenSiMirrorGameMode::StaticClass();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,Map);
 UE_LOG(LogTemp,Display,TEXT("WenSi mirror level created: map=%s geometry=%d exhibits=12 resets=2 saved=%d"),*Map,Index,Saved);
}
static FAutoConsoleCommand CreateMirrorMuseum(TEXT("WenSi.CreateMirrorMuseum"),TEXT("Create a separate realistic-initial-state mirror and size puzzle museum"),FConsoleCommandDelegate::CreateStatic(&AWenSiMirrorPuzzle::CreateMirrorLevel));
static void UpgradeBalanceMechanisms()
{
 if(!GEditor || GEditor->PlayWorld)return;
 UWorld* World=GEditor->GetEditorWorldContext().World();
 if(!World)return;
 const FString Map=World->GetOutermost()->GetName();
 if(Map!=TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum") && Map!=TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1"))return;
 TArray<AActor*> Retired;
 for(TActorIterator<AActor> It(World);It;++It)
 {
  const FString Label=It->GetActorLabel();
  if(It->ActorHasTag(TEXT("WenSiMechanismRevision2")) || Label.StartsWith(TEXT("PressureBase_")) || Label.StartsWith(TEXT("PressurePlate_")) || Label.StartsWith(TEXT("PressureSpring_")) || Label.StartsWith(TEXT("PressureWire_")) || Label.StartsWith(TEXT("CircuitWire_")) || Label.StartsWith(TEXT("CircuitWireToReward_")))Retired.Add(*It);
 }
 for(auto Actor:Retired)World->EditorDestroyActor(Actor,true);
 WenSiMuseumMechanisms::Build(World);
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,Map);
 UE_LOG(LogTemp,Display,TEXT("WenSi balance upgrade: retired=%d balance=1 cylindrical_cables=3 wall_plug=1 red_button=1 saved=%d"),Retired.Num(),Saved);
}
static FAutoConsoleCommand UpgradeBalanceMuseum(TEXT("WenSi.UpgradeBalance"),TEXT("Build the balance with a raised red button, low ring and wall-plug wiring"),FConsoleCommandDelegate::CreateStatic(&UpgradeBalanceMechanisms));
#endif
