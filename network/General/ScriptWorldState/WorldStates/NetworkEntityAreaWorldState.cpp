//
// name:		NetworkEntityAreaWorldState.cpp
// description: Support for network scripts to register an area in which entities might be created. This area will
//				be marked as available or unavailable by other players
// written by:	John Hynd
//
#include "NetworkEntityAreaWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "vector/geometry.h"
#include "phbound/BoundBox.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "peds/Ped.h"
#include "Script/Script.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetworkEntityAreaWorldStateData, CONFIGURED_FROM_FILE, atHashString("NetworkEntityAreas",0x36fad92f));

int CNetworkEntityAreaWorldStateData::ms_NextNetworkID = 0;
int CNetworkEntityAreaWorldStateData::ms_NextScriptID = 0;

// a relatively high epsilon value to take quantisation from network serialising into account
static const float AREA_EPSILON = 0.2f; 

#if __BANK
// indicates whether entity areas should be drawn or not
bool CNetworkEntityAreaWorldStateData::gs_bDrawEntityAreas = false;
#endif

CNetworkEntityAreaWorldStateData::CNetworkEntityAreaWorldStateData()
	: m_NetworkID(-1)
	, m_ScriptAreaID(-1)
	, m_nIncludeFlags(eInclude_Default)
	, m_IsOccupied(0)
	, m_HasReplyFrom(0)
	, m_bIsOccupied(false)
	, m_bIsDirty(false)
    , m_bClientCreated(false)
    , m_ClientPeerID(0)
	, m_uLastCheckTimeMs(0)
{
	m_AreaStart.Zero();
	m_AreaEnd.Zero();
	m_bIsAngledArea = false;
	m_AreaWidth = -1.0f;
	gnetDebug3("%s m_NetworkID starts off as -1 for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_ScriptAreaID);
}

CNetworkEntityAreaWorldStateData::CNetworkEntityAreaWorldStateData(const CGameScriptId &scriptID,
																   bool locallyOwned,
                                                                   bool clientCreated,
                                                                   u64  clientPeerID,
																   int networkID,
																   int scriptAreaID,
																   IncludeFlags nFlags,
																   const Vector3 &vStart,
																   const Vector3 &vEnd,
																   const float areaWidth)
	: CNetworkWorldStateData(scriptID, locallyOwned)
	, m_NetworkID(networkID)
	, m_ScriptAreaID(scriptAreaID)
	, m_nIncludeFlags(nFlags)
	, m_IsOccupied(0)
	, m_HasReplyFrom(0)
	, m_bIsOccupied(false)
	, m_bIsDirty(false)
    , m_bClientCreated(clientCreated)
    , m_ClientPeerID(clientPeerID)
	, m_uLastCheckTimeMs(0)
{
	m_AreaStart = vStart;
	m_AreaEnd = vEnd;
	m_AreaWidth = areaWidth;

	// setup bounding box
	InitializeBoundingBox();

	// setup network ID
	if(locallyOwned && m_NetworkID == -1)
	{
		bool isHostOfScript = CTheScripts::GetScriptHandlerMgr().IsNetworkHost(GetScriptID());
		if(isHostOfScript)
		{
			m_NetworkID = ms_NextNetworkID++;
			gnetDebug2("CNetworkEntityAreaWorldStateData::CNetworkEntityAreaWorldStateData script %s m_NetworkID is now %d for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_NetworkID, m_ScriptAreaID);
			gnetAssert(m_NetworkID >= 0 && m_NetworkID < MAX_NUM_NETWORK_IDS);
			if(ms_NextNetworkID >= MAX_NUM_NETWORK_IDS)
			{
				ms_NextNetworkID = 0;
			}
		}
		else
		{
			sysStack::PrintStackTrace();
			gnetDebug2("CNetworkEntityAreaWorldStateData::CNetworkEntityAreaWorldStateData() Not host of script %s! m_NetworkID will remain -1 for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_ScriptAreaID);
		}
	}
}

void CNetworkEntityAreaWorldStateData::InitializeBoundingBox()
{
	m_boxMatrix.Identity();
	m_boxMatrix.d = (m_AreaStart + m_AreaEnd) / 2.f;

	m_bIsAngledArea = false;
	if (m_AreaWidth != -1.0f)
	{
		//ANGLED

		m_bIsAngledArea = true;

		float angleRadians = fwAngle::GetRadianAngleBetweenPoints(m_AreaStart.x, m_AreaStart.y, m_AreaEnd.x, m_AreaEnd.y);
		m_boxMatrix.MakeRotateZ(angleRadians);

		float tempLength = (m_AreaEnd - m_AreaStart).XYMag();
		float tempZ = fabs(m_AreaEnd.z - m_AreaStart.z);

		//Fix problems if Y is 0 - use area width
		tempLength = (tempLength > 0.f) ? tempLength : m_AreaWidth;

		//Fix problems if Z is 0 - use area width
		tempZ = (tempZ > 0.f) ? tempZ : m_AreaWidth;

		// validate area limits
		gnetAssertf(tempLength > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid angled area limits supplied. too small (vEnd - vStart).XYMag()! using m_AreaWidth");
		gnetAssertf(tempZ > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid angled area limits supplied. too small  (vEnd.z - vStart.z)! using m_AreaWidth");

		m_boxSize.x = m_AreaWidth;
		m_boxSize.y = tempLength;
		m_boxSize.z = tempZ;
	}
	else
	{
		//AXIS-ALIGNED
		m_boxSize = (m_AreaEnd - m_AreaStart);
	}

	//Ensure the box size is absolute
	m_boxSize.Abs();
}

void CNetworkEntityAreaWorldStateData::Update()
{
	// bail out if we haven't made it into the match yet - this ensures that our 
	// trigger events will include all current players
	if(!NetworkInterface::IsGameInProgress())
		return;

	static unsigned const AREA_CHECK_INTERVAL = 1000;
	if(sysTimer::GetSystemMsTime() - m_uLastCheckTimeMs > AREA_CHECK_INTERVAL)
	{
		m_bIsDirty = true; 
	}
	
	if(m_bIsDirty)
	{
		// mark last check time and clear dirty flag
		m_uLastCheckTimeMs = sysTimer::GetSystemMsTime();
		m_bIsDirty = false;

		// test with desired types 
		int nTestTypes = 0;
		if(m_nIncludeFlags & eInclude_Vehicles)
			nTestTypes |= ArchetypeFlags::GTA_VEHICLE_TYPE;
		if(m_nIncludeFlags & eInclude_Peds)
			// include the flag in the comment if we want to include dead peds
			nTestTypes |= ArchetypeFlags::GTA_PED_TYPE; // | ArchetypeFlags::GTA_RAGDOLL_TYPE;
		if(m_nIncludeFlags & eInclude_Objects)
			nTestTypes |= ArchetypeFlags::GTA_OBJECT_TYPE;

		// when using box bounds these need to persist outside of the scope they are set in
		// so declare them here.
		WorldProbe::CShapeTestBoundingBoxDesc probeDesc;
		rage::phBoundBox boxBound;

		// fix for dimensions
		boxBound.SetBoxSize(VECTOR3_TO_VEC3V(m_boxSize));

		probeDesc.SetBound(static_cast<phBound*>(&boxBound));
		probeDesc.SetTransform(&m_boxMatrix);
		probeDesc.SetIncludeFlags(nTestTypes);
		probeDesc.SetContext(WorldProbe::ENetwork);

		// check if the result is different than our previous result
		bool bIsOccupied = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		bool bHaveReplied = (m_HasReplyFrom & (1 << NetworkInterface::GetLocalPhysicalPlayerIndex())) != 0;
		if((bIsOccupied != m_bIsOccupied) || !bHaveReplied)
		{
			// update state
			m_bIsOccupied = bIsOccupied;

			// mark state in occupied bitfield
			if(bIsOccupied)
				m_IsOccupied |= (1 << NetworkInterface::GetLocalPhysicalPlayerIndex());
			else
				m_IsOccupied &= ~(1 << NetworkInterface::GetLocalPhysicalPlayerIndex());

			// update reply status
			m_HasReplyFrom |= (1 << NetworkInterface::GetLocalPhysicalPlayerIndex());
			
			if (m_NetworkID == -1)
			{
				gnetDebug2("CNetworkEntityAreaWorldStateData::Update() script %s m_NetworkID is -1 for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_ScriptAreaID);
			}

			// let everyone else know (using local state)
			CEntityAreaStatusEvent::Trigger(GetScriptID(), m_NetworkID, m_bIsOccupied);
		}

		boxBound.Release(false);
	}

#if __BANK
	// if we should draw the area, add it to our debug draw
	if(CNetworkEntityAreaWorldStateData::gs_bDrawEntityAreas)
	{
		char buf[128];

		Vec3V bmin = Vec3V(m_AreaStart);
		Vec3V bmax = Vec3V(m_AreaEnd);
		Vec3V bcenter = VECTOR3_TO_VEC3V(m_boxMatrix.d);

		formatf(buf,"m_ScriptAreaID[%d] m_bIsAngledArea[%d]",m_ScriptAreaID,m_bIsAngledArea);
		grcDebugDraw::Text(bcenter,Color_black,buf);

		//Retain for debugging time - dirty issues
		//formatf(buf,"m_uLastCheckTimeMs[%u] deltatime[%u] >? AREA_CHECK_INTERVAL[%u]",m_uLastCheckTimeMs,(sysTimer::GetSystemMsTime() - m_uLastCheckTimeMs),AREA_CHECK_INTERVAL);
		//grcDebugDraw::Text(bcenter,Color_black,0,20,buf);

		if (m_bIsAngledArea)
		{
			Vector3 vHalfAngledBoxSize = m_boxSize / 2.0f;
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(-vHalfAngledBoxSize),VECTOR3_TO_VEC3V(vHalfAngledBoxSize),MATRIX34_TO_MAT34V(m_boxMatrix), m_bIsOccupied ? Color_green : Color_orange, false);

			//Retain for debug comparison of the physics box vs the project angled area box
			//CScriptAngledArea debugAngledArea(m_AreaStart,m_AreaEnd,m_AreaWidth,NULL);
			//debugAngledArea.DebugDraw(m_bIsOccupied ? Color_pink : Color_brown);
		}
		else
		{
			grcDebugDraw::BoxAxisAligned(bmin, bmax, m_bIsOccupied ? Color_green : Color_red, false);
		}

		grcDebugDraw::Arrow(bmin,bmax,0.2f,Color_blue);
	}
#endif
}

bool CNetworkEntityAreaWorldStateData::HasExpired() const
{
    // revert client created entity areas when the player that created them has left the session
    if(m_bClientCreated)
    {
        // wait a while before reverting the world state, to allow time for the local machine to be informed
        // about any newly joining players, so the state doesn't get removed incorrectly
        const unsigned DELAY_REMOVAL_PERIOD = 5000;
        unsigned timeSinceCreated = sysTimer::GetSystemMsTime() - GetTimeCreated();

        if(timeSinceCreated >= DELAY_REMOVAL_PERIOD)
        {
            if(!netInterface::GetPlayerFromPeerId(m_ClientPeerID))
            {
                return true;
            }
        }
    }

    return false;
}

void CNetworkEntityAreaWorldStateData::PlayerHasJoined(const netPlayer &player)
{
	// check if we've already sent out a status on this area
	bool bHaveReplied = (m_HasReplyFrom & (1 << NetworkInterface::GetLocalPhysicalPlayerIndex())) != 0;
	
	// let new players know our state
	if(bHaveReplied)
		CEntityAreaStatusEvent::Trigger(GetScriptID(), m_NetworkID, m_bIsOccupied, &player);
}

void CNetworkEntityAreaWorldStateData::PlayerHasLeft(const netPlayer &player)
{
	// clear out the replied status
	if(gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
		m_HasReplyFrom &= ~(1 << player.GetPhysicalPlayerIndex());
}

CNetworkEntityAreaWorldStateData *CNetworkEntityAreaWorldStateData::Clone() const
{
	return rage_new CNetworkEntityAreaWorldStateData(GetScriptID(), IsLocallyOwned(), IsClientCreated(), GetClientPeerID(), m_NetworkID, m_ScriptAreaID, m_nIncludeFlags, m_AreaStart, m_AreaEnd, m_bIsAngledArea ? m_AreaWidth : -1.0f);
}

void CNetworkEntityAreaWorldStateData::GetArea(Vector3 &vStart, Vector3 &vEnd, float &fWidth) const
{
	vStart = m_AreaStart;
	vEnd = m_AreaEnd;
	fWidth = m_AreaWidth;
}

bool CNetworkEntityAreaWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
	if(CNetworkWorldStateData::IsEqualTo(worldState))
	{
		const CNetworkEntityAreaWorldStateData* pState = SafeCast(const CNetworkEntityAreaWorldStateData, &worldState);
		if(m_AreaStart.IsClose(pState->m_AreaStart, AREA_EPSILON) && m_AreaEnd.IsClose(pState->m_AreaEnd, AREA_EPSILON))
			return true;
	}

	return false;
}

void CNetworkEntityAreaWorldStateData::ChangeWorldState()
{
	m_bIsDirty = true; 

	// setup local ID for areas added by other players (so that host migration can work)
	if(m_ScriptAreaID == -1)
	{
		m_ScriptAreaID = ms_NextScriptID++;
		if(ms_NextScriptID >= MAX_NUM_NETWORK_IDS)
		{
			ms_NextScriptID = 0;
		}
	}

    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "ADDING_ENTITY_AREA", "");
    LogState(NetworkInterface::GetObjectManagerLog());

	bool bInitialiseBox = m_bIsAngledArea ? true : false;

	if (!bInitialiseBox)
	{
		// zero dimension may occur through quantisation when serialising to clients
		// fix this up, but log that it occurred
		static const float k_MINIMUM_AREA_DIMENSION = 0.1f;
		if(Abs(m_AreaStart.x - m_AreaEnd.x) <= VERY_SMALL_FLOAT)
		{
			gnetDebug1("Entity Area :: x co-ordinate for start and end are very close (%f, %f). Fixing.", m_AreaStart.x, m_AreaEnd.x);
			m_AreaEnd.SetX(m_AreaStart.x + k_MINIMUM_AREA_DIMENSION);
			bInitialiseBox = true;
		}
		if(Abs(m_AreaStart.y - m_AreaEnd.y) <= VERY_SMALL_FLOAT)
		{
			gnetDebug1("Entity Area :: y co-ordinate for start and end are very close (%f, %f). Fixing.", m_AreaStart.y, m_AreaEnd.y);
			m_AreaEnd.SetY(m_AreaStart.y + k_MINIMUM_AREA_DIMENSION);
			bInitialiseBox = true;
		}
		if(Abs(m_AreaStart.z - m_AreaEnd.z) <= VERY_SMALL_FLOAT)
		{
			gnetDebug1("Entity Area :: z co-ordinate for start and end are very close (%f, %f). Fixing.", m_AreaStart.z, m_AreaEnd.z);
			m_AreaEnd.SetZ(m_AreaStart.z + k_MINIMUM_AREA_DIMENSION);
			bInitialiseBox = true;
		}
	}

	// need to recalculate bounding box 
	if(bInitialiseBox)
		InitializeBoundingBox();
}

void CNetworkEntityAreaWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING_ENTITY_AREA", "");
    LogState(NetworkInterface::GetObjectManagerLog());
}

void CNetworkEntityAreaWorldStateData::UpdateHostState(CNetworkWorldStateData &hostWorldState)
{
	CNetworkEntityAreaWorldStateData* pState = SafeCast(CNetworkEntityAreaWorldStateData, &hostWorldState);
	pState->m_ScriptAreaID = m_ScriptAreaID;
}

int CNetworkEntityAreaWorldStateData::AddEntityArea(const CGameScriptId &scriptID,
													 const Vector3 &vStart,
													 const Vector3 &vEnd,
													 const float areaWidth,
													 const IncludeFlags nFlags,
                                                     bool bHostArea)
{
	CNetworkEntityAreaWorldStateData::Pool* pPool = CNetworkEntityAreaWorldStateData::GetPool();
	if(!gnetVerifyf(pPool, "AddEntityArea :: Pool does not exist! Not in network game!"))
		return -1;

    if(bHostArea && !CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID))
    {
        gnetAssertf(0, "AddEntityArea :: Trying to create an entity area with the host area flag set on a script client! This is not allowed!");
        return -1;
    }

	//Only need to validate x/y/z if we are not an angled area - checked by areaWidth
	if (areaWidth == -1.f)
	{
		// validate area limits
		if(!gnetVerifyf(Abs(vStart.x - vEnd.x) > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid area limits supplied. X values are equal!"))
			return -1; 
		if(!gnetVerifyf(Abs(vStart.y - vEnd.y) > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid area limits supplied. Y values are equal!"))
			return -1; 
		if(!gnetVerifyf(Abs(vStart.z - vEnd.z) > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid area limits supplied. Z values are equal!"))
			return -1;

		// log out when entity area bounds are very close
		static const float k_MINIMUM_AREA_DIMENSION = 0.1f;
		if(Abs(vStart.x - vEnd.x) <= k_MINIMUM_AREA_DIMENSION)
			gnetDebug1("AddEntityArea :: x co-ordinate for start and end are very close (%f, %f)", vStart.y, vEnd.y);
		if(Abs(vStart.y - vEnd.y) <= k_MINIMUM_AREA_DIMENSION)
			gnetDebug1("AddEntityArea :: y co-ordinate for start and end are very close (%f, %f)", vStart.y, vEnd.y);
		if(Abs(vStart.z - vEnd.z) <= k_MINIMUM_AREA_DIMENSION)
			gnetDebug1("AddEntityArea :: z co-ordinate for start and end are very close (%f, %f)", vStart.z, vEnd.z);
	}
	else
	{
		float tempLength = (vEnd - vStart).XYMag();
		float tempZ = (vEnd.z - vStart.z);
		static const float k_MINIMUM_AREA_DIMENSION = 0.1f;

		// validate area limits
		if(!gnetVerifyf(tempLength > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid angled area limits supplied. too small (vEnd - vStart).XYMag() !"))
			return -1; 
		if(!gnetVerifyf(fabs(tempZ) > VERY_SMALL_FLOAT, "AddEntityArea :: Invalid angled area limits supplied. too small  (vEnd.z - vStart.z) !"))
			return -1;

		// log out when entity area bounds are very close
		if(tempLength <= k_MINIMUM_AREA_DIMENSION)
			gnetDebug1("AddEntityArea :: angled area limits supplied.  (vEnd - vStart).XYMag() (vStart %f,%f,%f and vEnd %f,%f,%f) are very close ", vStart.x,vStart.y,vStart.z, vEnd.x,vEnd.y,vEnd.z);
		if(fabs(tempZ) <= k_MINIMUM_AREA_DIMENSION)
			gnetDebug1("AddEntityArea :: angled area limits supplied.  (vEnd.z - vStart.z) are very close (%f, %f)", vStart.z, vEnd.z);
	}



	// validate area width
	if (!gnetVerifyf( ((areaWidth == -1.f) || (areaWidth > 0.f)), "AddEntityArea :: Invalid area width %f", areaWidth))
		return -1;

	// allocate a new script ID (irrelevant if it's used or not)
	int scriptAreaID = ms_NextScriptID++;
	if(ms_NextScriptID >= MAX_NUM_NETWORK_IDS)
	{
		ms_NextScriptID = 0;
	}

    bool clientCreated = !bHostArea;
    u64  clientPeerID  = 0;

    netPlayer *localPlayer = netInterface::GetLocalPlayer();

    if(clientCreated && localPlayer)
    {
        clientPeerID = localPlayer->GetRlPeerId();
    }

	// our script data
	CNetworkEntityAreaWorldStateData worldStateData(scriptID, true, clientCreated, clientPeerID, -1, scriptAreaID, nFlags, vStart, vEnd, areaWidth);

	// check if a duplicate exists
	if(!CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, false))
	{
		// we need to find the existing script ID
		s32 poolIndex = pPool->GetSize();
		while(poolIndex--)
		{
			CNetworkEntityAreaWorldStateData* pState = pPool->GetSlot(poolIndex);
			if(pState && worldStateData.IsEqualTo(*pState))
				return pState->m_ScriptAreaID;
		}
		gnetAssertf(0, "Duplicate not found in pool!");
	}

	// use the newly allocated ID
	return worldStateData.m_ScriptAreaID;
}

bool CNetworkEntityAreaWorldStateData::RemoveEntityArea(const CGameScriptId &scriptID, int scriptAreaID)
{
	if(scriptAreaID < 0)
		return false;
	
	CNetworkEntityAreaWorldStateData::Pool* pPool = CNetworkEntityAreaWorldStateData::GetPool();
	if(!pPool)
		return false;

	// look up world state from the area ID
	s32 poolIndex = pPool->GetSize();
	while(poolIndex--)
	{
		CNetworkEntityAreaWorldStateData* pState = pPool->GetSlot(poolIndex);
		
		if (pState)
		{
			const int LOG_NAME_LENGTH = 100;

			char logName1[LOG_NAME_LENGTH];
			char logName2[LOG_NAME_LENGTH];

			safecpy(logName1, scriptID.GetLogName(), LOG_NAME_LENGTH);
			safecpy(logName2, pState->GetScriptID().GetLogName(), LOG_NAME_LENGTH);

			if(pState->m_ScriptAreaID == scriptAreaID && gnetVerifyf(scriptID == pState->GetScriptID(), "Unexpected script ID! Expected (%s) but pState->GetScriptID() is (%s)", logName1, logName2))
			{
				CNetworkScriptWorldStateMgr::RevertWorldState(*pState, false);
				return true; 
			}
		}
		
	}
	// area not found
	return false;
}

bool CNetworkEntityAreaWorldStateData::UpdateOccupiedState(const CGameScriptId &scriptID, const int networkID, const PhysicalPlayerIndex playerID, bool bIsOccupied)
{
	bool bFoundArea = false; 

	if(gnetVerifyf(networkID != -1, "Invalid network ID!"))
	{
		CNetworkEntityAreaWorldStateData::Pool* pPool = CNetworkEntityAreaWorldStateData::GetPool();
		s32 poolIndex = pPool->GetSize();
		while(poolIndex--)
		{
			CNetworkEntityAreaWorldStateData* pState = pPool->GetSlot(poolIndex);
			if(pState && (pState->GetScriptID() == scriptID) && (pState->m_NetworkID == networkID))
			{
				// different tutorial session..? we still want to acknowledge reply but set occupied status to false
				CNetGamePlayer* pRemotePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerID);
				if(pRemotePlayer && pRemotePlayer->IsInDifferentTutorialSession())
					bIsOccupied = false;

				// mark state in occupied bitfield
				if(bIsOccupied)
					pState->m_IsOccupied |= (1 << playerID);
				else
					pState->m_IsOccupied &= ~(1 << playerID);

				// update reply status
				pState->m_HasReplyFrom |= (1 << playerID);

				// mark that we've found an area - we check all areas as some areas may be cloned for events
				bFoundArea = true;
			}
		}

		if(!bFoundArea)
			gnetDebug1("Couldn't find entity area. Might not know about it yet. Network ID: %d scriptID: %s", networkID, scriptID.GetLogName());
	}

	return bFoundArea;
}

CNetworkEntityAreaWorldStateData* CNetworkEntityAreaWorldStateData::GetWorldState(const CGameScriptId &scriptID, const int networkID)
{
	if(networkID < 0)
		return NULL;

	CNetworkEntityAreaWorldStateData::Pool* pPool = CNetworkEntityAreaWorldStateData::GetPool();
	if(!pPool)
		return NULL;
	
	s32 poolIndex = pPool->GetSize();
	while(poolIndex--)
	{
		CNetworkEntityAreaWorldStateData* pState = pPool->GetSlot(poolIndex);
		if(pState && (pState->GetScriptID() == scriptID) && (pState->m_NetworkID == networkID))
			return pState;
	}

	return NULL;
}

CNetworkEntityAreaWorldStateData* CNetworkEntityAreaWorldStateData::GetWorldState(int scriptAreaID)
{
	if(scriptAreaID < 0)
		return NULL;

	CNetworkEntityAreaWorldStateData::Pool* pPool = CNetworkEntityAreaWorldStateData::GetPool();
	if(!pPool)
		return NULL;

	// look up world state from the area ID
	s32 poolIndex = pPool->GetSize();
	while(poolIndex--)
	{
		CNetworkEntityAreaWorldStateData* pState = pPool->GetSlot(poolIndex);
		if(pState && pState->m_ScriptAreaID == scriptAreaID)
			return pState;
	}
	return NULL;
}

bool CNetworkEntityAreaWorldStateData::HasWorldState(int scriptAreaID)
{
	return GetWorldState(scriptAreaID) != NULL;
}

bool CNetworkEntityAreaWorldStateData::HaveAllReplied(int scriptAreaID)
{
	// look up world state from the area ID
	CNetworkEntityAreaWorldStateData* pState = GetWorldState(scriptAreaID);
	if(!pState)
		return false;

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		if((pState->m_HasReplyFrom & (1 << remotePlayer->GetPhysicalPlayerIndex())) == 0)
			return false;
	}

	// all players have replied
	return true;
}

bool CNetworkEntityAreaWorldStateData::HasReplyFrom(int scriptAreaID, const PhysicalPlayerIndex playerID)
{
	// look up world state from the area ID
	CNetworkEntityAreaWorldStateData* pState = GetWorldState(scriptAreaID);
	if(!pState)
		return false;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerID));
	if(pPlayer)
		return ((pState->m_HasReplyFrom & (1 << pPlayer->GetPhysicalPlayerIndex())) != 0);

	// player not active
	return false;
}

bool CNetworkEntityAreaWorldStateData::IsOccupied(int scriptAreaID)
{
	// look up world state from the area ID
	CNetworkEntityAreaWorldStateData* pState = GetWorldState(scriptAreaID);
	if(!pState)
		return false;

	return (pState->m_IsOccupied != 0);
}

bool CNetworkEntityAreaWorldStateData::IsOccupiedOn(int scriptAreaID, const PhysicalPlayerIndex playerID)
{
	// look up world state from the area ID
	CNetworkEntityAreaWorldStateData* pState = GetWorldState(scriptAreaID);
	if(!pState)
		return false;
	
	return ((pState->m_IsOccupied & (1 << playerID)) != 0);
}

void CNetworkEntityAreaWorldStateData::LogState(netLoggingInterface &log) const
{
	log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
	log.WriteDataValue("Network ID", "%d", m_NetworkID);
	log.WriteDataValue("Local ID", "%d", m_ScriptAreaID);
    log.WriteDataValue("Client Created", "%s", m_bClientCreated ? "TRUE" : "FALSE");

    if(m_bClientCreated)
    {
        log.WriteDataValue("Client PeerID", "%u", m_ClientPeerID);
    }

	log.WriteDataValue("Include Flags", "%d", m_nIncludeFlags);
	log.WriteDataValue("Occupied Field", "%d", m_IsOccupied);
	log.WriteDataValue("Replied Field", "%d", m_HasReplyFrom);
    log.WriteDataValue("Physical Player Bitmask", "%d", NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask());
	log.WriteDataValue("Area Start", "%.2f, %.2f, %.2f", m_AreaStart.x, m_AreaStart.y, m_AreaStart.z);
	log.WriteDataValue("Area End", "%.2f, %.2f, %.2f", m_AreaEnd.x, m_AreaEnd.y, m_AreaEnd.z);
}

void CNetworkEntityAreaWorldStateData::Serialise(CSyncDataReader &serialiser)
{
	SerialiseWorldState(serialiser);
}

void CNetworkEntityAreaWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
	SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkEntityAreaWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
	const unsigned SIZEOF_NETWORK_ID = datBitsNeeded<MAX_NUM_NETWORK_IDS>::COUNT + 1;
	const unsigned SIZEOF_FLAGS = 3;
	const unsigned SIZEOF_AREA_WIDTH = 16;

	GetScriptID().Serialise(serialiser);
	SERIALISE_INTEGER(serialiser, m_NetworkID, SIZEOF_NETWORK_ID);
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_nIncludeFlags), SIZEOF_FLAGS);
	SERIALISE_POSITION(serialiser, m_AreaStart);
	SERIALISE_POSITION(serialiser, m_AreaEnd);
	
    SERIALISE_BOOL(serialiser, m_bClientCreated);

    if(m_bClientCreated)
    {
        SERIALISE_UNSIGNED(serialiser, m_ClientPeerID, 64);
    }

	SERIALISE_BOOL(serialiser, m_bIsAngledArea);
	if (m_bIsAngledArea || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser,m_AreaWidth,1000.f,SIZEOF_AREA_WIDTH);
	}
	else
	{
		m_AreaWidth = -1.0f;
	}
}

void CNetworkEntityAreaWorldStateData::PostSerialise()
{
	// setup network ID
	if(m_NetworkID == -1)
	{
		bool isHostOfScript = CTheScripts::GetScriptHandlerMgr().IsNetworkHost(GetScriptID());
		if(isHostOfScript)
		{
			m_NetworkID = ms_NextNetworkID++;
			gnetDebug2("CNetworkEntityAreaWorldStateData::PostSerialise() script %s m_NetworkID is now %d for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_NetworkID, m_ScriptAreaID);

			
			gnetAssert(m_NetworkID >= 0 && m_NetworkID < MAX_NUM_NETWORK_IDS);
			if(ms_NextNetworkID >= MAX_NUM_NETWORK_IDS)
			{
				ms_NextNetworkID = 0;
			}
		}
		else
		{
			gnetDebug2("CNetworkEntityAreaWorldStateData::PostSerialise() Not host of script %s! m_NetworkID will remain -1 for m_ScriptAreaID %d !", GetScriptID().GetLogName(), m_ScriptAreaID);
		}
	}
}

#if __DEV

bool CNetworkEntityAreaWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
	// we shouldn't have two areas with the same limits
	return IsEqualTo(worldState);
}

#endif // __DEV

#if __BANK

static Vector3 gs_AreaStartBank;
static Vector3 gs_AreaEndBank;
static float   gs_AreaWidthBank;
static int gs_AreaID = 0;

static void NetworkBank_AddEntityArea()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			gs_AreaStartBank = vPlayerPosition - Vector3(5.0f, 5.0f, 1.0f);
			gs_AreaEndBank = vPlayerPosition + Vector3(5.0f, 5.0f, 4.0f);
			gs_AreaWidthBank = -1.0f;

			gs_AreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(scriptID, gs_AreaStartBank, gs_AreaEndBank, gs_AreaWidthBank, CNetworkEntityAreaWorldStateData::eInclude_Default, CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID));
		}
	}
}

