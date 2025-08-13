/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/EntityReport.h
// PURPOSE : generates and writes out a detailed report on a list of entities
// AUTHOR :  Ian Kiigan
// CREATED : 14/04/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_ENTITYREPORT_H_
#define _SCENE_DEBUG_ENTITYREPORT_H_

#if __BANK

#include "atl/array.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "rmcore/drawable.h"
#include "vector/Vector3.h"

class CEntity;
class CGtaPvsEntry;
class CBaseModelInfo;
class CPedModelInfo;
class CPedStreamRenderGfx;
class CPedVariationData;

// some useful stats that can be extracted from an entity's drawable(s)
class CEntityReportGeometryStats
{
public:
	enum eGeomStat
	{
		GEOM_STAT_PRIMITIVECOUNT = 0,
		GEOM_STAT_VERTEXCOUNT,

		GEOM_STAT_TOTAL
	};
	void Reset() { memset(m_stats, 0, sizeof(u32)*GEOM_STAT_TOTAL); }
	u32 Get(eGeomStat eStat) const { return m_stats[eStat]; }
	void Add(eGeomStat eStat, u32 statVal) { m_stats[eStat] += statVal; }
	void Populate(CEntity* pEntity);

private:
	void GetStatsForDrawable(const rmcDrawable* pDrawable);
	void GetStatsForFrag(CEntity* pEntity, CBaseModelInfo *pModelInfo);
	void GetStatsForStreamedPed(const CPedStreamRenderGfx* pGfx);
	void GetStatsForPed(const CPedModelInfo* pModelInfo, const CPedVariationData* pVarData);

	u32 m_stats[GEOM_STAT_TOTAL];
};

// base class for a report entry - not so detailed as it caters to entities without drawables (ie not streamed in yet)
class CEntityReportEntry
{
public:
	CEntityReportEntry() {}
	CEntityReportEntry(CEntity* pEntity);
	virtual ~CEntityReportEntry() {}
	virtual atString ToString();
	static atString GetDescString() { return atString("model, ipl, type, isprop, lodtype, loddist, camdist, pos"); }

protected:
	CEntity*	m_pEntity;
	atString	m_modelName;
	atString	m_iplName;
	s32			m_entityType;
	bool		m_bProp;
	u32			m_lodType;
	u32			m_lodDistance;
	float		m_distFromCamera;
	Vector3		m_vPos;
};

// more detailed report entry for entities that have drawables (ie all dependencies / assets are streamed in)
class CEntityReportEntryDetailed : public CEntityReportEntry
{
public:
	CEntityReportEntryDetailed() {}
	CEntityReportEntryDetailed(CEntity* pEntity);
	virtual ~CEntityReportEntryDetailed() {}
	virtual atString ToString();
	static atString GetDescString() { atString retStr = CEntityReportEntry::GetDescString(); retStr += ", numshaders, aabbsize, memmain(kb), memvram(kb), numpolys, numverts"; return retStr; }

protected:
	u32			m_shaderCount;
	Vector3		m_vAabbSize;
	u32			m_memMain;
	u32			m_memVram;

	CEntityReportGeometryStats m_geometryStats;
};

// a report based on a list of entities - splits into two lists, detailed and non-detailed
class CEntityReport
{
public:
	enum eReportSource
	{
		REPORT_SOURCE_PVS = 0,

		REPORT_SOURCE_TOTAL
	};
	CEntityReport(atArray<CEntity*>& entityList) { Regenerate(entityList); }
	void Regenerate(atArray<CEntity*>& entityList);
	void WriteToFile(const char* pszOutputFile);

private:
	atArray<CEntityReportEntryDetailed>		m_aLoadedEntities;
	atArray<CEntityReportEntry>				m_aUnloadedEntities;
};

// main interface for generating entity reports
class CEntityReportGenerator
{
public:
	static void	InitWidgets(bkBank& bank);
	static void Update(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop);
	static CEntityReport* GenerateReport(atArray<CEntity*>& entityList) { return rage_new CEntityReport(entityList); }

private:
	static void SelectReportFileCB();
	static void WriteNowCB() { ms_bWriteRequired = true; }

	static s32 ms_selectedReportSource;
	static char ms_achReportFileName[];
	static bool ms_bWriteRequired;
};

#endif	//__BANK

#endif	//_SCENE_DEBUG_ENTITYREPORT_H_