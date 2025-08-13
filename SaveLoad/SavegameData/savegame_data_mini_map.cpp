
#include "savegame_data_mini_map.h"
#include "savegame_data_mini_map_parser.h"


// Game headers
#include "frontend/MiniMap.h"
#include "SaveLoad/GenericGameStorage.h"



void CMiniMapSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMiniMapSaveStructure::PreSave"))
	{
		return;
	}

	m_PointsOfInterestList.Resize(MAX_POI);
	for (u32 iCount = 0; iCount < MAX_POI; iCount++)
	{
		m_PointsOfInterestList[iCount].m_bIsSet = CMiniMap::GetPointOfInterestDetails(iCount, m_PointsOfInterestList[iCount].m_vPos);
	}
}

void CMiniMapSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMiniMapSaveStructure::PostLoad"))
	{
		return;
	}

	for (u32 iCount = 0; iCount < m_PointsOfInterestList.GetCount(); iCount++)
	{
		if (m_PointsOfInterestList[iCount].m_bIsSet)
		{
			CMiniMap::AddOrRemovePointOfInterest(m_PointsOfInterestList[iCount].m_vPos.x, m_PointsOfInterestList[iCount].m_vPos.y);
		}
	}
}

