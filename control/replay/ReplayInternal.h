#ifndef _REPLAYINTERNAL_H_
#define _REPLAYINTERNAL_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "replay_channel.h"

// Replay Includes
#include "IReplayPlaybackController.h"
#include "ReplayInterfaceManager.h"
#include "ReplayEventManager.h"
#include "ReplayEnums.h"
#include "ReplaySupportClasses.h"
#include "ReplayController.h"
#include "ReplayControl.h"
#include "ReplayMarkerContext.h"
#include "ReplayTrackingInfo.h"
#include "ReplayTrackingControllerImpl.h"
#include "Misc/CameraPacket.h"
#include "frontend/VideoEditor/ui/Playback.h"

#include "atl/array.h"
#include "atl/slist.h"
#include "atl/map.h"

#include "SaveLoad/savegame_queued_operations.h"
#include "Scene/Physical.h"

#if __BANK && GTA_REPLAY
#include "camera/system/CameraManager.h"
#endif // __BANK && GTA_REPLAY

#include "file/ReplayFileManager.h"

class naAudioEntity;
class audScene;
class CMontage;

struct audCachedSoundPacket
{
	u8 *packet;
	u32 gameTime;
};

struct MarkerBufferInfo 
{
	tFileOpCompleteFunc				callback;
	u8								mission;
	u8								importance;
	atFixedArrayReplayMarkers		eventMarkers;
};

#if __BANK
#define REPLAY_BANK_ENTITIES_BEFORE_REPLAY	(REPLAY_BANK_PEDS_BEFORE_REPLAY|REPLAY_BANK_VEHS_BEFORE_REPLAY|REPLAY_BANK_OBJS_BEFORE_REPLAY|REPLAY_BANK_NODES_BEFORE_REPLAY)
#define REPLAY_BANK_ENTITIES_TO_DEL			(REPLAY_BANK_PEDS_TO_DEL|REPLAY_BANK_VEHS_TO_DEL|REPLAY_BANK_OBJS_TO_DEL)
#define REPLAY_BANK_ENTITIES_DURING_REPLAY	(REPLAY_BANK_PEDS_DURING_REPLAY|REPLAY_BANK_VEHS_DURING_REPLAY|REPLAY_BANK_OBJS_DURING_REPLAY|REPLAY_BANK_NODES_DURING_REPLAY)
#define REPLAY_BANK_DISABLE_ENTITIES		(REPLAY_BANK_DISABLE_PEDS|REPLAY_BANK_DISABLE_VEHS|REPLAY_BANK_DISABLE_OBJS)
#define REPLAY_BANK_ENTITIES_ALL			(REPLAY_BANK_FRAMES_ELAPSED|REPLAY_BANK_SECONDS_ELAPSED|REPLAY_BANK_ENTITIES_BEFORE_REPLAY|REPLAY_BANK_ENTITIES_TO_DEL|REPLAY_BANK_ENTITIES_DURING_REPLAY)
#endif // __BANK


class ReplayStreamingRequest
{
public:
	ReplayStreamingRequest() : m_StreamingModule(NULL), m_Index(0), m_StreamingFlags(0) {}

	virtual ~ReplayStreamingRequest() {};

	void SetStreamingModule(strStreamingModule& streamingModule) { m_StreamingModule = &streamingModule; }
	void SetIndex(u32 index) { m_Index = index; }
	void SetStreamingFlags(u32 streamingFlags ) { m_StreamingFlags = streamingFlags; }

	virtual void Load() {};

protected:
	virtual void Destroy() {};
	
	strStreamingModule* m_StreamingModule;
	u32					m_Index;
	u32					m_StreamingFlags;
};

class PTFX_ReplayStreamingRequest : public ReplayStreamingRequest
{
public:
	PTFX_ReplayStreamingRequest() : ReplayStreamingRequest() { }
	~PTFX_ReplayStreamingRequest() { Destroy(); }

	virtual void Load();

protected:	
	virtual void Destroy();
};

class CReplayInterfaceCamera;

enum eBlockLinkType
{
	LINK_BLOCKS_IN_SEQUENCE = 0,
	LINK_BLOCKS_AFTER_ANOTHER,
	LINK_BLOCKS_BEFORE_ANOTHER,
};

#define ENABLE_EVENTS_COOLDOWN 3

class CPacketInterp;
class CPacketEventInterp;

class iReplayInterface;
class CReplayInterfaceObject;
class CReplayAdvanceReader;

struct sReplayMarkerInfo;

class CReplayMgrInternal
{
	friend class ReplaySound;
	friend class CBasicEntityPacketData;
	friend class CPacketPedVariationChange;

	friend class CReplayInterfaceObject;	// Remove
	friend class CReplayInterfacePickup;
	friend class ReplayAttachmentManager;
	friend class CReplayEventManager;
	friend class CPacketVehVariationChange;

	friend class ReplayController;
	friend class CReplayControl;

public:

	// Basic Start/Stop & Save
	static void				StartRecording()
	{
		sm_desiredMode = REPLAYMODE_RECORD;
	}

	static void				StopRecording()
	{
		sm_desiredMode = REPLAYMODE_DISABLED;
	}

	static void				EnableExternal()
	{
		Enable();
	}

	static void				DisableExternal()
	{
		Disable();
	}

#if __BANK
	typedef void	(*scriptCallstackCallback)(atString&);
	static atString sm_PreventCallstack;
	static atString sm_SuppressCameraMovementCallstack;
	static bool		sm_SuppressCameraMovementByScript;

	static bool		sm_bUseCustomWorkingDir;
	static char		sm_customReplayPath[RAGE_MAX_PATH];
#endif
	static void				DisableThisFrame(BANK_ONLY(scriptCallstackCallback callback))
	{
#if __BANK
		if( !sm_SuppressEventsLastFrame )
		{
			callback(sm_PreventCallstack);
		}
#endif
		sm_SuppressEventsThisFrame = true;
	}

	static bool				IsSuppressedThisFrame()
	{
		return sm_SuppressEventsThisFrame;
	}

	static void				EnableEventsThisFrame()
	{
		sm_EnableEventsCooldown = ENABLE_EVENTS_COOLDOWN;
		sm_EnableEventsThisFrame = true;
	}

	static bool				IsEnableEventsThisFrame()
	{
		return sm_EnableEventsCooldown > 0;
	}

	static u32 				GetFrameTimeScriptEventsWereDisabled()
	{
		return sm_FrameTimeScriptEventsWereDisabled;
	}

	static u32 				GetFrameTimeAllEventsWereSuppressed()
	{
		return sm_FrameTimeEventsWereSuppressed;
	}

	static void				IncSuppressRefCount()
	{
		++sm_SuppressRefCount;
	}

	static void				DecSuppressRefCount()
	{
		--sm_SuppressRefCount;

		replayAssertf(sm_SuppressRefCount >= 0, "CReplayMgrInternal::EnableReplay - sm_SuppressRefCount is incorrect");

		if( sm_SuppressRefCount < 0 )
			sm_SuppressRefCount = 0;
	}

	static bool				IsSuppressed()
	{
		return sm_SuppressRefCount > 0;
	}

	static u32				GetNumSuppressedReferences()
	{
		return sm_SuppressRefCount;
	}

	static	void			DisableCameraMovementThisFrame(BANK_ONLY(scriptCallstackCallback callback = NULL))
	{
#if __BANK
		if( !sm_SuppressCameraMovementByScript )
		{
			if( callback )
			{			
				callback(sm_SuppressCameraMovementCallstack);				

				sm_SuppressCameraMovementByScript = true;
			}
			else 
			{
				sm_SuppressCameraMovementCallstack = "Disabled by Code";
			}
		}
#endif

		sm_SuppressCameraMovementThisFrame = true;
#if __BANK && GTA_REPLAY
		camManager::ms_DebugReplayCameraMovementDisabledThisFrame = true;
#endif // __BANK && GTA_REPLAY
	}

	static bool				WasCameraMovementSupressedLastFrame()
	{
		return sm_SuppressCameraMovementLastFrame;
	}

	static bool				IsCameraMovementSupressedThisFrame()
	{
		return sm_SuppressCameraMovementThisFrame;
	}
	
	static	bool			IsPlaybackFlagSet(u32 flag);
	static	bool			IsPlaybackFlagSetNextFrame(u32 flag);

	static	float			GetExportFrameStep() { return sm_exportFrameStep; }
	static	void			SetFixedTimeExport(bool fixedTimeExport) { sm_fixedTimeExport = fixedTimeExport; }
	static	bool			IsFixedTimeExport() { return sm_fixedTimeExport; }

protected:

#if __BANK
	static void QuitReplayCallback();
	static void DumpMonitoredEventsToTTY();
#endif

public:

	static bool				StartEnumerateClipFiles( const char* filepath, FileDataStorage& fileList, const char* filter = "" );
	static bool				CheckEnumerateClipFiles( bool& result );

	static bool				StartDeleteClipFile( const char* filepath );
	static bool				CheckDeleteClipFile( bool& result);

	static bool				StartEnumerateProjectFiles( const char* filepath, FileDataStorage& fileList, const char* filter = "" );
	static bool				CheckEnumerateProjectFiles( bool& result );

	static bool				StartUpdateFavourites();
	static bool				CheckUpdateFavourites( bool& result );

	static void				MarkAsFavourite(const char* path);
	static void				UnmarkAsFavourite(const char* path);
	static bool				IsMarkedAsFavourite(const char* path);

	static bool				StartMultiDelete( const char* filter = "" );
	static bool				CheckMultiDelete( bool& result );

