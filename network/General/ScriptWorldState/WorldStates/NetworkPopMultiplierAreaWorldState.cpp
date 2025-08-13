//
// name:		NetworkCarGenWorldState.cpp
// description:	Support for network scripts to switch pop multiplier areas
// written by:	Daniel Yelland
//
//
#include "NetworkPopMultiplierAreaWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "peds/Ped.h"
#include "Game/PopMultiplierAreas.h"

//--------------------------------------------------------

NETWORK_OPTIMISATIONS()

//--------------------------------------------------------

/* MAX_POP_MULTIPLIER_AREAS defined in Game/PopMultiplierAreas.h */
FW_INSTANTIATE_CLASS_POOL(CNetworkPopMultiplierAreaWorldStateData, MAX_POP_MULTIPLIER_AREAS, atHashString("CNetworkPopMultiplierAreaWorldStateData",0x1116261c));

//--------------------------------------------------------

const s32 CNetworkPopMultiplierAreaWorldStateData::INVALID_AREA_INDEX				= -1;
const float	CNetworkPopMultiplierAreaWorldStateData::MAXIMUM_PED_DENSITY_VALUE		= 100.0f;
const float	CNetworkPopMultiplierAreaWorldStateData::MAXIMUM_TRAFFIC_DENSITY_VALUE	= 100.0f;
const u32	CNetworkPopMultiplierAreaWorldStateData::SIZEOF_FLOAT_PACKING			= 12;
const u32	CNetworkPopMultiplierAreaWorldStateData::SIZEOF_AREA_INDEX_PACKING		= 5;

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::CNetworkPopMultiplierAreaWorldStateData>
// RSGLDS ADW 
//--------------------------------------------------------

CNetworkPopMultiplierAreaWorldStateData::CNetworkPopMultiplierAreaWorldStateData()
{}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::CNetworkPopMultiplierAreaWorldStateData>
// RSGLDS ADW 
//--------------------------------------------------------

