#include "EndingEmergence.h"
#include "MiningGamepadSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "BrainMiningFonts.h"
#include "MiningAudioSettings.h"
#include "EndingCrowd.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "MiningDebrisBurst.h"
#include "MiningToolEffectsComponent.h"
#include "Engine/StaticMesh.h"
#include "BrainMiningPlayerController.h"
#include "MiningInteractionHUDWidget.h"
#include "MiningCharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "PhysicsEngine/BodySetup.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"
#include "Fonts/FontMeasure.h"

namespace Ending
{
    const FName Tag(TEXT("EmergenceEndingGenerated"));
    const FLinearColor Gold(.95f,.47f,.09f), Teal(.035f,.62f,.58f), Blue(.065f,.18f,.42f), Rose(.48f,.035f,.085f);
    const FLinearColor Neural(.065f,.32f,.72f);
    const FLinearColor GoalBlue(.045f,.30f,1.f);
    const FVector City(22000,0,0), Earth(50000,0,0), Cosmos(95000,0,0), Desk(155000,0,0);
    const float GateX[]={1600,5800,7300,8650,10000};
    const float GateY[]={0,0,-170,170,0};
    float Ease(float V) { V=FMath::Clamp(V,0.f,1.f); return V*V*(3-2*V); }
    FVector ScaleRail(const FVector& A,const FVector& B,float T)
    {
        const double Radius=FMath::Exp(FMath::Lerp(FMath::Loge(A.Size()),FMath::Loge(B.Size()),double(T)));
        return FMath::Lerp(A.GetSafeNormal(),B.GetSafeNormal(),double(T)).GetSafeNormal()*Radius;
    }
    const FVector BrainCenter(19700,0,-8146.21125);
    constexpr float BrainRadius=10500;
    constexpr int NeuronCount=47;
    int Layer(int I) { return I==0?0:I==NeuronCount-1?10:1+(I-1)/5; }
    float Ground(float X,float Y=0)
    {
        if(X<1600)
        {
            const float Neck=Ease((X-1050)/550.f),Scale=1-.5f*Neck;
            return 425-212.5f*Neck-570*Scale*FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(X-800)/(1200*1200.f)-Y*Y/FMath::Square(650*Scale)));
        }
        if(X<=12000)return 850-FMath::Sqrt(FMath::Max(0.f,850*850.f-Y*Y));
        return BrainCenter.Z+FMath::Sqrt(FMath::Max(0.f,BrainRadius*BrainRadius-FMath::Square(X-BrainCenter.X)-Y*Y));
    }
}

class SEmergenceEndingView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEmergenceEndingView) {} SLATE_ARGUMENT(AEndingEmergence*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& A) { Owner=A._Owner; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override
    {
        if(!Owner.IsValid() || Owner->bComplete)return L;
        const auto* D=Owner.Get();const FVector2D Size=G.GetLocalSize();
        const float S=FMath::Min(Size.X/1600,Size.Y/900);
        // Retain controls, aim and target markers, without narrative or target labels.
        const float Fade=D->OverlayAlpha();
        if(Fade>0)FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(0,0,0,Fade));
        if(Fade>.995f)return L;
        if(D->Shot<0&&D->GateIndex<5)
        {
            auto Rect=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);};
            const auto* P=D->GetWorld()->GetFirstPlayerController();
            const auto* Mining=P&&P->GetPawn()?P->GetPawn()->FindComponentByClass<UMiningComponent>():nullptr;
            const FLinearColor Aim=D->GateIndex==2&&D->TargetNeuron>=0?Ending::Gold:Mining&&Mining->TargetRock?Ending::Teal:FLinearColor(.9f,.92f,.9f);
            Rect(Size.X*.5f-5*S,Size.Y*.5f-S,10,2,Aim);Rect(Size.X*.5f-S,Size.Y*.5f-5*S,2,10,Aim);
            if(D->GateIndex==2&&P)for(int I=0;I<D->NeuronPositions.Num();++I)if(D->CanPossess(I)||I==D->NeuronPositions.Num()-1)
            {
                FVector2D Screen;int Width=0,Height=0;P->GetViewportSize(Width,Height);
                if(Width<=0||Height<=0||!P->ProjectWorldLocationToScreen(D->NeuronWorldPosition(I),Screen,false))continue;
                Screen*=FVector2D(Size.X/Width,Size.Y/Height);
                if(Screen.X<45*S||Screen.X>Size.X-45*S||Screen.Y<45*S||Screen.Y>Size.Y-100*S)continue;
                const bool Goal=I==D->NeuronPositions.Num()-1;
                // The destination is identified by its pale blue body itself;
                // the four-corner aim widget belongs only to selectable hops.
                if(Goal)continue;
                const FLinearColor C=I==D->TargetNeuron?Ending::Gold:Ending::Teal;
                const float R=(I==D->TargetNeuron?26:18)*S;
                Rect(Screen.X-R,Screen.Y-R,10,2,C);Rect(Screen.X-R,Screen.Y-R,2,10,C);
                Rect(Screen.X+R-10*S,Screen.Y+R,10,2,C);Rect(Screen.X+R,Screen.Y+R-8*S,2,10,C);
            }
        }
        const FString Controls=D->Hint();
        if(!Controls.IsEmpty())
        {
            const auto Font=BrainMiningFonts::Style("Regular",FMath::Max(10,FMath::RoundToInt(13*S)));
            const FVector2f Position(48*S,Size.Y-40*S);
            FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(Size.X-96*S,30*S),FSlateLayoutTransform(Position+FVector2f(1,1))),Controls,Font,ESlateDrawEffect::None,FLinearColor(0,0,0,.7f));
            FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(Size.X-96*S,30*S),FSlateLayoutTransform(Position)),Controls,Font,ESlateDrawEffect::None,FLinearColor(.8f,.86f,.89f,.9f));
        }
        return L;
    }
private:
    TWeakObjectPtr<AEndingEmergence> Owner;
};

AEndingEmergenceGameMode::AEndingEmergenceGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/Showcase/Blueprints/BP_SharedMiningCharacter"));
    if(Pawn.Succeeded())DefaultPawnClass=Pawn.Class;
    HUDClass=AHUD::StaticClass();
}
AEndingEmergence::AEndingEmergence()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

USceneComponent* AEndingEmergence::Group(const TCHAR* Name,FVector Position,USceneComponent* Parent)
{
    auto* C=NewObject<USceneComponent>(this,MakeUniqueObjectName(this,USceneComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(Ending::Tag); C->SetupAttachment(Parent?Parent:RootComponent.Get());
    C->SetRelativeLocation(Position); C->RegisterComponent(); return C;
}

UStaticMeshComponent* AEndingEmergence::Shape(const TCHAR* Name,FVector Position,FVector Size,FLinearColor Tint,const TCHAR* Mesh,USceneComponent* Parent,const TCHAR* Material,bool Collision)
{
    auto* C=NewObject<UStaticMeshComponent>(this,MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(Ending::Tag); C->SetupAttachment(Parent?Parent:RootComponent.Get());
    C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(Mesh).StartsWith(TEXT("Facet"))?FString(TEXT("/Game/EndingEmergence/Meshes/SM_EE_"))+Mesh:FString(TEXT("/Engine/BasicShapes/"))+Mesh)));
    C->SetRelativeLocation(Position); C->SetRelativeScale3D(Size/100); C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
    C->SetGenerateOverlapEvents(false); C->SetCanEverAffectNavigation(false); C->SetCastShadow(false);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->RegisterComponent();return C;
}

UStaticMeshComponent* AEndingEmergence::Rod(FVector A,FVector B,float Width,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material)
{
    const bool Neural=FCString::Strcmp(Material,TEXT("NeuralFilament"))==0;
    auto* C=Shape(Neural?TEXT("TaperedNeuralBranch"):TEXT("Connection"),(A+B)*.5,FVector(Width,Width,(B-A).Size()),Tint,Neural?TEXT("FacetNeuronBranch"):TEXT("Cylinder"),Parent,Material);
    C->SetRelativeRotation(FRotationMatrix::MakeFromZ(B-A).Rotator());return C;
}

void AEndingEmergence::Ring(FVector Center,float Radius,FLinearColor Tint,USceneComponent* Parent,int32 Plane,float Width)
{
    for(int I=0;I<64;++I)
    {
        auto P=[&](float A){ const float X=FMath::Cos(A)*Radius,Y=FMath::Sin(A)*Radius;return Center+(Plane==0?FVector(0,X,Y):Plane==1?FVector(X,Y,0):FVector(X,0,Y));};
        Rod(P(I*2*PI/64),P((I+1)*2*PI/64),Width,Tint,Parent);
    }
}

UInstancedStaticMeshComponent* AEndingEmergence::Instances(const TCHAR* Name,const TCHAR* Mesh,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material)
{
    auto* C=NewObject<UInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UInstancedStaticMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C);C->ComponentTags.Add(Ending::Tag);C->SetupAttachment(Parent);C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(Mesh).StartsWith(TEXT("Facet"))?FString(TEXT("/Game/EndingEmergence/Meshes/SM_EE_"))+Mesh:FString(TEXT("/Engine/BasicShapes/"))+Mesh)));
    C->SetCollisionProfileName(TEXT("NoCollision"));C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->RegisterComponent();return C;
}

void AEndingEmergence::Person(FVector P,float S,FLinearColor Tint,USceneComponent* Parent)
{
    Shape(TEXT("PersonHead"),P+FVector(0,0,162)*S,FVector(35,34,42)*S,FLinearColor(.6f,.38f,.24f),TEXT("Sphere"),Parent);
    Shape(TEXT("Hair"),P+FVector(-4,0,174)*S,FVector(34,36,25)*S,FLinearColor(.035f,.025f,.023f),TEXT("Sphere"),Parent);
    Shape(TEXT("Shirt"),P+FVector(0,0,112)*S,FVector(42,59,65)*S,Tint,TEXT("Sphere"),Parent);
    for(int Side:{-1,1})
    {
        Rod(P+FVector(0,Side*15,82)*S,P+FVector(8,Side*18,12)*S,17*S,FLinearColor(.035f,.052f,.072f),Parent,TEXT("Surface"));
        Rod(P+FVector(0,Side*30,130)*S,P+FVector(12,Side*34,85)*S,12*S,Tint,Parent,TEXT("Surface"));
    }
}

void AEndingEmergence::Shell(const TCHAR* Name,const TArray<FVector>& Centers,const TArray<FVector2D>& Radii,FLinearColor Tint,const TCHAR* Material,bool FollowCurve)
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh(true,false,false,false);
    constexpr int Sides=96;
    for(int Row=0;Row<Centers.Num();++Row)for(int I=0;I<Sides;++I)
    {
        const float A=I*2*PI/Sides;
        const FVector Offset(0,FMath::Cos(A)*Radii[Row].X,FMath::Sin(A)*Radii[Row].Y);
        const FRotator Frame=(Centers[FMath::Min(Row+1,Centers.Num()-1)]-Centers[FMath::Max(Row-1,0)]).Rotation();
        Mesh.AppendVertex(FVector3d(Centers[Row]+(FollowCurve?Frame.RotateVector(Offset):Offset)));
    }
    for(int Row=0;Row<Centers.Num()-1;++Row)for(int I=0;I<Sides;++I)
    {
        const int A=Row*Sides+I,B=Row*Sides+(I+1)%Sides,C=A+Sides,D=B+Sides;
        // Chaos collision uses the opposite triangle winding to mesh normals.
        Mesh.AppendTriangle(A,B,C);Mesh.AppendTriangle(B,D,C);
    }
    FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
    auto* C=NewObject<UDynamicMeshComponent>(this,MakeUniqueObjectName(this,UDynamicMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C);C->ComponentTags.Add(Ending::Tag);C->SetupAttachment(Sets[0]);
    if(FCString::Strcmp(Name,TEXT("RedSpirallingVessel"))==0)C->ComponentTags.Add(TEXT("EndingBloodVisual"));
    C->SetEnableRaytracing(false);C->SetVisibleInRayTracing(false);C->SetCastShadow(false);
    C->SetCollisionProfileName(TEXT("BlockAll"));C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);
    C->SetComplexAsSimpleCollisionEnabled(true,false);C->bUseAsyncCooking=false;
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->SetMesh(MoveTemp(Mesh));C->RegisterComponent();
    C->GetBodySetup()->bDoubleSidedGeometry=false;C->GetBodySetup()->SetFlags(RF_Transient);C->UpdateCollision(false);
}