	static void				MarkAsDelete(const char* path);
	static void				UnmarkAsDelete(const char* path);
	static bool				IsMarkedAsDelete(const char* path);
	static u32				GetCountOfFilesToDelete(const char* filter);

	static u32				GetClipRestrictions(const ClipUID& clipUID, bool* firstPersonCam = NULL, bool* cutsceneCam = NULL, bool* blockedCam = NULL);

	static bool				StartGetHeader(const char* filePath);
	static bool				CheckGetHeader(bool& result, ReplayHeader& header);
		
	static bool				StartDeleteFile( const char* filepath );
	static bool				CheckDeleteFile( bool& result);

	static bool				StartLoadMontage( const char* filepath, CMontage* montage );
	static bool				CheckLoadMontage( bool& result );

	static bool				StartSaveMontage( const char* filepath, CMontage* montage );
	static bool				CheckSaveMontage( bool& result );
	static u64				GetOpExtendedResultData();

	static bool				IsClipUsedInAnyMontage( const ClipUID& uid );
	static bool				GetFreeSpaceAvailableForVideos( size_t& out_sizeBytes );

	static bool				IsClipFileValid( u64 const ownerId, char const * const fileName, bool const checkDisk = false ) 
	{ 
		return ReplayFileManager::IsClipFileValid( ownerId, fileName, checkDisk );
	}

	static bool				IsProjectFileValid( u64 const ownerId, char const * const fileName, bool const checkDisk = false ) 
	{ 
		return ReplayFileManager::IsProjectFileValid( ownerId, fileName, checkDisk );
	}

	static const ClipUID* GetClipUID( u64 const ownerId, const char* fileName)
	{
		return ReplayFileManager::GetClipUID( ownerId, fileName );
	}

	static bool				DoesClipHaveModdedContent( u64 const ownerId, char const * const fileName )
	{
		return ReplayFileManager::DoesClipHaveModdedContent( ownerId, fileName );
	}

	static u32 GetClipCount();
	static u32 GetProjectCount();
	static u32 GetMaxClips();
	static u32 GetMaxProjects(); 
	
	// Replay-Frontend related functions
	static void				BeginPlayback( IReplayPlaybackController& playbackController );
	static void				CleanupPlayback();
	// Called on BeginPlayback()/CleanupPlayback() (per play back sesssion).
	static void				OnBeginPlaybackSession(); 
	static void				OnCleanupPlaybackSession();
	// Called on when a clip starts and ends (per clip).
	static void				OnEnterClip();
	static void				OnExitClip();

	static void				Close();

	static	s32				GetCurrentEpisodeIndex();
	static float			LoadReplayLengthFromFileFloat( u64 const ownerId, const char* szFileName );
	static u32				LoadReplayLengthFromFile( u64 const ownerId, const char* szFileName )
	{
		u32 const c_result = (u32)LoadReplayLengthFromFileFloat( ownerId, szFileName );
		return c_result;
	}
	

	// For debug
	static	void			Display();
	static	bool			IsOkayToDelete()								{ return IsEditModeActive() == false || sm_IsBeingDeleteByReplay; } 

	static	FrameRef		GetCurrentSessionFrameRef();

	static  s32				GetCutSceneAudioSync()									{ return sm_CutSceneAudioSync;		}
	static  void			SetCutSceneAudioSync(s32 offset)						{ sm_CutSceneAudioSync = offset;    }

	static  int				GetRecordingThreadCount()								{ return sm_recordingThreadCount;	}

	static bool				ShouldRegisterElement(const CEntity* pEntity)			{ return sm_interfaceManager.ShouldRegisterElement(pEntity); }

	static  bool			IsSettingUpFirstFrame()			{ return sm_isSettingUpFirstFrame; }

private:
	//--------------------------------------------------------------------------------------------------------------------

	static sysIpcThreadId        sm_recordingThread[MAX_REPLAY_RECORDING_THREADS];
	static sysIpcThreadId		 sm_quantizeThread;
	static sysIpcCurrentThreadId sm_recordingThreadId[MAX_REPLAY_RECORDING_THREADS];
	static sysIpcSema            sm_entityStoringStart[MAX_REPLAY_RECORDING_THREADS];
	static sysIpcSema            sm_entityStoringFinished[MAX_REPLAY_RECORDING_THREADS];
	static sysIpcSema            sm_recordingFinished;
	static sysIpcSema            sm_quantizeStart;
	static sysIpcSema            sm_recordingThreadEnded;
	static sysIpcSema			 sm_quantizeThreadEnded;
	static bool                  sm_wantRecordingThreaded;
	static bool                  sm_recordingThreaded;
	static bool                  sm_exitRecordingThread;
	static bool                  sm_recordingThreadRunning;
	static u16					 sm_currentSessionBlockID;					 
	static bool                  sm_storingEntities;
	static int					 sm_wantRecordingThreadCount;
	static int					 sm_recordingThreadCount;
	
	static FrameRef					sm_frameRefBeforeConsolidation;
	static CAddressInReplayBuffer	sm_addressBeforeConsolidation;
	static CAddressInReplayBuffer	sm_addressAfterConsolidation;	

	static bool					sm_lockRecordingFrameRate;
	static int					sm_numRecordedFramesPerSec;
	static bool					sm_lockRecordingFrameRatePrev;
	static int					sm_numRecordedFramesPerSecPrev;
	static float				sm_recordingFrameCounter;

	static	void			RecordingThread(void* pData);
	static	void			QuantizeThread(void* pData);

	static	u32				BytesPerReplayBlock;
	static	u16				NumberOfReplayBlocks;
	static	u16				NumberOfReplayBlocksFromSettings;
	static	u16				NumberOfTempReplayBlocks;
	static	u16				TotalNumberOfReplayBlocks;

	static	u16				TempBlockIndex;

	static	bool			SettingsChanged;

	static	void			StoreGameTimers();
	static	void			RestoreGameTimers();

protected:
	static  void			SetBytesPerReplayBlock(u32 val)		
	{	
		if(val < MIN_BYTES_PER_REPLAY_BLOCK || val > MAX_BYTES_PER_REPLAY_BLOCK)
		{
			replayAssertf(false, "Tried to set a the replay bytes per block to be an invalid value (%d)", val);
			val = Clamp(val, MIN_BYTES_PER_REPLAY_BLOCK, MAX_BYTES_PER_REPLAY_BLOCK);
		}

		if(IsEditModeActive())
		{
			replayAssertf(false, "Can't set the settings while the replay is playing back (SetBytesPerReplayBlock)");
			return;
		}

		if(BytesPerReplayBlock == val)
			return;
				
		BytesPerReplayBlock = val;		
		SettingsChanged = true;	
	
	}
	static void				SetNumberOfReplayBlocksFromSettings(u16 val)
	{
		if(NumberOfReplayBlocksFromSettings != val)
		{
			if(IsEditModeActive())
			{
				replayAssertf(false, "Can't set the settings while the replay is playing back (SetBytesPerReplayBlock)");
				return;
			}

			NumberOfReplayBlocksFromSettings = val;
			SettingsChanged = true;	
		}
	}

	static  void			SetNumberOfReplayBlocks(u16 val)	
	{	
		if(NumberOfReplayBlocks == NumberOfReplayBlocksFromSettings && NumberOfReplayBlocks == val)
		{
			return;
		}

		if(val < MIN_NUM_REPLAY_BLOCKS || val > MAX_NUM_REPLAY_BLOCKS)
		{
			replayAssertf(false, "Tried to set a the number of replay blocks to be an invalid value");
			val = Clamp(val, MIN_NUM_REPLAY_BLOCKS, MAX_NUM_REPLAY_BLOCKS);
		}

		if(IsEditModeActive())
		{
			replayAssertf(false, "Can't set the settings while the replay is playing back (SetBytesPerReplayBlock)");
			return;
		}

		if(NumberOfReplayBlocks == val)
			return;

		NumberOfReplayBlocks = val;
		NumberOfTempReplayBlocks = NUMBER_OF_TEMP_REPLAY_BLOCKS;
		TotalNumberOfReplayBlocks = NumberOfReplayBlocks + NumberOfTempReplayBlocks;
		TempBlockIndex = NumberOfReplayBlocks;
	}

	static u16 GetNumberOfReplayBlocks()
	{
		return NumberOfReplayBlocks;
	}

	static  void SetMaxSizeOfStreamingReplay(u32 val)	
	{	
		if(val < MIN_REPLAY_STREAMING_SIZE || val > MAX_REPLAY_STREAMING_SIZE)
		{
			replayAssertf(false, "Tried to set the MaxSizeOfStreamingReplay to be an invalid value");
			val = Clamp(val, MIN_REPLAY_STREAMING_SIZE, MAX_REPLAY_STREAMING_SIZE);
		}
	}

	static	void			InitSession(unsigned initMode);
	static	void			ShutdownSession(unsigned);

	static	bool			Enable();
	static  void			WantQuit();
	static	void			QuitReplay();

	static	void			Process();
	
	static	void			PreProcess();
	static	void			PostRender(); // acts as a EndFrame()
	static	void			StartFrame();
	static	void			PostProcess();

	static	void			InitWidgets();

	static  bool			IsWaitingOnWorldStreaming();
	static	bool			IsEnabled()						{ return (sm_bReplayEnabled && !sm_bScriptWantsReplayDisabled); }

	static	bool			IsEditModeActive()				{ return sm_uMode == REPLAYMODE_EDIT; }
	static	bool			IsEditorActive()				{ return sm_IsEditorActive;		}
	static	bool			IsScrubbing()					{ return CVideoEditorPlayback::IsScrubbing();	}
	static	bool			IsFineScrubbing()				{ return m_FineScrubbing;	}
	
