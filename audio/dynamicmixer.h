//
// audio/dynamicmixer.h
//
// Copyright (C) 1999-2010 Rockstar Games. All rights reserved.
//

#ifndef AUD_DYNAMICMIXER_H
#define AUD_DYNAMICMIXER_H

//////////////////////////////////////////////////////////////////////
// Includes

#include "gameobjects.h"

#include "audioentity.h"

#include "atl/pool.h"
#include "atl/slist.h"
#include "audioengine/dynamicmixmgr.h"   
#include "audioengine/curve.h"
#include "audioengine/enginedefs.h"
#include "audioengine/dynamixdefs.h"
#include "audioengine/metadatamanager.h"
#include "math/amath.h"
#include "system/criticalsection.h"


#if __BANK
#include "data/base.h"

#define AUD_BOOL2STR(x) x ? "True" : "False"

enum AuditionSceneDrawMode 
{
	kNothing = 0,
	kEverything, 
	kScenesOnly,
	kVariables,
	kPatches,
	kCategories,
	kNumDrawModes
};

class audNorthDebugDrawManager;

#endif
/////////////////////////////////////////////////////////////////////
   
////////////////////////////////`/////////////////////////////////////
// Forward declarations
namespace rage
{
	class audWaveSlot;
	class audCategoryController;
	class audSoundSlot;
	class bkBank;
	class bkGroup;
}

/////////////////////////////////////////////////////////////////////

class audDynamicMixer;
class audScene;
class audScript;
class CEntity;


//////////////////////////////////////////////////////////////////////////////
// anAudDynMixPatch defines a set of dynamic mix variables which are applied to a specified
// fragment of the category hierarchy
//////////////////////////////////////////////////////////////////////////////
class audDynMixPatch
#if __BANK
	: public datBase
#endif
{
	friend class audDynamicMixer;
	friend class audScene;

public:
	audDynMixPatch();
	~audDynMixPatch();

	static const u32 sm_MaxCategoryControllers = 32;
	static const u32 sm_MaxMixPatches = 64;

	inline const MixerPatch * GetGameObject() const {return m_PatchObject;}
	u32 GetHash() { return m_ObjectHash; }
	inline u32 GetPatchActiveTime() const	{return m_PatchActiveTime; }
	bool Apply(float applyFactor = 1.f, audScene* scene = NULL);
	void Remove();
	void RecalculateCategoriesUsingApply(float scaleFactor);
	void ResetActiveTime() { m_PatchActiveTime = 0; }

	f32 GetApproachApplyFactor() const { return m_ApproachApplyFactor; }
	f32 GetApplyFactor();

	u16 GetFadeInTime() { return m_PatchObject ? m_PatchObject->FadeIn : 0; }
	u16 GetFadeOutTime() { return m_PatchObject ? m_PatchObject->FadeOut : 0; }

	bool HasFinishedFade();

	u32 GetTime() const;

	//Default for user function pointers
	static void DoNothing(void*) {}

#if __BANK
	//returns the number of lines drawn
	int DebugDraw(audNorthDebugDrawManager & drawMgr);
	const char* GetName() const;
#endif

private: 
	void SetPatchObject(MixerPatch* patchObject);
	void Initialize(MixerPatch* patchObject, audMixGroup * mixGroup = NULL, u32 startOffset = 0);
	void Initialize(audMixGroup * mixGroup = NULL);
	void Shutdown();
	bool IsReadyForDeletion() const;
	void Update();
	void UpdateFromRave();

	bool DoTransition();

	rage::audCategoryController*	m_CategoryControllers[sm_MaxCategoryControllers];
	MixerPatch*		m_PatchObject;

	audCurve					m_ApplyFactorCurve;
	audSimpleSmoother	m_ApplySmoother;

	float							m_ApplyFactor;
	float							m_ApproachApplyFactor;
	float							m_FadeOutApplyFactor;
	u32								m_PatchActiveTime;
	u32								m_TimeToDelete;
	u32								m_ObjectHash;
	u32								m_StartTime;
	u32								m_StopTime;
	audDynMixPatch**				m_MixPatchRef;
	const variableNameValuePair*			m_ApplyVariable;
	audScene*					m_Scene;
	audMixGroup *					m_MixGroup;

	u16								m_NumCategoryControllers;

	bool	m_IsApplied;
	bool	m_IsRemoving;
	bool	m_IsInitialized;
	bool	m_NoTransitions;
};

static const s32 g_MaxAudSceneVariables = 10;

struct audPatchGroup
{
	audPatchGroup()
	{
		patch = NULL;
		mixGroup = NULL;
	}
	audDynMixPatch * patch;
	audMixGroup * mixGroup;
};

//audScene describes a set of mixgroup-mixpatch pairs
class audScene
{
	friend class audDynamicMixer;

public:

