#pragma once

#include "DiggableRock.h"
#include "EndingComicRock.generated.h"

namespace EndingComic
{
    UE::Geometry::FDynamicMesh3 Boulder(FVector Size,int32 Seed);
    UE::Geometry::FDynamicMesh3 Volume(FVector Size);
    UE::Geometry::FDynamicMesh3 Tunnel(int32 Stage);
    void InkFacets(UE::Geometry::FDynamicMesh3& Mesh);
}

/** Local art treatment for the ending. The campaign's rock and drill stay unchanged. */
UCLASS(NotBlueprintable,Transient)
class BRAINMINING_API AEndingComicRock : public ADiggableRock
{
    GENERATED_BODY()
public:
    AEndingComicRock();
    int32 VisualSeed=0;
    bool bContinuousVolume=false;
    TWeakObjectPtr<AEndingComicRock> Previous,Next;
    virtual void ResetRock() override;
    virtual bool MineSphere(FVector WorldCenter,float WorldRadius) override;
    virtual bool MinePickaxe(FTransform WorldFrame,FVector HalfSize,int32 Seed,float Roughness) override;
private:
    bool CutLocal(FVector Center,float Radius,const UE::Geometry::FDynamicMesh3& Cutter);
};
