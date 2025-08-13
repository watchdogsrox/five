/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneManagerNew.h
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED :
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_CUTSCENEMANAGER_H 
#define CUTSCENE_CUTSCENEMANAGER_H 

//Rage game files
#include "atl/Array.h"
#include "cutscene/cutsmanager.h"
#include "cutscene/cutsentity.h"
#include "cutfile/cutfeventargs.h"
#include "cutfile/cutfobject.h"
#include "fwdebug/debugbank.h"
#include "grcore/stateblock.h"
#include "script/thread.h"

//Game header files
#if __BANK
	#include "Cutscene/CutSceneDebugManager.h"
	#include "Debug/Editing/CutsceneEditing.h"
#endif

#include "Cutscene/CutSceneAssetManager.h"
#include "Cutscene/CutSceneDefine.h"
#include "Cutscene/CutSceneStore.h"
#include "scene/RegdRefTypes.h"
#include "scene/Entity.h"
#include "system/criticalsection.h"

#include "renderer/Sprite2d.h"	// for USE_MULTIHEAD_FADE

#if !__NO_OUTPUT
#define cutsceneManagerErrorf(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_ERROR) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_ERROR) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneErrorf("%s" fmt, debugStr,##__VA_ARGS__); }
#define cutsceneManagerWarningf(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_WARNING) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_WARNING) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneWarningf("%s" fmt, debugStr,##__VA_ARGS__); }
#define cutsceneManagerDisplayf(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DISPLAY) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDisplayf("%s" fmt, debugStr,##__VA_ARGS__); }
#define cutsceneManagerDebugf1(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG1) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG1) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf1("%s" fmt, debugStr,##__VA_ARGS__); }
#define cutsceneManagerDebugf2(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG2) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf2("%s" fmt, debugStr,##__VA_ARGS__); }
#define cutsceneManagerDebugf3(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG3) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf3("%s" fmt, debugStr,##__VA_ARGS__); }
#else
#define cutsceneManagerErrorf(fmt,...) do {} while(false)
#define cutsceneManagerWarningf(fmt,...) do {} while(false)
#define cutsceneManagerDisplayf(fmt,...) do {} while(false)
#define cutsceneManagerDebugf1(fmt,...) do {} while(false)
#define cutsceneManagerDebugf2(fmt,...) do {} while(false)
#define cutsceneManagerDebugf3(fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

#if !__NO_OUTPUT
#define cutsceneManagerVisibilityDebugf3(fmt,...) if ( (Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf3("%s" fmt, debugStr,##__VA_ARGS__); }
#else
#define cutsceneManagerVisibilityDebugf3(fmt,...) do {} while(false)
#endif //!CUTSCENE_VISIBILITY_DEBUG


#if !__NO_OUTPUT
#define cutsceneManagerVariationDebugf3(fmt,...) if ( (Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3) ) { char debugStr[256]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf3("%s" fmt, debugStr,##__VA_ARGS__); }
#else
#define cutsceneManagerVariationDebugf3(fmt,...) do {} while(false)
#endif //!CUTSCENE_VARIATION_DEBUG


///////////////////////////////////////////////////////////////////////////////////////////////////

#include "system/autogpucapture.h"

#if ENABLE_STATS_CAPTURE && __BANK
#	define ENABLE_CUTSCENE_TELEMETRY    1
#else
#	define ENABLE_CUTSCENE_TELEMETRY    0
#endif

#if ENABLE_CUTSCENE_TELEMETRY

class CutSceneManager;

class CutsceneLightsTelemetry
{
public:
	CutsceneLightsTelemetry()
	{ 
		Reset(); 
	}

	void	CutSceneStart(const char *pName, bool bIsCutsceneCameraApproved, bool bIsCutsceneLightingApproved);		// Call when the cutscene starts playing
	void	CutSceneStop();							// Call when the cutscene stops playing
	void	Update(CutSceneManager *pManager);		// Update timing data
	bool	ShouldOutputTelemetry(); 				// Ticks when a cutscene stops, NetworkTelemetry will pick up on that

	atHashString &GetName()				{ return m_Name; }
	bool	GetCameraApproved()			{ return m_CameraApproved; }
	bool	GetLightingApproved()		{ return m_LightingApproved; }
	bool	GetDOFWasActive()			{ return m_DOFWasActive; }

	void	Reset()
	{
		m_Started = m_WasStarted = m_OutputNow = false; 
		m_CameraApproved = m_LightingApproved = false;	
		m_DOFWasActive = false;
		m_DirectionalLightsTimeSample.Reset();
		m_SceneLightsTimeSample.Reset();
		m_LODLightsTimeSample.Reset();
		m_TotalLightsTimeSample.Reset();
	}

	bool	m_Started;
	bool	m_WasStarted;
	bool	m_OutputNow;

	atHashString	m_Name;
	bool	m_CameraApproved;
	bool	m_LightingApproved;
	bool	m_DOFWasActive;

	MetricsCapture::SampledValue<float> m_DirectionalLightsTimeSample;
	MetricsCapture::SampledValue<float> m_SceneLightsTimeSample;
	MetricsCapture::SampledValue<float> m_LODLightsTimeSample;
	MetricsCapture::SampledValue<float> m_TotalLightsTimeSample;

};

extern	CutsceneLightsTelemetry	g_CutSceneLightTelemetryCollector;

#endif	// ENABLE_CUTSCENE_TELEMETRY



namespace rage {
	class crClip;
}

class CCutSceneLight;
class CCutSceneParticleEffect; 
class CCutSceneAnimMgrEntity; 
class CCutsceneAnimatedModelEntity; 
class CCutsceneAnimatedActorEntity; 
class CCutsceneAnimatedVehicleEntity; 
class CCutSceneCameraEntity; 
class CInteriorProxy;

enum eCutSceneExitFlags
{
	CEF_HEALTH				= 1,
	CEF_PED_VARIATION		= 2, 
};

enum SeamlessSkip
{
	SS_SET_SEAMLESS_SKIP, 
	SS_STOP_CUTSCENE_NOW, 
	SS_DEFAULT
};

