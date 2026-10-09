#include "MiningDesignerLibrary.h"
#include "SMiningManualEditor.h"
#include "SMiningSpatialEditor.h"
#include "MiningSpatialSafeEditing.h"
void UMiningDesignerLibrary::OpenSpatialEditor() { SMiningSpatialEditor::Open(); }
bool UMiningDesignerLibrary::CheckSpatialEdit(AActor* Actor) { return MiningSpatialSafeEditing::Apply(Actor, true); }
bool UMiningDesignerLibrary::SmoothSpatialIssue() { return MiningSpatialSafeEditing::SmoothIssue(); }
FString UMiningDesignerLibrary::SpatialEditingStatus() { return MiningSpatialSafeEditing::Status() + TEXT("\n") + MiningSpatialSafeEditing::Issue().Reason + TEXT("\n") + MiningSpatialSafeEditing::TerrainStatus() + TEXT("\n") + MiningSpatialSafeEditing::SaveStatus(); }
FString UMiningDesignerLibrary::SpatialDragPerformance()
{
	const auto& Stats = MiningSpatialSafeEditing::DragStats();
	return FString::Printf(TEXT("{\"requests\":%d,\"coalesced\":%d,\"committed\":%d,\"discarded\":%d,\"pending\":%s,\"last_input_ms\":%.3f,\"max_input_ms\":%.3f,\"last_compute_ms\":%.3f,\"last_commit_ms\":%.3f,\"max_commit_ms\":%.3f,\"last_latency_ms\":%.3f}"), Stats.Requests, Stats.Coalesced, Stats.Committed, Stats.Discarded, MiningSpatialSafeEditing::HasPendingInteractive() ? TEXT("true") : TEXT("false"), Stats.LastInputMilliseconds, Stats.MaxInputMilliseconds, Stats.LastComputeMilliseconds, Stats.LastCommitMilliseconds, Stats.MaxCommitMilliseconds, Stats.LastLatencyMilliseconds);
}

void UMiningDesignerLibrary::OpenManualEditor() { SMiningManualEditor::Open(); }
#include "MiningDesignerCatalog.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Framework/Docking/TabManager.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 MaxPresetCharacters = 131072;

struct FStagedDesignerValue
{
	FProperty* Property;
	void* Data;
	explicit FStagedDesignerValue(FProperty* InProperty) : Property(InProperty)
	{
		Data = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
		Property->InitializeValue(Data);
	}
	~FStagedDesignerValue()
	{
		Property->DestroyValue(Data);
		FMemory::Free(Data);
	}
};

bool ValidatePropertyText(FProperty* Property, const FString& Text, FString& Error)
{
	const FStructProperty* Struct = CastField<FStructProperty>(Property);
	const FArrayProperty* Array = CastField<FArrayProperty>(Property);
	if (!Struct && !Array) return true;
	const auto Reject = [&Error, Property]()
	{
		Error = MiningDesigner::Caption(Property->GetFName()) + TEXT("：包含未知、重复或格式错误的子参数");
		return false;
	};
	const FString Trimmed = Text.TrimStartAndEnd();
	if (!Trimmed.StartsWith(TEXT("(")) || !Trimmed.EndsWith(TEXT(")"))) return Reject();
	TArray<FString> Items;
	int32 Start = 1;
	int32 Depth = 0;
	for (int32 Index = 1; Index < Trimmed.Len() - 1; ++Index)
	{
		if (Trimmed[Index] == '(') ++Depth;
		if (Trimmed[Index] == ')') --Depth;
		if (Depth < 0 || Depth > 16) return Reject();
		if (Trimmed[Index] == ',' && Depth == 0)
		{
			Items.Add(Trimmed.Mid(Start, Index - Start).TrimStartAndEnd());
			Start = Index + 1;
		}
	}
	if (Depth != 0) return Reject();
	const FString Last = Trimmed.Mid(Start, Trimmed.Len() - Start - 1).TrimStartAndEnd();
	if (!Last.IsEmpty()) Items.Add(Last);
	if (Array && Items.Num() > 64) return Reject();
	TSet<FName> Seen;
	for (const FString& Item : Items)
	{
		if (Item.IsEmpty()) return Reject();
		if (Array)
		{
			if (!ValidatePropertyText(Array->Inner, Item, Error)) return false;
			continue;
		}
		FString Key, Value;
		if (!Item.Split(TEXT("="), &Key, &Value)) return Reject();
		const FName Name(*Key.TrimStartAndEnd());
		FProperty* Child = Struct->Struct->FindPropertyByName(Name);
		if (!Child || Seen.Contains(Name) || Value.TrimStartAndEnd().IsEmpty()) return Reject();
		Seen.Add(Name);
		if (!ValidatePropertyText(Child, Value, Error)) return false;
	}
	return true;
}

