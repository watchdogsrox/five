/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneCameraEntity.h
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_CAMERAENTITY_H 
#define CUTSCENE_CAMERAENTITY_H

//Rage Headers
#include "cutscene/cutscene_channel.h"
#include "cutscene/cutsentity.h"
#include "cutscene/CutSceneDefine.h"
#include "cutfile/cutfeventargs.h"
#include "cutfile/cutfobject.h"
#include "fwutil/flags.h"

//Game Headers
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraManager.h"

#if __BANK
#define cutsceneCameraEntityDebugf(pObject, fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && pObject ) { char debugStr[256]; CCutSceneCameraEntity::CommonDebugStr(pObject, debugStr); cutsceneDebugf1("%s" fmt, debugStr,##__VA_ARGS__); }
#else
#define cutsceneCameraEntityDebugf(pObject, fmt,...) do {} while(false)
#endif //__BANK

class CutSceneManager; 

class CCutSceneCameraEntity : public cutsUniqueEntity
{
public:	
	CCutSceneCameraEntity (const cutfObject* pObject);
	~CCutSceneCameraEntity ();

	enum RETURNING_TO_GAME_STATE
	{
		BLENDING_BACK_TO_THIRD_PERSON = BIT0,
		BLENDING_BACK_TO_FIRST_PERSON = BIT1,
		CATCHING_UP_THIRD_PERSON = BIT2,
		CATCHING_UP_FIRST_PERSON = BIT3,
		SCRIPT_REQUSTED_RETURN = BIT4,
		END_OF_SCENE_RETURN = BIT5
	};


#if __BANK
	enum DOF_STATE
	{
		DOF_DATA_DRIVEN,
		DOF_FORCED_ON,
		DOF_FORCED_OFF
	};
#endif

	 virtual void DispatchEvent( cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0 );

	 s32 GetType() const { return CUTSCENE_CAMERA_GAME_ENITITY; }

#if __BANK
	bool IsCameraPointingAtThisObject(const cutsEntity* pEnt) const ; 

	const char* GetCameraCutName() const { return m_DrawDistanceEventName; }

	const char* GetCameraCutEventName() const { return m_pCameraCutEventArgs ? m_pCameraCutEventArgs->GetName().GetCStr() : GetCameraCutName(); }

	void RenderDofPlanes() const; 

	void SetCameraHasFirstPersonBlendOutEvent(bool hasEvent) { m_ShouldProcessFirstPersonBlendOutEvents = hasEvent; }

#endif

	bool CanBlendOutThisFrame() const { return m_CanTerminateThisFrame; } 

	bool IsRegisteredGameEntityUnderScriptControl() const { return m_bIsRegisteredEntityScriptControlled; }

	void SetRegisteredGameEntityIsUnderScriptControl(bool ScriptControlled) { m_bIsRegisteredEntityScriptControlled = ScriptControlled; }

	void SetCameraReadyForGame(cutsManager* pManager, fwFlags32 State);

	float CalculateTimeOfDayDofModifier(const cutfCameraCutEventArgs* pCameraEvents); 

	bool ShouldUseDayCoCTrack(const cutsManager* pManager) const; 

	bool WillExitToFirstPersonCamera(const CutSceneManager* pManager) const;

	void SetCascadeShadowFocusEntityForSeamlessExit(const CEntity* pEntity);
	const CEntity* GetCascadeShadowFocusEntityForSeamlessExit() const;

	const cutfCameraCutEventArgs* GetCameraCutEventArgs() const { return m_pCameraCutEventArgs; }

	float GetMapLodScale() const;

	float GetReflectionLodRangeStart() const; 

	float GetReflectionLodRangeEnd()const;

	float GetReflectionSLodRangeStart()const;

	float GetReflectionSLodRangeEnd()const;

	float GetLodMultHD()const; 

	float GetLodMultOrphanHD()const;

	float GetLodMultLod()const; 

	float GetLodMultSlod1()const;

	float GetLodMultSlod2()const;

	float GetLodMultSlod3()const;

	float GetLodMultSlod4()const;

	float GetWaterReflectionFarClip()const;

	float GetLightSSAOInten()const;

	float GetExposurePush()const;

	bool GetDisableHighQualityDof()const;
	
	bool IsReflectionMapFrozen()const; 
	
	bool IsDirectionalLightDisabled()const;

	float GetDirectionalLightMultiplier() const; 

	float GetLensArtefactMultiplier() const; 

	float GetBloomMax() const; 

	const cutfCameraCutCharacterLightParams* GetCharacterLightParams() const;

	const cutfCameraObject* GetCameraObject() const { cutsceneAssert(!GetObject() || GetObject()->GetType()==CUTSCENE_CAMERA_OBJECT_TYPE); return static_cast<const cutfCameraObject*>(GetObject()); }

	void SetCharacterLightValues(float currentTime, float LightIntensity, float NightIntensity, const Vector3& Direction, const Vector3& Colour, bool UseTimecycle, bool UseAsIntensityModifier, bool ConvertToCameraSpace); 

	void UpdateDof(); 

	bool IsCuttingBackEarlyForScript() const; 

	bool IsInFirstPerson () const { return camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT); }
private:

#if __BANK
	void CalculateCameraWorldMatFromFace(Matrix34 &mMat); 

	void UpdateFaceViewer();

	void OverrideCameraProperties(cutsManager *pManager); 

	void UpdateOverriddenCamera(CutSceneManager* pCutsceneManager); 

	void UpdateDofState(CutSceneManager* pCutsceneManager); 
#endif

#if FPS_MODE_SUPPORTED
	camManager::eFirstPersonFlashEffectType GetFirstPersonFlashEffectType(const CutSceneManager* pManager);
	u32 GetFirstPersonFlashEffectDelay(const CutSceneManager* pManager, u32 blendOutDuration, bool bCatchup) const;
	bool ShouldTriggerFirstPersonCameraFlash(const CutSceneManager* pManager) const;
#endif // FPS_MODE_SUPPORTED

public: 
#if __BANK
	static atString ms_DofState; 
	static atString ms_CoCMod;
	static atString ms_DofPlanes; 
	static atString ms_DofEffect; 
	static atString ms_Blending; 
	static bool m_RenderCameraStatus; 
#endif


private:
	cutfCameraCutCharacterLightParams m_CharacterLight; 
	RegdConstEnt m_CascadeShadowFocusEntityForSeamlessExit;
	const cutfCameraCutEventArgs* m_pCameraCutEventArgs;
	
	bool m_EnableDOF; 
	bool m_CanTerminateThisFrame; 
	bool m_bIsRegisteredEntityScriptControlled; 
	bool m_ShouldUseCatchUpCameraExit; 
	bool m_CameraReadyForGame;
	bool m_HaveAppliedShadowBounds;
	bool m_BlendOutEventProcessed;
	bool m_FpsCutFlashTriggered;
	bool m_bAllowSettingOfCascadeShadowFocusEntity;
	bool m_ShouldProcessFirstPersonBlendOutEvents; 
	bool m_AreCatchingUpFromFirstPerson; 
	bool m_shouldForceFirstPersonModeIfTransitioningToAVehicle; 
	fwFlags32 m_ReturningToGameState; 

#if __BANK
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
	virtual void DebugDraw() const;
	void RenderDofInfo() const;
	const cutsEntity*  m_pTargetEntity; 
	char m_DrawDistanceEventName[CUTSCENE_OBJNAMELEN];
	bool m_bFaceModeEnabled; 
	float m_fZoomDistance; 
	bool m_bTriggeredFromWidget; 
	float m_LastTimeOfDayDofModifier; 
	u32 m_DofState; 
#endif

};

#endif