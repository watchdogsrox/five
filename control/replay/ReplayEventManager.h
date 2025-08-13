#ifndef REPLAYEVENTMANAGER_H
#define REPLAYEVENTMANAGER_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "atl/map.h"
#include "atl/freelist.h"

#include "ReplayInterfaceManager.h"
#include "ReplayTrackingController.h"
#include "ReplayRecorder.h"
#include "ReplayController.h"

#include "audio/DynamicMixerPacket.h"
#include "Audio/SoundPacket.h"
#include "rmptfx/ptxeffectinst.h"

#include "fwscene/stores/staticboundsstore.h"

class CReplayEventManager;
class CPacketHistory;
class FileStoreHandle;
struct preloadData;
class CHistoryPacketController;

#define		NUM_EVENT_BUFFERS			2
#define		EVENT_BUFFER_SIZE			128*1024
#define		EVENT_BLOCK_MONITOR_SIZE	128*1024	// To tweak
#define		EVENT_FRAME_MONITOR_SIZE	32*1024		// To tweak

#define		MONITOR_BUFFER_SAFETY_SIZE	0.9f		// If this amount is used in the buffer then it's considered 'nearly full' allowing old low priority packets to be removed.

struct ptxFireRef
{
	ptxFireRef(){}
	ptxFireRef(size_t id) : m_Id(id){}

	const bool operator == (const ptxFireRef &ref) const { return m_Id == ref.m_Id; }
	const bool operator != (const ptxFireRef &ref) const { return m_Id != ref.m_Id; }

	size_t m_Id;
};

typedef size_t ptxEffectRef;
typedef int		tTrackedDecalType;

#define		BUFFER_NAME_LENGTH			64
class CReplayPacketBuffer
{
public:
	CReplayPacketBuffer(int bufferSize, const char* pName)
	{
		m_pBuffer = (u8*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(bufferSize, 16);
#if REPLAY_DEBUG_MEM_FILL
		memset(m_pBuffer, 0xCC, bufferSize);
#endif // REPLAY_DEBUG_MEM_FILL
		m_recorder = CReplayRecorder(m_pBuffer, bufferSize-4);	// -4 to give us a wee guard we can check
		m_memPeak = 0;
		m_pFirstRecentPacket = NULL;

		replayFatalAssertf(strlen(pName) < BUFFER_NAME_LENGTH, "Name of buffer (%s) too long - %u", pName, (u32)strlen(pName));
		m_name = pName;

		BANK_ONLY(m_ttySpammed = false;)
	}

	~CReplayPacketBuffer()
	{
		sysMemManager::GetInstance().GetReplayAllocator()->Free((void*)m_pBuffer);
		m_pBuffer = NULL;
	}

	void					Reset()	
	{
		m_recorder.Reset(); 
#if REPLAY_DEBUG_MEM_FILL
		memset(m_pBuffer, 0xCC, m_recorder.GetBufferSize());
#endif // REPLAY_DEBUG_MEM_FILL
	}
	const u8*				GetBuffer() const		{	return m_pBuffer;			}
	u32						GetUsed() const			{	return m_recorder.GetMemUsed();	}
	u32						GetMemPeak() const		{	return m_memPeak;			}
	u32						GetPacketCount() const	{	return m_packetCount;		}

	CReplayRecorder&		GetRecorder()			{	return m_recorder;			}
	const CReplayRecorder&	GetRecorder() const		{	return m_recorder;			}

	void					SetMemPeak(u32 val)		{	m_memPeak = val;			}
	void					IncreasePacketCount()	{	++m_packetCount;			}
	void					DecreasePacketCount()	{	--m_packetCount;			}

	template<typename T>
	T*					RecordPacket(T& packet)	
	{
		T* pRet = m_recorder.RecordPacket(packet);		
		if(pRet)
		{
			IncreasePacketCount();

			if(m_pFirstRecentPacket == NULL)
				m_pFirstRecentPacket = (u8*)pRet;
		}	

		return pRet;
	}
	template<typename T>
	T*						GetFirstPacket()		{	return m_recorder.GetFirstPacket<T>();		}
	template<typename T>
	const T*				GetFirstPacket() const	{	return m_recorder.GetFirstPacket<T>();		}
	template<typename T>
	T*						GetNextPacket(const T* pCur)		{	return m_recorder.GetNextPacket<T>(pCur);	}
	template<typename T>
	const T*				GetNextPacket(const T* pCur) const	{	return m_recorder.GetNextPacket<T>(pCur);	}
	template<typename T>
	T*						RemovePacket(T* pPacket)
	{
		if(m_pFirstRecentPacket != NULL && (u8*)pPacket <= m_pFirstRecentPacket)
			m_pFirstRecentPacket -= pPacket->GetPacketSize();

		DecreasePacketCount();

		return m_recorder.RemovePacket<T>(pPacket);
	}

	void					ResetFirstRecentPacket()	{	m_pFirstRecentPacket = NULL;	}
	bool					IsPacketRecent(u8* p) const	{	return m_pFirstRecentPacket != NULL && p >= m_pFirstRecentPacket;	}

#if __BANK
	bool					ShouldSpamTTY()	const
	{
		bool ret = !m_ttySpammed;
		m_ttySpammed = true;
		return ret;
	}
	const char*				GetName() const				{	return m_name.c_str();			}
#endif // __BANK

private:
	u8*						m_pBuffer;
	CReplayRecorder			m_recorder;
	u32						m_memPeak;
	u32						m_packetCount;

	u8*						m_pFirstRecentPacket;

	atFixedString<BUFFER_NAME_LENGTH>	m_name;					
	BANK_ONLY(mutable bool			m_ttySpammed;)
};





//////////////////////////////////////////////////////////////////////////
typedef void(CReplayEventManager::*tFuncWithController)(ReplayController&);
typedef CPacketBase const*(CReplayEventManager::*tResetFunc)(ReplayController& controller, eReplayPacketId packetID, u32 currentGameTime);
typedef void(CReplayEventManager::*tPrintFunc)(ReplayController&, eReplayPacketOutputMode, FileHandle);
typedef bool(CReplayEventManager::*tGetReplayID)(const CPacketBase*, u8 index, CReplayID&) const;
typedef void(CReplayEventManager::*tLinkPacketFunc)(CPacketBase*, ReplayController&);
typedef bool(CReplayEventManager::*tCheckMonitoredEventFunc)(const CPacketEvent*, CHistoryPacketController&);
typedef void(CReplayEventManager::*tCheckTrackingFunc)(const CPacketEvent*);
typedef void(CReplayEventManager::*tUpdateMonitoredPacketFunc)(CPacketEvent*, CHistoryPacketController&);

typedef CPacketBase const*(CReplayEventManager::*tEventPlaybackFunc)(ReplayController&, eReplayPacketId, const u32, const u32, bool);
typedef CPacketBase const*(CReplayEventManager::*tEventTrackPlaybackFunc)(ReplayController&, const u32, const u32, bool);
typedef ePreloadResult (CReplayEventManager::*tEventPreloadFunc)(const CPacketBase*, const CReplayState&, const u32, const s32);
typedef ePreplayResult (CReplayEventManager::*tEventPreplayFunc)(const CPacketBase*, const CReplayState&, const u32, const s32);

#define PACKETNAMELENGTH 64
class CHistoryPacketController
{
public:
	
	CHistoryPacketController()
		: m_playFunction(NULL)
		, m_playTrackedFunction(NULL)
		, m_preloadFunction(NULL)
		, m_preplayFunction(NULL)
		, m_resetFunction(NULL)
		, m_invalidateFunction(NULL)
		, m_setupFunction(NULL)
		, m_printFunc(NULL)
		, m_getReplayIDFunc(NULL)
		, m_linkPacketsFunc(NULL)
		, m_checkExpiryFunc(NULL)
		, m_UpdateMonitoredPacketFunc(NULL)
		, m_checkTrackingFunc(NULL)
		, m_packetSize(0)
		, m_enabled(true)
		, m_playbackFlags(0)
		, m_expirePrevious(0)
	{}

	CHistoryPacketController(tEventPlaybackFunc playFunc, tEventPreloadFunc preloadFunc, tEventPreplayFunc preplayFunc, tResetFunc resetFunc, tFuncWithController invalidateFunc, tFuncWithController setupFunc, tPrintFunc printFunc, tGetReplayID getReplayIDFunc, tLinkPacketFunc linkFunc, tCheckMonitoredEventFunc expFunc, tCheckTrackingFunc checkFunc, u32 playbackFlags, tUpdateMonitoredPacketFunc updateMonitorFunc)
		: m_playFunction(playFunc)
		, m_playTrackedFunction(NULL)
		, m_preloadFunction(preloadFunc)
		, m_preplayFunction(preplayFunc)
		, m_resetFunction(resetFunc)
		, m_invalidateFunction(invalidateFunc)
		, m_setupFunction(setupFunc)
		, m_printFunc(printFunc)
		, m_getReplayIDFunc(getReplayIDFunc)
		, m_linkPacketsFunc(linkFunc)
		, m_checkExpiryFunc(expFunc)
		, m_UpdateMonitoredPacketFunc(updateMonitorFunc)
		, m_checkTrackingFunc(checkFunc)
		, m_packetSize(0)
		, m_enabled(true)
		, m_playbackFlags(playbackFlags)
		, m_expirePrevious(0)
	{}

