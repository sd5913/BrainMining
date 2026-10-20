#include "MiningNiagaraEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraEmitter.h"
#include "UObject/UObjectHash.h"

int32 UMiningNiagaraEditorLibrary::BindRendererScalar(UNiagaraSystem* System,FName MaterialParameter,FName UserParameter)
{
    if(!System || !UserParameter.ToString().StartsWith(TEXT("User.")))return 0;
    System->Modify();
    const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetFloatDef(),UserParameter);
    System->GetExposedParameters().SetParameterValue(0.f,Variable,true);
    TArray<UObject*> Objects;GetObjectsWithOuter(System,Objects,true);int32 Count=0;
    for(auto* Object:Objects)if(auto* Renderer=Cast<UNiagaraSpriteRendererProperties>(Object))
    {
        Renderer->Modify();
        auto& Bindings=Renderer->MaterialParameters.AttributeBindings;
        Bindings.RemoveAll([&](const auto& B){return B.MaterialParameterName==MaterialParameter;});
        auto& Binding=Bindings.AddDefaulted_GetRef();Binding.MaterialParameterName=MaterialParameter;
        Binding.NiagaraVariable=Variable;Binding.CacheValues(Renderer->GetTypedOuter<UNiagaraEmitter>());
        Renderer->PostEditChange();++Count;
    }
    System->MarkPackageDirty();return Count;
}

FString UMiningNiagaraEditorLibrary::InspectObject(UObject* Object)
{
    if(!Object)return TEXT("{}");
    auto Data=MakeShared<FJsonObject>();
    for(TFieldIterator<FProperty> It(Object->GetClass());It;++It)
    {
        FString Value;
        It->ExportTextItem_Direct(Value,It->ContainerPtrToValuePtr<void>(Object),nullptr,Object,PPF_None);
        Data->SetStringField(It->GetName(),Value);
    }
    FString Out;FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Out));return Out;
}

bool UMiningNiagaraEditorLibrary::SetObjectProperty(UObject* Object,FName PropertyName,const FString& Value)
{
    if(!Object || !Object->IsInA(UNiagaraSystem::StaticClass()))return false;
    auto* Property=FindFProperty<FProperty>(Object->GetClass(),PropertyName);
    if(!Property)return false;
    Object->Modify();
    if(!Property->ImportText_Direct(*Value,Property->ContainerPtrToValuePtr<void>(Object),Object,PPF_None))return false;
    FPropertyChangedEvent Event(Property);
    Object->PostEditChangeProperty(Event);Object->MarkPackageDirty();return true;
}

FString UMiningNiagaraEditorLibrary::InspectStack(UNiagaraSystem* System,FName Emitter,FName Script,FName Module)
{
    if(!System)return TEXT("{}");
    FNiagaraExt_StackItemReference Ref(System,Emitter,Script,Module);
    FNiagaraExternalEditContext Context(Ref);
    FString Out;
    if(Emitter.IsNone())
    {
        FNiagaraExt_SystemSummary Summary;
        UNiagaraExternalEditUtilities::GetSystemSummary(System,Summary,Context);
        FJsonObjectConverter::UStructToJsonObjectString(Summary,Out);
    }
    else if(Module.IsNone())
    {
        FNiagaraExt_EmitterTopology Topology;
        UNiagaraExternalEditUtilities::GetEmitterTopology(Ref,Topology,Context);
        FJsonObjectConverter::UStructToJsonObjectString(Topology,Out);
    }
    else
    {
        FNiagaraExt_ModuleInputValues Values;
        UNiagaraExternalEditUtilities::GetModuleInputValues(Ref,Values,Context);
        TSharedRef<FJsonObject> Object=MakeShared<FJsonObject>();
        for(const auto& Entry:Values.Inputs)
        {
            if(!Entry.Value.IsValid())continue;
            TSharedRef<FJsonObject> Item=MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("type"),Entry.Value.GetScriptStruct()->GetPathName());
            auto Data=MakeShared<FJsonObject>();
            FJsonObjectConverter::UStructToJsonObject(Entry.Value.GetScriptStruct(),Entry.Value.GetMemory(),Data,0,0);
            Item->SetObjectField(TEXT("value"),Data);
            Object->SetObjectField(Entry.Name.ToString(),Item);
        }
        FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Out));
    }
    return Out;
}

bool UMiningNiagaraEditorLibrary::SetStackValue(UNiagaraSystem* System,FName Emitter,FName Script,FName Module,const TArray<FName>& Inputs,const FString& StructPath,const FString& Json)
{
    if(!System)return false;
    auto* Struct=LoadObject<UScriptStruct>(nullptr,*StructPath);if(!Struct)return false;
    FNiagaraExt_StackItemReference Ref(System,Emitter,Script,Module);Ref.InputNameStack=Inputs;
    FNiagaraExternalEditContext Context(Ref);
    FNiagaraExt_StackInputValue Value;Value.InitializeAs(Struct);
    TSharedPtr<FJsonObject> Data;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data) || !Data.IsValid())return false;
    if(!FJsonObjectConverter::JsonObjectToUStruct(Data.ToSharedRef(),Struct,Value.GetMutableMemory(),0,0))return false;
    System->Modify();UNiagaraExternalEditUtilities::SetStackInputData(Ref,Value,Context);
    return Context.Errors.IsEmpty();
}

bool UMiningNiagaraEditorLibrary::CompileSystem(UNiagaraSystem* System)
{
    if(!System)return false;
    System->RequestCompile(false);System->WaitForCompilationComplete(true,false);
    return System->IsReadyToRun();
}
