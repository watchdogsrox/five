#ifndef PED_INTELLIGENCE_FACTORY_H
#define PED_INTELLIGENCE_FACTORY_H

#include "Debug\Debug.h"

class CPed;
class CPedIntelligence;

class CPedIntelligenceFactory
{
public:

	CPedIntelligenceFactory();
	virtual ~CPedIntelligenceFactory();

	//Instantiate the factory that will be used to 
	//instantiate CPedIntelligence's of the correct type.
	static void CreateFactory()
	{
		Assert(0==sm_pInstance);
		sm_pInstance=rage_new CPedIntelligenceFactory;
	}

	//Return the instance of the factory that will 
	//instantiate CPedIntelligence's of the correct type.
	static CPedIntelligenceFactory* GetFactory() 
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
	CPedIntelligence* Create(CPed* pPed);

protected:

	static CPedIntelligenceFactory* sm_pInstance;
};

#endif
