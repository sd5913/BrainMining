#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "InputCoreTypes.h"
#include "MiningCollection.h"
#include "Components/SceneComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"

namespace PopupOfficeArtTests
{
 class FCheckSkins final : public IAutomationLatentCommand
 {
 public:
  explicit FCheckSkins(FAutomationTestBase* InTest):Test(InTest){}
  bool Update() override
  {
   if(!Device.IsValid())
   {
    UWorld* World=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
    if(!World || !World->HasBegunPlay())
    {
     if(FPlatformTime::Seconds()-StartupTime<10.)return false;
     Test->AddError(TEXT("Office PIE did not start within ten seconds"));return true;
    }
    FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Device=World->SpawnActor<APopupOfficeDevice>(FVector(0,1000,0),FRotator::ZeroRotator,Params);
    if(!Device.IsValid()){Test->AddError(TEXT("Could not spawn isolated live desktop"));return true;}
    Device->GetSurface()->SetTickWhenOffscreen(true);Device->SetScreenInteractive(true);
    for(int32 I=0;I<PopupCloseContent::ArtCount;++I)
    {
     auto Slab=World->SpawnActor<APopupCloseFlyingWindow>(FVector(0,0,-5000-I*100),FRotator::ZeroRotator,Params);
     Slab->PrepareForPool(I);Slabs.Add(Slab);
    }
    Deadline=FPlatformTime::Seconds()+.35;return false;
   }
   for(auto Slab:Slabs)if(Slab.IsValid())Slab->IsReadyForLaunch();
   if(FPlatformTime::Seconds()<Deadline)return false;
   auto Desktop=Device->GetDesktop();
   if(EdgePhase==1)
   {
    Desktop->AdvanceIntroduction(30.f);
    Test->TestEqual(TEXT("Slow opening never opens the theme without a click"),Desktop->GetFrontPopupBody(),FString(TEXT("21天gamejam游戏创作主题公布！")));
    Desktop->AdvanceIntroduction(15.1f);
    Test->TestEqual(TEXT("Long-idle fallback opens the theme first"),Desktop->GetFrontPopupBody(),FString(TEXT("【涌现】")));
    Desktop->AdvanceIntroduction(20.1f);
    Test->TestEqual(TEXT("Later work cannot remain blocked forever"),Desktop->GetFrontPopupTitle(),FString(TEXT("组队群 · 新消息")));
    Test->TestTrue(TEXT("Timeout never rewards or deletes unresolved work"),Device->AccountingValid() && Device->Handled()==0 && Device->Pending()==3);
    EdgePhase=2;Deadline=FPlatformTime::Seconds()+.3;return false;
   }
   if(EdgePhase==2)
   {
    if(Desktop->GetFrontUnreadCount()>0)
    {
     Test->TestTrue(TEXT("First message acknowledgement stays interactive"),Device->ClickReplyForValidation());
     Test->TestEqual(TEXT("Unread notification immediately opens its message contents"),Desktop->GetFrontUnreadCount(),0);
     Deadline=FPlatformTime::Seconds()+.3;return false;
    }
    Test->TestTrue(TEXT("Visible top window remains clickable after timeouts"),Device->ClickReplyForValidation());
    Test->TestEqual(TEXT("Fallback theme remains readable below later work"),Desktop->GetFrontPopupBody(),FString(TEXT("【涌现】")));
    for(float Seconds:{0.f,25.f,36.f,52.f,65.f})
    {
     Desktop->SetWorkElapsed(Seconds);Device->AddProblem();
     const int32 Phase=Desktop->GetFrontPopupPhase();
     if(Seconds==36.f)Test->TestEqual(TEXT("Mid-project connection interruption"),Desktop->GetFrontPopupBody(),FString(TEXT("正在重新连接5/5")));
     Test->TestEqual(TEXT("Round time selects the development phase"),Phase,Seconds<30?0:Seconds<48?1:Seconds<60?2:3);
    }
    Device->ResetSession();Device->AddProblem();Device->AddProblem();Device->AddProblem();Device->SetScreenInteractive(true);
    EdgePhase=3;Deadline=FPlatformTime::Seconds()+.3;return false;
   }
   if(EdgePhase==3)
   {
    Test->TestTrue(TEXT("Early acknowledgement advances the announcement"),Device->ClickReplyForValidation());
    Desktop->AdvanceIntroduction(7.f);
    Test->TestEqual(TEXT("Theme receives its own reading interval after a click"),Desktop->GetFrontPopupBody(),FString(TEXT("【涌现】")));
    Desktop->AdvanceIntroduction(13.1f);
    Test->TestTrue(TEXT("Theme cannot block the desktop forever after an early click"),Desktop->GetFrontPopupPhase()==0);
    for(auto Slab:Slabs)if(Slab.IsValid())
    {
     auto Mesh=Slab->FindComponentByClass<UStaticMeshComponent>();auto Material=Mesh->GetMaterial(0);
     auto Resource=Material->GetMaterialResource(Slab->GetWorld()->Scene->GetShaderPlatform());
     Test->TestTrue(FString::Printf(TEXT("Physical popup %s ready (resource=%d, complete=%d, pso=%d)"),*Material->GetName(),Resource!=nullptr,Resource && Resource->IsGameThreadShaderMapComplete(),Mesh->IsPSOPrecaching()),Slab->IsReadyForLaunch());Slab->Destroy();
    }
    // Exercise the real train actor: opening must not distribute everyone
    // along the platform, and rebuilding a preview must reset the departure.
    auto Train=Device->GetWorld()->SpawnActor<AMiningCollectible>();
    Train->BuildModel(21);Train->Exposure=.4f;Train->AnimateEmergence(100.f);
    Test->TestFalse(TEXT("Opening starts with no crowd outside"),Train->EmergenceParts[0]->IsVisible());
    Train->AnimateEmergence(101.f);
    int Visible=0;for(int I=0;I<108;I+=3)Visible+=Train->EmergenceParts[I]->IsVisible()?1:0;
    Test->TestTrue(TEXT("Opening releases a small first wave, not all 36 passengers"),Visible>0 && Visible<8);
    const float FirstX=Train->EmergenceParts[0]->GetRelativeLocation().X;
    Test->TestTrue(TEXT("First passenger starts inside the doorway"),FirstX>-30.f);
    Train->AnimateEmergence(101.3f);
    Test->TestTrue(TEXT("Passengers walk through the door at the faster pace"),Train->EmergenceParts[0]->GetRelativeLocation().X<FirstX-35.f);
    for(int I=0;I<108;I+=3)
    {
     const auto P=Train->EmergenceParts[I]->GetRelativeLocation();
     if(Train->EmergenceParts[I]->IsVisible() && P.X>=-34 && P.X<=-26)
      Test->TestTrue(TEXT("Passengers cross within the door opening"),FMath::Abs(P.Y)<10);
    }
    Train->AnimateEmergence(102.f);
    Test->TestTrue(TEXT("Retiring passengers remain whole instead of separating into parts"),
     FMath::Abs(Train->EmergenceParts[0]->GetRelativeLocation().Z-Train->EmergenceParts[1]->GetRelativeLocation().Z)<2.f);
    // Sample each lane at the door and along the stream, not just total population.
    for(float Age=3.f;Age<18.f;Age+=.017f)
    {
     Train->AnimateEmergence(100.f+Age);int Moving=0;
     for(int I=0;I<108;I+=3)Moving+=Train->EmergenceParts[I]->IsVisible()?1:0;
     Test->TestTrue(TEXT("Crowd continues pouring out throughout later cycles"),Moving>=24);
     for(int Lane=0;Lane<3;++Lane)for(float Front:{-24.f,-50.f,-76.f})
     {
      bool Occupied=false;
      for(int Person=Lane;Person<36;Person+=3)
      {
       const auto Part=Train->EmergenceParts[Person*3];const float X=Part->GetRelativeLocation().X;
       Occupied|=Part->IsVisible() && X<=Front && X>=Front-16.f;
      }
      Test->TestTrue(TEXT("No lane develops a gap at the door or midway through the stream"),Occupied);
     }
    }
    Train->BuildModel(21);Train->AnimateEmergence(200.f);
    Test->TestFalse(TEXT("Rebuilt train resets the first wave"),Train->EmergenceParts[0]->IsVisible());
    Train->Destroy();
    Device->Destroy();return true;
   }
   if(!bSpawned)
   {
    // Accumulate later work before either introductory page is acknowledged.
    if(Skin==0)for(int32 I=0;I<3;++I)Device->AddProblem();
    else if(Skin>2)Device->AddProblem();
    bSpawned=true;Deadline=FPlatformTime::Seconds()+.3;return false;
   }
   if(Skin==0)Test->TestEqual(TEXT("Announcement remains in front of queued work"),Desktop->GetFrontPopupBody(),FString(TEXT("21天gamejam游戏创作主题公布！")));
   if(Skin==1)Test->TestEqual(TEXT("Large theme page comes before ordinary issues"),Desktop->GetFrontPopupBody(),FString(TEXT("【涌现】")));
   if(Skin==2)Test->TestEqual(TEXT("First ordinary popup starts with design"),Desktop->GetFrontPopupTitle(),FString(TEXT("组队群 · 新消息")));
   if(Skin<PopupCloseContent::Count)
   {
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("Validation/CollectibleIteration/Popups");
    IFileManager::Get().MakeDirectory(*Folder,true);
    FBufferArchive PNG;
    Test->TestTrue(TEXT("Render the visible desktop page"),FImageUtils::ExportRenderTarget2DAsPNG(Device->GetSurface()->GetRenderTarget(),PNG));
    Test->TestTrue(TEXT("Save narrative page capture"),FFileHelper::SaveArrayToFile(PNG,*(Folder/FString::Printf(TEXT("desktop-%d%s.png"),Skin,(PopupCloseContent::ArtVariant(Skin)==4 && Desktop->GetFrontUnreadCount()==0)?TEXT("-message"):TEXT("")))));
   }
   const int32 Before=Desktop->GetOpenCount();
   Test->TestEqual(TEXT("Each staged copy reaches the real desktop"),Desktop->GetFrontPopupBody(),PopupCloseContent::Get(Skin).Body.ToString());
   if(Desktop->GetFrontUnreadCount()>0)
   {
    Test->TestTrue(TEXT("Click one-message alert"),Device->ClickReplyForValidation());
    Test->TestEqual(TEXT("No empty desktop between notification and actual message"),Desktop->GetFrontUnreadCount(),0);
    Test->TestEqual(TEXT("Follow-up preserves the visible popup"),Desktop->GetOpenCount(),Before);
    Test->TestTrue(TEXT("Follow-up cannot award coins or lose pending work"),Device->AccountingValid());
    Deadline=FPlatformTime::Seconds()+.2;return false;
   }
   Test->TestTrue(PopupCloseContent::Get(Skin).Title.ToString(),Device->ClickReplyForValidation());
   Test->TestEqual(TEXT("Exactly one real button reply removes one popup"),Desktop->GetOpenCount(),Before-1);
   Test->TestEqual(TEXT("Each art skin keeps independent close accounting"),Desktop->GetManuallyClosedCount(),Skin+1);
   Test->TestTrue(TEXT("Narrative pages preserve device problem accounting"),Device->AccountingValid());
   ++Skin;bSpawned=false;Deadline=FPlatformTime::Seconds()+.15;
   if(Skin<PopupCloseContent::Count)return false;
   Device->ResetSession();Device->AddProblem();Device->AddProblem();Device->AddProblem();Device->SetScreenInteractive(true);
   Test->TestEqual(TEXT("Restart restores the theme announcement"),Desktop->GetFrontPopupBody(),FString(TEXT("21天gamejam游戏创作主题公布！")));
   EdgePhase=1;Deadline=FPlatformTime::Seconds()+.3;return false;
  }
 private:
  FAutomationTestBase* Test;
  TWeakObjectPtr<APopupOfficeDevice> Device;
  TArray<TWeakObjectPtr<APopupCloseFlyingWindow>> Slabs;
  double Deadline=0;
  int32 Skin=0,EdgePhase=0;
  bool bSpawned=false;
  double StartupTime=FPlatformTime::Seconds();
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficePopupArtInputTest,"BrainMining.Office.Art.PopupButtons",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOfficePopupArtInputTest::RunTest(const FString& Parameters)
{
 TestEqual(TEXT("First alert has one unread message"),PopupCloseContent::UnreadCount(0,3,2),1);
 TestEqual(TEXT("Early counts can grow to three"),PopupCloseContent::UnreadCount(10,4,2),3);
 TestEqual(TEXT("Late alerts can show 999 unread messages"),PopupCloseContent::UnreadCount(62,25,29),999);
 int32 WarningCount=0,FloodSkinMask=0;
 for(int32 I=0;I<192;++I){const int32 V=PopupCloseContent::FloodArtVariant(I);WarningCount+=V==2;FloodSkinMask|=1<<V;}
 TestEqual(TEXT("Physical warning windows reduced from 32 to 16"),WarningCount,16);
 TestEqual(TEXT("The quieter eruption still includes all six window forms"),FloodSkinMask,63);
 int32 SkinMask=0;
 for(int32 I=0;I<PopupCloseContent::Count;++I)SkinMask|=1<<PopupCloseContent::ArtVariant(I);
 TestEqual(TEXT("All six supplied popup forms are reachable"),SkinMask,63);
 TestEqual(TEXT("Slow opening preserves the planning revision before production"),PopupCloseContent::ForProgress(39.f,3),5);
 TestEqual(TEXT("Initial design document uses version 1.0"),PopupCloseContent::Get(5).Title.ToString(),FString(TEXT("策划案1.0")));
 for(int32 I=4;I<150;++I)
  for(float Seconds:{5.f,20.f,35.f,50.f,70.f})
   TestTrue(TEXT("The initial design document is not repeated in later traffic"),PopupCloseContent::ForProgress(Seconds,I)!=5);
 for(int32 I=0;I<100;++I)
  TestTrue(TEXT("Asset completion cannot appear during planning or early production"),PopupCloseContent::ForProgress(35.f,I)!=20 && PopupCloseContent::ForProgress(12.f,I)!=20);
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(PopupOfficeArtTests::FCheckSkins(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
