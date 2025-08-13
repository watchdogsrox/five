//
// name:		NetBlenderHeli.h
// written by:	Daniel Yelland
// description:	Network blender used by helicopters (and planes)

#ifndef NETBLENDERHELI_H
#define NETBLENDERHELI_H

// game includes
#include "network/objects/prediction/NetBlenderVehicle.h"

// ===========================================================================================================================
// CHeliBlenderData
// ===========================================================================================================================

struct CHeliBlenderData : public CVehicleBlenderData
{
    CHeliBlenderData()
    {
        m_UseBlendVelSmoothing                   = true;
        m_BlendVelSmoothTime                     = 150;   // The time in ms to base blender behaviour over (looks at where the object will be this time in the future)
        m_PredictAcceleration                    = true;  // indicates whether the blender should predict acceleration of the target object

        // low speed mode adjustments need to less severe for helis and planes
        m_LowSpeedMode.m_SmallVelocitySquared   = 1.0f; // velocity change for which we don't try to correct
        m_LowSpeedMode.m_MaxVelDiffFromTarget   = 3.0f; // the maximum velocity difference from the last received velocity allowed

        // helis and planes can accelerate and travel at much highers speeds than other vehicles, so we need
        // to be more forgiving about differences
        m_HighSpeedMode.m_PositionDeltaMaxLow    = 10.0f; // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 10.0f; // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 10.0f; // the position delta before a pop for objects being updated at a high update rate
    }

	const char *GetName() const { return "HELI"; }
};

// ===========================================================================================================================
// CNetBlenderHeli
// ===========================================================================================================================
class CVehicle;

class CNetBlenderHeli : public CNetBlenderVehicle
{
public:

	CNetBlenderHeli(CVehicle *vehicle, CHeliBlenderData *blenderData);
	~CNetBlenderHeli();
};

#endif  // NETBLENDERPHYSICAL_H
