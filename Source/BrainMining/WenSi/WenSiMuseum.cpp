#include "WenSi/WenSiPuzzle.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionCustom.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"

void AWenSiPuzzle::DressMuseum()
{
 if(!GEditor || GEditor->PlayWorld)return;
 UWorld* World=GEditor->GetEditorWorldContext().World();
 if(!World || World->GetOutermost()->GetName()!=TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"))return;
 for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("WenSiMuseumDecor")))return;
 TArray<UPackage*> Packages;
 auto Surface=[&](const TCHAR* Name,bool bGlow)->UMaterial*
 {
  const FString Path=FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name;
  if(auto Existing=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+Name)))return Existing;
  auto Package=CreatePackage(*Path);auto Material=NewObject<UMaterial>(Package,Name,RF_Public|RF_Standalone);
  auto Tint=NewObject<UMaterialExpressionVectorParameter>(Material);Tint->ParameterName=TEXT("Tint");Tint->Group=TEXT("Museum");Tint->DefaultValue=bGlow?FLinearColor(1.f,.88f,.68f):FLinearColor(.5f,.5f,.5f);
  Material->GetExpressionCollection().AddExpression(Tint);
  auto Amount=NewObject<UMaterialExpressionScalarParameter>(Material);Amount->ParameterName=bGlow?TEXT("Strength"):TEXT("Roughness");Amount->Group=TEXT("Museum");Amount->DefaultValue=bGlow?1.f:.72f;
  Material->GetExpressionCollection().AddExpression(Amount);
  if(bGlow)
  {
   Material->BlendMode=BLEND_Masked;Material->SetShadingModel(MSM_Unlit);Material->TwoSided=false;Material->bCastRayTracedShadows=false;
   // Pixel-sized face edges also work on the puzzle's translucent, intangible objects.
   auto Normal=NewObject<UMaterialExpressionVertexNormalWS>(Material);Material->GetExpressionCollection().AddExpression(Normal);
   auto Width=NewObject<UMaterialExpressionScalarParameter>(Material);Width->ParameterName=TEXT("OutlineWidth");Width->Group=TEXT("Museum");Width->DefaultValue=1.f;Material->GetExpressionCollection().AddExpression(Width);
   auto Offset=NewObject<UMaterialExpressionMultiply>(Material);Offset->A.Connect(0,Normal);Offset->ConstB=.05f;Material->GetExpressionCollection().AddExpression(Offset);
   auto UV=NewObject<UMaterialExpressionTextureCoordinate>(Material);Material->GetExpressionCollection().AddExpression(UV);
   auto Edge=NewObject<UMaterialExpressionCustom>(Material);Edge->OutputType=CMOT_Float1;Edge->Inputs.Reset();
   FCustomInput UVInput;UVInput.InputName=TEXT("UV");UVInput.Input.Connect(0,UV);Edge->Inputs.Add(UVInput);
   FCustomInput WidthInput;WidthInput.InputName=TEXT("Width");WidthInput.Input.Connect(0,Width);Edge->Inputs.Add(WidthInput);
   Edge->Code=TEXT("float2 e = min(UV, 1.0 - UV);\nfloat2 w = max(fwidth(UV) * max(Width, 0.1) * 1.5, float2(0.0001, 0.0001));\nreturn 1.0 - saturate(min(e.x / w.x, e.y / w.y));");Material->GetExpressionCollection().AddExpression(Edge);
   auto Mask=NewObject<UMaterialExpressionMultiply>(Material);Mask->A.Connect(0,Edge);Mask->B.Connect(0,Amount);Material->GetExpressionCollection().AddExpression(Mask);
   auto Color=NewObject<UMaterialExpressionMultiply>(Material);Color->A.Connect(0,Tint);Color->B.Connect(0,Amount);Material->GetExpressionCollection().AddExpression(Color);
   Material->GetEditorOnlyData()->EmissiveColor.Connect(0,Color);Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0,Offset);Material->GetEditorOnlyData()->OpacityMask.Connect(0,Mask);
  }
  else
  {
   auto Metal=NewObject<UMaterialExpressionScalarParameter>(Material);Metal->ParameterName=TEXT("Metallic");Metal->Group=TEXT("Museum");Metal->DefaultValue=0.f;Material->GetExpressionCollection().AddExpression(Metal);
   Material->GetEditorOnlyData()->BaseColor.Connect(0,Tint);Material->GetEditorOnlyData()->Roughness.Connect(0,Amount);Material->GetEditorOnlyData()->Metallic.Connect(0,Metal);
  }
  Material->PostEditChange();Material->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Material);Packages.Add(Package);return Material;
 };
 auto Parent=Surface(TEXT("M_MuseumSurface"),false);
 auto Glow=Surface(TEXT("M_InteractiveRim"),true);
 auto Palette=[&](const TCHAR* Name,FLinearColor Color,float Metal=0.f,float Roughness=.72f)->UMaterialInstanceConstant*
 {
  const FString Path=FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name;
  if(auto Existing=LoadObject<UMaterialInstanceConstant>(nullptr,*(Path+TEXT(".")+Name)))return Existing;
  auto Package=CreatePackage(*Path);auto Material=NewObject<UMaterialInstanceConstant>(Package,Name,RF_Public|RF_Standalone);
  Material->SetParentEditorOnly(Parent);Material->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Tint")),Color);
  Material->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic")),Metal);Material->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness")),Roughness);
  Material->PostEditChange();Material->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Material);Packages.Add(Package);return Material;
 };
 auto Cream=Palette(TEXT("MI_Limestone"),FLinearColor(.62f,.53f,.4f));
 auto Navy=Palette(TEXT("MI_DeepBlue"),FLinearColor(.028f,.065f,.085f));
 auto Gold=Palette(TEXT("MI_Brass"),FLinearColor(.48f,.29f,.095f),.7f,.38f);
 auto Dark=Palette(TEXT("MI_Charcoal"),FLinearColor(.045f,.047f,.05f));
 auto Tile=Palette(TEXT("MI_FloorStone"),FLinearColor(.28f,.26f,.22f));
 auto LightTile=Palette(TEXT("MI_LightStone"),FLinearColor(.5f,.45f,.36f));
 auto Wood=Palette(TEXT("MI_Walnut"),FLinearColor(.19f,.095f,.043f));
 auto Terracotta=Palette(TEXT("MI_Terracotta"),FLinearColor(.35f,.11f,.06f));
 auto Green=Palette(TEXT("MI_Patina"),FLinearColor(.065f,.22f,.18f),.35f);
 auto Red=Palette(TEXT("MI_Burgundy"),FLinearColor(.19f,.032f,.045f));
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
 // Existing architecture receives persistent materials so its palette survives reloads.
 for(TActorIterator<AActor> It(World);It;++It)
 {
  const FString Label=It->GetActorLabel();
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  for(auto Mesh:Meshes)
  {
   UMaterialInterface* Material=Label.Contains(TEXT("Floor"))?Tile:Label.Contains(TEXT("Ceiling"))?Dark:Label.Contains(TEXT("Pit"))?Dark:Cream;
   if(Label.Contains(TEXT("CageStop")))Material=Gold;
   if(auto Object=Cast<AWenSiObject>(*It))
   {
    Object->HighlightMaterial=Glow;
    Material=Object->ObjectKind==1?Gold:Object->ObjectKind==3?Red:Object->ObjectKind==4?Wood:Cream;
    if(Mesh->IsVisible())Mesh->SetOverlayMaterial(Glow);
   }
   Mesh->SetMaterial(0,Material);
  }
  TArray<UPointLightComponent*> Lights;It->GetComponents(Lights);
  for(auto Light:Lights){Light->SetIntensity(1800);Light->SetLightColor(FLinearColor(1.f,.85f,.66f));}
 }
 int32 Number=0;
 auto Shape=[&](FVector P,FVector Size,UMaterialInterface* Material,UStaticMesh* Mesh=nullptr,FRotator Rotation=FRotator::ZeroRotator,bool Solid=false)
 {
  auto Actor=World->SpawnActor<AStaticMeshActor>();Actor->SetActorLabel(FString::Printf(TEXT("Museum_%03d"),++Number));Actor->SetFolderPath(TEXT("Museum/Decoration"));Actor->Tags.Add(TEXT("WenSiMuseumDecor"));
  auto Component=Actor->GetStaticMeshComponent();Component->SetStaticMesh(Mesh?Mesh:Cube);Component->SetMaterial(0,Material);
  Component->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
  Actor->SetActorTransform(FTransform(Rotation,P,Size/100.f));return Actor;
 };
 auto Pedestal=[&](FVector P)
 {
  Shape(P+FVector(0,0,6),FVector(86,86,12),Dark,nullptr,FRotator::ZeroRotator,true);
  Shape(P+FVector(0,0,37),FVector(70,70,50),Navy,nullptr,FRotator::ZeroRotator,true);
  Shape(P+FVector(0,0,65),FVector(82,82,6),Gold);
 };
 auto Vase=[&](FVector P,UMaterialInterface* Material)
 {
  Shape(P+FVector(0,0,8),FVector(22,22,16),Material,Cylinder);
  Shape(P+FVector(0,0,30),FVector(36,36,32),Material,Cone,FRotator(180,0,0));
  Shape(P+FVector(0,0,54),FVector(20,20,18),Material,Cylinder);
  Shape(P+FVector(0,0,65),FVector(26,26,5),Gold,Cylinder);
 };
 // Checker stone floor and a restrained brass route inlay, all without new collision.
 for(int32 X=0;X<9;++X)for(int32 Y=0;Y<10;++Y)
  Shape(FVector(-500+X*80,-360+Y*80,.3),FVector(79,79,.5),(X+Y)%2?Tile:LightTile);
 for(float Y:{-96.f,96.f})Shape(FVector(-150,Y,.8),FVector(740,2,.6),Gold);
 for(float Y:{-390.f,390.f})
 {
  Shape(FVector(-145,Y,85),FVector(770,12,170),Navy);
  Shape(FVector(-145,Y,173),FVector(770,15,6),Gold);
  Shape(FVector(-145,Y,15),FVector(770,18,25),Dark);
  Shape(FVector(-145,Y,445),FVector(770,22,22),Cream);
  Shape(FVector(-145,Y,430),FVector(770,16,5),Gold);
  for(float X:{-475.f,-230.f,15.f,200.f})
  {
   Shape(FVector(X,Y,300),FVector(30,28,250),Cream);
   Shape(FVector(X,Y,182),FVector(44,38,16),Gold);
   Shape(FVector(X,Y,425),FVector(44,38,16),Gold);
  }
 }
 // Three varied artifact displays along the left wall, clear of the thinking route.
 for(int32 I=0;I<3;++I)
 {
  const FVector P(-375+I*225,-290,0);Pedestal(P);
  if(I==0)Vase(P+FVector(0,0,68),Terracotta);
  else if(I==1)
  {
   Shape(P+FVector(0,0,102),FVector(40,30,68),Green,Cone);
   Shape(P+FVector(0,0,140),FVector(26,26,26),Gold,nullptr,FRotator(0,25,15));
  }
  else
  {
   Shape(P+FVector(0,0,75),FVector(40,40,14),Cream);
   Shape(P+FVector(0,0,105),FVector(18,18,48),Terracotta,Cylinder);
   Shape(P+FVector(0,0,130),FVector(42,12,8),Gold);
  }
  // Open framed vitrine. Glass is suggested by the frame, keeping the low-fidelity style.
  for(float X:{-43.f,43.f})for(float Y:{-43.f,43.f})Shape(P+FVector(X,Y,126),FVector(3,3,115),Gold);
  Shape(P+FVector(0,0,185),FVector(90,90,6),Dark);
 }
 // A separate abstract sculpture and bench on the right; the puzzle sculpture stays unobstructed.
 Pedestal(FVector(-350,300,0));
 Shape(FVector(-350,300,112),FVector(50,26,80),Cream,nullptr,FRotator(0,25,12));
 Shape(FVector(-345,300,169),FVector(40,40,40),Green,nullptr,FRotator(20,30,10));
 Shape(FVector(100,310,48),FVector(165,48,12),Wood,nullptr,FRotator::ZeroRotator,true);
 for(float X:{40.f,160.f})Shape(FVector(X,310,24),FVector(12,40,42),Dark,nullptr,FRotator::ZeroRotator,true);
 // Framed geometric artworks, not written labels or extra puzzle descriptions.
 for(float X:{-360.f,-90.f,160.f})for(float Y:{-379.f,379.f})
 {
  Shape(FVector(X,Y,290),FVector(136,8,112),Gold);
  Shape(FVector(X,Y+(Y<0?6:-6),290),FVector(124,4,100),Dark);
  Shape(FVector(X-22,Y+(Y<0?10:-10),290),FVector(34,2,65),Terracotta);
  Shape(FVector(X+20,Y+(Y<0?12:-12),308),FVector(40,2,35),Cream,nullptr,FRotator(0,0,20));
 }
 // Exhibition arch and gallery ceiling beams leave the sculpture's vertical path free.
 for(float Y:{-127.f,127.f})Shape(FVector(235,Y,130),FVector(35,12,260),Gold);
 Shape(FVector(235,0,260),FVector(35,270,12),Gold);
 for(float X:{-430.f,-230.f,100.f})Shape(FVector(X,0,480),FVector(16,790,16),Cream);
 for(float Y:{-300.f,300.f})Shape(FVector(-140,Y,460),FVector(740,8,8),Dark);
 for(float X:{-370.f,-145.f,80.f})for(float Y:{-300.f,300.f})
 {
  Shape(FVector(X,Y,443),FVector(18,18,30),Dark,Cylinder,FRotator(20,0,0));
  auto Actor=World->SpawnActor<AActor>();Actor->Tags.Add(TEXT("WenSiMuseumDecor"));Actor->SetFolderPath(TEXT("Museum/Lights"));Actor->SetActorLabel(FString::Printf(TEXT("Museum_Spot_%03d"),++Number));
  auto Light=NewObject<USpotLightComponent>(Actor);Actor->SetRootComponent(Light);Actor->AddInstanceComponent(Light);Light->SetMobility(EComponentMobility::Movable);
  Light->SetIntensity(7000);Light->SetLightColor(FLinearColor(1.f,.83f,.61f));Light->SetAttenuationRadius(650);Light->SetOuterConeAngle(35);Light->SetCastShadows(false);Light->RegisterComponent();Actor->SetActorLocation(FVector(X,Y,425));Actor->SetActorRotation(FRotator(-90,0,0));
 }
 // Rear conservation gallery: shallow wall panels, repeated framing and illuminated edges.
 for(float X:{400.f,730.f,1280.f,1430.f})for(float Y:{-116.f,116.f})
 {
  Shape(FVector(X,Y,125),FVector(115,4,170),Navy);
  Shape(FVector(X,Y+(Y<0?3:-3),125),FVector(95,3,140),Gold);
  Shape(FVector(X,Y+(Y<0?5:-5),125),FVector(87,3,132),Dark);
  Shape(FVector(X,Y+(Y<0?7:-7),135),FVector(25,2,78),Terracotta,nullptr,FRotator(0,0,15));
 }
 for(float Y:{-112.f,112.f})
 {
  Shape(FVector(1000,Y,218),FVector(995,3,4),Gold);
  Shape(FVector(665,Y,10),FVector(430,4,18),Dark);
  Shape(FVector(1330,Y,10),FVector(330,4,18),Dark);
 }
 auto Exposure=World->SpawnActor<APostProcessVolume>();Exposure->Tags.Add(TEXT("WenSiMuseumDecor"));Exposure->SetActorLabel(TEXT("Museum_Exposure"));Exposure->SetFolderPath(TEXT("Museum/Lights"));Exposure->bUnbound=true;
 Exposure->Settings.bOverride_AutoExposureMinBrightness=true;Exposure->Settings.bOverride_AutoExposureMaxBrightness=true;
 Exposure->Settings.AutoExposureMinBrightness=1.f;Exposure->Settings.AutoExposureMaxBrightness=1.f;
 const bool AssetsSaved=UEditorLoadingAndSavingUtils::SavePackages(Packages,true);
 const bool MapSaved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge"));
 UE_LOG(LogTemp,Display,TEXT("WenSi museum decorated=%d assets_saved=%d map_saved=%d"),Number,AssetsSaved,MapSaved);
}
static FAutoConsoleCommand DressWenSiMuseum(TEXT("WenSi.DressMuseum"),TEXT("Dress the screen/bridge level as a low-fidelity museum"),FConsoleCommandDelegate::CreateStatic(&AWenSiPuzzle::DressMuseum));
#endif
