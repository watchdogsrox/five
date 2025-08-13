/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : A manager entity type class for managing both cutscene objects(special objects that are created by the cut scene system)
//			 and game side objects (objects that are created by the game e.g scripts, ped population). The idea of a cutscene object 
//			 will be replaced hopefully so that a ped is a ped and not an object that looks like a ped. 
//			 These entity classed interface with these object based on the events passed by the cuts manager through Dispatch_event.
// PURPOSE : Updates anims based on messages passed passed to it by the cut scene anim manager.
// AUTHOR  : Thomas French
// STARTED : 27/07/2008
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_ANIMENTITY_H 
#define CUTSCENE_ANIMENTITY_H

//Rage Headers
#include "cutscene/cutsanimentity.h"
#include "cutscene/cutsentity.h"
#include "paging/ref.h"

//Game Headers
#include "Peds/Ped.h"
#include "Cutscene/CutSceneDefine.h"
#include "Cutscene/cutscene_channel.h"

#if !__NO_OUTPUT
#define cutsceneModelEntityErrorf(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_ERROR) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_ERROR)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneErrorf("%s" fmt,debugStr,##__VA_ARGS__); }
#define cutsceneModelEntityWarningf(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_WARNING) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_WARNING)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneWarningf("%s" fmt,debugStr,##__VA_ARGS__); }
#define cutsceneModelEntityDisplayf(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DISPLAY)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneDisplayf("%s" fmt,debugStr,##__VA_ARGS__); }
#define cutsceneModelEntityDebugf1(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG1) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG1)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#define cutsceneModelEntityDebugf2(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG2)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneDebugf2("%s" fmt,debugStr,##__VA_ARGS__); }
#define cutsceneModelEntityDebugf3(fmt,...) if ( ((Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG3)) && GetCutfObject() ) { char debugStr[512]; CCutsceneAnimatedModelEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneDebugf3("%s" fmt,debugStr,##__VA_ARGS__); }

#else
#define cutsceneModelEntityErrorf(fmt,...) do {} while(false)
#define cutsceneModelEntityWarningf(fmt,...) do {} while(false)
#define cutsceneModelEntityDisplayf(fmt,...) do {} while(false)
#define cutsceneModelEntityDebugf1(fmt,...) do {} while(false)
#define cutsceneModelEntityDebugf2(fmt,...) do {} while(false)
#define cutsceneModelEntityDebugf3(fmt,...) do {} while(false)

#endif //!__NO_OUTPUT

class CCompEntity; 
class CutSceneManager;
class CTaskAnim; 
class CTaskCutScene;

struct sRegisteredGameEntities
{
	//possibly register the name of the cut scene here so that we can do some type of safety checks.
	atHashString SceneNameHash;
	atHashString ModelNameHash; 
	RegdDyn pEnt;
	bool  bDeleteBeforeEnd;				// the ped will be deleted before the scene ends
	bool  bCreatedForScript;	        // The ped is needed at the end of the scene but will be created by the cutscene
	float iEnterStatePhase;				//What phase in the scene that the ped can first be altered by script.
	float iExitStatePhase;				//What phase in the scene that the ped can be altered by script on exit.
	s32 iSceneId; 						//Need to reserve the entities if teh scene is skipped 
	bool bAppearInScene; 				//The ped has died and should not be displayed in the scene.

	sRegisteredGameEntities()
	{
		iSceneId = -1;					// this is invalid because all ids start from 0. 	
		SceneNameHash = atHashString(); 
		ModelNameHash = atHashString(); 
		pEnt = NULL;
		bDeleteBeforeEnd = true; 
		bCreatedForScript = false;
		iEnterStatePhase = 0.0f; 
		iExitStatePhase = 1.0f;
		bAppearInScene = true;
	}
};

