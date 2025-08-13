//
// scene/loader/ManagedImapGroup.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "scene/loader/ManagedImapGroup.h"

#include "atl/string.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "frontend/loadingscreens.h"
#include "frontend/PauseMenu.h"
#include "fwscene/stores/mapdatastore.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "grcore/debugdraw.h"
#include "scene/ContinuityMgr.h"
#include "spatialdata/aabb.h"
#include "spatialdata/sphere.h"

atArray<CManagedImapGroup> CManagedImapGroupMgr::ms_groups;

#if __BANK
bool CManagedImapGroupMgr::ms_bDisplayManagedImaps = false;
bool CManagedImapGroupMgr::ms_bDisplayActiveOnly = false;
#endif	//__BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		initialise managed imap group from packfile manifest metadata
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroup::Init(const CMapDataGroup& imapGroup)
{
	m_name = imapGroup.m_Name;
	m_bWeatherDependent = imapGroup.m_Flags.IsSet(WEATHER_DEPENDENT);
	m_bTimeDependent = imapGroup.m_Flags.IsSet(TIME_DEPENDENT);
	m_hoursOnOff = imapGroup.m_HoursOnOff;
	m_aWeatherTypes = imapGroup.m_WeatherTypes;

	m_mapDataSlot = -1;
	m_bActive = false;
	m_bOnlyRemoveWhenOutOfRange = false;

	// B* 1965200 - since fruit stalls usually have peds sitting behind them
	// flag these managed IMAP groups to only be removed when they are out of rendering distance
	if (imapGroup.m_Name.GetHash() == ATSTRINGHASH("ch1_02_fruitstall", 0x9317778f))
	{
		m_bOnlyRemoveWhenOutOfRange = true;
	}
	//
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetActive
// PURPOSE:		enable or disable group
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroup::SetActive(bool bActive)
{
	if (bActive)
	{
		fwMapDataStore::GetStore().RequestGroup(strLocalIndex(m_mapDataSlot), m_name, false);
	}
	else
	{
		fwMapDataStore::GetStore().RemoveGroup(strLocalIndex(m_mapDataSlot), m_name);
	}
	m_bActive = bActive;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SwapStateIfSafe
// PURPOSE:		toggles the active/inactive state of group if safe to do so (ie if not visible)
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroup::SwapStateIfSafe(Vec3V_In vPos)
{
	const spdAABB& entityAABB = fwMapDataStore::GetStore().GetBoxStreamer().GetBounds(m_mapDataSlot);
	const spdAABB& streamingAABB = fwMapDataStore::GetStore().GetBoxStreamer().GetStreamingBounds(m_mapDataSlot);

	Assertf(entityAABB.IsValid() && streamingAABB.IsValid(), "Managed IMAP group %s does not have valid bounds", m_name.GetCStr());

	bool bSafeToSwap = true;

	// if within visible range of map data file
	const bool bGameIsRendering = !(camInterface::IsFadedOut() || CLoadingScreens::AreActive() || CPauseMenu::GetPauseRenderPhasesStatus());
	if ( streamingAABB.ContainsPoint(vPos) && bGameIsRendering /*&& !g_ContinuityMgr.HasMajorCameraChangeOccurred()*/ )
	{
		const bool bForceOutOfRangeRequired = m_bOnlyRemoveWhenOutOfRange && m_bActive;

		// .. and if entity bounds are within view port, it isn't safe to swap
		spdSphere entitySphere = entityAABB.GetBoundingSphere();
		if ( camInterface::IsSphereVisibleInGameViewport( VEC3V_TO_VECTOR3(entitySphere.GetCenter()), entitySphere.GetRadiusf()) || bForceOutOfRangeRequired )
		{
			bSafeToSwap = false;
		}
	}

	if (bSafeToSwap)
	{
		SetActive(!m_bActive);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsRequired
// PURPOSE:		returns true if imap group is needed based on time of day and weather
//////////////////////////////////////////////////////////////////////////
bool CManagedImapGroup::IsRequired() const
{
	bool bWeatherTestPass = false;
	bool bTimeTestPass = false;

	if (m_bTimeDependent)
	{
		u32 hourMask = (u32) ( 1 << CClock::GetHour() );
		if ((m_hoursOnOff & hourMask) != 0)
		{
			bTimeTestPass = true;
		}
	}

	if (m_bWeatherDependent)
	{
		u32 weatherHash = ( g_weather.GetInterp()<0.5 ? g_weather.GetPrevType().m_hashName : g_weather.GetNextType().m_hashName );
		for (s32 i=0; i<m_aWeatherTypes.GetCount(); i++)
		{
			if (m_aWeatherTypes[i].GetHash()==weatherHash)
			{
				bWeatherTestPass = true;
			}
		}
	}

	// if both conditions are required, make sure both tests have passed
	if (m_bTimeDependent && m_bWeatherDependent)
	{
		return bTimeTestPass && bWeatherTestPass;
	}

	// otherwise check if either required condition has passed
	return (m_bTimeDependent && bTimeTestPass) || (m_bWeatherDependent && bWeatherTestPass);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		change state if required and possible
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroup::Update(Vec3V_In vPos)
{
	if (m_mapDataSlot==-1)
	{
		m_mapDataSlot = fwMapDataStore::GetStore().FindSlotFromHashKey(m_name.GetHash()).Get();
		if ( !Verifyf(m_mapDataSlot!=-1, "%s could not be found", m_name.GetCStr()) )
		{
			return;
		}
	}

	bool bRequired = IsRequired();
	if ( (m_bActive && !bRequired) || (!m_bActive && bRequired) )
	{
		SwapStateIfSafe(vPos);			
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		clear all known managed imap groups
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::Reset()
{
	for (s32 i=0; i<ms_groups.GetCount(); i++)
	{
		ms_groups[i].SetActive(false);
	}
	ms_groups.Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		requests or removes managed imap groups as required
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::Update(Vec3V_In vPos)
{
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
		return;
#endif // GTA_REPLAY

	for (s32 i=0; i<ms_groups.GetCount(); i++)
	{
		ms_groups[i].Update(vPos);
	}

#if __BANK
	UpdateDebug();
#endif	//__BANK
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Add
// PURPOSE:		register a managed imap group, from pack file metadata
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::Add(const CMapDataGroup& imapGroup)
{
	// don't add if it is already registered
	for (s32 i=0; i<ms_groups.GetCount(); i++)
	{
		if (ms_groups[i].GetName() == imapGroup.m_Name)
		{
			return;
		}
	}

	CManagedImapGroup& group = ms_groups.Grow();
	group.Init(imapGroup);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	MarkManagedImapFiles
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::MarkManagedImapFiles()
{
	for (s32 i=0; i<ms_groups.GetCount(); i++)
	{
		u32 nameHash = ms_groups[i].GetName().GetHash();
		strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlotFromHashKey(nameHash);
		if ( slotIndex.IsValid() )
		{
			fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(slotIndex);
			if (pDef && pDef->GetIsValid())
			{
				pDef->SetIsCodeManaged(true);
			}
		}
	}
}

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		debug widgets for managed imap groups
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Time & Weather IMAP groups");
	bank.AddToggle("Display groups", &ms_bDisplayManagedImaps);
	bank.AddToggle("Display active only", &ms_bDisplayActiveOnly);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		debug draw etc for listing managed groups
//////////////////////////////////////////////////////////////////////////
void CManagedImapGroupMgr::UpdateDebug()
{
	if (ms_bDisplayManagedImaps)
	{
		for (s32 i=0; i<ms_groups.GetCount(); i++)
		{
			CManagedImapGroup& imapGroup= ms_groups[i];
			if (!ms_bDisplayActiveOnly || imapGroup.IsActive())
			{
				grcDebugDraw::AddDebugOutput("(%d) MANAGED GROUP %s (weather control:%s, time control:%s %s, onlyRemoveWhenOutOfRange=%s",
					i, imapGroup.m_name.GetCStr(),
					( imapGroup.m_bWeatherDependent ? "Y" : "N" ),
					( imapGroup.m_bTimeDependent ? "Y" : "N" ),
					( imapGroup.m_bActive ? "ACTIVE" : ""),
					( imapGroup.m_bOnlyRemoveWhenOutOfRange ? "Y" : "N")
				);

				if (imapGroup.m_bTimeDependent)
				{
					atString timeString("");

					for (s32 hour=0; hour<24; hour++)
					{
						if (imapGroup.m_hoursOnOff & (1 << hour))
						{
							if (hour)
							{
								timeString += ", ";
							}
							char tmp[10];
							sprintf(tmp, "%02d:00", hour);
							timeString += tmp;
						}
					}
					grcDebugDraw::AddDebugOutput("... hours active: %s", timeString.c_str());
				}

				if (imapGroup.m_bWeatherDependent)
				{
					atString weatherString("");
					for (s32 weather=0; weather<imapGroup.m_aWeatherTypes.GetCount(); weather++)
					{
						if (weather)
						{
							weatherString += ", ";
						}
						weatherString += imapGroup.m_aWeatherTypes[weather].GetCStr();
					}
					grcDebugDraw::AddDebugOutput("... weather types: %s", weatherString.c_str());
				}
			}
		}
	}
}

#endif	//__BANK