CNetworkPopMultiplierAreaWorldStateData::CNetworkPopMultiplierAreaWorldStateData
(
	const CGameScriptId &_scriptID,
	const Vector3		&_minWS,
	const Vector3		&_maxWS,
	const float			_pedDensity,
	const float			_trafficDensity,
	const bool			_locallyOwned,
	const bool			_sphere,
	const bool			bCameraGlobalMultiplier
) : CNetworkWorldStateData
(
	_scriptID, 
	_locallyOwned
)
{
	m_minWS				= _minWS;
	m_maxWS				= _maxWS;
	m_pedDensity		= _pedDensity;
	m_trafficDensity	= _trafficDensity;
	m_isSphere			= _sphere;
	m_bCameraGlobalMultiplier = bCameraGlobalMultiplier;

	gnetAssertf(m_pedDensity		< MAXIMUM_PED_DENSITY_VALUE,		"CNetworkPopMultiplierAreaWorldStateData::c_str ped density value is too high! 100x normal density!? : RSGLDS ADW");
	gnetAssertf(m_trafficDensity	< MAXIMUM_TRAFFIC_DENSITY_VALUE,	"CNetworkPopMultiplierAreaWorldStateData::c_str traffic density value is too high! 100x normal density!? : RSGLDS ADW");
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::Clone>
// RSGLDS ADW
//--------------------------------------------------------

CNetworkPopMultiplierAreaWorldStateData *CNetworkPopMultiplierAreaWorldStateData::Clone() const
{
    return rage_new CNetworkPopMultiplierAreaWorldStateData(GetScriptID(), m_minWS, m_maxWS, m_pedDensity, m_trafficDensity, IsLocallyOwned(), m_isSphere, m_bCameraGlobalMultiplier); 
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::IsEqualTo>
// RSGLDS ADW
//--------------------------------------------------------

bool CNetworkPopMultiplierAreaWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkPopMultiplierAreaWorldStateData *popAreaState = SafeCast(const CNetworkPopMultiplierAreaWorldStateData, &worldState);

        const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

        if(
			m_minWS.IsClose(popAreaState->m_minWS, epsilon) &&
			m_maxWS.IsClose(popAreaState->m_maxWS, epsilon) &&
			(fabs(m_pedDensity- popAreaState->m_pedDensity) < 0.05f) &&
			(fabs(m_trafficDensity - popAreaState->m_trafficDensity) < 0.05f) &&
			m_isSphere == popAreaState->m_isSphere &&
			m_bCameraGlobalMultiplier == popAreaState->m_bCameraGlobalMultiplier
			)
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::ChangeWorldState>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent
	(
		NetworkInterface::GetObjectManagerLog(), "ADDING_POP_MULTIPLIER_AREA", 
        "%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
        GetScriptID().GetLogName(),
        m_minWS.x, m_minWS.y, m_minWS.z,
		m_maxWS.x, m_maxWS.y, m_maxWS.z
	);

	s32 index = CThePopMultiplierAreas::INVALID_AREA_INDEX;
	if (m_isSphere)
		index = CThePopMultiplierAreas::CreatePopMultiplierArea(m_minWS, m_maxWS.x, m_pedDensity, m_trafficDensity, m_bCameraGlobalMultiplier);
	else
		index = CThePopMultiplierAreas::CreatePopMultiplierArea(m_minWS, m_maxWS, m_pedDensity, m_trafficDensity, m_bCameraGlobalMultiplier);
	gnetAssertf(index != CThePopMultiplierAreas::INVALID_AREA_INDEX, "CNetworkPopMultiplierAreaWorldStateData::ChangeWorldState - Failed to add new area across network! RSGLDS ADW");
	UNUSED_VAR(index);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::RevertWorldState>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent
	(
		NetworkInterface::GetObjectManagerLog(), "REMOVING_POP_MULTIPLIER_AREA", 
        "%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
        GetScriptID().GetLogName(),
        m_minWS.x, m_minWS.y, m_minWS.z,
		m_maxWS.x, m_maxWS.y, m_maxWS.z
	);

	CThePopMultiplierAreas::RemovePopMultiplierArea(m_minWS, m_maxWS, m_isSphere, m_pedDensity, m_trafficDensity, m_bCameraGlobalMultiplier);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::AddArea>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::AddArea(
														const CGameScriptId &_scriptID,
														const Vector3		&_minWS,
														const Vector3		&_maxWS,
														const float			_pedDensity,
														const float			_trafficDensity,
														const bool          _fromNetwork,
														const bool			_sphere,
														const bool			bCameraGlobalMultiplier
													)
{
	CNetworkPopMultiplierAreaWorldStateData worldStateData(_scriptID, _minWS, _maxWS, _pedDensity, _trafficDensity, !_fromNetwork, _sphere, bCameraGlobalMultiplier);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, _fromNetwork);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::RemoveArea>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::RemoveArea(
															const CGameScriptId &_scriptID,
															const Vector3		&_minWS,
															const Vector3		&_maxWS,
															const float			_pedDensity,
															const float			_trafficDensity,
															const bool          _fromNetwork,
															const bool			sphere,
															const bool			bCameraGlobalMultiplier
														)
{
	CNetworkPopMultiplierAreaWorldStateData worldStateData(_scriptID, _minWS, _maxWS, _pedDensity, _trafficDensity, !_fromNetwork, sphere, bCameraGlobalMultiplier);
    CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, _fromNetwork);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::RemoveArea>
// RSGLDS ADW
//--------------------------------------------------------

bool CNetworkPopMultiplierAreaWorldStateData::FindArea(
														const Vector3		&_minWS,
														const Vector3		&_maxWS,
														const float			_pedDensity,
														const float			_trafficDensity,
														const bool			_sphere,
														const bool			bCameraGlobalMultiplier
													)
{
	CNetworkPopMultiplierAreaWorldStateData::Pool *pool = CNetworkPopMultiplierAreaWorldStateData::GetPool();

	if(pool)
	{
		s32 i = pool->GetSize();

		while(i--)
		{
			CNetworkPopMultiplierAreaWorldStateData *worldState = pool->GetSlot(i);

			if(worldState)
			{
				if(worldState->m_minWS.IsClose(_minWS, FLT_EPSILON))
				{
					if(worldState->m_maxWS.IsClose(_maxWS, FLT_EPSILON))
					{
						if(worldState->m_isSphere == _sphere)
						{
							if(fabs(worldState->m_pedDensity - _pedDensity) < 0.05f)
							{
								if(fabs(worldState->m_trafficDensity - _trafficDensity) < 0.05f)
								{
									if(worldState->m_bCameraGlobalMultiplier == bCameraGlobalMultiplier)
									{
										return true;
									}
								}
							}
						}
					}
				}		
			}
		}
	}

	return false;
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::LogState>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());

    if(m_isSphere)
    {
        // the population multiplier system this world state synchronises re-uses the box variables
        // storing the centre position in the box min position, and the radius in the x component of the box max position
        log.WriteDataValue("Centre", "%.2f, %.2f, %.2f", m_minWS.x, m_minWS.y, m_minWS.z);
        log.WriteDataValue("Radius", "%.2f", m_maxWS.x);
    }
    else
    {
        log.WriteDataValue("Min", "%.2f, %.2f, %.2f", m_minWS.x, m_minWS.y, m_minWS.z);
        log.WriteDataValue("Max", "%.2f, %.2f, %.2f", m_maxWS.x, m_maxWS.y, m_maxWS.z);
    }
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::Serialise>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::Serialise>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::SerialiseWorldState>
// RSGLDS ADW
//--------------------------------------------------------

template <class Serialiser> void CNetworkPopMultiplierAreaWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
	GetScriptID().Serialise(serialiser);
    SERIALISE_POSITION(serialiser, m_minWS);
    SERIALISE_POSITION(serialiser, m_maxWS);
	SERIALISE_PACKED_FLOAT(serialiser, m_pedDensity,		MAXIMUM_PED_DENSITY_VALUE,		SIZEOF_FLOAT_PACKING, "ped density");
	SERIALISE_PACKED_FLOAT(serialiser, m_trafficDensity,	MAXIMUM_TRAFFIC_DENSITY_VALUE,	SIZEOF_FLOAT_PACKING, "traffic density");
	SERIALISE_BOOL(serialiser, m_isSphere);
	SERIALISE_BOOL(serialiser, m_bCameraGlobalMultiplier);
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::GetRegion>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::GetRegion(Vector3 &min, Vector3 &max) const
{
    min = m_minWS;
    max = m_maxWS;
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::IsConflicting>
// RSGLDS ADW
//--------------------------------------------------------

#if __DEV

bool CNetworkPopMultiplierAreaWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool overlapping = false;

    if(!IsEqualTo(worldState))
    {
        if(worldState.GetType() == GetType())
        {
            const CNetworkPopMultiplierAreaWorldStateData *popMultiplierState = SafeCast(const CNetworkPopMultiplierAreaWorldStateData, &worldState);

			if (m_isSphere)
				overlapping = (m_minWS - popMultiplierState->m_minWS).Mag2() < (m_maxWS.x + popMultiplierState->m_maxWS.x * m_maxWS.x + popMultiplierState->m_maxWS.x);
			else
				overlapping = geomBoxes::TestAABBtoAABB(m_minWS, m_maxWS, popMultiplierState->m_minWS, popMultiplierState->m_maxWS);
        }
    }

    return overlapping;
}

#endif // __DEV

#if __BANK

bool g_bDebugMultiplierAreaIsCameraGlobal = true;
float g_fDebugMultiplierPeds = 1.0f;
float g_fDebugMultiplierTraffic = 1.0f;
float g_fDebugMultiplierSize = 5.0f;

static void NetworkBank_AddTestPopMultiplierSphere_New()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
			float size = g_fDebugMultiplierSize;
            
			float pedDensity		= g_fDebugMultiplierPeds;
			float trafficDensity	= g_fDebugMultiplierTraffic;

			s32 index = CThePopMultiplierAreas::CreatePopMultiplierArea(vPlayerPosition, size, pedDensity, trafficDensity, g_bDebugMultiplierAreaIsCameraGlobal);
			gnetAssertf(index != CThePopMultiplierAreas::INVALID_AREA_INDEX, "NetworkBank_AddTestPopMultiplierArea - Invalid index returned! - RSGLDS ADW");

			PopMultiplierArea const* area = CThePopMultiplierAreas::GetActiveArea(index);
			gnetAssert(area);

			CNetworkPopMultiplierAreaWorldStateData::AddArea(scriptID, area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, /*index,*/ false, area->IsSphere(), area->m_bCameraGlobalMultiplier);
        }
    }
}

