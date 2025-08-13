#ifndef VEHICLE_INTELLIGENCE_FACTORY_H
#define VEHICLE_INTELLIGENCE_FACTORY_H

#include "Debug\Debug.h"

class CVehicle;
class CVehicleIntelligence;

class CVehicleIntelligenceFactory
{
public:

	CVehicleIntelligenceFactory();
	virtual ~CVehicleIntelligenceFactory();

	//Instantiate the factory that will be used to 
	//instantiate CPedIntelligence's of the correct type.
	static void CreateFactory()
	{
		Assert(0==sm_pInstance);
		sm_pInstance=rage_new CVehicleIntelligenceFactory;
	}

	//Return the instance of the factory that will 
	//instantiate CPedIntelligence's of the correct type.
	static CVehicleIntelligenceFactory* GetFactory() 
	{
		Assert(sm_pInstance);
		return sm_pInstance;
	}

	static void DestroyFactory()
	{
		Assert(sm_pInstance);
		if(sm_pInstance) delete sm_pInstance;
		sm_pInstance=0;
	}

	//Instantiate a CPedIntelligence of the correct type.
	CVehicleIntelligence* Create(CVehicle* pVehicle);

protected:

	static CVehicleIntelligenceFactory* sm_pInstance;
};

#endif