void AEndingEmergence::BuildJourney()
{
    auto* Set=Sets[0].Get();FRandomStream R(803);
    TArray<FVector> Centers;TArray<FVector2D> Radii;
    // A hollow ellipsoid with a real curved floor and an open neck at the exit.
    for(int Row=0;Row<=96;++Row)
    {
        const float X=FMath::Lerp(-399.f,1600.f,Row/96.f);
        const float Cross=FMath::Sqrt(FMath::Max(.0001f,1-FMath::Square(X-800)/(1200*1200.f)));
        const float Neck=Ending::Ease((X-1050)/550.f),Scale=1-.5f*Neck;
        Centers.Add(FVector(X,0,425-212.5f*Neck));Radii.Add(FVector2D(650*Cross*Scale,570*Cross*Scale));
    }
    Shell(TEXT("EllipsoidalCellInterior"),Centers,Radii,FLinearColor(.26f,.062f,.085f),TEXT("CellInterior"));
    for(int I=0;I<27;++I)
    {
        const float X=R.FRandRange(-1300,1300),A=R.FRandRange(-.25f,PI+ .25f);
        const float Cross=FMath::Sqrt(1-X*X/(2400*2400.f));
        const FVector P(X,FMath::Cos(A)*950*Cross,850+FMath::Sin(A)*800*Cross);
        auto* C=Shape(TEXT("Organelle"),P,FVector(130,240,100),I%3?Ending::Teal*.6f:Ending::Rose,TEXT("Sphere"),Set,TEXT("Organic"));
        C->SetRelativeRotation(FRotator(I*17,I*29,25));
    }
    Shape(TEXT("CellNucleus"),FVector(560,-650,650),FVector(690,520,650),FLinearColor(.11f,.075f,.28f),TEXT("Sphere"),Set,TEXT("Organic"));
    Shape(TEXT("Nucleolus"),FVector(340,-500,780),FVector(130),Ending::Gold,TEXT("Sphere"),Set,TEXT("Glow"));
    // Arc-length sampling is shared by the tube, camera and blood cells. The
    // gentle corkscrew never inverts the player or changes speed on a bend.
    VesselPath.Empty();VesselDistances.Empty();Centers.Empty();Radii.Empty();
    for(int Row=0;Row<=240;++Row)
    {
        // Restore the previous uniformly distributed bend. Only the entrance
        // blends out of its straight neck; do not squeeze the turn into mid-route.
        const float T=Row/240.f,A=T*2*PI,Neck=Ending::Ease((T-.04f)/.16f);
        const FVector P(1600+10400*T,Neck*2100*FMath::Sin(A)*FMath::Square(FMath::Sin(PI*T)),212.5f+Neck*(637.5f+1100*(1-FMath::Cos(A))));
        VesselDistances.Add(Row?VesselDistances.Last()+FVector::Dist(P,VesselPath.Last()):0);VesselPath.Add(P);
        if(T<=.9f)
        {
            const float Flare=Ending::Ease((T-.77f)/.23f);
            Centers.Add(P);Radii.Add(FVector2D(FMath::Lerp(242.25f,850.f,Ending::Ease((T-.10f)/.12f))+Flare*400,FMath::Lerp(212.5f,850.f,Ending::Ease((T-.10f)/.12f))+Flare*400));
        }
    }
    Shell(TEXT("RedSpirallingVessel"),Centers,Radii,FLinearColor(.38f,.018f,.026f),TEXT("Vessel"),true);
    const float Length=VesselDistances.Last();
    for(int I=0;I<38;++I)
    {
        const float D=350+I*(Length-350)/38,Open=Ending::Ease((D/Length-.77f)/.23f);
        auto* Rib=Group(TEXT("CurvedVesselRib"),VesselPoint(D),Set);Rib->SetRelativeRotation(VesselRotation(D));
        Rib->ComponentTags.Add(TEXT("EndingBloodVisual"));
        Ring(FVector::ZeroVector,FMath::Lerp(210.f,843.f,Ending::Ease((D/Length-.10f)/.12f))+Open*400,FMath::Lerp(Ending::Rose*.44f,Ending::Teal*.18f,Open),Rib,0,10-Open*6);
    }
    // The final skin opens into a sparse lattice: the brain becomes visible
    // through it before the camera leaves the blood stream.
    for(int Strand=0;Strand<12;++Strand)for(int J=0;J<16;++J)
    {
        auto P=[&](int K){const float T=.86f+.14f*K/16,A=Strand*2*PI/12+K*.012f;return VesselPoint(Length*T)+VesselRotation(Length*T).RotateVector(FVector(0,FMath::Cos(A),FMath::Sin(A))*(850+Ending::Ease((T-.77f)/.23f)*400));};
        Rod(P(J),P(J+1),7,FMath::Lerp(Ending::Rose*.3f,Ending::Teal*.28f,J/16.f),Set)->ComponentTags.Add(TEXT("EndingBloodVisual"));
    }
    for(int I=0;I<130;++I)
    {
        const float A=R.FRandRange(-PI,PI),Rad=R.FRandRange(440,650);
        const FVector Origin(R.FRandRange(100,Length-100),FMath::Cos(A)*Rad,FMath::Sin(A)*Rad);
        const FVector P=VesselPoint(Origin.X)+VesselRotation(Origin.X).RotateVector(FVector(0,Origin.Y,Origin.Z)*FMath::Lerp(.24f,1.f,Ending::Ease((Origin.X/Length-.10f)/.12f)));
        auto* B=Shape(TEXT("BloodCell"),P,FVector(220,210,62),FLinearColor(.66f,.045f,.035f),TEXT("Sphere"),Set,TEXT("Blood"));
        B->SetRelativeRotation(FRotator(I*31,I*43,22));B->ComponentTags.Add(TEXT("EndingBloodVisual"));Blood.Add(B);BloodOrigins.Add(Origin);
    }
    TArray<USceneComponent*> BeforeBrain;Set->GetChildrenComponents(false,BeforeBrain);
    auto* BrainCollision=Shape(TEXT("WalkableBrainPlanet"),Ending::BrainCenter,FVector(Ending::BrainRadius*2),FLinearColor(.10f,.04f,.15f),TEXT("Sphere"),Set,TEXT("BrainGround"),true);
    BrainCollision->SetHiddenInGame(true);BrainCollision->SetVisibility(false);
    BuildBrainSurface();
    // Ambient neural filaments follow the small planet's surface.
    for(int I=0;I<330;++I)
    {
        const float X=R.FRandRange(13000,26700),Y=R.FRandRange(-2400,2400);
        const FVector P(X,Y,Ending::Ground(X,Y)+18);
        const FVector Q(X+110,Y+40,Ending::Ground(X+110,Y+40)+18);
        Nerves.Add(Rod(P,Q,8,Ending::Neural*.07f,Set,TEXT("NeuralFilament")));
        AmbientNeuronCores.Add(Shape(TEXT("Neuron"),P,FVector(32),Ending::Neural*.7f,TEXT("FacetNeuronSoma"),Set,TEXT("NeuronGlass")));
        if(I%6==0&&FMath::Abs(Y)>380)
        {
            // Low branching fibres replace the old upright bead-on-a-stick props.
            const FVector Fork=P+FVector(65,45,65);
            Nerves.Add(Rod(P,Fork,10,Ending::Neural*.3f,Set,TEXT("NeuralFilament")));
            for(int Side:{-1,1})Nerves.Add(Rod(Fork,Fork+FVector(80,Side*85,35),5,Ending::Neural*.18f,Set,TEXT("NeuralFilament")));
        }
    }
    // Nine branching layers: twice the original forward distance and ten hops to the goal.
    NeuronPositions.Add(FVector(13200,0,Ending::Ground(13200)+260));
    for(int Row=0;Row<9;++Row)for(int Lane=-2;Lane<=2;++Lane)
    {
        const float X=14500+Row*1300+R.FRandRange(-180,180),Y=Lane*800+R.FRandRange(-260,260);
        NeuronPositions.Add(FVector(X,Y,Ending::Ground(X,Y)+260));
    }
    NeuronPositions.Add(FVector(26200,0,Ending::Ground(26200)+400));
    for(int I=0;I<NeuronPositions.Num();++I)
    {
        const FVector P=NeuronPositions[I];const bool Goal=I==NeuronPositions.Num()-1;
        const float Size=Goal?1000:I==0?210:R.FRandRange(170,245);
        const FVector Normal=FVector(-(Ending::Ground(P.X+30,P.Y)-Ending::Ground(P.X-30,P.Y))/60,-(Ending::Ground(P.X,P.Y+30)-Ending::Ground(P.X,P.Y-30))/60,1).GetSafeNormal();
        const FQuat Surface=FRotationMatrix::MakeFromZX(Normal,FVector::ForwardVector).ToQuat();
        const FQuat Orientation=Surface*FQuat(FVector::UpVector,R.FRandRange(-PI,PI));
        const FLinearColor GoalBlue=Ending::GoalBlue;
        NeuronCores.Add(Shape(Goal?TEXT("LargestNeuron"):TEXT("PossessableNeuron"),P,FVector(Size,Size*R.FRandRange(.80f,1.08f),Size*R.FRandRange(.82f,1.06f)),Goal?GoalBlue*2:FLinearColor(.15f,.65f,1.1f),TEXT("FacetNeuronSoma"),Set,TEXT("NeuronHero")));
        NeuronCores.Last()->SetRelativeRotation(Orientation);
        Shape(TEXT("NeuronNucleus"),FVector::ZeroVector,FVector(28),FLinearColor(.12f,.38f,.85f),TEXT("Sphere"),NeuronCores.Last(),TEXT("NeuronCore"));
        const FString ArborMesh=I%4?FString::Printf(TEXT("FacetNeuronArbor%d"),I%4):TEXT("FacetNeuronArbor");
        auto* Arbor=Shape(TEXT("NeuronDendriticArbor"),P,FVector(Goal?Size*.7f:Size)*FVector(1,R.FRandRange(.8f,1.25f),1),Goal?GoalBlue*2:FLinearColor(.15f,.65f,1.1f),*ArborMesh,Set,TEXT("NeuralFilament"));
        Arbor->SetRelativeRotation(Orientation);Nerves.Add(Arbor);
        if(auto* Mat=Cast<UMaterialInstanceDynamic>(Arbor->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("Variant"),I*.731f);
    }
    // Fine curved axons lie along the brain surface and physically join the hosts.
    for(int A=0;A<NeuronPositions.Num();++A)for(int B=A+1;B<NeuronPositions.Num();++B)
    {
        const FVector Delta=NeuronPositions[B]-NeuronPositions[A];
        if(FMath::Abs(Ending::Layer(A)-Ending::Layer(B))>1||Delta.Size()>1750)continue;
        auto* Link=NewObject<USplineMeshComponent>(this,MakeUniqueObjectName(this,USplineMeshComponent::StaticClass(),TEXT("NeuralAxonLink")),RF_Transactional);
        AddInstanceComponent(Link);Link->ComponentTags.Add(Ending::Tag);Link->SetupAttachment(Set);
        Link->SetMobility(EComponentMobility::Movable);Link->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndingEmergence/Meshes/SM_EE_FacetNeuronBranch")));
        Link->SetForwardAxis(ESplineMeshAxis::Z,false);
        const FVector Mid=(NeuronPositions[A]+NeuronPositions[B])*.5;
        const FVector Bend(0,R.FRandRange(-180,180),R.FRandRange(-100,100));
        Link->SetRelativeLocation(Mid);Link->SetStartAndEnd(-Delta*.5,Delta+Bend,Delta*.5,Delta-Bend,false);
        Link->SetStartScale(FVector2D(.045f),false);Link->SetEndScale(FVector2D(.025f),false);
        Link->SetCollisionEnabled(ECollisionEnabled::NoCollision);Link->SetCastShadow(false);Link->SetCanEverAffectNavigation(false);
        auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_NeuralFilament")),Link);
        Mat->SetVectorParameterValue(TEXT("Tint"),Ending::Neural*.35f);Mat->SetScalarParameterValue(TEXT("Variant"),A*.71f+B*.13f);Link->SetMaterial(0,Mat);
        Link->RegisterComponent();Link->UpdateMesh();Nerves.Add(Link);
    }
    PulseTrail=nullptr;
    BuildBackgroundNeurons();
    // Leave a clear viewing gap outside the vessel. Translate the entire brain,
    // including collision, hosts and fine links, as one spatial layout.
    TArray<USceneComponent*> BrainParts;Set->GetChildrenComponents(false,BrainParts);
    for(auto* Part:BrainParts)if(!BeforeBrain.Contains(Part))Part->SetRelativeLocation(Part->GetRelativeLocation()+FVector(3200,0,0));
    for(FVector& Host:NeuronPositions)Host+=FVector(3200,0,0);
    // Only the final cell membrane is excavatable. Blood and brain need no tool.
    for(int I=0;I<1;++I)
    {
        const float X=Ending::GateX[I],Y=Ending::GateY[I];
        // Keep the plug concentric with the closed cell-to-vessel neck.
        const float Z=VesselPath.IsValidIndex(0)?VesselPath[0].Z:212.5f;
        auto* Gate=GetWorld()->SpawnActor<ADiggableRock>(ADiggableRock::StaticClass(),GetActorTransform().TransformPosition(FVector(X,Y,Z)),GetActorRotation());
        Gate->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);Gate->Tags.Add(Ending::Tag);
        // The circular plug spans the whole cell-to-vessel cross-section, so
        // the blood tube cannot be seen through the room before the first cut.
        Gate->RockSize=FVector(220,1300,1300);Gate->bUseCircularRockMesh=true;
        Gate->RockMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_RedMembrane"));
        Gate->ResetRock();Gates.Add(Gate);
    }
}

void AEndingEmergence::BuildCity()
{
    auto* Set=Sets[1].Get();FRandomStream R(75);
    // A separate continental-scale view, seen from above two broken cloud decks.
    NightContinent=Group(TEXT("NightContinentFromClouds"),FVector::ZeroVector,Set);
    Shape(TEXT("NightCoastAndCities"),FVector(0,0,-1000),FVector(1600000,1600000,1),FLinearColor::White,TEXT("Plane"),NightContinent,TEXT("NightContinent"));
    Shape(TEXT("LowCloudDeck"),FVector(0,0,32000),FVector(1400000,1400000,1),FLinearColor(.12f,.18f,.26f),TEXT("Plane"),NightContinent,TEXT("CloudDeck"));
    auto* HighCloud=Shape(TEXT("HighCloudDeck"),FVector(0,0,58000),FVector(1400000,1400000,1),FLinearColor(.09f,.13f,.21f),TEXT("Plane"),NightContinent,TEXT("CloudDeck"));
    HighCloud->SetRelativeRotation(FRotator(0,67,0));
    Shape(TEXT("CityGround"),FVector(0,0,-30),FVector(17000,17000,50),FLinearColor(.015f,.026f,.035f),TEXT("Cube"),Set);
    auto* Windows=Instances(TEXT("CityWindows"),TEXT("Cube"),Ending::Gold*.6f,Set,TEXT("Glow"));
    auto* Buildings=Instances(TEXT("Buildings"),TEXT("Cube"),FLinearColor(.035f,.055f,.09f),Set);
    for(int X=-7;X<=7;++X) for(int Y=-7;Y<=7;++Y)
    {
        if(X==0||Y==0) continue;
        const FVector P(X*1050,Y*1050,0);const float H=R.FRandRange(400,2300);
        Buildings->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(0,0,H*.5),FVector(7.2,7.4,H/100)));
        for(int Z=120;Z<H-60;Z+=180) for(int J=-2;J<=2;++J)
        {
            if(R.FRand()<.22) continue;
            Windows->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(-365,J*130,Z),FVector(.04,.45,.6)));
            Windows->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(J*130,-375,Z),FVector(.45,.04,.6)));
        }
    }
    for(int I=-7;I<=7;++I)
    {
        Shape(TEXT("StreetLightGrid"),FVector(I*1050,0,1),FVector(14,17000,3),Ending::Gold*.17f,TEXT("Cube"),Set,TEXT("Glow"));
        Shape(TEXT("StreetLightGrid"),FVector(0,I*1050,1),FVector(17000,14,3),Ending::Gold*.17f,TEXT("Cube"),Set,TEXT("Glow"));
    }
    for(int I=-8;I<=8;++I) for(int Side:{-1,1})
    {
        Shape(TEXT("Crosswalk"),FVector(I*40,Side*380,4),FVector(21,170,4),FLinearColor(.5f,.6f,.62f),TEXT("Cube"),Set);
        Shape(TEXT("Crosswalk"),FVector(Side*380,I*40,4),FVector(170,21,4),FLinearColor(.5f,.6f,.62f),TEXT("Cube"),Set);
    }
    Crowd=Instances(TEXT("Crowd"),TEXT("Sphere"),FLinearColor(.13f,.22f,.26f),Set);
    CrowdHeads=Instances(TEXT("CrowdHeads"),TEXT("Sphere"),FLinearColor(.47f,.29f,.2f),Set);
    for(int I=0;I<360;++I)
    {
        FVector P(R.FRandRange(-2800,2800),R.FRandRange(-280,280),0);
        if(I%2) Swap(P.X,P.Y);
        if(FVector2D(P).Size()<130) P.X+=240;
        const FTransform T(FRotator(0,I%2?90:0,0),P+FVector(0,0,84),FVector(.32,.4,1.08));Crowd->AddInstance(T);CrowdOrigins.Add(T);
        CrowdHeads->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(0,0,153),FVector(.29,.29,.35)));
    }
    EyePerson=Group(TEXT("OurStranger"),FVector::ZeroVector,Set);Person(FVector::ZeroVector,1,Ending::Teal*.5f,EyePerson);
    for(int Side:{-1,1})
    {
        Shape(TEXT("NeonBillboard"),FVector(650,Side*665,440),FVector(20,300,150),Side>0?Ending::Rose:Ending::Teal,TEXT("Cube"),Set,TEXT("Glow"));
        Rod(FVector(460,Side*460,0),FVector(460,Side*460,360),12,Ending::Blue,Set,TEXT("Surface"));
        Shape(TEXT("TrafficSignal"),FVector(460,Side*460,340),FVector(26,46,95),Ending::Teal,TEXT("Cube"),Set,TEXT("Glow"));
    }
}

void AEndingEmergence::ApplyCityBuildings()
{
    if(CityBuildingMeshes.Num()!=4||CityBuildingMeshes.Contains(nullptr)||!Sets.IsValidIndex(1))return;
    TArray<UInstancedStaticMeshComponent*> Existing;GetComponents(Existing);
    for(auto* C:Existing)
    {
        if(C->GetName().StartsWith(TEXT("Buildings"))||C->GetName().StartsWith(TEXT("CityWindows")))C->ClearInstances();
        if(C->GetName().StartsWith(TEXT("CityAssetBuildings"))){RemoveInstanceComponent(C);C->DestroyComponent();}
    }
    TArray<UInstancedStaticMeshComponent*> Groups;
    for(int32 I=0;I<4;++I)
    {
        auto* C=Instances(*FString::Printf(TEXT("CityAssetBuildings%d"),I),TEXT("Cube"),FLinearColor::White,Sets[1]);
        C->SetStaticMesh(CityBuildingMeshes[I]);C->EmptyOverrideMaterials();C->SetCastShadow(false);C->SetVisibleInRayTracing(false);Groups.Add(C);
    }
    FRandomStream R(75);
    for(int X=-7;X<=7;++X)for(int Y=-7;Y<=7;++Y)
    {
        if(X==0||Y==0)continue;
        const int32 Index=(FMath::Abs(X)==1&&FMath::Abs(Y)==1)?(X==Y?1:3):FMath::Abs(X*11+Y*7)%4;
        const auto Bounds=CityBuildingMeshes[Index]->GetBounds();
        const FVector Size=Bounds.BoxExtent*2;
        const float XY=FMath::Min(870.f/Size.X,870.f/Size.Y)*R.FRandRange(.95f,1.f);
        const float Height=Index==3?R.FRandRange(2800,4200):R.FRandRange(1500,2600);
        const FVector Scale(XY,XY,Height/Size.Z);
        const FRotator Rotation(0,R.RandRange(0,3)*90.f,0);
        const FVector Offset=Bounds.Origin*Scale-FVector(0,0,Bounds.BoxExtent.Z*Scale.Z);
        Groups[Index]->AddInstance(FTransform(Rotation,FVector(X*1050,Y*1050,0)-Rotation.RotateVector(Offset),Scale));
    }
    // Lightweight street dressing uses the same acquired pack and instancing.
    TArray<UStaticMeshComponent*> OldDress;GetComponents(OldDress);
    for(auto* C:OldDress)if(C->GetName().StartsWith(TEXT("CityDress"))){RemoveInstanceComponent(C);C->DestroyComponent();}
    if(CityPropMeshes.Num()==5&&!CityPropMeshes.Contains(nullptr))
    {
        TArray<UInstancedStaticMeshComponent*> Props;
        for(int I=0;I<5;++I)
        {
            auto* C=Instances(*FString::Printf(TEXT("CityDressProp%d"),I),TEXT("Cube"),FLinearColor::White,Sets[1]);
            C->SetStaticMesh(CityPropMeshes[I]);C->EmptyOverrideMaterials();C->SetVisibleInRayTracing(false);Props.Add(C);
        }
        auto Place=[&](int Index,FVector P,float Height,float Yaw)
        {
            const auto B=CityPropMeshes[Index]->GetBounds();const float Scale=Height/(B.BoxExtent.Z*2);
            const FRotator Rot(0,Yaw,0);
            const FVector Offset=(B.Origin-FVector(0,0,B.BoxExtent.Z))*Scale;
            Props[Index]->AddInstance(FTransform(Rot,P-Rot.RotateVector(Offset),FVector(Scale)));
        };
        for(int Side:{-1,1})for(int K=-6;K<=6;++K)
        {
            if(K==0)continue;
            const float Along=K*1050.f;
            Place(1,FVector(Along,Side*575.f,0),450,0);
            Place(1,FVector(Side*575.f,Along,0),450,90);
            Place(2,FVector(Along+240,Side*580.f,0),90,0);
            Place(2,FVector(Side*580.f,Along+240,0),90,90);
            Place(4,FVector(Along-180,Side*580.f,0),90,Side*90.f);
            if(K%3==0)Place(0,FVector(Along,Side*570.f,0),260,Side>0?180:0);
        }
        for(int X:{-1,1})for(int Y:{-1,1})Place(3,FVector(X*540,Y*540,0),340,X>0?180:0);
    }
    auto* Curbs=Instances(TEXT("CityDressCurbs"),TEXT("Cube"),FLinearColor(.16f,.21f,.28f),Sets[1]);
    auto* Lights=Instances(TEXT("CityDressLightStrips"),TEXT("Cube"),FLinearColor(.22f,.45f,.65f),Sets[1],TEXT("Glow"));
    for(int Side:{-1,1})for(int K=-7;K<=7;++K)
    {
        if(K==0)continue;
        for(int Axis=0;Axis<2;++Axis)
        {
            const FVector P=Axis?FVector(Side*560,K*1050,4):FVector(K*1050,Side*560,4);
            const FRotator Rot(0,Axis?90:0,0);
            Curbs->AddInstance(FTransform(Rot,P,FVector(9.2f,1.4f,.08f)));
            Lights->AddInstance(FTransform(Rot,P+FVector(0,0,5),FVector(8.8f,.035f,.02f)));
        }
    }
    MarkPackageDirty();
}