static void NetworkBank_AddEntityAngledArea()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			Vector3 vForwardVector = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());

			vForwardVector.Normalize();
			Vector3 vStart = vForwardVector * -5.f; vStart = vPlayerPosition + vStart; vStart.z = vPlayerPosition.z - 1.0f;
			Vector3 vEnd   = vForwardVector * 5.f; vEnd = vPlayerPosition + vEnd; vEnd.z = vPlayerPosition.z + 4.0f;

			gs_AreaStartBank = vStart;
			gs_AreaEndBank = vEnd;
			gs_AreaWidthBank = 20.f;

			gs_AreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(scriptID, gs_AreaStartBank, gs_AreaEndBank, gs_AreaWidthBank, CNetworkEntityAreaWorldStateData::eInclude_Default, CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID));
		}
	}
}

static void NetworkBank_AddEntityAngledAreaSameXYZ()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			Vector3 vForwardVector = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());

			vForwardVector.Normalize();
			Vector3 vStart = vPlayerPosition;
			Vector3 vEnd   = vPlayerPosition;

			gs_AreaStartBank = vStart;
			gs_AreaEndBank = vEnd;
			gs_AreaWidthBank = 20.f;

			gs_AreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(scriptID, gs_AreaStartBank, gs_AreaEndBank, gs_AreaWidthBank, CNetworkEntityAreaWorldStateData::eInclude_Default, CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID));
		}
	}
}

