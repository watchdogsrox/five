#ifndef REPLAYINTERFACE_H
#define REPLAYINTERFACE_H

#if GTA_REPLAY

#define MAX_POST_UPDATE_PACKET_ENTITIES2 512

#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/map.h"

#include "system/criticalsection.h"
#include "system/filemgr.h"

#include "replay_channel.h"
#include "ReplayController.h"
#include "ReplayRecorder.h"
#include "ReplaySupportClasses.h"
#include "ReplayModelManager.h"
#include "Misc/InterpPacket.h"
#include "Scene/EntityTypes.h"

namespace rage
{
	class bkGroup;
}

class CEntity;
class CPacketBase;

template<int deletePacketSize>
struct deletionData
{
	deletionData(){}
	deletionData(CReplayID id) : replayID(id)	{}

	CReplayID	replayID;
	CEntity*	pEntity;
	char		deletePacket[deletePacketSize];

	bool operator ==(const deletionData& other) const
	{
		return replayID == other.replayID;
	}
};

// Base class for replay traits.
// Intentionally left empty
template<typename T>
class CReplayInterfaceTraits
{};
class CPacketInterp;

template<typename T>
class CReplayInterface;

const unsigned int maxPacketDescriptorsPerInterface = 24;
const s32	entityCreationLimitPerFrame = 20;
#define PACKETNAMELENGTH 64
#define REPLAY_DEFAULT_ALIGNMENT 16
class CPacketDescriptorBase
{
public:
	typedef void(*tPrintFunc)(ReplayController&, eReplayPacketOutputMode, FileHandle);

	CPacketDescriptorBase()
		: m_packetID(PACKETID_INVALID)
		{}

	CPacketDescriptorBase(eReplayPacketId packetID, u32 packetSize, const char* packetName, tPrintFunc pf)
		: m_printFunc(pf)
		, m_packetID(packetID)
		, m_packetSize(packetSize)
	{
		strcpy_s(m_packetName, PACKETNAMELENGTH, packetName);
	}

	inline	eReplayPacketId		GetPacketID() const		{	return m_packetID;		}
	inline	u32					GetPacketSize() const	{	return m_packetSize;	}
	inline	const char*			GetPacketName() const	{	return m_packetName;	}

	void	PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle) const
	{
		(*m_printFunc)(controller, mode, handle);
	}


	tPrintFunc		m_printFunc;

	eReplayPacketId	m_packetID;
	u32				m_packetSize;
	char			m_packetName[PACKETNAMELENGTH];
};


template<typename T>
class CPacketDescriptor : public CPacketDescriptorBase
{
public:
	
	typedef bool(CReplayInterface<T>::*tRecFunc)(T*, CReplayRecorder&, CReplayRecorder&, CReplayRecorder&, u16 );
	typedef void(CReplayInterface<T>::*tExtractFunc)(T*, CPacketInterp const*, CPacketInterp const*, const ReplayController&);

	CPacketDescriptor()
		
	{}

	CPacketDescriptor(eReplayPacketId packetID, s32 entityType, u32 packetSize, const char* packetName, tPrintFunc pf, tRecFunc rf, tExtractFunc ef)
		: CPacketDescriptorBase(packetID, packetSize, packetName, pf)
		, m_recordFunc(rf)
		, m_extractFunc(ef)
		, m_entityType(entityType)
	{}

	
	inline	s32			GetEntityType() const	{	return m_entityType;	}
	
	tRecFunc			GetRecFunc() const		{	return m_recordFunc;	}
	tExtractFunc		GetExtractFunc() const	{	return m_extractFunc;	}

private:
	tRecFunc			m_recordFunc;
	tExtractFunc		m_extractFunc;

	s32					m_entityType;
};

#define PACKETDESCRIPTOR(id, et, type, recFunc) CPacketDescriptor(id, et, sizeof(type), #type, \
	&iReplayInterface::template PrintOutPacketDetails<type>,\
	recFunc, NULL)


// Base interface for the replay interfaces
class iReplayInterface
{
public:
			iReplayInterface(u32 recBufferSize, u32 crBufferSize, u32 fullCrBufferSize);
	virtual ~iReplayInterface();

	virtual u32		GetMemoryUsageForFrame(bool blockChange);

