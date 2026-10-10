#include "MiningExternalGeometry.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "Selections/MeshConnectedComponents.h"
#include "MeshQueries.h"

using namespace UE::Geometry;

FDynamicMesh3 MiningExternal::Box(const FVector& Size)
{
	FMinimalBoxMeshGenerator Generator;
	Generator.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(Size * 0.5));
	Generator.Generate();
	return FDynamicMesh3(&Generator);
}

bool MiningExternal::PrepareCutter(FDynamicMesh3& Mesh, FString& Error)
{
	if (Mesh.TriangleCount() == 0 || Mesh.TriangleCount() > 250000)
	{
		Error = TEXT("通路模型必须包含 1–250000 个三角形，请先简化模型。"); return false;
	}
	for (int32 Vertex : Mesh.VertexIndicesItr())
	{
		if (FVector(Mesh.GetVertex(Vertex)).ContainsNaN())
		{
			Error = TEXT("通路模型含有无效顶点坐标。"); return false;
		}
	}
	Mesh.DiscardAttributes();
	if (!Mesh.IsClosed())
	{
		FMergeCoincidentMeshEdges Weld(&Mesh);
		Weld.MergeVertexTolerance = 0.001; // cm: reconnect import seams, never fill actual holes.
		Weld.OnlyUniquePairs = true;
		Weld.Apply();
	}
	if (!Mesh.IsClosed() || !Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly))
	{
		Error = TEXT("通路模型不是有效封闭体积：请封口、合并重复顶点并修复非流形边。"); return false;
	}
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		if (Mesh.GetTriArea(Triangle) < 1.e-8)
		{
			Error = TEXT("通路模型存在退化三角形，请清理零面积面。"); return false;
		}
	}
	FDynamicMeshAABBTree3 Tree(&Mesh, true);
	if (Tree.TestSelfIntersection())
	{
		Error = TEXT("通路模型存在自交或重叠内部面：请在 Blender 做布尔并集，或分别导入切割体。"); return false;
	}
	FMeshConnectedComponents Components(&Mesh);
	Components.FindConnectedTriangles();
	const double ReferenceSign = FMath::Sign(TMeshQueries<FDynamicMesh3>::GetVolumeArea(Box(FVector(10))).X);
	for (const auto& Component : Components.Components)
	{
		const double Volume = TMeshQueries<FDynamicMesh3>::GetVolumeArea(Mesh, Component.Indices).X;
		if (!FMath::IsFinite(Volume) || FMath::Abs(Volume) < 1.e-6)
		{
			Error = TEXT("通路模型包含零体积部分。"); return false;
		}
		if (Volume * ReferenceSign < 0)
			for (int32 Triangle : Component.Indices) Mesh.ReverseTriOrientation(Triangle);
	}
	Mesh.CompactInPlace();
	return true;
}

bool MiningExternal::Build(const FVector& Size, double ShellThickness, const TArray<FDynamicMesh3>& Cutters,
	FDynamicMesh3& Rock, FDynamicMesh3& Shell, FString& Error)
{
	if (Size.ContainsNaN() || Size.GetMin() < 100 || Size.GetMax() > 100000 ||
		!FMath::IsFinite(ShellThickness) || ShellThickness < 10 || ShellThickness > 3000)
	{
		Error = TEXT("岩层各边须为 100–100000 cm，基岩厚度须为 10–3000 cm。"); return false;
	}
	FDynamicMesh3 NewRock = Box(Size);
	FDynamicMesh3 NewShell;
	if (!MiningChunks::Subtract(Box(Size + FVector(ShellThickness * 2)), NewRock, NewShell))
	{
		Error = TEXT("基岩外壳生成失败，原矿区保留。"); return false;
	}
	for (int32 Index = 0; Index < Cutters.Num(); ++Index)
	{
		FDynamicMesh3 Tool = Cutters[Index];
		if (!PrepareCutter(Tool, Error)) { Error = FString::Printf(TEXT("切割体 %d：%s"), Index + 1, *Error); return false; }
		FDynamicMesh3 CutRock, CutShell;
		if (!MiningChunks::Subtract(NewRock, Tool, CutRock) || !MiningChunks::Subtract(NewShell, Tool, CutShell))
		{
			Error = FString::Printf(TEXT("切割体 %d 布尔失败，请避免相切或共面，原矿区保留。"), Index + 1); return false;
		}
		NewRock = MoveTemp(CutRock);
		NewShell = MoveTemp(CutShell);
	}
	if (NewRock.TriangleCount() == 0 || NewShell.TriangleCount() == 0)
	{
		Error = TEXT("通路切除了全部岩层或全部基岩，请缩小切割体，原矿区保留。"); return false;
	}
	MiningChunks::Normals(NewRock);
	MiningChunks::Normals(NewShell);
	Rock = MoveTemp(NewRock);
	Shell = MoveTemp(NewShell);
	Error.Reset();
	return true;
}