static void NetworkBank_AddEntityAngledAreaSameY()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			Vector3 vForwardVector = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());

			vForwardVector.Normalize();
			Vector3 vStart = vForwardVector * -5.f; vStart = vPlayerPosition + vStart; vStart.z = vPlayerPosition.z - 1.0f;
			Vector3 vEnd   = vForwardVector * 5.f; vEnd = vPlayerPosition + vEnd; vEnd.z = vPlayerPosition.z + 4.0f;

			vStart.y = vPlayerPosition.y;
			vEnd.y = vPlayerPosition.y;

			gs_AreaStartBank = vStart;
			gs_AreaEndBank = vEnd;
			gs_AreaWidthBank = 20.f;

			gs_AreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(scriptID, gs_AreaStartBank, gs_AreaEndBank, gs_AreaWidthBank, CNetworkEntityAreaWorldStateData::eInclude_Default, CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID));
		}
	}
}

static void NetworkBank_AddEntityAngledAreaSameZ()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			Vector3 vForwardVector = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());

			vForwardVector.Normalize();
			Vector3 vStart = vForwardVector * -5.f; vStart = vPlayerPosition + vStart; vStart.z = vPlayerPosition.z - 1.0f;
			Vector3 vEnd   = vForwardVector * 5.f; vEnd = vPlayerPosition + vEnd; vEnd.z = vPlayerPosition.z + 4.0f;

			vStart.z = vPlayerPosition.z;
			vEnd.z = vPlayerPosition.z;

			gs_AreaStartBank = vStart;
			gs_AreaEndBank = vEnd;
			gs_AreaWidthBank = 20.f;

			gs_AreaID = CNetworkEntityAreaWorldStateData::AddEntityArea(scriptID, gs_AreaStartBank, gs_AreaEndBank, gs_AreaWidthBank, CNetworkEntityAreaWorldStateData::eInclude_Default, CTheScripts::GetScriptHandlerMgr().IsNetworkHost(scriptID));
		}
	}
}