	audScene();
	~audScene();

	const variableNameValuePair* GetVariableAddress(u32 nameHash);
	const variableNameValuePair* GetVariableAddress(const char * name);
	void SetVariableValue(const char *name, f32 value);
	void SetVariableValue(u32 nameHash, f32 value);
	//This is public since the scripts may ask an existing scene
	//to stop, however starting a scene must be done through 
	//audDynamicMixer
	void Stop();

	bool HasFinishedFade();

	bool IsFullyApplied();
	bool IsFullyAppliedForPause();

#if __BANK
	bool IsDebugScene() { return m_IsDebugScene; }
	void SetIsDebugScene() 
	{
		m_IsDebugScene = true;
		m_SceneRef = NULL;
	}
	bool IsTopLevel() { return m_IsTopLevel; }
	void SetIsTopLevel(bool isTopLevel) { m_IsTopLevel = isTopLevel; }
	void SetVariableValueDebug(u32 nameHash, f32 value);

	//Returns the number of lines drawn
	int DebugDraw(audNorthDebugDrawManager & drawMgr, AuditionSceneDrawMode mode) const;

	const char* GetName() const;


#endif

	audDynMixPatch * GetPatch(u32 nameHash);

	bool HasIncrementedMobileRadioCount() { return m_IncrementedMobileRadioCount; }
	bool IsInstanceOf(u32 sceneNameHash);
	void DecrementMobileRadioCount();

	const MixerScene * GetSceneSettings() const { return m_SceneSettings; }

	static const s32 sm_MaxScenes = 32;

	void SetHash(u32 hash) { m_SceneHash = hash; }
	u32 GetHash() const { return  m_SceneHash; }

	void PreparePatchesForKill();

	void SetIsReplay(bool isReplay) { m_IsReplay = isReplay; }
	bool GetIsReplay() { return m_IsReplay; }

private:

	void Start(const audScript* script = NULL, u32 startOffset = 0);
	void Shutdown();
	void SetSceneObject(MixerScene* sceneSettings);
	void Init(MixerScene* sceneSettings);
	bool IsReadyForDeletion();
	const audDynMixPatch* GetPatchFromIndex(u32 i) const;
	variableNameValuePair m_Variables[g_MaxAudSceneVariables];
	audPatchGroup m_Patches[MixerScene::MAX_PATCHGROUPS];
	MixerScene * m_SceneSettings;
	audScene** m_SceneRef;
	const audScript * m_Script;
	u32 m_CurrentTransition;
	u32 m_SceneHash;
	bool m_IsReplay;

	bool m_IncrementedMobileRadioCount;
#if __BANK
	bool m_IsDebugScene;
	bool m_IsTopLevel;
#endif

};




class audDynamicMixerObjectManager : public audObjectModifiedInterface
{
	friend class audDynamicMixer;

	static const u32 k_NameLength = 128;
	audDynamicMixerObjectManager()
	{
#if __BANK
		sysMemSet(m_RaveMixGroupName, 0, k_NameLength);
		sysMemSet(m_TransitionName, 0, k_NameLength);
		sysMemSet(m_ApplyVariableName, 0, k_NameLength);
		m_ApplyVariableValue = 1.f;

#endif
	}
	virtual void OnObjectAuditionStart(const u32 nameHash);
	virtual void OnObjectAuditionStop(const u32 nameHash);
	virtual void OnObjectModified(const u32 UNUSED_PARAM(nameHash));
	virtual void OnObjectViewed(const u32 UNUSED_PARAM(nameHash));

#if __BANK 

	f32 m_ApplyVariableValue;
	char m_RaveMixGroupName[k_NameLength];
	char m_TransitionName[k_NameLength];
	char m_ApplyVariableName[k_NameLength];
#endif
};

typedef atSNode<audDynMixPatch> audDynMixPatchNode;
typedef atSNode<audScene> audSceneNode;


//////////////////////////////////////////////////////////////////////////
// audDynamicMixer audio entity that manages and provides an interface
// for the application of dynamic mixing patches
//////////////////////////////////////////////////////////////////////////
class audDynamicMixer : public naAudioEntity
{

	friend class audDynMixPatch;
	friend class audScene;
	friend class audNorthAudioEngine;
	friend class audMixGroup;
private:
	audDynamicMixer();
	audDynamicMixer(const audDynamicMixer& mixer);
	audDynamicMixer& operator=(const audDynamicMixer&);
	~audDynamicMixer();

public:
	void Init();
	void Shutdown();
	void ShutdownSession(); //Cleans up all secenes and patches but doesn't delete pools
	static void Instantiate();

	audScene* GetActiveSceneFromHash(const u32 hash);
	audScene* GetActiveSceneFromMixerScene(const MixerScene *desiredScene);

