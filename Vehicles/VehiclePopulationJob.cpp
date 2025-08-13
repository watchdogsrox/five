// Vehicle population SPU update
// flavius.alecu: 25/10/2011

#include "math/simpleMath.h"

#include "vehiclepopulation.h"

#include "BaseTypes.h"

#include "renderer\Renderer.h"			// JW- had to move this up here to get a successful compile...

#include "grcore/viewport.h"
#include "grcore/viewport_inline.h"
#include "VehiclePopulationAsync.h"
#include "vector/Vector4.h"
#include "scene/world/gameWorld.h"
#include "system/timer.h"
#include "fwscene/world/WorldLimits.h"

bool VehiclePopulationAsyncUpdate::RunFromDependency(const sysDependency& dependency)
{
	CVehiclePopulationAsyncUpdateData* pJobParams = static_cast<CVehiclePopulationAsyncUpdateData*>(dependency.m_Params[0].m_AsPtr);
	u32* outNumGenLinks = static_cast<u32*>(dependency.m_Params[1].m_AsPtr);
	u32* pDependenciesRunningEa	= static_cast<u32*>(dependency.m_Params[2].m_AsPtr);

	CVehGenSphere * apOverlappingAreas = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetElements();

	CVehiclePopulation::GatherPotentialVehGenLinks(
		pJobParams->popCtrlCentre,
		pJobParams->sidewallFrustumOffset,
		pJobParams->maxNodeDistBase,
		pJobParams->maxNodeDist,
		pJobParams->m_bUseKeyholeShape,
		pJobParams->m_fKeyhole_InViewInnerRadius,
		pJobParams->m_fKeyhole_InViewOuterRadius,
		pJobParams->m_fKeyhole_BehindInnerRadius,
		pJobParams->m_fKeyhole_BehindOuterRadius,
		pJobParams->popCtrlDir,
		pJobParams->m_fKeyhole_CosHalfFOV,
		*outNumGenLinks, 
		pJobParams->p_aGenerationLinks,
		*pJobParams->m_pAverageLinkHeight,
		apOverlappingAreas,
		CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount());

	sysInterlockedDecrement(pDependenciesRunningEa);

	return true;
}