void AEndingEmergence::BuildSpace()
{
    auto* Planet=Sets[2].Get();
    auto* EarthSurface=Shape(TEXT("Earth"),FVector::ZeroVector,FVector(8000),FLinearColor(.015f,.10f,.28f),TEXT("Sphere"),Planet,TEXT("Planet"));
    auto* EarthShell=Shape(TEXT("Atmosphere"),FVector::ZeroVector,FVector(8200),FLinearColor(.08f,.42f,.95f),TEXT("Sphere"),Planet,TEXT("EarthHalo"));
    auto* EarthClouds=Shape(TEXT("EarthCloudShell"),FVector::ZeroVector,FVector(8075),FLinearColor::White,TEXT("Sphere"),Planet,TEXT("EarthClouds"));
    // A world-space sunrise behind the upper limb: the globe occludes both
    // the source and its rays, unlike a screen overlay or a complete bright ring.
    auto* Dawn=Group(TEXT("EarthDawnSource"),FVector(2200,-650,3850),Planet);
    Shape(TEXT("DawnPhotosphere"),FVector::ZeroVector,FVector(220),FLinearColor(12,7,2.5f),TEXT("Sphere"),Dawn,TEXT("Glow"));
    auto* Rays=Shape(TEXT("DawnScattering"),FVector::ZeroVector,FVector(5400,5400,1),FLinearColor::White,TEXT("Plane"),Dawn,TEXT("EarthDawn"));
    Rays->SetRelativeRotation(FRotator(90,0,0));
    auto* LandLights=Instances(TEXT("EarthLights"),TEXT("Sphere"),Ending::Gold,Planet,TEXT("Glow"));
    FRandomStream R(823);
    for(int I=0;I<1500;++I)
    {
        const float A=R.FRandRange(-PI,PI),Lat=R.FRandRange(-1.15,1.15);
        if(FMath::Sin(A*3+FMath::Sin(Lat*4)*2)+.6f*FMath::Sin(A*7-Lat*5)<.65f)continue;
        const FVector P(FMath::Cos(Lat)*FMath::Cos(A),FMath::Cos(Lat)*FMath::Sin(A),FMath::Sin(Lat));
        LandLights->AddInstance(FTransform(FRotator::ZeroRotator,P*4004,FVector(R.FRandRange(.022f,.06f))));
    }
    LandLights->ClearInstances();
    // A restrained, world-space backdrop becomes visible as the globe recedes.
    // Tiny instanced cubes are subpixel stars; they need no sphere tessellation.
    auto* OrbitStars=Instances(TEXT("EarthDistantStars"),TEXT("Cube"),FLinearColor(.24f,.35f,.52f),Planet,TEXT("Glow"));
    FRandomStream OrbitRandom(20162016);
    for(int I=0;I<1100;++I)
    {
        const FVector P(OrbitRandom.FRandRange(18000,34000),OrbitRandom.FRandRange(-38000,38000),OrbitRandom.FRandRange(-26000,26000));
        const float Size=OrbitRandom.FRandRange(.035f,.13f)*(I%31==0?1.8f:1.f);
        OrbitStars->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(.015f,Size,Size)));
    }
    if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Planet_Project/Assets/Static_Meshes/SM_Earth_Planet")))
    {
        const FVector Diameter=Mesh->GetBoundingBox().GetSize();
        for(auto* Surface:{EarthSurface,EarthShell,EarthClouds})
        {
            auto* Mat=Surface->GetMaterial(0);Surface->SetStaticMesh(Mesh);
            Surface->SetRelativeScale3D(FVector(Surface==EarthSurface?8000:Surface==EarthClouds?8075:8200)/Diameter);
            for(int Slot=0;Slot<Surface->GetNumMaterials();++Slot)Surface->SetMaterial(Slot,Mat);
        }
    }
    Universe=Group(TEXT("UniverseThatBecomesCytoplasm"),FVector::ZeroVector,Sets[3]);
    SolarSystem=Group(TEXT("SolarSystem"),FVector::ZeroVector,Universe);BuildSolarSystem();
    NebulaUniverse=Group(TEXT("NebulaUniverse"),FVector::ZeroVector,Universe);
    // Actual free star/nebula artwork is spread across depth layers. These
    // remain inside the enclosing cell and shrink with the same universe.
    for(int Layer=0;Layer<4;++Layer)
    {
        auto* Field=Shape(TEXT("DeepStarField"),FVector(-19000-Layer*4300,(Layer%2?1:-1)*12500,Layer*1700-2600),FVector(19000,21000,1),FLinearColor::White,TEXT("Plane"),NebulaUniverse,TEXT("SpaceField"));
        Field->SetRelativeRotation(FQuat(FVector::ForwardVector,Layer*.63f)*FRotator(90,0,0).Quaternion());
    }
    for(int Layer=0;Layer<3;++Layer)
    {
        auto* Field=Shape(TEXT("ParallaxNebula"),FVector(-12500-Layer*4300,(Layer%2?1:-1)*(6200+Layer*1400),Layer*2400-2400),FVector(16000,19000,1),FLinearColor::White,TEXT("Plane"),NebulaUniverse,TEXT("NebulaField"));
        Field->SetRelativeRotation(FQuat(FVector::ForwardVector,Layer*.71f)*FRotator(90,0,0).Quaternion());
    }
    auto* Stars=Instances(TEXT("Galaxies"),TEXT("Sphere"),FLinearColor(.32f,.5f,.85f),NebulaUniverse,TEXT("CosmicStar"));
    auto* GoldStars=Instances(TEXT("WarmStars"),TEXT("Sphere"),Ending::Gold,NebulaUniverse,TEXT("CosmicStar"));
    for(int Cluster=0;Cluster<13;++Cluster)
    {
        const FVector Center(-21000-Cluster*950,(Cluster%2?1:-1)*R.FRandRange(10500,19000),R.FRandRange(-7000,7000));
        const float Radius=Cluster?R.FRandRange(1100,2600):4200;
        auto* Haze=Shape(TEXT("GalaxyHaze"),Center,FVector(Radius*3,Radius*3,1),FLinearColor::White,TEXT("Plane"),NebulaUniverse,TEXT("NebulaField"));
        Haze->SetRelativeRotation(FRotator(65+Cluster%4*12,Cluster*47,0));
        for(int I=0;I<240;++I)
        {
            const float D=R.FRandRange(.03,1)*Radius,A=D/Radius*7+(I%3)*2*PI/3+R.FRandRange(-.18,.18);
            const FVector P=Center+Haze->GetRelativeRotation().RotateVector(FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,R.FRandRange(-.08,.08)*Radius));
            (I%3?Stars:GoldStars)->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(8,24)/100)));
        }
        auto* Core=Shape(TEXT("GalaxyCore"),Center,FVector(Radius*.10),Cluster%3?FLinearColor(.95f,.36f,.10f):FLinearColor(.25f,.55f,1.f),TEXT("Sphere"),NebulaUniverse,TEXT("StellarSurface"));
        Core->SetRelativeRotation(FRotator(Cluster*31,Cluster*67,23));
        auto* Corona=Shape(TEXT("StellarCorona"),Center+FVector(-Radius*.06f,0,0),FVector(Radius*.48f,Radius*.48f,1),Cluster%3?FLinearColor(1,.48f,.17f):FLinearColor(.30f,.52f,1.f),TEXT("Plane"),NebulaUniverse,TEXT("StellarCorona"));
        Corona->SetRelativeRotation(FQuat(FVector::ForwardVector,FMath::DegreesToRadians(Cluster*23.f))*FRotator(90,0,0).Quaternion());

    }
    auto* FarStars=Instances(TEXT("DistantStars"),TEXT("Sphere"),FLinearColor(.18f,.24f,.37f),Universe,TEXT("Glow"));
    for(int I=0;I<1400;++I)FarStars->AddInstance(FTransform(FRotator::ZeroRotator,R.VRand()*40000,FVector(R.FRandRange(.4,1.3))));
    BuildAbstractCells();
}

void AEndingEmergence::BuildAbstractCells()
{
    // Keep the outer universe cells symbolic; detailed anatomy is interior-only.
    for(auto* GroupRoot:{Cell.Get(),OtherCells.Get()})if(GroupRoot)
    {
        TArray<USceneComponent*> CellParts;GroupRoot->GetChildrenComponents(true,CellParts);
        for(int I=CellParts.Num()-1;I>=0;--I){RemoveInstanceComponent(CellParts[I]);CellParts[I]->DestroyComponent();}
        RemoveInstanceComponent(GroupRoot);GroupRoot->DestroyComponent();
    }
    FRandomStream R(823);
    Cell=Group(TEXT("RevealedCellMembrane"),FVector::ZeroVector,Sets[3]);
    Shape(TEXT("TransparentCell"),FVector::ZeroVector,FVector(3500,4200,3300),FLinearColor(.52f,.085f,.15f),TEXT("Sphere"),Cell,TEXT("LivingMembrane"));
    Shape(TEXT("GiantNucleus"),FVector(150,1130,340),FVector(560,510,540),FLinearColor(.19f,.045f,.24f),TEXT("FacetNucleus"),Cell,TEXT("CosmicOrganelle"));
    Shape(TEXT("GiantNucleolus"),FVector(-105,1030,390),FVector(115),FLinearColor(.40f,.15f,.26f),TEXT("FacetOrganelle"),Cell,TEXT("CosmicOrganelle"));
    for(int I=0;I<19;++I)
    {
        const float A=I*2*PI/19;
        const float Azimuth=A+R.FRandRange(-.11f,.11f),Spread=R.FRandRange(.73f,1.12f);
        auto* C=Shape(TEXT("GiantOrganelle"),FVector(R.FRandRange(-750,750),FMath::Cos(Azimuth)*1400*Spread,FMath::Sin(Azimuth)*1280*Spread),FVector(100,260,110)*R.FRandRange(.75f,1.15f),I%2?FLinearColor(.24f,.055f,.17f):FLinearColor(.14f,.055f,.25f),TEXT("FacetOrganelle"),Cell,TEXT("CosmicOrganelle"));
        C->SetRelativeRotation(FRotator(I*19,I*33,I*17));
    }
    // These structures exist at their final size from the beginning. The camera
    // crosses the enclosing membrane; nothing grows out of the universe's centre.
    Cell->SetRelativeScale3D(FVector(32));
    OtherCells=Group(TEXT("ManyUniversesInsideCells"),FVector::ZeroVector,Sets[3]);
    auto* SmallStars=Instances(TEXT("UniversesInOtherCells"),TEXT("Sphere"),FLinearColor(.32f,.43f,.70f),OtherCells,TEXT("Glow"));
    auto* WarmStars=Instances(TEXT("WarmUniversesInOtherCells"),TEXT("Sphere"),Ending::Gold*.55f,OtherCells,TEXT("Glow"));
    auto* Membranes=Instances(TEXT("DenseCellMembranes"),TEXT("Sphere"),FLinearColor(.52f,.085f,.15f),OtherCells,TEXT("LivingMembrane"));
    auto* Nuclei=Instances(TEXT("DenseCellNuclei"),TEXT("FacetNucleus"),FLinearColor(.19f,.045f,.24f),OtherCells,TEXT("CosmicOrganelle"));
    auto* Galaxies=Instances(TEXT("DenseCellGalaxies"),TEXT("Plane"),FLinearColor(.18f,.06f,.29f),OtherCells,TEXT("CellNebula"));
    UniverseCellCount=1;
    for(int Row=-11;Row<=11;++Row)for(int Col=-14;Col<=14;++Col)
    {
        if(Row==0&&Col==0)continue;
        ++UniverseCellCount;
        const FVector Center(R.FRandRange(-58000,58000),Col*145000+(FMath::Abs(Row)%2)*72500+R.FRandRange(-21000,21000),Row*128000+R.FRandRange(-18000,18000));
        const float Radius=R.FRandRange(58000,76000);
        Membranes->AddInstance(FTransform(FRotator(R.FRandRange(-12,12),0,R.FRandRange(-18,18)),Center,FVector(Radius*2,Radius*R.FRandRange(1.85f,2.15f),Radius*R.FRandRange(1.85f,2.1f))/100));
        Nuclei->AddInstance(FTransform(FRotator::ZeroRotator,Center+FVector(0,Radius*.60,Radius*.18),FVector(Radius*.30/100)));
        const FQuat NebulaFacing=FQuat(FVector::ForwardVector,R.FRandRange(-PI,PI))*FRotator(90,0,0).Quaternion();
        Galaxies->AddInstance(FTransform(NebulaFacing,Center,FVector(Radius*1.25,Radius*1.25,1)/100));
        for(int I=0;I<64;++I)
        {
            const float Dist=R.FRandRange(.015f,.7f)*Radius,A=Dist/Radius*11+(I%3)*2*PI/3;
            const FVector P=Center+FVector(FMath::Cos(A)*Dist*.45,FMath::Sin(A)*Dist,FMath::Cos(A)*Dist*.85);
            (I%3?SmallStars:WarmStars)->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(160,400)/100)));
        }
    }
}

void AEndingEmergence::ApplyCellVisuals()
{
    if(Sets.IsEmpty()||!Cell||!OtherCells)return;
    TArray<USceneComponent*> Parts;Sets[0]->GetChildrenComponents(true,Parts);
    for(int I=Parts.Num()-1;I>=0;--I)
    {
        auto* Part=Parts[I];const FString Name=Part->GetName();
        if(Name.StartsWith(TEXT("Organelle"))||Name.StartsWith(TEXT("CellNucleus"))||Name.StartsWith(TEXT("Nucleolus"))||Name.StartsWith(TEXT("CellDress")))
        {RemoveInstanceComponent(Part);Part->DestroyComponent();}
    }
    // Closed hard-faced silhouettes replace cutaway anatomy whose open surfaces
    // were exposed at player height. Keep a walkable central aisle without marks.
    Shape(TEXT("CellDressNucleusEnvelope"),FVector(950,-330,190),FVector(430,360,400),FLinearColor(.44f,.12f,.22f),TEXT("FacetNucleus"),Sets[0],TEXT("LivingMembrane"));
    Shape(TEXT("CellDressNucleus"),FVector(950,-330,190),FVector(320,280,310),FLinearColor(.21f,.075f,.23f),TEXT("FacetNucleus"),Sets[0],TEXT("FacetSurface"));
    Shape(TEXT("CellDressNucleolus"),FVector(795,-305,215),FVector(85),FLinearColor(.65f,.16f,.24f),TEXT("FacetOrganelle"),Sets[0],TEXT("FacetSurface"));
    for(int Side:{-1,1})for(int Row=0;Row<5;++Row)
    {
        const float X=180+Row*250,Y=Side*(280+35*(Row%2));
        if(Side==-1&&Row>1&&Row<4)continue;
        const FVector Center(X,Y,Ending::Ground(X,Y)+90);
        auto* Org=Group(TEXT("CellDressMitochondrion"),Center,Sets[0]);
        Org->SetRelativeRotation(FRotator(8+Row*5,Row*47+Side*23,Side*12));
        Shape(TEXT("CellDressMitoBody"),FVector::ZeroVector,FVector(250,118,100),FLinearColor(.42f,.075f,.09f),TEXT("FacetOrganelle"),Org,TEXT("FacetSurface"));
        for(int Fold=0;Fold<6;++Fold)
        {
            auto* Crista=Shape(TEXT("CellDressCrista"),FVector(-80+Fold*31,0,36),FVector(18,75,25),FLinearColor(.72f,.21f,.10f),TEXT("FacetOrganelle"),Org,TEXT("FacetSurface"));
            Crista->SetRelativeRotation(FRotator(0,Fold%2?18:-18,0));
        }
    }
    // Layered Golgi sacs remain intact rather than showing an anatomy cut plane.
    for(int Layer=0;Layer<5;++Layer)
    {
        auto* Sac=Shape(TEXT("CellDressGolgi"),FVector(640,360,80+Layer*27),FVector(240-Layer*18,115,22),FLinearColor(.55f,.12f,.21f),TEXT("FacetOrganelle"),Sets[0],TEXT("FacetSurface"));
        Sac->SetRelativeRotation(FRotator(0,-25+Layer*4,8));
    }
    BuildAbstractCells();
    MarkPackageDirty();
}

