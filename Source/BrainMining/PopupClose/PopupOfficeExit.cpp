#include "PopupClose/PopupOfficeExit.h"
#include "Subtitles/SubtitleLibrary.h"
#include "MiningRoomAudioSubsystem.h"
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupCloseOffice.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseLayout.h"
#include "MiningBombToolComponent.h"
#include "MiningBombActor.h"
#include "MiningWalletSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "PopupClose/PopupOfficeExplosionShake.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "ProfilingDebugging/CsvProfiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeExit,Log,All);
CSV_DEFINE_CATEGORY(OfficeFinale,true);
namespace
{
 constexpr int32 BombBudget=256;
 constexpr float PickupBombScale=1.9f;
 constexpr float ExitPlane=1090.f;
 constexpr float VisualUpdateHz=30.f;
 void WarmBombRenderPool(UInstancedStaticMeshComponent* Mesh)
 {
  Mesh->SetVisibility(false);Mesh->PreAllocateInstancesMemory(BombBudget);
  TArray<FTransform> Hidden;Hidden.Init(FTransform(FQuat::Identity,FVector(0,0,-5000),FVector(.0001f)),BombBudget);
  Mesh->AddInstances(Hidden,false,false,false);
 }
}

FPopupOfficeExit::FPopupOfficeExit(APopupCloseStage& Stage):Owner(Stage)
{
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto BombMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Bomb/SM_StickyBomb.SM_StickyBomb"));
 auto Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 auto Make=[&](const TCHAR* Name,UStaticMesh* Asset,FLinearColor Color)
 {
  auto Mesh=NewObject<UStaticMeshComponent>(&Owner,Name);Owner.AddInstanceComponent(Mesh);
  Mesh->SetupAttachment(Owner.GetRootComponent());Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetStaticMesh(Asset);Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->RegisterComponent();
  if(Asset==Cube){auto Mat=UMaterialInstanceDynamic::Create(Parent,Mesh);Mat->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Mat);}
  return Mesh;
 };
 Door=Owner.ExitDoorModel;
 bool bConnected=false;for(TActorIterator<APopupCloseOffice> It(Owner.GetWorld());It;++It)
  if(FVector::DistSquared(It->GetActorLocation(),Owner.GetActorLocation())<2500.f)bConnected=It->bWalkThroughConnections;
 const float CorridorEnd=bConnected?1200.f:1750.f;
 const float CorridorLength=CorridorEnd-990.f, CorridorCenter=(CorridorEnd+990.f)*.5f;
 auto Corridor=[&](const TCHAR* Name,FVector P,FVector Size,FLinearColor Color)
 {
  auto Mesh=Make(Name,Cube,Color);SetBox(Mesh,P,Size);CorridorParts.Add(Mesh);
 };
 // The safe exit belongs to the stage, so hiding the ruined office cannot remove it.
 Corridor(TEXT("OfficeSafeExitFloor"),FVector(CorridorCenter,0,-9.7f),FVector(CorridorLength,220,20),FLinearColor(.14f,.18f,.23f));
 Corridor(TEXT("OfficeSafeExitRoof"),FVector(CorridorCenter,0,370),FVector(CorridorLength,240,20),FLinearColor(.82f,.84f,.8f));
 Corridor(TEXT("OfficeSafeExitWallL"),FVector(CorridorCenter,-110,180),FVector(CorridorLength,20,360),FLinearColor(.8f,.82f,.79f));
 Corridor(TEXT("OfficeSafeExitWallR"),FVector(CorridorCenter,110,180),FVector(CorridorLength,20,360),FLinearColor(.8f,.82f,.79f));
 auto ExitLight=NewObject<UPointLightComponent>(&Owner,TEXT("OfficeSafeExitLight"));Owner.AddInstanceComponent(ExitLight);
 ExitLight->SetupAttachment(Owner.GetRootComponent());ExitLight->SetRelativeLocation(FVector(CorridorCenter,0,310));
 ExitLight->SetMobility(EComponentMobility::Movable);ExitLight->SetIntensity(2800);ExitLight->SetAttenuationRadius(700);
 ExitLight->SetLightColor(FLinearColor(1,.9f,.72f));ExitLight->SetCastShadows(false);ExitLight->RegisterComponent();CorridorParts.Add(ExitLight);
 Shutter=Make(TEXT("OfficeEvacuationShutter"),Cube,FLinearColor(.34f,.36f,.37f));SetBox(Shutter.Get(),FVector(1015,-100,145),FVector(16,8,290));
 Shutter->SetVisibility(false);Shutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Pickup=Make(TEXT("OfficeBombSkillPickup"),BombMesh,FLinearColor::White);
 if(BombMesh)BombMeshOrigin=BombMesh->GetBounds().Origin;
 FActorSpawnParameters PickupSpawn;PickupSpawn.Owner=&Owner;PickupSpawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto PickupActor=Owner.GetWorld()->SpawnActor<AActor>(Owner.GetActorLocation(),Owner.GetActorRotation(),PickupSpawn);
 PickupActor->Tags.Add(TEXT("BM_PlayerEffect"));PickupActor->SetActorTickEnabled(false);
 auto PickupBody=NewObject<USphereComponent>(PickupActor,TEXT("OfficeBombPickupPhysics"));PickupActor->AddInstanceComponent(PickupBody);
 PickupActor->SetRootComponent(PickupBody);PickupBody->SetWorldLocationAndRotation(Owner.GetActorTransform().TransformPosition(FVector(-4,0,150)),Owner.GetActorQuat());
 PickupBody->SetMobility(EComponentMobility::Movable);
 PickupBody->InitSphereRadius(BombMesh?FMath::Clamp(BombMesh->GetBounds().BoxExtent.GetMax()*PickupBombScale,12.f,28.f):12.f);
 PickupBody->SetCollisionProfileName(TEXT("PhysicsActor"));PickupBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 PickupBody->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);PickupBody->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 PickupBody->SetEnableGravity(true);PickupBody->BodyInstance.bUseCCD=true;
 PickupBody->SetLinearDamping(.25f);PickupBody->SetAngularDamping(1.5f);PickupBody->SetCanEverAffectNavigation(false);
 auto Bounce=NewObject<UPhysicalMaterial>(PickupBody);Bounce->Friction=.7f;Bounce->Restitution=.2f;PickupBody->SetPhysMaterialOverride(Bounce);
 PickupBody->RegisterComponent();Pickup->AttachToComponent(PickupBody,FAttachmentTransformRules::KeepRelativeTransform);
 // The single collectible keeps the full C4 detail and a larger, centered body.
 Pickup->SetRelativeScale3D(FVector(PickupBombScale));Pickup->SetRelativeLocation(-BombMeshOrigin*PickupBombScale);
 Pickup->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pickup->SetVisibility(false);
 Pickup->SetRenderCustomDepth(true);Pickup->SetCustomDepthStencilValue(2);
 auto Outline=NewObject<UPostProcessComponent>(&Owner,TEXT("OfficeBombPickupOutline"));
 Owner.AddInstanceComponent(Outline);Outline->SetupAttachment(Owner.GetRootComponent());
 Outline->bUnbound=true;Outline->bEnabled=false;Outline->Priority=50.f;
 if(auto Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionSilhouette.M_CollectionSilhouette")))
  Outline->Settings.AddBlendable(Material,1.f);
 Outline->RegisterComponent();PickupOutline=Outline;
 auto Instances=[&](const TCHAR* Name,UStaticMesh* Asset,FLinearColor Color)
 {
  auto Mesh=NewObject<UInstancedStaticMeshComponent>(&Owner,Name);Owner.AddInstanceComponent(Mesh);
  Mesh->SetupAttachment(Owner.GetRootComponent());Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Asset);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);
  if(Asset==Cube){auto Mat=UMaterialInstanceDynamic::Create(Parent,Mesh);Mat->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Mat);}
  Mesh->RegisterComponent();return Mesh;
 };
 Bombs=Instances(TEXT("OfficeFinaleBombs"),BombMesh,FLinearColor::White);
 // The crowd shares one reduced C4 asset and its materials. No per-bomb actors,
 // rigid bodies, countdown components or dynamic material instances are needed.
 if(auto FinaleMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/PopupClose/FX/SM_OfficeFinaleC4.SM_OfficeFinaleC4")))
 {
  Bombs->SetStaticMesh(FinaleMesh);Bombs->SetForcedLodModel(2);
  // Match named slots against the current full-detail C4. The reduced mesh
  // may predate the body texture, fuse lamp and display material import.
  if(BombMesh)for(int32 Slot=0;Slot<FinaleMesh->GetStaticMaterials().Num();++Slot)
  {
   const int32 SourceSlot=BombMesh->GetMaterialIndex(FinaleMesh->GetStaticMaterials()[Slot].MaterialSlotName);
   if(SourceSlot!=INDEX_NONE)Bombs->SetMaterial(Slot,BombMesh->GetMaterial(SourceSlot));
  }
 }
 Bombs->SetAffectDistanceFieldLighting(false);Bombs->SetEvaluateWorldPositionOffset(false);
 WarmBombRenderPool(Bombs.Get());BombVisuals.Reserve(BombBudget);
 Rubble=Instances(TEXT("OfficeRubble"),Cube,FLinearColor(.28f,.27f,.24f));
 DoorFragments=Instances(TEXT("OfficeDoorFragments"),Cube,FLinearColor(.23f,.27f,.29f));
 ReturnGrille=Instances(TEXT("OfficeReturnGrille"),Cube,FLinearColor(.34f,.36f,.37f));
 for(float Y:{-70.f,-35.f,0.f,35.f,70.f,100.f})ReturnGrille->AddInstance(FTransform(FRotator::ZeroRotator,FVector(1015,Y,145),FVector(.1f,.05f,2.9f)));
 for(float Z:{15.f,150.f,280.f})ReturnGrille->AddInstance(FTransform(FRotator::ZeroRotator,FVector(1015,0,Z),FVector(.1f,2.f,.06f)));
 ReturnGrille->SetVisibility(false);
 RuinBlocker=NewObject<UBoxComponent>(&Owner,TEXT("OfficeRuinBlocker"));Owner.AddInstanceComponent(RuinBlocker.Get());
 RuinBlocker->SetupAttachment(Owner.GetRootComponent());RuinBlocker->SetRelativeLocation(FVector(0,0,180));
 RuinBlocker->SetBoxExtent(FVector(1030,760,190));RuinBlocker->SetCollisionProfileName(TEXT("BlockAll"));
 RuinBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);RuinBlocker->SetHiddenInGame(true);RuinBlocker->RegisterComponent();
 for(int32 I=0;I<4;++I)
 {
  const FRotator R(0,PopupCloseLayout::DeviceYaw(I),0);
  Sources.Add(PopupCloseLayout::DevicePosition(I)+R.RotateVector(I>=2?FVector(-65,0,118):FVector(24,0,124)));
 }
 for(float X:{-820.f,-370.f,370.f,820.f})for(float Y:{-620.f,620.f})
  Sources.Add(PopupCloseLayout::Compress(FVector(X,Y,0)+FRotator(0,Y<0?-90:90,0).RotateVector(FVector(15,0,116))));
 // Generate a deterministic collapsed ceiling/furniture silhouette without Chaos bodies.
 FRandomStream Random(27031);
 for(int32 I=0;I<220;++I)
 {
  FRubble Piece;Piece.Position=FVector(Random.FRandRange(-940,940),Random.FRandRange(-700,700),Random.FRandRange(12,85));
  Piece.Size=FVector(Random.FRandRange(35,145),Random.FRandRange(30,115),Random.FRandRange(15,65));
  Piece.Rotation=FRotator(Random.FRandRange(-40,40),Random.FRandRange(-180,180),Random.FRandRange(-25,25));
  Piece.DropDelay=Random.FRandRange(.04f,.42f);Piece.DropDuration=Random.FRandRange(.85f,1.35f);RubblePieces.Add(Piece);
 }
 // A full-height heap at the doorway makes the permanent blockade visible.
 for(int32 Z=0;Z<6;++Z)for(int32 Y=0;Y<4;++Y)
 {
  FRubble Piece;Piece.Position=FVector(993+Random.FRandRange(-20,15),-82+Y*55,30+Z*56);
  Piece.Size=FVector(95,72,72);Piece.Rotation=FRotator(Random.FRandRange(-20,20),Random.FRandRange(-30,30),Random.FRandRange(-15,15));
  // Drop the doorway heap from the bottom up so the entrance visibly fills in.
  Piece.DropDelay=Z*.22f+Random.FRandRange(0.f,.08f);Piece.DropDuration=FMath::FRandRange(.75f,1.15f);RubblePieces.Add(Piece);
 }
 TArray<FTransform> HiddenRubble;HiddenRubble.Reserve(RubblePieces.Num());
 for(const auto& R:RubblePieces)HiddenRubble.Emplace(R.Rotation,R.Position+FVector(0,0,180),R.Size/100.f);
 Rubble->AddInstances(HiddenRubble,false,false,false);Rubble->SetVisibility(false);
 if(auto Instance=Owner.GetGameInstance())if(auto Wallet=Instance->GetSubsystem<UMiningWalletSubsystem>())Wallet->bBombSkillRequiresUnlock=true;
 BindPlayer();StartTimer();
 // Preload the existing skill's presentation so the finale does not synchronously load it.
 LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Bomb/FX/SM_BombPuff.SM_BombPuff"));
 LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Bomb/FX/M_BombFire.M_BombFire"));
 LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Bomb/FX/M_BombSmoke.M_BombSmoke"));
}
FPopupOfficeExit::~FPopupOfficeExit()
{
 FinishExitTurn();
 if(Owner.GetWorld())Owner.GetWorld()->GetTimerManager().ClearTimer(Timer);
 if(auto Tool=BoundTool.Get())Tool->OnBombExploded.RemoveDynamic(&Owner,&APopupCloseStage::OnPlayerBombExploded);
 for(auto Effect:Effects)if(Effect.IsValid())Effect->Destroy();
 auto PickupBody=Pickup.IsValid()?Cast<USphereComponent>(Pickup->GetAttachParent()):nullptr;
 for(auto Mesh:{Pickup.Get(),Shutter.Get()})if(Mesh)Mesh->DestroyComponent();
 if(PickupBody)if(auto PickupActor=PickupBody->GetOwner())PickupActor->Destroy();
 for(auto Mesh:{Bombs.Get(),Rubble.Get(),DoorFragments.Get(),ReturnGrille.Get()})if(Mesh)Mesh->DestroyComponent();
 if(RuinBlocker.IsValid())RuinBlocker->DestroyComponent();
 if(PickupOutline.IsValid())PickupOutline->DestroyComponent();
 for(auto Part:CorridorParts)if(Part.IsValid())Part->DestroyComponent();
}
void FPopupOfficeExit::SetBox(UStaticMeshComponent* Mesh,FVector P,FVector Size)
{if(Mesh){Mesh->SetRelativeLocation(P);Mesh->SetRelativeScale3D(Size/100.f);}}
void FPopupOfficeExit::StartTimer()
{
 LastTime=Owner.GetWorld()->GetTimeSeconds();
 Owner.GetWorld()->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([this](){Tick();}),1.f/60.f,true);
}
void FPopupOfficeExit::BindPlayer()
{
 auto PC=Owner.GetWorld()->GetFirstPlayerController();auto Pawn=PC?PC->GetPawn():nullptr;
 auto Tool=Pawn?Pawn->FindComponentByClass<UMiningBombToolComponent>():nullptr;
 if(Tool==BoundTool.Get())return;
 if(auto Previous=BoundTool.Get())Previous->OnBombExploded.RemoveDynamic(&Owner,&APopupCloseStage::OnPlayerBombExploded);
 BoundTool=Tool;
 if(Tool){Tool->RequireSkillUnlock();Tool->OnBombExploded.AddUniqueDynamic(&Owner,&APopupCloseStage::OnPlayerBombExploded);}
}
FVector FPopupOfficeExit::PickupPosition() const
{
 if(Pickup.IsValid())if(auto Body=Cast<USphereComponent>(Pickup->GetAttachParent()))return Body->GetComponentLocation();
 return Owner.GetActorTransform().TransformPosition(FVector(-4,0,150));
}
bool FPopupOfficeExit::CanCollect(APlayerController* PC) const
{
 if(Phase!=2 || !PC || !PC->GetPawn() || PC->IsPaused() || PC->IsMoveInputIgnored() || PC->IsLookInputIgnored())return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);const FVector Target=PickupPosition();
 if(FVector::DistSquared(Eye,Target)>FMath::Square(300.f) || FVector::DotProduct(View.Vector(),(Target-Eye).GetSafeNormal())<.86f)return false;
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(OfficeBombPickup),false,PC->GetPawn());
 return !Owner.GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query) || Hit.GetComponent()==Pickup.Get() || Hit.GetComponent()==Pickup->GetAttachParent();
}
bool FPopupOfficeExit::Collect(APlayerController* PC)
{
 if(!CanCollect(PC))return false;BindPlayer();auto Tool=BoundTool.Get();if(!Tool || !Tool->UnlockSkill())return false;
 Phase=3;Pickup->SetVisibility(false);Pickup->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(PickupOutline.IsValid())PickupOutline->bEnabled=false;
 UMiningRoomAudioSubsystem::Emit(&Owner,TEXT("S_BombPickup"),.6f);
 if(auto Body=Cast<USphereComponent>(Pickup->GetAttachParent())){Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
 Owner.ShowPrompt(PC,TEXT(""));UE_LOG(LogOfficeExit,Display,TEXT("Bomb skill acquired through approach/aim/E; shared unlock=%d inventory=%d"),Tool->IsSkillUnlocked(),Tool->BombCount);return true;
}
void FPopupOfficeExit::OfferBomb()
{
 if(Phase!=0)return;Phase=1;Age=0;Pickup->SetVisibility(true);
 USubtitleLibrary::PlayMainGameSubtitle(&Owner,TEXT("电脑弹窗玩法3"));
 UMiningRoomAudioSubsystem::Emit(&Owner,TEXT("S_BombOffer"),.65f);
 if(auto Body=Cast<USphereComponent>(Pickup->GetAttachParent()))
 {
  Body->SetWorldLocationAndRotation(Owner.GetActorTransform().TransformPosition(FVector(-4,0,150)),Owner.GetActorQuat(),false,nullptr,ETeleportType::TeleportPhysics);
  Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Body->SetSimulatePhysics(true);
  Body->SetMassOverrideInKg(NAME_None,1.2f,true);
  Body->SetPhysicsLinearVelocity(Owner.GetActorTransform().TransformVectorNoScale(FVector(-170,0,300)));
  Body->SetPhysicsAngularVelocityInDegrees(Owner.GetActorTransform().TransformVectorNoScale(FVector(110,180,80)));
 }
 UE_LOG(LogOfficeExit,Display,TEXT("Office eruption stopped; skill bomb physically ejected with gravity, CCD and world collision"));
}
void FPopupOfficeExit::HandleExplosion(const FVector& WorldPosition)
{
 if(Phase!=3)return;auto Tool=BoundTool.Get();
 if(!Tool || !Tool->IsSkillUnlocked() || Tool->LastExplosionTime<0 || FMath::Abs(Owner.GetWorld()->GetTimeSeconds()-Tool->LastExplosionTime)>.1f)return;
 const FVector P=Owner.GetActorTransform().InverseTransformPosition(WorldPosition);
 const FVector Closest(989,FMath::Clamp(P.Y,-98.f,98.f),FMath::Clamp(P.Z,0.f,290.f));
 if(FVector::DistSquared(P,Closest)>FMath::Square(FMath::Clamp(Tool->Settings.BlastRadius,50.f,3000.f)))return;
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(OfficeBlastDoor),false,Tool->GetOwner());
 if(Owner.GetWorld()->LineTraceSingleByChannel(Hit,WorldPosition,Owner.GetActorTransform().TransformPosition(FVector(1000,Closest.Y,Closest.Z)),ECC_Visibility,Query) && Hit.GetComponent()!=Door.Get())return;
 Phase=4;Age=0;Door->SetVisibility(false);Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 UMiningRoomAudioSubsystem::Emit(Door.Get(),TEXT("S_DoorBreak"),.65f,true);
 for(int32 I=0;I<8;++I)DoorFragments->AddInstance(FTransform(FRotator(0,I*31.f,15),FVector(935-I*6,-85+I*24,7),FVector(.5f,.35f,.08f)));
 UE_LOG(LogOfficeExit,Display,TEXT("Exit door breached by a real player bomb explosion; blast=%s"),*P.ToString());
}
void FPopupOfficeExit::Tick()
{
 const double Now=Owner.GetWorld()->GetTimeSeconds();const float Dt=FMath::Max(0.f,float(Now-LastTime));LastTime=Now;if(Dt<=0)return;
 TickExitTurn(Dt);
 BindPlayer();Age+=Dt;
 if(PickupOutline.IsValid())PickupOutline->bEnabled=Owner.bAreaActive && (Phase==1 || Phase==2);
 auto PC=Owner.GetWorld()->GetFirstPlayerController();auto Pawn=PC?PC->GetPawn():nullptr;
 if(Phase==1)
 {
  // Chaos controls its trajectory, rotation and resting position. Becoming
  // collectible must not freeze the body or teleport it onto the desk.
  if(Age>=.2f)Phase=2;
 }
 if(Phase==2 && Owner.bAreaActive && Owner.View==APopupCloseStage::EView::Free)
 {
  if(CanCollect(PC)){Owner.ShowPrompt(PC,TEXT("E 获取炸弹技能"));if(PC->WasInputKeyJustPressed(EKeys::E))Collect(PC);}
 }
 if(Pawn && Phase<5)
 {
  const FVector P=Owner.GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
  if(P.X<960 && P.X>-1000 && FMath::Abs(P.Y)<740)bInsideSeen=true;
  if(Phase==4 && bInsideSeen && bHavePosition && PreviousX<ExitPlane && P.X>=ExitPlane && FMath::Abs(P.Y)<105 && P.Z>0 && P.Z<310)SealRoom();
  PreviousX=P.X;bHavePosition=true;
 }
 if(Phase==5)
 {
  const float Duration=FMath::Max(2.f,Owner.EvacuationBombSeconds);
  EmissionDebt+=Dt*FMath::Lerp(2.f,7.f,FMath::Clamp(Age/Duration,0.f,1.f));
  int32 Rounds=0;while(EmissionDebt>=1.f && Rounds++<2){EmissionDebt-=1.f;for(int32 I=0;I<Sources.Num();++I)EmitBomb(I);}
  // Interaction still ticks at 60 Hz; only the distant cinematic meshes are
  // sampled at 30 Hz. Submit one incremental instance update, not a new proxy.
  if(!BombVisuals.IsEmpty() && FMath::FloorToInt(Age*VisualUpdateHz)>FMath::FloorToInt((Age-Dt)*VisualUpdateHz))
  {
   CSV_SCOPED_TIMING_STAT(OfficeFinale,BombInstanceUpdate);
   TArray<FTransform,TInlineAllocator<BombBudget>> Transforms;Transforms.Reserve(BombVisuals.Num());
   for(const auto& B:BombVisuals)
   {
    const float T=Now-B.Born;
    FVector P=B.Origin+B.Velocity*T+FVector(0,0,-490.f*T*T);P.X=FMath::Clamp(P.X,-960.f,960.f);P.Y=FMath::Clamp(P.Y,-700.f,700.f);P.Z=FMath::Max(14.f,P.Z);
    const FQuat R=(B.Rotation+FRotator(100*T,160*T,80*T)).Quaternion();
    Transforms.Emplace(R,P-R.RotateVector(BombMeshOrigin),FVector(1));
   }
   Bombs->BatchUpdateInstancesTransforms(0,MakeArrayView(Transforms),false,false,true);Bombs->MarkRenderInstancesDirty();
  }
  if(Age>=Duration)Collapse();
 }
 else if(Phase==6)
 {
   const float A=FMath::Clamp(Age/2.35f,0.f,1.f);
  if(A>=1.f || FMath::FloorToInt(Age*VisualUpdateHz)>FMath::FloorToInt((Age-Dt)*VisualUpdateHz))
  {
   CSV_SCOPED_TIMING_STAT(OfficeFinale,RubbleInstanceUpdate);
   TArray<FTransform,TInlineAllocator<256>> Transforms;Transforms.Reserve(RubblePieces.Num());
   for(const auto& R:RubblePieces)
   {
    const float PieceA=FMath::Clamp((Age-R.DropDelay)/FMath::Max(.1f,R.DropDuration),0.f,1.f);
    Transforms.Emplace(R.Rotation,R.Position+FVector(0,0,(1-PieceA)*(1-PieceA)*180),R.Size/100.f);
   }
   Rubble->BatchUpdateInstancesTransforms(0,MakeArrayView(Transforms),false,false,true);Rubble->MarkRenderInstancesDirty();
  }
  if(ExplosionWaves<3 && Age>=ExplosionWaves*.24f)
  {SpawnExplosion(FVector(-550+ExplosionWaves*550,(ExplosionWaves%2?250:-250),120),3.f);++ExplosionWaves;}
  if(A>=1)
  {
   bDestructionSignaled=true;Owner.GetWorld()->GetTimerManager().ClearTimer(Timer);
   Owner.GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(&Owner,[Stage=&Owner](){Stage->OnOfficeDestroyed.Broadcast();}));
  }
 }
}
void FPopupOfficeExit::FinishExitTurn()
{
 if(auto Controller=TurnController.Get())
 {
  Controller->SetIgnoreMoveInput(false);
  Controller->SetIgnoreLookInput(false);
 }
 TurnController.Reset();
}
void FPopupOfficeExit::TickExitTurn(float Dt)
{
 auto Controller=TurnController.Get();if(!Controller)return;
 ExitTurnElapsed+=Dt;
 const float Alpha=FMath::SmoothStep(0.f,1.f,FMath::Clamp(ExitTurnElapsed/.85f,0.f,1.f));
 Controller->SetControlRotation(FQuat::Slerp(ExitTurnStart.Quaternion(),ExitTurnTarget.Quaternion(),Alpha).Rotator());
 if(ExitTurnElapsed>=.85f)FinishExitTurn();
}
void FPopupOfficeExit::SealRoom()
{
 Phase=5;Age=EmissionDebt=0;Shutter->SetVisibility(true);Shutter->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);ReturnGrille->SetVisibility(true);
 USubtitleLibrary::PlayMainGameSubtitle(&Owner,TEXT("电脑弹窗玩法4"));
 UMiningRoomAudioSubsystem::Emit(Shutter.Get(),TEXT("S_Seal"),.7f,true);
 UMiningRoomAudioSubsystem::StopSource(&Owner);
 Bombs->SetVisibility(true);CSV_EVENT(OfficeFinale,TEXT("BombEruptionStart"));
 // Full-room collision prevents a flying player from returning through a different opening.
 RuinBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 if(Owner.Flood)Owner.Flood->Stop();Owner.CancelSession();
 if(auto Controller=Owner.GetWorld()->GetFirstPlayerController())
 {
  ExitTurnStart=Controller->GetControlRotation();
  const FVector Eye=Controller->GetPawn()?Controller->GetPawn()->GetPawnViewLocation():Controller->GetFocalLocation();
  ExitTurnTarget=(Owner.GetActorTransform().TransformPosition(FVector(300,0,145))-Eye).Rotation();
  TurnController=Controller;
  ExitTurnElapsed=0.f;
  Controller->SetIgnoreLookInput(true);
  Controller->SetIgnoreMoveInput(true);
  if(auto Character=Cast<ACharacter>(Controller->GetPawn()))Character->GetCharacterMovement()->StopMovementImmediately();
 }
 Owner.GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(&Owner,[Stage=&Owner](){Stage->OnOfficeEscaped.Broadcast();}));
 UE_LOG(LogOfficeExit,Display,TEXT("Player crossed exit: return path sealed, all 12 devices begin bomb eruption"));
}
void FPopupOfficeExit::EmitBomb(int32 I)
{
 FBombVisual B;B.Origin=Sources[I];B.Born=Owner.GetWorld()->GetTimeSeconds();
 const FVector Target(FMath::FRandRange(-900.f,900.f),FMath::FRandRange(-660.f,660.f),14);
 B.Velocity=(Target-B.Origin)/1.1f;B.Velocity.Z+=539.f;B.Rotation=FRotator(0,FMath::FRandRange(-180.f,180.f),0);
 const int32 Index=BombCursor++%BombBudget;
 if(BombVisuals.Num()<BombBudget)BombVisuals.Add(B);
 else BombVisuals[Index]=B;
 ++BombsEmitted;
}
void FPopupOfficeExit::SpawnExplosion(FVector P,float Scale)
{
 UMiningRoomAudioSubsystem::Emit(&Owner,TEXT("S_CardFire"),.5f,true,.7f);
 if(auto PC=Owner.GetWorld()->GetFirstPlayerController())
  if(auto Camera=PC->PlayerCameraManager) Camera->StartCameraShake(UPopupOfficeExplosionShake::StaticClass(),FMath::Clamp(Scale/3.f,.35f,1.2f));
 auto Effect=Owner.GetWorld()->SpawnActor<AMiningBombExplosion>(Owner.GetActorTransform().TransformPosition(P),FRotator::ZeroRotator);
 if(Effect){FMiningBombSettings Settings;Settings.ExplosionScale=Scale;Settings.FireStrength=.64f;Settings.SmokeStrength=.49f;Effect->Initialize(nullptr,Settings);Effects.Add(Effect);}
}
void FPopupOfficeExit::SetInteriorVisible(bool Visible)
{
 for(TActorIterator<APopupCloseOffice> It(Owner.GetWorld());It;++It)
  if(FVector::DistSquared(It->GetActorLocation(),Owner.GetActorLocation())<FMath::Square(50.f)){It->SetActorHiddenInGame(!Visible);It->SetActorEnableCollision(Visible);}
 for(auto Device:Owner.Devices)if(IsValid(Device)){Device->SetActorHiddenInGame(!Visible);Device->SetActorEnableCollision(Visible);}
 Owner.Ceiling->SetVisibility(Visible);Owner.Ceiling->SetCollisionEnabled(Visible?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 Owner.TableTop->SetVisibility(Visible);Owner.TableTop->SetCollisionEnabled(Visible?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 Owner.Keyboard->SetVisibility(Visible);Owner.RoomLight->SetIntensity(Visible?15000.f:3500.f);
}
void FPopupOfficeExit::Collapse()
{
 Phase=6;Age=0;Bombs->ClearInstances();BombVisuals.Reset();if(Owner.Flood)Owner.Flood->ClearVisuals();
 UMiningRoomAudioSubsystem::Emit(&Owner,TEXT("S_Collapse"),.85f,true);
 Bombs->SetVisibility(false);CSV_EVENT(OfficeFinale,TEXT("BombEruptionEnd"));
 SetInteriorVisible(false);Shutter->SetVisibility(false);ReturnGrille->SetVisibility(false);
 Rubble->SetVisibility(true);
 UE_LOG(LogOfficeExit,Display,TEXT("Office destroyed: bombs=%d, permanent rubble=%d, player remains outside"),BombsEmitted,RubblePieces.Num());
}
void FPopupOfficeExit::Reset()
{
 FinishExitTurn();
 Phase=BombsEmitted=BombCursor=ExplosionWaves=0;Age=EmissionDebt=0;bInsideSeen=bHavePosition=bDestructionSignaled=false;
 for(auto Effect:Effects)if(Effect.IsValid())Effect->Destroy();Effects.Reset();
 Door->SetVisibility(true);Door->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Pickup->SetVisibility(false);Pickup->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(PickupOutline.IsValid())PickupOutline->bEnabled=false;
 if(auto Body=Cast<USphereComponent>(Pickup->GetAttachParent()))
 {
  Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Body->SetWorldLocationAndRotation(Owner.GetActorTransform().TransformPosition(FVector(-4,0,150)),Owner.GetActorQuat(),false,nullptr,ETeleportType::TeleportPhysics);
 }
 Shutter->SetVisibility(false);Shutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);ReturnGrille->SetVisibility(false);
 RuinBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Bombs->ClearInstances();WarmBombRenderPool(Bombs.Get());Rubble->ClearInstances();Rubble->SetVisibility(false);
 TArray<FTransform> HiddenRubble;HiddenRubble.Reserve(RubblePieces.Num());
 for(const auto& R:RubblePieces)HiddenRubble.Emplace(R.Rotation,R.Position+FVector(0,0,180),R.Size/100.f);
 Rubble->AddInstances(HiddenRubble,false,false,false);
 DoorFragments->ClearInstances();BombVisuals.Reset();SetInteriorVisible(true);StartTimer();
}
FString FPopupOfficeExit::Describe() const
{
 return FString::Printf(TEXT("Exit phase=%d bomb_visible=%d skill_unlocked=%d door_open=%d escaped=%d ruined=%d bombs_emitted=%d bomb_instances=%d/%d rubble=%d return_blocked=%d"),
 Phase,Pickup.IsValid() && Pickup->IsVisible(),BoundTool.IsValid() && BoundTool->IsSkillUnlocked(),DoorIsOpen(),HasEscaped(),IsRuined(),BombsEmitted,Bombs.IsValid() && Bombs->IsVisible()?BombVisuals.Num():0,BombBudget,Rubble.IsValid() && Rubble->IsVisible()?Rubble->GetInstanceCount():0,RuinBlocker.IsValid() && RuinBlocker->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
}