bool ValidateValue(FProperty* Property, const void* Data, FString& Error)
{
	const auto Reject = [&Error, Property](const FString& Reason)
	{
		Error = MiningDesigner::Caption(Property->GetFName()) + TEXT("：") + Reason;
		return false;
	};
	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
	{
		const double Value = Numeric->IsFloatingPoint() ? Numeric->GetFloatingPointPropertyValue(Data) : double(Numeric->GetSignedIntPropertyValue(Data));
		if (!FMath::IsFinite(Value)) return Reject(TEXT("不能使用非有限数值"));
		if (Property->HasMetaData("ClampMin") && Value < FCString::Atod(*Property->GetMetaData("ClampMin")) - 0.000001) return Reject(TEXT("小于允许的最小值"));
		if (Property->HasMetaData("ClampMax") && Value > FCString::Atod(*Property->GetMetaData("ClampMax")) + 0.000001) return Reject(TEXT("大于允许的最大值"));
		return true;
	}
	if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
	{
		const int64 Value = Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Data);
		return (Enum->GetEnum()->IsValidEnumValue(Value) && Value < Enum->GetEnum()->GetMaxEnumValue()) || Reject(TEXT("不是有效选项"));
	}
	if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
	{
		for (TFieldIterator<FProperty> Child(Struct->Struct); Child; ++Child)
		{
			if (!ValidateValue(*Child, Child->ContainerPtrToValuePtr<void>(Data), Error)) return false;
		}
		return true;
	}
	if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Values(Array, Data);
		if (Values.Num() > 64) return Reject(TEXT("列表最多支持64项"));
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (!ValidateValue(Array->Inner, Values.GetRawPtr(Index), Error)) return false;
		}
		return true;
	}
	return CastField<FBoolProperty>(Property) != nullptr || Reject(TEXT("预设不支持此类型"));
}

FString CaptureValues(const UObject* Target, const UObject* Values, EMiningDesignerSection Section)
{
	UClass* Type = MiningDesigner::Family(Target);
	if (!Type || !Values) return {};
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("family"), Type->GetPathName());
	Root->SetNumberField(TEXT("section"), int32(Section));
	TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
	for (FName Name : MiningDesigner::Fields(Target, Section))
	{
		FProperty* Property = Target->GetClass()->FindPropertyByName(Name);
		if (!MiningDesigner::Transferable(Property)) continue;
		FString Value;
		const void* Data = Property->ContainerPtrToValuePtr<void>(Values);
		Property->ExportTextItem_Direct(Value, Data, Data, const_cast<UObject*>(Values), PPF_None);
		Fields->SetStringField(Name.ToString(), Value);
	}
	if (Fields->Values.IsEmpty()) return {};
	Root->SetObjectField(TEXT("values"), Fields);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return Json;
}

FString PresetDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("MiningDesignerPresets"));
}

FString PresetPrefix(const UObject* Target, EMiningDesignerSection Section)
{
	UClass* Type = MiningDesigner::Family(Target);
	return Type ? FString::Printf(TEXT("%s-%d-"), *Type->GetName(), int32(Section)) : FString();
}

bool PresetPath(UObject* Target, EMiningDesignerSection Section, const FString& Name, FString& Path, FString& Error)
{
	const FString Prefix = PresetPrefix(Target, Section);
	if (Prefix.IsEmpty() || Name.IsEmpty() || Name.Len() > 64 || Name != Name.TrimStartAndEnd() || Name != FPaths::MakeValidFileName(Name)
		|| Name.Contains(TEXT("..")) || Name.EndsWith(TEXT(".")))
	{
		Error = TEXT("预设名称需为1至64个字符，不能包含路径、冒号、首尾空格或连续句点。");
		return false;
	}
	Path = PresetDirectory() / (Prefix + Name + TEXT(".json"));
	return true;
}
}

void UMiningDesignerLibrary::OpenDesigner()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("MiningDesigner"));
}

bool UMiningDesignerLibrary::CanEdit(UObject* Target)
{
	if (!GEditor || GEditor->IsPlaySessionInProgress() || !IsValid(Target) || !MiningDesigner::Family(Target)) return false;
	const AActor* Actor = Cast<AActor>(Target);
	return Actor && Actor->GetWorld() == GEditor->GetEditorWorldContext().World() && !Actor->IsActorBeingDestroyed() && !Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
}

FString UMiningDesignerLibrary::CapturePreset(UObject* Target, EMiningDesignerSection Section)
{
	return CanEdit(Target) ? CaptureValues(Target, Target, Section) : FString();
}