void AEndingEmergence::BuildAfternoon()
{
    auto* Set=Sets[4].Get();
    const FLinearColor Wood(.28f,.13f,.057f),Cream(.69f,.61f,.44f),Ink(.018f,.039f,.045f);
    Shape(TEXT("RoomFloor"),FVector(0,0,-14),FVector(1700,1500,25),FLinearColor(.12f,.14f,.135f),TEXT("Cube"),Set);
    Shape(TEXT("BackWall"),FVector(620,0,240),FVector(25,1500,500),Cream*.7f,TEXT("Cube"),Set);
    Shape(TEXT("WarmRoomWall"),FVector(-620,0,240),FVector(25,1500,500),Cream*.62f,TEXT("Cube"),Set);
    Shape(TEXT("SideWall"),FVector(0,680,240),FVector(1250,25,500),Cream*.6f,TEXT("Cube"),Set);
    Shape(TEXT("AfternoonWindow"),FVector(80,662,290),FVector(680,8,280),FLinearColor(.79f,.60f,.33f),TEXT("Cube"),Set,TEXT("Glow"));
    for(int I=-1;I<=1;++I) Shape(TEXT("WindowMullion"),FVector(80+I*220,650,290),FVector(8,10,290),Wood,TEXT("Cube"),Set);
    Shape(TEXT("WindowMullion"),FVector(80,650,290),FVector(690,10,9),Wood,TEXT("Cube"),Set);
    for(int I=0;I<4;++I)
    {
        auto* Patch=Shape(TEXT("SunPatch"),FVector(-100+I*150,100,1),FVector(95,650,2),FLinearColor(.29f,.23f,.12f),TEXT("Cube"),Set);
        Patch->SetRelativeRotation(FRotator(0,24,0));
    }
    Shape(TEXT("SharedTable"),FVector(0,0,78),FVector(310,230,12),Wood,TEXT("Cube"),Set);
    for(int X:{-1,1})for(int Y:{-1,1})Shape(TEXT("TableLeg"),FVector(X*127,Y*88,36),FVector(11,11,74),Wood*.65f,TEXT("Cube"),Set);
    Shape(TEXT("NotebookCover"),FVector(-30,0,86),FVector(78,103,3),Ending::Teal*.3f,TEXT("Cube"),Set);
    Shape(TEXT("NotebookPaper"),FVector(-30,0,88),FVector(74,99,1),FLinearColor(.84f,.78f,.60f),TEXT("Cube"),Set);
    Rod(FVector(-30,-48,89),FVector(-30,48,89),.55,Wood,Set,TEXT("Surface"));
    for(int I=0;I<8;++I) Rod(FVector(-62,-37+I*10,89),FVector(-36,-37+I*10,89),.25,Wood*.45f,Set,TEXT("Surface"));
    // The same five linked openings, now a level sketch in the notebook.
    for(int I=0;I<5;++I)
    {
        const FVector A(-24,-36+I*17,89.7),B(-4,-36+I*17,89.7);
        Sketch.Add(Rod(A,B,.8,Ink,Set,TEXT("Surface")));
        if(I<4) Sketch.Add(Rod(B,B+FVector(0,17,0),.8,Ink,Set,TEXT("Surface")));
        Sketch.Add(Shape(TEXT("SketchNode"),(A+B)*.5,FVector(3,3,.7),Ink,TEXT("Sphere"),Set));
    }
    Pencil=Group(TEXT("Pencil"),FVector(-27,-18,95),Set);
    Rod(FVector::ZeroVector,FVector(-5,-24,27),1.2,Ending::Gold,Pencil,TEXT("Surface"));
    Shape(TEXT("PencilTip"),FVector::ZeroVector,FVector(1.4,1.4,3),Ink,TEXT("Cone"),Pencil);
    Shape(TEXT("LaptopBase"),FVector(66,34,87),FVector(61,80,3),Ink,TEXT("Cube"),Set);
    auto* Screen=Shape(TEXT("LaptopScreen"),FVector(95,34,111),FVector(3,78,48),Ink,TEXT("Cube"),Set);Screen->SetRelativeRotation(FRotator(0,0,-10));
    Shape(TEXT("LaptopDisplay"),FVector(92,34,111),FVector(1,71,40),Ending::Blue*.6f,TEXT("Cube"),Set,TEXT("Glow"));
    for(int I=0;I<5;++I) Shape(TEXT("LaptopLines"),FVector(91,20,99+I*5),FVector(.4,38-I*3,1),Ending::Teal*.5f,TEXT("Cube"),Set,TEXT("Glow"));
    Shape(TEXT("Mug"),FVector(-10,-75,97),FVector(15,15,23),Cream,TEXT("Cylinder"),Set);
    Shape(TEXT("Coffee"),FVector(-10,-75,109),FVector(13,13,.5),Wood*.12f,TEXT("Cylinder"),Set);
    // Authored in view space, attached to the rail camera at runtime. A distinct
    // thumb and four articulated fingers make the final snap readable in first person.
    EndingHand=Group(TEXT("FirstPersonRealityHand"),FVector::ZeroVector,Set);
    const FLinearColor Skin(.62f,.36f,.22f);
    Rod(FVector(-12,1,-32),FVector(0,0,-5),8.5f,Ending::Teal*.35f,EndingHand,TEXT("Surface"));
    Shape(TEXT("Wrist"),FVector(0,0,-6),FVector(7,7,12),Skin,TEXT("Sphere"),EndingHand);
    Shape(TEXT("Palm"),FVector(0,0,2),FVector(5.8,12,14),Skin,TEXT("Sphere"),EndingHand);
    for(int Finger=0;Finger<5;++Finger)for(int Joint=0;Joint<3;++Joint)
        Fingers.Add(Shape(TEXT("FingerJoint"),FVector::ZeroVector,FVector(3),Skin,TEXT("Sphere"),EndingHand));
    EndingTool=Group(TEXT("ReturnedPickaxe"),FVector::ZeroVector,EndingHand);
    ReturnedPickaxe=Shape(TEXT("MainGamePickaxe"),FVector::ZeroVector,FVector(65),FLinearColor::White,TEXT("Cube"),EndingTool);
    ReturnedPickaxe->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Pickaxe/SM_Pickaxe.SM_Pickaxe")));
    ReturnedPickaxe->EmptyOverrideMaterials();
    EndingHand->SetVisibility(false,true);
    // Plant, pinboard and loose notes keep the final room recognizably human.
    Shape(TEXT("Pinboard"),FVector(600,-330,235),FVector(12,360,190),Wood*.7f,TEXT("Cube"),Set);
    for(int I=0;I<9;++I)Shape(TEXT("StickyNote"),FVector(591,-460+(I%3)*110,175+(I/3)*55),FVector(2,60,38),I%2?Cream:Ending::Gold*.55f,TEXT("Cube"),Set);
    Shape(TEXT("PlantPot"),FVector(440,460,30),FVector(90,90,60),Wood,TEXT("Cylinder"),Set);
    for(int I=0;I<9;++I)
    {
        auto* Leaf=Shape(TEXT("Leaf"),FVector(440+FMath::Cos(float(I))*55,460+FMath::Sin(float(I))*55,110+I*8),FVector(24,85,20),FLinearColor(.06f,.16f,.08f),TEXT("Sphere"),Set);
        Leaf->SetRelativeRotation(FRotator(I*15,I*40,0));
    }
}

void AEndingEmergence::BuildDemo()
{
    if(!GetWorld()||GetWorld()->IsGameWorld())return;
    for(ADiggableRock* G:Gates)if(IsValid(G))G->Destroy();Gates.Empty();
    TArray<UActorComponent*> Old;GetComponents(Old);
    for(int I=Old.Num()-1;I>=0;--I)if(Old[I]->ComponentHasTag(Ending::Tag)){RemoveInstanceComponent(Old[I]);Old[I]->DestroyComponent();}
    Sets.Empty();Nerves.Empty();Sketch.Empty();Blood.Empty();BloodOrigins.Empty();CrowdOrigins.Empty();Fingers.Empty();NeuronPositions.Empty();NeuronCores.Empty();AmbientNeuronCores.Empty();
    Sets.Add(Group(TEXT("01_PlayableCellsBloodBrain"),FVector::ZeroVector));
    Sets.Add(Group(TEXT("02_CityAndPeople"),Ending::City));
    Sets.Add(Group(TEXT("03_Earth"),Ending::Earth));
    Sets.Add(Group(TEXT("04_UniverseInsideCell"),Ending::Cosmos));
    Sets.Add(Group(TEXT("05_ThatAfternoon"),Ending::Desk));
    BuildJourney();BuildCity();ApplyCityBuildings();BuildSpace();ApplyCellVisuals();BuildAfternoon();
    for(int I=1;I<Sets.Num();++I)Sets[I]->SetVisibility(false,true);
    MarkPackageDirty();
}

void AEndingEmergence::BeginPlay()
{
    Super::BeginPlay();
    if(CrowdAssets&&Sets.IsValidIndex(1))
    {
        StreetCrowd=NewObject<UEndingCrowdComponent>(this,TEXT("CitySamplePedestrians"));
        AddInstanceComponent(StreetCrowd);StreetCrowd->SetupAttachment(Sets[1]);StreetCrowd->RegisterComponent();
        if(StreetCrowd->Build(CrowdAssets))
        {
            if(Crowd)Crowd->ClearInstances();
            if(CrowdHeads)CrowdHeads->ClearInstances();
            TArray<USceneComponent*> Placeholders;
            if(EyePerson)EyePerson->GetChildrenComponents(true,Placeholders);
            for(auto* C:Placeholders)if(auto* M=Cast<UStaticMeshComponent>(C))M->SetStaticMesh(nullptr);
        }
        else{StreetCrowd->DestroyComponent();StreetCrowd=nullptr;}
    }
    // Generated shell geometry is saved, but its transient cooked body is not.
    // Recreate collision after loading/PIE duplication before the pawn can move.
    TArray<UDynamicMeshComponent*> Shells;GetComponents(Shells);
    for(auto* ShellMesh:Shells)
    {
        ShellMesh->bUseAsyncCooking=false;
        ShellMesh->SetComplexAsSimpleCollisionEnabled(true,false);
        ShellMesh->GetBodySetup()->bDoubleSidedGeometry=false;
        ShellMesh->UpdateCollision(false);
        ShellMesh->RecreatePhysicsState();
    }
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_VignetteIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.VignetteIntensity=.3;
    // These shots use authored emissive shading. Avoid building a Lumen scene
    // for the crowd/city on its first reveal; the campaign camera is unchanged.
    auto& EndingPP=Camera->GetCameraComponent()->PostProcessSettings;
    EndingPP.bOverride_DynamicGlobalIlluminationMethod=true;
    EndingPP.DynamicGlobalIlluminationMethod=EDynamicGlobalIlluminationMethod::None;
    EndingPP.bOverride_ReflectionMethod=true;
    EndingPP.ReflectionMethod=EReflectionMethod::None;
}

bool AEndingEmergence::Initialize()
{
    Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->GetPawn()||!Player->InputComponent||Gates.Num()!=1||Sets.Num()!=5||NeuronPositions.Num()!=Ending::NeuronCount||VesselPath.Num()<2)return false;
    Mining=Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(!Mining)return false;
    if(auto* PC=Cast<ABrainMiningPlayerController>(Player))
    {
        PC->SetMiningGUIEnabled(false);
        if(PC->InteractionHUD)PC->InteractionHUD->SetVisibility(ESlateVisibility::Collapsed);
    }
    FisheyeMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_Fisheye"));
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MouseMuseum/MI_EndingOilPaint.MI_EndingOilPaint")))
        PainterlyMaterial=UMaterialInstanceDynamic::Create(Base,this);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_PulseTransit")))
    {TransitMaterial=UMaterialInstanceDynamic::Create(Base,this);Camera->GetCameraComponent()->AddOrUpdateBlendable(TransitMaterial,1);}
    // Local instance only: the authored route is a short, grounded finale.
    if(auto* M=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())M->FlightSettings.bEnabled=false;
    CacheCellMotion();InitializeScaleMotion();
    // Keep transition audio resident: first playback must not flush loading on a cut.
    for(const TCHAR* Name:{TEXT("Cell"),TEXT("Blood"),TEXT("Brain"),TEXT("Street"),TEXT("Space"),TEXT("Connect"),TEXT("PulseFlight"),TEXT("Hit")})
        EndingSounds.Add(FName(Name),LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Audio/S_EE_"))+Name)));
    SavedFieldResetEnabled=Mining->bFieldResetEnabled;Mining->bFieldResetEnabled=false;
    if(GEngine&&GEngine->GameViewport)
    {
        SAssignNew(View,SEmergenceEndingView).Owner(this);View->SetVisibility(EVisibility::HitTestInvisible);
        GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),100);
    }
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_Exhaustion")))
    {ExhaustionMaterial=UMaterialInstanceDynamic::Create(Base,this);Camera->GetCameraComponent()->AddOrUpdateBlendable(ExhaustionMaterial,1);}
    // Copy the active pawn's actual mesh and material overrides, including its current style.
    TArray<UStaticMeshComponent*> PawnParts;Player->GetPawn()->GetComponents(PawnParts);
    for(auto* Part:PawnParts)if(Part->GetName()==TEXT("PickaxeModel"))
    {
        ReturnedPickaxe->SetStaticMesh(Part->GetStaticMesh());
        ReturnedPickaxe->SetRelativeTransform(Part->GetRelativeTransform());
        ReturnedPickaxe->SetOverlayMaterial(Part->GetOverlayMaterial());
        for(int I=0;I<Part->GetNumMaterials();++I)ReturnedPickaxe->SetMaterial(I,Part->GetMaterial(I));
    }
    // Present the broad face of the real axe instead of its thin edge.
    EndingTool->SetRelativeRotation(FRotator(0,75,0));
    if(auto* Effects=Player->GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>())
        for(const auto& Asset:Effects->DebrisMeshAssets)if(auto* Mesh=Asset.LoadSynchronous())EndingDebrisMeshes.Add(Mesh);
    EndingHand->AttachToComponent(Camera->GetCameraComponent(),FAttachmentTransformRules::KeepRelativeTransform);
    EndingHand->SetVisibility(false,true);
    ToolLight=NewObject<UPointLightComponent>(this,NAME_None,RF_Transient);
    ToolLight->SetupAttachment(Camera->GetCameraComponent());ToolLight->SetRelativeLocation(FVector(10,-20,35));
    ToolLight->SetIntensity(900);ToolLight->SetAttenuationRadius(350);ToolLight->SetCastShadows(false);ToolLight->RegisterComponent();ToolLight->SetVisibility(false);
    // Arrival waves illuminate distant nodes and links only. A shell around the
    // host crossed the camera near plane and caused a full-screen green flash.
    BuildFinale();
    SetAmbience(TEXT("Cell_Loop"),.26f);return true;
}

void AEndingEmergence::WarmStreetRendering()
{
    if(!StreetCrowd||!Sets.IsValidIndex(1))return;
    // Render the street once offscreen while the cell scene initializes. Its
    // shaders and instance buffers are ready before the thought-to-street fade.
    Sets[1]->SetVisibility(true,true);
    if(NightContinent)NightContinent->SetVisibility(false,true);
    StreetCrowd->UpdateCrowd(true,0);
    auto* Target=NewObject<UTextureRenderTarget2D>(this);
    Target->InitAutoFormat(256,144);Target->UpdateResourceImmediate(true);
    auto* Capture=NewObject<USceneCaptureComponent2D>(this);
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
    Capture->bAlwaysPersistRenderingState=true;
    Capture->TextureTarget=Target;Capture->CaptureSource=ESceneCaptureSource::SCS_FinalToneCurveHDR;
    Capture->FOVAngle=75;Capture->RegisterComponent();
    Capture->PostProcessSettings=Camera->GetCameraComponent()->PostProcessSettings;
    const FVector Position=GetActorTransform().TransformPosition(Ending::City+FVector(-1700,-900,3200));
    const FVector Look=GetActorTransform().TransformPosition(Ending::City+FVector(0,0,160));
    Capture->SetWorldLocationAndRotation(Position,(Look-Position).Rotation());
    GetWorld()->SendAllEndOfFrameUpdates();FlushRenderingCommands();
    Capture->CaptureScene();
    // Warm the remaining scale sets before gameplay as well. Otherwise their
    // first visible frame has to create render resources exactly on the cut.
    Capture->PostProcessSettings.WeightedBlendables.Array.Empty();
    auto Warm=[&](int SetIndex,FVector Eye,FVector Aim,float Fov)
    {
        for(int I=0;I<Sets.Num();++I)Sets[I]->SetVisibility(I==SetIndex,true);
        if(SetIndex==3){Cell->SetVisibility(false,true);OtherCells->SetVisibility(false,true);}
        if(SetIndex==1)
        {
            TArray<USceneComponent*> Parts;Sets[1]->GetChildrenComponents(false,Parts);
            for(auto* Part:Parts)Part->SetVisibility(Part==NightContinent,true);
        }
        Capture->FOVAngle=Fov;
        const FVector WorldEye=GetActorTransform().TransformPosition(Eye);
        Capture->SetWorldLocationAndRotation(WorldEye,(GetActorTransform().TransformPosition(Aim)-WorldEye).Rotation());
        // Publish visibility before warming this set's rendering resources.
        GetWorld()->SendAllEndOfFrameUpdates();FlushRenderingCommands();
        Capture->CaptureScene();
        FlushRenderingCommands();
    };
    if(NightContinent)NightContinent->SetVisibility(true,true);
    Warm(1,Ending::City+FVector(-110000,-30000,130000),Ending::City+FVector(15000,0,-1000),60);
    Warm(2,Ending::Earth+FVector(-5200,-1100,1300),Ending::Earth,68);
    Cell->SetVisibility(false,true);OtherCells->SetVisibility(false,true);
    const FVector SolarLook(FMath::Cos(2*.93f+.35f)*2400*.18f,FMath::Cos(2*.93f+.35f)*2400,FMath::Sin(2*.93f+.35f)*2400*.55f);
    Warm(3,Ending::Cosmos+FVector(-14500,-1800,4300),Ending::Cosmos+SolarLook,62);
    Warm(3,Ending::Cosmos+FVector(-760000,-135000,230000),Ending::Cosmos,62);
    // Finish this one-time initialization work before the playable camera moves.
    FlushRenderingCommands();
    Capture->DestroyComponent();
    for(int I=0;I<Sets.Num();++I)Sets[I]->SetVisibility(I==0,true);
    StreetCrowd->UpdateCrowd(false,0);
    bScaleWarmupComplete=true;
}

void AEndingEmergence::Sound(const TCHAR* Name,float Volume,float Pitch)
{
    auto& Cached=EndingSounds.FindOrAdd(FName(Name));
    if(!Cached)Cached=LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Audio/S_EE_"))+Name));
    if(Cached)UGameplayStatics::PlaySound2D(this,Cached,Volume,Pitch);
}

void AEndingEmergence::SetAmbience(const TCHAR* Name,float Volume)
{
    if(Ambience){Ambience->FadeOut(.8f,0);Ambience->bAutoDestroy=true;Ambience=nullptr;}
    const FString AssetName(Name);
    const FString Prefix=AssetName.EndsWith(TEXT("_Loop"))?TEXT("A_EE_"):TEXT("S_EE_");
    const FString Path=FString(TEXT("/Game/EndingEmergence/Audio/"))+Prefix+AssetName;
    auto& Cached=EndingSounds.FindOrAdd(FName(Name));
    if(!Cached)Cached=LoadObject<USoundBase>(nullptr,*Path);
    if(auto* S=Cached.Get())
    {
        Ambience=UGameplayStatics::CreateSound2D(this,S,0,1,0,nullptr,false,false);
        if (Ambience && GetGameInstance())
        {
            Ambience->SoundClassOverride=GetGameInstance()->GetSubsystem<UMiningAudioSettings>()->GetClass(3);
            Ambience->SetVolumeMultiplier(Volume);Ambience->Play();
        }
    }
    else UE_LOG(LogTemp,Warning,TEXT("ENDING_AUDIO_MISSING_AMBIENCE %s"),*Path);
}

void AEndingEmergence::SetMusic(const TCHAR* Name,float Volume)
{
    const FString AssetName(Name);
    const FString Path=FString(TEXT("/Game/EndingEmergence/Audio/M_EE_"))+AssetName;
    if(auto* S=LoadObject<USoundBase>(nullptr,*Path))
    {
        if(Music && Music->Sound==S && Music->IsPlaying())
        {
            Music->SetVolumeMultiplier(Volume);
            return;
        }
        if(Music){Music->Stop();Music->DestroyComponent();Music=nullptr;}
        Music=UGameplayStatics::CreateSound2D(this,S,0,1,0,nullptr,false,false);
        if (Music)
        {
            if(GetGameInstance())
                if(auto* Settings=GetGameInstance()->GetSubsystem<UMiningAudioSettings>())Music->SoundClassOverride=Settings->GetClass(1);
            Music->bIsUISound=true;
            Music->SetVolumeMultiplier(Volume);Music->FadeIn(.8f,1.f);
            UE_LOG(LogTemp,Display,TEXT("ENDING_MUSIC_PLAY %s volume=%.2f duration=%.2f"),*Path,Volume,S->GetDuration());
        }
    }
    else UE_LOG(LogTemp,Warning,TEXT("ENDING_AUDIO_MISSING_MUSIC %s"),*Path);
}