enum StreamingFlags
{        
	CS_LOAD_ANIM_DICT_EVENT = BIT0,          
	CS_UNLOAD_ANIM_DICT_EVENT= BIT1,       
	CS_LOAD_AUDIO_EVENT= BIT2,              
	CS_UNLOAD_AUDIO_EVENT= BIT3,            
	CS_LOAD_MODELS_EVENT= BIT4,             
	CS_UNLOAD_MODELS_EVENT= BIT5,           
	CS_LOAD_PARTICLE_EFFECTS_EVENT= BIT6,   
	CS_UNLOAD_PARTICLE_EFFECTS_EVENT = BIT7, 
	CS_LOAD_OVERLAYS_EVENT = BIT8,           
	CS_UNLOAD_OVERLAYS_EVENT = BIT9,         
	CS_LOAD_SUBTITLES_EVENT = BIT10,         
	CS_UNLOAD_SUBTITLES_EVENT = BIT11,       
	CS_LOAD_RAYFIRE_EVENT = BIT12,	 
	CS_UNLOAD_RAYFIRE_EVENT = BIT13, 
	CS_LOAD_SCENE_EVENT = BIT14,
	CS_UNLOAD_SCENE_EVENT = BIT15, 
	CS_LOAD_INTERIORS_EVENT = BIT16, 
	CS_UNLOAD_INTERIORS_EVENT = BIT17
};

enum PlayBackContextFlags
{
	CUTSCENE_REQUESTED_FROM_WIDGET = BIT0, 
	CUTSCENE_REQUESTED_DIRECTLY_FROM_SKIP = BIT1,
	CUTSCENE_REQUESTED_FROM_Z_SKIP = BIT2,
	CUTSCENE_REQUESTED_IN_MISSION = BIT3,
	CUTSCENE_PLAYBACK_FORCE_LOAD_AUDIO_EVENT = BIT4
};

enum OptionFlags
{
	CUTSCENE_OPTIONS_NONE = 0,

	CUTSCENE_PLAYER_TARGETABLE	= BIT0,	// Allows peds to continue interacting with the player whilst the scene is running.
	CUTSCENE_PROCGRASS_FORCE_HD	= BIT1,	// forces proc grass to use highest available LOD geometries
	CUTSCENE_DO_NOT_REPOSITION_PLAYER_TO_SCENE_ORIGIN = BIT2, // If specified, the player ped is not teleported to the scene origin if they are not in the cutscene.
	CUTSCENE_NO_WIDESCREEN_BORDERS = BIT3, // If specified, the minimap is not hidden at the start of the cutscene.
	CUTSCENE_DELAY_ENABLING_PLAYER_CONTROL_FOR_UP_TO_DATE_GAMEPLAY_CAMERA = BIT4,
	CUTSCENE_DO_NOT_CLEAR_PICKUPS = BIT5, //If specified, pickups will not be removed for the duration of the cutscene.
	CUTSCENE_CREATE_OBJECTS_AT_SCENE_ORIGIN = BIT6, //If specified, objects will be created at the scene origin
	CUTSCENE_PLAYER_EXITS_IN_A_VEHICLE = BIT7,
	CUTSCENE_PLAYER_FP_FLASH_MICHAEL = BIT8, // Use Michael's colour coded first person transition flash if exiting the cutscene into first person
	CUTSCENE_PLAYER_FP_FLASH_FRANKLIN = BIT9, // Use Franklin's colour coded first person transition flash if exiting the cutscene into first person
	CUTSCENE_PLAYER_FP_FLASH_TREVOR = BIT10, // Use Trevor's colour coded first person transition flash if exiting the cutscene into first person
	CUTSCENE_SUPPRESS_FP_TRANSITION_FLASH = BIT11, // Disable the first person transition flash on the cutscene exit
	CUTSCENE_USE_FP_CAMERA_BLEND_OUT_MODE = BIT12, // Allow the special first person only blend out mode when doing a standard camera blend back to first person mode.
	CUTSCENE_EXITS_INTO_COVER = BIT13,
};

#if __BANK
class CutsceneApprovalStatuses
{
public:
	CutsceneApprovalStatuses() {}
	~CutsceneApprovalStatuses() {}

	atHashString	m_CutsceneName;

	bool			m_FinalApproved;
	bool			m_AnimationApproved;
	bool			m_CameraApproved;
	bool			m_DofApproved;
	bool			m_LightingApproved;
	bool			m_FacialApproved;

	PAR_SIMPLE_PARSABLE; 
};

class ApprovedCutsceneList
{
public:
	ApprovedCutsceneList() {}
	~ApprovedCutsceneList() {}

	atArray<CutsceneApprovalStatuses> m_ApprovalStatuses;

	PAR_SIMPLE_PARSABLE; 
};
#endif // __BANK

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
class AuthorizedCutscene
{
public: 
	AuthorizedCutscene() {}
	~AuthorizedCutscene() {}

	atArray<atHashString> m_AuthorizedCutsceneList;

	PAR_SIMPLE_PARSABLE; 
};
#endif

//Describes a zone for triggering a seamless cut scene
struct CutSceneTrigger
{
	Matrix34 TriggerMat; //Need a trigger area for some seamless cut scene
	Vector3 vTriggerOffset;  //Store the trigger offset
	float fTriggerOrient;	//Trigger orientation 
	float fTriggerRadius; 
	float fTriggerAngle; 
	float fPlayerAngle; 
	bool bScriptOveride; 
};

class CutSceneManager : public cutsManager
{
public:

#if __BANK	
	friend class CCutSceneDebugManager; 
	friend class DebugEditing::CutsceneEditing;
	friend class CCascadeShadowBoundsDebug; 
#endif

	// The following vars are accessed by the render thread in the debug bar render
	struct DebugRenderState
	{
		void Reset()
		{
			isRunning = false;
			cutSceneCurrentTime = 0.0f;
			cutScenePreviousTime = 0.0f;
			cutscenePlayTime = 0.0f;
			cutsceneDuration = 0.0f;
			totalSeconds = 0.0f;
			currentConcatSectionIdx = -1;
			concatSectionCount = 0;
			cutsceneName[0] = 0;
			concatDataSceneName[0] = 0;
		}

		bool isRunning;
		float cutSceneCurrentTime;
		float cutScenePreviousTime;
		float totalSeconds;
		float cutscenePlayTime;
		float cutsceneDuration;
		int currentConcatSectionIdx;
		int concatSectionCount;
		char cutsceneName[128];
		char concatDataSceneName[128];
	};

	CutSceneManager();
	~CutSceneManager();
	
	void Init();
	void ShutDown();

// Cut scene manager creation, deletion and access
	static void CreateInstance();	//Create an instance of the cut scene.
	static CutSceneManager* GetInstance();	//Get access to our cut scene manager.
	static CutSceneManager* GetInstancePtr() { return sm_pInstance; }	//Get access to our cut scene manager, but don't fail any assert if it doesn't exist.
	static void DeleteInstance();	//Delete our cut scene manager at shutdown.
	void ShutDownCutscene();	//Returns the game to a normal state after the cut scene
	void ReleaseScriptResource(u32 cutSceneNameHash);

