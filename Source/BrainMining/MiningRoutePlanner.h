#pragma once

#include "CoreMinimal.h"

namespace MiningRoutes
{
	struct FSettings
	{
		double Width = 500;
		double Height = 400;
		double Thickness = 100;
		double MaxSlope = 30;
		double SearchDistance = 12000;
		double DoorMargin = 50;
	};

	struct FSection
	{
		FVector Start;
		FVector End;
		FBox Bounds(double Width, double Height, double Margin = 0) const;
	};

	struct FPlan
	{
		TArray<FVector> Points;
		TArray<FVector> FaceCorners;
		TArray<TArray<FVector>> FaceSections;
		TArray<FSection> Sections;
		TArray<FBox> Pads;
		double MaxSlope = 0;
		double Length = 0;
		int32 StartWall = 0;
		int32 EndWall = 1;
	};

	struct FShapeFrame
	{
		FVector Center = FVector::ZeroVector;
		double Width = 1000;
		double Height = 1000;
	};

	bool Shape(FPlan& Plan, TArray<FShapeFrame> Frames, FString& Error);
	bool BranchAnchor(const FPlan& Plan, const FVector& Target, bool bFloorAligned, double BranchHeight, FVector& Anchor);

	FVector WallDirection(int32 Wall);
	FVector Door(const FBox& Room, int32 Wall);
	bool Direct(const FBox& First, const FBox& Second, int32 StartWall, int32 EndWall, FPlan& Plan, FString& Error);
	bool Build(const TArray<FVector>& Points, const FSettings& Settings, FPlan& Plan, FString& Error);
	bool Validate(const FPlan& Plan, const FSettings& Settings, const TArray<FBox>& Rooms, int32 First, int32 Second, const TArray<FBox>& Reserved, FString& Error);
	bool Find(const TArray<FBox>& Rooms, int32 First, int32 Second, const FSettings& Settings, const TArray<FBox>& Reserved, int32 StartWall, int32 EndWall, FPlan& Plan, FString& Error);
}
