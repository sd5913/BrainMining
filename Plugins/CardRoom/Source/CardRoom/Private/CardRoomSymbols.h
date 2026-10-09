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