	virtual u32		GetDeletionsSize() const								{ return 0;	}
	u32				GetActualMemoryUsage() const							{ return m_memoryUsageActual;	}
	virtual void	DisplayEstimatedMemoryUsageBreakdown()					{}

	virtual	const char*	GetShortStr() const									= 0;
	virtual	const char*	GetLongFriendlyStr() const							= 0;

	virtual u8		GetPreferredRecordingThreadIndex()						{ return 3; }

	virtual void	Reset()													{	m_memoryUsageActual = 0; m_failedEntities.Reset();	}
	virtual void	Clear()													{	Reset();	m_instanceID = 0;	}
	virtual void	EnableRecording()										{}

	virtual void	RecordFrame(u16 sessionBlockIndex)						= 0;
	virtual void	RecordDeletionPackets(ReplayController&)				{}
	virtual bool	IsRecentlyDeleted(const CReplayID&) const				{	return false;	}
	virtual void	PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange);
	virtual	void	ResetCreatePacketsForCurrentBlock()						{}

	virtual void	PostRecordOptimize(CPacketBase*) const					{}
	virtual void	ResetPostRecordOptimizations() const					{}

	virtual bool	GetPacketGroup(eReplayPacketId packetID, u32& packetGroup) const;

	virtual void	PlaybackSetup(ReplayController&)						{};
	virtual void	LinkCreatePacketsForPlayback(ReplayController&, u32)	{}
	virtual void	LinkUpdatePackets(ReplayController&, u16, u32)			{}
	virtual void	ApplyFades(ReplayController& controller, s32);
	virtual void	ApplyFadesSecondPass()									{}

	virtual bool	TryPreloadPacket(const CPacketBase*, bool&, u32 PRELOADDEBUG_ONLY(, atString&))		{ return false; }
	virtual bool	FindPreloadRequests(ReplayController&, tPreloadRequestArray&, u32&, const u32, bool) = 0;
	virtual void	UpdatePreloadRequests(u32 time);
	virtual ePlayPktResult PreprocessPackets(ReplayController&, bool)		= 0;
	virtual	ePlayPktResult PlayPackets(ReplayController&, bool)				= 0;
	virtual void	JumpPackets(ReplayController&)							= 0;
	virtual bool	IsRelevantPacket(eReplayPacketId) const					= 0;
	virtual bool	IsRelevantUpdatePacket(eReplayPacketId) const			= 0;
	virtual bool	IsRelevantDeletePacket(eReplayPacketId) const			{ return false;	}
	virtual bool	IsRelevantOptimizablePacket(eReplayPacketId) const		{ return false;	}
	virtual bool	MatchesType(const atHashWithStringNotFinal&) const		{ replayFatalAssertf(false, "");	return false;	}
	virtual bool	MatchesType(const CEntity* pEntity) const				= 0;

	virtual u32		GetDeletePacketSize() const								{ return 0; }

 	virtual void	GetEntityAsEntity(CEntityGet<CEntity>&)					{}
	virtual void	FindEntity(CEntityGet<CEntity>&)						{}

	virtual void	Process()												{}
	virtual void	PostProcess()											{}
	virtual void	PostRender()											{}

	virtual void	OnDelete(CPhysical*)									{	replayFatalAssertf(false, "");	}
	virtual void	ResetAllEntities()										{}
	virtual void	StopAllEntitySounds()									{}
	virtual void	CreateDeferredEntities(ReplayController&)				{}
	virtual void	CreateBatchedEntities(s32&, const CReplayState&)		{}
	virtual bool	HasBatchedEntitiesToProcess() const						{	return false;	}
	virtual void	PreClearWorld()											{}
	virtual void	ClearWorldOfEntities()									{}

	virtual	bool	GetPacketName(eReplayPacketId, const char*&) const		{	replayFatalAssertf(false, "");	return false;	}

	virtual bool	IsCurrentlyBeingRecorded(const CReplayID& replayID) const= 0;
	virtual void	ResetEntityCreationFrames()								{}
	
	virtual void	RegisterForRecording(CPhysical*, FrameRef)				= 0;
	virtual void	RegisterAllCurrentEntities()							{}
	virtual void	DeregisterAllCurrentEntities()							{}
	virtual void	ClearPacketInformation()								{}
	virtual bool	EnsureIsBeingRecorded(const CEntity* pElem)				= 0;
	virtual bool	EnsureWillBeRecorded(const CEntity*)					= 0;
	virtual bool	SetCreationUrgent(const CReplayID& replayID)			= 0;

	virtual bool	ShouldRecordElement(const CEntity*) const				{ return true; }
	virtual bool	ShouldRegisterElement(const CEntity* pEntity) const		{ return ShouldRecordElement(pEntity); }