	static  void			PauseRenderPhase();
	static  void			UnPauseRenderPhase();

	static bool				IsReplayCursorJumping();

	static	bool			IsRecordingEnabled()			{ return sm_uMode == REPLAYMODE_RECORD;							}
	static	bool			IsReplayDisabled()				{ return sm_uMode == REPLAYMODE_DISABLED;						}
	static	bool			IsWaitingForSave()				{ return sm_uMode == REPLAYMODE_WAITINGFORSAVE;					}
	static  bool			HasInitialized()				{ return ReplayFileManager::IsInitialised();					}
	static  bool			ShouldRecord()					{ return IsAwareOfWorldForRecording();							}
	static  bool			IsRecording()					{ return IsRecordingEnabled() && fwTimer::IsGamePaused() == false; }
	static  bool			IsStoringEntities();

	static	void			SetReplayIsInControlOfWorld(bool control) {sm_ReplayInControlOfWorld = control;}
	static  bool			IsReplayInControlOfWorld()		{ return sm_ReplayInControlOfWorld;	}
	static	bool			IsClearingWorldForReplay()		{ return sm_ClearingWorldForReplay;	}

	static	bool			IsEditingCamera()				
	{ 
		return IsEditModeActive() && CReplayMarkerContext::GetMarkerStorage() != NULL &&
			CReplayMarkerContext::GetEditingMarkerIndex() == CReplayMarkerContext::GetCurrentMarkerIndex(); 
	}
	
	static	bool			IsPlaybackPlaying()				{ return sm_eReplayState.IsSet(REPLAY_STATE_PLAY); }
	static	bool			ShouldPauseSound()				{ return IsEditModeActive() && !IsPreCachingScene() && (IsCursorRewinding() || IsPlaybackPaused()); }
	static	bool			IsLoading()						{ return sm_FileManager.IsLoading() || IsWaitingForSave(); }
	static	float			GetPlayBackSpeed()				{ return sm_playbackSpeed; }
	static	bool			ShouldUpdateScripts()			{ return sm_shouldUpdateScripts;	}
	static  bool			IsUsingRecordedCamera();

	static  float			GetRecordingBufferProgress()	{ return sm_recordingBufferProgress;	}

	static	eReplayMode		GetMode()						{ return sm_uMode; }
	static	bool			WantsToLoad()					{ return sm_desiredMode == REPLAYMODE_LOADCLIP; }
	static	eReplayLoadingState	GetLoadState()				{ return sm_eLoadingState; }
	static  bool			IsSettingUp()					{ return sm_isSettingUp; }

	static	void			SetReplayPlayerInteriorProxyIndex(s32 index)	{ sm_playerIntProxyIndex = index; }
	static	s32				GetReplayPlayerInteriorProxyIndex()				{ return sm_playerIntProxyIndex; }

	// Weather
	static	void			OverrideWeatherPlayback(bool b)					{	sm_overrideWeatherPlayback = b;	}
	static	void			OverrideTimePlayback(bool b)					{	sm_overrideTimePlayback = b;	}

	static CEntity*			GetEntityAsEntity(s32 id)
	{
		CReplayID r(id);
		CEntityGet<CEntity> entityGet(r);
		sm_interfaceManager.GetEntity<CEntity>(entityGet);
		return entityGet.m_pEntity;
	}

	static void				GetEntityAsEntity(CEntityGet<CEntity>& eg)
	{
		sm_interfaceManager.GetEntity<CEntity>(eg);
	}

	static bool				GetBuildingInfo(const CPhysical* pEntity, u32& hashKey, u16& index);
	static void				RecordMapObject(const CEntity* pEntity);	

	static bool				AddBuilding(CBuilding* pBuilding);
	static void				RemoveBuilding(CBuilding* pBuilding);
	static CBuilding*		GetBuilding(u32 hash);
	static void				ResetStaticBuildings()		{	sm_BuildingHashes.Reset();	}
	static atMap<u32, CBuilding*>	sm_BuildingHashes;

	//////////////////////////////////////////////////////////////////////////
	// Returns true if the we should abort out of the calling 'RecordFx' function early
	template<typename P>
	static	bool			HandleEntityForFx(P& packet, const CPhysical* pEntity, u8 entityIndex)
	{
		if(pEntity)
		{
			if(pEntity->GetIsTypeObject())
			{
				RecordMapObject(pEntity);
			}


			// If it's a building we're playing this effect on then override the hash
			// and the replay ID to contain the hash of the static bounds and the index
			// of the physinst.
			if(pEntity->GetIsTypeBuilding() == true)
			{
				// Get the slot from the static bound store...
				u32 hashKey = 0;
				u16 index = 0;
				if(GetBuildingInfo(pEntity, hashKey, index))
				{
					packet.SetReplayID(hashKey, entityIndex);
					packet.SetStaticIndex(index, entityIndex);
				}
				else
				{
					replayDebugf1("Failed to get building info for a building...this effect will not be recorded");
					return true;
				}
			}
			else
			{
				if(packet.ShouldAbort())
					return true;

				PacketRecordWait();
				bool entityIsRecorded = sm_interfaceManager.EnsureIsBeingRecorded(pEntity);
				PacketRecordSignal();

				if(entityIsRecorded == false)
				{
					// Entity has not so far been recorded....
					// Check that the entity is actually in a pool and that it will actually be recorded next frame!
					if(sm_interfaceManager.EnsureWillBeRecorded(pEntity) == false)
					{
						replayFatalAssertf(packet.ShouldInterp() == false, "Packet should interp but entity will not be recorded...this could be bad");
						return true;
					}
				}

				replayAssertf(pEntity->GetReplayID() != ReplayIDInvalid, "Trying to record an event on an entity that does not have a valid ID");
				if(pEntity->GetReplayID() == ReplayIDInvalid)
					return true;

				packet.SetReplayID(pEntity->GetReplayID(), entityIndex);
				replayDebugf3("Entity used in effect: 0x%08X", pEntity->GetReplayID().ToInt());
			}
		}

		return false;
	}

	static bool		sm_wantToRecordEvents;
	static bool		sm_recordEvents;

	static bool		sm_wantToMonitorEvents;
	static bool		sm_monitorEvents;
	template<typename P>
	static	bool			RecordFx(const P& packetIn, const CPhysical** pEntity = NULL, bool avoidMonitor = false, bool skipPacketCheck = false)
	{
		replayAssertf(skipPacketCheck || packetIn.ShouldTrack() == false, "Tracked packet being recorded via 'RecordFx' instead of 'RecordPersistentFx'");

		if(sm_recordEvents == false || !IsAwareOfWorldForRecording())
			return false;

		if(!skipPacketCheck && sm_eventManager.ShouldProceedWithPacket(packetIn, IsRecording() && !CReplayControl::IsPlayerOutOfControl(), !avoidMonitor & sm_monitorEvents) == false)
			return false;

		P packet = packetIn;

		bool shouldAbort = false;
		for(u8 i = 0 ; i < packet.GetNumEntities(); ++i)
		{
			shouldAbort |= HandleEntityForFx(packet, pEntity[i], i);
		}

		if(shouldAbort)	// Something went wrong...abort
			return false;

		if(!packet.ValidatePacket())
			return false;	// Failed the validation so don't record the packet (It'll likely fall over during playback anyway)

		sm_eventManager.StoreEvent(packet, IsRecording() && !CReplayControl::IsPlayerOutOfControl(), !avoidMonitor & sm_monitorEvents);

 		return false;
	}


	template<typename P>
	static	bool			RecordFx(P* packetIn, const CPhysical** pEntity = NULL, bool avoidMonitor = false, bool skipPacketCheck = false)
	{
		replayAssertf(skipPacketCheck || packetIn->ShouldTrack() == false, "Tracked packet being recorded via 'RecordFx' instead of 'RecordPersistentFx'");

		if(sm_recordEvents == false || !IsAwareOfWorldForRecording())
			return false;

		if(!skipPacketCheck && sm_eventManager.ShouldProceedWithPacket(*packetIn, IsRecording() && !CReplayControl::IsPlayerOutOfControl(), !avoidMonitor & sm_monitorEvents) == false)
			return false;

		bool shouldAbort = false;
		for(u8 i = 0 ; i < packetIn->GetNumEntities(); ++i)
		{
			shouldAbort |= HandleEntityForFx(*packetIn, pEntity[i], i);
		}

		if(shouldAbort)	// Something went wrong...abort
			return false;

		if(!packetIn->ValidatePacket())
			return false;	// Failed the validation so don't record the packet (It'll likely fall over during playback anyway)

		sm_eventManager.StoreEvent(*packetIn, IsRecording() && !CReplayControl::IsPlayerOutOfControl(), !avoidMonitor & sm_monitorEvents);

		return false;
	}