void AEndingEmergence::RestartDemo()
{
    if(bRestoring||!Player||!Mining)return;
    TGuardValue<bool> Guard(bRestoring,true);
    ResetFinale();ResetScaleMotion();PrepareFinaleRocks();
    GateIndex=0;Shot=-1;ShotTime=0;Age=0;BeatAt=0;LastCuts=0;bComplete=false;BloodTravel=0;
    CurrentNeuron=TargetNeuron=TravellingToNeuron=INDEX_NONE;PossessionCount=0;VisitedNeurons.Empty();BloodSpeed=480;VesselDistance=150;BloodY=0;PulseAt=-100;BufferedJumpAt=-100;bEnteringBrain=false;TransitEffects(0);
    if(DroppedPickaxe){DroppedPickaxe->DestroyComponent();DroppedPickaxe=nullptr;}
    DriftOffset=DriftVelocity=FVector::ZeroVector;
    PullbackBoost=0;PullbackRate=1;
    for(ADiggableRock* G:Gates)if(IsValid(G)){G->ResetRock();G->SetActorHiddenInGame(false);}
    for(int I=0;I<Sets.Num();++I)Sets[I]->SetVisibility(I==0,true);
    if(PulseTrail)PulseTrail->SetVisibility(false);
    const FLinearColor GoalBlue=Ending::GoalBlue;
    for(int I=0;I<NeuronCores.Num();++I)if(auto* M=Cast<UMaterialInstanceDynamic>(NeuronCores[I]->GetMaterial(0)))M->SetVectorParameterValue(TEXT("Tint"),I==NeuronPositions.Num()-1?GoalBlue*2:Ending::Neural*.65f);
    Universe->SetRelativeScale3D(FVector(1));Cell->SetRelativeScale3D(FVector(32));
    ClearEndingEffects();
    RealityHits=0;LastStrike=-100;bImpactPending=false;UniverseSnapshot=nullptr;
    if(Voice){Voice->Stop();Voice->DestroyComponent();Voice=nullptr;}
    EndingHand->SetVisibility(false,true);
    for(UStaticMeshComponent* S:Sketch)S->SetVisibility(false);
    Player->ResetIgnoreMoveInput();Player->ResetIgnoreLookInput();
    Player->GetPawn()->SetActorHiddenInGame(false);
    // Hide both body and first-person arms without hiding their camera/tool children.
    TArray<USkeletalMeshComponent*> PlayerBodies;Player->GetPawn()->GetComponents(PlayerBodies);
    for(auto* Body:PlayerBodies){Body->SetHiddenInGame(true,false);Body->SetVisibility(false,false);Body->SetCastShadow(false);}
    Player->GetPawn()->SetActorLocation(GetActorTransform().TransformPosition(FVector(100,0,Ending::Ground(100)+110)),false,nullptr,ETeleportType::TeleportPhysics);
    Player->SetControlRotation(GetActorRotation());
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);}
    if(auto* C=Player->GetPawn()->FindComponentByClass<UCameraComponent>()){C->SetFieldOfView(90);if(FisheyeMaterial)C->AddOrUpdateBlendable(FisheyeMaterial,0);}
    Player->SetViewTarget(Player->GetPawn());Mining->SetInputSuspended(false);Mining->SetInputContextEnabled(true);
    bFinalResolvePlayed=false;UpdatePainterly();UpdateNerves();SetAmbience(TEXT("Cell_Loop"),.26f);
    if(Music){Music->Stop();Music->DestroyComponent();Music=nullptr;}
    SetMusic(TEXT("LightThought_Loop"),.45f);
}

void AEndingEmergence::PreviewFilm()
{
    if(!bReady||Shot>=0)return;
    bEnteringBrain=false;TravellingToNeuron=INDEX_NONE;TransitEffects(0);
    Mining->SetInputSuspended(true);Mining->SetInputContextEnabled(false);
    Player->SetIgnoreMoveInput(true);Player->SetIgnoreLookInput(true);
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();}
    Player->GetPawn()->SetActorHiddenInGame(true);Player->SetViewTarget(Camera);
    for(ADiggableRock* G:Gates)G->SetActorHiddenInGame(true);
    SetShot(0);UpdateFilm(0);
    UpdatePainterly();
}

float AEndingEmergence::Duration(int32 Index)
{
    const float Seconds[]={5,10,8,11,35,20,18,12,4.5f,0,3.7f,6.6f,2.8f,3.f,0};
    return Index>=0&&Index<UE_ARRAY_COUNT(Seconds)?Seconds[Index]:0;
}

void AEndingEmergence::SetShot(int32 Index)
{
    if(Index==0&&Shot<0&&GateIndex>=5)
    {if(!bScaleBridgePrepared)CaptureScaleBridge();bScaleBridgeReady=true;}
    else bScaleBridgeReady=Index>=2&&Index<=4&&Index==Shot+1&&bScaleBridgePrepared&&bScaleBridgeReady;
    bScaleBridgePrepared=false;
    Shot=Index;ShotTime=0;
    // Never briefly reactivate the entire cosmos at paper chapter boundaries.
    const int SetIndex=Shot>=9?INDEX_NONE:Shot<=2?1:Shot==3?2:3;
    for(int I=0;I<Sets.Num();++I)if(Sets[I]->IsVisible()!=(I==SetIndex))Sets[I]->SetVisibility(I==SetIndex,true);
    if(NightContinent)NightContinent->SetVisibility(Shot==2,true);
    if(Shot<=2)
    {
        TArray<USceneComponent*> CityParts;Sets[1]->GetChildrenComponents(false,CityParts);
        for(auto* Part:CityParts)if(Part!=NightContinent)Part->SetVisibility(Shot<2,true);
        EyePerson->SetVisibility(Shot<=1,true);
    }
    if(Shot==4){Universe->SetRelativeScale3D(FVector(1));Cell->SetRelativeScale3D(FVector(32));}
    if(Shot>=4&&Shot<=8)UpdateCosmos(Shot==4?0:1);
    EndingHand->SetVisibility(false,true);EndingTool->SetVisibility(false,true);
    if(ToolLight)ToolLight->SetVisibility(false);
    if(FinaleCellSpecks)FinaleCellSpecks->SetVisibility(Shot>=6&&Shot<=8);
    if(FinaleEntrancePreview)FinaleEntrancePreview->SetVisibility(Shot==8,true);
    if(Shot==7)
    {
        PrepareFinaleRocks();
        TitleCameraStart=GetActorTransform().InverseTransformPosition(Camera->GetActorLocation())-Ending::Cosmos;
        TitleCameraVelocity=GetActorTransform().InverseTransformVector(FilmCameraVelocity);
        TitleLookStart=Camera->GetActorRotation();TitleUpdateAt=-1;
        DriftVelocity=FVector::ZeroVector;Exhaustion=0;
        if(Ambience)Ambience->AdjustVolume(2,.22f);

    }
    if(Shot>=1&&Shot<=6)Sound(Shot%2?TEXT("ScaleWhoosh_01"):TEXT("ScaleWhoosh_02"),.22f,.9f+Shot*.035f);
    if(Shot==8){UpdateCellTitle(1);Sound(TEXT("PulseFlight"),.7f,.6f);Sound(TEXT("NeuralPulse_01"),.3f,.92f);}
    if(Shot==9){BeginFinaleDrill();}
    if(Shot==10)
    {
        PrepareFinaleVideo();
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),37.5f);
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("DotsReveal"),0);
        EndFinaleDrill(true);SetAmbience(TEXT("Cell_Loop"),.10f);
        PaperScene->SetRelativeRotation(FRotator(90,0,0));PaperScene->SetVisibility(true);
        // Start with only the circular portal visible. The desktop set is
        // revealed after the camera has entered the opening, preventing a
        // square sheet or its props from appearing around the distant hole.
        TArray<USceneComponent*> PaperParts;PaperScene->GetChildrenComponents(true,PaperParts);
        for(auto* Part:PaperParts)if(Part!=PaperSurface)Part->SetVisibility(false,true);
        if(PaperSurface)PaperSurface->SetVisibility(true,true);
        FinaleTunnel->SetVisibility(false,true);FinaleLight->SetVisibility(false);PaperSurface->SetVisibility(true);
        for(auto Rock:FinaleRocks)if(IsValid(Rock))Rock->SetActorHiddenInGame(true);
        Sound(TEXT("Connect"),.45f,.7f);Sound(TEXT("ScaleWhoosh_01"),.28f,.72f);
    }
    if(Shot==11)
    {
        EndFinaleDrill();
        if(ExitInkMaterial)ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),0);
        FinaleTunnel->SetVisibility(false,true);PaperScene->SetRelativeRotation(FRotator::ZeroRotator);TransitEffects(0);
        for(auto Rock:FinaleRocks)if(IsValid(Rock))Rock->SetActorHiddenInGame(true);
        SetAmbience(TEXT("Cell_Loop"),.06f);
    }
    if(Shot>=13)
    {
        PaperScene->SetVisibility(false,true);PaperCaveScene->SetVisibility(true,true);
        TransitEffects(0);if(ExitInkMaterial)ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),0);
        SetAmbience(TEXT("Afternoon"),.24f);
        if(!bFinalResolvePlayed){Sound(TEXT("FinalComputer"),.55f,.96f);bFinalResolvePlayed=true;}
    }
    if(Shot==0){SetAmbience(TEXT("Street"),.28f);Sound(TEXT("ScaleWhoosh_01"),.32f,.86f);}
    if(Shot==3){SetAmbience(TEXT("Space"),.18f);Sound(TEXT("ScaleWhoosh_02"),.34f,.82f);}
    UE_LOG(LogTemp,Display,TEXT("ENDING_SHOT %d"),Shot);
}

void AEndingEmergence::Pose(FVector Position,FVector LookAt,float Fov,float Roll)
{
    if(Shot>=1&&Shot<=6)Position=LookAt+FRotator(0,DriftOffset.Y,0).RotateVector(Position-LookAt);
    const FTransform T=GetActorTransform();
    FRotator Rot=(T.TransformPosition(LookAt)-T.TransformPosition(Position)).Rotation();Rot.Roll=Roll;
    Camera->SetActorLocationAndRotation(T.TransformPosition(Position),Rot);Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void AEndingEmergence::UpdateFilm(float Dt)
{
    const float Progress=FMath::Clamp(ShotTime/FMath::Max(1.f,Duration(Shot)),0.f,1.f);
    // Cross-scene launches accelerate outward. The continuous cosmic rail below
    // has its own joined distance curve, so it never brakes at a chapter boundary.
    const float T=Shot>=0&&Shot<=6?Progress:Ending::Ease(Progress);
    if(FinaleCellSpecks)if(auto* Mat=Cast<UMaterialInstanceDynamic>(FinaleCellSpecks->GetMaterial(0)))
        Mat->SetScalarParameterValue(TEXT("Fade"),Shot==6?Ending::Ease((T-.30f)/.30f):Shot>=7?1.f:0.f);
    const float W=bComfortMode?0:FMath::Clamp(Weightlessness,0.f,1.f);
    if(Shot>=1&&Shot<=6)
    {
        // Exponential damping is stable at different frame rates. Input changes velocity,
        // not position, so release visibly coasts. Bound the offset to protect composition.
        const FVector Push(0,UMiningGamepadSubsystem::ReadMove(Player).X,0);
        const double Decay=FMath::Exp(-2.2*Dt);
        const FVector Target=Push*3.5*W;
        DriftVelocity=DriftVelocity*Decay+Target*(1-Decay);
        DriftOffset=(DriftOffset+DriftVelocity*Dt).GetClampedToMaxSize(4);
        if(W==0){DriftVelocity=DriftOffset=FVector::ZeroVector;}
    }
    switch(Shot)
    {
    case 0: Pose(Ending::City+Ending::ScaleRail(FVector(38,0,195),FVector(-160,-80,650),T),Ending::City+FVector(0,0,160),75);break;
    case 1:
        if(!EyePerson->IsVisible())EyePerson->SetVisibility(true,true);
        Pose(Ending::City+Ending::ScaleRail(FVector(-160,-80,650),FVector(-1000,-550,3200),T),Ending::City+FMath::Lerp(FVector(0,0,160),FVector(100,0,75),T),FMath::Lerp(75.f,64.f,T));break;
    case 2:
        Pose(Ending::City+Ending::ScaleRail(FVector(-110000,-30000,130000),FVector(-180000,-45000,220000),T),Ending::City+FVector(15000,0,-1000),60);break;
    case 3:
        // Keep the front-facing rail, but leave room above the lit limb later on.
        Pose(Ending::Earth+Ending::ScaleRail(FVector(-5200,-1100,1300),FVector(-15000,-3400,4500),T),Ending::Earth+FVector(0,0,650*Ending::Ease((T-.18f)/.45f)),
            FMath::Lerp(68.f,66.f,T),FMath::Sin(ShotTime*.12)*1.3*W);break;
    case 4: case 5: case 6: UpdateCosmicRail();break;
    case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: UpdateFinale(Dt);break;
    }
    // The jump lifts and dollies the camera in world space with a fixed lens.
    // Handoff starts before the authored scene boundary can become visible.
    UpdateScaleMotion(Dt);
    Exhaustion=0;
    if((Shot==5||Shot==6)&&Age-TitleUpdateAt>.05f){UpdateCellTitle(0);TitleUpdateAt=Age;}
    if(ExhaustionMaterial)
    {
        ExhaustionMaterial->SetScalarParameterValue(TEXT("Gray"),Exhaustion);
        ExhaustionMaterial->SetScalarParameterValue(TEXT("RippleAge"),100);
        ExhaustionMaterial->SetScalarParameterValue(TEXT("ShakeStrength"),bComfortMode?.0006f:(RealityHits==3?.009f:.002f));
    }

}

float AEndingEmergence::StrikeReadiness() const
{
    return FMath::Clamp((Age-LastStrike)/.95f,0.f,1.f);
}

void AEndingEmergence::Speak(const TCHAR* Name)
{
    if(Voice){Voice->Stop();Voice->DestroyComponent();Voice=nullptr;}
    if(auto* S=LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Audio/S_EE_"))+Name)))
    {
        Voice=UGameplayStatics::CreateSound2D(this,S,.95f,1,0,nullptr,false,false);
        if (Voice && GetGameInstance())
        {
            Voice->SoundClassOverride=GetGameInstance()->GetSubsystem<UMiningAudioSettings>()->GetClass(4);
            Voice->Play();
        }
    }
}

void AEndingEmergence::StrikeUniverse()
{
    // A forgiving recovery beat, not a timing score: early clicks never erase progress.
    if(Shot!=8||RealityHits>=3||StrikeReadiness()<1||bImpactPending)return;
    LastStrike=Age;bImpactPending=true;Sound(TEXT("Swing"),.6f);
}

void AEndingEmergence::BuildStarlightAndPaper()
{
    FRandomStream R(605);
    // Dense bright sparks surge through the aperture, then arc down under gravity.
    for(int I=0;I<540;++I)
    {
        auto* Drop=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transient);
        Drop->SetupAttachment(Camera->GetCameraComponent());Drop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        auto* SparkMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_StarlightSpark")),this);
        SparkMaterial->SetVectorParameterValue(TEXT("Tint"),I%4==0?FLinearColor(.22f,.65f,1.f):I%7==0?FLinearColor(.75f,.5f,1.f):FLinearColor(1.f,.62f,.19f));
        SparkMaterial->SetScalarParameterValue(TEXT("Seed"),I*.173f);Drop->SetMaterial(0,SparkMaterial);
        Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);Drop->SetCastShadow(false);Drop->SetVisibleInRayTracing(false);Drop->RegisterComponent();Drop->SetVisibility(false);
        const float Size=R.FRandRange(.06f,.24f);Drop->SetRelativeScale3D(FVector(Size*R.FRandRange(3,8),Size,Size)/100);
        StarlightSparks.Add(Drop);SparkOrigins.Add(FVector(R.FRandRange(37,44),R.FRandRange(-3,3),R.FRandRange(-2,2)));
        SparkVelocities.Add(FVector(R.FRandRange(-29,-15),R.FRandRange(-26,26),R.FRandRange(8,35)));
    }
    for(int I=0;I<12;++I)
    {
        auto* Jet=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);
        Jet->SetupAttachment(Camera->GetCameraComponent());Jet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Jet->SetCastShadow(false);Jet->SetEnableRaytracing(false);
        auto* M=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_StarlightStream")),this);
        M->SetScalarParameterValue(TEXT("Seed"),I*.173f);Jet->SetMaterial(0,M);Jet->RegisterComponent();Jet->SetVisibility(false);StarlightRibbons.Add(Jet);
    }
    for(int I=0;I<15;++I)
    {
        const bool Final=I==14;
        const int Idea=Final?10:1+I%9;
        auto* Photo=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/EndingEmergence/Textures/T_EE_Idea%d"),Idea));
        const FVector2D CropSize=Final?FVector2D(1,1):FVector2D(R.FRandRange(.72f,1.f),R.FRandRange(.68f,1.f));
        const FVector2D CropOffset=Final?FVector2D::ZeroVector:FVector2D(R.FRandRange(0,1-CropSize.X),R.FRandRange(0,1-CropSize.Y));
        float Aspect=16.f/9;
        // Imported editor textures may still return square placeholder dimensions
        // while compiling. Source dimensions are stable for the supplied photos.
#if WITH_EDITORONLY_DATA
        if(Photo&&Photo->Source.GetSizeX()>0)Aspect=float(Photo->Source.GetSizeY())/Photo->Source.GetSizeX();
#else
        if(Photo&&Photo->GetSizeX()>0)Aspect=float(Photo->GetSizeY())/Photo->GetSizeX();
#endif
        PaperCenters.Add(Final?FVector2D(0,.015):FVector2D(R.FRandRange(-.78f,.78f),R.FRandRange(-.15f,.65f)));
        // X is viewport width fraction, Y is the true aspect after rotation/crop.
        PaperSizes.Add(FVector2D(Final?.70f:R.FRandRange(.22f,.43f),Aspect*CropSize.X/CropSize.Y));
        PaperRolls.Add(Final?-3.f:R.FRandRange(-38.f,38.f));
        PaperMotion.Add(FVector4(Final?5.65f:.70f+I*.235f+R.FRandRange(-.09f,.09f),Final?1.15f:R.FRandRange(.60f,.95f),R.FRandRange(.95f,1.55f),R.FRandRange(0.f,2*PI)));
        auto* Paper=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);
        Paper->SetupAttachment(Camera->GetCameraComponent());Paper->SetCollisionEnabled(ECollisionEnabled::NoCollision);Paper->SetCastShadow(false);Paper->SetEnableRaytracing(false);
        auto* M=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_IdeaPaper")),this);
        M->SetTextureParameterValue(TEXT("Idea"),Photo);
        M->SetVectorParameterValue(TEXT("PhotoCrop"),FLinearColor(CropOffset.X,CropOffset.Y,CropSize.X,CropSize.Y));
        M->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.18f,1.16f,1.12f));
        M->SetScalarParameterValue(TEXT("Seed"),I*.137f);
        Paper->SetMaterial(0,M);Paper->RegisterComponent();Paper->SetVisibility(false);EndingPapers.Add(Paper);
    }
    WetLens=0;PapersLanded=0;PapersArrived=0;bFinalPaperVisible=false;
}

