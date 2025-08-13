#include "Task/Vehicle/HelperFunctions.h"

#if ENABLE_HORSE

#include "Peds/ped.h"
#include "Peds/PedMotionData.h"
#include "Peds/PlayerInfo.h"
#include "scene/Physical.h"
#include "script/script.h"
#include "Task/Vehicle/PlayerHorseFollowTargets.h"
#include "Task/Vehicle/PlayerHorseSpeedMatch.h"
#include "Peds\Horse\horseComponent.h"

AI_OPTIMISATIONS()

namespace HelperFunctions
{
	const float HORSE_MBR_STILL		= 0.0f;
	const float HORSE_MBR_WALK		= 0.25f;
	const float HORSE_MBR_TROT		= 0.5f;
	const float HORSE_MBR_RUN		= 0.75f;
	const float HORSE_MBR_GALLOP	= 1.0f;

	const float HORSE_MBR_STILL_WALK	= 0.5f * (HORSE_MBR_STILL	+ HORSE_MBR_WALK);
	const float HORSE_MBR_WALK_TROT		= 0.5f * (HORSE_MBR_WALK	+ HORSE_MBR_TROT);
	const float HORSE_MBR_TROT_RUN		= 0.5f * (HORSE_MBR_TROT	+ HORSE_MBR_RUN);
	const float HORSE_MBR_RUN_GALLOP	= 0.5f * (HORSE_MBR_RUN		+ HORSE_MBR_GALLOP);
}

void HelperFunctions::Find3ActorsWithinRange(const CPed& actor, const Vector3 &position, float range, bhSpatialArrayActorTypes actorTypes, bhCreatureInfo* creaturesOut)
{
	const float cullDist = range;

	for(int i = 0; i < 3; i++)
	{
		creaturesOut[i].Dist2 = cullDist*cullDist;
	}

	u32 typeFlagsToCareAbout;
	u32 typeFlagValues;

	if (actorTypes == kTypeFlagHorse)
	{
		typeFlagsToCareAbout = CPed::kSpatialArrayTypeFlagHorse;
		typeFlagValues = CPed::kSpatialArrayTypeFlagHorse;
	}
	else
	{
		typeFlagsToCareAbout = 0;
		typeFlagValues = 0;
	}

	CSpatialArrayNode *closestNodes[3];

	const CSpatialArrayNode *excl1 = (actorTypes == kTypeFlagVehicle) ? NULL : &actor.GetSpatialArrayNode();
	const CSpatialArrayNode *excl2;
	
	if (actorTypes == kTypeFlagVehicle)
	{
		//don't look for our own vehicle
		const CVehicle *extraExclActor = actor.GetVehiclePedInside();

		if(extraExclActor)
		{
			excl2 = &extraExclActor->GetSpatialArrayNode();
		}
		else
		{
			excl2 = NULL;
		}

		const int numFound = CVehicle::ms_spatialArray->FindClosest3(VECTOR3_TO_VEC3V(position),
			closestNodes, 3, typeFlagsToCareAbout, typeFlagValues, excl1, excl2, cullDist);
		creaturesOut[0].Actor = NULL;
		creaturesOut[1].Actor = NULL;
		creaturesOut[2].Actor = NULL;

		for(int i = 0; i < numFound; i++)
		{
			CVehicle& actor2 = *CVehicle::GetVehicleFromSpatialArrayNode(closestNodes[i]);

			const Matrix34 &mtrx = MAT34V_TO_MATRIX34(actor2.GetMatrix());
			const float dist2 = mtrx.d.Dist2(position);
			creaturesOut[i].Actor = &actor2;
			creaturesOut[i].Matrix = &mtrx;
			creaturesOut[i].Dist2 = dist2;
		}
	}
	else
	{
		//don't look for our own horse
		const CPed *extraExclActor = actor.GetMountPedOn();

		if(extraExclActor)
		{
			excl2 = &extraExclActor->GetSpatialArrayNode();
		}
		else
		{
			excl2 = NULL;
		}

		const int numFound = CPed::ms_spatialArray->FindClosest3(VECTOR3_TO_VEC3V(position),
			closestNodes, 3, typeFlagsToCareAbout, typeFlagValues, excl1, excl2, cullDist);
		creaturesOut[0].Actor = NULL;
		creaturesOut[1].Actor = NULL;
		creaturesOut[2].Actor = NULL;

		for(int i = 0; i < numFound; i++)
		{
			CPed& actor2 = *CPed::GetPedFromSpatialArrayNode(closestNodes[i]);

			const Matrix34 &mtrx = MAT34V_TO_MATRIX34(actor2.GetMatrix());
			const float dist2 = mtrx.d.Dist2(position);
			creaturesOut[i].Actor = &actor2;
			creaturesOut[i].Matrix = &mtrx;
			creaturesOut[i].Dist2 = dist2;
		}
	}
}


