#ifndef REPLAYINTERFACEOBJ_H
#define REPLAYINTERFACEOBJ_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayInterface.h"
#include "Objects/object.h"
#include "Entity/ObjectPacket.h"
#include "Entity/DestroyedMapObjectsPacket.h"

#include "shaders/CustomShaderEffectWeapon.h"
#include "shaders/CustomShaderEffectProp.h"
#include "shaders/CustomShaderEffectTint.h"


#define OBJPACKETDESCRIPTOR(id, et, type, recFunc) CPacketDescriptor<CObject>(id, et, sizeof(type), #type, \
	&iReplayInterface::template PrintOutPacketDetails<type>,\
	recFunc,\
	NULL)

template<>
class CReplayInterfaceTraits<CObject>
{
public:
	typedef CPacketObjectCreate	tCreatePacket;
	typedef CPacketObjectUpdate	tUpdatePacket;
	typedef CPacketObjectDelete	tDeletePacket;
	typedef CObjInterp			tInterper;
	typedef CObject::Pool		tPool;

	typedef deletionData<MAX_OBJECT_DELETION_SIZE>	tDeletionData;

	static const char*	strShort;
	static const char*	strLong;
	static const char*	strShortFriendly;
	static const char*	strLongFriendly;

	static const int	maxDeletionSize;
};



class CReplayInterfaceObject
	: public CReplayInterface<CObject>
{
public:
	CReplayInterfaceObject();
	~CReplayInterfaceObject(){}

	using CReplayInterface<CObject>::AddPacketDescriptor;

	template<typename PACKETTYPE, typename VEHICLESUBTYPE>
	void		AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 vehicleType);

	void		ClearWorldOfEntities();

	bool		IsRelevantUpdatePacket(eReplayPacketId packetID) const
	{
		return (packetID > PACKETID_OBJECTCREATE && packetID < PACKETID_OBJECTDELETE);
	}

	bool		IsEntityUpdatePacket(eReplayPacketId packetID) const	{	return packetID >= PACKETID_OBJECTUPDATE && packetID <= PACKETID_OBJECTDOORUPDATE;	}

	void		PreRecordFrame(CReplayRecorder&, CReplayRecorder& );

	bool		RecordElement(CObject* pElem, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex);

#if REPLAY_DELAYS_QUANTIZATION
	void		PostRecordOptimize(CPacketBase* pPacket) const;
	void		ResetPostRecordOptimizations() const;

	bool		IsRelevantOptimizablePacket(eReplayPacketId packetID) const	{	return packetID == PACKETID_ANIMATEDOBJDATA;	}
#endif // REPLAY_DELAYS_QUANTIZATION

	CObject*	CreateElement(CReplayInterfaceTraits<CObject>::tCreatePacket const* pPacket, const CReplayState& state);

	void		AddReplayEntityToWorld(CEntity *pEntity, fwInteriorLocation interiorLocation);

	void		CreateBatchedEntities(s32& createdSoFar, const CReplayState& state);

	void		PlaybackSetup(ReplayController& controller);
	void		PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist);
	void		PlayDeletePacket(ReplayController& controller);
	void		PlayPacket(ReplayController& controller);

	void		OnDelete(CPhysical* pEntity);

	bool		MatchesType(const CEntity* pEntity) const;

	void		PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

	bool		TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason));

	void		RecordMapObject(CObject* pObject);
	void		StopRecordingMapObject(CObject* pObject);
	bool		IsRecordingMapObject(CObject* pObject) const;
	void		RecordObject(CObject* pObject);
	void		StopRecordingObject(CObject* pObject);

	bool		AllowObjectPhysicsOverride()		{	return m_allowPhysicsOverride;	}

	bool		GetBlockDetails(char* pString, bool& err, bool recording) const;

	void		PostProcess();

	void		ProcessDeferredMapObjectToHide();

	bool		ShouldRegisterElement(const CEntity* /*pEntity*/) const	{ return true; }	// All objects should always be registered on creation
	bool		ShouldRecordElement(const CObject* pObject) const;

public:
	int			AddDeferredMapObjectToHide(mapObjectID mapObjId, CReplayID replayID = ReplayIDInvalid, FrameRef frameRef = FrameRef());
	void		AddDeferredMapObjectToHideWithMoveToInterior(mapObjectID mapObjId, CObject *pObject, fwInteriorLocation interiorLocation);
	void		AddDeferredMapObjectToHideWithMoveToInterior(CObject *pObject, fwInteriorLocation interiorLocation);
	void		AddObjectsToWorldPostMapObjectHide(s32 addStamp, CObject *pMapObject);
	bool		DoesObjectExist(CObject *pObject);

public:
	void		AddMoveToInteriorLocation(CObject *pObject, fwInteriorLocation interiorLocation);

public:
	DestroyedMapObjectsManager *GetDestroyedMapObjectsManager() { return &m_DestroyedMapObjects; }

