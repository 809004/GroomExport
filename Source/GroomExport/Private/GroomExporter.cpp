// Fill out your copyright notice in the Description page of Project Settings.


#include "GroomExporter.h"

#include "AssetExportTask.h"
#include "GroomAsset.h"
#include "HairDescription.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "Alembic/AbcGeom/All.h"
#include "Alembic/AbcCoreOgawa/All.h"
#include "Alembic/AbcCoreAbstract/All.h"
#include "Alembic/AbcCoreFactory/All.h"
#include "Alembic/Abc/OArchive.h"
#include "Alembic/Abc/OObject.h"

#include "Alembic/Abc/All.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

/*
Alembic文件属性说明：
Alembic::AbcGeom::kConstantScope -- 逐对象的属性；
Alembic::AbcGeom::kUniformScope -- 逐发丝的属性；
Alembic::AbcGeom::kVertexScope -- 逐顶点的属性。
*/

UGroomExporter::UGroomExporter()
{
	FormatExtension.Add("abc");
	FormatDescription.Add(TEXT("Alembic file"));

	SupportedClass = UGroomAsset::StaticClass();
	bText = false;
}

bool UGroomExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UGroomAsset* GroomAsset = CastChecked<UGroomAsset>(Object);
	if (!GroomAsset)
	{
		return false;
	}

	TArray<FHairGroupInfoWithVisibility>& GroupsInfo = GroomAsset->HairGroupsInfo;

	FHairDescription HairDescription = GroomAsset->GetHairDescription();
	if (!HairDescription.IsValid())
	{
		return false;
	}

	//*********************************************************************************************************************
	//Alembic接口
	//坐标、宽度、每个发束包含的顶点数量作为曲线的基本数据；其他数据则作为属性附加
	const int NumChildrenObj = GroupsInfo.Num();
	Alembic::Abc::OArchive OutputAr(Alembic::AbcCoreOgawa::WriteArchive(), TCHAR_TO_UTF8(*UExporter::CurrentFilename)); //也不允许中文路径！！！
	Alembic::Abc::OObject TopObject = Alembic::Abc::OObject(OutputAr, Alembic::Abc::kTop);

	TVertexAttributesConstRef<FVector3f> VertexPositionRef = HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
	TStrandAttributesConstRef<int> StrandNumVerticesRef = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::VertexCount);
	TVertexAttributesRef<FVector3f> VertexColorRef = HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Color);
	TStrandAttributesConstRef<FVector2f> StrandRootUVRef = HairDescription.StrandAttributes().GetAttributesRef<FVector2f>(HairAttribute::Strand::RootUV);
	TStrandAttributesConstRef<int> StrandGroupIDRef = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::GroupID);

	int GlobalNumVertex = 0;
	int GlobalNumStrand = 0;
	for (int ObjIndex = 0; ObjIndex < NumChildrenObj; ObjIndex++)
	{
		int NumCurves = GroupsInfo[ObjIndex].NumCurves;
		int NumVerttices = GroupsInfo[ObjIndex].NumCurveVertices;
		std::string ObjectName = "exportObj" + std::to_string(ObjIndex);
		std::string CurveName = "exportCurve" + std::to_string(ObjIndex);
				
		Alembic::AbcGeom::OXform Xform = Alembic::AbcGeom::OXform(TopObject, ObjectName);
		Alembic::AbcGeom::OCurves ChildCurve(Xform, CurveName);
		Alembic::AbcGeom::OCurvesSchema& CurvesSchema = ChildCurve.getSchema();

		//毛发基础数据
		float ConstantWidths[] = { 0.01f };
		Alembic::AbcGeom::OFloatGeomParam::Sample WidthSample(
			Alembic::AbcGeom::FloatArraySample(ConstantWidths, 1),
			Alembic::AbcGeom::kConstantScope
		);

		Alembic::AbcGeom::OV2fGeomParam::Sample UvSample; //metahuman的毛发里没有顶点UV的属性，放空。

		TArray<float> Positions;
		for (int VertexIndex = 0; VertexIndex < NumVerttices; VertexIndex++)
		{
			FVertexID VertexID(VertexIndex + GlobalNumVertex);
			FVector3f TempVec = VertexPositionRef[VertexID];
			Positions.Add(TempVec.X);
			Positions.Add(TempVec.Y);
			Positions.Add(TempVec.Z);
		}

		TArray<int32> StrandVertex; //每个发丝包含的顶点数量
		for (int CurveIndex = 0; CurveIndex < NumCurves; CurveIndex++)
		{
			FStrandID StrandID(CurveIndex + GlobalNumStrand);
			StrandVertex.Add(StrandNumVerticesRef[StrandID]);
		}

		Alembic::AbcGeom::OCurvesSchema::Sample CurveSample(
			Alembic::AbcGeom::V3fArraySample((const Alembic::AbcGeom::V3f*)Positions.GetData(), NumVerttices),
			Alembic::AbcGeom::Int32ArraySample(StrandVertex.GetData(), NumCurves),
			Alembic::AbcGeom::kCubic,
			Alembic::AbcGeom::kNonPeriodic,
			WidthSample,
			UvSample
		);

		CurvesSchema.set(CurveSample);

		//毛发属性(顶点颜色、根UV)
		Alembic::Abc::OCompoundProperty Props = CurvesSchema.getArbGeomParams();

		if (VertexColorRef.IsValid())
		{
			std::vector<Alembic::AbcGeom::V3f> ColorVals;
			for (int ColorIndex = 0; ColorIndex < NumVerttices; ColorIndex++)
			{
				FVertexID ColorID(ColorIndex + GlobalNumVertex);
				FVector3f TempColor = VertexColorRef[ColorID];
				Alembic::AbcGeom::V3f TempV3f(TempColor.X, TempColor.Y, TempColor.Z);
				ColorVals.push_back(TempV3f);
			}

			Alembic::AbcGeom::OV3fGeomParam ColorParam(Props, "groom_color", false, Alembic::AbcGeom::kVertexScope, 1);
			Alembic::AbcGeom::V3fArraySample ColorArray((Alembic::AbcGeom::V3f*)ColorVals.data(), ColorVals.size());
			Alembic::AbcGeom::OV3fGeomParam::Sample ColorSamp(ColorArray, Alembic::AbcGeom::kVertexScope);
			ColorParam.set(ColorSamp);
		}

		if (StrandRootUVRef.IsValid())
		{
			std::vector<Alembic::AbcGeom::V2f> StrandRootUVs;
			for (int RootIndex = 0; RootIndex < NumCurves; RootIndex++)
			{
				FStrandID RootID(RootIndex + GlobalNumStrand);
				FVector2f TempUV = StrandRootUVRef[RootID];
				Alembic::AbcGeom::V2f TempV2f(TempUV.X, TempUV.Y);
				StrandRootUVs.push_back(TempV2f);
			}

			Alembic::AbcGeom::OV2fGeomParam RootUVParam(Props, "groom_root_uv", false, Alembic::AbcGeom::kUniformScope, 1);
			Alembic::AbcGeom::V2fArraySample RootUVArray((Alembic::AbcGeom::V2f*)StrandRootUVs.data(), StrandRootUVs.size());
			Alembic::AbcGeom::OV2fGeomParam::Sample RootUVSamp(RootUVArray, Alembic::AbcGeom::kUniformScope);
			RootUVParam.set(RootUVSamp);
		}

		if (StrandGroupIDRef.IsValid())
		{
			std::vector<Alembic::Util::int16_t> GroupIDs; //数据类型要相对应，否则重新导入引擎时，无法识别对应属性。
			for (int GroupIDIndex = 0; GroupIDIndex < NumCurves; GroupIDIndex++)
			{
				FStrandID GroupID(GroupIDIndex + GlobalNumStrand);
				Alembic::Util::int16_t TempInt = (Alembic::Util::int16_t)StrandGroupIDRef[GroupID];
				GroupIDs.push_back(TempInt);
			}

			Alembic::AbcGeom::OInt16GeomParam GroupIDParam(Props, "groom_group_id", false, Alembic::AbcGeom::kUniformScope, 1);
			Alembic::AbcGeom::Int16ArraySample GroupIDArray((Alembic::Util::int16_t*)GroupIDs.data(), GroupIDs.size());
			Alembic::AbcGeom::OInt16GeomParam::Sample GroupIDSample(GroupIDArray, Alembic::AbcGeom::kUniformScope);
			GroupIDParam.set(GroupIDSample);
		}

		GlobalNumVertex += NumVerttices;
		GlobalNumStrand += NumCurves;

		if (GlobalNumVertex > HairDescription.GetNumVertices() || GlobalNumStrand > HairDescription.GetNumStrands())
		{
			return false;
		}
	}

	return true;
}