	// Safety mechanism to avoid getting stuck waiting for screen fades when other
	// game systems fade in the camera.
	bool IsFadeTimerComplete();

	virtual bool FadeOut( const Color32 &color, float fDuration );
	
//	** LOADING **	
	//Base Class Override: Sets the Pre scene update state to load cutscene file.
	virtual void Load( const char*, const char* pExtension=NULL, bool bPlayNow=true, 
		EScreenFadeOverride fadeOutGameAtBeginning=DEFAULT_FADE, EScreenFadeOverride fadeInCutsceneAtBeginning=DEFAULT_FADE,
		EScreenFadeOverride fadeOutCutsceneAtEnd=DEFAULT_FADE, EScreenFadeOverride fadeInGameAtEnd=DEFAULT_FADE, bool bJustCutfile=false  );

	virtual bool LoadCutFile(const char*);	//Requests the streaming system to load the named cut files

//	** CUTSCENE STATE OVERRIDES **
	//Base Class Override: need to override this to guarantee that our cut file is loaded, the RDR implementation relies on a call back function. 
	virtual void DoLoadingCutfileState();
	virtual void DoLoadState(); 
	virtual void DoPlayState(); 
	virtual void DoLoadingState(); 
	virtual void DoPausedState();	//Pause any anims played through the task, possibly could pause the task
	virtual void DoFadingOutState(); 
	virtual void DoUnloadState();
	virtual void DoSkippingState(); 
	virtual void DoStoppedState(); 
	virtual void DoLoadingBeforeResumingState(); 
	virtual void DoAuthorizedState(); 

	bool AreReservedEntitiesReady(); 
//	** CUTSCENE STATE UPDATES **
	//Base Class Override: Main function that updates the cut scene state
	virtual void PreSceneUpdate();	//Useful to put debug draw info in here as it get called every frame
	virtual void PostSceneUpdate (float UNUSED_PARAM(fDelta));	//Base Class Override: Updates the timer at the end of that frame

	virtual bool IsLoading() const;

	//sets a flag that the pre-scene update was called so we can tell post scene system to also update
	void SetPreSceneUpdate (bool bPreSceneUpdate) { m_bPreSceneUpdateEventCalled = bPreSceneUpdate; }
	bool GetPreSceneUpdate () { return m_bPreSceneUpdateEventCalled; }	//Gets is the post scene update was called
	bool GetPostSceneUpdate() { return m_bPostSceneUpdateEventCalled; }	//Get the update date status
	void SetPostSceneUpdate(bool bUpdatePostScene) { m_bPostSceneUpdateEventCalled = bUpdatePostScene;  } // Set a flag so systems that in the post scene update should update 

	void SetLightPresceneUpdate(bool bPreSceneUpdate) { m_bPresceneLightUpdateEvent = bPreSceneUpdate; }
	bool GetLightPresceneUpdateOnly() { return m_bPresceneLightUpdateEvent; } 

	//Base Class Override: Dispatches an event class to all cut scene entities. Some entities are responsible for multiple objects and some are 
	//per object.
	//A cut scene entity is a class that interfaces between a rage events and game side cut scene objects.
	//The event args contains the info relating to the event ie fade event, would contain the colour, length of fade etc.
	virtual void DispatchEventToAllEntities(s32 iEventid, const cutfEventArgs *pEventsArgs=NULL);
	void DispatchUpdateEventPostScene();
	void DispatchEventToObjectsOfType(s32 iObjectType, s32 iEventId, cutfEventArgs *pEventArgs = NULL); 
	
//	** POSITION & ROTATION
	void SetNewStartPos(const Vector3 &vPos); 
	void SetNewStartHeading(const float &vPos); 
	void OverrideConcatSectionPosition(const Vector3 &vPos,  s32 concatsection); 
	void OverrideConcatSectionHeading(const float& heading, s32 concatsection); 
	void OverrideConcatSectionPitch(const float& pitch, s32 concatsection);
	void OverrideConcatSectionRoll(const float& roll, s32 concatsection);

//	** BLOCKING **	
	bool IsPointInBlockingBound(const Vector3 &vVec); 
#if __BANK
	s32 GetNumBlockingBoundObjects() { return m_editBlockingBoundObjectList.GetCount(); }
#endif //__BANK

//	** TIME, PHASE, FRAMES **
	float GetCutSceneCurrentTime () const { return m_fTime; } //Gets the current time of cut scene  
	float GetCutScenePreviousTime() const { return m_fPreviousTime; }
	float GetCutScenePlayTime() const { return m_fPlayTime; }
	float GetCutSceneDuration() const { return m_fDuration; }
	float GetCurrentCutSceneTimeStep () const { return m_fTime - m_fPreviousTime; }
	float GetCutScenePhase() const; 
	float GetAnimPhaseForSection(float fDuration, float fSectionStartTime, float fCurrentTime) const ; //Gets the anim phase for a given anim for that section 
	float GetPhaseUpdateAmount(const crClip* pClip, float fEventDispatchTime) const; //Get the phase amount to update the anim, also looks for bliking tags in the anim data
	float GetPlayTime(float sceneTime); // convert from the full range time to an actual play time based on the concat data list.
	//store the playback flags from script so we can set concat sections to be invalid for playback
	void  SetConcatSectionPlaybackFlags(s32 Flags) { m_ValidConcatSectionFlags = Flags; }
	bool  CanScriptChangeEntitiesPreUpdateLoading() const {return m_bCanScriptSetupEntitiesPreUpdateLoading; }
	bool  CanScriptChangeEntityModel() const {return m_bCanScriptChangeEntitiesModel; }
	void  SetScriptCanChangeEntitiesPreUpdateLoading(bool canScriptSetEntityStates) { m_bCanScriptSetupEntitiesPreUpdateLoading = canScriptSetEntityStates; }
	bool  CanScriptRequestSyncedSceneAudioPostScene() { return m_fTime > m_fFinalAudioPlayEventTime; } 
	//bool  IsAuthorizedForScript(); 
	//bool  IsAuthorizedForScript(const char* pSceneName); 

	void  SetPlaybackFlags(s32 flags) { m_PlaybackFlags.SetFlag(flags); }
	fwFlags32 GetPlayBackFlags() const { return m_PlaybackFlags; }
	bool DidFailToLoadInTimeBeforePlayWasCalled() { return m_bFailedToLoadBeforePlayWasRequested; }	