#if __BANK
	virtual rage::bkGroup* AddDebugWidgets()								= 0;
	virtual	rage::bkGroup* AddDebugWidgets(const char* groupName);
#endif // __BANK

	bool	DoSizing() const												{	return m_enableSizing && m_enabled;		}
	bool	DoRecording() const												{	return m_enableRecording && m_enabled;	}
	bool	DoPlayback() const												{	return m_enablePlayback && m_enabled;	}
	bool	ShouldUpdateOnPreprocess() const								{	return m_updateOnPreprocess;			}

	virtual	void	OffsetCreatePackets(s64 offset)							= 0;
	virtual void	OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize)= 0;

	virtual void	ProcessRewindDeletions(FrameRef)						{}
			
	virtual bool	GetBlockDetails(char*, bool&, bool) const						{	return false;	}
	virtual bool	ScanForAnomalies(char*, int&) const								{	return false;	}

	virtual void	OutputReplayStatistics(ReplayController& controller, atMap<s32, packetInfo>& infos);

	virtual void	PrintOutEntities()										{}
	virtual void	PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);
	virtual void	PrintOutPacketHeader(CPacketBase const* pPacket, eReplayPacketOutputMode mode, FileHandle handle);

	virtual	void	FlushPreloadRequests()									{	m_modelManager.FlushLoadRequests();	}

	void	SetEntityFailedToCreate(const CReplayID& id, const char* OUTPUT_ONLY(pReason))
			{
				if(m_failedEntities.Access(id) == NULL)
				{
					m_failedEntities[id] = true;
					replayDebugf1("Failed to create %s (ID: 0x%08X) due to: %s", GetShortStr(), id.ToInt(), pReason);
				}
			}
	bool	HasEntityFailedToCreate(const CReplayID& id) const				{	return m_failedEntities.Access(id) != NULL;	}

	u32		GetHash(u32 id, u32 hash);

	u32		GetFrameHash() const											{	return m_frameHash;		}

			template<typename PACKETTYPE>
	static	void	PrintOutPacketDetails(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
			{
				PACKETTYPE const* pPacket = controller.ReadCurrentPacket<PACKETTYPE>();
				if(mode == REPLAY_OUTPUT_XML)
					pPacket->PrintXML(handle);
				else
					pPacket->Print();
				controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
			}

	static	s16		GetNextInstanceID()
			{
				m_instanceID++;

				if (m_instanceID == 0x7FFF)
					m_instanceID = 0;

				return m_instanceID;
			}

			bool				m_enableSizing;
			bool				m_enableRecording;
			bool				m_enablePlayback;
			bool				m_enabled;
			bool				m_anomaliesCanExist;

			// Moved into the base class as a static so there are no entities
			// with identical ID's
	static	s16					m_instanceID;

			eReplayPacketId		m_relevantPacketMin;
			eReplayPacketId		m_relevantPacketMax;
			u32					m_packetGroup;

			u32					m_memoryUsageActual;

			bool				m_updateOnPreprocess;
	static 	int					sm_totalEntitiesCreatedThisFrame;

			u8*					m_recordBuffer;
			u32					m_recordBufferSize;
			u32					m_recordBufferUsed;
			bool				m_recordBufferFailure;

			u8*					m_pCreatePacketsBuffer;
			u32					m_createPacketsBufferSize;
			u32					m_createPacketsBufferUsed;
			bool				m_createPacketsBufferFailure;

			u8*					m_pFullCreatePacketsBuffer;
			u32					m_fullCreatePacketsBufferSize;
			u32					m_fullCreatePacketsBufferUsed;
			bool				m_fullCreatePacketsBufferFailure;

			CReplayModelManager	m_modelManager;
			atMap<CReplayID, int>	m_failedEntities;

			u32					m_frameHash;
};








