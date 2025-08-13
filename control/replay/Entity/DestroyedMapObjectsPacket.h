#ifndef INC_DESTROYED_MAP_OBJECTS_PACKET_H_
#define INC_DESTROYED_MAP_OBJECTS_PACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/InterpPacket.h"
#include "Control/Replay/PacketBasics.h"
#include "Control/replay/ReplayPacketIDs.h"

//////////////////////////////////////////////////////////////////////////
class DestroyedMapObjectsManager
{
public:
	typedef struct DestuctionRecord
	{
		u32 mapID;
		u32	frameRef;
		//		float position[3];		// Removed, add back if functionality requiring it is needed
	} DestuctionRecord;
public:
	DestroyedMapObjectsManager(); 
	~DestroyedMapObjectsManager();

public:
	void Init(u32 initialSize, int briefLifeTimeSize);
	void Update(Vec3V &position);
	void MakeRecordOfDestruction(class CObject *pObject);
	void DeleteRecordOfDestruction(class CObject *pObject);
	atArray <DestuctionRecord> &GetDestructionRecordsForClipStart() { return m_DestroyedMapObjectsForClipStart; };

	void AddBriefLifeTimeMapObject(class CObject *pObject);
	u32 GetNoOfBriefLifeTimeMapObjects() { return m_BriefLifeTimeMapObjects.GetCount(); }
	atArray <DestuctionRecord> &GetDestructionRecordsForBriefLifeTimeMapObjects(){ return m_BriefLifeTimeMapObjects; }
	void ResetBriefLifeTimeMapObjects() { m_BriefLifeTimeMapObjects.ResetCount(); }
private:
	atArray <DestuctionRecord> m_DestroyedMapObjectsForClipStart;
	atArray <DestuctionRecord> m_BriefLifeTimeMapObjects;

	friend class CPacketMapIDList;
	friend class CPacketDestroyedMapObjectsForClipStart;
	friend class CPacketBriefLifeTimeDestroyedMapObjects;
};


//////////////////////////////////////////////////////////////////////////
class CPacketMapIDList : public CPacketBase
{
public:
	void 	Store(atArray <DestroyedMapObjectsManager::DestuctionRecord> &destructionRecords, eReplayPacketId packetId, tPacketSize packetSize)
	{
		CPacketBase::Store(packetId, packetSize);

		m_NoOfMapIDs = 0;
		u32 *pDest = GetMapIDs();

		for(u32 i=0; i<destructionRecords.GetCount(); i++)
		{
			if(destructionRecords[i].frameRef != 0)
				pDest[m_NoOfMapIDs++] = destructionRecords[i].mapID;
		}

		AddToPacketSize(sizeof(u32)*m_NoOfMapIDs);
	}

	u32 *GetMapIDs() const { return (u32 *)((u8 *)this + GetPacketSize() - sizeof(u32)*m_NoOfMapIDs);	}

protected:
	u32 m_NoOfMapIDs;
};


//////////////////////////////////////////////////////////////////////////
class CPacketDestroyedMapObjectsForClipStart : public CPacketMapIDList
{
public:
	void 	Store(atArray <DestroyedMapObjectsManager::DestuctionRecord> &destructionRecords);
	void	StoreExtensions(atArray <DestroyedMapObjectsManager::DestuctionRecord> &) { PACKET_EXTENSION_RESET(CPacketDestroyedMapObjectsForClipStart); }
	void 	Extract(class CReplayInterfaceObject *pObjectInterface) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DESTROYED_MAP_OBJECTS_FOR_CLIP_START, "Validation of CPacketDestroyedMapObjectsForClipStart Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketDestroyedMapObjectsForClipStart), "Validation of CPacketDestroyedMapObjectsForClipStart packet extension Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_DESTROYED_MAP_OBJECTS_FOR_CLIP_START && VALIDATE_PACKET_EXTENSION(CPacketDestroyedMapObjectsForClipStart) && CPacketBase::ValidatePacket();
	}

	void	PrintXML(FileHandle handle) const;

protected:
	DECLARE_PACKET_EXTENSION(CPacketDestroyedMapObjectsForClipStart);
};


//////////////////////////////////////////////////////////////////////////
class CPacketBriefLifeTimeDestroyedMapObjects : public CPacketMapIDList
{
public:
	void 	Store(atArray <DestroyedMapObjectsManager::DestuctionRecord> &destructionRecords);
	void	StoreExtensions(atArray <DestroyedMapObjectsManager::DestuctionRecord> &) { PACKET_EXTENSION_RESET(CPacketBriefLifeTimeDestroyedMapObjects); }
	void 	Extract(class CReplayInterfaceObject *pObjectInterface, bool isBackWards) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS, "Validation of CPacketBriefLifeTimeDestroyedMapObjects Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketBriefLifeTimeDestroyedMapObjects), "Validation of CPacketBriefLifeTimeDestroyedMapObjects packet extension Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS && VALIDATE_PACKET_EXTENSION(CPacketBriefLifeTimeDestroyedMapObjects) && CPacketBase::ValidatePacket();
	}

	void	PrintXML(FileHandle handle) const;

protected:
	DECLARE_PACKET_EXTENSION(CPacketBriefLifeTimeDestroyedMapObjects);
};


#endif // GTA_REPLAY

#endif // INC_DESTROYED_MAP_OBJECTS_PACKET_H_