	fwFlags32 GetOptionFlags() const { return m_OptionFlags; }

//	** PLAYBACK, PLAYBACK STATE
	//Start the cut scene by calling the load cut scene with the cut scene name.
	void RequestCutscene(const char* pFileName, bool bPlayNow, EScreenFadeOverride fadeOutGameAtBeginning, EScreenFadeOverride fadeInCutsceneAtBeginning, EScreenFadeOverride fadeOutCutsceneAtEnd, EScreenFadeOverride fadeInGameAtEnd, scrThreadId ScriptId, s32 PlayBackContextFlags );	
	void PlaySeamlessCutScene(scrThreadId ScriptId, u32 OptionFlags = CUTSCENE_OPTIONS_NONE);
	void StartCutscene(); 

	bool IsRunning(); //Checks that the cut scene is not in an idle state
	bool IsStreaming(); //Checks that the cut scene is not streaming any of its assets.
	bool IsStreamedCutScene() { return m_bStreamedCutScene; } //Some cut scenes are pre streamed so tells audio that we require action soon as the play starts.  
	void TriggerCutsceneSkip(bool bForceSkip = false); 
	void UnloadPreStreamedScene(scrThreadId ScriptId); 
	bool WasSkipped () const { return m_bCutsceneWasSkipped; } 
	bool IsActive() const { return m_bIsCutSceneActive; }
	bool IsCutscenePlayingBack() const 
	{
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			return CReplayMgr::IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_CUTSCENE);
		}
		else
#endif
		{
			return m_IsCutscenePlayingBack;
		}
	} //cache this as a variable dont use the states 

	void SetCanSkipCutSceneInMultiplayer(bool bCanSkip) { m_CanSkipCutSceneInMultiplayerGame = bCanSkip; } 

	bool IsCutscenePlayingBackAndNotCutToGame() const
	{
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			return CReplayMgr::IsPlaybackFlagsSet(FRAME_PACKET_RECORDED_CUTSCENE);
		}
		else
#endif
		{
			return m_bCameraWillCameraBlendBackToGame;
		}
	}

	const CEntity* GetCascadeShadowFocusEntity();

#if !__FINAL
	virtual bool RequiresPreviousAnimDictOnSkip() { return GetPlayBackFlags().IsFlagSet(CUTSCENE_REQUESTED_FROM_WIDGET); }
#endif // !__FINAL

#if __BANK
	fwEntity*	GetIsolatedPedEntity() const;
#endif

	void StopCutsceneAndDontProgressAnim(); 

	//** TRANSITIONS IN AND OUT OF GAME STATE
	void SetCutSceneToGameState(bool forcePlayerControlOn);	//cut scene is over hand control back to the player
	bool CanSetCutSceneEnterStateForEntity(atHashString& SceneNameHash, atHashString& ModelNameHash);  //Tells the script that it can set ped attributes at the start of the scene.  
	bool CanSetCutSceneExitStateForEntity(atHashString& SceneNameHash, atHashString& ModelNameHash); //Tells a script that it can set ped attributes at the end of the scene. 
	bool CanSetCutSceneExitStateForCamera(bool bHideNonRegisteredEntities = false); 

	//** OVERLAYS AND BORDERS
	static void RenderCutsceneBorders(); 	//Renders a cut scene border.
	static void RenderOverlayToMainRenderTarget(bool bIsCutscenePlayingBack); 
	static void RenderOverlayToRenderTarget(unsigned int targetId);
	void RenderBinkMovieAndUpdateRenderTargets(); 
	static void Synchronise()
	{
		m_RenderBufferIndex ^= 1;
		#if __BANK
			UpdateDebugRenderState();
		#endif
	}

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
	static void RenderAuthorizedForScriptScreen(); 
	bool IsSceneAuthorized(atHashString& CutSceneHashString); 
	static void RenderWatermark();
