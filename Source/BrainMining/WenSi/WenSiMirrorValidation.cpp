#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiMirrorMechanismLayout.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString AWenSiMirrorPuzzle::ValidateMirrorPuzzle()
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(!GetWorld()->IsGameWorld() || !Pawn || Exhibits.Num()!=12)return TEXT("FAIL: requires PIE with all twelve exhibits");
 TArray<FString> Results;int32 Failures=0;
 auto Check=[&](bool OK,const TCHAR* Name){Results.Add(FString(OK?TEXT("PASS: "):TEXT("FAIL: "))+Name);if(!OK)++Failures;};
 auto Tagged=[&](const TCHAR* Tag)->AActor*{for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(Tag))return *It;return nullptr;};
 auto Settle=[&](){for(int32 T=0;T<120;++T)for(auto Exhibit:Exhibits)if(Exhibit->Kind<10 || Exhibit->Kind==12)Exhibit->Tick(.05f);UpdateMechanism();};
 auto Exchange=[&](int32 A,int32 B){Thoughts[0]=Find(A);Thoughts[1]=Find(B);SwapThoughts();AdvanceSwap(SwapTotalSeconds);Settle();};
 auto Sweep=[&](FVector Start,FVector End,float Scale=1.f)
 {FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Pawn);return GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32.f*Scale,88.f*Scale),Q);};
 ResetPuzzle();Settle();
 Check(Find(1)->Word==EWenSiMirrorWord::Heavy && Find(2)->Word==EWenSiMirrorWord::Ordinary && Find(3)->Word==EWenSiMirrorWord::Floating && Find(4)->Word==EWenSiMirrorWord::Passable,TEXT("initial exhibits match realistic descriptions"));
 Check(FMath::IsNearlyEqual(Find(2)->GetActorLocation().Z-GetActorLocation().Z,Find(2)->Rest.Z-GetActorLocation().Z,1.f),TEXT("ordinary statue stays grounded"));
 Check(Sweep(RoomPoint(FVector(490,0,90)),RoomPoint(FVector(820,0,90))),TEXT("initial corridor is physically blocked"));
 Check(Sweep(RoomPoint(FVector(490,0,230)),RoomPoint(FVector(820,0,230))),TEXT("flight cannot bypass the initial stone"));
 Pawn->SetActorLocation(RoomPoint(FVector(-550,0,89)));
 Exchange(3,2);Exchange(2,0);
 Check(Find(0)->GetActorLocation().Z-GetActorLocation().Z<180.f,TEXT("floating stone cannot escape a solid cage"));
 ResetPuzzle();Pawn->SetActorLocation(RoomPoint(FVector(-550,0,89)));
 Exchange(4,1);Exchange(3,2);
 float StatueTop=-BIG_NUMBER;for(auto Part:Find(2)->Parts)if(Part->IsVisible()&&Part->GetStaticMesh()){Part->UpdateBounds();StatueTop=FMath::Max(StatueTop,float(Part->Bounds.GetBox().Max.Z));}
 Check(StatueTop<GetActorLocation().Z+420.f,TEXT("floating statue on open plinth remains below the room ceiling"));
 Exchange(2,0);
 Check(!Sweep(RoomPoint(FVector(490,0,90)),RoomPoint(FVector(820,0,90))),TEXT("passable cage plus floating stone opens actual capsule passage"));
 Exchange(1,5);
 Check(Find(5)->Word==EWenSiMirrorWord::Passable,TEXT("word from first phase opens screen"));
 Pawn->SetActorLocation(RoomPoint(FVector(1150,80,89)));Exchange(10,12);
 Check(GetPlayerWord()==EWenSiMirrorWord::Small && Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()<25.f,TEXT("mirror thinking scales the real player collision capsule"));
 Check(!Find(11) && Find(10)->Word==GetPlayerWord(),TEXT("only the exterior mirror exists and tracks the miner state"));
 Check(!Sweep(RoomPoint(FVector(1210,320,25)),RoomPoint(FVector(1280,320,25)),.28f),TEXT("small player fits the maintenance hatch"));
 Check(Sweep(RoomPoint(FVector(1210,320,89)),RoomPoint(FVector(1280,320,89))),TEXT("ordinary player cannot pass maintenance hatch"));
 // Growing inside the enclosed maintenance box must reject the entire swap.
 Pawn->SetActorLocation(RoomPoint(FVector(1340,270,25)));const auto OldBall=Find(9)->Word;Exchange(10,9);
 Check(GetPlayerWord()==EWenSiMirrorWord::Small && Find(9)->Word==OldBall,TEXT("blocked growth rejects both sides of a word exchange"));
 Check(Thoughts[0]==Find(10) && Thoughts[1]==Find(9) && !bEditing,TEXT("rejected quick swap preserves both thoughts without opening mouse editing"));
 Pawn->SetActorLocation(RoomPoint(FVector(1650,40,25)));Exchange(5,7);Exchange(5,6);
 Check(bPressure && !bPower && !bRewardUnlocked,TEXT("heavy block tips balance; balance alone does not unlock prize"));
 auto Beam=Tagged(TEXT("WenSiBalanceBeam"));auto LeftTray=Tagged(TEXT("WenSiBalanceLeftTray"));auto Counterweight=Tagged(TEXT("WenSiBalanceCounterweight"));
 Check(Beam && LeftTray && Counterweight && Beam->GetActorRotation().Pitch>12.f && FMath::IsNearlyEqual(LeftTray->GetActorLocation().Z-GetActorLocation().Z,20.f,.1f) && Counterweight->GetActorLocation().Z-GetActorLocation().Z>85.f,TEXT("heavy side visibly lowers while counterweight side rises"));
 bool CylindricalWires=true;int32 BalanceWires=0,PowerWires=0;
 for(TActorIterator<AActor> It(GetWorld());It;++It)
  if(It->ActorHasTag(TEXT("WenSiBalanceCable")) || It->ActorHasTag(TEXT("WenSiPowerCable")))
  {
   auto Mesh=It->FindComponentByClass<UStaticMeshComponent>();
   if(Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName()==TEXT("Cylinder")){CylindricalWires&=FMath::IsNearlyEqual(Mesh->GetComponentScale().X,.045f,.001f);BalanceWires+=It->ActorHasTag(TEXT("WenSiBalanceCable"))?1:0;PowerWires+=It->ActorHasTag(TEXT("WenSiPowerCable"))?1:0;}
  }
 Check(CylindricalWires && BalanceWires>=5 && PowerWires>=5 && Tagged(TEXT("WenSiWallPlug")) && Tagged(TEXT("WenSiWallOutlet")) && Tagged(TEXT("WenSiPlugSupplyCable")) && !Tagged(TEXT("WenSiCircuitBattery")),TEXT("thick cylindrical wires connect the inserted wall plug to the conductive mechanism without a battery"));
 auto Button=Tagged(TEXT("WenSiBalanceButton"));int32 RingSegments=0;FVector RingCenter=FVector::ZeroVector;
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("WenSiBalanceButtonRing"))){++RingSegments;RingCenter+=It->GetActorLocation();}
 if(RingSegments)RingCenter/=RingSegments;
 Check(Button && FVector::Dist2D(Button->GetActorLocation(),Find(6)->GetActorLocation())<.1f && FVector::Dist2D(RingCenter,Find(6)->GetActorLocation())<.1f,TEXT("button and surrounding ring are centered directly beneath the weight"));
 Check(Button && RingSegments==32 && FMath::IsNearlyEqual(Button->GetActorLocation().Z-GetActorLocation().Z,8.f,.1f),TEXT("lowered pan depresses the red cylinder surrounded by a separate low ring"));
 Exchange(8,9);Check(!bPower,TEXT("conductive ball above hopper cannot power the lower contacts"));Exchange(8,9);
 Exchange(10,9);
 Check(GetPlayerWord()==EWenSiMirrorWord::Big && Find(9)->bBallLanded,TEXT("small ball drops into tray while miner becomes big"));
 Check(Find(9)->GetActorLocation().Z-GetActorLocation().Z<=80.1f && !bPower,TEXT("landed small ball alone is not conductive"));
 Exchange(8,9);
 Check(bPower && bRewardUnlocked && !bComplete,TEXT("conductive landed ball completes both locks but does not auto-collect reward"));
 Check(Sweep(RoomPoint(FVector(2150,-350,128)),RoomPoint(FVector(2260,-350,128)),1.45f),TEXT("big miner cannot bypass the reward aperture"));
 Exchange(10,8);Check(GetPlayerWord()==EWenSiMirrorWord::Small,TEXT("small word returns from metal block to miner"));
 if(RewardShutter)RewardShutter->SetActorLocation(RoomPoint(FVector(2190,-350,112)));
 Check(!Sweep(RoomPoint(FVector(2150,-350,25)),RoomPoint(FVector(2260,-350,25)),.28f),TEXT("small miner fits the unlocked reward aperture"));
 Pawn->SetActorLocation(RoomPoint(FVector(2300,-350,25)));UpdateMechanism();Check(bComplete,TEXT("physical entry to prize alcove completes puzzle"));
 ResetPuzzle();Settle();
 bool Restored=true;for(int32 I=0;I<Exhibits.Num();++I)Restored&=Exhibits[I]->Word==InitialWords[I]&&!Exhibits[I]->bBallLanded;
 Check(Restored && !bRewardUnlocked && !bComplete && GetPlayerWord()==EWenSiMirrorWord::Ordinary && !Thoughts[0].IsValid() && !Thoughts[1].IsValid(),TEXT("reset restores every word, size, phase, landing flag, thought and lock"));
 Check(Beam && LeftTray && Counterweight && FMath::IsNearlyZero(Beam->GetActorRotation().Pitch,.1f) && FMath::IsNearlyEqual(LeftTray->GetActorLocation().Z-GetActorLocation().Z,44.f,.1f) && FMath::IsNearlyEqual(Counterweight->GetActorLocation().Z-GetActorLocation().Z,66.f,.1f),TEXT("reset returns balance beam, pans and counterweight to their initial pose"));
 Check(Button && FMath::IsNearlyEqual(Button->GetActorLocation().Z-GetActorLocation().Z,18.f,.1f),TEXT("reset releases the raised red button"));
 Pawn->SetActorLocation(RoomPoint(FVector(-550,0,89)));PC->SetControlRotation((Find(3)->GetActorLocation()-RoomPoint(FVector(-550,0,154))).Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);Tick(.016f);
 Check(!Aim.IsValid(),TEXT("remote thinking is rejected"));
 Pawn->SetActorLocation(RoomPoint(FVector(-450,-200,89)));PC->SetControlRotation((Find(3)->GetActorLocation()-RoomPoint(FVector(-450,-200,154))).Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);Tick(.016f);Think();
 Check(Thoughts[0].IsValid() && Thoughts[0]->Kind==3,TEXT("nearby aimed exhibit can be thought about"));
 for(int32 Kind:{1,7})
 {
  Thoughts[0].Reset();Thoughts[1].Reset();auto Cover=Find(Kind);Cover->Word=EWenSiMirrorWord::Passable;Cover->Apply();
  const FVector Position=Kind==1?RoomPoint(FVector(520,0,89)):RoomPoint(FVector(1130,250,89));
  const FVector Target=Kind==1?RoomPoint(FVector(590,0,240)):RoomPoint(FVector(1246,260,130));
  Pawn->SetActorLocation(Position);PC->PlayerCameraManager->UpdateCamera(.016f);FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);PC->SetControlRotation((Target-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);Tick(.016f);Think();
  Check(Aim.Get()==Cover && Thoughts[0].Get()==Cover,Kind==1?TEXT("passable first-phase cover remains aimable and thinkable"):TEXT("passable second-phase cover remains aimable and thinkable"));
  Check(Cover->Parts[0]->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore && Cover->Parts[0]->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block,TEXT("passable cover allows movement but still blocks thinking trace"));
 }
 ResetPuzzle();
 auto* Cover=Find(1);auto* Statue=Find(2);
 Pawn->SetActorLocation(Cover->GetActorLocation());
 Thoughts[0]=Cover;Thoughts[1]=Statue;
 SwapThoughts();AdvanceSwap(SwapTravelSeconds);
 Check(HasSwapCommitted() && Cover->Word==EWenSiMirrorWord::Ordinary && Statue->Word==EWenSiMirrorWord::Heavy,TEXT("hollow cover interior does not falsely block an exchange"));
 AdvanceSwap(SwapTotalSeconds);ResetPuzzle();
 Pawn->SetActorLocation(Find(0)->GetActorLocation());
 Thoughts[0]=Find(0);Thoughts[1]=Find(3);const auto StoneWord=Find(0)->Word;
 SwapThoughts();AdvanceSwap(SwapTravelSeconds);
 Check(!IsSwapping() && Find(0)->Word==StoneWord && Thoughts[0].IsValid() && Thoughts[1].IsValid(),TEXT("real solid overlap rejects atomically and restores both bubbles"));
 ResetPuzzle();Pawn->SetActorLocation(RoomPoint(FVector(-550,0,89)));
 Thoughts[0]=Find(3);Thoughts[1]=Find(2);
 SwapThoughts();AdvanceSwap(.2f);CloseThoughts();AdvanceSwap(SwapTotalSeconds);
 Check(!IsSwapping() && Find(3)->Word==EWenSiMirrorWord::Floating && Find(2)->Word==EWenSiMirrorWord::Ordinary,TEXT("closing during flight cancels the pending effect"));
 Thoughts[0]=Find(3);Thoughts[1]=Find(2);SwapThoughts();ResetPuzzle();AdvanceSwap(SwapTotalSeconds);
 Check(!IsSwapping() && Find(3)->Word==EWenSiMirrorWord::Floating && !Thoughts[0].IsValid(),TEXT("reset cancels pending exchange and bubbles"));
 Thoughts[0]=Find(3);Thoughts[1]=Find(2);SwapThoughts();SetAreaActive(false);AdvanceSwap(SwapTotalSeconds);
 Check(!IsSwapping() && Find(3)->Word==EWenSiMirrorWord::Floating,TEXT("leaving the room cancels pending exchange"));
 SetAreaActive(true);ResetPuzzle();
 const FString Summary=FString::Printf(TEXT("%s: %d checks, %d failures"),Failures?TEXT("FAIL"):TEXT("PASS"),Results.Num(),Failures);Results.Insert(Summary,0);
 FFileHelper::SaveStringArrayToFile(Results,*(FPaths::ProjectSavedDir()/TEXT("Validation/WenSiMirror/runtime_checks.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 for(auto& Line:Results)UE_LOG(LogTemp,Display,TEXT("WenSiMirror %s"),*Line);return Summary;
}
