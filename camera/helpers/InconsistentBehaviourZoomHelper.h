//
// camera/helpers/InconsistentBehaviourZoomHelper.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef INCONSISTENT_BEHAVIOUR_ZOOM_HELPER_H
#define INCONSISTENT_BEHAVIOUR_ZOOM_HELPER_H

#include "camera/base/BaseObject.h"
#include "vector/vector3.h"

class camFrame;
class camInconsistentBehaviourZoomHelperMetadata; 
class CEntity;

class camInconsistentBehaviourZoomHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camInconsistentBehaviourZoomHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camInconsistentBehaviourZoomHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

public:
	bool	UpdateShouldReactToInconsistentBehaviour(const CEntity* lookAtTarget, const camFrame& frame, const bool hasLosToTarget);
	float	GetBoundsRadiusScale() const;
	float	GetMinFovDifference() const;
	float	GetMaxFov() const;
	u32		GetZoomDuration() const;

#if __BANK
	static void	AddWidgets(bkBank &bank);
#endif // __BANK

private:
	camInconsistentBehaviourZoomHelper(const camBaseObjectMetadata& metadata);

	bool	CalculateIsTargetBehavingInconsistentlyThisFrame(const CEntity* lookAtTarget, const camFrame& frame, const bool hasLosToTarget);

private:
	const camInconsistentBehaviourZoomHelperMetadata& m_Metadata;

	Vector3 m_VehicleVelocityOnPreviousUpdate;

	u32		m_TimeAtInconsistentBehaviour;
	u32		m_TimeElapsedSinceVehicleAirborne;
	u32		m_TimeElapsedSinceLostLos;
	u32		m_ReactionTime;
	u32		m_MinZoomDuration;
	u32		m_MaxZoomDuration;
	u32		m_NumUpdatesPerformed;

	float	m_HeadingOnPreviousUpdate;
	float	m_PitchOnPreviousUpdate;
	float	m_BoundsRadiusScaleToUse;

	bool	m_WasReactingToInconsistencyLastUpdate;

#if __BANK
	static float ms_DebugMinZoomDuration;
	static float ms_DebugMaxZoomDuration;
	static float ms_DebugReactionTime;
	static float ms_DebugBoundsRadiusScale;
	static bool ms_DebugShouldOverrideIsInconsistent;
	static bool ms_DebugIsInconsistent;
#endif // __BANK

private:
	//Forbid copy construction and assignment.
	camInconsistentBehaviourZoomHelper(const camInconsistentBehaviourZoomHelper& other);
	camInconsistentBehaviourZoomHelper& operator=(const camInconsistentBehaviourZoomHelper& other);
};

#endif //INCONSISTENT_BEHAVIOUR_ZOOM_HELPER