#endif

	//** INTERIORS
	void GetInteriorInfo(bool bForceLoading); 
	
	//** PLAYER AND PEDS//depot/gta5/script/dev/shared/include/native/commands_cutscene.sch
	s32 GetPlayerSceneId() const { return m_iPlayerObjectId;  }
	bool GetIsPlayerInScene() const { return m_bIsPlayerInScene; } 	//Checks if the player is in the scene.
	bool GetIsPedModel(u32 iModelIndex) const; //Check that the ped model we are trying to load is streamed and if so, created a real ped.

	//** SEAMLESS CUTSCENE GAME ENTITY REGISTERING
	void RegisterGameEntity(CDynamicEntity* pEntity, atHashString& SceneNameHash, atHashString& ModelNameHash, bool bDeleteBeforeEnd, bool bCreatedForScript,bool bAppearInScene = true, u32 options = 0); 
	CCutsceneAnimatedModelEntity* GetAnimatedModelEntityFromModelHash(atHashString& modelNameHash); 
	CCutsceneAnimatedModelEntity* GetAnimatedModelEntityFromSceneHandle(atHashString& SceneHandleHash, atHashString& modelNameHash); 
	CCutsceneAnimatedModelEntity* GetAnimatedModelEntityFromEntity(const CEntity *pEntity); 
	bool HasScriptVisibleTagPassedForEntity(const CEntity *pEntity, s32 EventHash); 

	void GetEntityByType(s32 EntityType, atArray<cutsEntity*> &pEntityList);
	void HideNonRegisteredModelEntities();

	// Variation management
	// If ModelHash is defined sets the variation on the given model otherwise sets the variation on all matching handles
	void SetCutScenePedVariation(atHashString& sceneHashString, int ComponentID, int DrawableID, int TextureID, atHashString& modelNameHash);
	void SetCutScenePedVariationFromPed(atHashString& sceneHashString, const CPed* pPed, atHashString& modelNameHash);
	void SetCutScenePedPropVariation(atHashString& sceneHashString, int Position, int NewPropIndex, int NewTextIndex, atHashString& modelNameHash);
	void ChangeCutSceneModel(atHashString& sceneHashString, atHashString& modelNameHash, atHashString& newmodelNameHash); 
	void SetCutSceneEntityStreamingFlags(atHashString& sceneHashString, atHashString& modelNameHash, u32 flags); 

	bool HasSceneStartedInTheSameFrameAsObjectsAreRegistered(); 
	scrThreadId GetCutSceneScriptThread()const { return m_ScriptThread;} 
	void OverrideScriptThreadId(scrThreadId ScriptId) { m_ScriptThread = ScriptId; }

	const strStreamingObjectName* GetSceneHashString() const { return &m_CutSceneHashString; }
	void SetSceneHashString(u32 hash) { m_CutSceneHashString.SetHash(hash); }

	void OverrideFadeOutAtStart(EScreenFadeOverride fade) { m_fadeOutGameAtBeginning = fade; }
	void OverrideFadeInAtStart(EScreenFadeOverride fade) { m_fadeInCutsceneAtBeginning = fade; }
	void OverrideFadeOutAtEnd(EScreenFadeOverride fade) { m_fadeOutCutsceneAtEnd = fade; }
	void OverrideFadeInAtEnd(EScreenFadeOverride fade) { m_fadeInGameAtEnd = fade; m_fadeInGameAtEndWhenStopping = fade; }
	void SetScriptHasOverridenFadeValues(bool OverridenFade) { m_HasScriptOverridenFadeValues = OverridenFade; }

	void SetCanVibratePadDuringCutScene(bool bCanVibrate) { m_bCanVibratePad = bCanVibrate; }
	bool CanVibratePadDuringCutScene() const { return m_bCanVibratePad; }

	void SetCarGenertorsUpdateDuringCutscene(bool bActive) { m_bAllowCargenToUpdate = bActive;  }
	bool CanCarGenertorsUpdateDuringCutscene() const { return m_bAllowCargenToUpdate; }
	
	void SetCanUseMobilePhoneDuringCutscene(bool bActive){ m_bCanUseMobilePhone = bActive; }
	bool CanUseMobilePhoneDuringCutscene() { return m_bCanUseMobilePhone; }

	void SetRepositionCoordAtSceneEnd(const Vector3 &vPos) { m_vPlayerPositionBeforeScene = vPos; }

	void SetCanBeSkipped(bool bCanSkip) { m_bCanSkipScene = bCanSkip; }

	bool IsSkipBlockedByCameraAnimTag() const;
	void SetSkippedBlockedByCameraAnimTagTime(float fAnimTagTime);

	//** SEAMLESS CUTSCENE TRIGGERING
	void SetSeamlessTriggerArea(const Vector3 &TriggerPoint, float fTriggerRadius, float fTriggerOrientation, float fTriggerAngle); //Allow the trigger area to be over ridden 
	void SetSeamlessTriggerOrigin(const Vector3 &TriggerOffset, bool bOverRide); 
	bool CanPedTriggerSCS(); 
	
	//** ASSET MANAGER ACCESSOR 
	CCutSceneAssetMgrEntity* GetAssetManager() { return m_pAssetManger; } 

	//** ANIM MANAGER ACCESSOR 
	CCutSceneAnimMgrEntity* GetAnimManager() { return m_pAnimManager; } 

	//** SEAMLESS CONTROL
	bool IsCutSceneSeamless() const { return m_bIsSeamless; }

	//** MAP OBJECT INTERACTION
	static bool GetClosestObjectCB(CEntity* pEntity, void* data); 
	static CEntity* GetEntityToAtPosition(const Vector3 &vPos, float fRadius, s32 iObjectModelIndex);
	void SetObjectInAreaVisibility(const Vector3 &vPos, float fRadius, s32 iModelIndex, bool bVisble); 
	void FixupRequestedObjects(const Vector3 &vPos, float fRadius, s32 iModelIndex);
 
	//Access to the camera entity
	const CCutSceneCameraEntity* GetCamEntity();

	bool HasCameraCutEarlyFromCutsceneInFirstPerson(); 


	// called by the anim manager when an anim dictionary first loads in
	void DictionaryLoadedCB(crClipDictionary* pDictionary, s32 section);
	
	void SetDeleteAllRegisteredEntites(bool DeleteRegisteredEntities) { m_bDeleteAllRegisteredEntites = DeleteRegisteredEntities; }
	bool ShouldDeleteAllRegisteredEntites() { return m_bDeleteAllRegisteredEntites; }
	
	bool ValidateEventTime(float EventTime); //check to see if these events are valid for this concat section

	float CalculateStreamingOffset(float Buffer, float CurrentTime) const;

	void SetHasCutThisFrame(bool bHasCutThisFrame) { m_bHasCutThisFrame = bHasCutThisFrame; }
	bool GetHasCutThisFrame() const { return m_bHasCutThisFrame; }

	void SetDisplayMiniMapThisUpdate(bool bDisplayMinimapThisUpdate) { m_bDisplayMiniMapThisUpdate = bDisplayMinimapThisUpdate; }
	bool GetDisplayMiniMapThisUpdate() const { return m_bDisplayMiniMapThisUpdate; }

	void SetAllowGameToPauseForStreaming(bool bAllow) { m_bAllowGameToPauseForStreaming = bAllow; }
	bool GetAllowGameToPauseForStreaming() const { return m_bAllowGameToPauseForStreaming; }

	bool GetShouldStopNow() const { return m_bShouldStopNow; }

	static int GetCurrentBufferIndex() { return (CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) ? m_RenderBufferIndex ^ 0x1 : m_RenderBufferIndex; }

	void SetShutDownMode(u32 mode) { m_ShutDownMode = mode; }
	u32 GetShutDownMode() { return m_ShutDownMode; }

	//script telling us the vehicle model the player will exit in
	void SetVehicleModelHashPlayerWillExitTheSceneIn(u32 vehicleModelHash) { m_VehicleModelThePlayerExitsTheSceneIn = vehicleModelHash; }
	u32 GetVehicleModelHashPlayerWillExitTheSceneIn() const { return m_VehicleModelThePlayerExitsTheSceneIn; }

	//fade
	//bool CanFadeOnSeamlessSkip() const { return m_bFadeOnSeamlessSkip; }
#if !__NO_OUTPUT
	static void CommonDebugStr(char * debugStr);
	u32 GetCutSceneCurrentFrame() const { return m_iCurrentFrame; }
	u32 m_iCurrentFrame;
#endif //!__NO_OUTPUT