bool UMiningDesignerLibrary::ApplyPreset(UObject* Target, EMiningDesignerSection Section, const FString& Json, FString& Error)
{
	Error.Reset();
	if (!CanEdit(Target)) { Error = TEXT("请停止试玩并选择当前编辑关卡中的有效对象。"); return false; }
	if (Json.IsEmpty() || Json.Len() > MaxPresetCharacters) { Error = TEXT("预设为空或超过128K字符限制。"); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) { Error = TEXT("不是有效的JSON预设。"); return false; }
	double Version = 0, SavedSection = -1;
	FString Family;
	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1 || !Root->TryGetStringField(TEXT("family"), Family)
		|| Family != MiningDesigner::Family(Target)->GetPathName() || !Root->TryGetNumberField(TEXT("section"), SavedSection) || SavedSection != int32(Section)
		|| !Root->TryGetObjectField(TEXT("values"), Values) || !Values || (*Values)->Values.IsEmpty())
	{
		Error = TEXT("预设版本、对象类型或分类不匹配。参数没有修改。");
		return false;
	}
	const TArray<FName> Allowed = MiningDesigner::Fields(Target, Section);
	TArray<TUniquePtr<FStagedDesignerValue>> Staged;
	for (const auto& Pair : (*Values)->Values)
	{
		const FName Name(*Pair.Key);
		FProperty* Property = Target->GetClass()->FindPropertyByName(Name);
		FString Text;
		if (!Allowed.Contains(Name) || !MiningDesigner::Transferable(Property) || !Pair.Value->TryGetString(Text))
		{
			Error = TEXT("预设包含未允许的参数：") + Name.ToString();
			return false;
		}
		if (!ValidatePropertyText(Property, Text, Error)) return false;
		TUniquePtr<FStagedDesignerValue> Value = MakeUnique<FStagedDesignerValue>(Property);
		Property->CopyCompleteValue(Value->Data, Property->ContainerPtrToValuePtr<void>(Target));
		FStringOutputDevice ImportErrors;
		const TCHAR* End = Property->ImportText_Direct(*Text, Value->Data, nullptr, PPF_None, &ImportErrors);
		while (End && FChar::IsWhitespace(*End)) ++End;
		if (!End || *End || !ImportErrors.IsEmpty()) { Error = TEXT("无法解析参数：") + Name.ToString(); return false; }
		if (!ValidateValue(Property, Value->Data, Error)) return false;
		Staged.Add(MoveTemp(Value));
	}
	const FScopedTransaction Transaction(FText::FromString(TEXT("应用矿业策划参数")));
	Target->Modify();
	for (const auto& Value : Staged)
	{
		Target->PreEditChange(Value->Property);
		Value->Property->CopyCompleteValue(Value->Property->ContainerPtrToValuePtr<void>(Target), Value->Data);
		FPropertyChangedEvent Event(Value->Property, EPropertyChangeType::ValueSet);
		Target->PostEditChangeProperty(Event);
	}
	Target->MarkPackageDirty();
	return true;
}

bool UMiningDesignerLibrary::ResetSection(UObject* Target, EMiningDesignerSection Section, FString& Error)
{
	if (!CanEdit(Target)) { Error = TEXT("请停止试玩并选择有效对象。"); return false; }
	return ApplyPreset(Target, Section, CaptureValues(Target, Target->GetClass()->GetDefaultObject(), Section), Error);
}

bool UMiningDesignerLibrary::SavePreset(UObject* Target, EMiningDesignerSection Section, const FString& Name, bool bOverwrite, FString& Error)
{
	Error.Reset();
	const FString Json = CapturePreset(Target, Section);
	FString Path;
	if (Json.IsEmpty()) { Error = TEXT("当前对象没有可保存的参数，或试玩尚未结束。"); return false; }
	if (!PresetPath(Target, Section, Name, Path, Error)) return false;
	if (!bOverwrite && FPaths::FileExists(Path)) { Error = TEXT("同名预设已存在，请换名或确认覆盖。"); return false; }
	if (!IFileManager::Get().MakeDirectory(*PresetDirectory(), true) || !FFileHelper::SaveStringToFile(Json, *(Path + TEXT(".tmp")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Error = TEXT("无法写入预设目录，请检查权限。"); return false;
	}
	if (!IFileManager::Get().Move(*Path, *(Path + TEXT(".tmp")), bOverwrite, false)) { Error = TEXT("无法完成预设保存，原预设未被主动删除。"); return false; }
	return true;
}

bool UMiningDesignerLibrary::LoadPreset(UObject* Target, EMiningDesignerSection Section, const FString& Name, FString& Error)
{
	FString Path, Json;
	if (!PresetPath(Target, Section, Name, Path, Error)) return false;
	const int64 Size = IFileManager::Get().FileSize(*Path);
	if (Size < 0 || Size > MaxPresetCharacters * 4 || !FFileHelper::LoadFileToString(Json, *Path)) { Error = TEXT("预设不存在、不可读取或文件过大。"); return false; }
	return ApplyPreset(Target, Section, Json, Error);
}

TArray<FString> UMiningDesignerLibrary::ListPresets(UObject* Target, EMiningDesignerSection Section)
{
	TArray<FString> Result, Files;
	const FString Prefix = PresetPrefix(Target, Section);
	if (Prefix.IsEmpty()) return Result;
	IFileManager::Get().FindFiles(Files, *(PresetDirectory() / (Prefix + TEXT("*.json"))), true, false);
	for (const FString& File : Files) Result.Add(File.Mid(Prefix.Len()).LeftChop(5));
	Result.Sort();
	return Result;
}
