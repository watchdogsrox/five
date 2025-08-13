/////////////////////////////////////////////////////////////////////////////////
// Title	:	PopZones.cpp
// Author	:	Graeme, Adam Croston
// Started	:	19.06.00
// Purpose	:	To keep track of population areas on the map.
/////////////////////////////////////////////////////////////////////////////////
#include "peds/popzones.h"

// C headers
#include <stdio.h>
#include <string.h>

// Framework includes
#include "fwscene/world/WorldLimits.h"

// Rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"

// Game headers
#include "ai/stats.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "debug/vectormap.h"
#include "game/clock.h"
#include "game/dispatch/DispatchData.h"
#include "peds/popcycle.h"
#include "peds/playerinfo.h"
#include "peds/ped.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "system/filemgr.h"
#include "vfx/metadata/VfxRegionInfo.h"
#include "vehicles/vehiclepopulation.h"
#include "text/textconversion.h"

using namespace AIStats;

AI_OPTIMISATIONS()

PARAM(check_pop_zones, "[PopZones] check popzones for overlap");

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZone
// Purpose		:	TODO
/////////////////////////////////////////////////////////////////////////////////
atArray<CPopZone>	CPopZones::sm_popZones;

// Cache for FindSmallestForPosition
CPopZones::SmallestPopZoneCacheEntry CPopZones::sm_smallestPopZoneCache[CACHED_SMALLEST_POP_ZONE_RESULTS];
u8 CPopZones::sm_smallestPopZoneCacheIndex = 0;

#if __BANK

bool				CPopZones::bPrintZoneInfo = false;
bool				CPopZones::bDisplayZonesOnVMap = false;
bool				CPopZones::bDisplayZonesInWorld = false;
bool				CPopZones::bDisplayOnlySlowLawResponseZones = false;
bool				CPopZones::bDisplayNonSlowLawResponseZones = false;

#endif // __BANK


CPopZone::CPopZone()
{
	m_id.Clear();
	m_associatedTextId.Clear();

	m_uberZoneCentreX = m_uberZoneCentreY = 0;

	m_bNoCops = false;
	m_MPGangTerritoryIndex = -1;
	m_bBigZoneForScanner = false;

	m_vehDirtMin = -1.f;
	m_vehDirtMax = -1.f;
	m_pedDirtMin = -1.f;
	m_pedDirtMax = -1.f;
	m_dirtGrowScale = 1.0f;
	m_dirtCol.Set(0);

	m_vfxRegionHashName = VFXREGIONINFO_DEFAULT_HASHNAME;
	m_plantsMgrTxdIdx = 0x07;
	m_scumminess = SCUMMINESS_ABOVE_AVERAGE;
	m_lawResponseDelayType = LAW_RESPONSE_DELAY_MEDIUM;
	m_vehicleResponseType = VEHICLE_RESPONSE_DEFAULT;

	m_fPopBaseRangeScale = 1.0f;
	m_fPopHeightScaleMultiplier = 1.0f;	
}
/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZoneSearchInfo
// Purpose		:	TODO
/////////////////////////////////////////////////////////////////////////////////
atArray<CPopZoneSearchInfo> CPopZones::sm_popZoneSearchInfos;

