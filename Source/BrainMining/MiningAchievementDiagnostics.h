#pragma once
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Opt-in diagnostics also work in Shipping without enabling cheats or QA profiles.
inline void MiningAchievementTrace(const FString& Message)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("MiningAchievementDiagnostics"))) return;
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Diagnostics");
    IFileManager::Get().MakeDirectory(*Directory, true);
    FFileHelper::SaveStringToFile(FDateTime::UtcNow().ToIso8601() + TEXT(" ") + Message + LINE_TERMINATOR,
        *(Directory / TEXT("SteamAchievements.log")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(), FILEWRITE_Append);
}