static void NetworkBank_AddTestPopMultiplierSphere_New_LocalOnly()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
			float size = g_fDebugMultiplierSize;

			float pedDensity		= g_fDebugMultiplierPeds;
			float trafficDensity	= g_fDebugMultiplierTraffic;

			OUTPUT_ONLY(s32 index = )CThePopMultiplierAreas::CreatePopMultiplierArea(vPlayerPosition, size, pedDensity, trafficDensity, g_bDebugMultiplierAreaIsCameraGlobal);
			gnetAssertf(index != CThePopMultiplierAreas::INVALID_AREA_INDEX, "NetworkBank_AddTestPopMultiplierArea - Invalid index returned! - RSGLDS ADW");
        }
    }
}

static void NetworkBank_AddTestPopMultiplierArea_New()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
			float size = g_fDebugMultiplierSize;
            Vector3 boxStart = vPlayerPosition - Vector3(size, size, size);
            Vector3 boxEnd   = vPlayerPosition + Vector3(size, size, size);

			float pedDensity		= g_fDebugMultiplierPeds;
			float trafficDensity	= g_fDebugMultiplierTraffic;

			OUTPUT_ONLY(s32 index = )CThePopMultiplierAreas::CreatePopMultiplierArea(boxStart, boxEnd, pedDensity, trafficDensity, g_bDebugMultiplierAreaIsCameraGlobal);
			gnetAssertf(index != CThePopMultiplierAreas::INVALID_AREA_INDEX, "NetworkBank_AddTestPopMultiplierArea - Invalid index returned! - RSGLDS ADW");

			CNetworkPopMultiplierAreaWorldStateData::AddArea(scriptID, boxStart, boxEnd, pedDensity, trafficDensity, false, false, g_bDebugMultiplierAreaIsCameraGlobal);
        }
    }
}

