//
// name:		NetBlenderBoat.h
// written by:	Daniel Yelland
// description:	Network blender used by boats

#ifndef NETBLENDERBOAT_H
#define NETBLENDERBOAT_H

// game includes
#include "network/objects/prediction/NetBlenderVehicle.h"

// ===========================================================================================================================
// CBoatBlenderData
// ===========================================================================================================================

struct CBoatBlenderData : public CVehicleBlenderData
{
    CBoatBlenderData()
    {
        // low speed mode adjustments need to less severe for boats
        m_LowSpeedMode.m_SmallVelocitySquared   = 1.0f; // velocity change for which we don't try to correct
        m_LowSpeedMode.m_MaxVelDiffFromTarget   = 3.0f; // the maximum velocity difference from the last received velocity allowed
    }

	const char *GetName() const { return "BOAT"; }
	virtual bool IsBoatBlender() { return true; }
};


// ===========================================================================================================================
// CNetBlenderBoat
// ===========================================================================================================================
class CBoat;

class CNetBlenderBoat: public CNetBlenderVehicle
{
public:

	CNetBlenderBoat(CBoat *pBoat, CBoatBlenderData *pBlendData);
	CNetBlenderBoat(CVehicle *pBoat, CVehicleBlenderData *pBlendData);
	~CNetBlenderBoat();

	virtual void Update();

	virtual bool DisableZBlending() const;

private:
	
	virtual bool DoNoZBlendHeightCheck(float heightDelta) const;
    virtual bool SetPositionOnObject(const Vector3 &position, bool warp);

	bool m_BypassNextWaterOffsetCheck;
};

#endif  // NETBLENDERBOAT_H