template<typename T>
class CReplayInterface
	: public iReplayInterface
{
protected:
	typedef typename CReplayInterfaceTraits<T>::tCreatePacket	tCreatePacket;
	typedef typename CReplayInterfaceTraits<T>::tUpdatePacket	tUpdatePacket;
	typedef typename CReplayInterfaceTraits<T>::tDeletePacket	tDeletePacket;
	typedef typename CReplayInterfaceTraits<T>::tPool			tPool;
	typedef typename CReplayInterfaceTraits<T>::tInterper		tInterper;
	typedef typename CReplayInterfaceTraits<T>::tDeletionData	tDeletionData;

	template<typename S>
	class CEntityTracker
	{
	public:
		CEntityTracker()
			: m_pPtr(NULL)
			, m_iInstID(-1)
		{}

		void	SetEntity(S* pEntity)		{	m_pPtr = pEntity;			}
		S*		GetEntity() const			{	return m_pPtr;				}

		void	Reset()						{	m_pPtr = NULL;	m_iInstID = -1;	}

		void	SetInstID(s16 iInstID)		{	m_iInstID = iInstID;		}
		s16		GetInstID() const			{	return m_iInstID;			}

		bool	IsEmpty() const				{	return (m_pPtr == NULL);	}

	protected:
		S*	m_pPtr;
		s16		m_iInstID;
	};

public:
	

	CReplayInterface(typename CReplayInterfaceTraits<T>::tPool& pool, u32 recBufferSize, u32 crBufferSize, u32 fullCrBufferSize);
	virtual			~CReplayInterface();

	template<typename PACKETTYPE>
	void			AddPacketDescriptor(eReplayPacketId packetID, const char* packetName);

	virtual u32		GetDeletionsSize() const	{	return m_deletionPacketsSize;	}
	virtual void	DisplayEstimatedMemoryUsageBreakdown()							{}

	const	char*	GetShortStr() const				{	return CReplayInterfaceTraits<T>::strShort;	}
	const	char*	GetLongFriendlyStr() const		{	return CReplayInterfaceTraits<T>::strLongFriendly;	}

	virtual void	Reset();
	virtual void	Clear();

	virtual void	PreRecordFrame(CReplayRecorder&, CReplayRecorder&)						{}
	virtual void	RecordFrame(u16 sessionBlockIndex);

			void	ConvertMapTypeDefHashToIndex(ReplayController& controller, tCreatePacket const* pCreatePacket);
	virtual void	PlaybackSetup(ReplayController& controller);
	virtual void	ApplyFades(ReplayController& controller, s32 frame);
	virtual void	ApplyFadesSecondPass();

	virtual ePlayPktResult PreprocessPackets(ReplayController& controller, bool entityMayNotExist);
	virtual ePlayPktResult PlayPackets(ReplayController& controller, bool entityMayNotExist);
	virtual void	PlayPacket(ReplayController& controller);
	
	virtual void	JumpPackets(ReplayController& controller);
			bool	DoesObjectExist(tCreatePacket const *pCreatePacket);
	virtual void	OnJumpDeletePacketForwardsObjectNotExisting(tCreatePacket const *pCreatePacket) { (void)pCreatePacket; }
	virtual void	OnJumpCreatePacketBackwardsObjectNotExisting(tCreatePacket const *pCreatePacket) { (void)pCreatePacket; }

	virtual	void	JumpUpdatePacket(ReplayController& controller);

	virtual bool	IsRelevantUpdatePacket(eReplayPacketId packetID) const	{	return packetID == m_updatePacketID;	}
	virtual bool	IsRelevantDeletePacket(eReplayPacketId packetID) const	{	return packetID == m_deletePacketID;	}
	virtual bool	IsEntityUpdatePacket(eReplayPacketId packetID) const	{	return packetID == m_updatePacketID;	}

	virtual bool	TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason));
	virtual bool	FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& requestCount, const u32 systemTime, bool isSingleFrame);

			bool	MatchesType(const atHashWithStringNotFinal& type) const				
			{	
				atHashWithStringNotFinal thisType = T::GetStaticClassId();
				return type == thisType;	
			}

	virtual bool	MatchesType(const CEntity* pEntity) const = 0;
			
	using iReplayInterface::GetHash;
			u32		GetHash(T* pEntity, u32 hash)
			{
				u32 id = (u32)pEntity->GetReplayID().ToInt();
				return GetHash(id, hash);
			}

			void	Process();
			void	PostProcess();

			bool	ProcessPostUpdateAsCreate(CPacketBase* pBasePacket);

			void	FreeUnusedCreatePackets(u8 block);

	virtual void	ClearWorldOfEntities();

	virtual	void	ResetAllEntities();
	virtual void	ResetEntity(T* pEntity)	{ (void)pEntity; }
	virtual	void	StopAllEntitySounds();
	virtual void	StopEntitySounds(T* pEntity)	{ (void)pEntity; }

			void	CreateDeferredEntities(ReplayController& controller);
			void	AddToCreateLater(const tCreatePacket* pCreatePacket);
			void	CreateBatchedEntities(s32& createdSoFar, const CReplayState& state);
			bool	HasBatchedEntitiesToProcess() const;

			void	WriteCreatePackets(FileHandle fileID);
			void	ReadCreatePackets(FileHandle fileID);

			bool	IsRelevantPacket(eReplayPacketId packetID) const;
	virtual bool	ShouldRelevantPacketBeProcessedDuringJumpPackets(eReplayPacketId packetID) { (void)packetID; return false; } // Will have passed IsRelevantPacket() test.
	virtual void	ProcessRelevantPacketDuringJumpPackets(eReplayPacketId packetID, ReplayController &controller, bool isBackwards) { (void)packetID; (void)controller; (void)isBackwards; replayAssertf(0, "Shouldn't reach here...Expecting function in derived class." ); }

			bool	FindPacketDescriptor(eReplayPacketId packetID, const CPacketDescriptor<T>*& pDescriptor) const;
			bool	FindPacketDescriptorForEntityType(s32 et, const CPacketDescriptor<T>*& pDescriptor) const;
	virtual bool	GetPacketSizeForEntityType(s32 entityType, u32& packetSize) const;
			bool	GetPacketName(eReplayPacketId packetID, const char*& packetSize) const;

			bool	IsCurrentlyBeingRecorded(const CReplayID& replayID) const;
			void	ResetEntityCreationFrames();

	virtual void	PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
					{	iReplayInterface::PrintOutPacket(controller, mode, handle);	}