void AEndingEmergence::UpdateStarlightAndPaper()
{
    using namespace UE::Geometry;
    const float T=Shot==9?ShotTime:Shot==10?Duration(9)+ShotTime:Duration(9)+Duration(10);
    WetLens=0;
    if(WetLensMaterial){WetLensMaterial->SetScalarParameterValue(TEXT("Wet"),WetLens);WetLensMaterial->SetScalarParameterValue(TEXT("SplashTime"),T);}
    for(int I=0;I<StarlightSparks.Num();++I)
    {
        const float Delay=.012f+(I%90)*.004f;
        const float AgeOfDrop=FMath::Fmod(FMath::Max(0.f,T-Delay),1.05f);
        const FVector P=SparkOrigins[I]+SparkVelocities[I]*AgeOfDrop+FVector(0,0,-54*AgeOfDrop*AgeOfDrop);
        StarlightSparks[I]->SetVisibility(T>=Delay&&T<5.4f&&P.X>10);
        StarlightSparks[I]->SetRelativeLocation(P);StarlightSparks[I]->SetRelativeRotation((SparkVelocities[I]+FVector(0,0,-108*AgeOfDrop)).Rotation());
        if(auto* M=Cast<UMaterialInstanceDynamic>(StarlightSparks[I]->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Flow"),(1-Ending::Ease((T-3.5f)/1.9f))*(1-Ending::Ease((AgeOfDrop-.75f)/.30f)));
    }
    for(int I=0;I<StarlightRibbons.Num();++I)
    {
        auto* Jet=StarlightRibbons[I].Get();Jet->SetVisibility(T>0&&T<4.6f);
        if(T<=0||T>=4.6f)continue;
        const float Strength=Ending::Ease(T/.08f)*(1-Ending::Ease((T-3.f)/1.6f));
        Cast<UMaterialInstanceDynamic>(Jet->GetMaterial(0))->SetScalarParameterValue(TEXT("Flow"),Strength);
        FDynamicMesh3 Mesh;Mesh.EnableAttributes();auto* UV=Mesh.Attributes()->PrimaryUV();
        const float Angle=I*2*PI/12+.19f*FMath::Sin(I*2.73f);
        for(int Y=0;Y<=24;++Y)for(int X=0;X<=6;++X)
        {
            const float U=X/6.f,V=Y/24.f;
            const float A=Angle+(U-.5f)*.72f+.13f*FMath::Sin(V*17-T*18+I);
            const float Radius=(1.3f+V*V*(15+I%4*3))*Strength;
            const float Ripple=1+.085f*FMath::Sin(V*25-T*24+U*8+I);
            Mesh.AppendVertex(FVector(43-V*27+FMath::Sin(U*12+V*18-T*8)*.3f,Radius*FMath::Cos(A)*Ripple,Radius*FMath::Sin(A)*Ripple+8*FMath::Sin(V*PI)-28*V*V));
            UV->AppendElement(FVector2f(U,V));
        }
        for(int Y=0;Y<24;++Y)for(int X=0;X<6;++X)
        {
            const int A=Y*7+X,B=A+1,C=A+7,D=C+1;
            const int One=Mesh.AppendTriangle(A,B,D),Two=Mesh.AppendTriangle(A,D,C);
            UV->SetTriangle(One,FIndex3i(A,B,D));UV->SetTriangle(Two,FIndex3i(A,D,C));
        }
        FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals(),false);Jet->SetMesh(MoveTemp(Mesh));
    }
    int Width=1280,Height=720;Player->GetViewportSize(Width,Height);
    int Arrived=0;
    for(int I=0;I<EndingPapers.Num();++I)
    {
        const bool Final=I==EndingPapers.Num()-1;
        const FVector4 Timing=PaperMotion[I];
        const float Local=T-Timing.X,U=FMath::Clamp(Local/Timing.Y,0.f,1.f),Travel=1-FMath::Pow(1-U,3.f);
        const float Slide=Final?0:FMath::Max(0.f,Local-Timing.Y)/Timing.Z;
        const float Phase=Timing.W;
        auto* Paper=EndingPapers[I].Get();
        if(U>=1)++Arrived;
        if(U<=0||Slide>=1.65f){Paper->SetVisibility(false);continue;}
        if(!Paper->IsVisible())Sound(TEXT("PaperFlight"),.20f,1+I*.025f);
        Paper->SetVisibility(true);
        const float Depth=Final?23.5f:25.f+(I%5)*.8f,HalfWidth=Depth*FMath::Tan(FMath::DegreesToRadians(31.f)),HalfHeight=HalfWidth*Height/FMath::Max(1,Width);
        const FVector End(Depth,PaperCenters[I].X*HalfWidth,PaperCenters[I].Y*HalfHeight);
        const FVector Begin(48,FMath::Sin(Phase)*2,-1);
        const FVector Arc(0,FMath::Sin(U*PI)*(I%2?5:-5),FMath::Sin(U*PI)*(5+I%4));
        // The transient sheets catch briefly, then peel, flutter and accelerate down.
        const FVector Slip(-.6f*Slide,FMath::Sin(Phase+Slide*3)*Slide*2.5f,-HalfHeight*(.22f*Slide+1.9f*Slide*Slide));
        Paper->SetRelativeLocation(FMath::Lerp(Begin,End,Travel)+Arc+Slip);
        Paper->SetRelativeRotation(FRotator((1-Travel)*(I%2?62:-55)+Slide*18*FMath::Sin(Phase+Slide*4),(1-Travel)*25+Slide*8*FMath::Cos(Phase+Slide*3),PaperRolls[I]+(1-Travel)*(I%2?150:-130)+Slide*(I%2?19:-16)));
        FDynamicMesh3 Mesh;Mesh.EnableAttributes();auto* UV=Mesh.Attributes()->PrimaryUV();
        for(int Y=0;Y<=4;++Y)for(int X=0;X<=6;++X)
        {
            const float A=X/6.f,B=Y/4.f;
            const float Curl=((1-Travel)*2.6f+FMath::Min(Slide,1.f)*1.4f)*FMath::Sin(A*PI)*FMath::Cos(B*PI+T*8+Phase)+.08f*FMath::Sin(A*PI*2+B*3+Phase);
            const float Shape=Final?.008f:.035f;
            const float PX=(A*2-1)*(1+Shape*FMath::Sin(B*9+Phase))+.025f*(B-.5f);
            const float PY=(1-B*2)*(1+Shape*FMath::Sin(A*13+Phase));
            const float Wide=PaperSizes[I].X*HalfWidth,Tall=Wide/PaperSizes[I].Y;
            Mesh.AppendVertex(FVector(Curl,PX*Wide,PY*Tall));UV->AppendElement(FVector2f(A,B));
        }
        for(int Y=0;Y<4;++Y)for(int X=0;X<6;++X)
        {
            const int A=Y*7+X,B=A+1,C=A+7,D=C+1;
            const int One=Mesh.AppendTriangle(A,B,D),Two=Mesh.AppendTriangle(A,D,C);
            UV->SetTriangle(One,FIndex3i(A,B,D));UV->SetTriangle(Two,FIndex3i(A,D,C));
        }
        FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals(),false);Paper->SetMesh(MoveTemp(Mesh));
    }
    if(Arrived>PapersArrived)Sound(TEXT("PaperLand"),Arrived==EndingPapers.Num()?.48f:.14f,1+(Arrived%3)*.08f);
    PapersArrived=Arrived;bFinalPaperVisible=Arrived==EndingPapers.Num();PapersLanded=bFinalPaperVisible?1:0;
}

void AEndingEmergence::CaptureUniverse()
{
    // Freeze the actual view for the front faces of the solid mesh fragments.
    UniverseSnapshot=NewObject<UTextureRenderTarget2D>(this,NAME_None,RF_Transient);
    int32 Width=1280,Height=720;Player->GetViewportSize(Width,Height);
    UniverseSnapshot->RenderTargetFormat=RTF_RGBA16f;
    UniverseSnapshot->InitAutoFormat(FMath::Max(1,Width),FMath::Max(1,Height));
    UniverseSnapshot->TargetGamma=2.2f;UniverseSnapshot->UpdateResourceImmediate(true);
    auto* Capture=NewObject<USceneCaptureComponent2D>(this,NAME_None,RF_Transient);
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->TextureTarget=UniverseSnapshot;Capture->FOVAngle=Camera->GetCameraComponent()->FieldOfView;
    Capture->PostProcessSettings=Camera->GetCameraComponent()->PostProcessSettings;
    Capture->RegisterComponent();Capture->SetWorldTransform(Camera->GetActorTransform());
    for(auto Burst:EndingDebris)if(IsValid(Burst))Capture->HiddenActors.Add(Burst.Get());
    TArray<USceneComponent*> HandParts;EndingHand->GetChildrenComponents(true,HandParts);
    for(auto Part:HandParts)if(auto Primitive=Cast<UPrimitiveComponent>(Part))Capture->HiddenComponents.Add(Primitive);
    Capture->CaptureScene();Capture->DestroyComponent();
}

void AEndingEmergence::NeuralWave()
{
    NeuralWaveCenters.Add(NeuronPositions[CurrentNeuron]);NeuralWaveTimes.Add(Age);
    while(NeuralWaveTimes.Num()>0&&Age-NeuralWaveTimes[0]>3)
    {NeuralWaveTimes.RemoveAt(0);NeuralWaveCenters.RemoveAt(0);}
}

float AEndingEmergence::NeuralFlash(int32 Index) const
{
    return NeuronPositions.IsValidIndex(Index)?NeuralFlashAt(NeuronPositions[Index]):0;
}

float AEndingEmergence::NeuralFlashAt(FVector Position) const
{
    float Brightness=0;
    for(int I=0;I<NeuralWaveTimes.Num();++I)
    {
        const float Distance=FVector::Dist(Position,NeuralWaveCenters[I]);
        const bool Thought=GateIndex>=5;
        const float T=Age-NeuralWaveTimes[I]-Distance/(Thought?11000.f:22000.f);
        if(Distance<=(Thought?4200:7200)&&T>=0&&T<(Thought?.30f:.65f))
            Brightness=FMath::Max(Brightness,FMath::Min(1.f,T/.012f)*FMath::Exp(-T*(Thought?14.f:4.f))*(1-Distance/(Thought?6000.f:14000.f)));
    }
    return Brightness;
}

void AEndingEmergence::EndingImpact()
{
    ImpactAt=Age;
    FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Burst=GetWorld()->SpawnActor<AMiningDebrisBurst>(Camera->GetActorLocation()+Camera->GetActorForwardVector()*180,FRotator::ZeroRotator,Params);
    if(Burst)
    {
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_CosmicDebris"));
        const auto* Effects=Player->GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>();
        Burst->Initialize(EndingDebrisMeshes,Material,-Camera->GetActorForwardVector(),100,Effects?Effects->DebrisChunkCount:36,Effects?Effects->DebrisLifetime:2);
        EndingDebris.Add(Burst);++EndingDebrisBursts;
    }
}

void AEndingEmergence::ClearEndingEffects()
{
    for(auto Shard:UniverseShards)if(IsValid(Shard))Shard->DestroyComponent();UniverseShards.Empty();
    ShardOrigins.Empty();ShardVelocities.Empty();ShardSpins.Empty();
    if(UniverseSurround){UniverseSurround->DestroyComponent();UniverseSurround=nullptr;}
    for(auto Jet:StarlightRibbons)if(IsValid(Jet))Jet->DestroyComponent();StarlightRibbons.Empty();
    for(auto Burst:EndingDebris)if(IsValid(Burst))Burst->Destroy();EndingDebris.Empty();
    if(ToolLight)ToolLight->SetVisibility(false);
    for(auto Shell:NeuralWaveShells)if(IsValid(Shell))Shell->SetVisibility(false);
    for(auto Drop:StarlightSparks)if(IsValid(Drop))Drop->DestroyComponent();StarlightSparks.Empty();SparkOrigins.Empty();SparkVelocities.Empty();
    for(auto Paper:EndingPapers)if(IsValid(Paper))Paper->DestroyComponent();EndingPapers.Empty();PaperCenters.Empty();PaperSizes.Empty();PaperRolls.Empty();PaperMotion.Empty();
    WetLens=0;PapersLanded=0;PapersArrived=0;bFinalPaperVisible=false;
    if(WetLensMaterial)WetLensMaterial->SetScalarParameterValue(TEXT("Wet"),0);
    NeuralWaveCenters.Empty();NeuralWaveTimes.Empty();Exhaustion=0;ImpactAt=-100;RetreatSpeed=0;EndingDebrisBursts=0;
    if(ExhaustionMaterial){ExhaustionMaterial->SetScalarParameterValue(TEXT("Gray"),0);ExhaustionMaterial->SetScalarParameterValue(TEXT("RippleAge"),100);}
}

void AEndingEmergence::BuildUniverseShards()
{
    using namespace UE::Geometry;
    auto* ImageMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_UniverseShard")),this);
    ImageMaterial->SetTextureParameterValue(TEXT("UniverseImage"),UniverseSnapshot);
    auto* EdgeMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_CosmicDebris"));
    int32 Width=1280,Height=720;Player->GetViewportSize(Width,Height);
    const float Depth=35,HalfWidth=Depth*FMath::Tan(FMath::DegreesToRadians(31.f)),HalfHeight=HalfWidth*Height/FMath::Max(1,Width);
    // Keep the live universe outside the aperture. Re-projecting a full-screen HDR
    // snapshot changes exposure and creates a flash; only detached faces use it.
    FRandomStream Random(917);constexpr int Count=24;
    TArray<FVector> Rim;const float Radius=HalfHeight*.56f;
    for(int I=0;I<Count;++I)
    {
        const float A=2*PI*I/Count;
        const FVector Direction(0,FMath::Cos(A),FMath::Sin(A));
        Rim.Add(FVector(Depth,0,0)+Direction*Radius*Random.FRandRange(.84f,1.12f));
    }
    auto ViewUV=[&](FVector P){return FVector2f(.5f+P.Y/(2*HalfWidth),.5f-P.Z/(2*HalfHeight));};
    FDynamicMesh3 Surround;Surround.EnableAttributes();Surround.Attributes()->EnableMaterialID();
    for(int I=0;I<Count;++I){Surround.AppendVertex(Rim[I]);Surround.AppendVertex(Rim[I]+FVector(2,0,0));}
    const int BackCenter=Surround.AppendVertex(FVector(Depth+2,0,0));
    auto AddFace=[&](int A,int B,int C,int Material)
    {
        const int Tri=Surround.AppendTriangle(A,B,C);Surround.Attributes()->GetMaterialID()->SetValue(Tri,Material);
        const int U=Surround.Attributes()->PrimaryUV()->AppendElement(ViewUV(Surround.GetVertex(A)));
        const int V=Surround.Attributes()->PrimaryUV()->AppendElement(ViewUV(Surround.GetVertex(B)));
        const int W=Surround.Attributes()->PrimaryUV()->AppendElement(ViewUV(Surround.GetVertex(C)));
        Surround.Attributes()->PrimaryUV()->SetTriangle(Tri,FIndex3i(U,V,W));
    };
    for(int I=0;I<Count;++I)
    {
        const int A=I*2,B=((I+1)%Count)*2;
        AddFace(A,B,B+1,1);AddFace(A,B+1,A+1,1);AddFace(BackCenter,A+1,B+1,0);
    }
    FMeshNormals::InitializeOverlayToPerTriangleNormals(Surround.Attributes()->PrimaryNormals());
    UniverseSurround=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);
    UniverseSurround->SetupAttachment(Camera->GetCameraComponent());UniverseSurround->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UniverseSurround->SetCastShadow(false);UniverseSurround->SetEnableRaytracing(false);
    UniverseSurround->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ApertureVoid")));
    UniverseSurround->SetMaterial(1,EdgeMaterial);UniverseSurround->SetMesh(MoveTemp(Surround));UniverseSurround->RegisterComponent();
    for(int Piece=0;Piece<Count;++Piece)
    {
        const FVector Front[]={FVector(Depth,0,0),Rim[Piece],Rim[(Piece+1)%Count]};
        const FVector2f UVs[]={ViewUV(Front[0]),ViewUV(Front[1]),ViewUV(Front[2])};
        const FVector Center=(Front[0]+Front[1]+Front[2])/3;
        const float Thickness=Random.FRandRange(1.2f,3.f);
        FDynamicMesh3 Mesh;Mesh.EnableAttributes();Mesh.Attributes()->EnableMaterialID();
        for(int I=0;I<6;++I)Mesh.AppendVertex(Front[I%3]-Center+FVector(I<3?0:Thickness,0,0));
        auto Triangle=[&](int A,int B,int C,int Material)
        {
            const int Tri=Mesh.AppendTriangle(A,B,C);Mesh.Attributes()->GetMaterialID()->SetValue(Tri,Material);
            const int U=Mesh.Attributes()->PrimaryUV()->AppendElement(UVs[A%3]),V=Mesh.Attributes()->PrimaryUV()->AppendElement(UVs[B%3]),W=Mesh.Attributes()->PrimaryUV()->AppendElement(UVs[C%3]);
            Mesh.Attributes()->PrimaryUV()->SetTriangle(Tri,FIndex3i(U,V,W));
        };
        Triangle(0,1,2,0);Triangle(5,4,3,1);
        for(int I=0;I<3;++I){const int J=(I+1)%3;Triangle(I,I+3,J,1);Triangle(J,I+3,J+3,1);}
        FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
        auto* Shard=NewObject<UDynamicMeshComponent>(this,NAME_None,RF_Transient);
        Shard->SetupAttachment(Camera->GetCameraComponent());Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shard->SetCastShadow(false);Shard->SetEnableRaytracing(false);Shard->SetMaterial(0,ImageMaterial);Shard->SetMaterial(1,EdgeMaterial);
        Shard->SetMesh(MoveTemp(Mesh));Shard->RegisterComponent();Shard->SetRelativeLocation(Center);
        UniverseShards.Add(Shard);ShardOrigins.Add(Center);
        ShardVelocities.Add(FVector(Random.FRandRange(-24,32),Center.Y*3.7f+Random.FRandRange(-7,7),Center.Z*2+Random.FRandRange(8,22)));
        ShardSpins.Add(FVector(Random.FRandRange(-170,170),Random.FRandRange(-210,210),Random.FRandRange(-120,120)));
    }
}