void HelperFunctions::Find4ActorsWithinRange(const CPed& actor, const Vector3 &position, float range, bhSpatialArrayActorTypes actorTypes, bhCreatureInfo* creaturesOut)
{
	const float cullDist = range;

	for(int i = 0; i < 4; i++)
	{
		creaturesOut[i].Dist2 = cullDist*cullDist;
	}

	u32 typeFlagsToCareAbout;
	u32 typeFlagValues;

	if (actorTypes == kTypeFlagHorse)
	{
		typeFlagsToCareAbout = CPed::kSpatialArrayTypeFlagHorse;
		typeFlagValues = CPed::kSpatialArrayTypeFlagHorse;
	}
	else
	{
		typeFlagsToCareAbout = 0;
		typeFlagValues = 0;
	}

	CSpatialArrayNode *closestNodes[4];

	const CSpatialArrayNode *excl1 = (actorTypes == kTypeFlagVehicle) ? NULL : &actor.GetSpatialArrayNode();
	const CSpatialArrayNode *excl2;


	if (actorTypes == kTypeFlagVehicle)
	{
		//don't look for our own vehicle
		const CVehicle *extraExclActor = actor.GetVehiclePedInside();

		if(extraExclActor)
		{
			excl2 = &extraExclActor->GetSpatialArrayNode();
		}
		else
		{
			excl2 = NULL;
		}

		const int numFound = CVehicle::ms_spatialArray->FindClosest4(VECTOR3_TO_VEC3V(position),
			closestNodes, 4, typeFlagsToCareAbout, typeFlagValues, excl1, excl2, cullDist);
		creaturesOut[0].Actor = NULL;
		creaturesOut[1].Actor = NULL;
		creaturesOut[2].Actor = NULL;
		creaturesOut[3].Actor = NULL;

		for(int i = 0; i < numFound; i++)
		{
			CVehicle& actor2 = *CVehicle::GetVehicleFromSpatialArrayNode(closestNodes[i]);

			const Matrix34 &mtrx = MAT34V_TO_MATRIX34(actor2.GetMatrix());
			const float dist2 = mtrx.d.Dist2(position);
			creaturesOut[i].Actor = &actor2;
			creaturesOut[i].Matrix = &mtrx;
			creaturesOut[i].Dist2 = dist2;
		}
	}
	else
	{
		//don't look for our own horse
		const CPed *extraExclActor = actor.GetMountPedOn();

		if(extraExclActor)
		{
			excl2 = &extraExclActor->GetSpatialArrayNode();
		}
		else
		{
			excl2 = NULL;
		}

		const int numFound = CPed::ms_spatialArray->FindClosest4(VECTOR3_TO_VEC3V(position),
			closestNodes, 4, typeFlagsToCareAbout, typeFlagValues, excl1, excl2, cullDist);
		creaturesOut[0].Actor = NULL;
		creaturesOut[1].Actor = NULL;
		creaturesOut[2].Actor = NULL;
		creaturesOut[3].Actor = NULL;

		for(int i = 0; i < numFound; i++)
		{
			CPed& actor2 = *CPed::GetPedFromSpatialArrayNode(closestNodes[i]);

			const Matrix34 &mtrx = MAT34V_TO_MATRIX34(actor2.GetMatrix());
			const float dist2 = mtrx.d.Dist2(position);
			creaturesOut[i].Actor = &actor2;
			creaturesOut[i].Matrix = &mtrx;
			creaturesOut[i].Dist2 = dist2;
		}
	}
}

