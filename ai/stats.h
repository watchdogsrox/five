//
// ai/stats.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef AI_STATS_H
#define AI_STATS_H

#include "profile/profiler.h"

//-----------------------------------------------------------------------------

namespace AIStats
{
	EXT_PF_TIMER(AddVehicleToJunction);
	EXT_PF_TIMER(AdjustAngularVelocityForIntroClip);
	EXT_PF_TIMER(ApplyOverridesForEntity);
	EXT_PF_TIMER(CanEventBeIgnoredBecauseOfInteriorStatus);
	EXT_PF_TIMER(CalculatePopulationRangeScale);
	EXT_PF_TIMER(ComputeHeadingAndPitch);
	EXT_PF_TIMER(CPedAILodManager_Update);
	EXT_PF_TIMER(CPedGeometryAnalyser_IsInAir);
	EXT_PF_TIMER(CreateScenarioPed);
	EXT_PF_TIMER(CScenarioPointRegion_ParserPostLoad);
	EXT_PF_TIMER(DoPathSearch);
	EXT_PF_TIMER(FindNodeClosestInRegion1);
	EXT_PF_TIMER(FindNodeClosestInRegion2);
	EXT_PF_TIMER(FindNodePairToMatchVehicle);
	EXT_PF_TIMER(FindPointsInArea);
	EXT_PF_TIMER(FindPointsInArea_Region);
	EXT_PF_TIMER(FindPointsInArea_Entity);
	EXT_PF_TIMER(NR_SP_SpatialArrayUpdate);
	EXT_PF_TIMER(FindSmallestForPosition);
	EXT_PF_TIMER(GetPedTypeForScenario);
	EXT_PF_TIMER(HandleDriveToGetup);
	EXT_PF_TIMER(IsParkingSpaceFreeFromObstructions);
	EXT_PF_TIMER(IsSafeToSwitchToFullPhysics);
	EXT_PF_TIMER(PickClipFromSet);
	EXT_PF_TIMER(PickClipUsingPointCloud);
	EXT_PF_TIMER(ProcessSlopesAndStairs);
	EXT_PF_TIMER(RecordNodesClosestToCoors);
	EXT_PF_TIMER(ScanAndInstanceJunctionsNearOrigin);
	EXT_PF_TIMER(SimulatePhysicsForLowLod);
	EXT_PF_TIMER(ScenarioExitHelperSubmitAsyncProbe);
	EXT_PF_TIMER(UpdateAnimationVFX);
	EXT_PF_TIMER(UpdateLinksAfterGather);
	EXT_PF_TIMER(UpdateRecklessDriving);

	EXT_PF_TIMER(CScenarioChainTests);
	EXT_PF_TIMER(CScenarioEntityOverrideVerification);
	EXT_PF_TIMER(UpdatePedNavMeshTracking);

}	// namespace AIStats

//-----------------------------------------------------------------------------

#endif	// AI_STATS_H

// End of file 'ai/stats.h'
