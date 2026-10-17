#if WITH_DEV_AUTOMATION_TESTS
#include "MiningSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSaveLifetimeTest,"BrainMining.Save.GarbageCollectionLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMiningSaveLifetimeTest::RunTest(const FString&)
{
    // Exercise the exact reference used by autosave without touching the player's slot.
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UMiningSaveSubsystem> Save(NewObject<UMiningSaveSubsystem>(Instance.Get()));
    Save->SaveData=NewObject<UMiningSaveGame>();
    Save->SaveData->Balance=12345;
    Save->SaveData->Cuts.AddDefaulted();
    TWeakObjectPtr<UMiningSaveGame> Data=Save->SaveData;
    for(int Pass=0;Pass<3;++Pass)
    {
        CollectGarbage(RF_NoFlags,true);
        if(!TestTrue(TEXT("Autosave object survives full garbage collection"),Data.IsValid()))return false;
        TArray<uint8> Bytes;
        TestTrue(TEXT("Retained object can still be serialized after GC"),UGameplayStatics::SaveGameToMemory(Data.Get(),Bytes));
        auto* Reloaded=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
        if(!TestNotNull(TEXT("Saved data can be read back"),Reloaded))return false;
        TestEqual(TEXT("Wallet survives collection and serialization"),Reloaded->Balance,int64(12345));
        TestEqual(TEXT("Mining journal survives collection and serialization"),Reloaded->Cuts.Num(),1);
    }
    Save->SaveData=nullptr;
    CollectGarbage(RF_NoFlags,true);
    TestFalse(TEXT("Releasing the save reference permits collection"),Data.IsValid());
    return true;
}
#endif