enum CCutsceneCustomClothEventType
{
	CCEVENT_TYPE_ATTACH_COLLISION			= 1,
	CCEVENT_TYPE_DETACH_COLLISION			= 2,
	CCEVENT_TYPE_SET_SKIN_ON				= 3,
	CCEVENT_TYPE_SET_SKIN_OFF				= 4,
	CCEVENT_TYPE_FORCE_PIN					= 5,
	CCEVENT_TYPE_SET_PACKAGE_INDEX			= 6,
	CCEVENT_TYPE_SET_SKIN_ON_ONCREATE		= 7,
	CCEVENT_TYPE_SET_SKIN_OFF_ONCREATE		= 8,
	CCEVENT_TYPE_SET_PRONE_ON				= 9,
	CCEVENT_TYPE_SET_PRONE_OFF				= 10,
	CCEVENT_TYPE_SET_PIN_RADIUS_THRESHOLD	= 11,
	CCEVENT_TYPE_SET_POSE					= 12,
	CCEVENT_TYPE_QUEUE_POSE_ON				= 13,
	CCEVENT_TYPE_QUEUE_POSE_OFF				= 14,
	CCEVENT_TYPE_CUSTOM_0					= 20,
	CCEVENT_TYPE_CUSTOM_1					= 21,
	CCEVENT_TYPE_MAX						= 30,
};

#define SIZE_OF_SCRIPT_VISIBLE_TAG_LIST    3

//Base class for an animated cut scene objects. 
class CCutsceneAnimatedModelEntity : public cutsUniqueEntity
{
public: 

	enum EntityOptionFlags // keep in sync with CUTSCENE_ENTITY_OPTION_FLAGS in commands_cutscene.sch
	{
		CEO_PRESERVE_FACE_BLOOD_DAMAGE = BIT(0),	// If specified, leave facial blood decals when the cutscene starts. These are removed by default.
		CEO_PRESERVE_BODY_BLOOD_DAMAGE = BIT(1),	// If specified, leave body damage decals when the cutscene starts. By default these are reduced, but not completely removed
		CEO_REMOVE_BODY_BLOOD_DAMAGE = BIT(2),		// If specified, body damage decals will be completely cleared (instead of just reduced) when the cutscene starts
		CEO_CLONE_DAMAGE_TO_CS_MODEL = BIT(3),		// If specified, if you have a reposition only entity, then any damage is cloned from that to the animated entity
		CEO_RESET_CAPSULE_AT_END = BIT(4),			// If specified, the character's capsule will be reset so that it is vertical at the end of cutscenes.
		CEO_IS_CASCADE_SHADOW_FOCUS_ENTITY_DURING_EXIT = BIT(5),	// If specified, this entity will be used as the focus for the cascade shadow system during seamless exits.  Required when characters get into vehicles at the end of cutscenes.
		CEO_IGNORE_MODEL_NAME = BIT(6),	// MP only - If specified, animated this registered ped in the scene, even if its model name doesn't match the original animated model
		CEO_PRESERVE_HAIR_SCALE = BIT(7),			// If specified, the characters hair scale is frozen when the cutscene starts.
		CEO_INSTANT_HAIR_SCALE_SETUP = BIT(8),				// If specified, the characters hair scale will snap to it's new value when the cutscene starts, otherwise it will lerp.
		CEO_DONT_RESET_PED_CAPSULE = BIT(9),
		CEO_UPDATE_AS_REAL_DOOR	= BIT(10)
	};
	
	enum EntityStreamingOptionFlags
	{
		CESO_DONT_STREAM_AND_APPLY_VARIATIONS = BIT(0)
	}; 

	CCutsceneAnimatedModelEntity(const cutfObject* pObject);

