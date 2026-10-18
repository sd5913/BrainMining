#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "MiningPrototypeGameMode.h"
#include "MiningCollection.generated.h"

class UBoxComponent;
class UDynamicMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UInputMappingContext;
class UInputAction;
class UMiningComponent;
class ADiggableRock;
class SMiningCollectionView;
class UStaticMeshComponent;
class UPostProcessComponent;
class AMiningCollectible;
class AHeavenToilet;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FMiningCollectionPlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ItemId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AreaTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<ADiggableRock> HostRock;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform ClueTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform ApproachTransform;
    // Optional authored prop replaces only the generated presentation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> PresentationActor;
    // A room-owned echo stage supplies the same pulse materials as its walls.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> EchoSource;
};

struct FCollectionAnt
{
    TWeakObjectPtr<AActor> Actor;
    TArray<TWeakObjectPtr<USceneComponent>> Legs;
    TWeakObjectPtr<UMaterialInstanceDynamic> FadeMaterial;
    FVector Start;
    FTransform Frame;
    float Length=260,Phase=0,Fade=1;
    int32 TrailIndex=INDEX_NONE;
};
struct FCollectionAntTrail
{
    TWeakObjectPtr<ADiggableRock> Rock;
    FVector Start,End;
    TArray<FVector> Points,Normals;
    TArray<bool> Exposed;
    int32 ObservedCuts=INDEX_NONE;
};
struct FCollectionBird
{
    TWeakObjectPtr<AMiningCollectible> Actor;
    TWeakObjectPtr<UStaticMeshComponent> LeftWing,RightWing;
    float Delay=0;
};
struct FCollectionClue
{
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<ADiggableRock> Rock;
    FVector Support;
    int32 ObservedCuts=INDEX_NONE;
    bool bFeather=false,bFalling=false,bLanded=false;
    float Age=0,Speed=0,Phase=0;
};

// The item IDs also index the catalogue. These remain stable across field resets.
UCLASS()
class BRAINMINING_API AMiningCollectible : public AActor
{
    GENERATED_BODY()
public:
    AMiningCollectible();
    static constexpr int32 ItemTypeCount = 12;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDynamicMeshComponent> Model;
    UPROPERTY() TObjectPtr<UDynamicMeshComponent> Outline;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ItemId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReleased = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCollected = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Exposure = 0;
    UPROPERTY() TObjectPtr<ADiggableRock> HostRock;
    UPROPERTY() TObjectPtr<AActor> PresentationActor;
    int32 LastObservedCuts = INDEX_NONE;
    FTransform BuriedTransform;
    void BuildModel(int32 Id, bool Flying=false);
    void SetHighlighted(bool Enabled);
    void ApplyEchoPresentation(class UMaterialInterface* Surface,class UMaterialInterface* Contours);
    void Release();
    float MeasureExposure() const;
    bool CanRelease(float Threshold) const;
    static FString ItemName(int32 Id);
    static FString Description(int32 Id);
    static FString Clue(int32 Id);
    bool bEchoPresentation=false;
};

