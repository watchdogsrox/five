//
// name:		NetBlenderBoat.cpp
// written by:	Daniel Yelland
// description:	Network blender used by boats

#include "network/objects/prediction/NetBlenderBoat.h"

#include "network/network.h"
#include "network/objects/entities/NetObjBoat.h"
#include "renderer/Water.h"
#include "vehicles/boat.h"

CompileTimeAssert(sizeof(CNetBlenderBoat) <= LARGEST_NET_PHYSICAL_BLENDER_CLASS);

NETWORK_OPTIMISATIONS()

// ===========================================================================================================================
// CNetBlenderBoat
// ===========================================================================================================================
CNetBlenderBoat::CNetBlenderBoat(CBoat *pBoat, CBoatBlenderData *pBlendData) :
CNetBlenderVehicle(pBoat, pBlendData)
{
	m_BypassNextWaterOffsetCheck = true;
}

CNetBlenderBoat::CNetBlenderBoat(CVehicle *pBoat, CVehicleBlenderData *pBlendData) :
	CNetBlenderVehicle(pBoat, pBlendData)
{
	m_BypassNextWaterOffsetCheck = true;
}

CNetBlenderBoat::~CNetBlenderBoat()
{
}

//
// Name			:	Update
// Purpose		:	Performs the blend and returns true if there was any change to the object
void CNetBlenderBoat::Update()
{
    if(GetPhysical() && GetPhysical()->GetIsTypeVehicle())
    {
        if(static_cast<CVehicle *>(GetPhysical())->GetVehicleType() == VEHICLE_TYPE_BOAT)
        {
            CNetObjBoat *netObjBoat = static_cast<CNetObjBoat *>(GetPhysical()->GetNetworkObject());

            if(netObjBoat && netObjBoat->IsAnchored())
            {
				UseLogarithmicBlendingThisFrame();
                m_BlendLinInterpStartTime = NetworkInterface::GetNetworkTime();
                m_blendPhysicalStartTime  = NetworkInterface::GetNetworkTime();
                m_BlendingPosition       = true;
                m_bBlendingOrientation    = true;
            }
        }
    }

    CNetBlenderVehicle::Update();
}

bool CNetBlenderBoat::DisableZBlending() const
{
	const CVehicle* vehicle = static_cast< const CVehicle* >( GetPhysical() );

    if( vehicle &&
		vehicle->GetBoatHandling() )
    {
        CNetObjVehicle *netObjBoat = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());

        bool locallyInWater  = vehicle->GetBoatHandling()->IsInWater();
        bool remotelyInWater = netObjBoat && netObjBoat->IsNetworkObjectInWater();

        if(locallyInWater || remotelyInWater)
        {
            return true;
        }
    }

    return false;
}

bool CNetBlenderBoat::DoNoZBlendHeightCheck(float heightDelta) const
{
    static const float BOAT_Z_POP_DIST = 100.0f;

    if(fabs(heightDelta) >= BOAT_Z_POP_DIST)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool CNetBlenderBoat::SetPositionOnObject(const Vector3 &position, bool warp)
{
    Vector3 newPosition = position;

    // for boats in water try to keep the same offset from the water level after the warp
    if(warp && DisableZBlending() && m_BypassNextWaterOffsetCheck == false)
    {
        Vector3 currentPosition = GetPositionFromObject();

        float waterLevel = currentPosition.z;

        if(Water::GetWaterLevel(currentPosition, &waterLevel, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
        {
            float zDiff = currentPosition.z - waterLevel;

            if(Water::GetWaterLevel(newPosition, &waterLevel, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
            {
                newPosition.z = waterLevel + zDiff;
            }
        }
    }

	m_BypassNextWaterOffsetCheck = false;
	    
	return CNetBlenderVehicle::SetPositionOnObject(newPosition, warp);
}
