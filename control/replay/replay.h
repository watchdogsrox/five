#ifndef _REPLAY_H_
#define _REPLAY_H_

#include "control/replay/replayinternal.h"

#if GTA_REPLAY

#include "control/replay/ReplayBufferMarker.h"
#include "control/replay/File/ReplayFileManager.h"
#include "control/replay/ReplayControl.h"
#include "scene/Entity.h"

#define OLD_REPLAY_INPUT 0


#define REPLAY_INVALID_OBJECT_HASH	0xffffffff

class CReplayInterfaceVeh;
class CReplayInterfaceObject;
class CReplayInterfaceGame;

class CReplayMgr : public CReplayMgrInternal
{
public:
	static void				InitSession(unsigned initMode);
	static void				ShutdownSession(unsigned shutdownMode);

	static bool				Enable()										{	return CReplayMgrInternal::Enable();					}
	static void				QuitReplay()									{	CReplayMgrInternal::WantQuit();							}

	static void				Process();
#if OLD_REPLAY_INPUT
	static void				PreProcess()									{	CReplayMgrInternal::ProcessInputs(); CReplayMgrInternal::PreProcess(); }
#else
	static void				PreProcess()									{	CReplayMgrInternal::PreProcess(); }
#endif //OLD_REPLAY_INPUT
	static void				PostRender()									{	CReplayMgrInternal::PostRender();						}

	static void				StartFrame()									{   CReplayMgrInternal::StartFrame();						}
	static void				PostProcess()									{	CReplayMgrInternal::PostProcess();						}

	static void				ProcessRecord(u32 replayTimeInMilliseconds);
	static void				WaitForEntityStoring()							{	CReplayMgrInternal::WaitForEntityStoring();				}

	static void				OnEditorActivate()								{	CReplayMgrInternal::OnEditorActivate();					}
	static void				OnEditorDeactivate()							{	CReplayMgrInternal::OnEditorDeactivate();				}

	static void				SetOnPlaybackSetupCallback(CReplayMgrInternal::replayCallback pCallback) { CReplayMgrInternal::SetOnPlaybackSetupCallback(pCallback); }

    static bool             CanFreeUpBlocks()                               {   return CReplayMgrInternal::CanFreeUpBlocks();           }
	static void				FreeUpBlocks()									{	CReplayMgrInternal::FreeUpBlocks();						}

	static void				OnSignOut()										{	CReplayMgrInternal::OnSignOut();						}

	static void				InitWidgets();

	static void				ClearSpeechSlots();

	static void				OnWorldNotRecordable()							{	CReplayMgrInternal::OnWorldNotRecordable();				}
	static void				OnWorldBecomesRecordable()						{	CReplayMgrInternal::OnWorldBecomesRecordable();			}

