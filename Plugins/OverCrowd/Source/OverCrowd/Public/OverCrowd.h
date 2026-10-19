// This file is Copyright (C) 2025 Overcrowd Development Engineering Ltd, all rights reserved

#pragma once

#include "Modules/ModuleManager.h"

class FOverCrowdModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