	template<typename P, typename T>
	static	void			RecordPersistantFx(const P& packetIn, const CTrackedEventInfo<T>& trackingInfo, const CPhysical** pEntity, bool allowNew = false)
	{
		replayAssertf(packetIn.ShouldTrack() == true || packetIn.ShouldInterp() == true, "Non-Tracked/Non-Interp packet being recorded via 'RecordPersistentFx' instead of 'RecordFx'");

		if(sm_recordEvents == false || !IsAwareOfWorldForRecording())
			return;
		
		if(sm_eventManager.ShouldProceedWithPacket(packetIn, IsRecording() && !CReplayControl::IsPlayerOutOfControl(), sm_monitorEvents) == false)
			return;

		REPLAY_CHECK(trackingInfo.m_pEffect != 0,NO_RETURN_VALUE, "RecordPersistantFx needs valid effect");

		P packet = packetIn;

		CTrackedEventInfo<T>* thisTrackingInfo = NULL;

		PacketRecordWait();
		sm_eventManager.TrackRecordingEvent(trackingInfo, thisTrackingInfo, allowNew, packet.ShouldEndTracking(), packet.GetPacketID());
		PacketRecordSignal();

		if(thisTrackingInfo == NULL || thisTrackingInfo->trackingID == -1)
		{
			// With the new audio recorder interface this is firing all the time as we've not currently hooked up all sounds.
			//replayDebugf1("Tracking failed on effect...perhaps it was started before the replay? %p, %p", trackingInfo.m_pEffect[0], trackingInfo.m_pEffect[1]);
			return;
		}

		packet.SetTrackedID(thisTrackingInfo->trackingID);

		RecordFx(packet, pEntity, false, true);
	}

	template<typename T>
	static void				StopTrackingFx(const CTrackedEventInfo<T>& trackingInfo)
	{
		if(sm_recordEvents == false || !IsAwareOfWorldForRecording())
			return;

		PacketRecordWait();
		CTrackedEventInfo<T>* thisTrackingInfo = NULL;
		sm_eventManager.TrackRecordingEvent(trackingInfo, thisTrackingInfo, false, false, PACKETID_INVALID);
		if(thisTrackingInfo)
			sm_eventManager.FreeTrackableDuringRecording(thisTrackingInfo);
		PacketRecordSignal();
	}


	static  void			OnWorldNotRecordable()
	{
		sm_eventManager.Reset();
		sm_interfaceManager.DeregisterAllCurrentEntities();
	}

	static  void			OnWorldBecomesRecordable()
	{
		sm_interfaceManager.RegisterAllCurrentEntities();
	}


	static	s64				CopyBlockToBlock(CBlockInfo* pTo, CBlockInfo* pFrom);
	// Camera functions
	static  bool			UseRecordedCamera()								{ return sm_useRecordedCamera;	}
	static	const CReplayFrameData&	GetReplayFrameData() 					{ return sm_replayFrameData;	}
	static	const CReplayClipData&	GetReplayClipData() 					{ return sm_replayClipData;		}

	static	CPed*			GetMainPlayerPtr()								{ return sm_pMainPlayer;		}
	static  void			SetMainPlayerPtr(CPed* pPlayer)					{ sm_pMainPlayer = pPlayer;		}

	static	void			UpdateTimer();

	static	u32				GetPlayBackStartTimeAbsoluteMs()				{ return sm_uPlaybackStartTime; }
	static	u32				GetPlayBackEndTimeAbsoluteMs()					{ return sm_uPlaybackEndTime; }
	static	u32				GetPlayBackEndTimeRelativeMs()					{ return sm_uPlaybackEndTime - sm_uPlaybackStartTime; }
	static	u32				GetTotalTimeRelativeMs()						{ return sm_uPlaybackEndTime - sm_uPlaybackStartTime; }

	static	u32				GetCurrentTimeRelativeMs()						{ return (u32)sm_fInterpCurrentTime;	}
	static	float			GetCurrentTimeRelativeMsFloat()					{ return sm_fInterpCurrentTime;	}
	static	float			GetVfxDelta();
	static	u32				GetCurrentGameTime()							{ return sm_CurrentTimeInMilliseconds; }
	static	float			GetPlaybackFrameStepDeltaMsFloat()				{ return sm_playbackFrameStepDelta; }

	static void				SetNextPlayBackState(u32 state);
	static void				ClearNextPlayBackState(u32 state);
	static CReplayState&	GetCurrentPlayBackState()								{ return sm_eReplayState; }
	static CReplayState&	GetNextPlayBackState()									{ return sm_nextReplayState; }

	static	bool			IsAwareOfWorldForRecording();
	static	void			OnCreateEntity(CPhysical* pEntity);
	static  void			OnDeleteEntity(CPhysical* pEntity);
	static	void			OnPlayerDeath();

	static	iReplayInterface*	GetReplayInterface(const atHashWithStringNotFinal& type);
	static	iReplayInterface*	GetReplayInterface(const CEntity* pEntity);

	//--------------------------------------------------------------------------------------------------------------------
	static	bool			IsDiskSpaceAvailable( size_t const size );
	static	bool			IsDiskSpaceAvailableForClip();

	static  bool			IsUserPaused()									{ return sm_eReplayState.IsSet(REPLAY_STATE_PAUSE); }
    static  bool			WantsPause()									{ return sm_nextReplayState.IsSet(REPLAY_STATE_PAUSE); }
	static  bool			IsSystemPaused()								{ return IsLoading() || IsClipTransition() || IsPreCachingScene(); }
	static	bool			IsPlaybackPaused()								{ return IsUserPaused() || IsSystemPaused(); }
	static	bool			IsJustPlaying()									{ return (IsPlaybackPlaying() && !IsScrubbing() && !IsJumping() && !IsUserPaused() && !GetNextPlayBackState().IsSet(REPLAY_STATE_PAUSE)	&& !IsCursorFastForwarding() && !IsCursorRewinding()); }
	static	bool			IsPreCachingScene()								{ return sm_eReplayState.IsPrecaching() || sm_nextReplayState.IsPrecaching(); }
	static	bool			IsRequestingClipTransition()					{ return sm_eReplayState.IsSet(REPLAY_STATE_CLIP_TRANSITION_REQUEST) || sm_nextReplayState.IsSet(REPLAY_STATE_CLIP_TRANSITION_REQUEST); }
	static	bool			IsLoadingClipTransition()						{ return sm_eReplayState.IsSet(REPLAY_STATE_CLIP_TRANSITION_LOAD); }
	static	bool			IsClipTransition()								{ return IsRequestingClipTransition() || IsLoadingClipTransition(); }
	static	bool			IsSinglePlaybackMode()							{ return (sm_pPlaybackController && sm_pPlaybackController->GetPlaybackSingleClipOnly()) || (!sm_pPlaybackController); }
	static	bool			IsExporting()									{ return sm_pPlaybackController && sm_pPlaybackController->IsExportingToVideoFile() && !sm_pPlaybackController->IsExportingPaused(); }
	static	bool			IsSaving()										{ return sm_FileManager.IsSaving(); }
	static  bool			AreEntitiesSetup()								{ return sm_allEntitiesSetUp;	}	// This bool is set to false in 'JumpTo' and reverts to true when entities should all be set up correctly.
	static	bool			WillSave()										{ return sm_doSave; }
	static  bool			CanSaveClip()									{ return (sm_ReplayBufferInfo.IsAnyTempBlockOn() == false || fwTimer::IsGamePaused()) && !IsSaving(); }
	static	void			KickPrecaching(bool clearSounds)				{ sm_WantsToKickPrecaching = true; if(clearSounds) { ClearOldSounds(); } }
	static  bool			ShouldPokeGameSystems()							{ return sm_PokeGameSystems; }
	static  bool			IsWaitingOnEventsForExport()					{ return (sm_eventManager.IsRetryingEvents() || sm_PokeGameSystems) && (sm_pPlaybackController && (sm_pPlaybackController->IsExportingToVideoFile() || !sm_pPlaybackController->GetPlaybackSingleClipOnly()));	}
	static  bool			ShouldDisabledCameraMovement();
	static  bool			ShouldSetCameraDisabledFlag();

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	static	bool			ShouldScriptsPrepareForSave();
	static	void			SetScriptsHavePreparedForSave(bool bHavePrepared);
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

	static	bool			ShouldScriptsCleanup()							{	return sm_waitingForScriptCleanup;	}
	static	void			SetScriptsHaveCleanedUp()						{	sm_scriptsHaveCleanedUp = true;		}
	static	void			PauseTheClearWorldTimeout(bool bPause);

	static	bool			IsPerformingFileOp()							{ return ReplayFileManager::IsPerformingFileOp(); }
	static	bool			IsSafeToTriggerUI();
#if USE_SRLS_IN_REPLAY
public:
	static	bool			IsCreatingSRL()									{ return sm_eReplayState.IsSet(REPLAY_RECORD_SRL); }
	static	bool			IsUsingSRL()									{ return sm_eReplayState.IsSet(REPLAY_PLAYBACK_SRL); }
protected:
#endif
	// Global packet Querying functions

	#define NUM_FRAMES_TO_WAIT_AFTER_JUMP 2
public:
	enum JumpOptions
	{
		JO_None = 0,
		JO_FineScrubbing = 1 << 0,
		JO_Force = 1 << 1,
		JO_FreezeRenderPhase = 1 << 2,
		JO_LinearJump = 1 << 3,
	};
protected:
	static	bool			JumpTo(float timeToJumpToMS, u32 jumpOptions);