	~CCutsceneAnimatedModelEntity();

	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);	
	
	//get a game object for a seamless cut scene. 
	CEntity* GetGameEntity() { return m_pEntity; }
	
	const CEntity* GetGameEntity() const { return m_pEntity; }
	
	//get the game object the game side object to be repositioned not animated
	CEntity* GetGameRepositionOnlyEntity() { return m_pEntityForRepositionOnly; }

	const CEntity* GetGameRepositionOnlyEntity() const { return m_pEntityForRepositionOnly; }

	void SetOptionFlags(u32 flags) { m_OptionFlags.SetAllFlags(flags); }
	s32 GetOptionFlags() const { return m_OptionFlags.GetAllFlags(); }
	
	void SetStreamingOptionFlags(u32 flags) { m_StreamingOptionFlags.SetAllFlags(flags); }
	s32 GetStreamingOptionFlags() const { return m_StreamingOptionFlags.GetAllFlags(); }
	
	bool IsBlockingVariationStreamingAndApplication() { return m_StreamingOptionFlags.IsFlagSet(CESO_DONT_STREAM_AND_APPLY_VARIATIONS); }

	//Gets a pointer to the object as defined in cutfile.
	const cutfObject* GetCutfObject() const { return m_pCutfObject; }

	//Get the hash of the scene name
	atHashString& GetSceneHandleHash() { return m_SceneHandleHash; } 
	
	//hash of the model name
	atHashString& GetModelNameHash() { return m_ModelNameHash; }

	//Get the time the start anim event was supposed to be dispatched as defined in the cutfile, not the actual time it actually triggered.
	float GetAnimEventStartTime() const { return m_fStartTime; } 

	Vector3 GetAnimatedVelocity() const { return m_vVelocity; } 

	//Set the game entity ready for game transition. 
	virtual void SetGameEntityReadyForGame(cutsManager* pManager) = 0; 
	
	bool IsGameEntityReadyForGame() const { return m_bIsReadyForGame; }

	bool HasStoppedBeenCalled() const { return m_bStoppedCalled; }

	bool IsRegisteredGameEntityUnderScriptControl() const { return m_bIsRegisteredEntityScriptControlled; }

	void SetRegisteredGameEntityIsUnderScriptControl(bool ScriptControlled) { m_bIsRegisteredEntityScriptControlled = ScriptControlled; }

	//hold info abou a script registered entity
	sRegisteredGameEntities m_RegisteredEntityFromScript; 

	bool IsRegisteredWithScript() const { return m_RegisteredEntityFromScript.SceneNameHash > 0; } 

	virtual bool IsScriptRegisteredGameEntity()
	{
		return m_OptionFlags.IsFlagSet(CEO_IGNORE_MODEL_NAME) ? true : (m_RegisteredEntityFromScript.ModelNameHash.GetHash() == GetModelNameHash().GetHash() && (m_RegisteredEntityFromScript.SceneNameHash.GetHash()>0 && (m_RegisteredEntityFromScript.SceneNameHash.GetHash() == GetSceneHandleHash().GetHash()))); 
	}
	
	bool IsScriptRegisteredRepositionOnlyEntity()
	{ 
		return m_OptionFlags.IsFlagSet(CEO_IGNORE_MODEL_NAME) ? false : (m_RegisteredEntityFromScript.SceneNameHash.GetHash() > 0 && m_RegisteredEntityFromScript.SceneNameHash.GetHash() == GetSceneHandleHash().GetHash()); 
	}

	bool HasScriptVisibleTagPassed(s32 EventHash); 

	virtual void ForceUpdateAi(cutsManager* UNUSED_PARAM(pManager)) { };

	const char* GetModelName() const { 
#if !__NO_OUTPUT
		return m_pEntity ? m_pEntity->GetModelName() : "Unknown"; 
#else
		return "Unknown"; 
#endif // !__NO_OUTPUT
	}

	//Accessors for network syncing so can sync the associated entities actual current state to remote machines
	bool GetWasInvincible() const {return m_bWasInvincible;}
	void SetWasInvincible(bool bWasInvincible) {m_bWasInvincible = bWasInvincible;}

protected:
	
#if !__NO_OUTPUT
	void SetUpEntityForCallStackLogging(); 
	void ResetEntityForCallStackLogging();