    // Querying functions
    using CReplayMgrInternal::WantsPause;
	static bool				IsWaitingOnWorldStreaming() 					{	return CReplayMgrInternal::IsWaitingOnWorldStreaming(); }
	static bool				IsEnabled()										{	return CReplayMgrInternal::IsEnabled();					}
	static bool				IsEditModeActive() 								{	return CReplayMgrInternal::IsEditModeActive();			}
	static bool				IsEditorActive() 								{	return CReplayMgrInternal::IsEditorActive();			}
	static bool				IsRecordingEnabled()							{	return CReplayMgrInternal::IsRecordingEnabled();		}
	static bool				IsWaitingForSave()								{   return CReplayMgrInternal::IsWaitingForSave();			}
	static bool				HasInitialized()								{   return CReplayMgrInternal::HasInitialized();			}
	static bool				IsReplayDisabled()								{	return CReplayMgrInternal::IsReplayDisabled();			}
	static bool				ShouldRecord()									{	return CReplayMgrInternal::ShouldRecord();				}
	static bool				IsRecording()									{	return CReplayMgrInternal::IsRecording();				}
	static bool				IsStoringEntities()								{	return CReplayMgrInternal::IsStoringEntities();			}
	static bool				IsLoading()										{	return CReplayMgrInternal::IsLoading();					}
	static bool				IsPlaying()										{	return CReplayMgrInternal::IsPlaybackPlaying();			}
	static  bool			IsUserPaused()									{	return CReplayMgrInternal::IsUserPaused();				}
	static  bool			IsSystemPaused()								{	return CReplayMgrInternal::IsSystemPaused();			}
	static bool				ShouldPauseSound()								{   return CReplayMgrInternal::ShouldPauseSound();			}
	static float			GetPlayBackSpeed()								{	return CReplayMgrInternal::GetPlayBackSpeed();			}
	static	bool			ShouldUpdateScripts()							{	return CReplayMgrInternal::ShouldUpdateScripts();		}
	static	void			SetReplayIsInControlOfWorld(bool control)		{   CReplayMgrInternal::SetReplayIsInControlOfWorld(control); }
	static  bool			IsReplayInControlOfWorld()						{	return CReplayMgrInternal::IsReplayInControlOfWorld();	}
	static bool				IsClearingWorldForReplay()						{	return CReplayMgrInternal::IsClearingWorldForReplay();	}
	static bool				IsReplayCursorJumping() 						{	return CReplayMgrInternal::IsReplayCursorJumping();	}
	static bool				IsScrubbing()									{	return CReplayMgrInternal::IsScrubbing();	}
	static bool				IsFineScrubbing()								{	return CReplayMgrInternal::IsFineScrubbing();	}
	static  bool			ShouldPokeGameSystems()							{	return CReplayMgrInternal::ShouldPokeGameSystems();		}
	static  bool			IsExporting()									{	return CReplayMgrInternal::IsExporting();		}
	static bool				AreEntitiesSetup()								{	return CReplayMgrInternal::AreEntitiesSetup();			}
	static bool				WasClipRecordedMP()								{	return IsPlaybackFlagSet(FRAME_PACKET_RECORDED_MULTIPLAYER); }
	static bool				AllowXmasSnow()									{   return IsReplayInControlOfWorld() && WasClipRecordedMP(); }

	static bool				IsEditingCamera()								{	return CReplayMgrInternal::IsEditingCamera();			}
	static u64				GetCurrentReplayUserId()						{	return ReplayFileManager::getCurrentUserIDToPrint();	}

	static bool				IsBlockingLoadingExpected()						{	return CReplayMgrInternal::IsBlockingLoadingExpected();	}

	static void				AddBuilding(CBuilding* pBuilding)				{	CReplayMgrInternal::AddBuilding(pBuilding);				}
	static void				RemoveBuilding(CBuilding* pBuilding)			{	CReplayMgrInternal::RemoveBuilding(pBuilding);			}
	static CBuilding*		GetBuilding(u32 hash)							{	return CReplayMgrInternal::GetBuilding(hash);			}