	static bool				IsJumping()										{ return sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP); }

	static float			GetMarkerSpeed()								{ return m_markerSpeed;		}
	static	void			SetMarkerSpeed(float speed)						{ m_markerSpeed = Clamp(speed, 0.01f, 5.0f); }

	static	float			GetCursorSpeed()								{ return m_cursorSpeed;	}
	static	void			SetCursorSpeed(float speed) 
	{
		replayDebugf1("CursorSpeed %f", speed);
		if(speed != 1.0f)
		{
			SetCursorState(REPLAY_CURSOR_SPEED, speed >= 0.0f && speed <= 1.0f);
			m_cursorSpeed		= speed;

			if(m_cursorSpeed < 0.0f)
			{
				SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_BACK);
			}
			else
			{
				SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD);
			}
		}
		else
		{
			SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD);
			SetCursorState(REPLAY_CURSOR_NORMAL, false);
			m_cursorSpeed		= 1.0f;
		}
	}

	static	void			ResumeNormalPlayback()
	{
		SetCursorState(REPLAY_CURSOR_NORMAL);
		SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD);
		m_cursorSpeed		= 1.0f;
		m_TimeToJumpToMS	= 0;
	}

	static void				StopPreCaching()
	{
		sm_KickPreloader = false;
		sm_IgnorePostLoadStateChange = false;
		sm_forcePortalScanOnClipLoad = true;
		GetCurrentPlayBackState().SetPrecaching(false);
		sm_nextReplayState.SetPrecaching(false);
		sm_uAudioStallTimer = 0;
		sm_uStreamingStallTimer = 0;
		sm_uStreamingSettleCount = 0;
		sm_eLoadingState = REPLAYLOADING_NONE;
	}

	static	u32				GetPacketInfoInSaveBlocks(atArray<CBlockProxy>& blocksToSave, u32& startTime, u32& endTime, u32&frameFlags);
	static	u32				GetTotalMsInBuffer();
	static  bool			IsUsingCustomDelta()							{ return GetCursorState() == REPLAY_CURSOR_SPEED; }
	static	bool			IsCursorJumpingForward()						{ return IsJumping() && GetCursorSpeed() >= 0.0f; }
	static	bool			WasJumping()									{ return GetLastPlayBackState().IsSet(REPLAY_CURSOR_JUMP); }
	static	bool			IsCursorRewinding()								{ return GetCursorState() != REPLAY_CURSOR_NORMAL && GetNextCursorState() != REPLAY_CURSOR_NORMAL && GetCursorSpeed() < 0.0f; }
	static	bool			IsCursorFastForwarding()						{ return GetCursorState() != REPLAY_CURSOR_NORMAL && GetNextCursorState() != REPLAY_CURSOR_NORMAL && GetCursorSpeed() > 0.0f; }
	static void				SetMissionName(const atString& name)			{ sm_currentMissionName = name;	}
	static void				SetFilterString(const atString& name)			{ sm_currentMontageFilter = name;	}
	static  const atString&	GetMissionName()								{ return sm_currentMissionName;	}
	static  const atString&	GetMontageFilter()								{ return sm_currentMontageFilter;	}
	static  s32				GetMissionIndex()								{ return sm_currentMissionIndex;	}
	static	void			SetMissionIndex(s32 index)						{ sm_currentMissionIndex = index;	}
	static	void			CaptureUpdatedThumbnail();
#if USE_SRLS_IN_REPLAY
	static	void			SetRecordingSRL(const bool set )				{ if(set){sm_eReplayState.SetState( REPLAY_RECORD_SRL );}else{sm_eReplayState.ClearState( REPLAY_RECORD_SRL | REPLAY_PLAYBACK_SRL );}	}
#endif
private:
	static	void			Init(unsigned initMode, bool bSaveThread = false);
	static	void			Shutdown();

	static	void			ProcessInputs();
	static	void			ProcessPlayback();
	static	bool			WaitingForDataToLoad();

	static	void			PostRenderInternal();

public:
	static	void			ProcessInputs(const ReplayInternalControl& control);
	static	void			ProcessRecord();
	static	float			GetNextAudioBeatTime( float const* overrideClipTimeMs = NULL );
	static	float			GetPrevAudioBeatTime( float const* overrideClipTimeMs = NULL );
	static	float			GetClosestAudioBeatTimeMs( float const clipTimeMs, bool const nextBeat );

	static	float			GetExtraBlockRemotePlayerRecordingDistance()	{ return sm_fExtraBlockRemotePlayerRecordingDistance; }

private:
	static	void			ValidateTitle();
	static	void			DisableReplayFromScripts()						{ sm_bScriptWantsReplayDisabled = true; }
	static	void			EnableReplayFromScripts()						{ sm_bScriptWantsReplayDisabled = false; }

	static	void			ProcessPlaybackAudio();
	static	void			RequestReplayMusic();
	static	void			FreezeAudio();
public:
	static	void			UnfreezeAudio();

private:
	static	void			ResetParticles();

	static  bool			Disable();

	static	s32				GetNextSoundId();
	static	void			ReleaseSoundId( s32 soundId );

	static	void			PrintHistoryPackets();
	static	void			PrintReplayPackets();

#if GTA_REPLAY_OVERLAY
	static  void			CalculateBlockStats(CBlockInfo* pBlock);	
#endif //GTA_REPLAY_OVERLAY
	static void				PrintHistoryPacketsFunc(FileHandle handle);
	static void				PrintReplayPacketsFunc(FileHandle handle);

	static	bool			OutputReplayStatistics(const atString& filepath);
	static	bool			ExportReplayPackets(const atString& filepath);

	// Replay-camera related functions
	static	Matrix34&		GetLastCamMatrix()								{ return sm_oCamMatrix; }
	static	void			SetCameraInSniperMode(bool bSniper)				{ sm_bCameraInSniperMode = bSniper; }
	static	bool			IsCameraInSniperMode()							{ return sm_bCameraInSniperMode; }
	static	u16				GetSniperModelIndex()							{ return sm_uSniperModelIndex; }
	static	void			SetSniperModelIndex(u16 uSniperMI)				{ sm_uSniperModelIndex = uSniperMI; }

	static	void			SetStreamingNumberOfRealObjects(s32 sObjs)		{ sm_sNumberOfRealObjectsRequested = sObjs; }

	static	s32				GetNumberOfFramesInBuffer();

	static	void			HandleLogicForMemory();

	static	void			SetCurrentTimeMs(float time)					{ sm_fInterpCurrentTime = time; }
	static  u32				GetCurrentTimeMs() 								{ return (u32)sm_fInterpCurrentTime; }
	static  float			GetCurrentTimeMsFloat() 						{ return sm_fInterpCurrentTime; }
	static	u32				GetCurrentTimeAbsoluteMs()						{ return (u32)(GetCurrentTimeAbsoluteMs16BitsAccurary() >> 16); }
	static	u64				GetCurrentTimeAbsoluteMs16BitsAccurary()		{ return ((u64)sm_uPlaybackStartTime << 16) + (u64)(sm_fInterpCurrentTime * (float)(0x1 << 16)); }

	static  bool			IsPausingMusicOnClipEnd() { return ( sm_pPlaybackController && sm_pPlaybackController->PauseMusicOnClipEnd() ) || (!sm_pPlaybackController); }
	static  bool			CanUpdateGameTimer() { return sm_pPlaybackController && sm_pPlaybackController->CanUpdateGameTimer(); }

	static  void			SetPlaybackFrameStepDelta(float delta)			{ sm_playbackFrameStepDelta = delta; } 
	static	float			GetPlaybackFrameStepDelta()						{ return sm_playbackFrameStepDelta; }
	static	void			SetExportFrameStep(float exportFrameStep)		{ sm_exportFrameStep = exportFrameStep; }
	static	void			ResetExportFrameCount()							{ sm_exportFrameCount = 0; }
	static	void			ResetExportTotalTime()							{ sm_exportTotalNs = 0; }


	static void					SetLastPlayBackState( CReplayState state)	{ sm_eLastReplayState = state; }
	static const CReplayState&	GetLastPlayBackState()						{ return sm_eLastReplayState; }

	static float			GetTotalTimeSec();
	static u32				GetTotalTimeMS();
	static float			GetTotalTimeMSFloat();


	static s32				GetMaxNumEntities()								{ return (s32)MAX_NUM_REPLAY_ENTITIES; }

	// Replay Sound
	//static naAudioEntity&				GetAudioEntity()			{ return s_AudioEntity; }
	//static atSNode<CPreparingSound>*	GetFreePreparingSoundNode();
	//static atSList<CPreparingSound>&	GetPreparingSoundsList()	{ return sm_lPreparingSounds; }
	//static void							ClearPreparingSounds();

	static void				ClearOldSounds();

	// Secondary Listener
	static	void			SetSecondaryListenerMatrix( const Matrix34 &matrix )	{ sm_SecondaryListenerMatrix			= matrix;		}
	static	void			SetLocalEnvironmentScanPosition( const Vector3 &pos )	{ sm_LocalEnvironmentScanPosition		= pos;			}
	static	void			SetSecondaryListenerContribution( f32 contribution )	{ sm_SecondaryListenerContribution		= contribution;	}
	/*TODO4FIVE static	void	SetCameraType( CCamTypes::eCamType camType )			{ sm_CameraType							= camType;		}*/
	static	void			SetIsEnvironmentPositionFixed( bool isFixed )			{ sm_bIsLocalEnvironmentPositionFixed	= isFixed;		}

	static	const Matrix34&	GetSecondaryListenerMatrix()					{ return sm_SecondaryListenerMatrix;			}
	static	const Vector3&	GetLocalEnvironmentScanPosition()				{ return sm_LocalEnvironmentScanPosition;		}
	static	f32				GetSecondaryListenerContribution()				{ return sm_SecondaryListenerContribution;		}
	static	bool			GetIsLocalEnvironmentPositionFixed()			{ return sm_bIsLocalEnvironmentPositionFixed;	}
	/*TODO4FIVE static	CCamTypes::eCamType	GetCameraType()							{ return sm_CameraType;							}*/

	static	CFrameInfo&	GetPrevFrameInterpInfo(u32 uAccurateTime);	

	// Delayed save
	static	void			EnableDelayedSave()								{ if (sm_eDelayedSaveState == REPLAY_DELAYEDSAVE_DISABLED)  { sm_eDelayedSaveState = REPLAY_DELAYEDSAVE_ENABLED; } }
	static	void			ProcessDelayedSave()							{ if (sm_eDelayedSaveState == REPLAY_DELAYEDSAVE_REQUESTED) { sm_eDelayedSaveState = REPLAY_DELAYEDSAVE_PROCESS; } else { sm_eDelayedSaveState = REPLAY_DELAYEDSAVE_DISABLED; } }

	// Input
	static	bool			IsSaveReplayButtonPressed();

	//--------------------------------------------------------------------------------------------------------------------
	static	bool			PortalScanRequired(Vector3& rvScanPos);

	//--------------------------------------------------------------------------------------------------------------------