static void NetworkBank_AddTestPopMultiplierArea_New_LocalOnly()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
			float size = g_fDebugMultiplierSize;
            Vector3 boxStart = vPlayerPosition - Vector3(size, size, size);
            Vector3 boxEnd   = vPlayerPosition + Vector3(size, size, size);

			float pedDensity		= g_fDebugMultiplierPeds;
			float trafficDensity	= g_fDebugMultiplierTraffic;

			OUTPUT_ONLY(s32 index = )CThePopMultiplierAreas::CreatePopMultiplierArea(boxStart, boxEnd, pedDensity, trafficDensity, g_bDebugMultiplierAreaIsCameraGlobal);
			gnetAssertf(index != CThePopMultiplierAreas::INVALID_AREA_INDEX, "NetworkBank_AddTestPopMultiplierArea - Invalid index returned! - RSGLDS ADW");
        }
    }
}

static void NetworkBank_RemoveTestPopMultiplierArea_New()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);

			int index = CThePopMultiplierAreas::GetLastActiveAreaIndex_DebugOnly();

			if(index != CThePopMultiplierAreas::INVALID_AREA_INDEX)
			{
				PopMultiplierArea const * const area = CThePopMultiplierAreas::GetActiveArea(index);
				gnetAssertf(area, "NetworkBank_RemoveTestPopMultiplierArea : valid Index but invalid pointer?! : RSGLDS ADW");
				gnetAssertf(area->m_init, "NetworkBank_RemoveTestPopMultiplierArea : valid Index but invalid pointer?! : RSGLDS ADW");
	
				if(CNetworkPopMultiplierAreaWorldStateData::FindArea(area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, false, area->m_bCameraGlobalMultiplier))
				{
		            CNetworkPopMultiplierAreaWorldStateData::RemoveArea(scriptID, area->m_minWS, area->m_maxWS, /*index,*/ area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, false, area->IsSphere(), area->m_bCameraGlobalMultiplier);

					CThePopMultiplierAreas::RemovePopMultiplierArea(area->m_minWS, area->m_maxWS, area->m_isSphere, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->m_bCameraGlobalMultiplier);
				}
				else
				{
					gnetAssertf(false, "ERROR : Attempting to network delete a local population mutliplier area!");
				}
			}
        }
    }
}