float HelperFunctions::UpdateSpeedMatchSimple(float distToTargetAhead, const CPed* pPed, const CPhysical* pTarget)
{
	if (!pPed || !pTarget)
		return 0.f;

	const CPed* pMount = pPed->GetMountPedOn();
	if (pMount)
	{
		pPed = pMount;
	}

	if (pTarget->GetIsTypePed())
	{
		const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
		const CPed* pTargetPedMount = pTargetPed->GetMountPedOn();
		if (pTargetPedMount)
		{
			pTarget = pTargetPedMount;
		}
	}

	const float fSpeed = Mag(VECTOR3_TO_VEC3V(pPed->GetVelocity())).Getf();
	const float fTargetSpeed = Mag(VECTOR3_TO_VEC3V(pTarget->GetVelocity())).Getf();

	TUNE_GROUP_FLOAT(HORSE_FOLLOW, K_P,		1.f,	0.f, 100.f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_FOLLOW, K_D,		1.f,	0.f, 100.f, 0.1f);
	float delta = K_P * distToTargetAhead + K_D * (fTargetSpeed - fSpeed);

	return fSpeed + delta;
}

float HelperFunctions::ComputeNormalizedSpeed(const CPed* pPed, float fSpeed)
{
	Assert(pPed);
	if (pPed)
	{
		const CPed* pMount = pPed->GetMountPedOn();
		if (pMount)
		{
			pPed = pMount;
		}
		const CHorseComponent* pHorseComponent = pPed->GetHorseComponent();
		Assert(pHorseComponent);
		if (pHorseComponent)
		{
			const hrsSpeedControl& horseSpeedControl = pHorseComponent->GetSpeedCtrl();
			const float topSpeed = horseSpeedControl.GetTopSpeed() * horseSpeedControl.GetMaterialSpeedMult();
			const float fNormalizedSpeed = rage::Clamp(fSpeed / topSpeed, 0.f, 1.f);
			return fNormalizedSpeed;
		}
	}

	Assert(false);
	return 0.f;
}

float HelperFunctions::ClampNormalizedSpeed(float fNormalizedSpeed, const CPed* pPed, const CPhysical* pTarget)
{
	Assert(pPed);
	Assert(pTarget);
	Assert(fNormalizedSpeed >= 0.f);
	Assert(fNormalizedSpeed <= 1.f);

	if (pPed && pTarget && pTarget->GetIsTypePed())
	{
		const CPed* pMount = pPed->GetMountPedOn();
		if (pMount)
		{
			pPed = pMount;
		}

		const CHorseComponent* pHorseComponent = pPed->GetHorseComponent();
		if (pHorseComponent)
		{
			const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
			const CPed* pTargetPedMount = pTargetPed->GetMountPedOn();
			if (pTargetPedMount)
			{
				pTargetPed = pTargetPedMount;
			}

			const float fTargetSpeed = ComputeNormalizedSpeed(pTargetPed, Mag(VECTOR3_TO_VEC3V(pTargetPed->GetVelocity())).Getf());

			if (fTargetSpeed >= HORSE_MBR_RUN_GALLOP)
			{
				fNormalizedSpeed = HORSE_MBR_RUN_GALLOP;
			}
			else if (fTargetSpeed >= HORSE_MBR_TROT_RUN)
			{
				fNormalizedSpeed = rage::Clamp(fNormalizedSpeed, HORSE_MBR_TROT_RUN, HORSE_MBR_GALLOP);
			}
			else if (fTargetSpeed >= HORSE_MBR_WALK_TROT)
			{
				fNormalizedSpeed = rage::Clamp(fNormalizedSpeed, HORSE_MBR_WALK_TROT, HORSE_MBR_GALLOP);
			}
			else if (fTargetSpeed >= HORSE_MBR_STILL_WALK)
			{
				fNormalizedSpeed = rage::Clamp(fNormalizedSpeed, HORSE_MBR_STILL_WALK, HORSE_MBR_GALLOP);
			}
			else
			{
				fNormalizedSpeed = rage::Clamp(fNormalizedSpeed, HORSE_MBR_STILL, HORSE_MBR_GALLOP);
			}
		}
	}

	return fNormalizedSpeed;
}

