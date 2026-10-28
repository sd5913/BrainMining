#pragma once
#include "CoreMinimal.h"
#include "CardRoomRun.h"

// Authored polygon art shared by Slate hand cards and physical table cards.
namespace CardRoomSymbols
{
    struct FTriangle { FVector2f A,B,C; FLinearColor Color; };
    inline TArray<FTriangle> Make(ERoomCard Type)
    {
        TArray<FTriangle> Out;
        auto Fan=[&](FVector2f Center,std::initializer_list<FVector2f> Edge,FLinearColor Color)
        {
            TArray<FVector2f> P; for(auto V:Edge) P.Add(V);
            for(int32 i=0;i<P.Num();++i) Out.Add({Center,P[i],P[(i+1)%P.Num()],Color});
        };
        const FLinearColor Ink(.045f,.055f,.07f),White(1,.96f,.76f);
        if(Type==ERoomCard::Fire)
        {
            Fan({.5f,.7f},{{.12f,.66f},{.2f,.42f},{.3f,.55f},{.31f,.27f},{.56f,.02f},{.53f,.34f},{.74f,.18f},{.72f,.49f},{.86f,.43f},{.92f,.68f},{.81f,.91f},{.5f,.99f},{.24f,.91f}},Ink);
            Fan({.5f,.7f},{{.18f,.68f},{.23f,.53f},{.35f,.66f},{.37f,.3f},{.5f,.15f},{.49f,.46f},{.69f,.3f},{.67f,.64f},{.82f,.53f},{.85f,.7f},{.76f,.85f},{.5f,.93f},{.29f,.86f}},FLinearColor(.98f,.20f,.025f));
            Fan({.51f,.8f},{{.30f,.75f},{.46f,.48f},{.5f,.69f},{.66f,.50f},{.72f,.76f},{.63f,.88f},{.45f,.91f}},FLinearColor(1,.61f,.035f));
            Fan({.51f,.84f},{{.41f,.82f},{.55f,.69f},{.62f,.84f},{.51f,.92f}},White);
        }
        else if(Type==ERoomCard::Poison)
        {
            Fan({.5f,.6f},{{.5f,.02f},{.76f,.3f},{.91f,.57f},{.88f,.81f},{.68f,.97f},{.33f,.97f},{.11f,.8f},{.09f,.57f},{.26f,.30f}},Ink);
            Fan({.5f,.6f},{{.5f,.12f},{.7f,.34f},{.83f,.6f},{.8f,.77f},{.64f,.89f},{.36f,.89f},{.19f,.77f},{.17f,.6f},{.3f,.34f}},FLinearColor(.20f,.65f,.055f));
            Fan({.45f,.63f},{{.27f,.55f},{.35f,.43f},{.66f,.43f},{.75f,.56f},{.7f,.74f},{.63f,.76f},{.63f,.83f},{.37f,.83f},{.37f,.76f},{.29f,.72f}},FLinearColor(.78f,.96f,.38f));
            Fan({.38f,.6f},{{.29f,.55f},{.46f,.58f},{.43f,.68f},{.32f,.67f}},Ink);
            Fan({.64f,.6f},{{.55f,.58f},{.73f,.55f},{.7f,.67f},{.59f,.68f}},Ink);
            Out.Add({{.5f,.67f},{.44f,.75f},{.56f,.75f},Ink});
            Fan({.45f,.81f},{{.43f,.76f},{.47f,.76f},{.47f,.85f},{.43f,.85f}},Ink);
            Fan({.56f,.81f},{{.54f,.76f},{.58f,.76f},{.58f,.85f},{.54f,.85f}},Ink);
            Out.Add({{.27f,.4f},{.43f,.22f},{.36f,.45f},White});
        }
        else if(Type!=ERoomCard::Laser)
        {
            const auto Color=FCardRoomRun::CardColor(Type);
            auto Line=[&](FVector2f A,FVector2f B,float Width=.04f)
            {
                const FVector2f D=(B-A).GetSafeNormal(),N(-D.Y*Width,D.X*Width);
                Out.Add({A+N,A-N,B+N,Color});Out.Add({B+N,A-N,B-N,Color});
            };
            auto Ring=[&](FVector2f C,float R,float Begin=0.f,float End=2*PI)
            {
                for(int I=0;I<24;++I){float A=FMath::Lerp(Begin,End,I/24.f),B=FMath::Lerp(Begin,End,(I+1)/24.f);
                    Line(C+FVector2f(FMath::Cos(A),FMath::Sin(A))*R,C+FVector2f(FMath::Cos(B),FMath::Sin(B))*R,.025f);}
            };
            switch(Type)
            {
            case ERoomCard::Lightning:
                Fan({.5f,.48f},{{.62f,.04f},{.22f,.54f},{.48f,.54f},{.36f,.96f},{.81f,.39f},{.55f,.39f}},Color);break;
            case ERoomCard::Ice:
                for(int I=0;I<6;++I){const float A=I*PI/3;const FVector2f D(FMath::Cos(A),FMath::Sin(A)),N(-D.Y,D.X),C(.5f,.5f);
                    Line(C,C+D*.43f,.025f);Line(C+D*.27f,C+D*.39f+N*.1f,.02f);Line(C+D*.27f,C+D*.39f-N*.1f,.02f);}break;
            case ERoomCard::Grass:
                Fan({.5f,.5f},{{.15f,.82f},{.19f,.39f},{.45f,.16f},{.88f,.1f},{.81f,.58f},{.55f,.84f}},Color);
                Fan({.48f,.51f},{{.22f,.78f},{.68f,.3f},{.55f,.66f}},White);break;
            case ERoomCard::Wind:
                for(int I=0;I<3;++I){Line({.08f,.3f+I*.2f},{.63f,.3f+I*.2f},.025f);Ring({.63f,.2f+I*.2f},.1f,-PI,PI/2);}break;
            case ERoomCard::Water:
                Fan({.5f,.65f},{{.5f,.04f},{.82f,.55f},{.84f,.75f},{.67f,.93f},{.34f,.93f},{.16f,.75f},{.18f,.55f}},Color);Ring({.48f,.66f},.18f,.2f,2.5f);break;
            case ERoomCard::Earth:
                Out.Add({{.04f,.88f},{.38f,.14f},{.72f,.88f},Color});Out.Add({{.35f,.88f},{.72f,.3f},{.97f,.88f},Color*.7f});
                Out.Add({{.28f,.36f},{.38f,.14f},{.48f,.36f},White});break;
            case ERoomCard::Metal:
                Fan({.5f,.4f},{{.5f,.03f},{.65f,.22f},{.57f,.7f},{.43f,.7f},{.35f,.22f}},Color);Line({.25f,.7f},{.75f,.7f});Line({.5f,.7f},{.5f,.95f});break;
            case ERoomCard::Light:
                Ring({.5f,.5f},.22f);for(int I=0;I<8;++I){float A=I*PI/4;FVector2f D(FMath::Cos(A),FMath::Sin(A));Line(FVector2f(.5f)+D*.32f,FVector2f(.5f)+D*.45f,.025f);}break;
            case ERoomCard::Shadow:
                Fan({.35f,.5f},{{.64f,.07f},{.3f,.13f},{.13f,.4f},{.17f,.71f},{.42f,.9f},{.75f,.87f},{.52f,.7f},{.42f,.4f}},Color);break;
            case ERoomCard::Star:
                for(int I=0;I<10;++I){float A=I*PI/5-PI/2,B=(I+1)*PI/5-PI/2;
                    Out.Add({{.5f,.5f},FVector2f(.5f)+FVector2f(FMath::Cos(A),FMath::Sin(A))*(I%2?.2f:.46f),FVector2f(.5f)+FVector2f(FMath::Cos(B),FMath::Sin(B))*(I%2?.46f:.2f),Color});}break;
            case ERoomCard::Gravity:
                Ring({.5f,.5f},.13f);Ring({.5f,.5f},.31f);Line({.06f,.73f},{.94f,.27f},.025f);break;
            case ERoomCard::Time:
                Line({.22f,.1f},{.78f,.1f});Line({.22f,.9f},{.78f,.9f});Line({.25f,.15f},{.75f,.85f});Line({.75f,.15f},{.25f,.85f});
                Out.Add({{.5f,.55f},{.3f,.82f},{.7f,.82f},Color});break;
            case ERoomCard::Echo:
                for(int I=0;I<3;++I)Ring({.22f,.5f},.2f+I*.18f,-1.2f,1.2f);Line({.16f,.4f},{.16f,.6f});break;
            case ERoomCard::Dragon:
                Fan({.5f,.5f},{{.13f,.12f},{.42f,.31f},{.48f,.06f},{.63f,.28f},{.88f,.18f},{.79f,.55f},{.57f,.94f},{.38f,.72f},{.12f,.65f},{.29f,.45f}},Color);
                Out.Add({{.48f,.4f},{.72f,.36f},{.61f,.48f},White});break;
            default:break;
            }
        }
        else
        {
            // Three explicit facets form a zigzag energy bolt.
            const FLinearColor Blue(.025f,.35f,.62f),Cyan(.10f,.86f,1.f);
            Out.Add({{.48f,.04f},{.84f,.04f},{.19f,.60f},Ink});
            Out.Add({{.19f,.60f},{.78f,.40f},{.46f,.98f},Ink});
            Out.Add({{.49f,.10f},{.72f,.10f},{.28f,.55f},Cyan});
            Out.Add({{.28f,.55f},{.68f,.46f},{.49f,.87f},Blue});
            Out.Add({{.53f,.15f},{.48f,.48f},{.34f,.50f},White});
            Out.Add({{.53f,.52f},{.62f,.5f},{.5f,.75f},Cyan});
            Out.Add({{.04f,.25f},{.24f,.31f},{.15f,.37f},Cyan});
            Out.Add({{.8f,.68f},{.97f,.78f},{.77f,.78f},Cyan});
            Out.Add({{.1f,.79f},{.24f,.69f},{.23f,.84f},Blue});
        }
        return Out;
    }
}