CPopZoneSearchInfo::CPopZoneSearchInfo()
{
	 m_ZoneCategory = (ZONECAT_NAVIGATION); 
	 m_bEnabled = true; // enabled by default
	 m_popScheduleIndexForSP = 0;
	 m_popScheduleIndexForMP = 0;
	 m_x1 = 0;
	 m_y1 = 0;
	 m_z1 = 0;
	 m_x2 = 0;
	 m_y2 = 0;
	 m_z2 = 0;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetTranslatedName
// Purpose		:	Returns the text to be displayed when entering this Zone
//					Only Navigation and Local Navigation Zones should have associated text
// Parameters	:	none
// Returns		:	Pointer to the text to display
/////////////////////////////////////////////////////////////////////////////////
char *CPopZone::GetTranslatedName() const
{
	return TheText.Get(m_associatedTextId.TryGetCStr());
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	InitLevelWithMapUnloaded
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::Init(unsigned initMode)
{
	if (initMode == INIT_BEFORE_MAP_LOADED)
	{
		ResetPopZones();

		// Make the first Name Zone and the first Info Zone a default zone which covers the entire level
		CPopZone popZone;
		{
            popZone.m_id.SetFromString("DEFAULT");
            popZone.m_associatedTextId.SetFromString("SanAnd");

			popZone.m_bNoCops = false;
			popZone.m_MPGangTerritoryIndex = -1;
			popZone.m_lawResponseDelayType = LAW_RESPONSE_DELAY_MEDIUM;
			popZone.m_vehicleResponseType = VEHICLE_RESPONSE_DEFAULT;
			popZone.m_specialAttributeType = SPECIAL_NONE;
		#if RSG_PC || RSG_DURANGO || RSG_ORBIS
			popZone.m_plantsMgrTxdIdx = 0x07;
		#else
			popZone.m_plantsMgrTxdIdx = 0x02;	// force txd=2 (underwater) for DEFAULT
		#endif
		}
		sm_popZones.PushAndGrow(popZone);
		CPopZoneSearchInfo popZoneSearchInfo;
		{
			popZoneSearchInfo.m_x1 = (s16)WORLDLIMITS_XMIN;
			popZoneSearchInfo.m_y1 = (s16)WORLDLIMITS_YMIN;
			popZoneSearchInfo.m_z1 = (s16)WORLDLIMITS_ZMIN;

			popZoneSearchInfo.m_x2 = (s16)WORLDLIMITS_XMAX;
			popZoneSearchInfo.m_y2 = (s16)WORLDLIMITS_YMAX;
			popZoneSearchInfo.m_z2 = (s16)WORLDLIMITS_ZMAX;

			popZoneSearchInfo.m_ZoneCategory = ZONECAT_NAVIGATION;

			popZoneSearchInfo.m_popScheduleIndexForSP = 0;
			popZoneSearchInfo.m_popScheduleIndexForMP = 0;
		}
		sm_popZoneSearchInfos.PushAndGrow(popZoneSearchInfo);

		// init cache values
		sm_smallestPopZoneCacheIndex = 0;
		sysMemSet(sm_smallestPopZoneCache, 0, sizeof(CPopZones::SmallestPopZoneCacheEntry)*CACHED_SMALLEST_POP_ZONE_RESULTS);			
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		PostCreation();
	}
}	

/////////////////////////////////////////////////////////////////////////////////
// Name			:	Update
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::Update(void)
{
#if __BANK

	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	CPopZone * pDebugZone = FindSmallestForPosition(&vOrigin, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);

	if(bPrintZoneInfo || bDisplayZonesOnVMap || bDisplayZonesInWorld)
	{
		if(pDebugZone)
		{
			Vector2 vPixelSize( 1.0f / ((float)grcDevice::GetWidth()), 1.0f / ((float)grcDevice::GetHeight()) );	
			Vector2 vTextPos( vPixelSize.x * 56.0f, 1.0f );
			vTextPos.y -= (vPixelSize.y * ((float)grcDebugDraw::GetScreenSpaceTextHeight())) * 13.0f;

			char debugText[256];

			formatf(debugText, "ZONE : \"%s\"", pDebugZone->GetTranslatedName());
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "ID : %s, TextID : %s", pDebugZone->m_id.TryGetCStr(), pDebugZone->m_associatedTextId.TryGetCStr());
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			const char * pZoneCat;
			switch(GetPopZoneZoneCategory(pDebugZone))
			{
				case ZONECAT_NAVIGATION : pZoneCat = "ZONECAT_NAVIGATION"; break;
				case ZONECAT_LOCAL_NAVIGATION : pZoneCat = "ZONECAT_LOCAL_NAVIGATION"; break;
				case ZONECAT_INFORMATION : pZoneCat = "ZONECAT_INFORMATION"; break;
				case ZONECAT_MAP : pZoneCat = "ZONECAT_MAP"; break;
				case ZONECAT_ANY: default: pZoneCat = "ZONECAT_ANY"; break;
			}

			formatf(debugText, "Category : %s", pZoneCat);
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "Pop Scaling (StartZ: %.1f, EndZ: %.1f, Multiplier: %.1f)", (float)pDebugZone->m_iPopHeightScaleLowZ, (float)pDebugZone->m_iPopHeightScaleHighZ, pDebugZone->m_fPopHeightScaleMultiplier);
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "Pop Schedule (SP: %i, MP: %i)  Gang Territory: %i", GetPopZonePopScheduleIndexForSP(pDebugZone), GetPopZonePopScheduleIndexForMP(pDebugZone),  pDebugZone->m_MPGangTerritoryIndex);
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "Response Speed (Law: %i, Vehicle: %i)", pDebugZone->m_lawResponseDelayType, pDebugZone->m_vehicleResponseType);
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "BigZone: %s NoCops: %s Enabled: %s Special: %s", 
				pDebugZone->m_bBigZoneForScanner ? "true" : "false",
				pDebugZone->m_bNoCops ? "true" : "false",
				GetPopZoneEnabled(pDebugZone) ? "true" : "false",
				pDebugZone->m_specialAttributeType ? "true" : "false");
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "AllowScenarioWeatherExits: %s", pDebugZone->m_bAllowScenarioWeatherExits ? "true" : "false");
			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			formatf(debugText, "Let hurry away leave pavements %s", pDebugZone->m_bLetHurryAwayLeavePavements ? "true" : "false");
			grcDebugDraw::Text(vTextPos, Color_NavyBlue, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		}
	}
	if(bDisplayZonesOnVMap)
	{
		CPopZone* pZone = FindSmallestForPosition(&vOrigin, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);

		const int popZoneCount = sm_popZones.GetCount();
		for(int popZoneIndex = 0; popZoneIndex < popZoneCount; ++popZoneIndex)
		{
			Vector3 vMin(	static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_x1),
							static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_y1),
							static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_z1));
			Vector3 vMax(	static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_x2),
							static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_y2),
							static_cast<float>(sm_popZoneSearchInfos[popZoneIndex].m_z2));

			if(&sm_popZones[popZoneIndex] == pZone)
			{
				static bool flash = true;
				flash = !flash;
				if(flash)
				{
					CVectorMap::DrawRectAxisAligned(vMin, vMax, Color32(0,0,0,128), true);
				}
			}
			else
			{
				CVectorMap::DrawRectAxisAligned(vMin, vMax, Color32(32,32,32,64), true);
			}
		}
	}
	if(bDisplayZonesInWorld)
	{
		camDebugDirector & debugDirector = camInterface::GetDebugDirector();
		Vec3V vOrigin = VECTOR3_TO_VEC3V(debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos);

		int iHeight = grcDebugDraw::GetScreenSpaceTextHeight();
		char txt[256];

		const Vector3 vFwd = debugDirector.GetFreeCamFrame().GetFront();
		const float fPlaneD = - DotProduct(vFwd, VEC3V_TO_VECTOR3(vOrigin));

		const float fMaxDist = 1024.0f;

		for(int z=0; z<sm_popZones.GetCount(); z++)
		{
			CPopZone * pZone = &sm_popZones[z];

			if(pZone->m_id.GetHash() != 0 && (!bDisplayOnlySlowLawResponseZones || pZone->m_lawResponseDelayType == LAW_RESPONSE_DELAY_SLOW) && 
				(!bDisplayNonSlowLawResponseZones || pZone->m_lawResponseDelayType != LAW_RESPONSE_DELAY_SLOW))
			{
				Vec3V vZoneMin((float)GetPopZoneX1(pZone), (float)GetPopZoneY1(pZone), (float)GetPopZoneZ1(pZone));
				Vec3V vZoneMax((float)GetPopZoneX2(pZone), (float)GetPopZoneY2(pZone), (float)GetPopZoneZ2(pZone));

				Color32 iCol;
				u32 iRandVal = static_cast<u32>(reinterpret_cast<size_t>(pZone));
				iRandVal ^= pZone->m_id.GetHash() * pZone->m_id.GetHash();
				iCol.Set(iRandVal);
				Vec3V vCol(iCol.GetRedf(), iCol.GetGreenf(), iCol.GetBluef());
				vCol = Normalize(vCol);
				iCol = Color32(vCol.GetXf(), vCol.GetYf(), vCol.GetZf(), 1.0f);

				//**************************
				// Draw the zone's min/max

				Vec3V vZoneMid = (vZoneMin+vZoneMax)*ScalarV(V_HALF);
				const float fDist = Mag(vOrigin - vZoneMid).Getf();
				const float fAlpha = 1.0f - Clamp(fDist / fMaxDist, 0.0f, 1.0f);
				iCol.SetAlpha((int)(fAlpha*255.0f));

				grcDebugDraw::BoxAxisAligned(vZoneMin, vZoneMax, iCol, false);		

			
				if(fDist < fMaxDist && DotProduct(vFwd, VEC3V_TO_VECTOR3(vZoneMid))+fPlaneD > 0.0f )
				{
					//float fAlpha = 1.0f - (fDist / fMaxTextDist);
					//iCol.SetAlpha((int)(fAlpha*255.0f));

					int iY = 0;

					// Zone Name
					formatf(txt, "Name : %s", pZone->m_associatedTextId.TryGetCStr());
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// Zone ID
					formatf(txt, "ID : %s", pZone->m_id.TryGetCStr());
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// Pop Schedule Index
					formatf(txt, "PopScheduleForSP : %i", GetPopZonePopScheduleIndexForSP(pZone));
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

                    // Pop Schedule Index
					formatf(txt, "PopScheduleForMP : %i", GetPopZonePopScheduleIndexForMP(pZone));
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// bNoCops
					formatf(txt, "bNoCops : %s", pZone->m_bNoCops ? "true" : "false");
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// MP Gang Territory Index
					formatf(txt, "MP Gang Territory : %d", pZone->m_MPGangTerritoryIndex);
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// law response speed
					formatf(txt, "Law response speed : %d", pZone->m_lawResponseDelayType);
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// vehicle response type
					formatf(txt, "Vehicle response type : %d", pZone->m_vehicleResponseType);
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// m_bBigZoneForScanner
					formatf(txt, "bBigZone : %s", pZone->m_bBigZoneForScanner ? "true" : "false");
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// m_bAirport
					formatf(txt, "Special attribute type : %d", pZone->m_specialAttributeType);
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// m_bAllowScenarioWeatherExits
					formatf(txt, "Allow Scenario Weather Exits: %s", pZone->m_bAllowScenarioWeatherExits ? "true" : "false");
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;

					// m_bLetHurryAwayLeavePavements
					formatf(txt, "Care about pavement for shocking events:  %s", pZone->m_bLetHurryAwayLeavePavements ? "true" : "false");
					grcDebugDraw::Text(vZoneMid, iCol, 0, iY, txt);
					iY += iHeight;
				}
			}
		}

		// Flash the debug zone on/off every few frames
		if(pDebugZone && (fwTimer::GetFrameCount() & 4))
		{
			Vec3V vZoneMin((float)GetPopZoneX1(pDebugZone), (float)GetPopZoneY1(pDebugZone), (float)GetPopZoneZ1(pDebugZone));
			Vec3V vZoneMax((float)GetPopZoneX2(pDebugZone), (float)GetPopZoneY2(pDebugZone), (float)GetPopZoneZ2(pDebugZone));
			// Vec3V vZoneMid = (vZoneMin+vZoneMax)*ScalarV(V_HALF);
			grcDebugDraw::BoxAxisAligned(vZoneMin, vZoneMax, Color_red, false);		
		}
	}