void AEndingEmergence::UpdateUniverseShards()
{
    for(int I=0;I<UniverseShards.Num();++I)
    {
        const float T=FMath::Max(0.f,ShotTime-(I%5)*.025f);
        UniverseShards[I]->SetRelativeLocation(ShardOrigins[I]+ShardVelocities[I]*T+FVector(0,0,-25*T*T));
        UniverseShards[I]->SetRelativeRotation(FRotator(ShardSpins[I].X*T,ShardSpins[I].Y*T,ShardSpins[I].Z*T));
        // All three dimensions persist while tumbling; only fade out after leaving view.
        UniverseShards[I]->SetVisibility(T<2.35f);
    }
}

void AEndingEmergence::UpdateEndingHand()
{
    const bool Cosmic=Shot<=8;
    const float Reveal=Shot==7?Ending::Ease((ShotTime-7)/3):1;
    const float Swing=Shot==8?FMath::Clamp((Age-LastStrike)/.75f,0.f,1.f):1;
    const float Windup=Swing<.32f?Ending::Ease(Swing/.32f):1-Ending::Ease((Swing-.32f)/.68f);
    const float Snap=0;
    EndingHand->SetRelativeLocation(Cosmic?FVector(82,12,-8-75*(1-Reveal))+FVector(-7,-7,-9)*Windup:
        FVector(48,16,-17)+FVector(-3,0,-2)*Snap);
    EndingHand->SetRelativeRotation(Cosmic?FRotator(0,-4,-3+Windup*42):FRotator(0,-18,-12+Snap*9));
    // Finger endpoints are in palm space. Middle finger presses against the thumb,
    // then slips sharply down into the palm while the thumb springs outward.
    for(int Finger=0;Finger<5;++Finger)
    {
        FVector Points[4];
        if(Finger==0)
        {
            Points[0]=FVector(0,-6,0);Points[1]=FVector(-2,-8,4);
            Points[2]=FVector(-4,-5+Snap* -3,8);Points[3]=FVector(-5,-1-Snap*8,9+Snap*2);
            if(Cosmic){Points[2]=FVector(-4,-3,5);Points[3]=FVector(-3,1,5);}
        }
        else
        {
            const float Y=-5+(Finger-1)*3.3f,Length=Finger==4?6:Finger==1?9:10;
            Points[0]=FVector(0,Y,7);Points[1]=FVector(-.8,Y,7+Length*.46f);
            const float Fold=Cosmic?1:Finger==2?Snap:Finger>=3?.85f:0;
            Points[2]=FVector(-2.5-Fold*2,Y,7+Length*.82f-Fold*6);
            Points[3]=FVector(-3-Fold*3,Y,7+Length-Fold*12);
            if(!Cosmic&&Finger==2){Points[2]=FMath::Lerp(FVector(-4,-.8,13),Points[2],Snap);Points[3]=FMath::Lerp(FVector(-5,-1,9),Points[3],Snap);}
        }
        for(int J=0;J<3;++J)
        {
            auto* C=Fingers[Finger*3+J].Get();const FVector Delta=Points[J+1]-Points[J];
            C->SetRelativeLocation((Points[J]+Points[J+1])*.5f);
            C->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
            C->SetRelativeScale3D(FVector(2.6f-J*.35f,2.6f-J*.35f,Delta.Size()+1.4f)/100);
        }
    }
}

void AEndingEmergence::UpdateNerves()
{
    for(int I=0;I<Nerves.Num();++I)
    {
        const bool On=I<(GateIndex<2?0:(GateIndex-1)*Nerves.Num()/4);
        if(auto* M=Cast<UMaterialInstanceDynamic>(Nerves[I]->GetMaterial(0)))M->SetVectorParameterValue(TEXT("Tint"),On?Ending::Gold*.8f:Ending::Teal*.07f);
    }
}

void AEndingEmergence::EnterBlood()
{
    GateIndex=1;BloodAt=Age;BeatAt=Age+3;BloodSpeed=480;BloodY=0;
    UpdatePainterly(); // Clear both views before the camera handoff, not a tick later.
    const FVector Eye=Player->PlayerCameraManager->GetCameraLocation();
    const FVector LocalEye=GetActorTransform().InverseTransformPosition(Eye);
    VesselDistance=FMath::Max(150.f,float(LocalEye.X)-1600);BloodStartZ=LocalEye.Z-VesselPoint(VesselDistance).Z;
    VesselLook=Player->GetControlRotation();VesselLook.Roll=0;
    VesselLookOffset=FRotator::ZeroRotator;VesselLookInputAt=Age-2;
    Player->ResetIgnoreLookInput();
    // A visible copy leaves the hand before the hidden pawn becomes a pulse.
    TArray<UStaticMeshComponent*> Parts;Player->GetPawn()->GetComponents(Parts);
    for(auto* Part:Parts)if(Part->GetName()==TEXT("PickaxeModel")&&Part->GetStaticMesh())
    {
        DroppedPickaxe=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transient);
        DroppedPickaxe->SetupAttachment(RootComponent);DroppedPickaxe->SetStaticMesh(Part->GetStaticMesh());
        DroppedPickaxe->SetCollisionEnabled(ECollisionEnabled::NoCollision);DroppedPickaxe->SetCastShadow(false);
        for(int I=0;I<Part->GetNumMaterials();++I)DroppedPickaxe->SetMaterial(I,Part->GetMaterial(I));
        DroppedPickaxe->RegisterComponent();DroppedPickaxe->SetWorldTransform(Part->GetComponentTransform());
        // First-person meshes use a separate render transform. Place the dropped
        // world mesh ahead of the eye so it visibly leaves the frame, not behind it.
        const FRotationMatrix ViewAxes(Player->GetControlRotation());
        PickaxeStart=LocalEye+GetActorTransform().InverseTransformVectorNoScale(ViewAxes.GetUnitAxis(EAxis::X)*130+ViewAxes.GetUnitAxis(EAxis::Y)*55-ViewAxes.GetUnitAxis(EAxis::Z)*40);
        DroppedPickaxe->SetRelativeLocation(PickaxeStart);break;
    }
    Mining->SetInputSuspended(true);Mining->SetInputContextEnabled(false);
    Player->SetIgnoreMoveInput(true);Player->GetPawn()->SetActorHiddenInGame(true);
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();}
    Camera->SetActorLocation(Eye);Camera->SetActorRotation(Player->GetControlRotation());Player->SetViewTarget(Camera);
    SetAmbience(TEXT("Blood_Loop"),.25f);Sound(TEXT("Connect"),.45f);
}

FVector AEndingEmergence::NeuronWorldPosition(int32 I) const
{
    return NeuronPositions.IsValidIndex(I)?GetActorTransform().TransformPosition(NeuronPositions[I]):GetActorLocation();
}

bool AEndingEmergence::CanPossess(int32 I) const
{
    if(Shot>=0||GateIndex!=2||TravellingToNeuron!=INDEX_NONE||!NeuronPositions.IsValidIndex(I)||!NeuronPositions.IsValidIndex(CurrentNeuron)||I==CurrentNeuron)return false;
    return FMath::Abs(Ending::Layer(I)-Ending::Layer(CurrentNeuron))==1&&FVector::Dist(NeuronPositions[I],NeuronPositions[CurrentNeuron])<2050;
}

void AEndingEmergence::EnterBrain()
{
    for(auto Gate:Gates)if(IsValid(Gate))Gate->SetActorHiddenInGame(true);
    // Leave the open vessel in the same shot; slow down and settle into host 0.
    bEnteringBrain=true;BrainEntryAt=Age;TransitStart=Camera->GetActorLocation();
    UpdatePainterly();
    TransitEnd=NeuronWorldPosition(0)+GetActorUpVector()*380;TransitLook=Camera->GetActorRotation();
    Player->SetIgnoreLookInput(true);
    SetAmbience(TEXT("Cell_Loop"),.22f);
    Sound(TEXT("PulseFlight"),.8f,.95f);
    if(DroppedPickaxe)DroppedPickaxe->SetVisibility(false);
}

void AEndingEmergence::PossessNeuron(int32 I)
{
    if(!CanPossess(I))return;
    BufferedJumpAt=-100;
    TravellingToNeuron=I;TargetNeuron=INDEX_NONE;PulseAt=Age;
    TransitStart=Camera->GetActorLocation();TransitEnd=NeuronWorldPosition(I)+GetActorUpVector()*(I==NeuronPositions.Num()-1?600:380);
    TransitLook=Camera->GetActorRotation();
    TransitDuration=FMath::Clamp(float(FVector::Dist(TransitStart,TransitEnd))/2100.f+.35f,.95f,1.35f);
    Player->ResetIgnoreLookInput();Sound(TEXT("PulseFlight"),.6f,1.15f/TransitDuration);
}

FVector AEndingEmergence::VesselPoint(float Distance) const
{
    if(VesselPath.Num()<2)return FVector(1600,0,850);
    Distance=FMath::Clamp(Distance,0.f,VesselDistances.Last());
    int Low=0,High=VesselDistances.Num()-1;
    while(High-Low>1){const int Mid=(Low+High)/2;if(VesselDistances[Mid]<Distance)Low=Mid;else High=Mid;}
    const float T=(Distance-VesselDistances[Low])/FMath::Max(1.f,VesselDistances[High]-VesselDistances[Low]);
    return FMath::Lerp(VesselPath[Low],VesselPath[High],T);
}

FRotator AEndingEmergence::VesselRotation(float Distance) const
{
    return (VesselPoint(Distance+35)-VesselPoint(Distance-35)).Rotation();
}

void AEndingEmergence::TransitEffects(float Strength)
{
    Strength*=bComfortMode?.12f:1.f;
    Camera->GetCameraComponent()->PostProcessBlendWeight=1;
    if(TransitMaterial)Camera->GetCameraComponent()->AddOrUpdateBlendable(TransitMaterial,Strength>0?1:0);
    if(TransitMaterial)TransitMaterial->SetScalarParameterValue(TEXT("Strength"),Strength);
    auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
    PP.MotionBlurAmount=.25f*Strength;
    PP.bOverride_SceneFringeIntensity=true;PP.SceneFringeIntensity=1.1f*Strength;
    PP.VignetteIntensity=.3f+.18f*Strength;
}

void AEndingEmergence::BeginThought()
{
    for(auto Gate:Gates)if(IsValid(Gate))Gate->SetActorHiddenInGame(true);
    GateIndex=5;BeatAt=Age+3.8f;TargetNeuron=INDEX_NONE;UpdateNerves();
    TArray<USceneComponent*> Parts;GetComponents(Parts);
    for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("EndingBloodVisual"))||Part->GetName().StartsWith(TEXT("EllipsoidalCellInterior"))||Part->GetName().StartsWith(TEXT("CellDress")))Part->SetVisibility(false,true);
    TakeoffStart=GetActorTransform().InverseTransformPosition(Camera->GetActorLocation());
    TakeoffLook=TakeoffStart+GetActorTransform().InverseTransformVectorNoScale(Player->GetControlRotation().Vector())*850;
    Player->SetIgnoreLookInput(true);Sound(TEXT("Connect"),.65f,.85f);
}

int32 AEndingEmergence::FindNeuronTarget(int32 Host,const FVector& Origin,const FVector& Direction) const
{
    if(!NeuronPositions.IsValidIndex(Host))return INDEX_NONE;
    int32 Target=INDEX_NONE;float Best=.990f;
    for(int32 I=0;I<NeuronPositions.Num();++I)
    {
        if(I==Host||FMath::Abs(Ending::Layer(I)-Ending::Layer(Host))!=1||FVector::Dist(NeuronPositions[I],NeuronPositions[Host])>=2050)continue;
        const float Alignment=FVector::DotProduct(Direction,(NeuronWorldPosition(I)-Origin).GetSafeNormal());
        if(Alignment>Best){Best=Alignment;Target=I;}
    }
    return Target;
}

