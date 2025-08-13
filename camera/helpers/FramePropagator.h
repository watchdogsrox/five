//
// camera/helpers/FramePropagator.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FRAME_PROPAGATOR_H
#define FRAME_PROPAGATOR_H

#include "camera/helpers/Frame.h"

class camBaseCamera;
class camNearClipScanner;
class CViewport;

const float g_CutsceneOverrideFarDof = -1000.0f; //TODO: Remove this hack.

//A helper for the final stage of camera frame processing and propagation to the viewport.
class camFramePropagator
{
public:
	camFramePropagator();
	~camFramePropagator() {}

	void	Init();
	void	Shutdown();

	const camNearClipScanner* GetNearClipScanner() const { return m_NearClipScanner; }

	void	DisableNearClipScanThisUpdate() { m_ShouldDisableNearClipScanThisUpdate = true; }

	bool	Propagate(camFrame& sourceFrame, const camBaseCamera* activeCamera, CViewport& destinationViewport);

	bool	ComputeAndPropagateRevisedFarClipFromTimeCycle(camFrame& sourceFrame, CViewport& destinationViewport);

#if __BANK
	bool	ComputeDebugOutputText(char* string, u32 maxLength) const;
	void	AddWidgets(bkBank& bank);
#endif

private:
	bool	ComputeIsFrameValid(const camFrame& frame, const camBaseCamera* activeCamera);
	void	UpdateFarClipFromTimeCycle(camFrame& frame);
	const CEntity* ComputeAttachEntityForViewport(const camFrame& sourceFrame, const camBaseCamera* activeCamera, bool& wasEntityInvalidDueToNoLos) const;

	camNearClipScanner*	m_NearClipScanner;
	u32		m_NumUpdatesBypassed;
	bool	m_ShouldDisableNearClipScanThisUpdate				: 1;

#if __BANK
	Vector4	m_OverriddenDofPlanes;
	Vector2	m_OverriddenDofFocusDistanceGridScaling;
	Vector2	m_OverriddenDofSubjectMagPowerNearFar;
	float	m_OverriddenNearClip;
	float	m_OverriddenFarClip;
	float	m_OverriddenDofStrength;
	float	m_OverriddenFov;
	float	m_OverriddenMotionBlurStrength;
	float	m_OverriddenDofMaxPixelDepth;
	float	m_OverriddenDofMaxPixelDepthBlendLevel;
	float	m_OverriddenDofPixelDepthPowerFactor;
	float	m_OverriddenFNumberOfLens;
	float	m_OverriddenFocalLengthMultiplier;
	float	m_OverriddenDofFStopsAtMaxExposure;
	float	m_OverriddenDofFocusDistanceBias;
	float	m_OverriddenDofMaxNearInFocusDistance;
	float	m_OverriddenDofMaxNearInFocusDistanceBlendLevel;
	float	m_OverriddenDofMaxBlurRadiusAtNearInFocusLimit;
	float	m_OverriddenDofBlurDiskRadiusPowerFactorNear;
	float	m_OverriddenDofFocusDistanceIncreaseSpringConstant;
	float	m_OverriddenDofFocusDistanceDecreaseSpringConstant;
	float	m_OverriddenDofPlanesBlendLevel;
	float	m_OverriddenDofFullScreenBlurBlendLevel;
	float	m_OverriddenDofOverriddenFocusDistance;
	float	m_OverriddenDofOverriddenFocusDistanceBlendLevel;
	bool	m_OverriddenUseHighQualityDofState;
	bool	m_OverriddenUseShallowDofState;

	//NOTE: Bit fields are not supported by RAG widgets.
	bool	m_ShouldOverrideDofPlanes[camFrame::NUM_DOF_PLANES];
	bool	m_ShouldOverrideDofFocusDistanceGridScaling;
	bool	m_ShouldOverrideDofSubjectMagPowerNearFar;
	bool	m_ShouldOverrideNearClip;
	bool	m_ShouldOverrideFarClip;
	bool	m_ShouldOverrideDofStrength;
	bool	m_ShouldOverrideUseHighQualityDof;
	bool	m_ShouldOverrideUseShallowDof;
	bool	m_ShouldOverrideFov;
	bool	m_ShouldOverrideMotionBlurStrength;
	bool	m_ShouldOverrideDofMaxPixelDepth;
	bool	m_ShouldOverrideDofMaxPixelDepthBlendLevel;
	bool	m_ShouldOverrideDofPixelDepthPowerFactor;
	bool	m_ShouldOverrideFNumberOfLens;
	bool	m_ShouldOverrideFocalLengthMultiplier;
	bool	m_ShouldOverrideDofFStopsAtMaxExposure;
	bool	m_ShouldOverrideDofFocusDistanceBias;
	bool	m_ShouldOverrideDofMaxNearInFocusDistance;
	bool	m_ShouldOverrideDofMaxNearInFocusDistanceBlendLevel;
	bool	m_ShouldOverrideDofMaxBlurRadiusAtNearInFocusLimit;
	bool	m_ShouldOverrideDofBlurDiskRadiusPowerFactorNear;
	bool	m_ShouldOverrideDofFocusDistanceIncreaseSpringConstant;
	bool	m_ShouldOverrideDofFocusDistanceDecreaseSpringConstant;
	bool	m_ShouldOverrideDofPlanesBlendLevel;
	bool	m_ShouldOverrideDofFullScreenBlurBlendLevel;
	bool	m_ShouldOverrideDofOverriddenFocusDistance;
	bool	m_ShouldOverrideDofOverriddenFocusDistanceBlendLevel;

	bool	m_ShouldDisplayViewportAttachEntityStatus;

	bool	m_DebugHasAttachEntityForViewport;
	bool	m_DebugWasAttachEntityInvalidDueToNoLos;
#endif // __BANK

	static bank_float ms_MinMotionBlurStrength;
};

#endif // FRAME_PROPAGATOR_H