#if __BANK
	virtual	bkGroup* AddDebugWidgets();
#endif // __BANK

			virtual bool	ShouldRecordElement(const T*) const		{	return true;	}
			bool			ShouldRecordElement(const CEntity* pEntity) const;

	template<typename PACKETTYPE, typename SUBTYPE>
			bool	RecordElementInternal(T* pElem, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex);

	template<typename PACKETTYPE, typename SUBTYPE>
			void	ExtractPacket(T* pElem, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller);

			void	LinkCreatePacketsForPlayback(ReplayController& controller, u32 endPosition);
			void	LinkUpdatePackets(ReplayController& controller, u16 previousBlockFrameCount, u32 endPosition);
			void	PostStorePacket(tUpdatePacket* pPacket, const FrameRef& currentFrameRef, const u16 previousBlockFrameCount);
			void	GetEntity(CEntityGet<T>& entityGet);
			void	FindEntity(CEntityGet<CEntity>& entityGet);

			tCreatePacket*  GetCreatePacket(const CReplayID& replayID);
			const tCreatePacket*  GetCreatePacket(const CReplayID& replayID) const;

	virtual bool	GetBlockDetails(char* pString, bool& err, bool recording) const;
#if __BANK
	virtual bool	ScanForAnomalies(char* pString, int& index) const;
#endif // __BANK

