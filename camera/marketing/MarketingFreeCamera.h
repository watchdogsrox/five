//
// camera/marketing/MarketingFreeCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_FREE_CAMERA_H
#define MARKETING_FREE_CAMERA_H

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/base/BaseCamera.h"

class camMarketingFreeCameraMetadata;
class CPad;

//A marketing camera that allows free movement via control input.
class camMarketingFreeCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingFreeCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camMarketingFreeCamera(const camBaseObjectMetadata& metadata);

public:
	virtual void	Reset();
	virtual void	AddWidgetsForInstance();

	static void		InitClass();
	static void		AddWidgets(bkBank &bank);

protected:
	virtual bool	Update();
	virtual void	ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;
	const CEntity*	ComputeFocusEntity() const;
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	CEntity*		FindNearestEntityToPosition(const Vector3& position, float maxDistanceToTest) const;

	void			UpdateControl();
	void			UpdateLookAroundControl(CPad& pad);
	bool			ComputeLookAroundInput(CPad& pad, float& headingInput, float& pitchInput) const;
	void			UpdateTranslationControl(CPad& pad);
	void			ComputeTranslationInput(CPad& pad, Vector3& translationInput, Vector3& relativeTranslationInput) const;
	void			UpdateZoomControl(CPad& pad);
	bool			ComputeZoomInput(CPad& pad, float& zoomInput);
	void			UpdateRollControl(CPad& pad);
	bool			ComputeRollInput(CPad& pad, float& rollInput);
	virtual void	UpdateMiscControl(CPad& pad);

	virtual void	ApplyTranslation();
	void			ApplyZoom();
	void			ApplyDof();
	virtual void	ApplyRotation();

	void			UpdateLighting();
	void			TeleportFollowEntity();

	static void		OnLensChange();
	static void		OnManualLensChange();

	const camMarketingFreeCameraMetadata& m_Metadata;

	Vector3	m_TranslationVelocity;
	Vector3	m_Translation;
	Vector3	m_RelativeTranslationVelocity;
	Vector3	m_RelativeTranslation;

	float	m_LookAroundSpeedScaling;
	float	m_LookAroundHeadingSpeed;
	float	m_LookAroundPitchSpeed;
	float	m_LookAroundHeadingOffset;
	float	m_LookAroundPitchOffset;

	float	m_TranslationSpeedScaling;

	float	m_AimAcceleration;				// copied out of metadata so we can expose in widgets
	float	m_AimDeceleration;				// copied out of metadata so we can expose in widgets
	float	m_TranslationAcceleration;		// copied out of metadata so we can expose in widgets
	float	m_TranslationDeceleration;		// copied out of metadata so we can expose in widgets

	float	m_ZoomSpeed;
	float	m_ZoomOffset;
	float	m_Fov;

	float	m_RollSpeed;
	float	m_RollOffset;

	bool	m_ShouldResetRoll;

	//NOTE: Bit fields cannot be used with RAG widgets.
	bool	m_ShouldInvertLook;
	bool	m_ShouldSwapPadSticks;
	bool	m_ShouldLoadNearbyIpls;

	// Depth of field tuning
	float	m_DofNearAtDefaultFov;
	float	m_DofFarAtDefaultFov;
	float	m_DofNearAtMaxZoom;
	float	m_DofFarAtMaxZoom;
	float	m_DofNearAtMinZoom;
	float	m_DofFarAtMinZoom;

	static camDepthOfFieldSettingsMetadata ms_DofSettings;
	static s32		ms_LensOption;
	static s32		ms_FocusMode;
	static float	ms_FocalPointChangeSpeed;
	static float	ms_FocalPointMaxSpeed;
	static float	ms_FocalPointAcceleration;
	static float	ms_MaxDistanceToTestForEntity;
	static float	ms_ManualFocusDistance;
	static float	ms_OverriddenFoV;
	static float	ms_FocusEntityBoundRadiusScaling;
	static bool		ms_ShouldKeepFocusEntityBoundsInFocus;

private:
	//Forbid copy construction and assignment.
	camMarketingFreeCamera(const camMarketingFreeCamera& other);
	camMarketingFreeCamera& operator=(const camMarketingFreeCamera& other);
};

#endif // __BANK

#endif // MARKETING_FREE_CAMERA_H