#if __BANK
//	** DEBUG PUBLIC **		
	void InitLevelWidgets();	//called by game code to init widgets
	void ShutdownLevelWidgets(); ////called by game code to shutdown widgets
	void DebugDraw();
	u32 GetMBFrame();
	u32 GetRenderMBFrame();
	u32 m_uRenderMBFrame[2];

	void SetRenderMBFrame(bool enable) { m_bRenderMBFrameAndSceneName = enable; }
	
	const char* GetPlaybackFlagName(); 

	const CCutSceneDebugManager& GetDebugManager() const  { return m_DebugManager; }
	CCutSceneDebugManager& GetDebugManager()  { return m_DebugManager; }

	void RenderCutSceneTriggerArea(); //Called by the script commands
	void OutputMoveNetworkForEntities(); 
	bool WasStartedFromWidget() const { return m_bStartedFromWidget; }
	void PretendStartedFromWidget()		{ m_bStartedFromWidget = true; }
	bool m_RunSoakTest;
	int m_SoakTestPlaybackRateIdx;
	void RenderCutsceneLightsInfo();  
	void EnableAudioSyncing(); 
	void ShowOnlyActiveCutsceneLights(); 
	static bool m_displayActiveLightsOnly; 
	static bool m_bRenderCutsceneLights;
	static bool m_bRenderCutsceneStaticLight; 
	static bool m_bRenderAnimatedLights;

	void UpdateLightDebugging(); 
	virtual void ActivateCameraTrackingCB(); 
	virtual void SnapLightToCamera(); 
	virtual void SnapCameraToLight(); 

	void UpdateLightWithCamera(SEditCutfLightInfo *pEditLightInfo); 
	void UpdateCameraToLight(SEditCutfLightInfo *pEditLightInfo); 
	void SetLightToCamera(SEditCutfLightInfo *pEditLightInfo, const camFrame& frame);
	void SetExternalTimeStep(float timeStep) { m_ExternalTimeStep =  timeStep; }
	void SetUseExternalTimeStep(bool enable) { m_bUseExternalTimeStep = enable; }
	void SaveMaxLightXml(); 
	void SyncLightData(); 
	void CreateLightAuthoringMat(SEditCutfLightInfo *pEditLightInfo ); 
	bool ValidateLightObjects(const cutfLightObject* pFirstLight, const cutfLightObject* pSecondLight); 
	void ModifyMaxLightObject(cutfLightObject *pMaxLightObject, SEditCutfLightInfo *pEditLightInfo); 
	void AddMaxProperties(cutfLightObject *pMaxLightObject, SEditCutfLightInfo *pEditLightInfo); 
	virtual void BankLightSelectedCallback();

	virtual void CreateLightCB(); 
	virtual void DeleteLightCB(); 
	virtual void DuplicateLightCB(); 
	virtual void DuplicateLightFromCameraCB(); 
	virtual void RenameLightCB();
	virtual void UpdateActiveLightSelectionHistory(); 
	virtual void PopulateActiveLightList(bool forceRepopulateList = false);
	s32 GetActiveLightSelectionIndex(); 

	bool AddLightWithEvents(cutfLightObject* pLight, atHashString& camerHashName); 
	CAuthoringHelper m_Helper; 
	static void SetShouldPausePlaybackForAudioLoad(bool val) { m_bShouldPausePlaybackForAudioLoad = val; }
	static bool PushAssetFolder(const char *szAssetFolder);
	bool m_CanSaveLightAuthoringFile; 
	char m_lightSaveStatus[256]; 
	bkText* m_pSaveLightStatusText; 

	static const DebugRenderState& GetDebugRenderState() { return ms_DebugState; }

#endif
#if USE_MULTIHEAD_FADE
	static bool StartMultiheadFade(bool in, bool bInstant = false, bool bFullscreenMovie = true);
	static void SetBlinderDelay(int iLinger) { m_iBlinderDelay = iLinger; }
	static void SetManualBlinders(bool bManual) { m_bManualControl = bManual; }
	static bool GetManualBlinders() { return m_bManualControl; }
	static bool GetAreBlindersUp() { return m_bBlindersUp; }
#endif

	void CutsceneSmokeTest();

	void SetEnableReplayRecord(bool bEnable);
	bool GetEnableReplayRecord() const;
	bool IsReplayRecording() const { return m_bReplayRecording; }

#if GTA_REPLAY
	bool ReplayLoadCutFile();
	void ReplayCleanup();

	void SetReplayCutsceneCharacterLightParams(const cutfCameraCutCharacterLightParams &params) { m_ReplayCharacterLightParams = params; }
	const cutfCameraCutCharacterLightParams* GetReplayCutsceneCharacterLightParams() { return &m_ReplayCharacterLightParams; }

	void SetReplayCutsceneCameraArgs(const cutfCameraCutEventArgs & params) { m_ReplayCutsceneCameraArgs = params; }
	const cutfCameraCutEventArgs* GetReplayCutsceneCameraArgs() { return &m_ReplayCutsceneCameraArgs; }
#endif

#if __BANK
	bool IsCallStackLogging() { return m_LogUsingCallStacks; } 
#endif
protected:
		//u32 m_Frame;				//bank frame count used to set ped variation events 
		static int m_RenderBufferIndex;
		static sysCriticalSectionToken sm_CutsceneLock;
private:


	void AddScriptResource(scrThreadId ScriptId);

	void ValidateAudioLoadAndPlayEvents(); 


	void ComputeTimeStep (); // gets the last time step. 

	//branching: Is where the cutscene system is told which concat sections it should play
	

	bool ValidateEvent(s32 type, fwFlags32 StreamingFlags); 

	void UpdateEventsForInvalidSections(float NewTime);	

	void UpdateStreaming(float Buffer, s32 &LoadIndex, fwFlags32 EventFlags, float CurrentTime);

	void SyncLoadIndicesToStartTime(s32 &LoadIndex, fwFlags32 EventFlags, float CurrentTime); 

	bool CreateConcatSectionPlayBackList(s32 PlayBackList); //creates a list of 
	
	void SetStartAndEndTimeBasedOnPlayBackList(s32 PlayBackList); 

	void SetPlayerIsInScene(); 	//Sets if the player is in the scene.

	void TerminateLoadedOnlyScene(); //calls cleanup on a scene where only its assets have been loaded
	
	virtual void Clear(); //Clears all of the data as both an initialization and cleanup step

	//Base class override 
	//Associates all our "Entity" manager type classes with the objects that come from the data file. Some entities control multiple objects
	// and some are per object. This is the pseudo game/rage interface but is largely decided by the format of the data.
	virtual cutsEntity*	ReserveEntity(const cutfObject* pObject);	//associate our entity (delegate) with the object 
	virtual void ReleaseEntity(cutsEntity* pDelegate, const cutfObject* pObject); 

	void LoadCutSceneSectionMapCollision(bool bLoad); //load the collision for the cut scene

	//**SEAMLESS SKIPPING 
	
	bool	HaveScriptReservedAssetsBeenRequestedForSkip() const { return m_bRequestedScriptAssetsForEndOfScene; }
	void	RequestScriptReservedAssetsForSkip ();//set a flag that we have skipped a seamless cut scene

	void	SetIsSkipping(bool bSkipping) { m_bSkippingPlayback = bSkipping; } 

	void	UpdateSkip(); 
	float	CalculateSkipTargetTime(int Frame); //test to see if we can directly update the timer to what ever we want.
	
		//** TIMER
	void InitialiseCutSceneTimer();  //set the cut scene time to an initial value
	void UpdateCutSceneTimer(); //Update our cut scene timer
	void UpdateTime(); 
	float AdjustTimeForBlockingTags(float time);

		//** TRANSITION TO GAME STATE
	void ResetCutSceneVars(); //Resets the cut scene vars
	void SetGameToCutSceneState(); //Sets up the game state from game play to cut scene mode. ie no hud player control etc.
	void SetGameToCutSceneStatePostScene(); 
	void SetUpCutSceneData(); //Sets up all the vars for the restart of the game
	void CleanupTerminatedScriptCutSceneAssets(); 

	//void CheckAudioAssetsAreReadyThisFrame();
	bool AreAllAudioAssetsLoaded(); 

	void ForceFade();

	static void LetterBoxRenderingPrologue();
	static void LetterBoxRenderingEpilogue();