protected:

	virtual u32		GetCreatePacketSize() const								{	return sizeof(tCreatePacket);	}
	virtual u32		GetUpdatePacketSize(T*) const							{	return sizeof(tUpdatePacket);	}
	virtual u32		GetDeletePacketSize() const								{	return sizeof(tDeletePacket);	}

	virtual void	PreUpdatePacket(ReplayController& controller);
	virtual void	PreUpdatePacket(const tUpdatePacket* pPacket, const tUpdatePacket* pNextPacket, float interpValue, const CReplayState& flags, bool canDefer = true);

	virtual void	PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist);
	virtual void	PlayCreatePacket(ReplayController& controller);
	virtual void	PlayDeletePacket(ReplayController& controller);

	virtual T*		CreateElement(tCreatePacket const*, const CReplayState&) 					{	replayAssert(false && "CreateElement not implemented");	return NULL;	}
			void	PostCreateElement(tCreatePacket const* pPacket, T* pElement, const CReplayID& replayID);

	virtual bool	RecordElement(T* pElem, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex)	{	return RecordElementInternal<tUpdatePacket, T>(pElem, recorder, createPacketRecorder, fullCreatePacketRecorder, sessionBlockIndex);	}
			
			void	RegisterAllCurrentEntities();
			void	DeregisterAllCurrentEntities();
			void	RegisterForRecording(CPhysical* pEntity, FrameRef creationFrame = FrameRef::INVALID_FRAME_REF);
	virtual	void	DeregisterEntity(CPhysical* pEntity);
			void	ClearPacketInformation();
			bool	EnsureIsBeingRecorded(const CEntity* pElem);
			bool	EnsureWillBeRecorded(const CEntity* pElem);
			bool	SetCreationUrgent(const CReplayID& replayID);
	
	virtual bool	RemoveElement(T* pElem, const tDeletePacket* pDeletePacket, bool isRewinding = true);
	virtual void	ProcessRewindDeletionWithoutEntity(tCreatePacket const *pCreatePacket, CReplayID const &replayID);

	virtual void	SetEntity(const CReplayID& id, T* pEntity, bool playback);
	virtual void	ClrEntity(const CReplayID& id, const T* pEntity);

	virtual	void	OnDelete(CPhysical* pEntity);
			void	RecordDeletionPackets(ReplayController& controller);
			void	ClearDeletionInformation();
			void	ResetCreatePacketsForCurrentBlock()
			{ 
				sysCriticalSection cs(m_createPacketCS);
				m_CurrentCreatePackets.Reset(); 
				m_CreatePostUpdate.clear();
			}

			void	ProcessRewindDeletions(FrameRef frame);

			bool	IsRecentlyDeleted(const CReplayID& id) const;

			void	OffsetCreatePackets(s64 offset);
			void	OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize);

			void	GetEntityAsEntity(CEntityGet<CEntity>& entityGet);

			void	PrintOutEntities();			