private:
	u32			GetUpdatePacketSize(CObject* pObj) const;

	u32			GetFragDataSize(const CEntity* pEntity) const;
	u32			GetFragBoneDataSize(const CEntity* pEntity) const;

	void		RecordFragData(CEntity* pEntity, CReplayRecorder& recorder);
	void		RecordFragBoneData(CEntity* pEntity, CReplayRecorder& recorder);
	void		RecordBoneData(CEntity* pEntity, CReplayRecorder& recorder);
	bool		ShouldRecordBoneData(CObject* pEntity);

	void		ExtractObjectType(CObject* pObject, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller);

	bool		RemoveElement(CObject* pElem, const CPacketObjectDelete* pDeletePacket, bool isRewinding = true);
	void		RemoveElementAnimatedBuildings(CObject* pMapObject);
	
	void		UnhideMapObject(CObject* pMapObject);

	void		SetFragParent(CObject* pObject, CEntity* pParent) const;
public:
	void		ResolveMapIDAndAndUnHide(u32 mapHashID, CReplayID const &replayID);
private:
	void		ProcessRewindDeletionWithoutEntity(CPacketObjectCreate const *pCreatePacket, CReplayID const &replayID);

private:
	void		SetHideRecordForMapObject(u32 hash, CReplayID replayID, FrameRef frameRef);
	void		RemoveHideRecordForMapObject(u32 hash, CReplayID replayID);
	bool		IsMapObjectExpectedToBeHidden(u32 hash);
#if __BANK
public:
	bool		IsMapObjectExpectedToBeHidden(u32 hash, CReplayID& replayID, FrameRef& frameRef);
private:
#endif // __BANK

public:
	bool		ShouldRelevantPacketBeProcessedDuringJumpPackets(eReplayPacketId packetID);
	void		ProcessRelevantPacketDuringJumpPackets(eReplayPacketId packetID, ReplayController &controller, bool isBackwards);
	void		OnJumpDeletePacketForwardsObjectNotExisting(tCreatePacket const *pCreatePacket);
	void		OnJumpCreatePacketBackwardsObjectNotExisting(tCreatePacket const *pCreatePacket);

private:
	atArray<mapObjectID>	m_mapObjectsToHide;
	atMap<CEntity*, mapObjectID>	m_mapObjectsMap;
	atArray<CObject*>		m_mapObjectsToRecord;
	atArray<CObject*>		m_otherObjectsToRecord;

	struct replayIDFrameRefPair 
	{
		replayIDFrameRefPair()
		{
			id = ReplayIDInvalid;
		}
		CReplayID id;
		FrameRef frameRef;
	};

	class deferredMapObjectToHide
	{
	public:
		deferredMapObjectToHide()
		{
			m_MapObjectId = 0;
			m_objectsToAddStamp = -1;
		}
		deferredMapObjectToHide(mapObjectID mapObjId, const CReplayID& replayObjId)
		{
			m_MapObjectId = mapObjId;
			m_objectsToAddStamp = -1;
			m_ReplayObjectId = replayObjId;
		}
		bool operator==(const deferredMapObjectToHide &other) const
		{
			return m_MapObjectId == other.m_MapObjectId;
		}
		CReplayID m_ReplayObjectId;
		mapObjectID m_MapObjectId;
		s32 m_objectsToAddStamp;
		replayIDFrameRefPair m_replayIDFrameRefPair;
	};

	// Objects we need to add to the world which have no map object counterpart.
	class counterpartlessObjectToAdd
	{
	public:
		counterpartlessObjectToAdd()
		{
			m_pObjectToAdd = NULL;
		}
		counterpartlessObjectToAdd(CObject *pObjectToAdd, fwInteriorLocation loc)
		{
			m_pObjectToAdd = pObjectToAdd;
			m_location = loc;
		}
		~counterpartlessObjectToAdd()
		{
		}
	public:
		CObject *m_pObjectToAdd;
		fwInteriorLocation m_location;
	};

	u32										m_objectToAddStamp;
	atArray<deferredMapObjectToHide>		m_deferredMapObjectToHide;
	atArray<counterpartlessObjectToAdd>		m_counterpartlessObjectsToAdd;
	atMap<u32, replayIDFrameRefPair>		m_mapObjectHideInfos;

	atMap<CReplayID, CReplayID>				m_fragChildrenUnlinked;

	bool		m_allowPhysicsOverride;

	int			FindHiddenMapObject(const mapObjectID& id) const;

	DestroyedMapObjectsManager m_DestroyedMapObjects;

	tPacketVersion		m_currentCreatePacketVersion;
};

#define INTERPER_OBJ InterpWrapper<CReplayInterfaceTraits<CObject>::tInterper>


#endif // GTA_REPLAY

#endif // REPLAYINTERFACEPED_H