	static bool				IsRenderedCamInVehicle()
	{
		if(!CReplayMgrInternal::IsUsingRecordedCamera())
			return false;
		return IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_CAM_IN_VEHICLE);
	}

	static bool				GetCamInVehicleFlag()
	{
		if(!CReplayMgrInternal::IsUsingRecordedCamera())
			return false;
		return IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_CAM_IN_VEHICLE_VFX);
	}

	static bool				GetCamInFirstPersonFlag()
	{
		if(!CReplayMgrInternal::IsUsingRecordedCamera())
			return false;
		return IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_FIRST_PERSON);
	}

	static bool				ShouldUseAircraftShadows()						
	{
		if(!IsReplayInControlOfWorld())
			return false;
		return IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_USE_AIRCRAFT_SHADOWS);
	}

	static float			GetRecordingBufferProgress()					{	return CReplayMgrInternal::GetRecordingBufferProgress();	}

	static float			GetExtraBlockRemotePlayerRecordingDistance()	{   return CReplayMgrInternal::GetExtraBlockRemotePlayerRecordingDistance(); }

	static	void			SetReplayPlayerInteriorProxyIndex(s32 index)	{	CReplayMgrInternal::SetReplayPlayerInteriorProxyIndex(index); }
	static	s32				GetReplayPlayerInteriorProxyIndex()				{   return CReplayMgrInternal::GetReplayPlayerInteriorProxyIndex(); }

	static	eReplayMode		GetMode()										{   return CReplayMgrInternal::GetMode();					}
	static	bool			WantsToLoad()									{   return CReplayMgrInternal::WantsToLoad();				}
	static	eReplayLoadingState	GetLoadState()								{   return CReplayMgrInternal::GetLoadState();				}
	static bool				IsSettingUpFirstFrame()							{	return CReplayMgrInternal::IsSettingUpFirstFrame();		}
	static bool				IsSettingUp()									{   return CReplayMgrInternal::IsSettingUp();				}
	static bool				IsPreparingFrame()								{	return CReplayMgrInternal::IsSettingUpFirstFrame() && CReplayMgrInternal::IsPreCachingScene();	}

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	static	bool			ShouldScriptsPrepareForSave()					{	return CReplayMgrInternal::ShouldScriptsPrepareForSave();	}
	static	void			SetScriptsHavePreparedForSave(bool bHavePrepared)				{	CReplayMgrInternal::SetScriptsHavePreparedForSave(bHavePrepared);	}
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

	static	bool			ShouldScriptsCleanup()							{	return CReplayMgrInternal::ShouldScriptsCleanup();		}
	static	void			SetScriptsHaveCleanedUp()						{	CReplayMgrInternal::SetScriptsHaveCleanedUp();			}
	static	void			PauseTheClearWorldTimeout(bool bPause)			{	CReplayMgrInternal::PauseTheClearWorldTimeout(bPause);	}

	static void				SetMissionName(const atString& name)			{	CReplayMgrInternal::SetMissionName(name);				}
	static void				SetFilterString(const atString& name)			{	CReplayMgrInternal::SetFilterString(name);				}
	static s32				GetMissionIndex()								{	return CReplayMgrInternal::GetMissionIndex();			}
	static void				SetMissionIndex(const s32 index)				{	CReplayMgrInternal::SetMissionIndex(index);				}
	static const atString&	GetMissionName() 								{	return CReplayMgrInternal::GetMissionName();			}
	static const atString&	GetMontageFilter()								{	return CReplayMgrInternal::GetMontageFilter();			}

	static bool				DoesCurrentClipContainCutscene()				{	return ( GetReplayClipData().m_clipFlags & FRAME_PACKET_RECORDED_CUTSCENE ) != 0;	}
	static bool				DoesCurrentClipContainFirstPersonCamera()		{	return ( GetReplayClipData().m_clipFlags & FRAME_PACKET_RECORDED_FIRST_PERSON ) != 0;	}
	static bool				DoesCurrentClipDisableCameraMovement()		{	return ( GetReplayClipData().m_clipFlags & FRAME_PACKET_DISABLE_CAMERA_MOVEMENT ) != 0;	}
	
	static void				StopAllSounds()									{   CReplayMgrInternal::UnfreezeAudio(); }

	// Replay-array ID/entity related
	static CEntity*			GetEntity(s32 id)								{	return CReplayMgrInternal::GetEntityAsEntity(id); }

	static bool				IsEntityNotValid(const CEntity* pEntity)
	{
		return (pEntity != NULL && pEntity->GetIsPhysical() == false && pEntity->GetIsTypeBuilding() == false);
	}
	// Direct access to tell the Replay to record something
	template<typename P>
	static void				RecordFx(const P& info, const CEntity* pEntity=NULL, bool avoidMonitor = false)	
	{	
		if(IsEntityNotValid(pEntity))
			return;

		const CPhysical* pEntities[2] = {(CPhysical*)pEntity, NULL};
		CReplayMgrInternal::RecordFx<P>(info, pEntities, avoidMonitor);
	}

	template<typename P>
	static void				RecordFx(const P& info, const CEntity* pEntity1, const CEntity* pEntity2, bool avoidMonitor = false)	
	{
		if(IsEntityNotValid(pEntity1) || IsEntityNotValid(pEntity2))
			return;

		const CPhysical* pEntities[2] = {(CPhysical*)pEntity1, (CPhysical*)pEntity2};
		CReplayMgrInternal::RecordFx<P>(info, pEntities, avoidMonitor);
	}

	template<typename P>
	static void				RecordFx(P* info, const CEntity* pEntity=NULL, bool avoidMonitor = false)	
	{	
		if(IsEntityNotValid(pEntity))
			return;

		const CPhysical* pEntities[2] = {(CPhysical*)pEntity, NULL};
		CReplayMgrInternal::RecordFx<P>(info, pEntities, avoidMonitor);
	}

	template<typename P, typename T>
	static void				RecordPersistantFx(const P& info, const CTrackedEventInfo<T>& trackingInfo, const CEntity* pEntity=NULL, bool allowNew = false)
	{
		if(IsEntityNotValid(pEntity))
			return;

		const CPhysical* pEntities[2] = {(CPhysical*)pEntity, NULL};
		CReplayMgrInternal::RecordPersistantFx<P, T>(info, trackingInfo, pEntities, allowNew);	
	}

	template<typename P, typename T>
	static void				RecordPersistantFx(const P& info, const CTrackedEventInfo<T>& trackingInfo, const CEntity* pEntity1, const CEntity* pEntity2, bool allowNew = false)
	{	
		if(IsEntityNotValid(pEntity1) || IsEntityNotValid(pEntity2))
			return;

		const CPhysical* pEntities[2] = {(CPhysical*)pEntity1, (CPhysical*)pEntity2};
		CReplayMgrInternal::RecordPersistantFx<P, T>(info, trackingInfo, pEntities, allowNew);	
	}

	template<typename T>
	static void				StopTrackingFx(const CTrackedEventInfo<T>& trackingInfo)
	{
		CReplayMgrInternal::StopTrackingFx(trackingInfo);
	}

	static void				ReplayRecordSound(const u32 soundHash, const audSoundInitParams *initParams = NULL, const CEntity* pAudEntity = NULL, const CEntity* pTracker = NULL, eAudioSoundEntityType entityType = eNoGlobalSoundEntity, const u32 slowMoSoundHash = g_NullSoundHash);
	static void				ReplayRecordSound(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams *initParams = NULL, const CEntity* pAudEntity = NULL, const CEntity* pTracker = NULL, eAudioSoundEntityType entityType = eNoGlobalSoundEntity, const u32 slowMoSoundRef = g_NullSoundHash);
	static void				ReplayRecordSoundPersistant(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams *initParams, audSound *trackingSound, const CEntity* pAudEntity = NULL, eAudioSoundEntityType entityType = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID, bool isUpdate = false);
	static void				ReplayRecordSoundPersistant(const u32 soundHash, const audSoundInitParams *initParams, audSound *trackingSound, const CEntity* pAudEntity = NULL, eAudioSoundEntityType entityType = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID, bool isUpdate = false);
	static void				ReplayPresuckSound(const u32 presuckSoundHash, const u32 presuckPreemptTime, const u32 dynamicSceneHash, const CEntity* pAudEntity);

	static void				ReplayRecordPlayStreamFromEntity(CEntity* pEntity, audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);
	static void				ReplayRecordPlayStreamFromPosition(const Vector3 &pos, audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);
	static void				ReplayRecordPlayStreamFrontend(audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);
	static void				RecordStopStream();

	static void				RecordEntityAttachment(const CReplayID& child, const CReplayID& parent);
	static void				RecordEntityDetachment(const CReplayID& child, const CReplayID& parent);

	static	void			OnCreateEntity(CPhysical* pEntity)				{	CReplayMgrInternal::OnCreateEntity(pEntity);			}
	static	void			OnDelete(CEntity* pElement)						{	if(pElement->GetIsPhysical())	CReplayMgrInternal::OnDeleteEntity((CPhysical*)pElement);		}
	static	void			OnPlayerDeath()									{	CReplayMgrInternal::OnPlayerDeath();					}

	static void				RecordWarpedEntity(CDynamicEntity* pEntity);
	static void				CancelRecordingWarpedOnEntity(CPhysical* pEntity);

	// Camera functions
	static	bool			UseRecordedCamera()								{	return CReplayMgrInternal::UseRecordedCamera();			}
	static	const CReplayFrameData&	GetReplayFrameData()					{	return CReplayMgrInternal::GetReplayFrameData();		}
	static	const CReplayClipData&	GetReplayClipData() 					{	return CReplayMgrInternal::GetReplayClipData();			}

	static	CPed*			GetMainPlayerPtr()								{	return CReplayMgrInternal::GetMainPlayerPtr();			}
	static  void			SetMainPlayerPtr(CPed* pPlayer)					{	CReplayMgrInternal::SetMainPlayerPtr(pPlayer);			}

	static	void			UpdateTimer()									{	CReplayMgrInternal::UpdateTimer();						}
	static	u32				GetPlayBackStartTimeAbsoluteMs()				{	return CReplayMgrInternal::GetPlayBackStartTimeAbsoluteMs(); }
	static	u32				GetPlayBackEndTimeAbsoluteMs()					{	return CReplayMgrInternal::GetPlayBackEndTimeAbsoluteMs(); }
	static	u32				GetPlayBackEndTimeRelativeMs()					{	return CReplayMgrInternal::GetPlayBackEndTimeRelativeMs(); }
	static	u32				GetTotalTimeRelativeMs()						{	return CReplayMgrInternal::GetTotalTimeRelativeMs();	}

	// Time querying functions
	static	u32				GetCurrentTimeRelativeMs()						{	return CReplayMgrInternal::GetCurrentTimeRelativeMs();	}
	static	float			GetCurrentTimeRelativeMsFloat()					{	return CReplayMgrInternal::GetCurrentTimeRelativeMsFloat();	}
	static float			GetPlaybackFrameStepDeltaMsFloat()				{	return CReplayMgrInternal::GetPlaybackFrameStepDeltaMsFloat(); }
	static	float			GetVfxDelta()									{	return CReplayMgrInternal::GetVfxDelta();				}

	// Global packet Querying functions
	static	void			OverrideWeatherPlayback(bool b)					{	CReplayMgrInternal::OverrideWeatherPlayback(b);			}
	static	void			OverrideTimePlayback(bool b)					{	CReplayMgrInternal::OverrideTimePlayback(b);			}

	// Replay state changes
	static	void			SetNextPlayBackState(u32 state)					{	CReplayMgrInternal::SetNextPlayBackState(state);		}
	static	void			ClearNextPlayBackState(u32 state)				{	CReplayMgrInternal::ClearNextPlayBackState(state);		}
	static	const CReplayState&	GetCurrentPlayBackState()					{	return CReplayMgrInternal::GetCurrentPlayBackState();	}
	static	const CReplayState&	GetNextPlayBackState()						{	return CReplayMgrInternal::GetNextPlayBackState();		}
	static  void			StopPreCaching()								{   CReplayMgrInternal::StopPreCaching();					}

	static	bool			IsJustPlaying()									{	return CReplayMgrInternal::IsJustPlaying();				}
	static	bool			IsPlaybackPaused()								{	return CReplayMgrInternal::IsPlaybackPaused();			}
	static	bool			IsPreCachingScene()								{	return CReplayMgrInternal::IsPreCachingScene();			}
	static	bool			IsClipTransition()								{	return CReplayMgrInternal::IsClipTransition();			}
	static	bool			IsSinglePlaybackMode()							{	return CReplayMgrInternal::IsSinglePlaybackMode();		}
	static	bool			WillSave()										{   return CReplayMgrInternal::WillSave();					}
	static	bool			IsSaving()										{	return CReplayMgrInternal::IsSaving();					}
	static	bool			CanSaveClip()									{	return CReplayMgrInternal::CanSaveClip();				}
	static	void			KickPrecaching(bool clearSounds)				{	CReplayMgrInternal::KickPrecaching(clearSounds);		}


	static	bool			IsPerformingFileOp()							{	return CReplayMgrInternal::IsPerformingFileOp();		}
	static	bool			IsSafeToTriggerUI()								{	return CReplayMgrInternal::IsSafeToTriggerUI();			}

	static void				SaveMarkedUpClip(MarkerBufferInfo& info)			{	CReplayMgrInternal::SaveMarkedUpClip(info);	}

	// Settings
	static  void			SetBytesPerReplayBlock(u32 val)					{	CReplayMgrInternal::SetBytesPerReplayBlock(val);		}
	static  void			SetNumberOfReplayBlocksFromSettings(u16 val)	{	CReplayMgrInternal::SetNumberOfReplayBlocksFromSettings(val);		}
	static  void			SetMaxSizeOfStreamingReplay(u32 val)			{	CReplayMgrInternal::SetMaxSizeOfStreamingReplay(val);	}

	static u16				GetNumberOfReplayBlocks()						{	return CReplayMgrInternal::GetNumberOfReplayBlocks();			}

	static	void			SetTrafficLightCommands(CEntity* pEntity, const char* commands);
	static  const char*		GetTrafficLightCommands(CEntity* pEntity);

	static void				SetVehicleVariationData(CVehicle* pVehicle);

	static void				RecordMapObject(CObject* pObject);
	static void				StopRecordingMapObject(CObject* pObject);
	static bool				IsRecordingMapObject(CObject* pObject);
	static void				RecordObject(CObject* pObject);
	static void				StopRecordingObject(CObject* pObject);
	static bool				ShouldHideMapObject(CObject* pObject);
	static bool				DisallowObjectPhysics(CObject* pObject);
	static bool				ShouldProcessControl(CEntity* pEntity);
	static bool				ShouldRecordWarpFlag(CEntity* pEntity);