protected:
	virtual	void	ApplyUpdateFadeSpecial(CEntityData& entityData, const tUpdatePacket& packet) { (void)entityData; (void)packet; }

			bool	PlayUpdateBackwards(const ReplayController& controller, const tUpdatePacket* pUpdatePacket);

			int		GetNextEntityID(T* pEntity);

			void	PrintEntity(T* pEntity, const char* str1, const char* str2);

			T*		EnsureEntityIsCreated(const CReplayID& id, const CReplayState& state, bool canAddToCreateLater);

			void	LinkCreatePacket(const tCreatePacket* pCreatePacket);
			void	UnlinkCreatePacket(const CReplayID& replayID, bool jumping);
			bool	HasCreatePacket(const CReplayID& replayID) const;
	virtual void	HandleCreatePacket(const tCreatePacket* pCreatePacket, bool notDelete, bool firstFrame, const CReplayState& state);

			void	SetCurrentlyBeingRecorded(int index, T* pEntity);

			void	HandleFailedToLoadModelError(const tCreatePacket* pPacket);
	

	tPool&							m_pool;
	tInterper						m_interper;

	
	//////////////////////////////////////////////////////////////////////////
	// Returned from the entity registry (below) to allow us to unlock after we've
	// finished accessing an entry.
	class CTrackerGuard
	{
	public:
		CTrackerGuard(CEntityTracker<T>* t, sysCriticalSectionToken* cs)
			: m_tracker(t)
			, m_csToken(cs)
		{}

		CTrackerGuard(CTrackerGuard&& other)
			: m_tracker(other.m_tracker)
			, m_csToken(other.m_csToken)
		{
			other.m_tracker = NULL;
			other.m_csToken = NULL;
		}

		~CTrackerGuard()
		{
			UnlockCS();
		}

		CEntityTracker<T>*			Get()	{	return m_tracker;	}

	private:
		CTrackerGuard(const CTrackerGuard&);
		CTrackerGuard& operator=(const CTrackerGuard&);

		void UnlockCS()
		{
			if(m_csToken)
			{
				m_csToken->Unlock();
			}
		}

		CEntityTracker<T>*			m_tracker;
		sysCriticalSectionToken*	m_csToken;
	};


	//////////////////////////////////////////////////////////////////////////
	// Wrap up the entity trackers.  Main thing here is to lock around access
	// so multiple threads can't be modifying and accessing at the same time!
	class CEntityRegistry
	{
	public:
		CEntityRegistry(size_t count)
			: m_count(count)
			, m_entityTrackersUsed(0)
		{
			m_pEntities	= rage_new CEntityTracker<T>[m_count];
		}

		~CEntityRegistry()
		{
			delete[] m_pEntities;
		}
	
		void							Reset()
		{
			for(int i = 0; i < m_count; ++i)
				m_pEntities[i].Reset();
		}

		// Get access to an entry in the list...
		// Pass the guard back by value as we have an rvalue reference constructor (see CTrackerGuard)
		CTrackerGuard					Get(s16 index, bool lockCS = true)
		{
			replayAssertf(index >= 0 && index < m_count, "Index (%d) out of range (0-%d)", index, (s32)m_count-1);
			if(index < 0 || index >= m_count)
				return CTrackerGuard(NULL, NULL);

			if(lockCS)
				m_csToken.Lock();
			return CTrackerGuard(&m_pEntities[index], lockCS ? &m_csToken : NULL);
		}

		void							IncrementCount()		{	++m_entityTrackersUsed;	}
		void							DecrementCount()		{	--m_entityTrackersUsed;	}
		size_t							GetCount() const		{	return m_entityTrackersUsed;	}

		void							Lock()					{	m_csToken.Lock();	}
		void							Unlock()				{	m_csToken.Unlock();	}

	private:
		CEntityRegistry(const CEntityRegistry&);
		CEntityRegistry& operator=(const CEntityRegistry&);

		sysCriticalSectionToken			m_csToken;
		CEntityTracker<T>*				m_pEntities;
		const size_t					m_count;
		size_t							m_entityTrackersUsed;
	};
	CEntityRegistry					m_entityRegistry;


	struct sCreatePacket
	{
		sCreatePacket() : pCreatePacket_const(NULL)	{}
		sCreatePacket(const tCreatePacket* p)	: pCreatePacket_const(p)	{}
		sCreatePacket& operator=(const tCreatePacket* p)	{ pCreatePacket_const = p;	return *this;	}
		bool operator!=(const tCreatePacket* p) const		{	return pCreatePacket_const != p;		}

		union 
		{
			tCreatePacket* pCreatePacket;
			const tCreatePacket* pCreatePacket_const;
		};
	};
	atMap<CReplayID, sCreatePacket>	m_CurrentCreatePackets;
	atMap<CReplayID, tUpdatePacket*>	m_LatestUpdatePackets;
	atArray<const tCreatePacket*>	m_CreatePostUpdate;

	atArray<CReplayID>				m_recentUnlinks;
	
	atArray<CReplayID>				m_currentlyRecorded;

	CEntityData*					m_entityDatas;

	eReplayPacketId					m_createPacketID;
	eReplayPacketId					m_deletePacketID;
	eReplayPacketId					m_updatePacketID;
	eReplayPacketId					m_updateDoorPacketID;

	u32								m_entitiesDuringRecording;
	u32								m_entitiesDuringPlayback;

	atRangeArray<CPacketDescriptor<T>, maxPacketDescriptorsPerInterface>	m_packetDescriptors;
	u32								m_numPacketDescriptors;


	atArray<tDeletionData>		m_deletionsToRecord;
	u32							m_deletionPacketsSize;
	atArray<CReplayID>			m_recentDeletions;

	atMap<CReplayID, const tCreatePacket*>	m_deletionsOnRewind;	// We don't actually need the value, but want the Map searching...is there a Set?
	struct deferredCreationData
	{
		deferredCreationData()
			: pPacket(NULL)
			, pNextPacket(NULL)
			, interpValue(0.0f)
		{}
		const tUpdatePacket*	pPacket;
		const tUpdatePacket*	pNextPacket;
		float					interpValue;
	};
	atArray<deferredCreationData>	m_deferredCreationList;

	mutable sysCriticalSectionToken	m_createPacketCS;
};

#define INTERPER InterpWrapper<tInterper>

#include "ReplayInterfaceImpl.h"

#endif // GTA_REPLAY

#endif // REPLAYINTERFACE_H
