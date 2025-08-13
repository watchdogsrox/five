
#include "CalcAmbientScaleAsync.h"

#include "atl/array.h"
#include "system/interlocked.h"

#include "timecycle/tcmodifier.h"

#include "peds/ped.h"
#include "scene/Entity.h"

bool CalcAmbientScaleAsync::RunFromDependency(const sysDependency& dependency)
{
	atArray<CEntity*>& aNonPedEntities = *static_cast<atArray<CEntity*>*>(dependency.m_Params[0].m_AsPtr);
	atArray<CalcAmbientScalePedData>& aPedEntities = *static_cast<atArray<CalcAmbientScalePedData>*>(dependency.m_Params[1].m_AsPtr);
	u32* pDependencyRunningEa	= static_cast< u32* >( dependency.m_Params[2].m_AsPtr );

	int numNonPeds = aNonPedEntities.GetCount();
	int numPeds = aPedEntities.GetCount();

	for(int i = 0; i < numNonPeds; i++)
	{
		aNonPedEntities[i]->CalculateDynamicAmbientScales();
	}

	for(int i = 0; i < numPeds; i++)
	{
		aPedEntities[i].m_pPed->CalculateDynamicAmbientScalesWithVehicleScaler(aPedEntities[i].m_incaredNess);
	}

	sysInterlockedExchange(pDependencyRunningEa, 0);

	return true;
}