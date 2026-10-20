#include "ProjectTypographyEditorLibrary.h"
#include "BrainMiningFonts.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
    // Keep the supplied OTFs intact. Only the bytes embedded into the UI font
    // faces lose the 0.5 em leading; outlines, advances and kerning stay intact.
    bool NormalizeLeading(TArray<uint8>& Bytes)
    {
        auto Read16 = [&](uint32 Offset) -> uint16
        { return (uint16(Bytes[Offset]) << 8) | Bytes[Offset + 1]; };
        auto Read32 = [&](uint32 Offset) -> uint32
        { return (uint32(Read16(Offset)) << 16) | Read16(Offset + 2); };
        auto Write32 = [&](uint32 Offset, uint32 Value)
        { for (uint32 I = 0; I < 4; ++I) Bytes[Offset + I] = uint8(Value >> (24 - I * 8)); };
        if (Bytes.Num() < 12) return false;
        const uint32 Tables = Read16(4);
        if (12 + Tables * 16 > uint32(Bytes.Num())) return false;
        uint32 Head = 0, Hhea = 0, OS2 = 0;
        for (uint32 I = 0; I < Tables; ++I)
        {
            const uint32 Entry = 12 + I * 16;
            const uint32 Offset = Read32(Entry + 8), Length = Read32(Entry + 12);
            if (uint64(Offset) + Length > uint64(Bytes.Num())) return false;
            const uint32 Tag = Read32(Entry);
            if (Tag == 0x68656164 && Length >= 54) Head = Offset; // head
            if (Tag == 0x68686561 && Length >= 10) Hhea = Offset; // hhea
            if (Tag == 0x4F532F32 && Length >= 74) OS2 = Offset; // OS/2
        }
        if (!Head || !Hhea || !OS2) return false;
        Bytes[Hhea + 8] = Bytes[Hhea + 9] = 0;
        Bytes[OS2 + 72] = Bytes[OS2 + 73] = 0;
        Write32(Head + 8, 0);
        auto Checksum = [&](uint32 Offset, uint32 Length) -> uint32
        {
            uint32 Sum = 0;
            for (uint32 I = 0; I < Length; I += 4)
            {
                uint32 Word = 0;
                for (uint32 J = 0; J < 4; ++J)
                    Word = (Word << 8) | (I + J < Length ? Bytes[Offset + I + J] : 0);
                Sum += Word;
            }
            return Sum;
        };
        for (uint32 I = 0; I < Tables; ++I)
        {
            const uint32 Entry = 12 + I * 16;
            Write32(Entry + 4, Checksum(Read32(Entry + 8), Read32(Entry + 12)));
        }
        Write32(Head + 8, 0xB1B0AFBAu - Checksum(0, Bytes.Num()));
        return true;
    }

    bool Save(UObject* Asset)
    {
        Asset->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
    }

    template<class T> T* Asset(const FString& Name)
    {
        const FString Path = TEXT("/Game/Fonts/") + Name;
        if (auto* Existing = LoadObject<T>(nullptr, *(Path + TEXT(".") + Name))) return Existing;
        auto* Result = NewObject<T>(CreatePackage(*Path), *Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Result);
        return Result;
    }

    bool Import(const TCHAR* Weight)
    {
        const FString Source = FPaths::ProjectDir() / TEXT("SourceAssets/Fonts") / (FString(TEXT("SourceHanSansCN-")) + Weight + TEXT(".otf"));
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Source) || !NormalizeLeading(Bytes)) return false;
        auto* Face = Asset<UFontFace>(FString(TEXT("FF_SourceHanSansCN_")) + Weight);
        Face->Modify();
        Face->SourceFilename = Source;
        Face->Hinting = EFontHinting::Default;
        Face->LoadingPolicy = EFontLoadingPolicy::Inline;
        Face->LayoutMethod = EFontLayoutMethod::Metrics;
        Face->FontFaceData->SetData(MoveTemp(Bytes));
        Face->CacheSubFaces();
        Face->PostEditChange();
        if (!Save(Face)) return false;

        auto* Font = Asset<UFont>(FString(TEXT("F_SourceHanSansCN_")) + Weight);
        Font->Modify();
        Font->FontCacheType = EFontCacheType::Runtime;
        auto& Composite = Font->GetMutableInternalCompositeFont();
        Composite.DefaultTypeface.Fonts.Reset();
        auto& Entry = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
        Entry.Name = Weight;
        Entry.Font = FFontData(Face);
        Composite.MakeDirty();
        return Save(Font);
    }

    bool IsTitle(const UWidget* Widget)
    {
        const FString Name = Widget->GetName();
        return Name.Contains(TEXT("Title")) || Name.Contains(TEXT("Heading"))
            || Name.Contains(TEXT("Header")) || Name.Contains(TEXT("Subtitle"))
            || Name == TEXT("ItemName") || Name == TEXT("CatalogueName");
    }

    // Covers text blocks, editable text, rich text defaults, and nested button
    // or combo styles while preserving every non-font property of the widget.
    int32 Rewrite(UStruct* Type, void* Container, bool bTitle)
    {
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(Type); It; ++It)
        {
            auto* Struct = CastField<FStructProperty>(*It);
            if (!Struct) continue;
            for (int32 Index = 0; Index < Struct->ArrayDim; ++Index)
            {
                void* Value = Struct->ContainerPtrToValuePtr<void>(Container, Index);
                if (Struct->Struct == FSlateFontInfo::StaticStruct())
                {
                    auto& Font = *static_cast<FSlateFontInfo*>(Value);
                    Font = BrainMiningFonts::Restyle(Font, bTitle);
                    ++Count;
                }
                else Count += Rewrite(Struct->Struct, Value, bTitle);
            }
        }
        return Count;
    }
}