#endif

	CTaskCutScene* CreateCutsceneTask(CutSceneManager* pManager, const crClip* pClip, float fEventTime); 
	
	virtual void PlayClip (CutSceneManager* pManager, const crClip* pClip, float fEventTime, const strStreamingObjectName pAnimDict) = 0; 

	//Set the time the event was dispatched as defined in the cutfile. 
	void SetAnimPlayBackEventTime(float fEventTime); 

	void GetMoverTrackVelocity(const crClip* pClip, float fCurrentPhase, float fLastPhase, float fTimeStep); 
	
	//Store the pointer to the game object
	void SetGameEntity(CDynamicEntity * pEnt) { m_pEntity = pEnt; }
	
	void SetRepositionOnlyEntity(CDynamicEntity * pEnt) { m_pEntityForRepositionOnly = pEnt; }

	void UpdateCutSceneTaskPhase(CTask* pTask, cutsManager* pManager);

	//Create a actual game object not a cut scene special object.
	virtual void CreateGameEntity(strLocalIndex ModelIndex, bool CreateRepositionEntity = false, bool bRequestVariations = false, bool bApplyVariations = false) = 0; 
	
	//Store the properties if the object is streamed out and streamed in again
	virtual void RestoreEntityProperties(CEntity * pEnt) = 0;
	
	//Set the game entity ready for the cutscene state
	virtual void SetGameEntityReadyForCutscene() = 0; 
	
	virtual void SetRepositionOnlyEntityReadyForCutscene() = 0; 

	virtual bool RemoveEntityFromTheWorld(CEntity* pEntity, bool bDeleteScriptRegistered = false); 

	bool ShouldUpdateCutSceneTask(CTask* pTask, cutsManager* pManager); 

	//Position the entity was created 
	Vector3 m_vCreatePos; 

	//pointer to the clip passed by anim system
	pgRef<const crClip> m_pClip;

	const cutfObject* m_pCutfObject; 

	taskRegdRef<CTaskCutScene>  m_pCutSceneTask;

	fwFlags32 m_OptionFlags;
	fwFlags32 m_StreamingOptionFlags;  

	float m_fOriginalLODMultiplier;	
	float m_fOriginalHairScale;
	Vector3 m_CurrentPosition; 
	float m_CurrentHeading;
	phBoundComposite* m_CollisionBound;
	bool m_bUpdateCollisionBound;	
	bool m_bWasInvincible; 
	bool m_bIsReadyForGame; 
	bool m_bStoppedCalled; 
	bool m_bIsRegisteredEntityScriptControlled; 
	bool m_bCreatedByCutscene; 

	bool m_bComputeInitialVariationsForGameEntity;
	bool m_bComputeVariationsForSkipForRepositionOnlyEntity;
	bool m_bRequestRepositionOnlyEntity;

	static const fwMvPropertyId ms_VisibleToScriptKey;
	static u32 ms_EventKey;

#if __DEV
	virtual void DebugDraw() const; 
#endif 

#if !__NO_OUTPUT
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif //!__NO_OUTPUT

private:
	void RegisterPlayer(cutsManager* pManager, const cutfObject* pObject); 

	void DeleteNonRegisteredEntities(); 

	//Set up entities registered with the scripts. 
	void SetUpScriptRegisteredEntity(cutsManager* pManager, const cutfObject* pObject);
	
	void CreateAnimatedEntityWhenModelLoaded(cutsManager* pManager, const cutfObject* pObject, bool bApplyVariations, bool bRequestVariations); 

	void RequestRepositionOnlyEntityModelWhenNeeded(CutSceneManager* pManager, const cutfObject* pObject);



protected:
	void CreateRepositionOnlyGameEntityWhenModelLoaded(cutsManager* pManager, const cutfObject* pObject, bool bApplyVariations, bool bRequestVariations); 

	CTaskCutScene* GetCutsceneTaskForEntity(); 
	
	void ComputeScriptVisibleTagsOnAnimClear(CutSceneManager* pCutManager); 

	void ComputeScriptVisibleTagsOnAnimSet(CutSceneManager* pCutManager); 

	void ComputeScriptVisibleTagsOnUpdate(CutSceneManager* pCutManager); 

	void FindScriptVisibleTags(const crClip* pClip, float startPhase, float endPhase); 
	
	void ResetScriptVisibleTags(); 
	
	void SetVisibility(CEntity* pEnt, bool Visible); 
	
	atHashString m_ModelNameHash; 

