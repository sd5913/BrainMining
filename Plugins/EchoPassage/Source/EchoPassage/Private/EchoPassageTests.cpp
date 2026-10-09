#include "EchoPassageLayout.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoRouteTest,"EchoPassage.Route.SingleConnectedPath",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEchoRouteTest::RunTest(const FString& Parameters)
{
    const auto Cells=EchoPassageLayout::Cells();TSet<FIntPoint> Unique;
    for(int i=0;i<Cells.Num();++i)
    {
        TestFalse(TEXT("No overlapping cells"),Unique.Contains(Cells[i]));Unique.Add(Cells[i]);
        int Degree=0;for(const auto& C:Cells)if(FMath::Abs(C.X-Cells[i].X)+FMath::Abs(C.Y-Cells[i].Y)==1)++Degree;
        TestEqual(TEXT("No branch or shortcut"),Degree,(i==0||i==Cells.Num()-1)?1:2);
        if(i>0)TestEqual(TEXT("Consecutive route cells touch"),FMath::Abs(Cells[i].X-Cells[i-1].X)+FMath::Abs(Cells[i].Y-Cells[i-1].Y),1);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoPulseTest,"EchoPassage.Echo.ArrivalFadeAndRange",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEchoPulseTest::RunTest(const FString& Parameters)
{
    using namespace EchoPassageLayout;
    TestEqual(TEXT("Not revealed before wave arrives"),Visibility(.3f,900,1500,1000,2.6f),0.f);
    TestTrue(TEXT("Wave reaches distant wall"),Visibility(1.f,900,1500,1000,2.6f)>0);
    TestEqual(TEXT("Outside range remains dark"),Visibility(2.f,1600,1500,1000,2.6f),0.f);
    TestEqual(TEXT("Contour disappears"),Visibility(4.f,900,1500,1000,2.6f),0.f);
    TestTrue(TEXT("Residual fades"),Visibility(1.f,900,1500,1000,2.6f)>Visibility(2.f,900,1500,1000,2.6f));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoMazeTest,"EchoPassage.Route.EasyMaze",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEchoMazeTest::RunTest(const FString& Parameters)
{
    using namespace EchoPassageLayout;
    const auto Route=MainRoute(true),Grid=Cells(true);
    auto Adjacent=[&](FIntPoint P){TArray<FIntPoint> Out;for(const auto& Q:Grid)if(FMath::Abs(P.X-Q.X)+FMath::Abs(P.Y-Q.Y)==1)Out.Add(Q);return Out;};
    TSet<FIntPoint> Unique;int Edges=0,DeadEnds=0,Junctions=0;
    for(const auto& P:Grid)
    {
        TestFalse(TEXT("Maze cells never overlap"),Unique.Contains(P));Unique.Add(P);
        const auto N=Adjacent(P);Edges+=N.Num();
        TestTrue(TEXT("No confusing four-way junction"),N.Num()<=3);
        if(N.Num()==3)++Junctions;
        if(N.Num()==1 && P!=Route[0] && P!=Route.Last())
        {
            ++DeadEnds;int Steps=0;FIntPoint Current=P,Previous(MAX_int32,MAX_int32);
            for(int Guard=0;Guard<Grid.Num();++Guard)
            {
                const auto Neighbors=Adjacent(Current);if(Neighbors.Num()>2)break;
                FIntPoint Next=Neighbors[0];if(Next==Previous && Neighbors.Num()>1)Next=Neighbors[1];
                Previous=Current;Current=Next;++Steps;
            }
            TestTrue(TEXT("Wrong turn returns to a junction within eight metres"),Steps<=2);
        }
    }
    TMap<FIntPoint,int32> Distance;TArray<FIntPoint> Queue;Queue.Add(Route[0]);Distance.Add(Route[0],0);
    for(int Index=0;Index<Queue.Num();++Index)for(const auto& Q:Adjacent(Queue[Index]))if(!Distance.Contains(Q)){Distance.Add(Q,Distance[Queue[Index]]+1);Queue.Add(Q);}
    TestEqual(TEXT("Every corridor is reachable"),Distance.Num(),Grid.Num());
    TestEqual(TEXT("Only one forgiving loop"),Edges/2-Grid.Num()+1,1);
    TestEqual(TEXT("Two short dead ends"),DeadEnds,2);
    TestEqual(TEXT("Four simple junctions"),Junctions,4);
    TestEqual(TEXT("Main path is 96 metres"),Distance.FindRef(Route.Last())*400,9600);
    TestTrue(TEXT("Maze is longer than the original passage"),Route.Num()>MainRoute().Num());
    TestFalse(TEXT("Old exit cannot accidentally finish the maze"),IsAtExit(FVector(4100,1600,90),true));
    TestTrue(TEXT("New doorway completes maze"),IsAtExit(ExitOffset(true)+FVector(4020,1600,90),true));
    TestFalse(TEXT("Adjacent wall cannot trigger exit"),IsAtExit(ExitOffset(true)+FVector(4020,1810,90),true));
    TestFalse(TEXT("Walking above the doorway cannot trigger exit"),IsAtExit(ExitOffset(true)+FVector(4020,1600,998),true));
    return true;
}
#endif
