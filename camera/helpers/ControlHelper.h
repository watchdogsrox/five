//
// camera/helpers/ControlHelper.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CONTROL_HELPER_H
#define CONTROL_HELPER_H

#include "atl/binmap.h"
#include "vector/vector2.h"

#include "camera/base/BaseObject.h"
#include "camera/system/CameraMetadata.h"

class camEnvelope;
class CControl;
class CEntity;
class CVehicle;

//A helper that parameterizes camera-related control input.
class camControlHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camControlHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camControlHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camControlHelper(const camBaseObjectMetadata& metadata);

public:
	~camControlHelper();

	s32				GetViewMode() const					{ return m_ViewMode; }
	const camControlHelperMetadataViewModeSettings* GetViewModeSettings(s32 mode = -1) const;
	const float*	GetViewModeBlendLevels() const		{ return m_ViewModeBlendLevels; }
	bool			IsViewModeBlending() const;
	float			GetOverallViewModeBlendLevel() const;

	s32				GetViewModeContext() const			{ return m_ViewModeContext; }

	bool 			IsLookAroundInputPresent() const		{ return m_IsLookAroundInputPresent; }
	float			GetLookAroundInputEnvelopeLevel() const { return m_LookAroundInputEnvelopeLevel; }

	float			GetLookAroundHeadingOffset() const	{ return m_LookAroundHeadingOffset; }
	float			GetLookAroundPitchOffset() const	{ return m_LookAroundPitchOffset; }
	float			GetLookAroundSpeed() const			{ return m_LookAroundSpeed; }

	float			GetNormLookAroundHeadingOffset() const	{ return m_NormalisedLookAroundHeadingInput; }
#if __BANK
	float			GetNormLookAroundPitchOffset() const	{ return m_NormalisedLookAroundPitchInput; }
#endif // __BANK

	float			ComputeMaxLookAroundOrientationSpeed() const;

	void			SetLookAroundDecelerationScaling(float scaling) { m_LookAroundDecelerationScaling = scaling; }

	void			SkipControlHelperUpdateThisUpdate() { m_SkipControlHelperThisUpdate = true; }
	void			SetShouldUpdateViewModeThisUpdate()	{ m_ShouldUpdateViewModeThisUpdate = true; }
	void			IgnoreViewModeInputThisUpdate()		{ m_ShouldIgnoreViewModeInputThisUpdate = true; }
	void			SkipViewModeBlendThisUpdate()		{ m_ShouldSkipViewModeBlendThisUpdate = true; }
	void			IgnoreLookAroundInputThisUpdate()	{ m_ShouldIgnoreLookAroundInputThisUpdate = true; }
	void			SetUseLeftStickInputThisUpdate()	{ m_UseLeftStickForLookInputThisUpdate = true; }
	void			SetUseBothStickInputThisUpdate()	{ m_UseBothSticksForLookInputThisUpdate = true; }

	void			SetForceAimSensitivityThisUpdate()					{ m_ShouldForceAimSensitivityThisUpdate = true; }
	void			SetForceDefaultLookAroundSensitivityThisUpdate()	{ m_ShouldForceDefaultLookAroundSensitivityThisUpdate = true; }

	void			SetFirstPersonAimSensitivityLimitThisUpdate(s32 limit) { m_LimitFirstPersonAimSensitivityThisUpdate = limit; }

	bool			IsUsingZoomInput() const;
	float			GetZoomFov() const;
	const Vector2&	GetZoomFactorLimits() const			{ return m_ZoomFactorLimits; }
	float			GetZoomFactorSpeed() const			{ return m_ZoomFactorSpeed; }

	bool			IsUsingLookBehindInput() const;
	bool			IsLookingBehind() const				{ return m_IsLookingBehind; }
	bool			WasLookingBehind() const			{ return m_WasLookingBehind; }
	void			SetLookBehindState(bool state)		{ m_IsLookingBehind = state; }
	void			ActivateLookBehindInput()			{ m_IsLookBehindInputActive = true; }
	void			DeactivateLookBehindInput()			{ m_IsLookBehindInputActive = false; }
	void			IgnoreLookBehindInputThisUpdate()	{ m_ShouldIgnoreLookBehindInputThisUpdate = true; }
	bool			WillBeLookingBehindThisUpdate(const CControl& control) const;

	bool			IsInAccurateMode() const			{ return m_IsInAccurateMode; }
	void			SetAccurateModeState(bool state);
	void			IgnoreAccurateModeInputThisUpdate()	{ m_ShouldIgnoreAccurateModeInputThisUpdate = true; }

