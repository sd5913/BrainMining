#include "WenSi/WenSiPuzzle.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

void AWenSiPuzzle::UpgradeMuseumCircuit()
{
 if(!GEditor || GEditor->PlayWorld)return;
 UWorld* World=GEditor->GetEditorWorldContext().World();
 if(!World || World->GetOutermost()->GetName()!=TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"))return;
 AWenSiPuzzle* Puzzle=nullptr;
 for(TActorIterator<AWenSiPuzzle> It(World);It;++It){Puzzle=*It;break;}
 if(!Puzzle)return;
 if(Puzzle->bCircuitPuzzle){UE_LOG(LogTemp,Display,TEXT("WenSi circuit upgrade: already installed"));return;}
 auto Material=[](const TCHAR* Name)
 {
  const FString Path=FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name;
  return LoadObject<UMaterialInterface>(nullptr,*Path);
 };
 auto Cream=Material(TEXT("MI_Limestone"));auto Dark=Material(TEXT("MI_Charcoal"));auto Navy=Material(TEXT("MI_DeepBlue"));
 auto Brass=Material(TEXT("MI_Brass"));auto Tile=Material(TEXT("MI_FloorStone"));auto LightTile=Material(TEXT("MI_LightStone"));
 auto Glow=Material(TEXT("M_InteractiveRim"));
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 TArray<AActor*> Retired;
 for(TActorIterator<AActor> It(World);It;++It)
 {
  const FString Label=It->GetActorLabel();
  if(Label==TEXT("PitBottom"))Retired.Add(*It);
  if(auto Object=Cast<AWenSiObject>(*It))if(Object->ObjectKind==4)Retired.Add(*It);
  if(Label.StartsWith(TEXT("Museum_")) && !Label.StartsWith(TEXT("Museum_Spot_")))
  {
   const int32 Number=FCString::Atoi(*Label.Mid(7));
   if(Number>=212 && Number<=247)Retired.Add(*It);
  }
  if(Label==TEXT("PassageLeft") || Label==TEXT("PassageRight"))
   It->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(475,Label==TEXT("PassageLeft")?-130:130,135),FVector(3.5,.2,7.3)));
  if(Label==TEXT("LowCeiling"))It->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(475,0,250),FVector(3.5,2.6,.2)));
 }
 for(auto Actor:Retired)World->EditorDestroyActor(Actor,true);
 int32 Number=0;
 auto Block=[&](const TCHAR* Name,FVector Position,FVector Size,UMaterialInterface* Surface,bool bSolid=true)
 {
  auto Actor=World->SpawnActor<AStaticMeshActor>();Actor->SetActorLabel(FString::Printf(TEXT("Circuit_%s_%03d"),Name,++Number));Actor->SetFolderPath(TEXT("Museum/CircuitGallery"));Actor->Tags.Add(TEXT("WenSiCircuitDecor"));
  auto Mesh=Actor->GetStaticMeshComponent();Mesh->SetStaticMesh(Cube);Mesh->SetMaterial(0,Surface);Mesh->SetCollisionProfileName(bSolid?TEXT("BlockAll"):TEXT("NoCollision"));
  Actor->SetActorTransform(FTransform(FRotator::ZeroRotator,Position,Size/100.f));return Actor;
 };
 // Replace the pit corridor with a second, enclosed gallery. Flying cannot bypass the locks.
 Block(TEXT("Floor"),FVector(1075,0,-15),FVector(850,800,30),Tile);
 Block(TEXT("Ceiling"),FVector(1075,0,310),FVector(850,800,20),Dark);
 for(float Y:{-410.f,410.f})Block(TEXT("Wall"),FVector(1075,Y,150),FVector(850,20,300),Cream);
 for(float Y:{-270.f,270.f})Block(TEXT("EntranceShoulder"),FVector(650,Y,150),FVector(20,280,300),Cream);
 for(int32 X=0;X<8;++X)for(int32 Y=0;Y<8;++Y)
  Block(TEXT("Tile"),FVector(700+X*100,-350+Y*100,.3),FVector(99,99,.5),(X+Y)%2?Tile:LightTile,false);
 for(float Y:{-395.f,395.f})
 {
  Block(TEXT("Wainscot"),FVector(1075,Y,52),FVector(830,8,104),Navy,false);
  Block(TEXT("Trim"),FVector(1075,Y,107),FVector(830,10,5),Brass,false);
  Block(TEXT("Cornice"),FVector(1075,Y,285),FVector(830,12,14),Cream,false);
  for(float X:{730.f,1000.f,1460.f})
  {
   Block(TEXT("Frame"),FVector(X,Y+(Y<0?6:-6),210),FVector(95,6,98),Brass,false);
   Block(TEXT("Art"),FVector(X,Y+(Y<0?10:-10),210),FVector(84,3,86),Dark,false);
   auto Accent=Block(TEXT("Accent"),FVector(X-15,Y+(Y<0?13:-13),212),FVector(20,2,58),Cream,false);Accent->SetActorRotation(FRotator(0,0,20));
  }
 }
 Block(TEXT("ConductorPlinth"),FVector(840,-210,36),FVector(64,64,72),Navy);
 Block(TEXT("ConductorPlinthRim"),FVector(840,-210,76),FVector(78,78,8),Brass,false);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto Object=[&](int32 Kind,FVector Position,EWenSiRule Rule)
 {
  auto Actor=World->SpawnActor<AWenSiObject>(Position,FRotator::ZeroRotator,Params);
  Actor->Rule=Rule;Actor->HighlightMaterial=Glow;Actor->Configure(Kind);Actor->SetActorLabel(Actor->Noun());Actor->SetFolderPath(TEXT("Museum/CircuitPuzzle"));
  return Actor;
 };
 Object(5,FVector(1190,-200,65),EWenSiRule::Light);
 Object(6,FVector(1190,160,100),EWenSiRule::Heavy);
 Object(7,FVector(840,-210,140),EWenSiRule::Heavy);
 Object(8,FVector(840,-210,112.5),EWenSiRule::Conductive);
 auto Bench=World->SpawnActor<AWenSiCircuitMechanism>(FVector(1190,0,0),FRotator::ZeroRotator,Params);
 Bench->Configure();Bench->SetActorLabel(TEXT("双锁奖励展柜"));Bench->SetFolderPath(TEXT("Museum/CircuitPuzzle"));
 // Keep reset on the gallery side: donating the screen's void word must not seal it away.
 auto Button=World->SpawnActor<AWenSiResetButton>(FVector(690,90,0),FRotator::ZeroRotator,Params);
 Button->Configure();Button->SetActorLabel(TEXT("谜题重置按钮"));Button->SetFolderPath(TEXT("Museum/CircuitPuzzle"));
 auto FirstRoomButton=World->SpawnActor<AWenSiResetButton>(FVector(-520,150,0),FRotator::ZeroRotator,Params);
 FirstRoomButton->Configure();FirstRoomButton->SetActorLabel(TEXT("第一展厅重置按钮"));FirstRoomButton->SetFolderPath(TEXT("Museum/CircuitPuzzle"));
 for(FVector Position:{FVector(890,-150,265),FVector(1250,100,265)})
 {
  auto Actor=World->SpawnActor<AActor>();auto Light=NewObject<UPointLightComponent>(Actor);Actor->SetRootComponent(Light);Actor->AddInstanceComponent(Light);
  Light->SetMobility(EComponentMobility::Movable);Light->SetIntensity(5000);Light->SetAttenuationRadius(800);Light->SetLightColor(FLinearColor(1.f,.88f,.72f));Light->SetCastShadows(false);Light->RegisterComponent();Actor->SetActorLocation(Position);
  Actor->SetActorLabel(TEXT("Circuit_GalleryLight"));Actor->SetFolderPath(TEXT("Museum/CircuitGallery"));Actor->Tags.Add(TEXT("WenSiCircuitDecor"));
 }
 Puzzle->bCircuitPuzzle=true;Puzzle->bStageTwoUnlocked=false;Puzzle->Modify();World->MarkPackageDirty();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"));
 UE_LOG(LogTemp,Display,TEXT("WenSi circuit upgrade: retired=%d gallery_parts=%d interactive_objects=8 reset_buttons=2 saved=%d"),Retired.Num(),Number,Saved);
}
static void AddFirstRoomResetButton()
{
 if(!GEditor || GEditor->PlayWorld)return;
 UWorld* World=GEditor->GetEditorWorldContext().World();
 if(!World || World->GetOutermost()->GetName()!=TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"))return;
 for(TActorIterator<AWenSiResetButton> It(World);It;++It)
  if(It->GetActorLocation().X<0.f){UE_LOG(LogTemp,Display,TEXT("WenSi first-room reset: already installed"));return;}
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto Button=World->SpawnActor<AWenSiResetButton>(FVector(-520,150,0),FRotator::ZeroRotator,Params);
 if(!Button)return;
 Button->Configure();Button->SetActorLabel(TEXT("第一展厅重置按钮"));Button->SetFolderPath(TEXT("Museum/CircuitPuzzle"));
 World->MarkPackageDirty();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"));
 UE_LOG(LogTemp,Display,TEXT("WenSi first-room reset: added at (-520,150,0), saved=%d"),Saved);
}
static FAutoConsoleCommand FirstRoomReset(TEXT("WenSi.AddFirstRoomReset"),TEXT("Add the first gallery reset button to an existing circuit museum"),FConsoleCommandDelegate::CreateStatic(&AddFirstRoomResetButton));
static FAutoConsoleCommand UpgradeCircuit(TEXT("WenSi.UpgradeMuseumCircuit"),TEXT("Replace the museum bridge with the four-word circuit puzzle and a full reset button"),FConsoleCommandDelegate::CreateStatic(&AWenSiPuzzle::UpgradeMuseumCircuit));
#endif
