
#include "Control\replay\ReplayInterfaceObj.h"
#include "Control/replay/Entity/DestroyedMapObjectsPacket.h"

#if GTA_REPLAY

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketDestroyedMapObjectsForClipStart.																					//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CPacketDestroyedMapObjectsForClipStart::Store(atArray <DestroyedMapObjectsManager::DestuctionRecord> &destructionRecords)
{
	CPacketMapIDList::Store(destructionRecords, PACKETID_DESTROYED_MAP_OBJECTS_FOR_CLIP_START, sizeof(CPacketDestroyedMapObjectsForClipStart)); 
}


void CPacketDestroyedMapObjectsForClipStart::Extract(class CReplayInterfaceObject *pObjectInterface) const
{
	u32 *pSrc = GetMapIDs();

	for(u32 i=0; i<m_NoOfMapIDs; i++)
		pObjectInterface->AddDeferredMapObjectToHide(mapObjectID(pSrc[i]));
}


//////////////////////////////////////////////////////////////////////////
void CPacketDestroyedMapObjectsForClipStart::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	u32 *pSrc = GetMapIDs();

	char str[1024];
	snprintf(str, 1024, "\t\t<CPacketDestroyedMapObjectsForClipStart>");
	CFileMgr::Write(handle, str, istrlen(str));

	for(u32 i=0; i<m_NoOfMapIDs; i++)
	{
		snprintf(str, 1024, "%u ", pSrc[i]);
		CFileMgr::Write(handle, str, istrlen(str));
	}

	snprintf(str, 1024, "</CPacketDestroyedMapObjectsForClipStart>\n");
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketBriefLifeTimeDestroyedMapObjects.																					//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CPacketBriefLifeTimeDestroyedMapObjects::Store(atArray <DestroyedMapObjectsManager::DestuctionRecord> &destructionRecords)
{
	CPacketMapIDList::Store(destructionRecords, PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS, sizeof(CPacketBriefLifeTimeDestroyedMapObjects)); 
}


void CPacketBriefLifeTimeDestroyedMapObjects::Extract(class CReplayInterfaceObject *pObjectInterface, bool isBackWards) const
{
	u32 *pMapIds = GetMapIDs();

	if(isBackWards)
	{
		CReplayID invalidReplayID;

		for(int i=0; i<m_NoOfMapIDs; i++)
			pObjectInterface->ResolveMapIDAndAndUnHide(pMapIds[i], invalidReplayID);
	}
	else
	{
		for(int i=0; i<m_NoOfMapIDs; i++)
			pObjectInterface->AddDeferredMapObjectToHide(pMapIds[i]);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketBriefLifeTimeDestroyedMapObjects::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	u32 *pSrc = GetMapIDs();

	char str[1024];
	snprintf(str, 1024, "\t\t<CPacketBriefLifeTimeDestroyedMapObjects>");
	CFileMgr::Write(handle, str, istrlen(str));

	for(u32 i=0; i<m_NoOfMapIDs; i++)
	{
		snprintf(str, 1024, "%u ", pSrc[i]);
		CFileMgr::Write(handle, str, istrlen(str));
	}

	snprintf(str, 1024, "</CPacketBriefLifeTimeDestroyedMapObjects>\n");
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DestroyedMapObjectsManager.																								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DestroyedMapObjectsManager::DestroyedMapObjectsManager() 
{

}

DestroyedMapObjectsManager::~DestroyedMapObjectsManager()
{

}

void DestroyedMapObjectsManager::Init(u32 initialSize, int briefLifeTimeSize)
{
	m_DestroyedMapObjectsForClipStart.Reserve(initialSize);

	for(u32 i=0; i<initialSize; i++)
	{
		m_DestroyedMapObjectsForClipStart.Append();
		m_DestroyedMapObjectsForClipStart[i].mapID = 0;
		m_DestroyedMapObjectsForClipStart[i].frameRef = 0;
	}

	m_BriefLifeTimeMapObjects.Reserve(briefLifeTimeSize);
	m_BriefLifeTimeMapObjects.ResetCount();
}

void DestroyedMapObjectsManager::Update(Vec3V &position)
{
	(void)position;

	// TODO:- Evict from list based upon distance as per the game.
}


void DestroyedMapObjectsManager::MakeRecordOfDestruction(CObject *pObject)
{
	// Don't update if we're in playback
	if( CReplayMgr::IsEditModeActive() )
		return;

	if(pObject->GetHash() != REPLAY_INVALID_OBJECT_HASH)
	{
		// Does this id already exist?
		for(s32 i=0;i<m_DestroyedMapObjectsForClipStart.size();i++)
		{
			if( m_DestroyedMapObjectsForClipStart[i].frameRef != 0 && m_DestroyedMapObjectsForClipStart[i].mapID == pObject->GetHash() )
			{
				// Already exists, update it's ref and early out.
				m_DestroyedMapObjectsForClipStart[i].frameRef = fwTimer::GetFrameCount();
				return;
			}
		}

		// New entry required
		// Find the lowest frameRef and overwrite
		s32 IDX = -1;
		u32	lowestRef = 0x7fffffff;
		for(s32 i=0;i<m_DestroyedMapObjectsForClipStart.size();i++)
		{
			if( m_DestroyedMapObjectsForClipStart[i].frameRef < lowestRef )
			{
				IDX = i;
				lowestRef = m_DestroyedMapObjectsForClipStart[i].frameRef;
				if( lowestRef == 0 )
				{
					break;		// Early out on empty
				}
			}
		}
		if( IDX >= 0 )
		{
			m_DestroyedMapObjectsForClipStart[IDX].mapID = pObject->GetHash();
			m_DestroyedMapObjectsForClipStart[IDX].frameRef = fwTimer::GetFrameCount();

			// We'll store the position and de-populate based up distance from player later.
	//		m_DestroyedMapObjects[m_NextFree].position[0] = pObject->GetTransform().GetPosition().GetXf();
	//		m_DestroyedMapObjects[m_NextFree].position[1] = pObject->GetTransform().GetPosition().GetYf();
	//		m_DestroyedMapObjects[m_NextFree].position[2] = pObject->GetTransform().GetPosition().GetZf();
		}
	}
}

void DestroyedMapObjectsManager::DeleteRecordOfDestruction(CObject *pObject)
{
	// Don't update if we're in playback
	if( CReplayMgr::IsEditModeActive() )
		return;

	if(pObject->GetHash() != REPLAY_INVALID_OBJECT_HASH)
	{
		// Find this map ID and remove it from the list
		for(int i=0;i<m_DestroyedMapObjectsForClipStart.size();i++)
		{
			if( m_DestroyedMapObjectsForClipStart[i].mapID == pObject->GetHash() )
			{
				m_DestroyedMapObjectsForClipStart[i].frameRef = 0;
				break;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void DestroyedMapObjectsManager::AddBriefLifeTimeMapObject(class CObject *pObject)
{
	if(m_BriefLifeTimeMapObjects.GetCount() == m_BriefLifeTimeMapObjects.GetCapacity())
	{
		replayWarningf("DestroyedMapObjectsManager::AddBriefLifeTimeMapObject()...m_BriefLifeTimeMapObjects array full- Dropping an object.");
		return;
	}
	DestuctionRecord &destructionRecord = m_BriefLifeTimeMapObjects.Append();
	destructionRecord.mapID = pObject->GetHash();
	destructionRecord.frameRef = 1; // Might need these later, force to 1 now so that test in CPacketMapIDList::Store() passes.
}

#endif // GTA_REPLAY