#if !__FINAL
	bool IsDLCCutscene(); 
#endif

private:
	
	int m_iScriptRefCount;

	atArray<s32> m_ObjectIdsReserevedForPostScene; 
	SeamlessSkip m_SeamlessSkipState; 
	CutSceneTrigger sSeamlessTrigger; //A struct that describes a trigger area for a seamless cut scene 
		
		//** SCENE NAME
	strStreamingObjectName m_CutSceneHashString; 
	Vector3 m_vPlayerPositionBeforeScene; 
	
	//double m_fLastFrameTime; //last time from synced clock

		//**INTERIOR 
	CInteriorProxy* m_pInteriorProxy; 	//pointer to an interior instance.
	CCutSceneAnimMgrEntity* m_pAnimManager; //Store a pointer to our anim manager
	CCutSceneAssetMgrEntity* m_pAssetManger; //game side cut scene objects
	static CutSceneManager* sm_pInstance;  //pointer to an instance of our cut scene manager

	float m_fTargetSkipTime; //The time to be skipped too.
	float m_fPreviousTime; //timestep last frame
	float m_fValidFrameStep; //store a valid time step need to apply for stepping
	float m_fFinalAudioPlayEventTime; 

	float m_fPlayTime;		// the actual play time of the scene (based on the concat sections we've chosen to play).
	float m_fDuration;		// The actual time the scene will play for.

	u32 m_EndFadeTime; // the time the camera fade should end. (if 0, no camera fade has been started)


	scrThreadId m_ScriptThread;

	u32 m_uFrameCountOfFirstRegisterCall; //frame number of the first register call
	u32 m_VehicleModelThePlayerExitsTheSceneIn; 

	s32 m_GameTimeMinutes; 
	s32 m_GameTimeHours; 
	s32 m_GameTimeSeconds; 
	s32 m_iPlayerObjectId;	
	
	s32 m_iNextAudioLoadEvent; 
	s32 m_ValidConcatSectionFlags; 
	fwFlags32 m_PlaybackFlags; 

	int m_SectionBeforeSkip; 

	fwFlags32 m_OptionFlags; // cutscene playback options (see eOptionFlags)
	
	bool m_bStreamObjectsWhileSceneIsPlaying; 
	bool m_bApplyTargetSkipTime; //Should a skip time be applied this frame 
	bool m_bFadeOnSeamlessSkip; 
	bool m_bRequestedScriptAssetsForEndOfScene; //Set a flag to indicate a seamless skip
	bool m_bAreScriptReservedEntitiesLoaded; 
	bool m_bIsSeamless; 	//Checks if the scene is seamless for loading 
	bool m_bIsSeamlessSkipping; //Checks we are skipping
	bool m_bCutsceneWasSkipped; 
	// bool m_bHaveValidTimeStep; 
	bool m_IsCutscenePlayingBack;
	bool m_bCameraWillCameraBlendBackToGame;
	bool m_ShouldEnablePlayerControlPostScene; 
	bool m_bIsCutSceneActive; //Set true when a cut scene starts and is used to reset the cut scene when it enters the idle state 
	bool m_bStreamedCutScene;
	bool m_bPreSceneUpdateEventCalled; //Sets a flag in the dispatch event to we should update in the post scene
	bool m_bPresceneLightUpdateEvent; 
	bool m_bPostSceneUpdateEventCalled;
	bool m_bIsPlayerInScene; 
	bool m_bSkipCallBackSetup; 
	bool m_bCanVibratePad; 
	bool m_bAllowCargenToUpdate; 
	bool m_bCanUseMobilePhone; 
	bool m_HasScriptOverridenFadeValues; 
	bool m_bCanSkipScene;
	float m_fSkipBlockedCameraAnimTagTime;
	bool m_CanSkipCutSceneInMultiplayerGame; 
	bool m_bHasScriptRegisteredAnEntity; 
	bool m_bShouldRestoreCutsceneAtEnd; 
	bool m_bShouldStopNow; 
	bool m_bDelayTerminatingAFrame;
	bool m_bWasDelayedTerminating; 
	static	bool m_bShouldPausePlaybackForAudioLoad; //made static as it needs to be referenced by a static render function.
	bool m_bCanStartCutscene; 
	bool m_bDeleteAllRegisteredEntites; 
	bool m_bCanScriptSetupEntitiesPreUpdateLoading; 
	bool m_bHasScriptSetupEntitiesPreUpdateLoadingFlagBeenSet; 
	bool m_bCanScriptChangeEntitiesModel; 
	bool m_bHasScriptChangeEntitiesModelFlagBeenSet; 
	bool m_bFailedToLoadBeforePlayWasRequested; 
	bool m_bWasSingledStepped; 
	// bool m_bResumingFromGamePauseBreaking; 
	bool m_IsResumingPlayBackFromSkipping; 
	bool m_bHasCutThisFrame; 
	bool m_bDisplayMiniMapThisUpdate;
	bool m_bAllowGameToPauseForStreaming;
	bool m_HasSetCutsceneToGameStatePostScene; 
	bool m_bUpdateCutsceneTimer; 
	bool m_bEnableReplayRecord;
	bool m_bReplayRecording;
	bool m_bHasProccessedEarlyCutEventForSinglePlayerExits; 
	bool m_bShouldProcessEarlyCameraCutEventsForSinglePlayerExits;

	u32 m_ShutDownMode; 

	static grcRasterizerStateHandle		ms_LetterBoxPrevRSHandle;
	static grcDepthStencilStateHandle	ms_LetterBoxPrevDSSHandle;
	static grcBlendStateHandle			ms_LetterBoxPrevBSHandle;