#if __BANK
	static bool				IsMapObjectExpectedToBeHidden(u32 hash, CReplayID& replayID, FrameRef& frameRef);
#endif // __BANK

	// Interior/Room recording.
	static void				RecordRoomRequest(u32 interiorProxyNameHash, u32 posHash, u32 roomIndex);
	static bool				NotifyInteriorToExteriorEviction(CObject *pObject, fwInteriorLocation interiorLocation);

	// Destroyed map object recording.
	static void				RecordDestroyedMapObject(CObject *pObject);
	static void				RecordRemadeMapObject(CObject *pObject);

	// Cursor control
	static void				JumpToFloat(float timeToJumpToMS, u32 jumpOptions);
	using					CReplayMgrInternal::IsJumping;
	static bool				WasJumping()									{	return CReplayMgrInternal::WasJumping();						}
	
	static float			GetCursorSpeed()								{	return CReplayMgrInternal::GetCursorSpeed();					}
	static void				SetCursorSpeed(float speed)						{	CReplayMgrInternal::SetCursorSpeed(speed);						}
	static void				ResumeNormalPlayback()							{	CReplayMgrInternal::ResumeNormalPlayback();						}

	// Markers
	static float			GetMarkerSpeed()								{	return CReplayMgrInternal::GetMarkerSpeed();					}
	static void				SetMarkerSpeed(float speed)						{	CReplayMgrInternal::SetMarkerSpeed(speed);						}

	static	u32				GetTotalMsInBuffer()							{	return CReplayMgrInternal::GetTotalMsInBuffer();				}
	static	u32				GetPacketInfoInSaveBlocks(atArray<CBlockProxy>& blocksToSave, u32& startTime, u32& endTime, u32&frameFlags)	{	return CReplayMgrInternal::GetPacketInfoInSaveBlocks(blocksToSave, startTime, endTime, frameFlags);	}

	static bool				IsUsingCustomDelta()							{	return CReplayMgrInternal::IsUsingCustomDelta();				}

	static	bool			IsCursorFastForwarding()						{	return CReplayMgrInternal::IsCursorFastForwarding(); }
	static bool				IsCursorRewinding()								{	return CReplayMgrInternal::IsCursorRewinding();					}
	
	static u32				GetGameTimeAtRelativeFrame(s32 frameOffset)		{	return CReplayMgrInternal::GetGameTimeAtRelativeFrame(frameOffset);	}

	// Miscellaneous
	static void				CaptureUpdatedThumbnail()						{	CReplayMgrInternal::CaptureUpdatedThumbnail();						}
	static void				PauseRenderPhase()								{	CReplayMgrInternal::PauseRenderPhase();								}

	static bool				IsPlaybackFlagsSet(u32 flag)					{	return CReplayMgrInternal::IsPlaybackFlagSet(flag);					}
	static bool				IsPlaybackFlagSetNextFrame(u32 flag)			{	return CReplayMgrInternal::IsPlaybackFlagSetNextFrame(flag);		}
	static bool				IsUsingRecordedCamera()							{	return CReplayMgrInternal::IsUsingRecordedCamera();					}
	static void				SetWasModifiedContent()							{	CReplayMgrInternal::SetWasModifiedContent();						}

	//block info
	static	CBlockProxy		GetCurrentBlock()								{	return CReplayMgrInternal::GetCurrentBlock();						}
	static  CBlockProxy		FindFirstBlock()								{   return CReplayMgrInternal::FindFirstBlock();						}
	static  CBlockProxy		FindLastBlock()									{   return CReplayMgrInternal::FindLastBlock();							}
	static  int				ForeachBlock(int(func)(CBlockProxy, void*), void* pParam = NULL, u16 numBlocks = 0)	{	return CReplayMgrInternal::ForeachBlock(func, pParam, numBlocks);	}

	static bool				SaveBlock(CBlockProxy block)					{   return CReplayMgrInternal::SaveBlock(block);						}
	static void				ClearSaveBlocks()								{   return CReplayMgrInternal::ClearSaveBlocks();						}
	static bool				IsBlockMarkedForSave(CBlockProxy block)			{   return CReplayMgrInternal::IsBlockMarkedForSave(block);				}
	static bool				IsAnyTempBlockOn()								{   return CReplayMgrInternal::IsAnyTempBlockOn();						}

	//for debug
	static bool				IsOkayToDelete()								{   return CReplayMgrInternal::IsOkayToDelete(); }

	static bool				ShouldIncreaseFidelity();
	static bool				ShouldSustainRecording()						{   return CReplayMgrInternal::ShouldSustainRecording();				}

	static bool				ShouldRegisterElement(const CEntity* pEntity)	{	return CReplayMgrInternal::ShouldRegisterElement(pEntity);			}

	static bool				FakeSnipingMode();

	static bool				ShouldPreventMapChangeExecution(const CEntity* pEntity);