#if RSG_PC
	bool			WasUsingKeyboardMouseLastInput() const { return m_WasUsingKeyboardMouseLastInput; }
#endif // RSG_PC

	void			CloneSpeeds(const camControlHelper& sourceControlHelper);

	void			Update(const CEntity& attachParent, bool isEarlyUpdate = false);

	void			SetOverrideViewModeForSpectatorModeThisUpdate() { m_IsOverridingViewModeForSpectatorModeThisUpdate = true; }

	void			SetShouldUseOriginalMaxControllerAimSensitivity(bool state) { m_ShouldUseOriginalMaxControllerAimSensitivity = state; }

	static void		InitClass();
	static void		ShutdownClass();

	static s32		GetViewModeForContext(s32 contextIndex);
	static bool		SetViewModeForContext(s32 contextIndex, s32 viewMode);

	static s32		GetLastSetViewMode()				{ return ms_LastSetViewMode; }

	static s32		ComputeViewModeContextForVehicle(const CVehicle& vehicle);

	static void		StoreContextViewModesInMap(atBinaryMap<atHashValue, atHashValue>& map);
	static void		RestoreContextViewModesFromMap(const atBinaryMap<atHashValue, atHashValue>& map);

	static float	GetZoomFactor()						{ return ms_ZoomFactor; }
	static void		SetZoomFactor(float zoomFactor)		{ ms_ZoomFactor = zoomFactor; }
	static void		OverrideZoomFactorLimits(float minZoomFactor, float maxZoomFactor);
	static void		ClearOverriddenZoomFactorLimits()	{ ms_OverriddenZoomFactorLimits.Zero(); ms_ShouldOverrideZoomFactorLimits = false; }

	static float	ComputeOffsetBasedOnAcceleratedInput(float input, float acceleration, float deceleration, float maxSpeed, float& speed,
						float timeStep, bool shouldResetSpeedOnInputFlip = true);

	static s32		ComputeViewModeContextForVehicle(u32 ModelHashKey);

	static u32		GetBonnetCameraToggleTime();

#if __BANK
	static void		AddWidgets(bkBank& bank);
	static bool		DebugHadSetViewModeForContext(s32& contextIndex, s32& viewModeForContext);
	static void		DebugResetHadSetViewModeForContext();
#endif // __BANK

public:

#if __BANK
	static float	ms_LookAroundSpeedScalar; 
	static bool		ms_UseOverriddenLookAroundHelper;
	static camControlHelperMetadataLookAround ms_OverriddenLookAroundHelper;
#endif // __BANK

