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
    Test->TestEqual(TEXT("Later work cannot remain blocked forever"),Desktop->GetFrontPopupTitle(),FString(TEXT("组队群 · 2条新消息")));
    Test->TestTrue(TEXT("Timeout never rewards or deletes unresolved work"),Device->AccountingValid() && Device->Handled()==0 && Device->Pending()==3);
    EdgePhase=2;Deadline=FPlatformTime::Seconds()+.3;return false;
   }
   if(EdgePhase==2)
   {
    Test->TestTrue(TEXT("Visible top window remains clickable after timeouts"),Device->ClickReplyForValidation());
    Test->TestEqual(TEXT("Fallback theme remains readable below later work"),Desktop->GetFrontPopupBody(),FString(TEXT("【涌现】")));
    for(float Seconds:{0.f,25.f,36.f,45.f,60.f})
    {
     Desktop->SetWorkElapsed(Seconds);Device->AddProblem();
     const int32 Phase=Desktop->GetFrontPopupPhase();
     if(Seconds==36.f)Test->TestEqual(TEXT("Mid-project connection interruption"),Desktop->GetFrontPopupBody(),FString(TEXT("正在重新连接5/5")));
     Test->TestEqual(TEXT("Round time selects the development phase"),Phase,Seconds<20?0:Seconds<40?1:Seconds<55?2:3);
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
   if(Skin==2)Test->TestEqual(TEXT("First ordinary popup starts with design"),Desktop->GetFrontPopupTitle(),FString(TEXT("组队群 · 2条新消息")));
   if(Skin<3)
   {
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("Validation/NarrativeOct27");
    IFileManager::Get().MakeDirectory(*Folder,true);
    FBufferArchive PNG;
    Test->TestTrue(TEXT("Render the visible desktop page"),FImageUtils::ExportRenderTarget2DAsPNG(Device->GetSurface()->GetRenderTarget(),PNG));
    Test->TestTrue(TEXT("Save narrative page capture"),FFileHelper::SaveArrayToFile(PNG,*(Folder/FString::Printf(TEXT("desktop-%d.png"),Skin))));
   }
   const int32 Before=Desktop->GetOpenCount();
   Test->TestEqual(TEXT("Each staged copy reaches the real desktop"),Desktop->GetFrontPopupBody(),FString(PopupCloseContent::Get(Skin).Body));
   Test->TestTrue(PopupCloseContent::Get(Skin).Title,Device->ClickReplyForValidation());
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
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(PopupOfficeArtTests::FCheckSkins(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