#if USE_SRLS_IN_REPLAY
	static void				SetRecordingSRL(const bool set)						{	CReplayMgrInternal::SetRecordingSRL(set);						}
#endif
public:
	static void				ExtractCachedWaterFrame(float *pDest, float *pSecondaryDest);

	static bool				ShouldForceHideEntity(CEntity* pEntity);

public:
	static u32				GenerateObjectHash(const Vector3 &position, s32 modelNameHash);
	static u32				GetInvalidHash();
	static u32				hash(const u32 *k, u32 length, u32 initval);

public:
	static void				RecordCloth(CPhysical *pEntity, rage::clothController *pClothController, u32 i);
	static void				SetClothAsReplayControlled(CPhysical *pEntity, fragInst *pFragInst, bool onOff);
	static void				PreClothSimUpdate(CPed *pLocalPlayer);
private:
	static CReplayInterfaceVeh*	GetVehicleInterface();
	static CReplayInterfaceObject*	GetObjectInterface();
	static CReplayInterfaceGame* GetGameInterface();

	friend class CPacketVehicleCreate;
};

#if __ASSERT
namespace rage
{
	extern void	SetNoPlacementAssert(bool var);
};
#endif // __ASSERT

#endif // GTA_REPLAY

#endif // _REPLAY_H_
