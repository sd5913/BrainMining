// Copyright Epic Games, Inc. All Rights Reserved.

#include "BrainMining.h"
#include "Modules/ModuleManager.h"
#include "MiningLocalizationSubsystem.h"

class FBrainMiningModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
        MiningText::RegisterTables();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FBrainMiningModule, BrainMining, "BrainMining");

DEFINE_LOG_CATEGORY(LogBrainMining)