#if __BANK
	static	void			AddDebugWidgets();
	static	void			UpdateBanks(u32 eBankFlags);

	static bool				s_bTurnOnDebugBones;
	static u16				s_uBoneId;
#endif	//__BANK

	// Used only to build interpolation tables
	static void		ResetPlaybackAddressIDs(DEV_ONLY(s32 sAllocs));

	static naAudioEntity& GetAudioEntity()	{	return s_AudioEntity;	}

	static	u32		GetReplayFullFrameSystemTime()	{	return (u32)sm_oReplayFullFrameTimer.GetMsTime();	}
	static	u32		GetReplayInterpSystemTime()		{	return (u32)sm_oReplayInterpFrameTimer.GetMsTime();	}

	// Replay-buffer related
	static	bool	RewindHistoryBufferAddress(CAddressInReplayBuffer& roBufferAddress, u32 uTime);
	static	bool	InvalidateHistoryBufferFromFrameToPlay(CAddressInReplayBuffer& roBufferAddress, u32 uTime);
	static	bool	ProcessHistoryBufferFromStart(ReplayController& controller, u32 uTime);
	static	bool	GoToNextReplayBlock();
	static  void    SetSoundIdTaken( s32 soundId, bool taken );

	// Memory related
	static	bool	AllocateMemory(u16 maxBlocksCount, u32 blockSize);
	static	int		SetupBlock(CBlockInfo& block, void*);
	static	void	FreeMemory();
	static  int		ShutdownBlock(CBlockInfo& block, void*);
	static	bool	IsMemoryAllocated()	{ return sm_IsMemoryAllocated; }

	static  bool	SetupReplayBuffer(u16 normalBlockCount, u16 tempBlockCount);
	static  bool	CheckAllThreadsAreClear();

	enum ValidationResult
	{
		eValidationOk,
		eValidationPaused,
		eValidationOther,
	};
	static	ValidationResult	Validate(bool preload);

	// Continuous dumping related
	static	void	SetTempBuffer();

	//--------------------------------------------------------------------------------------------------------------------
	// Replay related
	/// Recording
	static	u32		CalculateSizeOfBlock(atArray<u32>& sizes, bool blockChange);

	static	void	StoreEntityInformation(u32 threadIndex);
	static	void	ConsolidateFrame(ReplayController& controller, CAddressInReplayBuffer& addressBeforeConsolidation, FrameRef& frameRefBeforeConsolidation);
public:
	static	void	WaitForEntityStoring()
	{	
		PF_PUSH_TIMEBAR_BUDGETED("Wait for replay entity store", 0.1f);
		if(sm_recordingThreaded == true && sm_storingEntities == true)
		{
			replayAssertf(IsRecordingEnabled(), "Recording is not enabled in WaitForEntityStoring()");
			sysIpcWaitSema(sm_entityStoringFinished[0]);
		}

		sm_storingEntities = false;
		PF_POP_TIMEBAR();
	}
	static	void	WaitForRecordingFinish()
	{	
		PF_PUSH_TIMEBAR_BUDGETED("Wait for replay recording", 1.0f);
		if(sm_recordingThreaded == true && sm_recordingThreadRunning == true)
		{
			replayAssertf(IsRecordingEnabled(), "Recording is not enabled in WaitForRecordingFinish()");
			sysIpcWaitSema(sm_recordingFinished);
		}

		sm_recordingThreadRunning = false;
		PF_POP_TIMEBAR();
	}
	static  bool    WantsToQuit() { return sm_wantQuit; }

	static void		OnEditorActivate();	
	static void		OnEditorDeactivate();

    static bool     CanFreeUpBlocks();
	static void		FreeUpBlocks();
	static	void	OnSignOut();

private:	

	static	void	RecordFrame(u32 threadIndex);
	static	void	RecordParachuteBoneData(CEntity* pEntity);
	static  void	RecordFramePacketFlags(CPacketFrame* pCurrFramePacket);
	static  void	ResetRecordingFrameFlags() { sm_SuppressCameraMovementThisFrame = false; }

	//--------------------------------------------------------------------------------------------------------------------
	// Replay buffers playback
	static	void	PlayBackThisFrame(bool bFirstTime = false);
	static	bool	PlayBackThisFrameInterpolation(ReplayController& controller, bool preprocessOnly, bool entityMayNotExist = false);
	static	bool	PlayBackHistory(ReplayController& controller, u32 LastTime, bool entityMayBeAbsent = false);

	static	void	EnableRecording();
	static	void	DisableRecording();

	static	void	TriggerPlayback();
	static	void	PrepareForPlayback();

	static	void	FinishPlayback();

	static	void	SetFinishPlaybackPending(bool bPending, bool bUserCancel = false)	{ sm_bFinishPlaybackPending = bPending; sm_bUserCancel = bUserCancel; }
	static	bool	IsFinishPlaybackPending()											{ return sm_bFinishPlaybackPending; }
	static	bool	IsUserCancel()														{ return sm_bUserCancel; }
	static  bool    IsLastFrame();	// this is used for the rendering and is a special case use caution
	static	bool	IsLastFrameAndLastClip();
	static	void	UpdateClip();

	static	bool	ShouldPlayBackPacket( eReplayPacketId packetId, u32 playBackFlags, s32 replayId = 0, u32 packetTime = ~0U, u32 startTimeWindow = 0 );
	static	u32		GetFlagForPacket( eReplayPacketId packetId );

	// Replay loading/deleting related
	static void		SetupInitialPlayback();
	static CAddressInReplayBuffer FindEndOfBlock(CAddressInReplayBuffer startAddressOfBlock);
	static CAddressInReplayBuffer FindOverlapWithNextBlock(CAddressInReplayBuffer startAddressOfBlock);
	static CAddressInReplayBuffer FindOverlapWithPreviousBlock(CAddressInReplayBuffer startAddressOfBlock);

	static bool		SetupPlayback(CAddressInReplayBuffer& startAddress, CAddressInReplayBuffer& endAddress, eBlockLinkType const linkType, const u16 startingMasterBlockIndex);

	static	bool	ClearWorld();

	// Replay-cam related
	static	void	ProcessReplayCamera(eReplayMode eType);
	static	void	GetFirstCameraFocusCoords(Vector3 *pResult);

private:
	// Interpolation stuff
	static  bool	ProcessEventJump();
	static	bool	UpdateInterpolationFrames(u64 uAccurateGameTime, bool preprocessOnly);
	static	bool	IsLastFrame(u32 uAccurateGameTime);
	static  bool	IsLastFrame(FrameRef frame);
	static	bool	IsFirstFrame(u32 uAccurateGameTime);
	static	bool	IsFirstFrame(FrameRef frame);

	static	void	ApplyFades();
	static	void	PauseReplayPlayback();

#define ENABLE_PACKET_CRITICAL_SECTIONS	1
#if ENABLE_PACKET_CRITICAL_SECTIONS	
protected:
	inline static void PacketRecordWait()		
	{
		sm_PacketRecordCriticalSection.Lock(); 
	}
	inline static void PacketRecordSignal()		
	{
		sm_PacketRecordCriticalSection.Unlock(); 
	}
#else
	inline static void PacketRecordWait()			{};
	inline static void PacketRecordSignal()			{};
#endif
private:

	static u32				GetCursorState() { return sm_eReplayState.GetState(REPLAY_CURSOR_MASK); }
	static u32				GetNextCursorState() { return sm_nextReplayState.GetState(REPLAY_CURSOR_MASK);	}
	static void				SetCursorState(u32 cursorOverrideState, bool clearOldSounds = true)
	{
		if(clearOldSounds)
		{
			ClearOldSounds();
		}
		sm_bWasPreviouslyRewindingOrFastForwarding = true;
		SetNextPlayBackState(cursorOverrideState);
	}

	static void ProcessCursorMovement();
	static void SaveCallback();

#if __BANK
	static void LoadCallback();
	static void DeleteCallback();
	static void EnumerateCallback();
	static void SetAllSaved();
	static void RecordCallback();
	static void PlayCallback();
	static void PauseCallback();
	static void ScrubbingCallback();
	static void CursorSpeedCallback();
#if DO_REPLAY_OUTPUT_XML
	static void ExportReplayPacketCallback();
#endif // REPLAY_DEBUG && RSG_PC
	static void IncSuppressRefCountCallBack();
	static void DecSuppressRefCountCallBack();
	static void ClipSelectorCallBack();
	static void GetClipNames(atArray<const char*>& clipNames);

	static void	SaveReplayPacketsFunc(FileHandle handle);
	static bool	SaveReplayPacketCompleteFunc(int retCode);
	static void	ExportPacketFunc(FileHandle handle);
	static void WaitForReplayExportToFinish();

	static void Core0Callback();
	static void Core5Callback();

	static void HighlightReplayEntity();
	static void HighlightMapObjectViaMapHash();
	static void HighlightMapObject();
	static void HighlightParent();
