#pragma once
#include "CoreMinimal.h"

/** Damped spring with small integration steps: stable during a slow mining frame. */
struct FMiningGUISpring
{
	float Value = 1.f;
	float Velocity = 0.f;
	float Target = 1.f;
	void Step(float Delta, float Stiffness, float Damping)
	{
		float Remaining = FMath::Min(Delta, 0.1f);
		while (Remaining > 0.f)
		{
			const float Dt = FMath::Min(Remaining, 1.f / 120.f);
			Velocity += ((Target - Value) * Stiffness - Velocity * Damping) * Dt;
			Value += Velocity * Dt;
			Remaining -= Dt;
		}
		if (FMath::Abs(Target - Value) < 0.0005f && FMath::Abs(Velocity) < 0.005f)
		{
			Value = Target;
			Velocity = 0.f;
		}
	}
};
