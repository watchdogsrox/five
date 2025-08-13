//
//
//    Filename: TimeModelInfo.cpp
//     Creator: Klaas Schilstra
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing a Time models that are displayed only at certain times of the day.
//
//

// Game headers
#include "modelinfo/Timemodelinfo.h"

#include "scene/loader/mapdata_buildings.h"

void CTimeModelInfo::InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool bHasAssetReferences)
{
	CBaseModelInfo::InitArchetypeFromDefinition(mapTypeDefIndex, definition, bHasAssetReferences);
	CTimeArchetypeDef& def = *smart_cast<CTimeArchetypeDef*>(definition);

	GetTimeInfo()->SetTimes(def.m_timeFlags);
}