private:
	Vector3 m_vVelocity; 

	//The scene name hashed so that when we create we can register our object.
	atHashString m_SceneHandleHash;

	float m_fStartTime; 	//The time the anim event was dispatched defined in the cutfile

	float m_ExitSceneTime;	// the time this entity is due to exit the scene
	
	//Pointer to a game entity we grab from the world
	RegdDyn m_pEntity;

	RegdDyn m_pEntityForRepositionOnly; 


	//bool m_bScriptWillSetEntityPostCutscene; 

	//Keep a record of the anim dict for later on if the object is not used 
	char m_AnimDict[32]; 

	s32 m_SectionAnimWasRecieved; 

	//flag to check if we have deleted it
	bool m_HasBeenDeletedByCutscene; 

	//store that we need to create our game ped but lets not do it when create our entity
	bool m_bCreateGameObject; 
	bool m_bClearedOrSetAnimThisFrame; 
	bool m_bShouldProcessTags; 

	s32 m_ScriptVisibleTags[SIZE_OF_SCRIPT_VISIBLE_TAG_LIST]; 

	u32 m_FrameTagsAreProcessed; 

#if !__NO_OUTPUT
	bool m_bIsloggingEntityCalls; 
#endif //!__NO_OUTPUT
};

////////////////////////////////////////////////////////////////////

//An entity class that manages a real game object or a cutscene object by passing it events from the cuts manager

class CCutSceneAnimatedPropEntity : public CCutsceneAnimatedModelEntity
{
public:
	CCutSceneAnimatedPropEntity(const cutfObject* pObject);

	~CCutSceneAnimatedPropEntity();

	s32 GetType() const { return CUTSCENE_PROP_GAME_ENITITY; } 
	
	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);	
	
	//Get a pointer to a real game object in this case a Cobject
	const CObject* GetGameEntity() const { return static_cast<const CObject*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }
	
	CObject* GetGameEntity() { return static_cast<CObject*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }

	//Get a pointer to a real game object in this case a Cobject
	const CObject* GetGameRepositionOnlyEntity() const { return static_cast<const CObject*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	CObject* GetGameRepositionOnlyEntity() { return static_cast<CObject*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	virtual void ForceUpdateAi(cutsManager* pManager);


protected:
	virtual void RestoreEntityProperties(CEntity * UNUSED_PARAM(pEnt)) {};
	
	//Set the game entity ready for the cutscene state
	virtual void SetGameEntityReadyForCutscene(); 

	virtual void SetRepositionOnlyEntityReadyForCutscene();

	//Set the game entity ready for game transition. 
	virtual void SetGameEntityReadyForGame(cutsManager* pManager); 

	virtual void PlayClip (CutSceneManager* pManager, const crClip* pClip, float fEventTime, const strStreamingObjectName pAnimDict); 
	
	//Create a game version of the object. 
	void CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity = false, bool bRequestVariations = false, bool bApplyVariations = false);

	void UpdateCutSceneTask(cutsManager* pManager); 

	int m_iAnimDict; 
};

class CCutSceneAnimatedWeaponEntity : public CCutSceneAnimatedPropEntity
{
public:
	CCutSceneAnimatedWeaponEntity(const cutfObject* pObject);

	~CCutSceneAnimatedWeaponEntity();

	s32 GetType() const { return CUTSCENE_WEAPON_GAME_ENTITY; } 

	void SetRegisteredGenericWeaponType(u32 GenericType) { m_RegisteredGenericWeaponType = GenericType; }
	
	u32 GetCutObjectGenericWeaponType () const; 

	virtual bool IsScriptRegisteredGameEntity();

	//Set the game entity ready for the cutscene state
	virtual void SetGameEntityReadyForCutscene();

private:
	u32 m_RegisteredGenericWeaponType; 
};

////////////////////////////////////////////////////////////////////

struct sVehicleData
{	
	sVehicleData()
		:iBodyColour(0)
		,iSecondColour(0)
		,iSpecColour(0)
		,iWheelColour(0)
		,iBodyColour5(0)
		,iBodyColour6(0)
		,iLiveryId(-1)
		,iLivery2Id(-1)
		,fDirt(0.0f)
	{

	}