private:
	void			InitViewModeContext(const CEntity& attachParent);
	void			InitViewMode();
	void			UpdateLookBehindControl(const CControl& control, bool isEarlyUpdate);
	void			UpdateViewModeControl(const CControl& control, const CEntity& attachParent);
	bool			ComputeShouldUpdateViewMode() const;
	void			ValidateViewMode(s32& viewMode) const;
	bool			ComputeIsViewModeValid(s32 viewMode) const;
	const camControlHelperMetadataLookAround& GetLookAroundMetadata() const; 
	void			UpdateLookAroundControl(const CControl& control);
	bool			ComputeLookAroundInput(const CControl& control, float& inputMag, float& headingInput, float& pitchInput, bool& usingAccelerometers) const;
	void			UpdateLookAroundInputEnvelope(bool isInputPresent);
	float			GetDeadZoneScaling(float fInputMag) const;
	float			GetAimSensitivityScaling() const;
	void			UpdateZoomControl(CControl& control);
	bool			ComputeZoomInput(CControl& control, float& zoomInput) const;
	void			UpdateZoomFactorLimits();
	void			UpdateAccurateModeControl(CControl& control);
	void			Reset();

	static s32		GetViewModeForContextInternal(s32 contextIndex);
	static s32*		GetViewModeForContextWithValidation(s32 contextIndex);

	const camControlHelperMetadata& m_Metadata;

	float			m_ViewModeSourceBlendLevels[camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS];
	float			m_ViewModeBlendLevels[camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS];
	Vector2			m_ZoomFactorLimits;
	camEnvelope*	m_ViewModeBlendEnvelope;
	camEnvelope*	m_LookAroundInputEnvelope;
	s32				m_ViewModeContext;
	s32				m_ViewMode;
	float			m_LookAroundSpeed;
	float			m_NormalisedLookAroundHeadingInput;
	float			m_NormalisedLookAroundPitchInput;
	float			m_LookAroundHeadingOffset;
	float			m_LookAroundPitchOffset;
	float			m_LookAroundHeadingInputPreviousUpdate;
	float			m_LookAroundInputEnvelopeLevel;
	float			m_LookAroundDecelerationScaling;
	float			m_ZoomFactorSpeed;
	u32				m_TimeReleasedLookBehindInput;
	s32				m_LimitFirstPersonAimSensitivityThisUpdate;
	u32				m_LastTimeViewModeControlDisabled;
	bool			m_ShouldUpdateViewModeThisUpdate			: 1;
	bool			m_ShouldIgnoreViewModeInputThisUpdate		: 1;
	bool			m_ShouldSkipViewModeBlendThisUpdate			: 1;
	bool			m_ShouldIgnoreLookAroundInputThisUpdate		: 1;
	bool			m_IsLookBehindInputActive					: 1;
	bool			m_ShouldIgnoreLookBehindInputThisUpdate		: 1;
	bool			m_IsLookAroundInputPresent					: 1;
	bool			m_IsLookingBehind							: 1;
	bool			m_WasLookingBehind							: 1;
	bool			m_ShouldIgnoreAccurateModeInputThisUpdate	: 1;
	bool			m_IsInAccurateMode							: 1;
	bool			m_IsOverridingViewModeForSpectatorModeThisUpdate : 1;
	bool			m_ShouldUseOriginalMaxControllerAimSensitivity : 1;
	bool			m_UseLeftStickForLookInputThisUpdate		: 1;
	bool			m_UseBothSticksForLookInputThisUpdate		: 1;
	bool			m_ShouldForceAimSensitivityThisUpdate		: 1;
	bool			m_ShouldForceDefaultLookAroundSensitivityThisUpdate : 1;
	bool			m_SkipControlHelperThisUpdate				: 1;
#if RSG_ORBIS
	bool			m_WasPlayerAiming							: 1;
#endif // RSG_ORBIS
#if RSG_PC
	bool			m_WasUsingKeyboardMouseLastInput			: 1;
#endif // RSG_PC

	static Vector2	ms_OverriddenZoomFactorLimits;
	static s32*		ms_ContextViewModes;
	static float	ms_ZoomFactor;
	static bool		ms_ShouldOverrideZoomFactorLimits;
	static bool		ms_MaintainCurrentViewMode;
	static s32		ms_LastViewModeContext;
	static s32		ms_LastSetViewMode;
	static s32		ms_ContextToForceFirstPersonCamera;
	static s32		ms_ContextToForceThirdPersonCamera;
	static s32		ms_ThirdPersonViewModeToForce;
	static bool		ms_FirstPersonCancelledForCinematicCamera;

#if __BANK
	static s32		ms_DebugAimSensitivitySetting;
	static s32		ms_DebugLookAroundSensitivitySetting;
	static s32		ms_DebugRequestedContextIndex;
	static s32		ms_DebugRequestedViewModeForContext;
	static bool		ms_DebugWasViewModeForContextSetThisUpdate;
#endif // __BANK

private:
	//Forbid copy construction and assignment.
	camControlHelper(const camControlHelper& other);
	camControlHelper& operator=(const camControlHelper& other);
};

#endif // CONTROL_HELPER_H