#if __BANK
	static DebugRenderState ms_DebugState;
#endif

private:

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
	static bool m_RenderUnauthorizedScreen; 
	static u32 m_RenderUnauthorizedScreenTimer; 
	static bool m_RenderUnauthorizedWaterMark; 
	static bool m_IsAuthorizedForPlayback; 

	void LoadAuthorizedCutsceneList(); 
	void UpdateAuthorization(); 
	AuthorizedCutscene m_AuthorizedCutscenes; 
#endif

#if USE_MULTIHEAD_FADE
	static bool m_bBlindersUp;
	static bool m_bManualControl;
	static int m_iBlinderDelay;
#endif // USE_MULTIHEAD_FADE

#if GTA_REPLAY
	//In replay we dont have a cutscene camera so store these values here;
	cutfCameraCutCharacterLightParams m_ReplayCharacterLightParams;

	//same from the event args
	cutfCameraCutEventArgs m_ReplayCutsceneCameraArgs;
#endif

//	** DEBUG PRIVATE **
#if __BANK
	static void UpdateDebugRenderState();

	void OverrideGameTime(); 

	void SoakTest(); 

	//** WIDGETS  
	void ActivateBank(); //Set up widgets
	void DeactivateBank(); 
	
	CCutSceneDebugManager m_DebugManager;//store the debug manager class 
	void ScrubBackwardsCB();
	void ScrubForwardsCB();
	virtual void SetScrubbingControl(u32 CurrentFrame); 

	void SelectNewObjCb();
	void RefreshObjList();
	void CaptureSceneToObj();
    void SnapCamToSceneOrigin();
	void GenerateCapsule(Mat34V_In mtxIn, phBoundCapsule* capsule, FileHandle& objFile, u32& nextIndex, const Vector3& translation, float rotation, float scale);
	void CaptureDrawable(CEntity* entity, rmcDrawable* drawable, FileHandle& objFile, FileHandle& pivotFile, Matrix34& entityMatrix, u32& nextIndex, Matrix43* ms, const Vector3& euler);

	static const int ms_AssetFoldersMax = 16;
	static int ms_AssetFoldersCount;
	static const char* ms_AssetFolders[ms_AssetFoldersMax]; 
	static void DeleteAllAssetFolders();
	const char *FindAssetFolderForCutfile(const char *sceneName);
	void SaveCutfile(); //Save the tune file to the correct place
	void SaveGameDataCutfile(); 
	virtual void SaveDrawDistanceFromWidget(); 

	//	**HIDDEN OBJECTS 
	virtual SEditCutfObjectLocationInfo*	GetHiddenObjectInfo();
	virtual SEditCutfObjectLocationInfo*	GetFixupObjectInfo();
	virtual sEditCutfBlockingBoundsInfo*	GetBlockingBoundObjectInfo(); 
	
	//	**SCENE PROPERTIES
	virtual void DrawSceneOrigin(); //Render the scene origin
	
	//	** WIDGETS  
	void StartEndCutsceneButton(); //Call back function for starting a cutscene
	void OnSearchTextChanged(); 

	void DumpCutFile(); 

	//Frames selection
	virtual void SetFrameRangeForPlayBackSliders();  //Set the frame range for the slider so that the event animator work in frames  not time

	void LoadApprovedList();
	void IsSceneApproved(); 
	void CheckForCutsceneExistanceUsingApprovedList();
	void ValidateCutscene();
	static void RenderCameraInfo();
	static void RenderUnapprovedSceneInfoCallback(bool FinalApproved, bool AnimationApproved, bool CameraApproved, bool FacialApproved, bool LightingApproved, bool DOFapproved); 
	static void RenderStreamingPausedForAudio(bool AudioPaused);
	static void RenderContentRestrictedData(bool bRestictedBuild);

	cutfCutsceneFile2* m_pDataLightXml; 

	ApprovedCutsceneList m_ApprovedCutsceneList;
	atArray<const char*> m_BankFileNames;			//name of cut scenes
	char m_searchText[260];
	bkCombo* m_sceneNameCombo; 

	char m_capturePath[128];
	atString cOrignalSceneName;
	
	bkSlider* m_pCaptureRadiusSlider;
	bkCombo* m_pObjsCombo;
	fwDebugBank *m_pCutSceneBank;				// widget bank		
	bkSlider* m_pSkipToFrameSlider; 
	
	float m_ExternalTimeStep;
	u32 m_iExternalFrame;

	u32 m_iCaptureRadius;
	u32 m_iSkipToFrame;  

	s32 m_selectedObj;
	s32 m_BankFileNameSelected;		//index into the cutscene 

	bool m_bStartedFromWidget; 
	bool m_captureMap;
	bool m_captureProps;
	bool m_captureVehicles;
	bool m_capturePeds;
	bool m_useSceneOrigin;
	bool m_showCaptureSphere;
	bool m_capturePedCapsules;
	bool m_bRenderCutSceneBorders;	//render borders	
	bool m_bRenderMBFrameAndSceneName;
	bool m_bVerboseExitStateLogging;
	bool m_bUseExternalTimeStep; 
	bool m_LogUsingCallStacks; 
	bool m_AllowScrubbingToZero; 
	bool m_bSnapCameraToLights; 

	float m_fCurrentAudioTime;
	float m_fLastAudioTime;

	static bool m_bIsFinalApproved;
	static bool m_bIsCutsceneAnimationApproved;
	static bool m_bIsCutsceneCameraApproved;
	static bool m_bIsCutsceneFacialApproved;
	static bool m_bIsCutsceneLightingApproved; 
	static bool m_bIsCutsceneDOFApproved;
	static float m_fApprovalMessagesOnScreenTimer;
	static bool m_IsDlcScene; 

#endif	//end __BANK
};

class CutSceneManagerWrapper
{
public:

    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void PreSceneUpdate();
    static void PostSceneUpdate();
};





///////////////////////////////////////////////////////////////////////////////////////////////////

#endif