	int iBodyColour; 
	int iSecondColour; 
	int iSpecColour; 
	int iWheelColour; 
	int iBodyColour5;
	int iBodyColour6;
	int iLiveryId; 
	int iLivery2Id; 
	float fDirt; 
	atArray<s32> HiddenBones;
};

////////////////////////////////////////////////////////////////////
//An entity class that manages a real game vehicle or a cutscene vehicle by passing it events from the cuts manager

class CCutsceneAnimatedVehicleEntity : public CCutsceneAnimatedModelEntity
{
public:
	CCutsceneAnimatedVehicleEntity(const cutfObject* pObject); 

	~CCutsceneAnimatedVehicleEntity();

	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);
	
	s32 GetType() const { return CUTSCENE_VEHICLE_GAME_ENITITY; }
	
	virtual void PlayClip (CutSceneManager* pManager, const crClip* pClip, float fEventTime, const strStreamingObjectName pAnimDict ); 

	const CVehicle* GetGameEntity() const { return static_cast<const CVehicle*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }
	
	CVehicle* GetGameEntity() { return static_cast<CVehicle*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }
	
	const CVehicle* GetGameRepositionOnlyEntity() const { return static_cast<const CVehicle*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	CVehicle* GetGameRepositionOnlyEntity() { return static_cast<CVehicle*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	void SetVehicleDamage(const Vector3& scrVecDamage, float damageAmount, float deformationScale, bool localDamage); 

#if __BANK
	void SetBoneToHide(s32 iBone, bool bHide); 

	const atArray<s32>& GetHiddenBoneList() { return m_HiddenBones; }
#endif
	
	void SetGameVehicleExtra(const atArray<s32>& Bones); //apply variation to a game side vehicle
	
	virtual bool IsScriptRegisteredGameEntity() 
	{ 
		return m_RegisteredEntityFromScript.SceneNameHash.GetHash()>0 && (m_RegisteredEntityFromScript.SceneNameHash.GetHash() == GetSceneHandleHash().GetHash()); 
	}

	bool IsRepositionOnlyOnSkip()
	{
		return IsScriptRegisteredGameEntity() && IsScriptRegisteredRepositionOnlyEntity() && !m_RegisteredEntityFromScript.pEnt	&& m_RegisteredEntityFromScript.bCreatedForScript;
	}

private: 
	
	void UpdateCutSceneTask(cutsManager* pManager);

	//Set the game entity ready for the cutscene state
	virtual void SetGameEntityReadyForCutscene(); 

	virtual void SetRepositionOnlyEntityReadyForCutscene(); 

	//Set the game entity ready for game transition. 
	virtual void SetGameEntityReadyForGame(cutsManager* pManager); 

	//Setup the variation on a vehicle using the last events passed to it
	virtual void RestoreEntityProperties(CEntity * pEnt);

	//Get an existing game vehicle
	void SetVariation(int iBodyColour, int iSecondColour, int iSpecColour, int iWheelColour, int iBodyColour5, int iBodyColour6, int LiveryId, int Livery2Id, float fDirt); 	

	void SetupSuspension();

	void CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity = false, bool bRequestVariations = false, bool bApplyVariations = false); 
	
	void HideVehicleFragmentComponent(); 

	virtual void ForceUpdateAi(cutsManager* pManager);

	s32 m_iAnimDict;  

	sVehicleData m_sVehicleData;

#if __BANK
	
	atArray<s32> m_HiddenBones;	//List of hidden bones
	
	void InitaliseVehicleExtraList(); //if debugging a real vehicle display any extras switched off before the scene starts. 
#endif

};

////////////////////////////////////////////////////////////////////
struct sPedComponentVar
 {
	u32 iComponent; 
	s32 iDrawable; 
	s32 iTexture; 
	float EventTime; 
};



//An entity class that manages a real game ped or a cutscene ped by passing it events from the cuts manager
struct sActorVariationData
{	
	atVector<sPedComponentVar> iPedVarData; 
};