static void NetworkBank_RemoveEntityArea()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayerPed = FindFollowPed();
		if(pPlayerPed)
		{
			const char* scriptname = NetworkInterface::IsInFreeMode() ? "freemode" : "mptestbed";
			CGameScriptId scriptID(scriptname, -1);
			CNetworkEntityAreaWorldStateData::RemoveEntityArea(scriptID, gs_AreaID);
		}
	}
}

static void NetworkBank_ToggleDrawEntityAreas()
{
#if __BANK
	CNetworkEntityAreaWorldStateData::gs_bDrawEntityAreas = !CNetworkEntityAreaWorldStateData::gs_bDrawEntityAreas;
#endif
}

void CNetworkEntityAreaWorldStateData::AddDebugWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Network");

	if(gnetVerifyf(pBank, "Unable to find network bank!"))
	{
		pBank->PushGroup("Entity Areas", false);
		{
			pBank->AddButton("Add an AA Entity Area Around Player", datCallback(NetworkBank_AddEntityArea));
			pBank->AddButton("Add an Angled Entity Area Around Player", datCallback(NetworkBank_AddEntityAngledArea));
			pBank->AddButton("Add an Angled Entity Area Around Player (same xyz)", datCallback(NetworkBank_AddEntityAngledAreaSameXYZ));
			pBank->AddButton("Add an Angled Entity Area Around Player (same y)", datCallback(NetworkBank_AddEntityAngledAreaSameY));
			pBank->AddButton("Add an Angled Entity Area Around Player (same z)", datCallback(NetworkBank_AddEntityAngledAreaSameZ));
			pBank->AddButton("Remove last created Entity Area", datCallback(NetworkBank_RemoveEntityArea));
			pBank->AddButton("Toggle Draw Entity Areas (Dev Only)", datCallback(NetworkBank_ToggleDrawEntityAreas));
		}
		pBank->PopGroup();
	}
}

