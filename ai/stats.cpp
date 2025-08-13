//
// ai/stats.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "ai/stats.h"

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------

namespace AIStats
{
	PF_PAGE(AIVarious, "AI Various");
	PF_GROUP(Functions);
	PF_GROUP(Systems);
	PF_LINK(AIVarious, Functions);
	PF_LINK(AIVarious, Systems);

	PF_TIMER(AddVehicleToJunction, Functions);
	PF_TIMER(AdjustAngularVelocityForIntroClip, Functions);
	PF_TIMER(ApplyOverridesForEntity, Functions);
	PF_TIMER(CanEventBeIgnoredBecauseOfInteriorStatus, Functions);
	PF_TIMER(CalculatePopulationRangeScale, Functions);
	PF_TIMER(ComputeHeadingAndPitch, Functions);
	PF_TIMER(CPedAILodManager_Update, Functions);
	PF_TIMER(CPedGeometryAnalyser_IsInAir, Functions);
	PF_TIMER(CreateScenarioPed, Functions);
	PF_TIMER(CScenarioPointRegion_ParserPostLoad, Functions);
	PF_TIMER(DoPathSearch, Functions);
	PF_TIMER(FindNodeClosestInRegion1, Functions);
	PF_TIMER(FindNodeClosestInRegion2, Functions);
	PF_TIMER(FindNodePairToMatchVehicle, Functions);
	PF_TIMER(FindPointsInArea, Functions);
	PF_TIMER(FindPointsInArea_Region, Functions);
	PF_TIMER(FindPointsInArea_Entity, Functions);
	PF_TIMER(NR_SP_SpatialArrayUpdate, Functions);
	PF_TIMER(FindSmallestForPosition, Functions);
	PF_TIMER(GetPedTypeForScenario, Functions);
	PF_TIMER(HandleDriveToGetup, Functions);
	PF_TIMER(IsParkingSpaceFreeFromObstructions, Functions);
	PF_TIMER(IsSafeToSwitchToFullPhysics, Functions);
	PF_TIMER(PickClipFromSet, Functions);
	PF_TIMER(PickClipUsingPointCloud, Functions);
	PF_TIMER(ProcessSlopesAndStairs, Functions);
	PF_TIMER(RecordNodesClosestToCoors, Functions);
	PF_TIMER(ScanAndInstanceJunctionsNearOrigin, Functions);
	PF_TIMER(SimulatePhysicsForLowLod, Functions);
	PF_TIMER(ScenarioExitHelperSubmitAsyncProbe, Functions);
	PF_TIMER(UpdateAnimationVFX, Functions);
	PF_TIMER(UpdateLinksAfterGather, Functions);
	PF_TIMER(UpdateRecklessDriving, Functions);

	PF_TIMER(CScenarioChainTests, Systems);
	PF_TIMER(CScenarioEntityOverrideVerification, Systems);
	PF_TIMER(UpdatePedNavMeshTracking, Systems);

}	// namespace AIStats

//-----------------------------------------------------------------------------

// End of file 'ai/stats.cpp'