	CHistoryPacketController(tEventTrackPlaybackFunc playFunc, tEventPreloadFunc preloadFunc, tEventPreplayFunc preplayFunc, tResetFunc resetFunc, tFuncWithController invalidateFunc, tFuncWithController setupFunc, tPrintFunc printFunc, tGetReplayID getReplayIDFunc, tLinkPacketFunc linkFunc, tCheckMonitoredEventFunc expFunc, tCheckTrackingFunc checkFunc, u32 playbackFlags, tUpdateMonitoredPacketFunc updateMonitorFunc)
		: m_playFunction(NULL)
		, m_playTrackedFunction(playFunc)
		, m_preloadFunction(preloadFunc)
		, m_preplayFunction(preplayFunc)
		, m_resetFunction(resetFunc)
		, m_invalidateFunction(invalidateFunc)
		, m_setupFunction(setupFunc)
		, m_printFunc(printFunc)
		, m_getReplayIDFunc(getReplayIDFunc)
		, m_linkPacketsFunc(linkFunc)
		, m_checkExpiryFunc(expFunc)
		, m_UpdateMonitoredPacketFunc(updateMonitorFunc)
		, m_checkTrackingFunc(checkFunc)
		, m_packetSize(0)
		, m_enabled(true)
		, m_playbackFlags(playbackFlags)
		, m_expirePrevious(0)
	{}


	bool				ShouldPlay(u32 flags) const
	{
		u32 result1 = m_playbackFlags & REPLAY_STATE_MASK & flags;
		u32 result2 = m_playbackFlags & REPLAY_CURSOR_MASK & flags;
		u32 result3 = m_playbackFlags & REPLAY_DIRECTION_MASK & flags;

		return result1 != 0 && result2 != 0 && result3 != 0;
	}
	
	tEventPlaybackFunc		m_playFunction;
	tEventTrackPlaybackFunc	m_playTrackedFunction;
	tEventPreloadFunc		m_preloadFunction;
	tEventPreplayFunc		m_preplayFunction;

	tResetFunc				m_resetFunction;
	tFuncWithController		m_invalidateFunction;
	tFuncWithController		m_setupFunction;
	tPrintFunc				m_printFunc;
	tGetReplayID			m_getReplayIDFunc;

	tLinkPacketFunc			m_linkPacketsFunc;

	tCheckMonitoredEventFunc	m_checkExpiryFunc;
	tUpdateMonitoredPacketFunc	m_UpdateMonitoredPacketFunc;

	tCheckTrackingFunc		m_checkTrackingFunc;
	
	u32						m_packetSize;
	char					m_packetName[PACKETNAMELENGTH];
	bool					m_enabled;

	u32						m_playbackFlags;

	int						m_expirePrevious;

	atMap<u32, int>			m_expireTrackers;
};


const u32 EVENT_SCRATCH_BUFFER_SIZE = 1024*1024;






class CReplayEventManager
{
public:
	CReplayEventManager();
	~CReplayEventManager();

	void	SetInterfaceManager(CReplayInterfaceManager* interfaceManager)		{	m_interfaceManager = interfaceManager;	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename FUNCPTRS>
	void	SetupHistoryPacketController(CHistoryPacketController*& pController, u32 packetID, const char* packetName, u32 packetFlags)
	{
		FUNCPTRS funcPtrs;
		replayAssertf((packetFlags & REPLAY_MONITOR_PACKET_LOW_PRIO && packetFlags & REPLAY_MONITOR_PACKET_HIGH_PRIO) == false, "Wrong monitor flags used to register %s...only use one priority", packetName);

		m_historyPacketControllers[packetID] = CHistoryPacketController(
			funcPtrs.m_playFunction,
			(packetFlags & REPLAY_PRELOAD_PACKET) ? funcPtrs.m_preloadFunction : NULL,
			(packetFlags & REPLAY_PREPLAY_PACKET) ? funcPtrs.m_preplayFunction : NULL,
			funcPtrs.m_resetFunction,
			funcPtrs.m_invalidateFunction,
			funcPtrs.m_setupFunction,
			funcPtrs.m_printFunc,
			&CReplayEventManager::GetReplayIDFromPacket<PACKETTYPE>,
			funcPtrs.m_linkFunc,
			(packetFlags & REPLAY_MONITOR_PACKET_FLAGS) ? funcPtrs.m_checkExpiryFunction : NULL,
			funcPtrs.m_checkTrackingFunc,
			packetFlags,
			(packetFlags & REPLAY_MONITORED_UPDATABLE) ? funcPtrs.m_updateMonitorFunction : NULL);

		pController = m_historyPacketControllers.Access(packetID);
		replayDebugf1("Register %s", packetName);
		pController->m_packetSize = sizeof(PACKETTYPE);
		strcpy_s(pController->m_packetName, PACKETNAMELENGTH, packetName);
	}



	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename ENTITYTYPE, typename FUNCPTRS>
	void	RegisterHistoryPacket(u32 packetID, const char* packetName, u32 packetFlags)
	{
		ValidateEntityCount<ENTITYTYPE>(PACKETTYPE::GetNumEntities(), packetName);

		CHistoryPacketController* pController = m_historyPacketControllers.Access(packetID);

		if(!pController)
		{
			SetupHistoryPacketController<PACKETTYPE, FUNCPTRS>(pController, packetID, packetName, packetFlags);
			replayFatalAssertf(pController, "");
		}
		else
		{
			replayFatalAssertf(false, "Error replay packet ID %i was registered more than once!", packetID);
		}
	}


	//////////////////////////////////////////////////////////////////////////
	void			ResetHistoryPacketsAfterTime(ReplayController& controller, u32 time);
	bool			PlayBackHistory(ReplayController& controller, u32 LastTime, bool entityMayBeAbsent);
	CPacketBase const*	PlayBackHistory(ReplayController& controller, eReplayPacketId packetID, u32 packetSize, const u32 endTimeWindow, bool entityMayBeAbsent);
	void			PlaybackSetup(ReplayController& controller);

	bool			FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& preloadReqCount, const s32 preloadOffsetTime, const u32 systemTime);
	int				PreloadPackets(tPreloadRequestArray& preloadRequests, tPreloadSuccessArray& successfulPreloads, const CReplayState& replayFlags, const u32 replayTime);

	bool			FindPreplayRequests(ReplayController& controller, atArray<const CPacketEvent*>& preplayRequests);
	int				PreplayPackets(const atArray<const CPacketEvent*>& preplayRequests, atArray<s32>& preplaysServiced, const CReplayState& replayFlags, const u32 replayTime, const u32 searchExtentTime);

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	CPacketBase const* ResetPackets(ReplayController& controller, eReplayPacketId packetID, u32 currentGameTime)
	{
		CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		while(controller.GetCurrentPacketID() == packetID)
		{
			PACKETTYPE const* pHistoryPacket = controller.ReadCurrentPacket<PACKETTYPE>();

			if(pHistoryPacket->GetGameTime() > currentGameTime)
				break;

			// Set/reset the status of the packet depending on the direction we're playing in
			if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) && !controller.GetPlaybackFlags().IsSet(REPLAY_STATE_PAUSE))
				pHistoryPacket->SetPlayed(true);
			else if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
				pHistoryPacket->SetPlayed(false);

			controller.AdvancePacket();
			pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		}