bool UProjectTypographyEditorLibrary::InstallProjectFonts()
{
    if (!Import(TEXT("Bold")) || !Import(TEXT("Medium"))) return false;
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game"));
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    Registry.Get().GetAssets(Filter, Assets);
    TArray<TSharedPtr<FJsonValue>> Report;
    bool Success = true;
    for (const auto& Data : Assets)
    {
        auto* BP = Cast<UWidgetBlueprint>(Data.GetAsset());
        if (!BP || !BP->WidgetTree) continue;
        BP->Modify();
        BP->WidgetTree->Modify();
        int32 Fonts = 0;
        BP->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            Widget->Modify();
            Fonts += Rewrite(Widget->GetClass(), Widget, IsTitle(Widget));
        });
        if (!Fonts) continue;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        const bool Saved = BP->Status != BS_Error && Save(BP);
        Success &= Saved;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("asset"), BP->GetPathName());
        Row->SetNumberField(TEXT("fonts"), Fonts);
        Row->SetBoolField(TEXT("saved"), Saved);
        Report.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), Success);
    Result->SetArrayField(TEXT("widgets"), Report);
    FString Json;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("Validation/ProjectFonts");
    IFileManager::Get().MakeDirectory(*ReportDir, true);
    FFileHelper::SaveStringToFile(Json, *(ReportDir / TEXT("installed.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("PROJECT_FONTS_INSTALLED %s"), *Json);
    return Success;
}


bool UProjectTypographyEditorLibrary::InstallCreditsFonts()
{
    for(const bool English:{false,true})
    {
        const FString Name=English?TEXT("CreditsRockwell"):TEXT("CreditsHiragino");
        const FString Source=FPaths::ProjectDir()/TEXT("SourceAssets/Fonts")/(English?TEXT("Rockwell.otf"):TEXT("HiraginoSansGB-W6.otf"));
        TArray<uint8> Bytes;
        if(!FFileHelper::LoadFileToArray(Bytes,*Source))return false;
        auto* Face=Asset<UFontFace>(TEXT("FF_")+Name);
        Face->Modify();Face->SourceFilename=Source;
        Face->Hinting=EFontHinting::Default;Face->LoadingPolicy=EFontLoadingPolicy::Inline;Face->LayoutMethod=EFontLayoutMethod::Metrics;
        Face->FontFaceData->SetData(MoveTemp(Bytes));Face->CacheSubFaces();Face->PostEditChange();
        if(!Save(Face))return false;
        auto* Font=Asset<UFont>(TEXT("F_")+Name);
        Font->Modify();Font->FontCacheType=EFontCacheType::Runtime;
        auto& Composite=Font->GetMutableInternalCompositeFont();Composite.DefaultTypeface.Fonts.Reset();
        auto& Entry=Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();Entry.Name=TEXT("Regular");Entry.Font=FFontData(Face);Composite.MakeDirty();
        if(!Save(Font))return false;
    }
    return true;
}