#endif

	static void FindStartOfPlayback(CAddressInReplayBuffer& oBufferAddress);

	static sysCriticalSectionToken	sm_PacketRecordCriticalSection;

	//--------------------------------------------------------------------------------------------------------------------

	//--------------------------------------------------------------------------------------------------------------------
	static const s16 NUM_EXTRA_REPLAY_ENTITIES			= 512;
	static const s16 NUM_ENTITIES_CREATION_RECORD		= 4096;
	static const s16 NUM_EXTRA_REPLAY_SOUNDS			= 10; /*TODO4FIVE MAX_CONTROLLER_SOUNDS / 2;*/	// A bit of extra slack
	static const s16 MAX_REPLAY_SOUNDS					= /*TODO4FIVE MAX_CONTROLLER_SOUNDS + */ NUM_EXTRA_REPLAY_SOUNDS;
	static const s16 MAX_PREPARING_SOUNDS				= 512;
	static const s16 MAX_POST_UPDATE_PACKET_ENTITIES	= NUM_EXTRA_REPLAY_ENTITIES;
	static const s16 MAX_NUM_REPLAY_ENTITIES			= /*TODO4FIVE MAXNOOFPEDS + VEHICLE_POOL_SIZE + OBJECT_POOL_SIZE + */NUM_EXTRA_REPLAY_ENTITIES;
	static const s16 MAX_NUM_REPLAY_PARTICLES			= /*TODO4FIVE MAX_FX_INSTS*2 + */NUM_EXTRA_REPLAY_ENTITIES;
	static const s16 MAX_NUM_REPLAY_ENTITIES_TO_DELETE	= 256;
	static const u16 SOUND_WINDOW_SIZE					= 100; // Only allow sounds within SOUND_WINDOW_SIZE ms of current frame to start playing

	static atSNode<CPreparingSound>		sm_aPreparingSounds[MAX_PREPARING_SOUNDS];
	static atSList<CPreparingSound>		sm_lPreparingSounds;
	//--------------------------------------------------------------------------------------------------------------------
	static	IReplayPlaybackController*	sm_pPlaybackController;
	static  bool						sm_bCheckForPreviewExit;
	static  bool						sm_bIsLastClipFinished;
	//--------------------------------------------------------------------------------------------------------------------
	static CReplayState				sm_eReplayState;
	static CReplayState				sm_eLastReplayState;
	static CReplayState				sm_nextReplayState;

	static eReplayLoadingState		sm_eLoadingState;
	static bool						sm_isSettingUpFirstFrame;
	static bool						sm_isSettingUp;			// Are we currently still setting up the replay for playback
	static bool						sm_allEntitiesSetUp;
public:
	enum 
	{
		eNoPreload = 0,
		ePreloadWaiting = 1,
		ePreloadKickNeeded = 2,
		ePreloadFirstKickNeeded = 3,
	};
	static u32						sm_JumpPrepareState;
	static bool						sm_allowBlockingLoading;
	static bool						IsBlockingLoadingExpected()		{	return sm_allowBlockingLoading;	}
	static bool						IsJumpPreparing()				{	return sm_JumpPrepareState > eNoPreload;	}
private:
	static bool						sm_SuppressEventsThisFrame;
	static bool						sm_SuppressEventsLastFrame;
	static u32						sm_FrameTimeEventsWereSuppressed;

	static bool						sm_EnableEventsThisFrame;
	static int						sm_EnableEventsCooldown;
	static bool						sm_EnableEventsLastFrame;
	static u32						sm_FrameTimeScriptEventsWereDisabled;

	static bool						sm_PreviouslySuppressedReplay;
	static u32						sm_CurrentTimeInMilliseconds;
	static s32						sm_SuppressRefCount;

	static bool						sm_wantPause;
	static bool						sm_canPause;

	static bool						sm_wantQuit;

	static bool						sm_IsMemoryAllocated;
	static bool						sm_bFileRequestMemoryAlloc;

	static CReplayFrameData			sm_replayFrameData;
	static CReplayClipData			sm_replayClipData;
	static bool						sm_useRecordedCamera;
	static int						sm_SuppressCameraMovementCameraFrameCount;
	static bool						sm_SuppressCameraMovementThisFrame;
	static bool						sm_SuppressCameraMovementLastFrame;

	static CBufferInfo				sm_ReplayBufferInfo;
	static	u32						sm_BlockSize;

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	enum eScriptSavePrepState
	{
		SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT,
		SCRIPT_SAVE_PREP_NOT_PREPARED,
		SCRIPT_SAVE_PREP_PREPARED
	};
	static	eScriptSavePrepState	sm_ScriptSavePreparationState;
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

	static	sysTimer				sm_scriptWaitTimer;

	static	bool					sm_waitingForScriptCleanup;
	static	bool					sm_scriptsHaveCleanedUp;
	static	bool					sm_bScriptsHavePausedTheClearWorldTimeout;
	static  bool					sm_forcePortalScanOnClipLoad;

	static	bool					sm_IsEditorActive;

	static	bool					sm_wantBufferSwap;

	static atString					sm_currentMissionName;
	static atString					sm_currentMontageFilter;
	static s32						sm_currentMissionIndex;

	static u32						sm_previousFrameHash;
	static u32						sm_currentFrameHash;

	static	CAddressInReplayBuffer	sm_oRecord;
	static	CAddressInReplayBuffer	sm_oPlayback;
	static	CAddressInReplayBuffer	sm_oPlaybackStart;

	static	CPacketFrame*			sm_pPreviousFramePacket;

	static float					sm_recordingBufferProgress;

	//new current frame functionality
	static const CFrameInfo&		GetFirstFrame()			{	return sm_firstFrameInfo;	}
	static const CFrameInfo&		GetLastFrame()			{	return sm_lastFrameInfo;	}

	static void						MoveCurrentFrame(const s32 moveAmount, CFrameInfo& currFrame, CFrameInfo& prevFrame, CFrameInfo& nextFrame);
	static void						UpdateCreatePacketsForNewBlock(CAddressInReplayBuffer newBlockStart);

public:
	typedef void	(*replayCallback)();

	static	const CPacketFrame*		GetRelativeFrame(s32 nodeOffset, const CPacketFrame* pCurrFramePacket);
protected:

	static	u32						GetGameTimeAtRelativeFrame(s32 nodeOffset);

	static	CBlockProxy				GetCurrentBlock()								{	return CBlockProxy(sm_oRecord.GetBlock());		}
	static	CBlockProxy				FindFirstBlock()								{	return sm_ReplayBufferInfo.FindFirstBlock();	}
	static	CBlockProxy				FindLastBlock()									{	return sm_ReplayBufferInfo.FindLastBlock();		}
	static  int						ForeachBlock(int(func)(CBlockProxy, void*), void* pParam = NULL, u16 numBlocks = 0)		{	return sm_ReplayBufferInfo.ForeachBlock(func, pParam, numBlocks);	}

	static bool						IsAnyTempBlockOn()	{	return sm_ReplayBufferInfo.IsAnyTempBlockOn(); }

private:
	static	FrameRef				GetFrameAtTime(u32 time);

	static	const CPacketFrame*		GetNextFrame(const CPacketFrame* pCurrFramePacket);
	static	const CPacketFrame*		GetPrevFrame(const CPacketFrame* pCurrFramePacket);

	static	CFrameInfo				sm_currentFrameInfo;
	static	CFrameInfo				sm_nextFrameInfo;
	static	CFrameInfo				sm_prevFrameInfo;
	static	CFrameInfo				sm_firstFrameInfo;
	static	CFrameInfo				sm_lastFrameInfo;

	static  CFrameInfo				sm_eventJumpCurrentFrameInfo;
	static	CFrameInfo				sm_eventJumpNextFrameInfo;
	static	CFrameInfo				sm_eventJumpPrevFrameInfo;

	static	float	sm_fLastInterpValue;

	// Used to cache the start of a frame in the history buffer
	static	CAddressInReplayBuffer	sm_oHistoryProcessPlayback;

	static	u32						sm_uPlaybackStartTime;
	static  bool					sm_bShouldSetStartTime;
	static	u32						sm_uPlaybackEndTime;
	static	float					sm_fInterpCurrentTime;

	static  u32						sm_uTimePlaybackBegan;

	static sysIpcSema				sm_LoadOpWaitForMem;
	static bool						(*sm_pSaveResultCB)(bool);

	//--------------------------------------------------------------------------------------------------------------------
	static	float					sm_playbackFrameStepDelta;
	static	float					sm_exportFrameStep;
	static	u64						sm_exportFrameCount;
	static	u64						sm_exportTotalNs;
	// Pfx delta
	static	float					sm_fTimeScale;
	static	float					sm_playbackSpeed;

	static	bool					sm_shouldUpdateScripts;
	static	bool					sm_fixedTimeExport;

	//--------------------------------------------------------------------------------------------------------------------
	static	s32						sm_playerIntProxyIndex;
	//--------------------------------------------------------------------------------------------------------------------
	static	eReplayMode				sm_uMode;
	static	eReplayMode				sm_desiredMode;
	static	Matrix34				sm_oCamMatrix;
	static	bool					sm_bCameraInSniperMode;
	static	bool					sm_bFinishPlaybackPending;
	static	bool					sm_bUserCancel;
	static	bool					sm_KickPreloader;
	static 	bool 					sm_bJumpPreloaderKicked;
	static	bool					sm_bPreloaderResetFwd;
	static	bool					sm_bPreloaderResetBack;
	static  bool					sm_IgnorePostLoadStateChange;
	static  bool					sm_KickPrecaching;
	static  bool					sm_WantsToKickPrecaching;
	static  bool					sm_doInitialPrecache;
	static	bool					sm_OverwatchEnabled;
	static  bool					sm_ShouldOverwatchBeEnabled;
	static  bool					sm_ReplayInControlOfWorld;
	static	bool					sm_ClearingWorldForReplay;
	static	bool					sm_PokeGameSystems;
	static  bool					sm_NeedEventJump;					// Events need to continue jumping over several frames
	static  u32						sm_JumpToEventsTime;				// Time to jump to for the events (this is processed over several frames)
	static  bool					sm_DoFullProcessPlayback;			// Did we get through preprocess fully?  Are all the entities in place?

	static  u64						sm_ownerOfFileToLoad;
	static	atString				sm_fileToLoad;
