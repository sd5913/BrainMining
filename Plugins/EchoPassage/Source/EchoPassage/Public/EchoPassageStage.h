#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoPassageStage.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEchoPassageCompleted,FName,PassageId);
UCLASS()
class ECHOPASSAGE_API AEchoPassageStage : public AActor
{
    GENERATED_BODY()
public:
    AEchoPassageStage();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere,Category="Echo",meta=(ClampMin="300",ClampMax="4000")) float EchoRadius=1550;
    UPROPERTY(EditAnywhere,Category="Echo",meta=(ClampMin="100",ClampMax="3000")) float EchoSpeed=1000;
    UPROPERTY(EditAnywhere,Category="Echo",meta=(ClampMin=".3",ClampMax="6")) float OutlineDuration=2.6f;
    UPROPERTY(EditAnywhere,Category="Echo",meta=(ClampMin=".2",ClampMax="3")) float KnockCooldown=.65f;
    UPROPERTY(EditAnywhere,Category="Layout") bool bUseMaze=false;
    UPROPERTY(EditAnywhere,Category="Layout") bool bWalkThroughConnections=false;
    UPROPERTY(EditAnywhere,Category="Integration") bool bRoomInSameLevel=false;
    // A soft world reference preserves module independence and carries the map dependency for cooking.
    UPROPERTY(EditAnywhere,Category="Integration") TSoftObjectPtr<UWorld> DestinationLevel;
    bool IsAtExit(const FVector& WorldPosition) const;
    UPROPERTY(EditAnywhere,Category="Integration") FName PassageId=TEXT("BlindPassage_P1");
    UPROPERTY(BlueprintAssignable,Category="Integration") FEchoPassageCompleted OnPassageCompleted;
    UFUNCTION(BlueprintCallable,Category="Echo") void EmitEcho(FVector WorldLocation,bool bLanding=false);
    UFUNCTION(BlueprintCallable,Category="Echo") void ResetPassage();
    void Collapse();
    void Complete();
    bool bCollapsed=false,bLanded=false,bCompleted=false;
    int32 KnockCount=0;
    float LastEcho=-100,LandingTime=-100;
    FVector GetEntry() const {return GetActorTransform().TransformPosition(FVector(-480,0,998));}
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Rock;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Outlines;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> WarmWood;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> WarmGlow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TrapFloor;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> EchoMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RockMaterial;
    int32 PulseIndex=0;
    void Box(UInstancedStaticMeshComponent* Mesh,FVector Position,FVector Size);
    void Line(FVector A,FVector B,float Width=1.35f);
    void OutlineBox(FVector Position,FVector Size);
};
