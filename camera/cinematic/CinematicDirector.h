//
// camera/cinematic/CinematicDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_DIRECTOR_H
#define CINEMATIC_DIRECTOR_H

#include "control/stuntjump.h"
#include "camera/base/BaseDirector.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/helpers/Frame.h"
#include "camera/cinematic/context/BaseCinematicContext.h"

#include "atl/vector.h"
#include "math/random.h"

class camBaseCamera;
class camCinematicDirectorMetadata;
class CEntity;

class camCinematicDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicDirector, camBaseDirector);

public:
	friend class camBaseCinematicContext; 

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor
	
	enum eSideOfTheActionLine
	{
		ACTION_LINE_ANY = 0,
		ACTION_LINE_LEFT,
		ACTION_LINE_RIGHT
	};
	
	enum ScriptLaunchedContexts
	{
		CINEMATIC_SPECTATOR_NEWS_CHANNEL_CONTEXT = 4287968789
	};

private:
	camCinematicDirector(const camBaseObjectMetadata& metadata);

public:
	~camCinematicDirector();

	bool			IsRenderedCameraInsideVehicle() const;
	bool			IsRenderedCameraVehicleTurret() const;
	bool			IsRenderedCameraFirstPerson() const;
	bool			IsRenderedCameraForcingMotionBlur() const;

	bool			ShouldDisplayReticule() const;
	//s32				GetRenderedInVehicleViewMode() const					{ return m_RenderedInVehicleViewMode; }

	void			SetCinematicButtonDisabledByScriptState(bool state)		{ m_IsCinematicButtonDisabledByScript = state; }
	bool			IsCinematicButtonDisabledByScriptState() const			{ return m_IsCinematicButtonDisabledByScript; }	

	void 			DisableFirstPersonInVehicleThisUpdate()							{ m_IsFirstPersonInVehicleDisabledThisUpdate = true; }
	bool			IsFirstPersonInVehicleDisabledThisUpdate() const				{ return m_IsFirstPersonInVehicleDisabledThisUpdate; }

    void            IgnoreMenuPreferenceForBonnetCameraThisUpdate()             { m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate = true; }
    bool            IsMenuPreferenceForBonnetCameraIgnoredThisUpdate() const    { return m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate; }

	void 			DisableVehicleIdleModeThisUpdate()						{ m_IsVehicleIdleModeDisabledThisUpdate = true; }
	bool			IsVehicleIdleModeDisabledThisUpdate() const				{ return m_IsVehicleIdleModeDisabledThisUpdate; }
	void			InvalidateCinematicVehicleIdleMode();
	void			DisableCinematicSlowMoThisUpdate()					{ m_IsScriptDisablingSlowMoThisUpdate = true; }
	bool			IsCinematicSlowMoDisabledThisUpdate() const			{ return m_IsScriptDisablingSlowMoThisUpdate; }

	void			InvalidateIdleCamera();
	
	u32				GetTimeBeforeCinematicVehicleIdleCam() const;

	bool			IsRenderingCinematicMountedCamera(bool shouldTestValidity = true) const; 
	bool			IsRenderingCinematicPointOfViewCamera(bool shouldTestValidity = true) const;
	bool			IsRenderingCinematicIdleCamera(bool shouldTestValidity = true) const; 
	bool			IsRenderingCinematicWaterCrash(bool shouldTestValidity = true) const; 
	bool			IsRenderingCinematicCameraInsideVehicle(bool shouldTestValidity = true) const; 

    float           GetVehicleImpactShakeMinDamage() const;
	
	camCinematicMountedCamera* GetFirstPersonVehicleCamera() const;
    camCinematicMountedCamera* GetBonnetVehicleCamera() const;

	const camBaseCinematicContext* GetCurrentContext() const; 
	
	bool			IsRenderingAnyInVehicleCinematicCamera() const; 
	bool			IsRenderingAnyInVehicleFirstPersonCinematicCamera() const;

	void			ScriptDisableWaterClippingTestThisUpdate() { m_IsScriptDisablingWaterClippingTestThisUpdate = true; }
	bool			IsScriptDisablingWaterClippingTest() { return m_IsScriptDisablingWaterClippingTestThisUpdate; }

