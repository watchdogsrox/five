/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/EntityReport.cpp
// PURPOSE : generates and writes out a detailed report on a list of entities
// AUTHOR :  Ian Kiigan
// CREATED : 14/04/11 
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/EntityReport.h"

#if __BANK

#include "file/limits.h"
#include "fragment/type.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/stores/mapdatastore.h"
#include "modelInfo/PedModelInfo.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationDebug.h"
#include "peds/rendering/PedVariationStream.h"
#include "physics/gtaArchetype.h"
#include "renderer/PostScan.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/Entity.h"
#include "scene/lod/LodDebug.h"
#include "streaming/streaming.h"
#include "streaming/streamingcleanup.h"
#include "system/nelem.h"

//fw headers
#include "fwscene/stores/mapdatastore.h"

SCENE_OPTIMISATIONS();

s32 CEntityReportGenerator::ms_selectedReportSource = 0;
char CEntityReportGenerator::ms_achReportFileName[RAGE_MAX_PATH];
bool CEntityReportGenerator::ms_bWriteRequired = false;

const char* apszReportSources[CEntityReport::REPORT_SOURCE_TOTAL] =
{
	"PVS (all visible)",
};

// TODO -- use GetEntityTypeStr
const char* apszEntityTypeNames[] =
{
	"ENTITY_TYPE_NOTHING",
	"ENTITY_TYPE_BUILDING",
	"ENTITY_TYPE_ANIMATED_BUILDING",
	"ENTITY_TYPE_VEHICLE",
	"ENTITY_TYPE_PED",
	"ENTITY_TYPE_OBJECT",
	"ENTITY_TYPE_DUMMY_OBJECT",
	"ENTITY_TYPE_PORTAL",
	"ENTITY_TYPE_MLO",
	"ENTITY_TYPE_NOTINPOOLS",
	"ENTITY_TYPE_PARTICLESYSTEM",
	"ENTITY_TYPE_LIGHT",
	"ENTITY_TYPE_COMPOSITE",
	"ENTITY_TYPE_INSTANCE_LIST",
	"ENTITY_TYPE_GRASS_INSTANCE_LIST",
	"ENTITY_TYPE_VEHICLE_GLASS"
};
CompileTimeAssert(NELEM(apszEntityTypeNames) == ENTITY_TYPE_TOTAL);

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CEntityReportEntry
// PURPOSE:		ctor, populates stats and things of interest!
//////////////////////////////////////////////////////////////////////////
CEntityReportEntry::CEntityReportEntry(CEntity* pEntity) : m_pEntity(pEntity)
{
	if (pEntity)
	{
		m_modelName = pEntity->GetModelName();
		m_iplName = pEntity->GetIplIndex()==-1 ? "n/a" : INSTANCE_STORE.GetSlot(strLocalIndex(pEntity->GetIplIndex()))->m_name.GetCStr();
		m_entityType = pEntity->GetType();
		m_lodType = pEntity->GetLodData().GetLodType();
		m_lodDistance = pEntity->GetLodDistance();
		m_distFromCamera = CLodDebug::GetDistFromCamera(pEntity);
		m_vPos.Set(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
		m_bProp = pEntity->GetBaseModelInfo()->GetIsProp();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ToString
// PURPOSE:		returns the contents of report entry in a string
//////////////////////////////////////////////////////////////////////////
atString CEntityReportEntry::ToString()
{
	char achTmp[500];
	sprintf(achTmp, "%s, %s, %s, %s, %s, %d, %4.1f, <%4.1f,%4.1f,%4.1f>",
		m_modelName.c_str(),
		m_iplName.c_str(),
		apszEntityTypeNames[m_entityType],
		m_bProp?"Y":"N",
		fwLodData::ms_apszLevelNames[m_lodType],
		m_lodDistance,
		m_distFromCamera,
		m_vPos.x, m_vPos.y, m_vPos.z
	);
	return atString(achTmp);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CEntityReportEntryDetailed
// PURPOSE:		ctor, populates stats and things of interest!
//////////////////////////////////////////////////////////////////////////
CEntityReportEntryDetailed::CEntityReportEntryDetailed(CEntity* pEntity) : CEntityReportEntry(pEntity)
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if (pModelInfo)
		{
			rmcDrawable* pDrawable = pModelInfo->GetDrawable();
			if (pDrawable)
			{
				spdAABB tempBox;
				const spdAABB &aabb = pEntity->GetBoundBox(tempBox);

				m_shaderCount = pDrawable->GetShaderGroup().GetCount();
				Assertf(aabb.IsValid(), "CEntityReportEntryDetailed has invalid bbox");
				m_vAabbSize = aabb.GetMaxVector3() - aabb.GetMinVector3();
				m_memMain = (u32) CSceneIsolator::GetEntityCost(pEntity, SCENEISOLATOR_COST_VIRT) >>10;
				m_memVram = (u32) CSceneIsolator::GetEntityCost(pEntity, SCENEISOLATOR_COST_PHYS) >>10;
				m_geometryStats.Populate(pEntity);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ToString
// PURPOSE:		returns the contents of report entry in a string
//////////////////////////////////////////////////////////////////////////
atString CEntityReportEntryDetailed::ToString()
{
	atString retStr = CEntityReportEntry::ToString();
	char achTmp[500];
	sprintf(achTmp, ", %d, <%4.1f,%4.1f,%4.1f>, %d, %d, %d, %d",
		m_shaderCount,
		m_vAabbSize.x, m_vAabbSize.y, m_vAabbSize.z,
		m_memMain,
		m_memVram,
		m_geometryStats.Get(CEntityReportGeometryStats::GEOM_STAT_PRIMITIVECOUNT),
		m_geometryStats.Get(CEntityReportGeometryStats::GEOM_STAT_VERTEXCOUNT)
	);
	retStr += achTmp;
	return retStr;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Regenerate
// PURPOSE:		populates report based on specified entity list
//////////////////////////////////////////////////////////////////////////
void CEntityReport::Regenerate(atArray<CEntity*>& entityList)
{
	m_aLoadedEntities.Reset();
	m_aUnloadedEntities.Reset();

	for (u32 i=0; i<entityList.size(); i++)
	{
		CEntity* pEntity = entityList[i];
		if (pEntity)
		{
			if (pEntity->GetDrawable())
			{
				m_aLoadedEntities.PushAndGrow(CEntityReportEntryDetailed(pEntity));
			}
			else
			{
				m_aUnloadedEntities.PushAndGrow(CEntityReportEntry(pEntity));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteToFile
// PURPOSE:		writes report out to the specified file
//////////////////////////////////////////////////////////////////////////
void CEntityReport::WriteToFile(const char* pszOutputFile)
{
	Assert(strlen(pszOutputFile)>0);

	fiStream* logStream = pszOutputFile[0] ? fiStream::Create(pszOutputFile, fiDeviceLocal::GetInstance()) : NULL;
	if (!logStream)
	{
		Errorf("Could not create '%s'", pszOutputFile);
		return;
	}

	// write loaded list
	if (m_aLoadedEntities.size()>0)
	{
		fprintf(logStream, "# START ENTITY REPORT: Loaded Entities\n");
		fflush(logStream);
		fprintf(logStream, "# %s\n", CEntityReportEntryDetailed::GetDescString().c_str());
		fflush(logStream);
		for (u32 i=0; i<m_aLoadedEntities.size(); i++)
		{
			CEntityReportEntryDetailed& entry = m_aLoadedEntities[i];
			fprintf(logStream, "%s\n", entry.ToString().c_str());
			fflush(logStream);
		}
		fprintf(logStream, "# END ENTITY REPORT: Loaded Entities\n");
		fflush(logStream);
	}

	// write unloaded list
	if (m_aUnloadedEntities.size()>0)
	{
		fprintf(logStream, "\n# START ENTITY REPORT: Unloaded Entities\n");
		fflush(logStream);
		fprintf(logStream, "# %s\n", CEntityReportEntry::GetDescString().c_str());
		fflush(logStream);
		for (u32 i=0; i<m_aUnloadedEntities.size(); i++)
		{
			CEntityReportEntry& entry = m_aUnloadedEntities[i];
			fprintf(logStream, "%s\n", entry.ToString().c_str());
			fflush(logStream);
		}
		fprintf(logStream, "# END ENTITY REPORT: Unloaded Entities\n");
		fflush(logStream);
	}
	logStream->Close();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		adds widgets for report generation
//////////////////////////////////////////////////////////////////////////
void CEntityReportGenerator::InitWidgets(rage::bkBank &bank)
{
	bank.PushGroup("Entity Report");
	bank.AddCombo("Type", &ms_selectedReportSource, CEntityReport::REPORT_SOURCE_TOTAL, &apszReportSources[0]);
	bank.AddText("Output file", ms_achReportFileName, RAGE_MAX_PATH);
	bank.AddButton("Pick file", datCallback(SelectReportFileCB));
	bank.AddButton("Write now", datCallback(WriteNowCB));
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		checks if there is a write pending, and processes it if required
//////////////////////////////////////////////////////////////////////////
void CEntityReportGenerator::Update(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop)
{
	if (!ms_bWriteRequired) { return; }

	atArray<CEntity*> entityList;

	switch (ms_selectedReportSource)
	{
	case CEntityReport::REPORT_SOURCE_PVS:
		{
			// extract entity list from current PVS
			CGtaPvsEntry* pPvsCurrent = pPvsStart;
			while (pPvsCurrent != pPvsStop)
			{
				CEntity* pEntity = pPvsCurrent->GetEntity();
				if (pEntity)
				{
					entityList.PushAndGrow(pEntity);
				}
				pPvsCurrent++;
			}
		}
		break;
	default:
		break;
	}

	// create report and write it out to specified location
	if (entityList.size()>0 && strlen(ms_achReportFileName)>0)
	{
		CEntityReport* pReport = GenerateReport(entityList);
		if (pReport)
		{
			
			pReport->WriteToFile(ms_achReportFileName);
			delete pReport;
		}
	}
	ms_bWriteRequired = false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStatsForDrawable
// PURPOSE:		increments / updates stats for specified rmcDrawable
//////////////////////////////////////////////////////////////////////////
void CEntityReportGeometryStats::GetStatsForDrawable(const rmcDrawable* pDrawable)
{
	if (!pDrawable) { return; }

	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

	if(lodGroup.ContainsLod(LOD_HIGH))
	{
		const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);
		for(s32 i=0;i<lod.GetCount();i++)
		{
			const grmModel* model = lod.GetModel(i);
			for(s32 j=0;j<model->GetGeometryCount();j++)
			{
				grmGeometry& geo = model->GetGeometry(j);
				Add(GEOM_STAT_PRIMITIVECOUNT, geo.GetPrimitiveCount());
				Add(GEOM_STAT_VERTEXCOUNT, geo.GetVertexCount());
				//TODO: add more stats here
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStatsForFrag
// PURPOSE:		increments / updates stats for specified frag
//////////////////////////////////////////////////////////////////////////
void CEntityReportGeometryStats::GetStatsForFrag(CEntity *pEntity, CBaseModelInfo *pModelInfo)
{
	const fragType* pFragType = pModelInfo->GetFragType();
	if (pFragType)
	{
		fragDrawable* pDrawable = pFragType->GetCommonDrawable();
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();
		Assert(pDrawable);
		if (pFragType->GetSkinned() || !pFragInst)
		{
			GetStatsForDrawable(pDrawable);
		} 
		else 
		{
			u64 damagedBits;
			u64 destroyedBits;
			PopulateBitFields(pFragInst, damagedBits, destroyedBits);

			for (u64 child=0; child<pFragInst->GetTypePhysics()->GetNumChildren(); ++child)
			{
				fragTypeChild* pChildType = pFragInst->GetTypePhysics()->GetAllChildren()[child];
				pDrawable = NULL;
				if ((destroyedBits & ((u64)1<<child)) == 0) {
					if ((damagedBits & ((u64)1<<child)) != 0)
					{
						//broken...
						pDrawable = pChildType->GetDamagedEntity() ? pChildType->GetDamagedEntity() : pChildType->GetUndamagedEntity();
					}
					else
					{
						//not broken...
						pDrawable = pChildType->GetUndamagedEntity();
					}
				}
				GetStatsForDrawable(pDrawable);
			}
		}
	}
	else
	{
		GetStatsForDrawable(pEntity->GetDrawable());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetGeometryStat
// PURPOSE:		extracts various stats of interest for the model(s) related to a specified entity
//////////////////////////////////////////////////////////////////////////
void CEntityReportGeometryStats::Populate(CEntity* pEntity)
{
	Reset();

	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if(pEntity->GetIsTypePed())
		{
			const CPedModelInfo* pPedModelInfo = static_cast<const CPedModelInfo*>(pModelInfo);
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if (pPedModelInfo->GetIsStreamedGfx())
			{
				const CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetConstPedRenderGfx();
				if (pGfxData)
				{
					GetStatsForStreamedPed(pGfxData);
				}
			} 
			else 
			{
				GetStatsForPed(pPedModelInfo, &(pPed->GetPedDrawHandler().GetVarData()));
			}	
		}
		else
		{
			if(pModelInfo->GetFragType())
			{
				GetStatsForFrag(pEntity, pModelInfo);
			}
			else
			{
				rmcDrawable* pDrawable = pModelInfo->GetDrawable();
				if(pDrawable)
				{
					GetStatsForDrawable(pDrawable);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStatsForStreamedPed
// PURPOSE:		go through ped component slots and get stats from each model
//////////////////////////////////////////////////////////////////////////
void CEntityReportGeometryStats::GetStatsForStreamedPed(const CPedStreamRenderGfx* pGfx)
{
	const rmcDrawable* pDrawable = NULL;
	for(u32 i=0; i<PV_MAX_COMP; i++)	
	{
		pDrawable = pGfx->GetDrawableConst(i);
		if (pDrawable)
		{
			GetStatsForDrawable(pDrawable);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStatsForPed
// PURPOSE:		extract geometry stats from each component (peds with variations)
//////////////////////////////////////////////////////////////////////////
void CEntityReportGeometryStats::GetStatsForPed(const CPedModelInfo* pModelInfo, const CPedVariationData* pVarData)
{
	atArray<rmcDrawable*> aDrawables;
	if (pModelInfo->GetVarInfo())
	{
		CPedVariationDebug::GetDrawables(pModelInfo, pVarData, aDrawables);
		for (u32 i=0; i<aDrawables.size(); i++)
		{
			GetStatsForDrawable(aDrawables[i]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SelectReportFileCB
// PURPOSE:		picks location for output report file
//////////////////////////////////////////////////////////////////////////
void CEntityReportGenerator::SelectReportFileCB()
{
	if (!BANKMGR.OpenFile(ms_achReportFileName, 256, "*.txt", true, "Entity Report (*.txt)"))
	{
		ms_achReportFileName[0] = '\0';
	}
}

#endif	//__BANK