void CNetworkEntityAreaWorldStateData::DisplayDebugText()
{
	CNetworkEntityAreaWorldStateData::Pool *pool = CNetworkEntityAreaWorldStateData::GetPool();

	s32 i = pool->GetSize();
	while(i--)
	{
		CNetworkEntityAreaWorldStateData *worldState = pool->GetSlot(i);
		if(worldState)
		{
			Vector3 vStart, vEnd;
			float fWidth = 0.f;
			worldState->GetArea(vStart, vEnd, fWidth);
			grcDebugDraw::AddDebugOutput("%sEntity Area (%s): NetID: %d, LocalID: %d, Area: (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f), Width: %d, Flags: %d, Occupied: %s, Anywhere: %s (%d), Replied: %d",
                worldState->m_bClientCreated ? "Client " : "",
				worldState->GetScriptID().GetLogName(),
				worldState->m_NetworkID,
				worldState->m_ScriptAreaID,
				vStart.x, vStart.y, vStart.z,
				vEnd.x, vEnd.y, vEnd.z,
				fWidth,
				worldState->m_nIncludeFlags,
				worldState->m_bIsOccupied ? "true" : "false",
				worldState->m_IsOccupied != 0 ? "true" : "false",
				worldState->m_IsOccupied,
				worldState->m_HasReplyFrom);
		}
	}
}
#endif // __BANK
