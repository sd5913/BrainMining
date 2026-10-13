#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningPrototypeGameMode.h"
#include "EndingEmergence.generated.h"

class ADiggableRock;
class UMiningComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class ACameraActor;
class UAudioComponent;
class UMaterialInterface;
class SEmergenceEndingView;

/** Standalone playable ending study; never changes the campaign or shared pawn defaults. */
UCLASS()
class BRAINMINING_API AEndingEmergence : public AActor
{
    GENERATED_BODY()
public:
    AEndingEmergence();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Ending") void BuildDemo();
    UFUNCTION(BlueprintCallable, Category="Ending") void RestartDemo();
    UFUNCTION(BlueprintCallable, Category="Ending") void PreviewFilm();
    UFUNCTION(BlueprintPure, Category="Ending") FVector GetGateApproach(int32 Index) const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ending", meta=(ClampMin="0",ClampMax="1")) float Weightlessness = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ending", meta=(ClampMin="0.25",ClampMax="4")) float FilmSpeed = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ending") TArray<TObjectPtr<ADiggableRock>> Gates;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") int32 GateIndex = 0;
    // -1: gameplay, 0..6: scale journey, 7: blackout, 8: eye, 9: developers, 10: end.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") int32 Shot = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") float ShotTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") bool bComplete = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") FVector DriftVelocity = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") FVector DriftOffset = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") bool bComfortMode = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") float PullbackRate = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Ending") float BloodTravel = 0;
    FString Title() const;
    FString Subtitle() const;
    FString Hint() const;
    float OverlayAlpha() const;
    float BrainLens() const;
    static float Duration(int32 Index);
private:
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Sets;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Nerves;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Sketch;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blood;
    UPROPERTY() TArray<FVector> BloodOrigins;
    UPROPERTY() TObjectPtr<USceneComponent> Universe;
    UPROPERTY() TObjectPtr<USceneComponent> Cell;
    UPROPERTY() TObjectPtr<USceneComponent> OtherCells;
    UPROPERTY() TObjectPtr<USceneComponent> EyePerson;
    UPROPERTY() TObjectPtr<USceneComponent> NightContinent;
    UPROPERTY() TObjectPtr<USceneComponent> Pencil;
    UPROPERTY() TObjectPtr<USceneComponent> Idea;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EyeLids;
    UPROPERTY() TArray<FVector> LidOrigins;
    UPROPERTY() TObjectPtr<USceneComponent> Speaker;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SpeakerMouth;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Crowd;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdHeads;
    UPROPERTY() TArray<FTransform> CrowdOrigins;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Player;
    UPROPERTY(Transient) TObjectPtr<UMiningComponent> Mining;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Ambience;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FisheyeMaterial;
    TSharedPtr<SEmergenceEndingView> View;
    bool bReady = false;
    bool bRestoring = false;
    float Age = 0;
    float BeatAt = 0;
    float PullbackBoost = 0;
    FVector TakeoffStart = FVector::ZeroVector;
    FVector TakeoffLook = FVector::ZeroVector;
    int32 LastCuts = 0;
    USceneComponent* Group(const TCHAR* Name, FVector Position, USceneComponent* Parent=nullptr);
    UStaticMeshComponent* Shape(const TCHAR* Name,FVector Position,FVector Size,FLinearColor Tint,const TCHAR* Mesh=TEXT("Sphere"),USceneComponent* Parent=nullptr,const TCHAR* Material=TEXT("Surface"),bool Collision=false);
    UStaticMeshComponent* Rod(FVector A,FVector B,float Width,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material=TEXT("Glow"));
    void Ring(FVector Center,float Radius,FLinearColor Tint,USceneComponent* Parent,int32 Plane=0,float Width=9);
    UInstancedStaticMeshComponent* Instances(const TCHAR* Name,const TCHAR* Mesh,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material=TEXT("Surface"));
    void Person(FVector Position,float Scale,FLinearColor Tint,USceneComponent* Parent);
    void BuildJourney();
    void Shell(const TCHAR* Name,const TArray<FVector>& Centers,const TArray<FVector2D>& Radii,FLinearColor Tint,const TCHAR* Material);
    void BuildCity();
    void BuildSpace();
    void BuildAfternoon();
    bool Initialize();
    void SetShot(int32 Index);
    void UpdateFilm(float DeltaSeconds);
    void UpdateNerves();
    void Pose(FVector Position,FVector LookAt,float Fov=65,float Roll=0);
    void Sound(const TCHAR* Name,float Volume=1,float Pitch=1);
    void SetAmbience(const TCHAR* Name,float Volume);
};

UCLASS()
class BRAINMINING_API AEndingEmergenceGameMode : public AMiningPrototypeGameMode
{
    GENERATED_BODY()
public:
    AEndingEmergenceGameMode();
};
