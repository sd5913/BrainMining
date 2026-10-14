#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationEditorCommon.h"

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
    Deadline=FPlatformTime::Seconds()+.35;return false;
   }
   if(FPlatformTime::Seconds()<Deadline)return false;
   auto Desktop=Device->GetDesktop();
   if(!bSpawned)
   {
    Device->AddProblem();bSpawned=true;Deadline=FPlatformTime::Seconds()+.3;return false;
   }
   const TCHAR* Names[]={TEXT("Window OK"),TEXT("SystemMessage OK"),TEXT("SystemError OK"),TEXT("Robot checkbox"),TEXT("Message OK"),TEXT("Loading Cancel")};
   const int32 Before=Desktop->GetOpenCount();
   Test->TestTrue(Names[Skin],Device->ClickReplyForValidation());
   Test->TestEqual(TEXT("Exactly one real button reply removes one popup"),Desktop->GetOpenCount(),Before-1);
   Test->TestEqual(TEXT("Each art skin keeps independent close accounting"),Desktop->GetManuallyClosedCount(),Skin+1);
   ++Skin;bSpawned=false;Deadline=FPlatformTime::Seconds()+.15;
   if(Skin<6)return false;
   Device->Destroy();return true;
  }
 private:
  FAutomationTestBase* Test;
  TWeakObjectPtr<APopupOfficeDevice> Device;
  double Deadline=0;
  int32 Skin=0;
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
