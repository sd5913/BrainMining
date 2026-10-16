#include "MobileRuntimeSettings.h"

#include "HAL/Platform.h"

bool UMobileRuntimeSettings::IsMobileRuntime()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return true;
#else
    return false;
#endif
}

bool UMobileRuntimeSettings::IsMobileVerticalSliceEnabled()
{
    const UMobileRuntimeSettings* Settings = Get();
    return IsMobileRuntime() || FParse::Param(FCommandLine::Get(), TEXT("MobileVerticalSlice")) ||
        (Settings && Settings->bEnableMobileVerticalSlice);
}

const UMobileRuntimeSettings* UMobileRuntimeSettings::Get()
{
    return GetDefault<UMobileRuntimeSettings>();
}