const char* HelperFunctions::GetGaitName(float fNormalizedSpeed)
{
	float& s = fNormalizedSpeed;

	if (s > 1.f) return "flight";
	if (s > HORSE_MBR_RUN_GALLOP) return "gallop";
	if (s == HORSE_MBR_RUN_GALLOP) return "run/gallop";
	if (s > HORSE_MBR_TROT_RUN) return "run";
	if (s == HORSE_MBR_TROT_RUN) return "trot/run";
	if (s > HORSE_MBR_WALK_TROT) return "trot";
	if (s == HORSE_MBR_WALK_TROT) return "walk/trot";
	if (s > HORSE_MBR_STILL_WALK) return "walk";
	if (s == HORSE_MBR_STILL_WALK) return "still/walk";
	if (s >= HORSE_MBR_STILL) return "still";
	return "negative";
}

void HelperFunctions::AddPlayerHorseFollowTarget(int playerIndex, CPhysical* followTgt, float offsX, float offsZ, int followMode, int followPriority, bool disableWhenNotMounted, bool isRidingInPosse)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (Verifyf(pPlayer, "AddPlayerHorseFollowTarget: invalid player index %d", playerIndex))
	{
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pPlayerInfo, "AddPlayerHorseFollowTarget: No player info"))
		{
			CPlayerHorseSpeedMatch* playerInputHorse = pPlayerInfo->GetInputHorse();
			if (Verifyf(playerInputHorse, "AddPlayerHorseFollowTarget: Missing sagPlayerInputHorse"))
			{
				CPlayerHorseFollowTargets &horseFollowTargets = playerInputHorse->GetHorseFollowTargets();

				if (Verifyf(!horseFollowTargets.IsFull(), "AddPlayerHorseFollowTarget: Tried to add too many follow targets"))
				{
					horseFollowTargets.AddTarget(followTgt, offsX, offsZ, followMode, followPriority, disableWhenNotMounted, isRidingInPosse);
				}
			}
		}
	}
}

void HelperFunctions::RemPlayerHorseFollowTarget(int playerIndex, CPhysical* followTgt)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (Verifyf(pPlayer, "RemPlayerHorseFollowTarget: invalid player index %d", playerIndex))
	{
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pPlayerInfo, "RemPlayerHorseFollowTarget: No player info"))
		{
			CPlayerHorseSpeedMatch* playerInputHorse = pPlayerInfo->GetInputHorse();
			if (Verifyf(playerInputHorse, "RemPlayerHorseFollowTarget: Missing sagPlayerInputHorse"))
			{
				CPlayerHorseFollowTargets &horseFollowTargets = playerInputHorse->GetHorseFollowTargets();
				horseFollowTargets.RemoveTarget(followTgt);
			}
		}
	}
}

bool HelperFunctions::TestCoarseMoverBound(const Vector3& vCentroid, Vector3 verts[3], float fCoarseMoverBoundsShift )
{
	// Check coarse mover bounds. If any of the vertexes of the triangle collides with a horse only bound, then mark the polygon as horse bound.
	for ( int i=0; i < 3; ++i)
	{
		// Push the vertexes towards the centroid some amount to account for entity radius if the centroid is not already too close
		Vector3 vTestPoint = vCentroid;
		Vector3 vVertexToCentroid = vCentroid - verts[i];
		float fDistanceSq = vVertexToCentroid.Mag2();
		if ( fDistanceSq < fCoarseMoverBoundsShift * fCoarseMoverBoundsShift )
		{
			vVertexToCentroid.Normalize();
			vTestPoint = verts[i] + vVertexToCentroid * fCoarseMoverBoundsShift * fCoarseMoverBoundsShift;
		}

		// Make a sphere test point for that point.
		WorldProbe::CShapeTestSphereDesc shapeTest;
		WorldProbe::CShapeTestFixedResults<> probeResult;
		shapeTest.SetResultsStructure(&probeResult);
		shapeTest.SetSphere(vTestPoint, 0.01f);
		shapeTest.SetIncludeFlags( 0xFFFFFFFF );
		shapeTest.SetTypeFlags( ArchetypeFlags::GTA_MAP_TYPE_HORSE | ArchetypeFlags::GTA_VEHICLE_BVH_TYPE);
		shapeTest.SetExcludeTypeFlags( ArchetypeFlags::GTA_MAP_TYPE_MOVER );
		if( WorldProbe::GetShapeTestManager()->SubmitTest(shapeTest) )
		{
			return true;
		}
	}

	return false;
}

#endif // ENABLE_HORSE