public:
	static atString					sm_lastLoadedClip;
private:

	static	s32						sm_sNumberOfRealObjectsRequested;
	static	u32						sm_uStreamingStallTimer;
	static	u32						sm_uAudioStallTimer;
	static  u32						sm_uStreamingSettleCount;
	static	u16						sm_uSniperModelIndex;
	static	bool					sm_bReplayEnabled;

	static	u32						sm_uLastPlaybackTime;
	static	char					sm_sReplayFileName[REPLAYPATHLENGTH];
	static	bool					sm_bSaveReplay;
	static	eDelaySaveState			sm_eDelayedSaveState;
	static	bool					sm_bIsDead;

	static	bool					sm_bScriptWantsReplayDisabled;

	static	s32						sm_uCurrentFrameInBlock;

	static	bool					sm_bWasPreviouslyRewindingOrFastForwarding;

	static	bool					sm_overrideWeatherPlayback;
	static	bool					sm_overrideTimePlayback;

	// Audio
	static	naAudioEntity			s_AudioEntity;
	static	audScene*				sm_ffwdAudioScene;
	static	audScene*				sm_rwdAudioScene;

	// Secondary Listener
	static	Matrix34				sm_SecondaryListenerMatrix;
	static	Vector3					sm_LocalEnvironmentScanPosition;
	static	f32						sm_SecondaryListenerContribution;
	/*TODO4FIVE static	CCamTypes::eCamType		sm_CameraType;*/
	static	bool					sm_bIsLocalEnvironmentPositionFixed;

	static	bool					sm_bDisableLumAdjust;

	static  bool                    sm_bInRelease;
	static  s32                     sm_MontageReleaseTime;
	static  s32                     sm_SimpleReleaseStopTime;
	static  s32                     sm_SimpleReleaseStartTime;

	// The distance we add on the the camera range to test against prostitute usage.
	static	float					sm_fExtraBlockRemotePlayerRecordingDistance;

	static	sysTimer				sm_oReplayFullFrameTimer;
	static	sysTimer				sm_oReplayInterpFrameTimer;

	static	s16						sm_iCurrentSoundBaseId;
	static	s16						sm_iSoundInstanceID;

	static atArray<audCachedSoundPacket> sm_CachedSoundPackets;

	static	CPed*					sm_pMainPlayer;

	static	bool					sm_reInitAudioEntity;

	static	bool					sm_doSave;
	static	bool					sm_IsMarkedClip;
	static  u32						sm_postSaveFrameCount;

	//--------------------------------------------------------------------------------------------------------------------
	static s32						sm_MusicDuration;
	static s32						sm_MusicOffsetTime;
	static s32						sm_sMusicStartCountDown;

#if __BANK
	static f32						sm_fMonatgeMusicVolume;
	static f32						sm_fMonatgeSFXVolume;
#endif

#if __WIN32PC/*TODO4FIVE  && __SPOTCHECK*/
	static bool						sm_bSpotCheck;
#endif

#if __BANK
	static bool						sm_bRecordingEnabledInRag;
	static int						sm_ClipIndex;
	
	static	bkSlider*				sm_replayBlockPosSlider;
	static	bkSlider*				sm_replayBlockSlider;
	static  bkCombo*				sm_ReplayClipNameCombo;
	static  bkText*					sm_ReplayClipNameText;
#endif // __BANK

	static CReplayInterfaceManager	sm_interfaceManager;
	static CReplayEventManager		sm_eventManager;
	static CReplayAdvanceReader*	sm_pAdvanceReader;

	static float					m_TimeToJumpToMS;
	static float					m_jumpDelta;
	static float					m_cursorSpeed;
	static float					m_markerSpeed;
	static s32						sm_CutSceneAudioSync;
	static f32						sm_AudioSceneStopTimer;
	static f32						sm_AudioSceneStopDelay;
	static bool						m_ForceUpdateRopeSim;
	static bool						m_ForceUpdateRopeSimWhenJumping;
	static bool						m_FineScrubbing;
	static bool						sm_isWithinEnvelope;

	static bool						m_WantsReactiveRenderer;
	static s32						m_FramesTillReactiveRenderer;

#if USE_SAVE_SYSTEM
	static CSavegameQueuedOperation_ReplayLoad sm_ReplayLoadStructure;
	static CSavegameQueuedOperation_ReplaySave sm_ReplaySaveStructure;
#endif	//	USE_SAVE_SYSTEM

	static bool sm_IsBeingDeleteByReplay;

	static bool						sm_haveFrozenShadowSetting;
	static bool						sm_shadowsFrozenInGame;

	static bool						sm_padReplayToggle;

	static replayCallback	m_onPlaybackSetup;

	static void DoOnPlaybackSetup()										{	if(m_onPlaybackSetup)	(*m_onPlaybackSetup)();			}

	//FileManager stuff
	static CSavegameQueuedOperation_LoadReplayClip sm_ReplayLoadClipStructure;

	static ReplayFileManager		sm_FileManager;
	static bool						sm_TriggerPlayback;
	static atString					sm_Filepath;
	static bool						sm_ClipCompressionEnabled;
	static atArray<CBlockProxy>		sm_BlocksToSave;
	static rage::atMap<s32, PTFX_ReplayStreamingRequest> sm_StreamingRequests;

	static s16						sm_ShouldSustainRecordingLine;

	static	bool					sm_bMultiplayerClip;

	static bool						sm_shouldLoadReplacementCasinoImaps;
	static bool						sm_vinewoodIMAPSsForceLoaded;
	static atHashString				sm_islandActivated;

	//Game timer values to save and restore between clips
	static u32						m_CamTimerTimeInMilliseconds;
	static float					m_CamTimerTimeStepInSeconds;
	static u32						m_GameTimerTimeInMilliseconds;
	static float					m_GameTimerTimeStepInSeconds;
	static u32						m_ScaledNonClippedTimerTimeInMilliseconds;
	static float					m_ScaledNonClippedTimerTimeStepInSeconds;
	static u32						m_NonScaledClippedTimerTimeInMilliseconds;
	static float					m_NonScaledClippedTimerTimeStepInSeconds;
	static u32						m_ReplayTimerTimeInMilliseconds;
	static float					m_ReplayTimerTimeStepInSeconds;
	static bool						m_ReplayTimersStored;

public:

	static rage::atMap<s32, PTFX_ReplayStreamingRequest>& GetStreamingRequests() { return sm_StreamingRequests; }

	static void						SaveClip();
	static void						LoadClip( u64 const ownerId, const char* filename);

	static MarkerBufferInfo			markerBufferInfo;
	static eReplayFileErrorCode		sm_LastErrorCode;
	static void						SaveMarkedUpClip(MarkerBufferInfo& info);
	static bool						SaveBlock(CBlockProxy block);
	static void						ClearSaveBlocks();
	static bool						IsBlockMarkedForSave(CBlockProxy block);
	static const atArray<CBlockProxy>& GetBlocksToSave();
	static eReplayFileErrorCode		GetLastErrorCode() { return sm_LastErrorCode; }
	static bool						DidLastFileOpFail()	{ return sm_LastErrorCode > REPLAY_ERROR_SUCCESS;	}
	static void						UpdateAvailableDiskSpace();
	static bool						IsMultiplayerClip() { return sm_bMultiplayerClip; }
	static u64						GetAvailiableDiskSpace();

	static void						UpdateFrameTimeScriptEventsWereDisabled();
	static void						UpdateFrameTimeAllEventsWereSuppressed();

	static bool						ShouldSustainRecording();

	static s32						GetMissionIDFromClips(const char* missionFilter);
	static s32						CalculateCurrentMissionIndex();

	static void						SetWasModifiedContent();

private:
	static void						SaveAllBlocks();
	static void						SetSaveResultCallback(bool (*pCompleteCB)(bool)) { sm_pSaveResultCB = pCompleteCB; }
	static bool						OnLoadCompleteFunc(int retCode);
	static bool						OnSaveCompleteFunc(int retCode);
	static bool						LoadStartFunc(ReplayHeader& pHeader);

	static bool						sm_wasModifiedContent;

#if RSG_BANK && !__NO_OUTPUT
	static void						DebugStackPrint(size_t, const char* text, size_t);
#endif

public:
	static void	SetOnPlaybackSetupCallback(replayCallback pCallback)	{ replayAssertf(m_onPlaybackSetup == NULL || pCallback == NULL, "CReplayMgrInternal::SetOnPlaybackSetupCallback - a callback has already been set"); m_onPlaybackSetup = pCallback; }

#if __BANK
	static bool sm_bUpdateSkeletonsEnabledInRag;
	static bool sm_bQuantizationOptimizationEnabledInRag;
#endif
};


#endif // GTA_REPLAY

#endif // _REPLAYINTERNAL_H_
