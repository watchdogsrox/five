//
// camera/switch/SwitchCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SWITCH_CAMERA_H
#define SWITCH_CAMERA_H

#include "vectormath/vec3v.h"

#include "camera/base/BaseCamera.h"
#include "scene/RegdRefTypes.h"

class camSwitchCameraMetadata;
class CPlayerSwitchEstablishingShotMetadata;
class CPlayerSwitchParams;

class camSwitchCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camSwitchCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum eCameraState
	{
		INACTIVE,
		INTRO,
		ASCENT_CUT,
		LOOK_UP,
		LOOK_DOWN,
		PAN,
		DESCENT_CUT,
		ESTABLISHING_SHOT_HOLD,
		ESTABLISHING_SHOT,
		OUTRO_HOLD,
		OUTRO_SWOOP
	};

protected:
	camSwitchCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camSwitchCamera();

	bool			HasBehaviourFinished() const	{ return m_HasBehaviourFinished; }

	u32				GetInterpolateOutDurationToApply() const { return m_InterpolateOutDurationToApply; }

	void			Init(const CPed& sourcePed, const CPed& destinationPed, const CPlayerSwitchParams& switchParams);
	void			OverrideDestination(const Matrix34& worldMatrix);
	void			SetEstablishingShotMetadata(const CPlayerSwitchEstablishingShotMetadata* metadata);
	bool			PerformIntro();
	void			PerformAscentCut(u32 cutIndex);
	void			ComputeFrameForAscentCut(u32 cutIndex, camFrame& frame) const;
	void			ComputeFrameForAscentCeiling(camFrame& frame) const		{ ComputeFrameForAscentCut(m_NumAscentCuts - 1, frame); }
	void			PerformLookUp();
	void			PerformLookDown();
	void			PerformPan(u32 duration);
	void			PerformDescentCut(u32 cutIndex);
	void			ComputeFrameForDescentCut(u32 cutIndex, camFrame& frame) const;
	void			ComputeFrameForDescentCeiling(camFrame& frame) const	{ ComputeFrameForDescentCut(m_NumDescentCuts - 1, frame); }
	void			PerformEstablishingShot();
	bool			ComputeFrameForEstablishingShotStart(camFrame& frame) const;
	bool			ComputeFrameForEstablishingShotEnd(camFrame& frame) const;
	void			PerformOutroHold();
	void			PerformOutroSwoop();
	void			GetTransition(Vec3V_InOut vPos0, Vec3V_InOut vPos1, float& fFov0, float& fFov1) const;
	void			GetTransitionFov(float& fFov0, float& fFov1) const; // faster version which just returns fov

	void			SetDest(const CPed& destinationPed, const CPlayerSwitchParams& switchParams);
#if __BANK
	static void		AddWidgets(bkBank& bank);
#endif // __BANK

protected:
	virtual bool	Update();
	bool			UpdateIntro();
	bool			UpdateAscentCut();
	bool			UpdateLookUp();
	bool			UpdateLookDown();
	bool			UpdatePan();
	bool			UpdateDescentCut();
	bool			UpdateEstablishingShotHold();
	bool			UpdateEstablishingShot();
	bool			UpdateOutroHold();
	bool			UpdateOutroSwoop();
	float			ComputeStartHeading() const;
	void			ComputeAscentCutPosition(u32 cutIndex, Vector3& cutPosition) const;
	void			ComputeDescentCutPosition(u32 cutIndex, Vector3& cutPosition) const;
	float			ComputeStatePhase(u32 duration) const;
	void			ComputeBaseFrame(camFrame& frame, float desiredHeading) const;
	void			ComputeBaseFrameForEstablishingShot(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, camFrame& frame) const;
	void			ComputeWorldMatrixForEstablishingShotStart(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const;
	void			ComputeWorldMatrixForEstablishingShotEnd(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const;
	void			ComputeWorldMatrixForEstablishingShotCatchUp(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const;

	const camSwitchCameraMetadata& m_Metadata;

	Vector3			m_StartPosition;
	Vector3			m_EndPosition;
	const CPlayerSwitchEstablishingShotMetadata* m_EstablishingShotMetadata;
	eCameraState	m_State;
	u32				m_NumAscentCuts;
	u32				m_NumDescentCuts;
	u32				m_StateDuration;
	u32				m_StateStartTime;
	u32				m_InterpolateOutDurationToApply;
	s32				m_ControlFlags;
	float			m_StartHeading;
	float			m_EndHeading;
	float			m_AscentFloorHeight;
	float			m_DescentFloorHeight;
	float			m_CeilingHeight;
	float			m_InitialFovForPan;
	float			m_InitialMotionBlurMaxVelocityScale;
	bool			m_IsInitialised;
	bool			m_ShouldApplyCutBehaviour;
	bool			m_ShouldBlendInCutBehaviour;
	bool			m_HasBehaviourFinished;
	bool			m_WasSourcePedInVehicle;
	bool			m_IsUsingFirstPersonEstablishShot; 
	bool			m_ShouldReportAsFinishedForFPSCutback; 
private:
	//Forbid copy construction and assignment.
	camSwitchCamera(const camSwitchCamera& other);
	camSwitchCamera& operator=(const camSwitchCamera& other);

#if __BANK
	void			DebugOverrideCameraHeading(float& desiredHeading, u32 cutIndex, bool ascent) const;
	float			DebugComputePanDirectionHeading() const;

	static s32		ms_DebugOrientationOption;
#endif // __BANK
};

#endif // SWITCH_CAMERA_H