	static bool FrontendSceneIsOverridden() 
	{
		return sm_FrontendSceneOverrided;
	}

	bool IsInitialized() const { return m_IsInitialized; }; 

	static bool IsPlayerFrontend() {return sm_PlayerFrontend;}
	static bool IsStealthScene() {return sm_StealthScene;}
	/************************************************************************
	 * Function:	StartScene
	 * Purpose:	Activates an audScene
	 ***********************************************************************/
	bool StartScene(const char* sceneName, audScene** sceneRef=NULL, const audScript* script=NULL, u32 startOffset = 0);
	bool StartScene(u32 sceneHash, audScene** sceneRef=NULL, const audScript* script=NULL, u32 startOffset = 0);
	bool StartScene(const MixerScene* sceneSettings, audScene** sceneRef=NULL, const audScript* script=NULL, u32 startOffset = 0);
	bool StartScene(audMetadataRef objectRef, audScene** sceneRef=NULL, const audScript* script=NULL, u32 startOffset = 0);


	/************************************************************************
	Function:	ApplyPatch
	Purpose:	Activates a mix patch
	*************************************************************************/
	void ApplyPatch(const u32 patchObjectHash, audMixGroup * mixGroup = NULL, audDynMixPatch** mixPatch=NULL, audScene* scene=NULL, float applyFactor = 1.f, u32 startOffset = 0) ;
	//void ApplyPatch(const MixerPatch* patchObject, audMixGroup * mixGroup = NULL, audDynMixPatch** mixPatch=NULL, audScene* scene=NULL, float applyFactor = 1.f) ;

	 //PURPOSE:	And and remove entities from mix groups
	 void RemoveEntityFromMixGroup(CEntity *entity,f32 fadeTime = -1.f);
	 void AddEntityToMixGroup(CEntity * entity, u32 mixGroupHash,f32 fadeTime = -1);
	 void MoveEntityToMixGroup(CEntity* entity, u32 mixGroupHash,f32 transitionTime);
	/************************************************************************
	Function:	RemoveAll
	Purpose:	Removes all mix patches currently active
	************************************************************************/
	void RemoveAll();

	/************************************************************************
	Function:	Update
	Purpose:	Updates, called from northAudioEngine
	************************************************************************/
	void Update();

	virtual void FinishUpdate();

	bool IsMetadataInChunk(void * metadataPtr, const audMetadataChunk &chunk);
	void StopOnlineScenes();
	void StopScenesFromChunk(const char * chunkName);
	void KillReplayScenes();


#if __BANK
	void AddWidgets(bkBank& bank);

	audScene * GetDebugScene(u32 sceneHash);
	bool StartDebugScene(u32 sceneHash, bool isTopLevel = false);
	void StopDebugScene(u32 sceneHash);
	void AddDebugVariable();
	audDynamicMixerObjectManager & GetObjectManager() { return sm_DynamicMixerObjectManager; }

	void SetMixGroupDebugToolIsActive(bool active) { sm_MixgroupDebugToolIsActive = active; }

	static void StartRagDebugScene();
	static void StopRagDebugScene();

	void DrawDebugScenes();
	void PrintScenes() const;
	void PrintPatches() const;
	void PrintDebug() const;
	static void DebugStopAllScenes();
	static void DebugStopNamedScenes();

#endif

	void IncrementMobileRadioCount();
	void DecrementMobileRadioCount();
	u32 GetMobileRadioCount() { return m_MobileRadioActiveCount; }

private:



		static audDynamicMixer*							sm_Instance;

		atSList<audDynMixPatch>			m_ListMixPatches;
		atSList<audScene>				m_ListScenes;

		u32								m_MobileRadioActiveCount;
		bool m_IsInitialized;
	
		static bool				sm_PlayerFrontend;
		static bool				sm_StealthScene;

		static bool				sm_FrontendSceneOverrided;


#if __BANK
		sysCriticalSectionToken sm_DebugDrawLock;
		audScene * m_RaveScene;
		static audDynamicMixerObjectManager sm_DynamicMixerObjectManager;

		static bool sm_MixgroupDebugToolIsActive;
		static bool sm_AlwaysUpdateDebugApplyVariable;
		static bool sm_DisableMixgroupSoundProcess;
		static char sm_DebugSceneName[audDynamicMixerObjectManager::k_NameLength];

		static float sm_AuditionSceneDrawOffsetY;

		static AuditionSceneDrawMode sm_SceneDrawMode;
		static bool sm_PrintAllControllers;
		static bool sm_DisableGlobalControllers;
		static bool sm_DisableDynamicControllers;
		static bool sm_PrintDebug;
#endif

};


// Make this a singleton
#define DYNAMICMIXER (audNorthAudioEngine::GetDynamicMixer())

#endif