#endif // __BANK
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CreatePopZone
// Purpose		:	Sets up the name, position and type of a new zone. 
//					This function will be called when loading the .ipl file
//					Default ratios/densities are set up for appropriate zones.
//					These can be changed in the scripts later.
// Parameters	:	TODO
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::CreatePopZone(	const char* pZoneIdName,
								const Vector3 &boxMin_, const Vector3 &boxMax_,
								const char* pAssociatedTextIdName,
								eZoneCategory ZoneCategory)
{
	// Make sure the min and max values for the box is in the right order.
	float temp_float;
	Vector3 boxMin = boxMin_, boxMax = boxMax_;
	if(boxMin.x > boxMax.x)
	{
		temp_float = boxMin.x;
		boxMin.x = boxMax.x;
		boxMax.x = temp_float;
	}
	if(boxMin.y > boxMax.y)
	{
		temp_float = boxMin.y;
		boxMin.y = boxMax.y;
		boxMax.y = temp_float;
	}
	if(boxMin.z > boxMax.z)
	{
		temp_float = boxMin.z;
		boxMin.z = boxMax.z;
		boxMax.z = temp_float;
	}

	char idBuff[8];
	strncpy(idBuff, pZoneIdName, 7);
	idBuff[7] = 0;
	int len = istrlen(idBuff);
	for(int i = len; i< 7; ++i){idBuff[i] = 0;}
	CTextConversion::MakeUpperCaseAsciiOnly(idBuff);

	char associatedTextIdBuff[8];
	strncpy(associatedTextIdBuff, pAssociatedTextIdName, 7);
	associatedTextIdBuff[7] = 0;
	len = istrlen(associatedTextIdBuff);
	for(int i = len; i< 7; ++i){associatedTextIdBuff[i] = 0;}
	CTextConversion::MakeUpperCaseAsciiOnly(associatedTextIdBuff);

	CPopZone popZone;
	{
		popZone.m_id.SetFromString(idBuff);
		popZone.m_associatedTextId.SetFromString(associatedTextIdBuff);

		popZone.m_bNoCops = false;
		popZone.m_MPGangTerritoryIndex = -1;

		popZone.m_vehDirtMin = -1.f;
		popZone.m_vehDirtMax = -1.f;
		popZone.m_pedDirtMin = -1.f;
		popZone.m_pedDirtMax = -1.f;
		popZone.m_dirtGrowScale = 1.0f;
		popZone.m_dirtCol.Set(0);

		popZone.m_vfxRegionHashName = VFXREGIONINFO_DEFAULT_HASHNAME;
		popZone.m_plantsMgrTxdIdx = 0x07;
	}
	sm_popZones.PushAndGrow(popZone);
	CPopZoneSearchInfo popZoneSearchInfo;
	{
		popZoneSearchInfo.m_x1 = (s16)boxMin.x;
		popZoneSearchInfo.m_y1 = (s16)boxMin.y;
		popZoneSearchInfo.m_z1 = (s16)boxMin.z;

		popZoneSearchInfo.m_x2 = (s16)boxMax.x;
		popZoneSearchInfo.m_y2 = (s16)boxMax.y;
		popZoneSearchInfo.m_z2 = (s16)boxMax.z;

		popZoneSearchInfo.m_ZoneCategory = ZoneCategory;

		popZoneSearchInfo.m_popScheduleIndexForSP = 0;
		popZoneSearchInfo.m_popScheduleIndexForMP = 0;
	}
	sm_popZoneSearchInfos.PushAndGrow(popZoneSearchInfo);
}


