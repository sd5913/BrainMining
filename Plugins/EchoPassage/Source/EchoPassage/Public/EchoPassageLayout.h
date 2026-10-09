#pragma once
#include "CoreMinimal.h"
// Fixed, reviewable layouts. The short prototype remains available alongside the easy maze.
namespace EchoPassageLayout
{
    constexpr float CellSize=400.f;
    inline TArray<FIntPoint> MainRoute(bool Maze=false)
    {
        if(Maze) return {{0,0},{1,0},{2,0},{3,0},{3,1},{3,2},{3,3},
            {4,3},{5,3},{6,3},{7,3},{7,4},{7,5},{7,6},
            {8,6},{9,6},{10,6},{11,6},{11,7},{11,8},{11,9},
            {12,9},{13,9},{14,9},{15,9}};
        return {{0,0},{1,0},{2,0},{3,0},{3,1},{3,2},{4,2},{5,2},{6,2},{6,3},{6,4},{7,4},{8,4},{9,4}};
    }
    inline TArray<FIntPoint> Cells(bool Maze=false)
    {
        auto Out=MainRoute(Maze);
        if(Maze)
        {
            // Two eight-metre dead ends and one forgiving loop which rejoins the main route.
            Out.Append({{1,-1},{1,-2},{3,4},{3,5},{4,5},{5,5},{5,4},{9,5},{9,4}});
        }
        return Out;
    }
    inline FIntPoint ExitCell(bool Maze=false) {return MainRoute(Maze).Last();}
    inline FVector ExitOffset(bool Maze=false)
    {
        const auto E=ExitCell(Maze);
        return FVector((E.X-9)*CellSize,(E.Y-4)*CellSize,0);
    }
    inline bool IsAtExit(FVector Local,bool Maze=false)
    {
        const FVector P=Local-ExitOffset(Maze);
        return P.X>3920 && P.X<4550 && FMath::Abs(P.Y-1600)<130 && P.Z>0 && P.Z<250;
    }
    inline float Visibility(float Age,float Distance,float Radius,float Speed,float Fade)
    {
        const float Arrival=Age-Distance/FMath::Max(1.f,Speed);
        if(Arrival<0 || Distance>Radius || Arrival>Fade) return 0;
        return FMath::Square(1-Arrival/FMath::Max(.01f,Fade))*(FMath::Pow(FMath::Clamp(1-Distance/Radius,0.f,1.f),1.35f));
    }
}