#if __BANK
	virtual void	AddWidgetsForInstance();

	void ComputeBonnetCameraHashCB(); 

	u32 GetOverriddenBonnetCamera(); 
#endif // __BANK

	const bool		ShouldUsePreferedShot()	{ return m_ShouldUsePreferedShot; } 
	const u32		GetUsePreferredMode() { return m_InputSelectedShotPreference; }
	const bool		ShouldChangeCinematicShot() { return m_ShouldChangeShot; }
	const bool		IsCinematicInputActive() const { return (!IsCinematicButtonDisabledByScriptState() && m_IsCinematicInputActive) || m_IsCinematicInputActiveForScript ; }
	const bool		IsAnyCinematicContextActive () const {return m_currentContext >= 0 && m_currentContext < m_Contexts.GetCount(); }

	bool			IsSlowMoActive() { return m_IsSlowMoActive; }

	camBaseCinematicContext* FindContext(u32 m_hash); 

	camFrame& GetFrameNonConst() { return m_Frame; }
	
	eSideOfTheActionLine GetSideOfActionLine(const CEntity* pNewLookAtEntity) const; 

	bool IsCameraPositionValidWithRespectToTheLine( const CEntity* pEntity, const Vector3& camPosition) const; 

	const Vector3& GetLastValidCinematicCameraPosition() const  { return m_LastCameraPosition; }

	void SetScriptOverridesCinematicInputThisFrame(bool OverrideCinematicInputThisFrame) { m_ShouldScriptOverrideCinematicInputThisUpdate = OverrideCinematicInputThisFrame; } 
	
	void SetCinematicLatchedStateForScript(bool active) { m_LatchedState = active; }

	void SetCanActivateNewsChannelContext(bool ActivateNewsChannelContextThisUpdate ) { m_CanActivateNewsChannelContextThisUpdate = ActivateNewsChannelContextThisUpdate; } 
	bool CanActivateNewsChannelContext() const { return m_CanActivateNewsChannelContextThisUpdate; }
	
	void SetCanActivateMissionCreatorFailContext (bool ActivateNewsChannelContextThisUpdate ) { m_CanActivateMissionCreatorFailContext = ActivateNewsChannelContextThisUpdate; } 
	bool CanActivateMissionCreatorFailContext() const { return m_CanActivateMissionCreatorFailContext;  }

	mthRandom& GetRandomNumber() { return m_Random; }

	void RegisterVehicleDamage(const CVehicle* pVehicle, const Vector3& impactDirection, float damage); 

	bool HaveUpdatedACinematicCamera() { return m_HaveUpdatedACinematicCamera; }

	void DoSecondUpdateForTurretCamera(camBaseCamera* pCamera, bool bDoPostUpdate);

    void CacheHeadingAndPitchForVehiclePovCameraDuringWaterClip(const float heading, const float pitch);

	static bool IsBonnetCameraEnabled() { return ms_UseBonnetCamera; }

	static bool IsSpectatingCopsAndCrooksEscape();

public:
	static bool		ms_UseBonnetCamera; 

private:
	virtual bool	Update();
	virtual void	PostUpdate();
	
	void			PreUpdateContext(); 
	bool			UpdateContext();
	bool			CreateHigherPriorityContext(); 

	void			UpdateCinematicViewModeControl();
	void			UpdateCinematicViewModeSlowMotionControl();

	void			CleanUpCinematicContext();
	
	void			DoesCurrentContextSupportQuickToggle();

	void			UpdateRenderedCameraVehicleFlags(); 
	
	void			UpdateScannedEntitesForWantedClonedPlayer();

#if FPS_MODE_SUPPORTED
	void			RegisterWeaponFireRemote(const CWeapon& weapon, const CEntity& firingEntity);
	void			RegisterWeaponFireLocal(const CWeapon& weapon, const CEntity& firingEntity);
#endif

protected:
	void			SetShouldUsePreferredShot(bool UsePrefferedShot) { m_ShouldUsePreferedShot = UsePrefferedShot;  }
	
	void			UpdateInputForQuickToggleActivationMode(); 

