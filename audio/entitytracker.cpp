//
// audio/entitytracker.cpp
//
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//

#include "entitytracker.h"

// Rage headers
#include "vector/matrix34.h"

// Game headers
#include "scene/Entity.h"
#include "debugaudio.h"

AUDIO_OPTIMISATIONS()

const Vector3 audPlaceableTracker::GetPosition() const
{
	naAssertf(m_ParentPlaceable, "audPlaceableTracker doesn't have a valid parent");
	return VEC3V_TO_VECTOR3(m_ParentPlaceable->GetTransform().GetPosition());
}

audCompressedQuat audPlaceableTracker::GetOrientation() const
{
	// Converting from the parent's matrix to quaternion is a slow operation, so we don't want
	// to re-calculate it every time someone queries the orientation. Therefore we store it as a member variable
	// and simply return that.
	return m_Orientation;
}

void audPlaceableTracker::CalculateOrientation()
{
	// Recalculate the orientation quaternion. Ideally this function should be called once per
	// frame by the owner of the tracker.
	m_Orientation.Set(QuatVFromMat34V(m_ParentPlaceable->GetMatrix()));
}