class CCutsceneAnimatedActorEntity : public CCutsceneAnimatedModelEntity
{
public:	
	CCutsceneAnimatedActorEntity(const cutfObject* pObject);

	~CCutsceneAnimatedActorEntity();

	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);	

	//This over rides the base implementation of this to allow dual anims (face and body to be played on a ped)
	virtual void PlayClip (CutSceneManager* pManager, const crClip* pClip, float fEventTime, const strStreamingObjectName UNUSED_PARAM (pAnimDict) ); 
	
	bool GetActorBonePosition(eAnimBoneTag ePedBone, Matrix34& mMat) const; 
	
	//Set the variation in the current variations array (and on the actual ped if valid)
	void SetCurrentActorVariation(CPed* pPed, u32 iComponent, s32 iDrawable, s32 iTexture ); 

	//Set the variation in the initial variations array
	void SetScriptActorVariationData(u32 iComponent, s32 iDrawable, s32 iTexture); 

	//Get an existing game ped
	CPed* GetGameEntity() { return static_cast<CPed*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }
	
	const CPed* GetGameEntity() const { return static_cast<const CPed*>(CCutsceneAnimatedModelEntity::GetGameEntity()); }

	CPed* GetGameRepositionOnlyEntity() { return static_cast<CPed*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	const CPed* GetGameRepositionOnlyEntity() const { return static_cast<const CPed*>(CCutsceneAnimatedModelEntity::GetGameRepositionOnlyEntity()); }

	//for type checking and pointer casting
	s32 GetType()const { return CUTSCENE_ACTOR_GAME_ENITITY; }
	
	void CreateGamePed(u32 iModelIndex, bool CreateRepositionEntity);
	
	//Set up the ped and set him into the game world
	void InitActor(CPed* pPed);	
	
	void ComputeInitialVariations(atHashString& modelHashString); 

	void ComputeVariationsForSkip(atHashString& modelHashString); 

	void PreStreamVariations(CutSceneManager* pCutSceneManager, float CurrentTime, float LookAhead);
	


private:

#if __BANK
	void UpdateWithFaceViewer(CutSceneManager* pManager, CTaskCutScene* pCutSceneTask); 
#endif

	void AddActorToWorld(CPed* pPed); 

	void UpdateCutSceneTask(cutsManager* pManager);

	virtual void ForceUpdateAi(cutsManager* pManager);

	virtual void CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity = false, bool bRequestVariations = false, bool bApplyVariations = false); 
	
	virtual void RestoreEntityProperties(CEntity * pEnt);

	virtual bool RemoveEntityFromTheWorld(CEntity* pEntity, bool bDeleteRegistered = false); 

	//Set the game entity ready for the cut scene state
	virtual void SetGameEntityReadyForCutscene(); 

	virtual void SetRepositionOnlyEntityReadyForCutscene(); 

	//Set the game entity ready for game transition. 
	virtual void SetGameEntityReadyForGame(cutsManager* pManager); 
	
	sActorVariationData m_sCurrentActorVariationData;
	
	sActorVariationData m_sScriptActorVariationData; 

	void StoreActorVariationData(u32 iComponent, s32 iDrawable, s32 iTexture, sActorVariationData& ActorVariationData ); 

	//Update the current move blender 
	void UpdateActorMotionState(); 

	void GetEntityVariationEvents(); 

	atVector<cutfEvent*> m_pVariationEvents; 
	u32 m_VariationStreamingIndex; 

	// Vehicle lighting
	// 0.0 = outside vehicle, 1.0 = in vehicle.  We update this during the task.
	float m_fVehicleLightingScalar;
	float m_fVehicleLightingScalarTargetValue;
	float m_fVehicleLightingScalarBlendRate;
};

class CCutSceneRayFireEntity : public cutsUniqueEntity
{
public:
	CCutSceneRayFireEntity(const cutfObject* pObject);
	
	~CCutSceneRayFireEntity(); 

	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);	

private: 
	s32 m_iRayFireId; 
	float m_fEventTime; 	
}; 

#endif