static void NetworkBank_RemoveTestPopMultiplierArea_New_LocalOnly()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);

			int index = CThePopMultiplierAreas::GetLastActiveAreaIndex_DebugOnly();

			if(index != CThePopMultiplierAreas::INVALID_AREA_INDEX)
			{
				PopMultiplierArea const * const area = CThePopMultiplierAreas::GetActiveArea(index);
				gnetAssertf(area, "NetworkBank_RemoveTestPopMultiplierArea : valid Index but invalid pointer?! : RSGLDS ADW");
				gnetAssertf(area->m_init, "NetworkBank_RemoveTestPopMultiplierArea : valid Index but invalid pointer?! : RSGLDS ADW");
	
				if(!CNetworkPopMultiplierAreaWorldStateData::FindArea(area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->m_isSphere, area->m_bCameraGlobalMultiplier))
				{
					CThePopMultiplierAreas::RemovePopMultiplierArea(area->m_minWS, area->m_maxWS, area->m_isSphere, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->m_bCameraGlobalMultiplier);
				}
				else
				{
					gnetAssertf(false, "ERROR : Attempting to locally delete a networked population mutliplier area!");
				}
			}
        }
    }	
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::AddDebugWidgets>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Pop Multiplier Areas", false);
        {
			bank->AddToggle("Display", &CThePopMultiplierAreas::gs_bDisplayPopMultiplierAreas);
			bank->AddToggle("Add as \"camera-global\"", &g_bDebugMultiplierAreaIsCameraGlobal);
			bank->AddSlider("Ped mult", &g_fDebugMultiplierPeds, 0.0f, 10.0f, 0.1f);
			bank->AddSlider("Traffic mult", &g_fDebugMultiplierTraffic, 0.0f, 10.0f, 0.1f);
			bank->AddSlider("Area size", &g_fDebugMultiplierSize, 1.0f, 100000.0f, 1.0f);
			

			bank->AddButton("Remove test pop multiplier area", datCallback(NetworkBank_RemoveTestPopMultiplierArea_New));
			bank->AddButton("Remove test pop multiplier area local only", datCallback(NetworkBank_RemoveTestPopMultiplierArea_New_LocalOnly));
			bank->AddButton("Add test pop multiplier area", datCallback(NetworkBank_AddTestPopMultiplierArea_New));
			bank->AddButton("Add test pop multiplier area local only", datCallback(NetworkBank_AddTestPopMultiplierArea_New_LocalOnly));
			bank->AddButton("Add test pop multiplier sphere", datCallback(NetworkBank_AddTestPopMultiplierSphere_New));
			bank->AddButton("Add test pop multiplier sphere local only", datCallback(NetworkBank_AddTestPopMultiplierSphere_New_LocalOnly));

        }
        bank->PopGroup();
    }
}

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::DisplayDebugText>
// RSGLDS ADW
//--------------------------------------------------------

void CNetworkPopMultiplierAreaWorldStateData::DisplayDebugText()
{
	CNetworkPopMultiplierAreaWorldStateData::Pool *pool = CNetworkPopMultiplierAreaWorldStateData::GetPool();

	s32 i = pool->GetSize();
    const unsigned MAX_BUFFER_SIZE = 2048;
	char buffer[MAX_BUFFER_SIZE] = "\0";

	while(i--)
	{
		CNetworkPopMultiplierAreaWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            Vector3 min,max;
            worldState->GetRegion(min, max);

            const unsigned SMALL_BUFFER_SIZE = 100;
			char temp[SMALL_BUFFER_SIZE] = "\0";
			formatf(temp, "Network Pop multiplier area: %d %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)\n", 
			i,
			worldState->GetScriptID().GetLogName(),
			min.x, min.y, min.z,
			max.x, max.y, max.z
			);
            temp[SMALL_BUFFER_SIZE-1]='\0';
			safecat(buffer, temp);
            buffer[MAX_BUFFER_SIZE-1]='\0';
        }
	}

	CNetGamePlayer* localNetGamePlayer	= NetworkInterface::GetLocalPlayer(); 
	if(localNetGamePlayer)
	{
		CPed* player = localNetGamePlayer->GetPlayerPed();

		if(player)
		{
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()), Color_white, buffer, false);
		}
	}

 /*   CNetworkPopMultiplierAreaWorldStateData::Pool *pool = CNetworkPopMultiplierAreaWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkPopMultiplierAreaWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            Vector3 min,max;
            worldState->GetRegion(min, max);
            grcDebugDraw::AddDebugOutput
			(
				"Pop multiplier areas: %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", 
				worldState->GetScriptID().GetLogName(),
				min.x, min.y, min.z,
				max.x, max.y, max.z
			);
        }
    }*/
}

#endif // __BANK