		return pBasePacket;
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename ENTITYTYPE>
	eShouldExtract GetEntitiesForPacket(PACKETTYPE const* pPacket, CEventInfo<ENTITYTYPE>& eventInfo, bool entityMayBeAbsent) const
	{
		eventInfo.ResetEntities();
		eShouldExtract extractResult = REPLAY_EXTRACT_SUCCESS;
		for(u8 i = 0; i < pPacket->GetNumEntities(); ++i)
		{
			if(pPacket->GetReplayID(i) != ReplayIDInvalid && pPacket->GetReplayID(i) != NoEntityID)
			{
				ENTITYTYPE* entity = FindEntity<ENTITYTYPE>(pPacket->GetReplayID(i), pPacket->GetStaticIndex(i), eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) && !entityMayBeAbsent);
				if (entity)
				{
					eventInfo.SetEntity(entity, i);
				}
				else if(entityMayBeAbsent)
				{
#if !__NO_OUTPUT
					const char* packetName;
					if (GetPacketName(pPacket->GetPacketID(), packetName))
					{
						replayDebugf1("Can't find entity for %s (ID %x, static index %u)", packetName, pPacket->GetReplayID(i).ToInt(), pPacket->GetStaticIndex(i));
					}
#endif
					extractResult = REPLAY_EXTRACT_FAIL_RETRY;
				}
				else
				{
					extractResult = REPLAY_EXTRACT_FAIL;
				}

			}
			else
			{
				eventInfo.SetEntity(NULL, i);
			}
		}
		return extractResult;
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	eShouldExtract GetEntitiesForPacket(PACKETTYPE const* pPacket, CTrackedEventInfoBase& eventInfo, bool isPreloading) const
	{
		//eventInfo.ResetEntities();	 // Don't do this for tracked?
		eShouldExtract extractResult = REPLAY_EXTRACT_SUCCESS;
		for(u8 i = 0; i < pPacket->GetNumEntities(); ++i)
		{
			if(pPacket->GetReplayID(i) != ReplayIDInvalid && pPacket->GetReplayID(i) != NoEntityID)
			{
				eventInfo.pEntity[i] = FindEntity<CEntity>(pPacket->GetReplayID(i), pPacket->GetStaticIndex(i), eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) && !isPreloading);

				if (eventInfo.pEntity[i] == NULL)
				{
#if !__NO_OUTPUT
					const char* packetName;
					if (GetPacketName(pPacket->GetPacketID(), packetName))
					{
						replayDebugf1("Can't find entity for %s (ID %x, static index %u)", packetName, pPacket->GetReplayID(i).ToInt(), pPacket->GetStaticIndex(i));
					}
#endif
					extractResult = REPLAY_EXTRACT_FAIL_RETRY;
				}
			}
			else
			{
				eventInfo.pEntity[i] = NULL;
			}
		}
		return extractResult;
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename ENTITYTYPE, typename EVENTINFOTYPE>
	ePreloadResult PreloadEventPackets(const CPacketBase* pPacket, const CReplayState& replayFlags, const u32 replayTime, const s32 preloadOffsetTime)
	{
		EVENTINFOTYPE eventInfo;
		eventInfo.SetPlaybackFlags(replayFlags);
		eventInfo.SetReplayTime(replayTime);
		eventInfo.SetPreloadOffsetTime(preloadOffsetTime);

		PACKETTYPE const* pEventPacket = (PACKETTYPE const*)pPacket;
		pEventPacket->ValidatePacket();
		GetEntitiesForPacket(pEventPacket, eventInfo, true);

		if(pEventPacket->template ValidateForPlayback<ENTITYTYPE>(eventInfo))
		{
			return pEventPacket->Preload(eventInfo);
		}

		return PRELOAD_FAIL_MISSING_ENTITY;
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename ENTITYTYPE, typename EVENTINFOTYPE>
	ePreplayResult PreplayEventPackets(const CPacketBase* pPacket, const CReplayState& replayFlags, const u32 replayTime, const s32 searchExtentTime)
	{
		EVENTINFOTYPE eventInfo;
		eventInfo.SetPlaybackFlags(replayFlags);
		eventInfo.SetReplayTime(replayTime);

		PACKETTYPE const* pEventPacket = (PACKETTYPE const*)pPacket;
		pEventPacket->ValidatePacket();

		// First check we're not too early or too late...
		if(eventInfo.GetReplayTime() < (pEventPacket->GetGameTime() - (u32)((float)searchExtentTime * 1.1f)))
		{
			return PREPLAY_FAIL;			// Far too early... (before the scanpoint)
		}
		else if(eventInfo.GetReplayTime() > pEventPacket->GetGameTime())
		{
			return PREPLAY_TOO_LATE;		// Too late...cancel
		}

		// Get the entities for the packet...
		GetEntitiesForPacket(pEventPacket, eventInfo, true);

		if(pEventPacket->template ValidateForPlayback<ENTITYTYPE>(eventInfo))
		{
			if(eventInfo.GetReplayTime() < (pEventPacket->GetGameTime() - pEventPacket->GetPreplayTime(eventInfo)))
			{
				return PREPLAY_WAIT_FOR_TIME;	// Too early...wait a bit
			}
			return pEventPacket->Preplay(eventInfo);
		}

		return PREPLAY_MISSING_ENTITY;		// Wait and see...
	}


	void UpdatePacketPlayState(const CPacketEvent* pEventPacket, const ReplayController& controller, const u32 packetFlags, eShouldExtract extractResult)
	{
		// Set/reset the status of the packet depending on the direction we're playing in
		if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) && extractResult < REPLAY_EXTRACT_FAIL_RETRY)
		{
			pEventPacket->SetPlayed(true);
		}
		else
		{
			// If it's an initial state packet and we're not allowed to play it backwards (the flag)
			// then don't set it to unplayed...this way it won't get played again!
			// NOTE: We should probably be checking we're actually going backwards here...as we're sometimes wrongly unplaying packets (url:bugstar:2657111)
			// I don't want to put this change in at this late stage though as it will affect EVERYTHING
			bool playInitialStatePacketBackWards = controller.GetIsFirstFrame() && (packetFlags & REPLAY_MONITORED_PLAY_BACKWARDS) /*&& controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK)*/;
			if(pEventPacket->IsInitialStatePacket() == false || playInitialStatePacketBackWards)
				pEventPacket->SetPlayed(false);
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Extract basic event packets
	template<typename PACKETTYPE, typename ENTITYTYPE>
	CPacketBase const* ExtractEventPackets(ReplayController& controller, eReplayPacketId packetID, const u32 currentGameTime, const u32 packetFlags, const bool entityMayBeAbsent)
	{
		CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		static CEventInfo<ENTITYTYPE> eventInfo;
		eventInfo.SetPlaybackFlags(controller.GetPlaybackFlags());
		eventInfo.SetReplayTime(currentGameTime);
		eventInfo.SetIsFirstFrame(controller.GetIsFirstFrame());

		while(controller.GetCurrentPacketID() == packetID)
		{
			PACKETTYPE const* pEventPacket = controller.ReadCurrentPacket<PACKETTYPE>();

			if(pEventPacket->GetGameTime() > currentGameTime)
				break;

			// Check the current status of the packet against the flags
			eShouldExtract extractResult = ShouldExtract<PACKETTYPE, ENTITYTYPE>(controller, *pEventPacket, packetFlags, entityMayBeAbsent);
			if(extractResult == REPLAY_EXTRACT_SUCCESS)
			{
				extractResult = GetEntitiesForPacket(pEventPacket, eventInfo, entityMayBeAbsent);

				if(extractResult == REPLAY_EXTRACT_SUCCESS && pEventPacket->template ValidateForPlayback<ENTITYTYPE>(eventInfo))
				{
					pEventPacket->Extract(eventInfo);
				}				
			}

			UpdatePacketPlayState(pEventPacket, controller, packetFlags, extractResult);
			m_eventPlaybackResults |= (u32)extractResult;

			controller.AdvancePacket();
			pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		}

		return pBasePacket;
	}


	//////////////////////////////////////////////////////////////////////////
	// Extract event packets which interpolate to another
	template<typename PACKETTYPE, typename ENTITYTYPE>
	CPacketBase const* ExtractInterpEventPackets(ReplayController& controller, eReplayPacketId packetID, const u32 currentGameTime, const u32 packetFlags, bool entityMayBeAbsent)
	{
		CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		static CInterpEventInfo<PACKETTYPE, ENTITYTYPE> eventInfo;
		eventInfo.SetPlaybackFlags(controller.GetPlaybackFlags());
		eventInfo.SetReplayTime(currentGameTime);
		eventInfo.SetIsFirstFrame(controller.GetIsFirstFrame());

		while(controller.GetCurrentPacketID() == packetID)
		{
			PACKETTYPE const* pEventPacket = controller.ReadCurrentPacket<PACKETTYPE>();

			if(pEventPacket->GetGameTime() > currentGameTime)
				break;

			// Check the current status of the packet against the flags
			eShouldExtract extractResult = ShouldExtract<PACKETTYPE, ENTITYTYPE>(controller, *pEventPacket, packetFlags, entityMayBeAbsent);
			if(extractResult == REPLAY_EXTRACT_SUCCESS)
			{
				// Loop until we find the correct two packets to interpolate between
				// based on the current time.
				PACKETTYPE const* pCurrPacket = pEventPacket;
				PACKETTYPE const* pNextPacket = pEventPacket;
				do
				{
					pCurrPacket = pNextPacket;
					// Get the next packet related to this one
					controller.GetNextPacket(pCurrPacket, pNextPacket);
					eventInfo.SetNextPacket(pNextPacket);
				}
				while(pNextPacket && pNextPacket->GetGameTime() < currentGameTime);

				GetEntitiesForPacket(pCurrPacket, eventInfo, entityMayBeAbsent);

				float interp = 0.0f;
				if(pNextPacket)
					interp = GetInterpValue((float)currentGameTime, (float)pCurrPacket->GetGameTime(), (float)pNextPacket->GetGameTime(), true);
				eventInfo.SetInterp(interp);

				pCurrPacket->Extract(eventInfo);				
			}

			// I don't think we need to do this for interpolation packets as
			// they have to always be played
			//UpdatePacketPlayState(pEventPacket, controller, packetFlags, extractResult);
			m_eventPlaybackResults |= (u32)extractResult;

			controller.AdvancePacket();
			pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		}

		return pBasePacket;
	}


	//////////////////////////////////////////////////////////////////////////
	// Extract event packets which share data (entities mainly)
	#define replayTrackedDebugf	replayDebugf3
	template<typename PACKETTYPE, typename TRACKEDTYPE>
	CPacketBase const* ExtractTrackedEventPackets(ReplayController& controller, const u32 currentGameTime, const u32 packetFlags, bool entityMayBeAbsent)
	{
		PACKETTYPE const* pHistoryPacket = controller.ReadCurrentPacket<PACKETTYPE>();

		eShouldExtract extractResult = ShouldExtract<PACKETTYPE, CEntity>(controller, *pHistoryPacket, packetFlags, entityMayBeAbsent);
		if(extractResult == REPLAY_EXTRACT_SUCCESS)
		{
			s16 trackedID = pHistoryPacket->GetTrackedID();

			if(trackedID == -1)
			{	// No tracking ID so just call extract
				CTrackedEventInfo<TRACKEDTYPE> tempInfo;
				tempInfo.SetPlaybackFlags(controller.GetPlaybackFlags());
				tempInfo.SetReplayTime(currentGameTime);

				GetEntitiesForPacket(pHistoryPacket, tempInfo, false);

				pHistoryPacket->Extract(tempInfo);
			}
			else
			{
				// Check for a trackable using the tracking ID
				CTrackedEventInfo<TRACKEDTYPE>* pTrackedInfo = GetTrackedInfo<TRACKEDTYPE>(trackedID);
				pTrackedInfo->SetIsFirstFrame(controller.GetIsFirstFrame());

				if(!(pTrackedInfo->IsStale() && pHistoryPacket->ShouldStartTracking() == false))
				{
					bool wasNULL = !pTrackedInfo->isAlive();
					if(wasNULL && pHistoryPacket->ShouldStartTracking() == false)
					{
						replayTrackedDebugf("Effect has been stopped outside the replay...too bad %d with packet %p, ", trackedID, pHistoryPacket);
					}
					else
					{
						pTrackedInfo->SetPlaybackFlags(controller.GetPlaybackFlags());
						pTrackedInfo->SetReplayTime(currentGameTime);
						pTrackedInfo->trackingID = trackedID;

						if(GetEntitiesForPacket(pHistoryPacket, *pTrackedInfo, entityMayBeAbsent) == REPLAY_EXTRACT_SUCCESS)
						{
							pHistoryPacket->Extract(*pTrackedInfo);
						}
					}

					if(pTrackedInfo->IsEffectAlive(0) || pTrackedInfo->IsEffectAlive(1))
					{	
						pTrackedInfo->SetStale(false);
						if(wasNULL)
						{
							pTrackedInfo->SetStarted(true);
							replayTrackedDebugf("Created %d with packet %p", trackedID, pHistoryPacket);
						}
						else
						{
							replayTrackedDebugf("Tracked %d with packet %p", trackedID, pHistoryPacket);
						}
					}
					else
					{	
						replayTrackedDebugf("Removed %d with packet %p", trackedID, pHistoryPacket);
						pTrackedInfo->reset();
					}
				}
				else
				{
					replayTrackedDebugf("Stale %d with packet %p", trackedID, pHistoryPacket);
				}
			}			
		}	
		
		UpdatePacketPlayState(pHistoryPacket, controller, packetFlags, extractResult);
		m_eventPlaybackResults |= (u32)extractResult;

		controller.AdvancePacket();
		return controller.ReadCurrentPacket<CPacketBase>();
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename ENTITYTYPE>
	eShouldExtract ShouldExtract(const ReplayController& controller, const PACKETTYPE& packet, const u32 packetFlags, bool entityMyNotExist)
	{
		if(packet.IsCancelled() == true)
		{
			return REPLAY_EXTRACT_FAIL;
		}

		if(controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
		{	// If we're jumping about then entities may not actually exist so allow for this
			// and say we should not extract the event packet.
			for(u8 i = 0; i < packet.GetNumEntities(); ++i)
			{
				if(packet.GetReplayID(i) != ReplayIDInvalid && packet.GetReplayID(i) != NoEntityID)
				{
					if(FindEntity<ENTITYTYPE>(packet.GetReplayID(i), packet.GetStaticIndex(i), false) == NULL)
					{
						return entityMyNotExist ? REPLAY_EXTRACT_FAIL_RETRY : REPLAY_EXTRACT_FAIL;
					}
				}
			}
		}

		return packet.ShouldExtract(controller.GetPlaybackFlags().GetState(), packetFlags, controller.GetIsFirstFrame());
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	void ResetPacketInternal(ReplayController& controller)
	{
		PACKETTYPE* pHistoryPacket = controller.GetCurrentPacket<PACKETTYPE>();
		if (pHistoryPacket->GetReplayID() != -1 && pHistoryPacket->GetStatus() == PFX_PLAYED)
		{
			pHistoryPacket->SetPlayed(false);
		}
		controller.AdvancePacket();
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	void InvalidatePacketInternal(ReplayController& controller)
	{
		controller.EnableModify();
		PACKETTYPE* pPacket = controller.GetCurrentPacket<PACKETTYPE>();
		if (pPacket->ShouldInvalidate())
		{
			pPacket->Invalidate();
		}
		controller.DisableModify();
		controller.AdvancePacket();
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE, typename TRACKEDTYPE>
	void SetupTrackedPacket(ReplayController& controller)
	{
		controller.EnableModify();
		PACKETTYPE* pPacket = controller.GetCurrentPacket<PACKETTYPE>();

		s16 trackedID = pPacket->GetTrackedID();

		if(trackedID > -1)
		{
#if DO_REPLAY_OUTPUT && !__FINAL
			CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)pPacket->GetPacketID());
#endif // DO_REPLAY_OUTPUT

 			bool beforeSetup = IsTrackableBeingTracked<TRACKEDTYPE>(trackedID);
 			bool afterSetup = pPacket->Setup(beforeSetup);
 			if(afterSetup && !beforeSetup)
			{
				NOTFINAL_ONLY(replayTrackedDebugf("Setup %d, %s", trackedID, pPacketController->m_packetName);)
 				SetupTrackable<TRACKEDTYPE>(trackedID, pPacket);
			}
 			else if(!beforeSetup && !afterSetup)
			{
				NOTFINAL_ONLY(replayTrackedDebugf("Cancel %d, %s", trackedID, pPacketController->m_packetName);)
				pPacket->Cancel();
			}
			else if(beforeSetup && !afterSetup)
 			{
				NOTFINAL_ONLY(replayTrackedDebugf("Clear %d, %s", trackedID, pPacketController->m_packetName);)
				ClearTrackable<TRACKEDTYPE>(trackedID);
			}
			else
			{
				replayAssert(beforeSetup && afterSetup);
				if(pPacket->ShouldStartTracking())
				{
					CPacketEventTracked* pLastTracked = GetLastTracked<TRACKEDTYPE>(trackedID);
					replayAssert(pLastTracked);
					pLastTracked->ValidatePacket();
					pLastTracked->SetEndTracking();
					ClearTrackable<TRACKEDTYPE>(trackedID);
					SetupTrackable<TRACKEDTYPE>(trackedID, pPacket);

					NOTFINAL_ONLY(replayTrackedDebugf("Clear and set up %d, %s", trackedID, pPacketController->m_packetName);)
				}
				else
				{
					NOTFINAL_ONLY(replayTrackedDebugf("Update %d, %s", trackedID, pPacketController->m_packetName);)
					UpdateTrackable<TRACKEDTYPE>(trackedID, pPacket);
				}
			}
		}
		controller.DisableModify();

		SetupPacket<PACKETTYPE>(controller);
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	void SetupLinkedPacket(ReplayController& controller)
	{
		controller.EnableModify();
		PACKETTYPE* pPacket = controller.GetCurrentPacket<PACKETTYPE>();

		if(pPacket->ShouldInterp() && pPacket->GetNextPosition() != (u32)-1)
		{
			u16 nextBlock = pPacket->GetNextBlockIndex();
			u32 nextPosition = pPacket->GetNextPosition();
			CBlockInfo* pBlock = controller.GetBufferInfo().FindBlockFromSessionIndex(nextBlock);
			if(pBlock)
			{
				u8* p = pBlock->GetData();
				p += nextPosition;
				PACKETTYPE* pNextPacket = (PACKETTYPE*)p;
				pNextPacket->ValidatePacket();
			}
		}
		
		controller.DisableModify();

		SetupPacket<PACKETTYPE>(controller);
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	void SetupPacket(ReplayController& controller)
	{
		controller.EnableModify();
		PACKETTYPE* pPacket = controller.GetCurrentPacket<PACKETTYPE>();

		if(pPacket->IsStaticGeometryAware())
		{
			for(u8 i = 0; i < pPacket->GetNumEntities(); ++i)
			{
				if(pPacket->GetStaticIndex(i) == NON_STATIC_BOUNDS_BUILDING)
				{
					m_buildingHashes[pPacket->GetReplayID(i)] = NULL;
				}
			}
		}

		controller.DisableModify();
	}


	//////////////////////////////////////////////////////////////////////////
	template<typename PACKETTYPE>
	void	PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
	{
		PACKETTYPE const* pPacket = controller.ReadCurrentPacket<PACKETTYPE>();

		const char* packetName = NULL;
		GetPacketName(pPacket->GetPacketID(), packetName);
		replayAssertf(packetName, "Failed to find packet name");

		if(mode == REPLAY_OUTPUT_DISPLAY)
		{
			replayDebugf1("Packet %s", packetName);

			pPacket->Print();

			replayDebugf1("");
		}
		else if(mode == REPLAY_OUTPUT_XML)
		{
			char str[1024];
			snprintf(str, 1024, "\t<Packet>\n");
			CFileMgr::Write(handle, str, istrlen(str));

			snprintf(str, 1024, "\t\t<Position>\n");
			CFileMgr::Write(handle, str, istrlen(str));

			snprintf(str, 1024, "\t\t\t<Block>%u</Block>\n", controller.GetCurrentBlockIndex());
			CFileMgr::Write(handle, str, istrlen(str));

			snprintf(str, 1024, "\t\t\t<Address>%p</Address>\n", pPacket);
			CFileMgr::Write(handle, str, istrlen(str));

			snprintf(str, 1024, "\t\t</Position>\n");
			CFileMgr::Write(handle, str, istrlen(str));


			snprintf(str, 1024, "\t\t<Name>%s</Name>\n", packetName);
			CFileMgr::Write(handle, str, istrlen(str));

			pPacket->PrintXML(handle);

			snprintf(str, 1024, "\t</Packet>\n");
			CFileMgr::Write(handle, str, istrlen(str));
		}

		controller.AdvancePacket();
	}

	eReplayClipTraversalStatus	PrintOutHistoryPackets(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

#if GTA_REPLAY_OVERLAY
	eReplayClipTraversalStatus CalculateBlockStats(ReplayController& controller, CBlockInfo* pBlock);
	void GeneratePacketNameList(eReplayPacketId packetID, const char*& packetName);
#endif //GTA_REPLAY_OVERLAY

	void	ResetPacket(ReplayController& controller, eReplayPacketId packetID, u32 packetSize, u32 uTime);
	void	InvalidatePacket(ReplayController& controller, eReplayPacketId packetID, u32 packetSize);

	bool	IsRelevantHistoryPacket(eReplayPacketId packetID);

#if __BANK
	void	AddDebugWidgets();
#endif //__BANK

	bool	ShouldPlayBackPacket(eReplayPacketId packetId, u32 playBackFlags, s32 replayId, u32 packetTime, u32 startTimeWindow);

	template<typename TYPE>
	void	AddExpiryInfo(eReplayPacketId packetId, CTrackedEventInfo<TYPE>& eventInfoIn);

	template<typename TYPE>
	void	TrackRecordingEvent(const CTrackedEventInfo<TYPE>& eventInfoIn, CTrackedEventInfo<TYPE>*& pEventInfoOut, const bool allowNew, const bool endTracking, eReplayPacketId packetID);
	template<typename TYPE>
	void	SetTrackedStoredInScratch(const CTrackedEventInfo<TYPE>& eventInfo);

	template<typename PACKETTYPE, typename TRACKEDTYPE>
	void	LinkEventPackets(CPacketBase* pBasePacket, ReplayController& controller)
	{
		PACKETTYPE* pPacket = (PACKETTYPE*)pBasePacket;
		REPLAY_CHECK(pPacket->ValidatePacket(), NO_RETURN_VALUE, "Failed to validate packet in LinkEventPackets");

		// Find tracked info from packet tracked info
		CTrackedEventInfo<TRACKEDTYPE>* pTrackedInfo = GetTrackedInfo<TRACKEDTYPE>(pPacket->GetTrackedID());
		if(pTrackedInfo != NULL)
		{
			PACKETTYPE* pPrevPacket = NULL;
			if(pTrackedInfo->pBlockOfPrevPacket)
			{
				const u8* p = NULL;
				const CBlockInfo* pPrevPacketsBlock = pTrackedInfo->pBlockOfPrevPacket;
				const CBlockInfo* pCurrPacketsBlock = controller.GetCurrentBlock();

				// There are 3 different scenarios here...
				// 1. The previous packet is in the same memory block we're in
				// 2. The previous packet is in the same session block, but different memory block (temp buffer switch)
				// 3. The previous packet is in a different session block
				// 
				
				// If it's a different session block...
				if(pPrevPacketsBlock->GetSessionBlockIndex() != pCurrPacketsBlock->GetSessionBlockIndex())
				{
					// We've changed session block...but was the previous a temp block with the current not?
					if(pPrevPacketsBlock->IsTempBlock() == true && pCurrPacketsBlock->IsTempBlock() == false)
					{
						// We're in a non-temp block so the temp block before would have been copied into a normal block so get that...
						CBlockInfo* pBlock = controller.GetBufferInfo().GetPrevBlock(pCurrPacketsBlock);
						p = pBlock->GetData();
					}
					else if(pPrevPacketsBlock->IsTempBlock() == false && pCurrPacketsBlock->IsTempBlock() == true)
					{
						// We're in a temp block but the prev was in a normal block...this is fine...
						p = pPrevPacketsBlock->GetData();
					}
					else
					{
						// Both prev and curr blocks were temp or normal so again leave as is...
						p = pPrevPacketsBlock->GetData();
					}
				}
				else
				{
					// Same session block...so we've copied into the temp or normal block recently
					// or we just haven't changed block (in which case pPrevPacketsBlock == pCurrPacketsBlock)
					// Simply use the current block
					p = pCurrPacketsBlock->GetData();
				}


				p += pTrackedInfo->offsetOfPrevPacket;
				if(*(tPacketID*)p == pPacket->GetPacketID())
				{
					pPrevPacket = (PACKETTYPE*)(p);
					if(!pPrevPacket->ValidatePacket())
					{
						DumpReplayPackets((char*)pPrevPacket);
						replayDebugf1("Block of prev packet: M:%d, S:%d, IsTemp:%s", pPrevPacketsBlock->GetMemoryBlockIndex(), pPrevPacketsBlock->GetSessionBlockIndex(), pPrevPacketsBlock->IsTempBlock() ? "Y" : "N");
						replayDebugf1("Offset of prev packet: %d", pTrackedInfo->offsetOfPrevPacket);
						replayDebugf1("Block of curr packet: M:%d, S:%d, IsTemp:%s", pCurrPacketsBlock->GetMemoryBlockIndex(), pCurrPacketsBlock->GetSessionBlockIndex(), pCurrPacketsBlock->IsTempBlock() ? "Y" : "N");

						// Something went wrong....run away!
						FreeTrackableDuringRecording(pTrackedInfo);
						REPLAY_CHECK(false, NO_RETURN_VALUE, "Failed check on 'previous' event packet");
					}
				}
			}

			replayFatalAssertf(controller.GetCurrentPosition() >= pPacket->GetPacketSize(), "Position is not far enough along to have stored the packet...");
			
			// Set the tracking info correctly if this is an interp packet...
			pTrackedInfo->SetInterp(pPacket->ShouldInterp());
			if(pTrackedInfo->IsInterp())
			{	
				pTrackedInfo->pBlockOfPrevPacket = controller.GetCurrentBlock();
				pTrackedInfo->offsetOfPrevPacket = controller.GetCurrentPosition();

				if(pPrevPacket && pPrevPacket->ShouldInterp() && pTrackedInfo->pBlockOfPrevPacket != NULL)
				{
					pPrevPacket->SetNextOffset(pTrackedInfo->pBlockOfPrevPacket->GetSessionBlockIndex(), pTrackedInfo->offsetOfPrevPacket);

					// Double check the packet we just stored...
					if(pTrackedInfo->pBlockOfPrevPacket->IsTempBlock() == false)
					{
						if(pPrevPacket && pPrevPacket->ShouldInterp())
						{
							u16 nextBlock = pPrevPacket->GetNextBlockIndex();
							u32 nextPosition = pPrevPacket->GetNextPosition();
							u8* p = controller.GetBufferInfo().FindBlockFromSessionIndex(nextBlock)->GetData();
							p += nextPosition;
							PACKETTYPE* pNextPacket = (PACKETTYPE*)p;
							if(!pNextPacket->ValidatePacket())
							{
								DumpReplayPackets((char*)pPacket);
								DumpReplayPackets((char*)pNextPacket);
						
								// Something went wrong....run away!
								FreeTrackableDuringRecording(pTrackedInfo);
								REPLAY_CHECK(false, NO_RETURN_VALUE, "Failed check on setting 'next' event packet");
							}


							p = pTrackedInfo->pBlockOfPrevPacket->GetData();
							p += pTrackedInfo->offsetOfPrevPacket;
							if(*(tPacketID*)p == pPacket->GetPacketID())
							{
								PACKETTYPE* pPrevPacket2 = (PACKETTYPE*)(p);
								if(!pPrevPacket2->ValidatePacket())
								{
									DumpReplayPackets((char*)pPacket);
									DumpReplayPackets((char*)pPrevPacket2);

									// Something went wrong....run away!
									FreeTrackableDuringRecording(pTrackedInfo);
									REPLAY_CHECK(false, NO_RETURN_VALUE, "Failed check on setting 'previous' event packet");
								}
							}
						}
					}
				}
			}

			pTrackedInfo->updatedThisFrame = true;

			// Flags the trackable that it should be free'd this frame.
			if(pPacket->ShouldEndTracking())
				FreeTrackableDuringRecording(pTrackedInfo);
		}
	}

	bool	GetPacketName(s32 packetID, const char*& packetName) const;
	void	GetBlockDetails(char* pStr, bool& err) const;

	void	ReleaseAndClear();

	float   GetInterpValue(float currVal, float startVal, float endVal, bool clamp);

	bool	IsRecorderNearlyFull(const CReplayRecorder& recorder) const
	{
		return recorder.GetMemUsed() > (u32)((float)recorder.GetBufferSize() * MONITOR_BUFFER_SAFETY_SIZE);
	}

	template<typename PACKETTYPE, typename TRACKEDTYPE>
	void	CheckTracking(const CPacketEvent* pPacket)
	{
		PACKETTYPE* pTrackedPacket = (PACKETTYPE*)pPacket;
		CTrackedEventInfo<TRACKEDTYPE>* pTrackedInfo = GetTrackedInfo<TRACKEDTYPE>(pTrackedPacket->GetTrackedID());
		if(pTrackedPacket->ShouldEndTracking())
			FreeTrackableDuringRecording(pTrackedInfo);
	}

	template<typename PACKETTYPE>
	void	StoreEvent(PACKETTYPE& packet, bool recordPacket, bool monitorPacket)
	{
		sysCriticalSection cs(m_eventBufferCS);

		packet.SetGameTime(fwTimer::GetReplayTimeInMilliseconds());

		if(recordPacket)
		{
			m_eventMemFrame += packet.GetPacketSize();

			if(m_eventRecorder.IsSpaceForPacket(packet.GetPacketSize()) == false)
			{
				return;
			}

			m_eventRecorder.RecordPacket(packet);
			u32 memUsed = m_eventRecorder.GetMemUsed();
			if (memUsed > m_eventMemPeak)
			{
				if (IsRecorderNearlyFull(m_eventRecorder))
				{
					replayDebugf1("Event pool peak: %u", memUsed);
					replayAssertf(false, "Replay event pool nearly full! Event pool peak: %u", memUsed);
				}
				m_eventMemPeak = memUsed;
			}
		}

		if(monitorPacket)
		{
			CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packet.GetPacketID());
			if(pPacketController == NULL || !(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_FLAGS))
				return;

			if(packet.ShouldInterp())
			{
				BANK_ONLY(sysStack::PrintStackTrace());
				replayDebugf1("Interp event packets such as this cannot be monitored");
				return;
			}

			// If we only allow one of these packets to be monitored then increment this guard here
			if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_JUST_ONE)
			{
				++pPacketController->m_expirePrevious;
			}
			else if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_ONE_PER_EFFECT)
			{
				if(pPacketController->m_expireTrackers.Access(packet.GetExpiryTrackerID()) == nullptr)
				{
					pPacketController->m_expireTrackers[packet.GetExpiryTrackerID()] = 1;
				}
				else
				{
					++pPacketController->m_expireTrackers[packet.GetExpiryTrackerID()];
				}
			}

			// If we have the 'frame' monitor flag then record in the frame monitor buffer
			if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_FRAME)
			{
				if(m_frameMonitorBuffer.GetRecorder().IsSpaceForPacket(packet.GetPacketSize()) == false)
				{
					DumpMonitoredEventsToTTY(m_frameMonitorBuffer);
					replayDebugf1("Frame Monitor full...see Log for packets");
				}

				PACKETTYPE* pMonitoredPacket = m_frameMonitorBuffer.RecordPacket(packet);
				if(pMonitoredPacket != NULL)
				{
					if(m_frameMonitorBuffer.GetUsed() > m_frameMonitorBuffer.GetMemPeak())
					{
						if(IsRecorderNearlyFull(m_frameMonitorBuffer.GetRecorder()))
						{
							DumpMonitoredEventsToTTY(m_frameMonitorBuffer);
							replayDebugf1("Frame Monitor nearly full...see Log for packets");
							replayDebugf1("Event monitor peak: %u", m_frameMonitorBuffer.GetUsed());
						}
						m_frameMonitorBuffer.SetMemPeak(m_frameMonitorBuffer.GetUsed());
					}
				}
			}
			else // Else record in the 'block' monitor buffer
			{
				if(m_blockMonitorBuffer.GetRecorder().IsSpaceForPacket(packet.GetPacketSize()) == false)
				{
					DumpMonitoredEventsToTTY(m_blockMonitorBuffer);
					replayDebugf1("Block Monitor full...see TTY for packets");
					return;
				}

				packet.SetIsInitialStatePacket(true);
				PACKETTYPE* pMonitoredPacket = m_blockMonitorBuffer.RecordPacket(packet);
				if(pMonitoredPacket != NULL)
				{
					if(m_blockMonitorBuffer.GetUsed() > m_blockMonitorBuffer.GetMemPeak())
					{
						if(IsRecorderNearlyFull(m_blockMonitorBuffer.GetRecorder()))
						{
							DumpMonitoredEventsToTTY(m_blockMonitorBuffer);
							replayDebugf1("Block Monitor nearly full...see TTY for packets");
							replayDebugf1("Event monitor peak: %u", m_blockMonitorBuffer.GetUsed());
						}
						m_blockMonitorBuffer.SetMemPeak(m_blockMonitorBuffer.GetUsed());
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Used in monitoring normal single use event packets to determine when
	// they should be removed from the monitor buffer
	template<typename PACKETTYPE, typename ENTITYTYPE>
	bool				HasEventExpired(const CPacketEvent* pEventPacket, CHistoryPacketController& controller)
	{
		const PACKETTYPE* pPacket = (const PACKETTYPE*)pEventPacket;
		pPacket->ValidatePacket();

		// If we only allow on of these packets in the monitor buffer then see if we have
		if(controller.m_playbackFlags & REPLAY_MONITOR_PACKET_JUST_ONE)
		{
			if(controller.m_expirePrevious > 1)
			{
				--controller.m_expirePrevious;
				return true;
			}
		}
		else if(controller.m_playbackFlags & REPLAY_MONITOR_PACKET_ONE_PER_EFFECT)
		{
			if(controller.m_expireTrackers[pPacket->GetExpiryTrackerID()] > 1)
			{
				--controller.m_expireTrackers[pPacket->GetExpiryTrackerID()];
				return true;
			}
		}

		CEventInfo<ENTITYTYPE> eventInfo;
		if(pPacket->NeedsEntitiesForExpiryCheck())
		{	// Only do this for the packets that really need it.
			if(GetEntitiesForPacket(pPacket, eventInfo, false) != REPLAY_EXTRACT_SUCCESS)
				return true;
		}

		return pPacket->HasExpired(eventInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	// Same as above function except for tracked event packets
	template<typename PACKETTYPE, typename TRACKEDTYPE>
	bool				HasTrackedEventExpired(const CPacketEvent* pEventPacket, CHistoryPacketController& controller)
	{
		const PACKETTYPE* pPacket = (const PACKETTYPE*)pEventPacket;
		pPacket->ValidatePacket();

		CTrackedEventInfo<TRACKEDTYPE>* pTrackedInfo = GetTrackedInfo<TRACKEDTYPE>(pPacket->GetTrackedID());
		if(!pTrackedInfo)	// No tracking info so expire
			return true;

		// If we only allow on of these packets in the monitor buffer then see if we have
		if(controller.m_playbackFlags & REPLAY_MONITOR_PACKET_JUST_ONE)
		{
			const ExpiryInfo* expiryInfo = pTrackedInfo->GetExpiryInfo(pPacket->GetPacketID());

			if(expiryInfo && expiryInfo->HasExpired())
			{
				return true;
			}
		}

		bool result = false;

		CTrackedEventInfo<TRACKEDTYPE> info = *pTrackedInfo;
		if(pPacket->NeedsEntitiesForExpiryCheck())
		{	// Only do this for the packets that really need it.
			if(GetEntitiesForPacket(pPacket, info, false) != REPLAY_EXTRACT_SUCCESS)
				result = true;
		}

		if(!result)
			result = pPacket->HasExpired(info);

		if(result && (controller.m_playbackFlags & REPLAY_MONITOR_PACKET_JUST_ONE) && (pPacket->GetPacketID() == PACKETID_FORCE_ROOM_FOR_ENTITY))
		{
			// This Packet doesn't have a 'stop' to free up the trackable so handle it here
			const ExpiryInfo* expiryInfo = pTrackedInfo->GetExpiryInfo(pPacket->GetPacketID());
			if(!expiryInfo || expiryInfo->GetCount() == 1)
			{
				FreeTrackableDuringRecording<TRACKEDTYPE>(pTrackedInfo);
			}
		}
		else if(result)
		{
			// Free the trackable for the expired packet.
			// Any packets that require the same tracking information will also expire...which
			// they would have done anyway as they'd have had the same requirements.
			FreeTrackableDuringRecording<TRACKEDTYPE>(pTrackedInfo);
		}

		return result;
	}

	//////////////////////////////////////////////////////////////////////////
	// Used in monitoring normal single use event packets to determine when
	// they should be removed from the monitor buffer
	template<typename PACKETTYPE, typename ENTITYTYPE>
	void				UpdateMonitorPacket(CPacketEvent* pEventPacket, CHistoryPacketController& /*controller*/)
	{
		PACKETTYPE* pPacket = (PACKETTYPE*)pEventPacket;
		pPacket->ValidatePacket();
		pPacket->UpdateMonitorPacket();
	}

	//////////////////////////////////////////////////////////////////////////
	// Same as above function except for tracked event packets
	template<typename PACKETTYPE, typename TRACKEDTYPE>
	void				UpdateTrackedMonitorPacket(CPacketEvent* pEventPacket, CHistoryPacketController& /*controller*/)
	{
		PACKETTYPE* pPacket = (PACKETTYPE*)pEventPacket;
		pPacket->ValidatePacket();
		pPacket->UpdateMonitorPacket();
	}


	bool	ShouldProceedWithPacket(const CPacketEvent& packet, bool recordPacket, bool monitorPacket) const;
	void	CheckMonitoredEvents(bool notRecording);
	void	CheckMonitoredEvents(CReplayPacketBuffer& buffer, bool notRecording);
	void	DumpMonitoredEventsToTTY(bool force = false) const;
	void	DumpMonitoredEventsToTTY(const CReplayPacketBuffer& buffer, bool force = false) const;
	void	RecordEvents(ReplayController& controller, bool blockChange, u32 expectedSize);
	void	GetMonitorStats(u32& count, u32& bytes, u32& count1, u32& bytes1) const;

	void						FreeTrackablesThisFrameDuringRecording();
	void						ClearAllRecordingTrackables();
	void						CleanAllTrackables();
	void						SetPlaybackTrackablesStale();
	void						ResetSetupInformation();
	void						CheckAllTrackablesDuringRecording();
	s32							GetTrackablesCount() const;
	void						RemoveInterpEvents();

	template<typename TRACKEDTYPE>
	CTrackedEventInfo<TRACKEDTYPE>*	GetTrackedInfo(s16 id)							{	return GetController<TRACKEDTYPE>().GetTrackedInfo(id);		}
	template<typename TRACKEDTYPE>
	const CTrackedEventInfo<TRACKEDTYPE>*	GetTrackedInfo(s16 id) const					{	return GetController<TRACKEDTYPE>().GetTrackedInfo(id);		}
	template<typename TRACKEDTYPE>
	void						SetupTrackable(s16 id, CPacketEventTracked* pPacket){	GetController<TRACKEDTYPE>().SetupTrackable(id, pPacket);	}
	template<typename TRACKEDTYPE>
	void						ClearTrackable(s16 id)								{	GetController<TRACKEDTYPE>().ClearTrackable(id);			}
	template<typename TRACKEDTYPE>
	bool						IsTrackableBeingTracked(s16 id) const				{	return GetController<TRACKEDTYPE>().IsTrackableBeingTracked(id);	}
	template<typename TRACKEDTYPE>
	void						UpdateTrackable(s16 id, CPacketEventTracked* pPacket){	GetController<TRACKEDTYPE>().UpdateTrackable(id, pPacket);	}
	template<typename TRACKEDTYPE>
	CPacketEventTracked*		GetLastTracked(s16 id)								{	return GetController<TRACKEDTYPE>().GetLastTracked(id);		}
	template<typename TRACKEDTYPE>
	void						FreeTrackableDuringRecording(CTrackedEventInfo<TRACKEDTYPE>* pTrackable)	{	GetController<TRACKEDTYPE>().FreeTrackableDuringRecording(pTrackable);	}
	template<typename TRACKEDTYPE>
	int							GetTrackedCount() const								{	return GetController<TRACKEDTYPE>().GetTrackablesCount();	}

	template<typename TRACKEDTYPE>
	CReplayTrackingController<TRACKEDTYPE>&	GetController();
	template<typename TRACKEDTYPE>
	const CReplayTrackingController<TRACKEDTYPE>&	GetController() const;

	void	Reset();
	void	Cleanup();
	void	ClearCurrentEvents();

	void	Lock()						{ m_eventBufferCS.Lock(); }	// Unlock once we've actually recorded the events
	u32		GetEventBufferReq(bool blockChange) const
	{
		return m_eventRecorder.GetMemUsed() + m_frameMonitorBuffer.GetUsed() + (blockChange == true ? m_blockMonitorBuffer.GetUsed() : 0);
	}
	
	u32		GetEventCount() const		{	return m_eventCount;	}

	void	ResetPlaybackResults()		{	m_eventPlaybackResults = 0;		}
	bool	IsRetryingEvents() const	{	return (m_eventPlaybackResults & REPLAY_EXTRACT_FAIL_RETRY) != 0; }

	void	ResetStaticBuildingMap()					{	m_buildingHashes.Reset();						}
	int		GetStaticBuildingRequiredCount() const		{	return m_buildingHashes.GetNumUsed();			}
	bool	IsStaticBuildingRequired(u32 hash) const	{	return m_buildingHashes.Access(hash) != NULL;	}

private:

 	template<typename ENTITYTYPE>
 	ENTITYTYPE*	FindEntity(s32 entityID, u16, bool mustExist = true) const
 	{
 		if(entityID == ReplayIDInvalid || entityID == NoEntityID)
 			return NULL;
 
		CReplayID id(entityID);
		CEntityGet<ENTITYTYPE> entityGet(id);
 		m_interfaceManager->GetEntity<ENTITYTYPE>(entityGet);
		ENTITYTYPE* pEntity = entityGet.m_pEntity;
 		
 		if(mustExist)
 		{
 			if(!pEntity && entityGet.m_alreadyReported)
 			{
 				replayDebugf3("Creation of entity 0x%08X failed so it won't be retrieved...see log", entityID);
 				return NULL;
 			}
 			REPLAY_CHECK(pEntity != NULL, NULL, "Failed to find entity for event 0x%X", entityID);
 		}
 		return pEntity;
 	}
	
	template<typename ENTITYTYPE>
	void	ValidateEntityCount(u8 ASSERT_ONLY(numEntities), const char* ASSERT_ONLY(pPacketName))
	{
		replayFatalAssertf(numEntities > 0, "Error validating %s - Number of entities is incorrect", pPacketName);
	}

	template<typename PACKETTYPE>
	bool	GetReplayIDFromPacket(const CPacketBase* pPacketData, u8 index, CReplayID& replayID) const
	{
		const PACKETTYPE* pPacket = (const PACKETTYPE*)pPacketData;
		pPacket->ValidatePacket();
		if(pPacket->GetNumEntities() <= index)
			return false;

		if(pPacket->IsStaticGeometryAware() && pPacket->GetStaticIndex(index) != InvalidStaticIndex)
		{
			replayID = ReplayIDInvalid;
			return true;
		}

		replayID = pPacket->GetReplayID(index);
		return true;
	}

	u8*		GetCurrentEventBuffer()		{	return m_pEventBuffers[m_currentEventBuffer];	}
	void	CycleEventBuffers()
	{	
		replayAssertf(m_eventMemFrame <= EVENT_BUFFER_SIZE, "We couldn't store all the events...increase the event pool...we needed %u", m_eventMemFrame);

		m_currentEventBuffer = ((m_currentEventBuffer + 1) % NUM_EVENT_BUFFERS);

		m_eventRecorder = CReplayRecorder(GetCurrentEventBuffer(), EVENT_BUFFER_SIZE);
		m_eventMemFrame = 0;
	}
	
	void	ProcessRecordedEvents(ReplayController& controller, u32 endPosition);

	CReplayInterfaceManager*				m_interfaceManager;

	atMap<u32, CHistoryPacketController>	m_historyPacketControllers;

	CReplayTrackingController<tTrackedSoundType> m_audioTrackingController;
	CReplayTrackingController<tTrackedSceneType> m_sceneTrackingController;
	CReplayTrackingController<ptxEffectInst*>	m_ptfxTrackingController;
	CReplayTrackingController<ptxEffectRef>		m_ptfxRefTrackingController;
	CReplayTrackingController<ptxFireRef>		m_fireRefTrackingController;
	CReplayTrackingController<tTrackedDecalType>	m_decalTrackingController;

	u8*										m_pEventBuffers[NUM_EVENT_BUFFERS];
	u8										m_currentEventBuffer;
	CReplayRecorder							m_eventRecorder;
	mutable sysCriticalSectionToken			m_eventBufferCS;
	u32										m_eventMemUsed;
	u32										m_eventMemPeak;
	u32										m_eventCount;
	u32										m_eventMemFrame;

	CReplayPacketBuffer						m_blockMonitorBuffer;
	CReplayPacketBuffer						m_frameMonitorBuffer;

	u32										m_eventPlaybackResults;

	atMap<u32, void*>						m_buildingHashes;
};

template<>
CEntity* CReplayEventManager::FindEntity<CEntity>(s32 entityID, u16, bool mustExist) const;
template<>
void* CReplayEventManager::FindEntity<void>(s32, u16, bool) const;

template<typename PACKETTYPE, typename ENTITYTYPE, typename TRACKEDTYPE>
struct EventTrackedPacketFuncPtrs
{
	EventTrackedPacketFuncPtrs()
		: m_playFunction(&CReplayEventManager::ExtractTrackedEventPackets<PACKETTYPE, TRACKEDTYPE>)
		, m_preloadFunction(&CReplayEventManager::PreloadEventPackets<PACKETTYPE, ENTITYTYPE, CTrackedEventInfo<TRACKEDTYPE>>)
		, m_preplayFunction(&CReplayEventManager::PreplayEventPackets<PACKETTYPE, ENTITYTYPE, CTrackedEventInfo<TRACKEDTYPE>>)
		, m_checkExpiryFunction(&CReplayEventManager::HasTrackedEventExpired<PACKETTYPE, TRACKEDTYPE>)
		, m_resetFunction(&CReplayEventManager::ResetPackets<PACKETTYPE>)
		, m_invalidateFunction(&CReplayEventManager::InvalidatePacketInternal<PACKETTYPE>)
		, m_setupFunction(&CReplayEventManager::SetupTrackedPacket<PACKETTYPE, TRACKEDTYPE>)
		, m_printFunc(&CReplayEventManager::PrintOutPacket<PACKETTYPE>)
		, m_linkFunc(&CReplayEventManager::LinkEventPackets<PACKETTYPE, TRACKEDTYPE>)
		, m_checkTrackingFunc(&CReplayEventManager::CheckTracking<PACKETTYPE, TRACKEDTYPE>)
		, m_updateMonitorFunction(&CReplayEventManager::UpdateTrackedMonitorPacket<PACKETTYPE, TRACKEDTYPE>)
	{}

	tEventTrackPlaybackFunc	m_playFunction;
	tEventPreloadFunc		m_preloadFunction;
	tEventPreplayFunc		m_preplayFunction;

	tCheckMonitoredEventFunc	m_checkExpiryFunction;
	tUpdateMonitoredPacketFunc	m_updateMonitorFunction;

	tResetFunc				m_resetFunction;
	tFuncWithController		m_invalidateFunction;
	tFuncWithController		m_setupFunction;
	tPrintFunc				m_printFunc;

	tLinkPacketFunc			m_linkFunc;

	tCheckTrackingFunc		m_checkTrackingFunc;
};

template<typename PACKETTYPE, typename ENTITYTYPE, typename TRACKEDTYPE>
struct EventInterpPacketFuncPtrs
{
	EventInterpPacketFuncPtrs()
		: m_playFunction(&CReplayEventManager::ExtractInterpEventPackets<PACKETTYPE, ENTITYTYPE>)
		, m_preloadFunction(&CReplayEventManager::PreloadEventPackets<PACKETTYPE, ENTITYTYPE, CEventInfo<ENTITYTYPE>>)
		, m_preplayFunction(&CReplayEventManager::PreplayEventPackets<PACKETTYPE, ENTITYTYPE, CEventInfo<ENTITYTYPE>>)
		, m_checkExpiryFunction(&CReplayEventManager::HasTrackedEventExpired<PACKETTYPE, TRACKEDTYPE>)
		, m_resetFunction(&CReplayEventManager::ResetPackets<PACKETTYPE>)
		, m_invalidateFunction(&CReplayEventManager::InvalidatePacketInternal<PACKETTYPE>)
		, m_setupFunction(&CReplayEventManager::SetupLinkedPacket<PACKETTYPE>)
		, m_printFunc(&CReplayEventManager::PrintOutPacket<PACKETTYPE>)
		, m_linkFunc(&CReplayEventManager::LinkEventPackets<PACKETTYPE, TRACKEDTYPE>)
		, m_checkTrackingFunc(&CReplayEventManager::CheckTracking<PACKETTYPE, TRACKEDTYPE>)
		, m_updateMonitorFunction(&CReplayEventManager::UpdateTrackedMonitorPacket<PACKETTYPE, TRACKEDTYPE>)
	{}

	tEventPlaybackFunc		m_playFunction;
	tEventPreloadFunc		m_preloadFunction;
	tEventPreplayFunc		m_preplayFunction;

	tCheckMonitoredEventFunc	m_checkExpiryFunction;
	tUpdateMonitoredPacketFunc	m_updateMonitorFunction;

	tResetFunc				m_resetFunction;
	tFuncWithController		m_invalidateFunction;
	tFuncWithController		m_setupFunction;
	tPrintFunc				m_printFunc;

	tLinkPacketFunc			m_linkFunc;

	tCheckTrackingFunc		m_checkTrackingFunc;
};

template<typename PACKETTYPE, typename ENTITYTYPE>
struct EventPacketFuncPtrs
{
	EventPacketFuncPtrs()
		: m_playFunction(&CReplayEventManager::ExtractEventPackets<PACKETTYPE, ENTITYTYPE>)
		, m_preloadFunction(&CReplayEventManager::PreloadEventPackets<PACKETTYPE, ENTITYTYPE, CEventInfo<ENTITYTYPE>>)
		, m_preplayFunction(&CReplayEventManager::PreplayEventPackets<PACKETTYPE, ENTITYTYPE, CEventInfo<ENTITYTYPE>>)
		, m_checkExpiryFunction(&CReplayEventManager::HasEventExpired<PACKETTYPE, ENTITYTYPE>)
		, m_resetFunction(&CReplayEventManager::ResetPackets<PACKETTYPE>)
		, m_invalidateFunction(&CReplayEventManager::InvalidatePacketInternal<PACKETTYPE>)
		, m_setupFunction(&CReplayEventManager::SetupPacket<PACKETTYPE>)
		, m_printFunc(&CReplayEventManager::PrintOutPacket<PACKETTYPE>)
		, m_linkFunc(NULL)
		, m_checkTrackingFunc(NULL)
		, m_updateMonitorFunction(&CReplayEventManager::UpdateMonitorPacket<PACKETTYPE, ENTITYTYPE>)
	{}

	tEventPlaybackFunc		m_playFunction;
	tEventPreloadFunc		m_preloadFunction;
	tEventPreplayFunc		m_preplayFunction;

	tCheckMonitoredEventFunc	m_checkExpiryFunction;
	tUpdateMonitoredPacketFunc	m_updateMonitorFunction;

	tResetFunc				m_resetFunction;
	tFuncWithController		m_invalidateFunction;
	tFuncWithController		m_setupFunction;
	tPrintFunc				m_printFunc;

	tLinkPacketFunc			m_linkFunc;

	tCheckTrackingFunc		m_checkTrackingFunc;
};


#define REGISTEREVENTPACKET(packetID, packetClass, entityType, flags)\
	sm_eventManager.RegisterHistoryPacket<packetClass, entityType, EventPacketFuncPtrs<packetClass, entityType>>(packetID, STRINGIFY(packetClass),  flags);

#define REGISTERINTERPPACKET(packetID, packetClass, entityType, trackedType, flags)\
	sm_eventManager.RegisterHistoryPacket<packetClass, entityType, EventInterpPacketFuncPtrs<packetClass, entityType, trackedType>>(packetID, STRINGIFY(packetClass), flags);

#define REGISTERTRACKEDPACKET(packetID, packetClass, entityType, trackedType, flags)\
	sm_eventManager.RegisterHistoryPacket<packetClass, entityType, EventTrackedPacketFuncPtrs<packetClass, entityType, trackedType>>(packetID, STRINGIFY(packetClass), flags);


#endif // GTA_REPLAY

#endif // REPLAYEVENTMANAGER_H
