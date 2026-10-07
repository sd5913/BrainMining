#include "MiningDiggingSettings.h"
#include "MiningComponent.h"

void FMiningDiggingSettings::ApplyTo(UMiningComponent& Mining) const
{
	if (!bOverrideCharacter) return;
	const auto Safe = [](float Value, float Minimum, float Maximum, float Default)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Minimum, Maximum) : Default;
	};
	Mining.Reach = Safe(Reach, 50, 1000, 400);
	Mining.Radius = Safe(Radius, 25, 300, 160);
	Mining.Interval = Safe(Interval, 0.1f, 2, 0.3f);
	Mining.InsetFraction = Safe(InsetFraction, 0, 0.9f, 0.3f);
	Mining.DownwardOffset = Safe(DownwardOffset, 0, 150, 40);
	Mining.FallResetDistance = Safe(FallResetDistance, 0, 100000, 2000);
}
