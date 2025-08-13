#include "vehicleAI\VehicleIntelligenceFactory.h"
#include "VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"

CVehicleIntelligenceFactory* CVehicleIntelligenceFactory::sm_pInstance=0;

CVehicleIntelligenceFactory::CVehicleIntelligenceFactory()
{
	CVehicleIntelligence::InitPool( MEMBUCKET_GAMEPLAY );
	CTaskVehicleSerialiserBase::InitPool( MEMBUCKET_GAMEPLAY );
}

CVehicleIntelligenceFactory::~CVehicleIntelligenceFactory()
{
	CVehicleIntelligence::ShutdownPool();
}


//Instantiate a CVehicleIntelligence of the correct type.
CVehicleIntelligence* CVehicleIntelligenceFactory::Create(CVehicle* pVehicle)
{
	CVehicleIntelligence* p = NULL;

	if(pVehicle->InheritsFromHeli())
		p=rage_new CHeliIntelligence(pVehicle);
	else if (pVehicle->InheritsFromPlane())
		p=rage_new CPlaneIntelligence(pVehicle);
	else
		p=rage_new CVehicleIntelligence(pVehicle);

	return p;
}
