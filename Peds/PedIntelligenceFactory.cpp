#include "Peds\PedIntelligenceFactory.h"
#include "PedIntelligence.h"

CPedIntelligenceFactory* CPedIntelligenceFactory::sm_pInstance=0;

CPedIntelligenceFactory::CPedIntelligenceFactory()
{
	CPedIntelligence::InitPool( MAXNOOFPEDS, MEMBUCKET_GAMEPLAY );
}

CPedIntelligenceFactory::~CPedIntelligenceFactory()
{
	CPedIntelligence::ShutdownPool();
}


//Instantiate a CPedIntelligence of the correct type.
CPedIntelligence* CPedIntelligenceFactory::Create(CPed* pPed)
{
	CPedIntelligence* p=rage_new CPedIntelligence(pPed);
	return p;
}