#if __ASSERT
/////////////////////////////////////////////////////////////////////////////////
// Name			:	CheckForOverlap
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::CheckForOverlap(void)
{
	const int popZoneCount = sm_popZones.GetCount();
	for(int i = 1; i < popZoneCount; ++i)
	{	// Check that each navigation zone is entirely contained within the first navigation zone
		Assertf(IsEntirelyInsideAnother(&sm_popZones[i], &sm_popZones[0]), "Zone %s is outside the world zone\n", sm_popZones[i].m_id.TryGetCStr());
		
		for(int j = (i+1); j < popZoneCount; ++j)
		{	// Check that each zone is entirely inside or entirely outside every other zone
			if ( (IsEntirelyInsideAnother(&sm_popZones[i], &sm_popZones[j]))
				|| (IsEntirelyInsideAnother(&sm_popZones[j], &sm_popZones[i])) )
			{
			}
			else
			{
				if (Overlaps(&sm_popZones[i], &sm_popZones[j]))
				{
					Assertf(0, "CPopZones::CheckForOverlap - pop zone %s with Min <<%d,%d,%d>> and Max <<%d,%d,%d>> overlaps with pop zone %s with Min <<%d,%d,%d>> and Max <<%d,%d,%d>>",
						sm_popZones[i].m_id.TryGetCStr(), 
						sm_popZoneSearchInfos[i].m_x1, sm_popZoneSearchInfos[i].m_y1, sm_popZoneSearchInfos[i].m_z1, 
						sm_popZoneSearchInfos[i].m_x2, sm_popZoneSearchInfos[i].m_y2, sm_popZoneSearchInfos[i].m_z2, 
						sm_popZones[j].m_id.TryGetCStr(), 
						sm_popZoneSearchInfos[j].m_x1, sm_popZoneSearchInfos[j].m_y1, sm_popZoneSearchInfos[j].m_z1, 
						sm_popZoneSearchInfos[j].m_x2, sm_popZoneSearchInfos[j].m_y2, sm_popZoneSearchInfos[j].m_z2);
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	IsPopZoneEntirelyInsideAnother
// Purpose		:	checks whether one Zone completely contains another
// Parameters	:	pointers to two Zones(the one which is expected to be smaller is first)
// Returns		:	TRUE if the first Zone is inside the second Zone
/////////////////////////////////////////////////////////////////////////////////
bool CPopZones::IsEntirelyInsideAnother(const CPopZone* pSmallZone, const CPopZone* pBigZone)
{
	const CPopZoneSearchInfo* pSmallZoneInfo = GetSearchInfoFromPopZone(pSmallZone);
	const CPopZoneSearchInfo* pBigZoneInfo = GetSearchInfoFromPopZone(pBigZone);

	if(	(pSmallZoneInfo->m_x1 >= pBigZoneInfo->m_x1) && (pSmallZoneInfo->m_x2 <= pBigZoneInfo->m_x2) &&
		(pSmallZoneInfo->m_y1 >= pBigZoneInfo->m_y1) && (pSmallZoneInfo->m_y2 <= pBigZoneInfo->m_y2) &&
		(pSmallZoneInfo->m_z1 >= pBigZoneInfo->m_z1) && (pSmallZoneInfo->m_z2 <= pBigZoneInfo->m_z2))
	{
		return TRUE;
	}
	else
	{
		//Vector3 TempPoint;
		
		//TempPoint = Vector3(pSmallZone->m_x1, pSmallZone->m_y1, pSmallZone->m_z1);
	
		//Assertf((TempPoint.x > pBigZone->m_x1) && (TempPoint.x < pBigZone->m_x2) &&
		//		(TempPoint.y > pBigZone->m_y1) && (TempPoint.y < pBigZone->m_y2) &&
		//		(TempPoint.z > pBigZone->m_z1) && (TempPoint.z < pBigZone->m_z2), "Overlapping zones %s and %s\n",(char*)&(pSmallZone->m_id),(char*)&(pBigZone->m_id));

		//TempPoint = Vector3(pSmallZone->m_x2, pSmallZone->m_y2, pSmallZone->m_z2);
		
		//Assertf((TempPoint.x > pBigZone->m_x1) && (TempPoint.x < pBigZone->m_x2) &&
		//		(TempPoint.y > pBigZone->m_y1) && (TempPoint.y < pBigZone->m_y2) &&
		//		(TempPoint.z > pBigZone->m_z1) && (TempPoint.z < pBigZone->m_z2), "Overlapping zones %s and %s\n",(char*)&(pSmallZone->m_id),(char*)&(pBigZone->m_id));
	
		return FALSE;
	}
}

bool CPopZones::DoesOverlapInOneDimension(s32 FirstMin, s32 FirstMax, s32 SecondMin, s32 SecondMax)
{
	s32 LeftValue = Max(FirstMin, SecondMin);
	s32 RightValue = Min(FirstMax, SecondMax);

	s32 Difference = RightValue - LeftValue;

	if (Difference > 0)
	{
		return true;
	}

	return false;
}

bool CPopZones::Overlaps(const CPopZone* pFirstZone, const CPopZone* pSecondZone)
{
	const CPopZoneSearchInfo* pFirstZoneInfo = GetSearchInfoFromPopZone(pFirstZone);
	const CPopZoneSearchInfo* pSecondZoneInfo = GetSearchInfoFromPopZone(pSecondZone);

	//	Assume that we've already dealt with the case of one zone being entirely inside the other
	if (DoesOverlapInOneDimension(pFirstZoneInfo->m_x1, pFirstZoneInfo->m_x2, pSecondZoneInfo->m_x1, pSecondZoneInfo->m_x2))
	{	//	Overlap in X
		if (DoesOverlapInOneDimension(pFirstZoneInfo->m_y1, pFirstZoneInfo->m_y2, pSecondZoneInfo->m_y1, pSecondZoneInfo->m_y2))
		{	//	Overlap in Y
			if (DoesOverlapInOneDimension(pFirstZoneInfo->m_z1, pFirstZoneInfo->m_z2, pSecondZoneInfo->m_z1, pSecondZoneInfo->m_z2))
			{	//	Overlap in Z
				return true;
			}
		}
	}

	return false;
}
#endif // __ASSERT


/////////////////////////////////////////////////////////////////////////////////
// Name			:	ClearPopZones
// Purpose		:	To clean the sm_popZones array out.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::ResetPopZones(void)
{
	sm_popZones.Reset();
	sm_popZoneSearchInfos.Reset();
	// reset cache values
	sm_smallestPopZoneCacheIndex = 0;
	sysMemSet(sm_smallestPopZoneCache, 0, sizeof(CPopZones::SmallestPopZoneCacheEntry)*CACHED_SMALLEST_POP_ZONE_RESULTS);			
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	PostCreation
// Purpose		:	This function only affects navigation(and local navigation) zones
//					This function should be called after all the zones
//					have been loaded from the .ipl file and Created.
//					The navigation zones are arranged so that it is easy to find which zones 
//					are inside which others.(This rearrangement assumes that no zone 
//					overlaps another.)
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::PostCreation(void)
{
#if __ASSERT
	if (PARAM_check_pop_zones.Get())
	{
		CheckForOverlap();
	}
#endif // __ASSERT

	// compute mid points for any zones with the same name for use with the police scanner
	const int popZoneCount = sm_popZones.GetCount();
	for(int i = 0; i < popZoneCount; ++i)
	{
		u32 textHash = sm_popZones[i].m_associatedTextId.GetHash();
		s64 sumX = 0, sumY=0;
		u32 numZones = 0;
		u32 zoneSize = 0;
		for(int j = 0; j < popZoneCount; ++j)
		{
			if(sm_popZones[j].m_associatedTextId.GetHash() == textHash)
			{
				sumX +=(sm_popZoneSearchInfos[j].m_x1 + sm_popZoneSearchInfos[j].m_x2) / 2;
				sumY +=(sm_popZoneSearchInfos[j].m_y1 + sm_popZoneSearchInfos[j].m_y2) / 2;

				zoneSize +=(u32)(Vector3((f32)sm_popZoneSearchInfos[j].m_x1,(f32)sm_popZoneSearchInfos[j].m_y1,(f32)sm_popZoneSearchInfos[i].m_z1) - Vector3((f32)sm_popZoneSearchInfos[j].m_x2,(f32)sm_popZoneSearchInfos[j].m_y2,(f32)sm_popZoneSearchInfos[i].m_z2)).Mag();
				numZones++;
			}
		}

		sm_popZones[i].m_uberZoneCentreX = static_cast<s16>(sumX / numZones);
		sm_popZones[i].m_uberZoneCentreY = static_cast<s16>(sumY / numZones);
		sm_popZones[i].m_bBigZoneForScanner =(zoneSize > 200);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	DoesPointLiesWithinPopZone
// Purpose		:	checks whether a given point is inside a given Zone
// Parameters	:	pointer to the point, pointer to the Zone
// Returns		:	TRUE if the point is inside the Zone
/////////////////////////////////////////////////////////////////////////////////
bool CPopZones::DoesPointLiesWithinPopZone(const Vector3* pPoint, const CPopZone* pZone)
{
	const CPopZoneSearchInfo* pZoneInfo = GetSearchInfoFromPopZone(pZone);
	if(	(pPoint->x >= pZoneInfo->m_x1) && (pPoint->x <= pZoneInfo->m_x2) &&
		(pPoint->y >= pZoneInfo->m_y1) && (pPoint->y <= pZoneInfo->m_y2) &&
		(pPoint->z >= pZoneInfo->m_z1) && (pPoint->z <= pZoneInfo->m_z2))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	DoesPointLiesWithinPopZoneFast
// Purpose		:	checks whether a given point is inside a given Zone
// Parameters	:	X/Y/Z coordinates of the point as integers, pointer to the Zone
// Returns		:	TRUE if the point is inside the Zone
/////////////////////////////////////////////////////////////////////////////////

bool CPopZones::DoesPointLieWithinPopZoneFast(int pPoint_X, int pPoint_Y, int pPoint_Z, const CPopZoneSearchInfo* pZone)
{
	if(	(pPoint_X >= pZone->m_x1) && (pPoint_X <= pZone->m_x2) &&
		(pPoint_Y >= pZone->m_y1) && (pPoint_Y <= pZone->m_y2) &&
		(pPoint_Z >= pZone->m_z1) && (pPoint_Z <= pZone->m_z2))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

int CPopZones::SmallestDistanceToPopZoneEdge(int pPoint_X, int pPoint_Y, int pPoint_Z, const CPopZoneSearchInfo* pZone)
{
	int dif = pPoint_X - pZone->m_x1;
	dif = MIN(dif, pPoint_Y - pZone->m_y1);
	dif = MIN(dif, pPoint_Z - pZone->m_z1);
	dif = MIN(dif, pZone->m_x2 - pPoint_X);
	dif = MIN(dif, pZone->m_y2 - pPoint_Y);
	dif = MIN(dif, pZone->m_z2 - pPoint_Z);

	return dif;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	IsPointInPopZonesWithText
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
bool CPopZones::IsPointInPopZonesWithText(const Vector3* pPoint, const char* pZoneLabel)
{
    atHashString zoneHash(pZoneLabel);
	const int popZoneCount = sm_popZones.GetCount();
	for(int popZoneIndex = 0; popZoneIndex < popZoneCount; ++popZoneIndex)
	{
		if(sm_popZoneSearchInfos[popZoneIndex].m_bEnabled)
		{
			if(sm_popZones[popZoneIndex].m_associatedTextId.GetHash() == zoneHash.GetHash())
			{
				if(DoesPointLiesWithinPopZone(pPoint, &sm_popZones[popZoneIndex]))
				{
					return true;
				}
			}
		}
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetZone
// Purpose		:	Finds the first Zone(of the given type) with the given id.
// Parameters	:	zoneIdName to search for, Type of zone to search for
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
CPopZone* CPopZones::GetPopZone(const char* pZoneIdName)
{
	Assert(pZoneIdName);
	atHashString pz(pZoneIdName);

	const int popZoneCount = sm_popZones.GetCount();
	for(int popZoneIndex = 0; popZoneIndex < popZoneCount; ++popZoneIndex)
	{
		if(sm_popZones[popZoneIndex].m_id.GetHash() == pz.GetHash())
		{
			return &sm_popZones[popZoneIndex];
		}
	}
	return NULL;
}

int CPopZones::GetPopZoneId(const CPopZone* pPopZone)
{
	if(pPopZone)
		return ptrdiff_t_to_int(pPopZone - &sm_popZones[0]);
	return INVALID_ZONEID;
}

CPopZone* CPopZones::GetPopZone(int zoneId)
{
	if(zoneId != INVALID_ZONEID)
		return &sm_popZones[zoneId];
	return NULL;
}

bool CPopZones::IsValidForSearchType(const CPopZoneSearchInfo *zoneInfo, eZoneCategory zoneCategory, eZoneType zoneType)
{
    bool isValid = false;

	if(zoneInfo && zoneInfo->m_bEnabled)
	{
		if((zoneCategory == zoneInfo->m_ZoneCategory) || (zoneCategory == ZONECAT_ANY))
		{
			if((zoneType == ZONETYPE_ANY) ||
			   (zoneType == ZONETYPE_SP && zoneInfo->m_popScheduleIndexForSP != -1) ||
			   (zoneType == ZONETYPE_MP && zoneInfo->m_popScheduleIndexForMP != -1))
			{
				isValid = true;
			}
		}
	}
    return isValid;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetSearchInfoFromPopZone
// Purpose		:	Return a CPopZoneSearchInfo for the given Pop Zone
// Parameters	:	a pointer to the pop zone - it better not be Null
// Returns		:	The matching CPopZoneSearchInfo
/////////////////////////////////////////////////////////////////////////////////
const CPopZoneSearchInfo* CPopZones::GetSearchInfoFromPopZone(const CPopZone* pPopZone)
{
	FastAssert(pPopZone);
	int index = ptrdiff_t_to_int(pPopZone - sm_popZones.GetElements());

	Assert(index >= 0 && index < sm_popZones.GetCount());
	return &sm_popZoneSearchInfos[index];	
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	InitWidgets
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

void GetNameOfZoneAtDebugCam()
{
	Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	CPopZone* pZone = CPopZones::FindSmallestForPosition(&vOrigin, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);

	if(pZone)
	{
        Displayf("ZONE IS \"%s\"", pZone->m_associatedTextId.TryGetCStr());
	}
	else
	{
		Displayf("ZONE IS \"\"");
	}
}

void CPopZones::InitWidgets(bkBank& BANK_ONLY(bank))
{
#if __BANK
	bank.PushGroup("Pop Zones", false);
	bank.AddToggle("Print zone info", &bPrintZoneInfo);
	bank.AddToggle("Display pop zones (on VM)", &bDisplayZonesOnVMap);
	bank.AddToggle("Display pop zones (in World)", &bDisplayZonesInWorld);
	bank.AddToggle("Only display SLOW law response zones (in World)", &bDisplayOnlySlowLawResponseZones);
	bank.AddToggle("Only display non-SLOW law response zones (in World)", &bDisplayNonSlowLawResponseZones);
	bank.AddButton("GET_NAME_OF_ZONE at debug cam", GetNameOfZoneAtDebugCam);
	bank.PopGroup();
#endif // __DEV
}

#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// Name			:	FindSmallestForPosition
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
CPopZone* CPopZones::FindSmallestForPosition(const Vector3* pPoint, eZoneCategory zoneCategory, eZoneType zoneType)
{
	PF_FUNC(FindSmallestForPosition);

// #if __DEV
// 	char * pZoneName = NULL;
// #endif
	// casting down to int here - precision loss is worth it for the performance gain
	int pPoint_X = (int)pPoint->GetX();
	int pPoint_Y = (int)pPoint->GetY();
	int pPoint_Z = (int)pPoint->GetZ();

	// Try to use cached results
#define CACHED_RESULT_DISTANCE_MAX_TOLERANCE (10.0f)
	CPopZone* cachedResult = NULL;
	for(int i = 0; i < CACHED_SMALLEST_POP_ZONE_RESULTS; i++)
	{
		if(sm_smallestPopZoneCache[i].m_Result && sm_smallestPopZoneCache[i].m_ZoneCategory == zoneCategory && sm_smallestPopZoneCache[i].m_ZoneType == zoneType)
		{
			// adjust the tolerance for how close the cached point was to the edge of it's pop zone
			if(sm_smallestPopZoneCache[i].m_Point.IsClose(*pPoint, MIN(CACHED_RESULT_DISTANCE_MAX_TOLERANCE, sm_smallestPopZoneCache[i].m_smallestDistanceToEdge)) && sm_smallestPopZoneCache[i].m_Result)
			{
				// test to make sure the point lies in the cached zone
				if(DoesPointLieWithinPopZoneFast(pPoint_X, pPoint_Y, pPoint_Z, GetSearchInfoFromPopZone(sm_smallestPopZoneCache[i].m_Result)) 
					&& SmallestDistanceToPopZoneEdge(pPoint_X, pPoint_Y, pPoint_Z,GetSearchInfoFromPopZone(sm_smallestPopZoneCache[i].m_Result))>0) // don't use cached results that are right on the edge of the zone
				{
					cachedResult = sm_smallestPopZoneCache[i].m_Result;
					return cachedResult;
				}
				
			}
		}
	}
	// Cached value not found. Do full test
	
	CPopZone *pSmallestZone    = 0;
	u32 smallestZoneIndex = 0;
	//int       SmallestZoneSize = INT_MAX;
	u64       SmallestZoneSize = 0xffffffffffffffff;

	FastAssert( sm_popZoneSearchInfos.GetCount() == sm_popZones.GetCount());

	int popZoneIndex = 0;
	const int popZoneCount = sm_popZoneSearchInfos.GetCount();
	while(popZoneIndex < popZoneCount)
	{
        const CPopZoneSearchInfo *currentZoneInfo = &sm_popZoneSearchInfos[popZoneIndex];
        if(currentZoneInfo && IsValidForSearchType(currentZoneInfo, zoneCategory, zoneType))
        {
		    if(DoesPointLieWithinPopZoneFast(pPoint_X, pPoint_Y, pPoint_Z, currentZoneInfo))
		    {
				const u64 dx = (currentZoneInfo->m_x2 - currentZoneInfo->m_x1);
				const u64 dy = (currentZoneInfo->m_y2 - currentZoneInfo->m_y1);
				const u64 dz = (currentZoneInfo->m_z2 - currentZoneInfo->m_z1);

				const u64 SizeOfCurrentZone = dx * dy * dz;
				

			    if(SizeOfCurrentZone < SmallestZoneSize)
			    {
				    pSmallestZone = &sm_popZones[popZoneIndex];;
				    SmallestZoneSize = SizeOfCurrentZone;
					smallestZoneIndex = popZoneIndex;
			    }
		    }
        }
		
		++popZoneIndex;
	}	

	// cache result
	sm_smallestPopZoneCache[sm_smallestPopZoneCacheIndex].m_Result = pSmallestZone;
	sm_smallestPopZoneCache[sm_smallestPopZoneCacheIndex].m_Point = *pPoint;
	sm_smallestPopZoneCache[sm_smallestPopZoneCacheIndex].m_ZoneCategory = zoneCategory;
	sm_smallestPopZoneCache[sm_smallestPopZoneCacheIndex].m_ZoneType = zoneType;

	// store off the smallest difference between the point and it's zone, to try and reduce errors	
	sm_smallestPopZoneCache[sm_smallestPopZoneCacheIndex].m_smallestDistanceToEdge = SmallestDistanceToPopZoneEdge(pPoint_X, pPoint_Y, pPoint_Z, &sm_popZoneSearchInfos[smallestZoneIndex])*.75f;

	sm_smallestPopZoneCacheIndex++;
	if(sm_smallestPopZoneCacheIndex >= CACHED_SMALLEST_POP_ZONE_RESULTS)
	{
		sm_smallestPopZoneCacheIndex = 0; // wrap
	}

	return pSmallestZone;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetDirtRangeInternal
// Purpose		:	Sets the vehicle dirt range for the zone specified
// Parameters	:	minimum dirt value, maximum dirt value
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetVehDirtRange(int zone, float dirtMin, float dirtMax)
{
	sm_popZones[zone].m_vehDirtMin = dirtMin;
	sm_popZones[zone].m_vehDirtMax = dirtMax;
}

void CPopZones::SetPedDirtRange(int zone, float dirtMin, float dirtMax)
{
	sm_popZones[zone].m_pedDirtMin = dirtMin;
	sm_popZones[zone].m_pedDirtMax = dirtMax;
}

void CPopZones::SetDirtGrowScale(int zone, float dirtScale)
{
	sm_popZones[zone].m_dirtGrowScale = dirtScale;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetDirtColorInternal
// Purpose		:	Sets the vehicle dirt color for the zone specified
// Parameters	:	dirt color
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetDirtColor(int zone, const Color32& dirtCol)
{
	sm_popZones[zone].m_dirtCol = dirtCol;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetVfxRegionHashName
// Purpose		:	Sets the vfx region hash value from the specified zone
// Parameters	:	vfx region hash value
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////

void CPopZones::SetVfxRegionHashName(int zone, atHashWithStringNotFinal hashName)
{
	sm_popZones[zone].m_vfxRegionHashName = hashName;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetPlantsMgrTxdIdx
// Purpose		:	Sets PlantsMgr txd index for specified zone
// Parameters	:	txd index
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////

void CPopZones::SetPlantsMgrTxdIdx(int zone, u8 txdIdx)
{
	Assert(txdIdx < 8);	// must fit into 3 bits
	sm_popZones[zone].m_plantsMgrTxdIdx = txdIdx;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetLawResponseDelayType
// Purpose		:	Sets the delay type of our law response (generally slow, medium, fast)
// Parameters	:	enum/int representing the type of delay
/////////////////////////////////////////////////////////////////////////////////

void CPopZones::SetLawResponseDelayType(int zone, int iLawResponseDelayType)
{
	sm_popZones[zone].m_lawResponseDelayType = static_cast<u8>(iLawResponseDelayType);
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetVehicleResponseType
// Purpose		:	Sets the zone type of our vehicle response (default, countryside, city, etc) that determines vehicle types
// Parameters	:	enum/int representing the type of zone
/////////////////////////////////////////////////////////////////////////////////

void CPopZones::SetVehicleResponseType(int zone, int iVehicleResponseType)
{
	sm_popZones[zone].m_vehicleResponseType = static_cast<u8>(iVehicleResponseType);
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetSpecialAttribute
// Purpose		:	Sets the special type of the zone (none, airport)
// Parameters	:	enum/int representing the type of zone
/////////////////////////////////////////////////////////////////////////////////

void CPopZones::SetSpecialAttribute(int zone, int iSpecialAttributeType)
{
	sm_popZones[zone].m_specialAttributeType = static_cast<u8>(iSpecialAttributeType);
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetScumminess
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetScumminess (int zone, int iScumminess)
{
	if (Verifyf( (zone >= 0) && (zone < sm_popZones.GetCount()), "CPopZones::SetScumminess - zone index %d is out of range 0 to %d", zone, sm_popZones.GetCount()))
	{
		sm_popZones[zone].m_scumminess = iScumminess;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetScumminess
/////////////////////////////////////////////////////////////////////////////////
int  CPopZones::GetScumminess (int zone)
{
	if (Verifyf( (zone >= 0) && (zone < sm_popZones.GetCount()), "CPopZones::GetScumminess - zone index %d is out of range 0 to %d", zone, sm_popZones.GetCount()))
	{
		return sm_popZones[zone].m_scumminess;
	}
	return SCUMMINESS_ABOVE_AVERAGE;
}

void CPopZones::SetPopulationRangeScaleParameters(int zone, float fBaseRangeScale, float fLowZ, float fHightZ, float fMaxMultiplier)
{
	Assert(fHightZ >= fLowZ);
	Assert(fMaxMultiplier >= 1.0f && fMaxMultiplier <= 4.0f);

	sm_popZones[zone].m_fPopBaseRangeScale = Min(fBaseRangeScale, CVehiclePopulation::ms_fGatherLinksUseKeyHoleShapeRangeThreshold);
	sm_popZones[zone].m_iPopHeightScaleLowZ = (s16) fLowZ;
	sm_popZones[zone].m_iPopHeightScaleHighZ = (s16) fHightZ;
	sm_popZones[zone].m_fPopHeightScaleMultiplier = Min(fMaxMultiplier, CVehiclePopulation::ms_fGatherLinksUseKeyHoleShapeRangeThreshold);
}

void CPopZones::SetAllowScenarioWeatherExits(int zone, int iAllowScenarioWeatherExits)
{
	if (Verifyf( (zone >= 0) && (zone < sm_popZones.GetCount()), "CPopZones::SetAllowScenarioWeatherExits - zone index %d is out of range 0 to %d", zone, sm_popZones.GetCount()))
	{
		sm_popZones[zone].m_bAllowScenarioWeatherExits = (iAllowScenarioWeatherExits != 0);
	}
}

void CPopZones::SetAllowHurryAwayToLeavePavement(int zone, int iAllow)
{
	if (Verifyf( (zone >= 0) && (zone < sm_popZones.GetCount()), "CPopZones::SetAllowHurryAwayToLeavePavement - zone index %d is out of range 0 to %d", zone, sm_popZones.GetCount()))
	{
		sm_popZones[zone].m_bLetHurryAwayLeavePavements = (iAllow != 0);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetZoneScumminessFromString
// Purpose		:	Gets the zone scumminess level from a string value 
// Parameters	:	String value of the zone scumminess
/////////////////////////////////////////////////////////////////////////////////
int CPopZones::GetZoneScumminessFromString(char* pString)
{
	if (pString)
	{
		if (stricmp(pString, "SL_POSH") == 0)
		{
			return SCUMMINESS_POSH;
		}
		else if (stricmp(pString, "SL_NICE") == 0)
		{
			return SCUMMINESS_NICE;
		}
		else if (stricmp(pString, "SL_ABOVE_AV") == 0)
		{
			return SCUMMINESS_ABOVE_AVERAGE;
		}
		else if (stricmp(pString, "SL_BELOW_AV") == 0)
		{
			return SCUMMINESS_BELOW_AVERAGE;
		}
		else if (stricmp(pString, "SL_CRAP") == 0)
		{
			return SCUMMINESS_CRAP;
		}
		else if (stricmp(pString, "SL_SCUM") == 0)
		{
			return SCUMMINESS_SCUM;
		}
		else
		{
			Assertf(0, "Invalid zone scumminess type (%s)", pString);
		}
	}

	return SCUMMINESS_ABOVE_AVERAGE;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetSpecialZoneAttributeFromString
// Purpose		:	Gets the special type from a string value 
// Parameters	:	String value of the special type
/////////////////////////////////////////////////////////////////////////////////
int CPopZones::GetSpecialZoneAttributeFromString(char* pString)
{
	if (stricmp(pString, "SPECIAL_NONE") == 0)
	{
		return SPECIAL_NONE;
	}
	else if (stricmp(pString, "SPECIAL_AIRPORT") == 0)
	{
		return SPECIAL_AIRPORT;
	}
	else
	{
		Assertf(0, "Invalid special attribute type (%s)", pString);
	}

	return SPECIAL_NONE;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetPopScheduleIndicesInternal
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetPopScheduleIndices(int zone, s32 popScheduleIndexForSP, s32 popScheduleIndexForMP)
{
	// casting down to s8 here
	// will assert if we need more space - update PopZones.h if needed
	Assertf(popScheduleIndexForSP <= 127 && popScheduleIndexForMP <= 127, "Casting s32 down to s8, losing data");
	sm_popZoneSearchInfos[zone].m_popScheduleIndexForSP = static_cast<s8>(popScheduleIndexForSP);
	sm_popZoneSearchInfos[zone].m_popScheduleIndexForMP = static_cast<s8>(popScheduleIndexForMP);
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetAllowCops
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetAllowCops(int zone, bool allowCops)
{
	sm_popZones[zone].m_bNoCops = !allowCops;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetMPGangTerritoryIndex
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetMPGangTerritoryIndex(s32 zone, s32 territoryIndex)
{
	if (Verifyf( (zone >= 0) && (zone < sm_popZones.GetCount()), "CPopZones::SetMPGangTerritoryIndex - zone index %d is out of range 0 to %d", zone, sm_popZones.GetCount()))
	{
		sm_popZones[zone].m_MPGangTerritoryIndex = (s16) territoryIndex;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	SetEnabled
// Purpose		:	Turn on/off a zone
// Parameters	:	
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopZones::SetEnabled(int zone, bool bEnabled)
{
	sm_popZoneSearchInfos[zone].m_bEnabled = bEnabled;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetEnabled
// Purpose		:	See if a zone is enabled
// Parameters	:	
// Returns		:	true/false
/////////////////////////////////////////////////////////////////////////////////
bool CPopZones::GetEnabled(int zone)
{
	return sm_popZoneSearchInfos[zone].m_bEnabled;
}