UCLASS()
class BRAINMINING_API AMiningCollectionField : public AActor
{
    GENERATED_BODY()
public:
    AMiningCollectionField();
    UPROPERTY(EditAnywhere, Category="Collection") bool bAreaManaged = false;
    UPROPERTY(EditAnywhere, Category="Collection") FKey CatalogueKey = EKeys::Tab;
    // Kept for the existing catalogue UMG's authored material slots.
    UPROPERTY(EditAnywhere, Category="Collection|UI") TArray<TObjectPtr<class UMaterialInterface>> CatalogueArtwork;
    UPROPERTY(EditAnywhere, Category="Collection") bool bSharedAbilities = false;
    UPROPERTY(EditAnywhere, Category="Collection") bool bShowExplorationHUD = true;
    // A campaign shares one inventory but authors its four caches across several mines.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection|Placement") bool bUseAuthoredPlacements = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection|Placement") bool bBuriedWithClues = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection|Placement") TArray<FMiningCollectionPlacement> AuthoredPlacements;
    void SetAreaActive(bool bActive);
    bool IsAreaActive() const { return bAreaActive; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection", meta=(ClampMin="0.1", ClampMax="1.0")) float ExposureThreshold = .65f;
    // Zero chooses a new seed each run. A positive seed reproduces a layout.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection") int32 RandomSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection|V02") bool bDeepField = false;
    // Enabled only by the expanded collection test map, never by campaign defaults.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection") bool bExpandedCollection = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection") FString DisplayVersion=TEXT("V0.2");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection|V02", meta=(ClampMin="650",ClampMax="1800",Units="cm")) float ExcavationDepth = 1100;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection|V02") int32 FlockBursts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection|V02") int32 AmbientAntCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") int32 ActiveSeed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") TArray<TObjectPtr<AMiningCollectible>> Items;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") TArray<int32> Inventory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") bool bCatalogueOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") int32 SelectedId = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") TObjectPtr<AMiningCollectible> Target;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collection") TObjectPtr<AMiningCollectible> Preview;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> PreviewTexture;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<AHeavenToilet> PreviewToilet;
    UPROPERTY() TObjectPtr<UPostProcessComponent> HighlightPostProcess;
    UFUNCTION(BlueprintCallable) void CollectTarget();
    // Register a room-owned reward with the existing inventory and pickup path.
    bool RegisterExternalCollectible(AMiningCollectible* Item);
    UFUNCTION(BlueprintCallable) void ToggleCatalogue();
    UFUNCTION(BlueprintCallable) void ResetCollection();
    UFUNCTION(BlueprintCallable) void SelectItem(int32 Id);
    UFUNCTION(BlueprintCallable) void RotatePreview(float Yaw, float Pitch);
    UFUNCTION(BlueprintCallable) void ZoomPreview(float Steps);
    UFUNCTION(BlueprintCallable) void ResetPreview();
    int32 UniqueCount() const;
    TArray<int32> CatalogueIds() const;
    FString Toast;
    float ToastRemaining = 0;
private:
    bool InitializePlayer();
    void SpawnField();
    void SpawnAuthoredClues(const FMiningCollectionPlacement& Placement);
    void SpawnAuthoredPresentation(AMiningCollectible* Item,const FMiningCollectionPlacement& Placement);
    void SpawnDeepField();
    void SpawnExpandedCollection();
    void RelocateGeneratedField();
    void TickAmbient(float DeltaSeconds);
    void TickClues(float DeltaSeconds);
    void MakeFeather(ADiggableRock* Rock, FVector Support, float Angle);
    void SpawnAntTrail(ADiggableRock* Rock,FVector Start,FVector End);
    void UpdateAntTrails();
    bool FindBirdExit(TArray<FVector>& Path) const;
    AActor* MakeProp(FVector Position, FVector Size, FLinearColor Color, bool Round=false);
    void RefreshTarget();
    void SetCatalogueOpen(bool Open);
    UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;
    UPROPERTY() TObjectPtr<APlayerController> Player;
    UPROPERTY() TObjectPtr<UMiningComponent> Mining;
    UPROPERTY() TObjectPtr<UInputMappingContext> CollectionContext;
    UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
    TSharedPtr<SMiningCollectionView> View;
    bool bInitialized = false;
    bool bAreaActive = true;
    int32 PreviewRefreshFrames=0;
    TArray<FCollectionAnt> Ants;
    TArray<FCollectionAntTrail> AntTrails;
    TArray<FCollectionBird> Birds;
    TArray<FCollectionClue> Clues;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ReagentStain;
    TArray<FVector> BirdPath;
    TWeakObjectPtr<AMiningCollectible> ResidentBird;
    float AmbientTime=0,FlightTime=-1,NextFlockCheck=0;
    int32 BirdObservedCuts=INDEX_NONE;
    float PollTime = 0;
    float PreviewYaw = -25;
    float PreviewPitch = 0;
    float PreviewDistance = 235;
    FTransform StartTransform;
};

UCLASS()
class BRAINMINING_API AMiningCollectionGameMode : public AMiningPrototypeGameMode
{
    GENERATED_BODY()
public:
    AMiningCollectionGameMode();
};
