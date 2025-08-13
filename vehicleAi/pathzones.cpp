#include "pathzones.h"
#include "parser/manager.h"
#include "scene/DataFileMgr.h"
#include "core/game.h"

// Parser files
#include "PathZones_parser.h"

// CPathZoneManager::CPathZoneManager()
// {
// 
// }

void CPathZoneManager::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		ParsePathZoneFile();
	}
}

CPathZoneData	CPathZoneManager::m_PathZoneData;

void CPathZoneManager::Shutdown(unsigned /*shutdownMode*/)
{

}

void CPathZoneManager::ParsePathZoneFile()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PATH_ZONES_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		parSettings settings = parSettings::sm_StandardSettings;
		//settings.SetFlag(parSettings::PRELOAD_FILE, false);

		ASSERT_ONLY(bool bLoadedOk = )PARSER.LoadObject(pData->m_filename, "xml", m_PathZoneData, &settings);
		Assertf(bLoadedOk, "Error loading pathzones.xml");
	}
}

s32 CPathZoneManager::GetZoneForPoint(const Vector2& pos)
{
	return m_PathZoneData.m_PathZones.GetZoneForPoint(pos);
}

const CPathZone& CPathZoneManager::GetPathZone(s32 iKey)
{
	return m_PathZoneData.m_PathZones.GetZone(iKey);
}

const CPathZoneMapping& CPathZoneManager::GetPathZoneMapping(s32 iSrcKey, s32 iDestKey)
{
	return m_PathZoneData.m_PathZoneMappings.FindMappingBetweenZones(iSrcKey, iDestKey);
}

bool CPathZone::IsPointWithinZone(const Vector2& pos) const
{
	return pos.x >= vMin.x && pos.x <= vMax.x
		&& pos.y >= vMin.y && pos.y <= vMax.y;
}

s32 CPathZoneArray::GetZoneForPoint(const Vector2& pos) const
{
	for (int i = 0; i < m_Entries.GetCount(); i++)
	{
		if (m_Entries[i].IsPointWithinZone(pos))
		{
			return m_Entries[i].ZoneKey;
		}
	}

	return -1;
}

const CPathZoneMapping& CPathZoneMappingArray::FindMappingBetweenZones(const s32 iSrcKey, const s32 iDestKey) const
{
	for (int i = 0; i < m_Entries.GetCount(); i++)
	{
		if (m_Entries[i].SrcKey == iSrcKey && m_Entries[i].DestKey == iDestKey)
		{
			return m_Entries[i];
		}
	}

	Assertf(0, "Cannot find CPathZoneMapping with keys %d / %d, returning junk data", iSrcKey, iDestKey);
	return m_Entries[0];
}