void AEndingEmergence::UpdatePulse(float Dt)
{
    if(GateIndex==1&&!bEnteringBrain)
    {
        const float T=Age-BloodAt;
        BloodSpeed=FMath::FInterpTo(BloodSpeed,FMath::Lerp(480.f,950.f,FMath::Max(0.f,float(UMiningGamepadSubsystem::ReadMove(Player).Y))),Dt,3);
        VesselDistance=FMath::Min(VesselDistance+BloodSpeed*Dt,VesselDistances.Last());
        BloodY=FMath::Clamp(BloodY+Dt*150*UMiningGamepadSubsystem::ReadMove(Player).X,-240.f,240.f);
        const float Z=BloodStartZ*(1-Ending::Ease(T/1.8f));
        const FRotator Frame=VesselRotation(VesselDistance);
        Camera->SetActorLocation(GetActorTransform().TransformPosition(VesselPoint(VesselDistance)+Frame.RotateVector(FVector(0,BloodY,Z))));
        // Observe mouse rotation supplied by the normal player controller before
        // writing the automatic heading. Keep the player's offset independent
        // from bends, and only ease it home after a deliberate idle grace period.
        const FRotator Input=(Player->GetControlRotation()-Camera->GetActorRotation()).GetNormalized();
        if(FMath::Abs(Input.Yaw)+FMath::Abs(Input.Pitch)>.001f)
        {
            VesselLookOffset.Yaw=FRotator::NormalizeAxis(VesselLookOffset.Yaw+Input.Yaw);
            VesselLookOffset.Pitch=FMath::Clamp(VesselLookOffset.Pitch+Input.Pitch,-80.f,80.f);
            VesselLookInputAt=Age;
        }
        else if(Age-VesselLookInputAt>1.6f)VesselLookOffset=FMath::RInterpTo(VesselLookOffset,FRotator::ZeroRotator,Dt,2.f);
        // Aim from the actual laterally shifted eye to the centre of the tunnel,
        // farther ahead and with less lag than the old short tangent look.
        float Ahead=FMath::Min(VesselDistance+1000.f,VesselDistances.Last());
        const FVector LocalEye=GetActorTransform().InverseTransformPosition(Camera->GetActorLocation());
        // Frame the farthest visible centre of the passage. On tight bends a
        // fixed distant target would look through a wall; retain a safe chord.
        for(float Reach=1300;Reach<=3400;Reach+=300)
        {
            const float Candidate=FMath::Min(VesselDistance+Reach,VesselDistances.Last());
            const FVector End=VesselPoint(Candidate);bool Clear=true;
            for(int S=1;S<8;++S)
            {
                const float U=S/8.f;
                if(FVector::Dist(FMath::Lerp(LocalEye,End,U),VesselPoint(FMath::Lerp(VesselDistance,Candidate,U)))>600){Clear=false;break;}
            }
            if(!Clear)break;Ahead=Candidate;
        }
        FVector Target=GetActorTransform().TransformPosition(VesselPoint(Ahead));
        if(Ahead-VesselDistance<100)Target+=GetActorTransform().TransformVectorNoScale(Frame.Vector()*500);
        const FRotator Forward=(Target-Camera->GetActorLocation()).Rotation();
        VesselLook=FMath::RInterpTo(VesselLook,Forward,Dt,6.f);VesselLook.Roll=0;
        FRotator Look=(VesselLook+VesselLookOffset).GetNormalized();Look.Roll=0;Look.Pitch=FMath::Clamp(Look.Pitch,-85.f,85.f);
        Player->SetControlRotation(Look);Camera->SetActorRotation(Look);
        Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(90.f,95.f,(BloodSpeed-480)/470));
        if(DroppedPickaxe)
        {
            DroppedPickaxe->SetRelativeLocation(PickaxeStart+FVector(600*T,50*T,120*T-260*T*T));
            DroppedPickaxe->AddLocalRotation(FRotator(155*Dt,30*Dt,65*Dt));
            DroppedPickaxe->SetVisibility(T<2.1f);
        }
        if(VesselDistance>=VesselDistances.Last())EnterBrain();
    }
    if(bEnteringBrain)
    {
        const float T=FMath::Clamp((Age-BrainEntryAt)/1.45f,0.f,1.f);
        // Hermite tangent starts at the final blood speed and ends at rest.
        const FVector StartVelocity=GetActorTransform().TransformVectorNoScale(VesselRotation(VesselDistances.Last()).Vector())*BloodSpeed*1.45f;
        Camera->SetActorLocation(FMath::CubicInterp(TransitStart,StartVelocity,TransitEnd,FVector::ZeroVector,T));
        const FRotator ArriveLook=(NeuronWorldPosition(3)-TransitEnd).Rotation();
        FRotator EntryLook=FQuat::Slerp(TransitLook.Quaternion(),ArriveLook.Quaternion(),Ending::Ease(T)).Rotator();
        const float EntryEnvelope=FMath::Square(FMath::Sin(PI*T));
        EntryLook.Roll=bComfortMode?0:3.8f*FMath::Sin(T*2*PI)*EntryEnvelope;Camera->SetActorRotation(EntryLook);
        TransitEffects(EntryEnvelope*1.35f);
        Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(FMath::Lerp(90.f,100.f,(BloodSpeed-480)/470),94.f,Ending::Ease(T))+(bComfortMode?4:44)*EntryEnvelope);
        if(T>=1)
        {
            bEnteringBrain=false;TransitEffects(0);GateIndex=2;CurrentNeuron=0;VisitedNeurons.Add(0);BeatAt=Age+4;NeuralWave();
            Player->SetControlRotation(ArriveLook);Player->ResetIgnoreLookInput();Sound(TEXT("Connect"),.3f,1.3f);
        }
    }
    if(GateIndex==2&&TravellingToNeuron!=INDEX_NONE)
    {
        if(UMiningGamepadSubsystem::ReadJumpPressed(Player))BufferedJumpAt=Age;
        const float T=FMath::Clamp((Age-PulseAt)/TransitDuration,0.f,1.f),Travel=Ending::Ease(T),Envelope=FMath::Square(FMath::Sin(PI*T));
        const FVector Position=FMath::Lerp(TransitStart,TransitEnd,Travel)+GetActorUpVector()*(110*FMath::Sin(PI*Travel));
        Camera->SetActorLocation(Position);
        // Position follows the pulse; aim remains entirely under mouse control.
        FRotator Look=Player->GetControlRotation();
        Look.Roll=bComfortMode?0:2.5f*FMath::Sin(T*2*PI)*Envelope;Camera->SetActorRotation(Look);
        Camera->GetCameraComponent()->SetFieldOfView(94+(bComfortMode?4:46)*Envelope);TransitEffects(Envelope*1.35f);
        // A fresh press near arrival can cancel only the settling tail. The next
        // pulse starts at this exact viewpoint, preserving position and free look.
        const int32 ChainedTarget=(T>=.78f&&Age-BufferedJumpAt<=.3f&&TravellingToNeuron!=NeuronPositions.Num()-1)
            ?FindNeuronTarget(TravellingToNeuron,Position,Player->GetControlRotation().Vector()):INDEX_NONE;
        if(T>=1||ChainedTarget!=INDEX_NONE)
        {
            CurrentNeuron=TravellingToNeuron;TravellingToNeuron=INDEX_NONE;++PossessionCount;VisitedNeurons.Add(CurrentNeuron);
            Look.Roll=0;Camera->SetActorRotation(Look);TransitEffects(0);
            NeuralWave();Sound(TEXT("Connect"),.28f,1+CurrentNeuron*.018f);
            if(CurrentNeuron==NeuronPositions.Num()-1)BeginThought();
            else if(ChainedTarget!=INDEX_NONE)PossessNeuron(ChainedTarget);
        }
    }
    if(GateIndex==2&&TravellingToNeuron==INDEX_NONE)
    {
        Camera->SetActorLocation(NeuronWorldPosition(CurrentNeuron)+GetActorUpVector()*380);
        Camera->SetActorRotation(Player->GetControlRotation());
        Camera->GetCameraComponent()->SetFieldOfView(94);TransitEffects(0);
        TargetNeuron=FindNeuronTarget(CurrentNeuron,Camera->GetActorLocation(),Camera->GetActorForwardVector());
        if((UMiningGamepadSubsystem::ReadJumpPressed(Player)||Age-BufferedJumpAt<=.3f)&&TargetNeuron!=INDEX_NONE)PossessNeuron(TargetNeuron);
    }
    if(PulseTrail)PulseTrail->SetVisibility(TravellingToNeuron!=INDEX_NONE);
    const float EntryReveal=GateIndex>=5?1.f:(GateIndex==2||bEnteringBrain)?Ending::Ease((Age-BrainEntryAt-.9f)/1.1f):0.f;
    auto NeuronTint=[&](int I)
    {
        const bool Selected=GateIndex==2&&(I==TargetNeuron||I==TravellingToNeuron);
        const bool Goal=I==NeuronPositions.Num()-1;
        if(Selected)return Ending::Gold*1.5f;
        const FLinearColor Base=Goal?Ending::GoalBlue*(2.f+.2f*FMath::Sin(Age*2)):GateIndex>=5?FLinearColor(.20f,.65f,1.1f)*1.4f:FLinearColor(.15f,.65f,1.1f)*(VisitedNeurons.Contains(I)?1.2f:1.f);
        return FMath::Lerp(Base,Goal?Ending::GoalBlue*3.f:FLinearColor(.65f,.98f,1.f)*5,NeuralFlash(I));
    };
    for(int I=0;I<NeuronCores.Num();++I)
    {
        const float CameraDistance=FVector::Dist(Camera->GetActorLocation(),NeuronCores[I]->GetComponentLocation());
        const bool Visible=(I!=CurrentNeuron||GateIndex==5||TravellingToNeuron!=INDEX_NONE)&&CameraDistance>(I==NeuronPositions.Num()-1?700.f:280.f);
        if(NeuronCores[I]->IsVisible()!=Visible)NeuronCores[I]->SetVisibility(Visible,true);
        if(auto* M=Cast<UMaterialInstanceDynamic>(NeuronCores[I]->GetMaterial(0)))
            {
                const FLinearColor Tint=NeuronTint(I);
                M->SetVectorParameterValue(TEXT("Tint"),Tint);M->SetScalarParameterValue(TEXT("Reveal"),EntryReveal);
                for(auto Child:NeuronCores[I]->GetAttachChildren())if(auto* Mesh=Cast<UStaticMeshComponent>(Child))
                    if(auto* CoreMaterial=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
                    {CoreMaterial->SetVectorParameterValue(TEXT("Tint"),Tint*.65f);CoreMaterial->SetScalarParameterValue(TEXT("Reveal"),EntryReveal);}
            }
    }
    for(auto Core:AmbientNeuronCores)if(auto* M=Cast<UMaterialInstanceDynamic>(Core->GetMaterial(0)))
    {M->SetVectorParameterValue(TEXT("Tint"),FMath::Lerp(Ending::Neural*.45f,FLinearColor(.65f,.98f,1.f)*3,NeuralFlashAt(GetActorTransform().InverseTransformPosition(Core->GetComponentLocation()))));M->SetScalarParameterValue(TEXT("Reveal"),EntryReveal);}
    for(auto Nerve:Nerves)if(auto* M=Cast<UMaterialInstanceDynamic>(Nerve->GetMaterial(0)))
    {
        const bool Arbor=Nerve->GetName().StartsWith(TEXT("NeuronDendriticArbor"));
        const FVector LocalPosition=GetActorTransform().InverseTransformPosition(Nerve->GetComponentLocation());
        const int32 Host=Arbor?NeuronPositions.IndexOfByPredicate([&](const FVector& P){return P.Equals(LocalPosition,.1f);}):INDEX_NONE;
        const FLinearColor Base=GateIndex>=5?FLinearColor(.20f,.65f,1.1f)*1.4f:Nerve->GetName().StartsWith(TEXT("NeuralAxonLink"))?Ending::Neural*.45f:Ending::Neural*.12f;
        M->SetVectorParameterValue(TEXT("Tint"),Host!=INDEX_NONE?NeuronTint(Host):FMath::Lerp(Base,FLinearColor(.65f,.98f,1.f)*5,NeuralFlashAt(LocalPosition)));
        M->SetScalarParameterValue(TEXT("Reveal"),EntryReveal);M->SetScalarParameterValue(TEXT("Activity"),GateIndex>=5?1.f:0.f);
    }
    for(auto Part:BrainDecorationBatches)
        if(auto* M=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0))){M->SetScalarParameterValue(TEXT("Reveal"),EntryReveal);M->SetScalarParameterValue(TEXT("Activity"),GateIndex>=5?1.f:0.f);}

}

void AEndingEmergence::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bReady){bReady=Initialize();if(!bReady)return;RestartDemo();}
    if(bComplete){Age+=Dt;ShotTime+=Dt;UpdateCredits();return;}
    // First-use resource uploads in cooked builds must not skip an entire short scale transition.
    if((Shot>=0 && Shot<=8) || (Shot<0 && GateIndex>=5))Dt=FMath::Min(Dt,1.f/15.f);
    Age+=Dt;UpdateCellMotion();UpdatePainterly();
    // Warm resources in the opening cell. Cross-scale jumps use only the live
    // 3D camera; an arrival never displays a static scene-capture image.
    if(!bScaleWarmupComplete&&Age>1.5f)WarmStreetRendering();
    if(Player->WasInputKeyJustPressed(EKeys::C))bComfortMode=!bComfortMode;
    if(Shot<0&&Player->WasInputKeyJustPressed(EKeys::P)){PreviewFilm();return;}
    if(Shot<0)
    {
        const FVector P=GetActorTransform().InverseTransformPosition(Player->GetPawn()->GetActorLocation());
        if(GateIndex==0&&Gates.IsValidIndex(0))
        {
            auto* Gate=Gates[GateIndex].Get();
            if(Gate->SuccessfulCuts!=LastCuts){LastCuts=Gate->SuccessfulCuts;Sound(TEXT("Hit"),.36f,1+GateIndex*.08f);}
            if(Gate->SuccessfulCuts>0 && P.X>Ending::GateX[GateIndex]+50)
            {
                // Hand over to the floating camera once the player has entered
                // a through-hole. Waiting for the entire walking capsule to
                // clear the far rim can strand it at the narrower vessel neck.
                FHitResult RemainingMembrane;
                const FVector Exit=GetActorTransform().TransformPosition(FVector(Ending::GateX[GateIndex]+Gate->RockSize.X*.5f+80,P.Y,P.Z));
                if(!Gate->ActorLineTraceSingle(RemainingMembrane,Player->GetPawn()->GetActorLocation(),Exit,ECC_Visibility,FCollisionQueryParams(SCENE_QUERY_STAT(EndingMembraneExit),true)))
                {LastCuts=0;EnterBlood();}
            }
        }
        if(auto* C=Player->GetPawn()->FindComponentByClass<UCameraComponent>())
        {
            C->SetFieldOfView(FMath::FInterpTo(C->FieldOfView,bComfortMode?90.f:FMath::Lerp(90.f,96.f,BrainLens()),Dt,2));
            if(FisheyeMaterial)C->AddOrUpdateBlendable(FisheyeMaterial,bComfortMode?0:BrainLens()*.22f);
        }
        UpdatePulse(Dt);
        if(GateIndex>=5)
        {
            const float Lift=FMath::Clamp(3.8f-(BeatAt-Age),0.f,3.8f);
            const float U=Lift/3.8f;
            Pose(FMath::Lerp(TakeoffStart,FVector(24700,-5600,14500),U*U),FMath::Lerp(TakeoffLook,FVector(23100,0,1800),Ending::Ease(Lift/2)),FMath::Lerp(94.f,110.f,U*U));
            if(ScaleZoomMaterial)ScaleZoomMaterial->SetScalarParameterValue(TEXT("Strength"),Ending::Ease((Lift-1.2f)/1.5f)*(bComfortMode?.35f:1.f)*2.5f);
            if(Lift>3.6f&&!bScaleBridgePrepared){CaptureScaleBridge();bScaleBridgePrepared=true;}
            const int Burst=FMath::FloorToInt(Lift/.42f);
            if(Burst>FMath::FloorToInt((Lift-Dt)/.42f))
            {
                for(int K=0;K<3;++K){NeuralWaveCenters.Add(NeuronPositions[(Burst*17+K*13)%NeuronPositions.Num()]);NeuralWaveTimes.Add(Age-K*.035f);}
                while(NeuralWaveTimes.Num()>0&&Age-NeuralWaveTimes[0]>3){NeuralWaveTimes.RemoveAt(0);NeuralWaveCenters.RemoveAt(0);}
            }
        }
        if(GateIndex>=5&&Age>=BeatAt)PreviewFilm();
    }
    else
    {
        const FVector PreviousFilmPosition=Camera->GetActorLocation();
        const bool Pulling=Shot>=1&&Shot<=6;
        // W is intentionally absent: the sequence always moves outward. S boosts
        // the rail clock, increasingly at larger scales, with a short inertial release.
        const float TargetBoost=Pulling?FMath::Max(0.f,float(-UMiningGamepadSubsystem::ReadMove(Player).Y)):0.f;
        PullbackBoost=FMath::Lerp(PullbackBoost,TargetBoost,1-FMath::Exp(-2.4f*Dt));
        const float ScalePhase=Pulling?FMath::Clamp((Shot-1+ShotTime/Duration(Shot))/6,0.f,1.f):0;
        PullbackRate=Shot>=0&&Shot<=6?1.5f*(1+PullbackBoost*(1.4f+2.6f*ScalePhase*ScalePhase)):1;
        ShotTime+=Dt*FMath::Clamp(FilmSpeed,.25f,4.f)*PullbackRate;
        // Interactive ending and speech always use real time, including preview.
        if(Shot>=7)ShotTime-=Dt*(FMath::Clamp(FilmSpeed,.25f,4.f)-1);
        if(Shot!=9&&Shot<13&&ShotTime>=Duration(Shot))
        {
            const float Remainder=ShotTime-Duration(Shot);
            // Resolve the exact rail endpoint before carrying time into the next
            // chapter. This also preserves the incoming title velocity.
            ShotTime=Duration(Shot);UpdateFilm(0);
            SetShot(Shot+1);ShotTime=Remainder;
        }

        UpdateFilm(Dt);
        if(Dt>0)FilmCameraVelocity=(Camera->GetActorLocation()-PreviousFilmPosition)/Dt;
        if(Shot==6)RetreatSpeed=FVector::Dist(Camera->GetActorLocation(),PreviousFilmPosition)/FMath::Max(.001f,Dt)/FMath::Clamp(FilmSpeed,.25f,4.f);
    }
    if(Shot<0&&GateIndex<=1)
    {
        BloodTravel+=Dt*(510+120*FMath::Sin(Age*5.2f));
        for(int I=0;I<Blood.Num();++I)
        {
            const FVector Origin=BloodOrigins[I];const float D=FMath::Fmod(Origin.X+BloodTravel*(.88f+(I%7)*.04f),VesselDistances.Last());
            const FRotator Frame=VesselRotation(D);
            const float NeckScale=FMath::Lerp(.24f,1.f,Ending::Ease((D/VesselDistances.Last()-.10f)/.12f));
            const FVector Offset=FVector(0,Origin.Y+FMath::Sin(Age*1.4f+I)*18,Origin.Z+FMath::Cos(Age*1.8f+I)*22)*NeckScale;
            Blood[I]->SetRelativeLocation(VesselPoint(D)+Frame.RotateVector(Offset));
            Blood[I]->SetRelativeRotation(Frame.Quaternion()*FRotator(I*31+FMath::Sin(Age+I)*14,I*43+Age*8,22).Quaternion());
            Blood[I]->SetRelativeScale3D(FVector(2.2,2.1,.62)*Ending::Ease(D/180)*Ending::Ease((VesselDistances.Last()-D)/450));
        }
    }
    if(StreetCrowd)StreetCrowd->UpdateCrowd(Shot>=0&&Shot<=1,Dt);
    if(!StreetCrowd&&Shot>=0&&Shot<=2&&Crowd)
    {
        for(int I=0;I<CrowdOrigins.Num();++I)
        {
            FTransform T=CrowdOrigins[I];FVector P=T.GetLocation();
            const float Travel=FMath::Sin(Age*.095f+I*.17f)*180;
            if(I%2)P.Y+=Travel;else P.X+=Travel;
            P.Z+=FMath::Sin(Age*4+I)*2.3;T.SetLocation(P);Crowd->UpdateInstanceTransform(I,T,false,false,true);
            CrowdHeads->UpdateInstanceTransform(I,FTransform(FRotator::ZeroRotator,P+FVector(0,0,69),FVector(.29,.29,.35)),false,false,true);
        }
        Crowd->MarkRenderStateDirty();CrowdHeads->MarkRenderStateDirty();
    }
}

float AEndingEmergence::BrainLens() const
{
    if(!Player||!Player->GetPawn()||Shot>=0)return 0;
    return Ending::Ease((GetActorTransform().InverseTransformPosition(Player->GetPawn()->GetActorLocation()).X-6100)/500.f);
}

FVector AEndingEmergence::GetGateApproach(int32 I) const
{
    I=FMath::Clamp(I,0,4);const float X=Ending::GateX[I]-330,Y=Ending::GateY[I];return GetActorTransform().TransformPosition(FVector(X,Y,Ending::Ground(X,Y)+110));
}

FString AEndingEmergence::Title() const
{
    if(Shot<0)return GateIndex<1?TEXT("01 / 最后一矿"):GateIndex<2?TEXT("02 / 顺着光流"):TEXT("03 / 一个念头正在连接");
    const TCHAR* Names[]={TEXT("04 / 念头，飘起"),TEXT("05 / 我们之中"),TEXT("06 / 万家灯火"),TEXT("07 / 薄薄的大气层"),TEXT("08 / 无重力的远方"),TEXT("09 / 宇宙，细胞质"),TEXT("10 / 每一个，都有宇宙"),TEXT("11 / WOoOW"),TEXT("12 / 坠入一个念头"),TEXT("13 / 向光钻掘"),TEXT("14 / 灵感的出口"),TEXT("WOoOW / 那个下午")};
    return Names[FMath::Clamp(Shot,0,11)];
}

FString AEndingEmergence::Hint() const
{
    if(Shot<0)
    {
        if(GateIndex==0)return TEXT("WASD 移动   鼠标 观察   左键 挖掘");
        if(bEnteringBrain)return TEXT("");
        if(GateIndex==1)return TEXT("W 加速   A / D 微调   鼠标 转头");
        if(GateIndex==2)return TEXT("鼠标 瞄准神经元   空格 跃迁");
    }
    if(Shot>=1&&Shot<=6)return TEXT("S 加速拉远   A / D 微调");
    if(Shot==9)return TEXT("W 加速挖掘   A / D 微调");
    return TEXT("");
}

FString AEndingEmergence::Subtitle() const
{
    if(Shot<0)
    {
        if(GateIndex==0&&Age<9)return TEXT("最后一块矿里，好像有光在流动。");
        if(bEnteringBrain)return TEXT("血流的尽头，微光开始彼此连接。");
        if(GateIndex==1&&Age<BeatAt+5)return TEXT("它和更多的微光，汇进了同一条河。");
        if(GateIndex==2&&Age<BeatAt)return TEXT("你不再拿着镐子。你就是那一道脉冲。");
        if(GateIndex>=5)return TEXT("一个念头，出现了。");
        return TEXT("");
    }
    if(Shot==0&&ShotTime>2)return TEXT("一个念头，飘了出来。");
    if(Shot==1&&ShotTime>9&&ShotTime<16)return TEXT("然后，是我们。");
    return TEXT("");
}

float AEndingEmergence::OverlayAlpha() const
{
    if(Shot<0)return 0;
    if(Shot>=10)return 0;

    // Thought-to-street and the scale journey both use image motion, not white.
    return 0;
}

void AEndingEmergence::EndPlay(const EEndPlayReason::Type Reason)
{
    if(PainterlyMaterial && Player && Player->GetPawn())
        if(auto* C=Player->GetPawn()->FindComponentByClass<UCameraComponent>())C->RemoveBlendable(PainterlyMaterial);
    RemoveCredits();ResetFinale();ClearEndingEffects();
    if(Mining)Mining->bFieldResetEnabled=SavedFieldResetEnabled;
    if(Ambience){Ambience->Stop();Ambience->DestroyComponent();}
    if(Music){Music->Stop();Music->DestroyComponent();}
    if(View&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    if(Voice){Voice->Stop();Voice->DestroyComponent();}
    View.Reset();if(Camera)Camera->Destroy();
    Super::EndPlay(Reason);
}