private:
	
	atVector<camBaseCinematicContext*> m_Contexts; 
	const camCinematicDirectorMetadata& m_Metadata;
	camEnvelope*	m_SlowMoEnvelope;
	RegdConstEnt	m_TargetEntity;
	
	RegdConstEnt	m_LastLookAtEntity;
	
	camDampedSpring m_SlowMotionScalingSpring;
	
	mthRandom		m_Random; 

	Vector3			m_FollowPedPositionOnPreviousUpdate;
	Vector3			m_LastCameraPosition; 
	u32				m_PreviousKillShotStartTime;
	u32				m_PreviousReactionShotStartTime;
	u32				m_TimeCinematicButtonIsDown; 
	u32				m_TimeCinematicButtonInitalPressed; 
	u32				m_InputSelectedShotPreference; 
	u32				m_TimeCinematicButtonWasHeldPressed; 
	u32				m_CurrentSlowMotionBlendOutDurationOnShotTermination; 
	u32				m_SlowMotionBlendOutDurationOnShotTermination; 
    u32             m_VehiclePovCameraWasTerminatedForWaterClippingTime;
	
#if __BANK
	s32				m_DebugBonnetCameraIndex; 

#endif

	s32				m_currentContext;

	float 			m_SlowMotionScaling;
	float			m_desiredSlowMotionScaling; 
	float			m_previousSlowMoValue; 
    float           m_CachedHeadingForVehiclePovCameraDuringWaterClipping;
    float           m_CachedPitchForVehiclePovCameraDuringWaterClipping;

	bool			m_IsCinematicButtonDisabledByScript		: 1; //Has the script disabled the cinematic button for the active camera.
	bool			m_IsFirstPersonInVehicleDisabledThisUpdate		: 1;
	bool			m_IsVehicleIdleModeDisabledThisUpdate	: 1;
	bool			m_HasCinematicContextChangedThisUpdate	: 1;
	bool			m_CanActivateMeleeCam					: 1;
	bool			m_ShouldSlowMotion						: 1;
	bool			m_HasControlStickBeenReset				: 1;
	bool			m_IsCinematicCameraModeLocked			: 1;
	bool			m_ShouldUsePreferedShot					: 1;
	bool			m_ShouldChangeShot						: 1;
	bool			m_IsSlowMoActive						: 1;
	bool			m_IsCinematicInputActive				: 1; 
	bool			m_HaveOverriddenSlowMoShot				: 1; 
	bool			m_HaveCancelledOverridenShot			: 1; 
	
	bool			m_LatchedState							: 1; 
	bool			m_IsCinematicButtonDown					: 1; 
	bool			m_IsUsingQuickToggleThisFrame			: 1; 
	bool			m_HaveJustChangedToToggleContext		: 1; 
	bool			m_IsRenderedCamInVehicle				: 1; 
	bool			m_IsRenderedCamPovTurret				: 1;
	bool			m_IsRenderedCamFirstPerson				: 1;
	bool			m_ShouldRespectTheLineOfAction			: 1;
	bool			m_ShouldScriptOverrideCinematicInputThisUpdate : 1; 
	bool			m_IsCinematicInputActiveForScript		: 1; 
	bool			m_CanActivateNewsChannelContextThisUpdate : 1; 
	bool			m_CanActivateMissionCreatorFailContext	: 1; 
	bool			m_WasCinematicButtonAciveAfterScriptStoppedBlocking : 1; 
	bool			m_HaveStoredPressedTime	: 1; 
	bool			m_ShouldHoldCinematicShot : 1; 
	bool			m_CanBlendOutSlowMotionOnShotTermination : 1; 
	bool			m_IsScriptDisablingSlowMoThisUpdate	: 1;
	bool			m_HaveUpdatedACinematicCamera : 1; 
	bool			m_DisableBonnetCameraForScriptedAnimTask : 1; 
    bool            m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate : 1;
	bool			m_IsScriptDisablingWaterClippingTestThisUpdate : 1;
    bool            m_VehiclePovCameraWasTerminatedForWaterClipping : 1;

#if __BANK
	bool		m_WasCinematicInputBlockedByScriptForDebugOutput	 : 1; 
#endif
	//RegdCamBaseCamera m_RenderedCamera;

	//Forbid copy construction and assignment.
	camCinematicDirector(const camCinematicDirector& other);
	camCinematicDirector& operator=(const camCinematicDirector& other);
};



#endif // CINEMATIC_DIRECTOR_H
