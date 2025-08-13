// File header
#include "Peds/PedWeapons/PedTargetEvaluator.h"

//rage headers
#include "math/vecMath.h"

// Game headers
#include "Camera/CamInterface.h"
#if FPS_MODE_SUPPORTED
#include "Camera/gameplay/aim/FirstPersonShooterCamera.h"
#endif
#include "Camera/gameplay/GameplayDirector.h"
#include "Camera/helpers/Frame.h"
#include "Debug/DebugScene.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/UIReticule.h"
#include "Game/Localisation.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Ik/solvers/TorsoSolver.h"
#include "Network/Network.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/Ped_Channel.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Physics/TaskNM.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/movement/TaskGetUp.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Vehicles/VehicleGadgets.h"
#include "vehicles/vehicle.h"
#include "vehicles/Submarine.h"
#include "vehicles/trailer.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "scene/EntityIterator.h"
#include "debug/vectormap.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedTargetEvaluator
//////////////////////////////////////////////////////////////////////////

// Profiling
PF_PAGE(PedTargeting, "PedTargeting");
PF_GROUP(CPedTargetEvaluator);
PF_LINK(PedTargeting, CPedTargetEvaluator);
PF_TIMER(FindTarget, CPedTargetEvaluator);

// Static constants - exposed to RAG
CPedTargetEvaluator::Tunables CPedTargetEvaluator::ms_Tunables;	
IMPLEMENT_PED_TARGET_EVALUATOR_TUNABLES(CPedTargetEvaluator, 0x2c3db397);

bank_bool  CPedTargetEvaluator::CHECK_VEHICLES								= true;
bank_bool  CPedTargetEvaluator::USE_FLAT_HEADING							= false;

dev_float CPedTargetEvaluator::TARGET_DISTANCE_WEIGHTING_DEFAULT			= 0.5f;
dev_float CPedTargetEvaluator::TARGET_HEADING_WEIGHTING_DEFAULT				= 1.0f;
dev_float CPedTargetEvaluator::TARGET_DISANCE_FALLOFF_DEFAULT				= 0.25f;

#if __BANK
bank_u32  CPedTargetEvaluator::DEBUG_TARGETSCORING_MODE						= 0;
bool CPedTargetEvaluator::DEBUG_TARGETSCORING_ENABLED						= false;
bank_bool CPedTargetEvaluator::DEBUG_VEHICLE_LOCKON_TARGET_SCRORING_ENABLED	= false;
bank_bool CPedTargetEvaluator::DEBUG_LOCKON_TARGET_SCRORING_ENABLED			= false;
bank_bool CPedTargetEvaluator::DEBUG_MELEE_TARGET_SCRORING_ENABLED			= false;
bank_bool CPedTargetEvaluator::DEBUG_TARGET_SWITCHING_SCORING_ENABLED		= false;
bank_bool CPedTargetEvaluator::DEBUG_FOCUS_ENTITY_LOGGING					= false;
bank_bool CPedTargetEvaluator::PROCESS_REJECTION_STRING						= true;
bank_bool CPedTargetEvaluator::DISPLAY_REJECTION_STRING_PEDS				= false;
bank_bool CPedTargetEvaluator::DISPLAY_REJECTION_STRING_OBJECTS				= false;
bank_bool CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES			= false;
bank_u32  CPedTargetEvaluator::DEBUG_TARGETSCORING_DIRECTION				= 0;
#endif

atFixedArray<sTargetEntity, CPedTargetEvaluator::MAX_ENTITIES>  CPedTargetEvaluator::ms_aEntityTargets;

RegdPed CPedTargetEvaluator::ms_pIgnoreDownedPed;

#if __BANK

#if !__FINAL
CEntity* GetDebugEntity()
{
	return CDebugScene::FocusEntities_Get(0);
}
#endif

static u32 ms_NumDebugTexts = 0;

#define targetingDebugf(pEntity, fmt, ...)		if(CPedTargetEvaluator::DEBUG_FOCUS_ENTITY_LOGGING && GetDebugEntity() == pEntity) \
												{ \
													Displayf("Targeting Failed for entity (%s:%p)", pEntity->GetIsPhysical() ? static_cast<const CPhysical*>(pEntity)->GetDebugName() : "None", pEntity); \
													Displayf(fmt,##__VA_ARGS__); \
												} \
												if(CPedTargetEvaluator::PROCESS_REJECTION_STRING) \
												{ \
													Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()); \
													pEntity->GetLockOnTargetAimAtPos(vEntityPosition); \
													char debugText[100]; \
													sprintf(debugText, fmt, ##__VA_ARGS__); \
													if(pEntity != CGameWorld::FindLocalPlayer() && ( (pEntity->GetIsTypeObject() && CPedTargetEvaluator::DISPLAY_REJECTION_STRING_OBJECTS) || (pEntity->GetIsTypePed() && CPedTargetEvaluator::DISPLAY_REJECTION_STRING_PEDS) || (pEntity->GetIsTypeVehicle() && CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES))) \
													{ \
														CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, ms_NumDebugTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, Color_blue, 1); \
													} \
													if(pEntity->GetIsTypePed()) \
													{ \
														const CPed *pPed = static_cast<const CPed*>(pEntity); \
														const_cast<CPed*>(pPed)->SetLastPlayerTargetingRejectionString(debugText); \
													} \
												}
#else
#define targetingDebugf(fmt,...)
#endif

void CPedTargetEvaluator::GetLockOnTargetAimAtPos(const CEntity* pEntity, const CPed* pTargetingPed, const s32 iFlags, Vector3& aimAtPos)
{
	pEntity->GetLockOnTargetAimAtPos(aimAtPos);

	if (pEntity->GetIsTypeVehicle() && pTargetingPed->IsPlayer())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
		const CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
		if (pVehicleModelInfo && pVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_LENGTH_OF_VEHICLE_BOUNDS_FOR_PLAYER_LOCKON_POS))
		{		
			const CPlayerInfo* pPlayerInfo = pTargetingPed->GetPlayerInfo();
			if (pPlayerInfo)
			{
				const CPlayerPedTargeting& rTargeting = pPlayerInfo->GetTargeting();
				Vector3 vFreeAimPosition = rTargeting.GetClosestFreeAimTargetPos();
				if (vFreeAimPosition.IsZero())
				{
					const CWeaponInfo* pWeaponInfo = pTargetingPed->GetEquippedWeaponInfo();
					if (pWeaponInfo)
					{
						// Use ped heading when in vehicle.
						if (iFlags & TS_PED_HEADING)
						{
							vFreeAimPosition = VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetForward()) * pWeaponInfo->GetLockOnRange());
						}
						else if (iFlags & TS_CAMERA_HEADING)
						{
							// Else use camera heading.
							vFreeAimPosition = camInterface::GetPos() + (camInterface::GetFront() * pWeaponInfo->GetLockOnRange());
						}
					}
				}

				if (vFreeAimPosition.IsNonZero())
				{
					Vector3 vMin = pVehicle->GetBoundingBoxMin();
					Vector3 vMax = pVehicle->GetBoundingBoxMax();

					float fHalfWidth = rage::Abs(vMax.x - vMin.x) * 0.5f;
					vMin.x += fHalfWidth;
					vMax.x -= fHalfWidth;

					float fHalfHeight = rage::Abs(vMax.z - vMin.z) * 0.5f;
					vMin.z += fHalfHeight;
					vMax.z -= fHalfHeight;

					Vector3 vMinWorld = pVehicle->TransformIntoWorldSpace(vMin);
					Vector3 vMaxWorld = pVehicle->TransformIntoWorldSpace(vMax);

					fwGeom::fwLine lineBetweenMinMax(vMinWorld, vMaxWorld);
					lineBetweenMinMax.FindClosestPointOnLine(vFreeAimPosition, aimAtPos);
				}
			}			
		}
	}
}

float CPedTargetEvaluator::tHeadingLimitData::GetAngularLimitForDistance(float fDistance) const 
{
	if(fHeadingAngularLimitClose > 0.0f)
	{
		float fInterp = ClampRange(fDistance, fHeadingAngularLimitCloseDistMin, fHeadingAngularLimitCloseDistMax);
		float fHeadingLimit = fHeadingAngularLimitClose + ((fHeadingAngularLimit - fHeadingAngularLimitClose) * fInterp);
		return fHeadingLimit;
	}

	return fHeadingAngularLimit;
}

u32 CPedTargetEvaluator::GetTargetMode( const CPed* pTargetingPed )
{
	Assert( pTargetingPed );

	u32 nTargetMode;

#if KEYBOARD_MOUSE_SUPPORT || USE_STEAM_CONTROLLER
	// Do not allow assisted aiming for keyboard/mouse (url:bugstar:1646774).
	// This also applies to Steam Controller (also included in UseRelativeLook check)
	const CControl* pControl = pTargetingPed->GetControlFromPlayer();
	if(pControl && pControl->UseRelativeLook())
	{
		nTargetMode = TARGETING_OPTION_FREEAIM;
	}
	else
#endif // KEYBOARD_MOUSE_SUPPORT || USE_STEAM_CONTROLLER
	{
		nTargetMode = CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG);
	}

	// correct invalid target mode
	if((nTargetMode < TARGETING_OPTION_GTA_TRADITIONAL) || (nTargetMode >= MAX_TARGETING_OPTIONS))
	{
		pedAssertf(0, "Invalid Target Mode: %u", nTargetMode);
		nTargetMode = TARGETING_OPTION_GTA_TRADITIONAL;
	}

	// SNAPSHOT ability
	const CPlayerSpecialAbility* pSpecialAbility = pTargetingPed->GetSpecialAbility();
	if( BANK_ONLY(!CPlayerPedTargeting::ms_bEnableSnapShotBulletBend &&) pSpecialAbility && pSpecialAbility->GetType() == SAT_SNAPSHOT && pSpecialAbility->IsActive() )
	{
		// Additionally to the code that targets headshots (see CPed::GetLockOnTargetAimAtPos) we
		// lower the target mode by 1. Giving the player the benefits of the lower levels of targeting
		switch ( nTargetMode )
		{
		case TARGETING_OPTION_FREEAIM:
		case TARGETING_OPTION_ASSISTED_FREEAIM:
			nTargetMode = TARGETING_OPTION_GTA_TRADITIONAL;
			break;
		default:
			break;
		}
	}

	return nTargetMode;
}

#if FPS_MODE_SUPPORTED
float FacingAngleClamp(bool bFacingDirection, float fReferenceAngle, float fAngle, float fMin, float fMax)
{
	float fDiff = SubtractAngleShorter(fReferenceAngle, fMin) + SubtractAngleShorter(fAngle, fReferenceAngle);
	if((bFacingDirection && fDiff < 0.0f) || (!bFacingDirection && fDiff > 0.0f))
	{
		return fMin;
	}

	fDiff = SubtractAngleShorter(fReferenceAngle, fMax) + SubtractAngleShorter(fAngle, fReferenceAngle);
	if((bFacingDirection && fDiff > 0.0f) || (!bFacingDirection && fDiff < 0.0f))
	{
		return fMax;
	}

	return fAngle;
}
#endif

static dev_float ADDITIONAL_TARGET_WEIGHTING = 10.0f;

CEntity* CPedTargetEvaluator::FindTarget(const CPedTargetEvaluator::tFindTargetParams &testParams)
{
	PF_FUNC(FindTarget);

	const CPed *pTargetingPed = testParams.pTargetingPed;
	u32 iFlags = testParams.iFlags;

	pedFatalAssertf(pTargetingPed, "pTargetingPed is NULL");

	// Storage for the best target result
	CEntity* pBestTarget = NULL;
	float fBestScore = 0.0f;
	float fBestThreatScore = 0.0f;
	float fBestRejectForDistanceThreatScore = 0.0f;
	float fBestRejectCantSeeFromCoverThreatScore = 0.0f;

	bool bIsOnFootHoming = pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsOnFootHoming();

	// Alter the flags depending on the debug booleans
	if((!(iFlags & TS_CAMERA_HEADING) && !(iFlags & TS_PED_HEADING) && !(iFlags & TS_DESIRED_DIRECTION_HEADING)) && !bIsOnFootHoming)
	{
		iFlags |= TS_ACCEPT_HEADING_FAILURES;
	}

	if(!CHECK_VEHICLES)
	{
		iFlags &= ~TS_CHECK_VEHICLES;
	}

	//
	// Create an entity iterator, which will be used to go through the possible targets
	//

	s32 iIteratorFlags = 0;
	if(iFlags & TS_CHECK_PEDS)			iIteratorFlags |= IteratePeds;
	if(iFlags & TS_CHECK_OBJECTS)		iIteratorFlags |= IterateObjects;
	if(iFlags & TS_CHECK_VEHICLES)		iIteratorFlags |= IterateVehicles;

	const Vector3 vTargetingPedPosition = VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetPosition());

	//! Give a bonus if ped is very close to our aim vector.
	weaponAssert(pTargetingPed->GetWeaponManager());

	const CWeaponInfo *pWeaponInfo;
	bool bDoClosestPointToLineTest = false;
	Vector3 vClosestStart, vClosestEnd;
	if(pTargetingPed->GetWeaponManager()->GetEquippedVehicleWeapon())
	{
		pWeaponInfo = pTargetingPed->GetWeaponManager()->GetEquippedVehicleWeapon()->GetWeaponInfo();
	}
	else
	{
		const CWeapon* pWeapon       = pTargetingPed->GetWeaponManager()->GetEquippedWeapon();
		const CObject* pWeaponObject = pTargetingPed->GetWeaponManager()->GetEquippedWeaponObject();
	
		pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

		if(pWeapon && pWeaponObject)
		{
			const Matrix34& mWeapon = RCC_MATRIX34(pWeaponObject->GetMatrixRef());
			pWeapon->CalcFireVecFromAimCamera(pTargetingPed, mWeapon, vClosestStart, vClosestEnd);
			bDoClosestPointToLineTest = true;
		}
	}

	ms_aEntityTargets.Reset();

#if __BANK
	if(CDebugScene::bDisplayTargetingRanges)
	{
		// Draw the inner band (in green).
		const Color32 innerBandCol(0,200,0,128);
		CVectorMap::DrawCircle(vTargetingPedPosition, testParams.fMaxDistance, innerBandCol, false);

		// Draw the inner band (in red).
		if(testParams.fMaxRejectionDistance > testParams.fMaxDistance)
		{
			const Color32 innerBandCol(200,0,0,128);
			CVectorMap::DrawCircle(vTargetingPedPosition, testParams.fMaxRejectionDistance, innerBandCol, false);
		}
	}

	if(CDebugScene::bDisplayTargetingCones)
	{
		float fHeading = 0.0f;
		if(iFlags & TS_CAMERA_HEADING)
		{
			fHeading = camInterface::GetHeading();
		}
		else
		{
			fHeading = pTargetingPed->GetCurrentHeading();
		}

		float fH1 = testParams.headingLimitData.fHeadingAngularLimitClose * DtoR;
		float fRad1 = testParams.headingLimitData.fHeadingAngularLimitCloseDistMax;

		float fH2 = testParams.headingLimitData.fHeadingAngularLimit * DtoR;
		float fRad2 = testParams.fMaxDistance;

		static dev_u32 segments = 1;
		const Color32 innerBandCol(0,0,200,128);
		CVectorMap::DrawWedge(vTargetingPedPosition, 0.0f, fRad1, fHeading-fH1, fHeading+fH1, segments, innerBandCol, false);
		CVectorMap::DrawWedge(vTargetingPedPosition, 0.0f, fRad2, fHeading-fH2, fHeading+fH2, segments, innerBandCol, false);
	}

#endif

#if __ASSERT
	int nNumUncheckedEntities = 0;
#endif

	CEntityIterator entityIterator(iIteratorFlags, testParams.pCurrentTarget);
	CEntity* pEntity = entityIterator.GetNext();
	
#if FPS_MODE_SUPPORTED
	bool bAcceptHeadingFailures = (iFlags & TS_ACCEPT_HEADING_FAILURES) != 0;
#endif

	while(pEntity)
	{
		FindTargetInternal(testParams, pEntity, pTargetingPed, iFlags, vTargetingPedPosition, pWeaponInfo, bAcceptHeadingFailures, bDoClosestPointToLineTest, vClosestStart, vClosestEnd, bIsOnFootHoming ASSERT_ONLY(, nNumUncheckedEntities));

		pEntity = entityIterator.GetNext();
	}

	const CPlayerInfo* pPlayerInfo = pTargetingPed->GetPlayerInfo();
	if (pPlayerInfo)
	{
		const CPlayerPedTargeting& rTargeting = pPlayerInfo->GetTargeting();
		if (rTargeting.GetNumTargetableEntities() > 0)
		{
			for (int i = 0; i < rTargeting.GetNumTargetableEntities(); ++i)
			{
				CEntity* pEntity = rTargeting.GetTargetableEntity(i);
				if (pEntity)
				{
					FindTargetInternal(testParams, pEntity, pTargetingPed, iFlags, vTargetingPedPosition, pWeaponInfo, bAcceptHeadingFailures, bDoClosestPointToLineTest, vClosestStart, vClosestEnd, bIsOnFootHoming ASSERT_ONLY(, nNumUncheckedEntities));
				}
			}
		}
	}

#if FPS_MODE_SUPPORTED	
	//Restore flag to original value after iterating through entities
	if(!bAcceptHeadingFailures) 
	{
		iFlags &= ~TS_ACCEPT_HEADING_FAILURES;
	}
#endif

#if __ASSERT
	if(nNumUncheckedEntities > 0)
	{
		pedAssertf(0, "%d unchecked target entities. Increase max entity target array!", nNumUncheckedEntities);
	}
#endif

	//! DMKH. Sort targets by score. This allows us to do LOS checks from best to worst targets. Could perhaps sort as we push into list, which
	//! might be faster.
	if(iFlags & TS_USE_NON_NORMALIZED_SCORE)
		ms_aEntityTargets.QSort(0, -1, sTargetEntity::SortNonNormalized);
	else 
		ms_aEntityTargets.QSort(0, -1, sTargetEntity::Sort);

	float fBestLosBlockedScore = 0.0f;

	bool bRejectedTargetForDistance = false;
	bool bRejectTargetsOutsideMinHeading = false;
	bool bRejectTargetsOutsideHeadingThreshold = false;
	bool bBestTargetBehindLowVehicle = false;
	bool bFailedToFindMeleeActionForBestTarget = false;
	bool bRejectedTargetCantSeeFromCover = false;

	float fRejectTargetHeading = 0.0f;
	s32 iFallbackTargetIndex = -1;
	s32 iChosenTargetIndex = -1;

#if __BANK
	static dev_bool s_bRejectBestTargetForDistance = false;
	if(ms_aEntityTargets.GetCount() > 0 && s_bRejectBestTargetForDistance)
	{
		ms_aEntityTargets[0].bRejectLockOnForDistance = true;
	}
#endif

	bool bTestForPotentialPeds = (iFlags & TS_USE_MELEE_SCORING)!=0;

	bool bLockOn = false;
	CEntity *pLockOnEntity = NULL;
	if(pTargetingPed->GetPlayerInfo())
	{
		pLockOnEntity = pTargetingPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
		bLockOn = pLockOnEntity != NULL ? true : false;
	}

	for(int i = 0; i < ms_aEntityTargets.GetCount(); ++i)
	{
		sTargetEntity &targetEntity = ms_aEntityTargets[i];

		bool bTargetIsPlayer = targetEntity.pEntity && targetEntity.pEntity->GetIsTypePed() && static_cast<CPed*>(targetEntity.pEntity)->IsPlayer();

		//! Don't consider this entity. We have already found something better in a previous pass.
		if(testParams.pfThreatScore && *testParams.pfThreatScore >= targetEntity.fThreatScore)
		{
			break;
		}

		//! If we have rejected lock, check next target's threat. If it is less than our best scoring target, and isn't considered
		//! a threat, then reject.
		if(!bTargetIsPlayer)
		{
			if(bRejectedTargetForDistance)
			{
				if( (targetEntity.fThreatScore < fBestRejectForDistanceThreatScore) && ((targetEntity.fThreatScore / ADDITIONAL_TARGET_WEIGHTING) <= ms_Tunables.m_PrioPotentialThreat) )
				{
					continue;
				}
			}

			//! Reject lower scoring targets outside min heading range if we are currently rejecting them.
			if(bRejectTargetsOutsideMinHeading && targetEntity.bOutsideMinHeading)
			{
				continue;
			}

			//! Reject target if we have to swing camera too much having already found a target that is in cover.
			if(bRejectTargetsOutsideHeadingThreshold)
			{
				if(abs(fRejectTargetHeading - targetEntity.fHeadingScore) > ms_Tunables.m_RejectLockonHeadingTheshold)
				{
					continue;
				}
			}

			//! Don't prefer lower threat targets if we have rejected lock on due to not being able to see from cover.
			if(bRejectedTargetCantSeeFromCover && (targetEntity.fThreatScore < fBestRejectCantSeeFromCoverThreatScore))
			{
				continue;
			}
		}

		bool bUseMeleeLOSPos = (iFlags & TS_USE_MELEE_LOS_POSITION)!=0 && (pEntity != pLockOnEntity);  

		if(!targetEntity.bRejectedInVehicle)
		{
			bool bNeedsLOS = GetNeedsLineOfSightToTarget(targetEntity.pEntity);
			bool bPassedLOSTest = (!bNeedsLOS || 
				!GetIsLineOfSightBlocked(pTargetingPed, 
				targetEntity.pEntity, 
				false, 
				(iFlags & TS_IGNORE_LOW_OBJECTS)!=0, 
				(iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, 
				(iFlags & TS_IGNORE_VEHICLES_IN_LOS) != 0, 
				(iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS)!=0,
				bUseMeleeLOSPos,
				bTestForPotentialPeds,
				iFlags));

			if(targetEntity.pEntity->GetIsTypePed() &&
				bNeedsLOS && 
				!bPassedLOSTest && 
				!bRejectTargetsOutsideHeadingThreshold && 
				(iFlags & TS_DO_COVER_LOCKON_REJECTION) && 
				!(iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS) &&
				targetEntity.fHeadingScore >= ms_Tunables.m_HeadingScoreForCoverLockOnRejection )
			{
				const CPed *pTargetPed = static_cast<const CPed*>(targetEntity.pEntity);
				if(pTargetPed->GetIsInCover() && 
					pTargetPed->GetCoverPoint() && 
					pTargetingPed->GetPedIntelligence()->GetTargetting() &&
					pTargetingPed->GetPedIntelligence()->GetTargetting()->AreTargetsWhereaboutsKnown())
				{
					//! Redo test to cover peek position. If this passes, it means we should have locked on to this ped.
					if(!GetIsLineOfSightBlocked(pTargetingPed, 
						targetEntity.pEntity, 
						false, 
						(iFlags & TS_IGNORE_LOW_OBJECTS)!=0, 
						(iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, 
						(iFlags & TS_IGNORE_VEHICLES_IN_LOS) != 0, 
						true,
						bUseMeleeLOSPos,
						bTestForPotentialPeds, 
						iFlags))
					{
						if(ms_Tunables.m_RejectLockIfBestTargetIsInCover)
						{
							bRejectTargetsOutsideHeadingThreshold = true;
							fRejectTargetHeading = targetEntity.fHeadingScore;
						}

						bRejectTargetsOutsideMinHeading = true;
					}
				}
			}

			if(bPassedLOSTest)
			{
				//! If we are trying to find a melee target, then check we can perform a suitable action on that ped.
				bool bEntityValid = true;

				//! Allow targeting entities flagged as being targetable in rejection distance. But only if we haven't already found 
				//! a better target that we have rejected.
				bool bCanTargetInRejectionDistance = !bRejectedTargetForDistance && CanBeTargetedInRejectionDistance(pTargetingPed, pWeaponInfo, targetEntity.pEntity);

				if(targetEntity.bRejectLockOnForDistance && !bCanTargetInRejectionDistance)
				{
					bRejectedTargetForDistance = true;
					fBestRejectForDistanceThreatScore = MAX(targetEntity.fThreatScore, fBestRejectForDistanceThreatScore);
					continue;
				}

				if(targetEntity.bCantSeeFromCover && !bRejectedTargetCantSeeFromCover)
				{
					bRejectedTargetCantSeeFromCover = true;
					iFallbackTargetIndex = i;
					fBestRejectCantSeeFromCoverThreatScore = MAX(targetEntity.fThreatScore, fBestRejectCantSeeFromCoverThreatScore);
					bEntityValid = false;
				}

				if(bBestTargetBehindLowVehicle && targetEntity.bTargetBehindLowVehicle)
				{
					bEntityValid = false;
				}
				else if(!bBestTargetBehindLowVehicle && targetEntity.bTargetBehindLowVehicle)
				{
					bBestTargetBehindLowVehicle = true;
					iFallbackTargetIndex = i;
					bEntityValid = false;
				}

				if(iFlags & TS_CALCULATE_MELEE_ACTION)
				{
					//! If we have found a better target, but don't have a valid move, don't target dead peds instead.
					if(bFailedToFindMeleeActionForBestTarget && targetEntity.pEntity->GetIsTypePed())
					{
						if(static_cast<CPed*>(targetEntity.pEntity)->IsDead() /*|| IsDownedPedThreat(static_cast<CPed*>(targetEntity.pEntity), true)*/)
						{
							bEntityValid = false; 
						}
					}

					if(CTaskMelee::FindSuitableActionForTarget( pTargetingPed, targetEntity.pEntity, bLockOn ) == NULL)
					{
						if(!bFailedToFindMeleeActionForBestTarget &&
							!bLockOn &&
							targetEntity.pEntity->GetIsTypePed() && 
							!IsDownedPedThreat(static_cast<CPed*>(targetEntity.pEntity), true))
						{
							bFailedToFindMeleeActionForBestTarget = true;
							iFallbackTargetIndex = i;
						}

						bEntityValid = false;
					}
				}

				if(bEntityValid)
				{
					pBestTarget = targetEntity.pEntity;
					if(iFlags & TS_USE_NON_NORMALIZED_SCORE)
						fBestScore = targetEntity.GetTotalScoreNonNormalized();
					else 
						fBestScore = targetEntity.GetTotalScore();

					fBestThreatScore = targetEntity.fThreatScore;

					iChosenTargetIndex = i;

					break;
				}
			}
		}

		fBestLosBlockedScore = MAX(targetEntity.fThreatScore, fBestLosBlockedScore);
	}

	//! If we haven't found a better entity, allow setting.
	if(iFallbackTargetIndex >= 0 && !pBestTarget)
	{
		pBestTarget = ms_aEntityTargets[iFallbackTargetIndex].pEntity;
		fBestThreatScore = ms_aEntityTargets[iFallbackTargetIndex].fThreatScore;
		if(iFlags & TS_USE_NON_NORMALIZED_SCORE)
			fBestScore = ms_aEntityTargets[iFallbackTargetIndex].GetTotalScoreNonNormalized();
		else 
			fBestScore = ms_aEntityTargets[iFallbackTargetIndex].GetTotalScore();
	
		iChosenTargetIndex = iFallbackTargetIndex;
	}

	//! If we already have a melee target, check new target is better.
	if( (iFlags & TS_USE_MELEE_SCORING) && 
		pBestTarget && 
		pBestTarget->GetIsTypePed() &&
		testParams.pCurrentMeleeTarget && 
		(pBestTarget != testParams.pCurrentMeleeTarget))
	{
		if(pTargetingPed->GetPlayerInfo() && (pTargetingPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget() == testParams.pCurrentMeleeTarget))
		{
			static dev_bool s_bCheckTargetIsPlayer = false;
			if(!s_bCheckTargetIsPlayer || static_cast<CPed*>(pBestTarget)->IsAPlayerPed())
			{
				//! Find current target in potential list. And make sure it's not higher priority than best target (as this would indicates that
				//! we couldn't lock on to it).
				s32 nCurrentTargetIndex = FindPotentialTarget(testParams.pCurrentMeleeTarget);
				if(nCurrentTargetIndex > 0 && (nCurrentTargetIndex > iChosenTargetIndex) )
				{
					//! If the best score is <= current melee threat, then do a quick check to see if we should allow lock on.
					if( ms_aEntityTargets[iChosenTargetIndex].fMeleeScore <= ms_aEntityTargets[nCurrentTargetIndex].fMeleeScore )
					{	
						bool bFoundActionForCurrentTarget = CTaskMelee::FindSuitableActionForTarget( pTargetingPed, testParams.pCurrentMeleeTarget, true, false ) != NULL;

						if(bFoundActionForCurrentTarget)
						{
							bool bUseMeleeLOSPos = (iFlags & TS_USE_MELEE_LOS_POSITION)!=0 && (testParams.pCurrentMeleeTarget != pLockOnEntity);

							bool bNeedsLOS = GetNeedsLineOfSightToTarget(testParams.pCurrentMeleeTarget);
							bool bPassedLOSTest = (!bNeedsLOS || 
								!GetIsLineOfSightBlocked(pTargetingPed, 
								testParams.pCurrentMeleeTarget, 
								false, 
								(iFlags & TS_IGNORE_LOW_OBJECTS)!=0, 
								(iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, 
								(iFlags & TS_IGNORE_VEHICLES_IN_LOS) != 0, 
								(iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS)!=0,
								bUseMeleeLOSPos,
								bTestForPotentialPeds,
								iFlags));

							if(bPassedLOSTest)
							{
								//! peds are equal, so NULL out best target (let calling code decide what to do).
								pBestTarget = NULL;
							}
						}
					}	
				}	
			}
		}
	}

#if __BANK

	/*if( (iFlags & TS_CALCULATE_MELEE_ACTION) ) 
	{
		bool bPedOnGround = pBestTarget && 
			pBestTarget->GetIsTypePed() && 
			(static_cast<CPed*>(pBestTarget)->GetBonePositionCached(BONETAG_HEAD).z < (pTargetingPed->GetBonePositionCached(BONETAG_HEAD).z - 0.2f));


		if(pBestTarget && pBestTarget->GetIsTypePed() && (bPedOnGround || IsDownedPedThreat(static_cast<CPed*>(pBestTarget), true) || static_cast<CPed*>(pBestTarget)->IsInjured()))
		{
			Assert(0);
		}
	
		Displayf("\n\n Last Melee Target Debug Start - Time %d \n\n", fwTimer::GetFrameCount());
		Displayf("Chosen Ped: %p", pBestTarget);
		Displayf("iChosenTargetIndex: %d", iChosenTargetIndex);

		for(int t = 0; t <= iChosenTargetIndex; ++t)
		{
			Displayf("\n[%d] fHeadingScore: %f", t, ms_aEntityTargets[t].fHeadingScore);
			Displayf("[%d] fDistanceScore: %f", t, ms_aEntityTargets[t].fDistanceScore);
			Displayf("[%d] fMeleeScore: %f", t, ms_aEntityTargets[t].fMeleeScore);
			Displayf("[%d] fTaskModifier: %f", t, ms_aEntityTargets[t].fTaskModifier);
			Displayf("[%d] fAimModifier: %f", t, ms_aEntityTargets[t].fAimModifier);
			Displayf("[%d] fThreatScore: %f", t, ms_aEntityTargets[t].fThreatScore);
		}

		Displayf("\n\n Last Melee Target Debug End \n\n");
	}*/

#endif

	if(testParams.pfThreatScore)
	{
		*testParams.pfThreatScore = Max(fBestThreatScore, fBestRejectForDistanceThreatScore);
	}

	bool bBestTargetIsAPlayer = pBestTarget && pBestTarget->GetIsTypePed() && static_cast<CPed*>(pBestTarget)->IsPlayer();

	if(!bBestTargetIsAPlayer)
	{
		if(!(iFlags & TS_USE_MELEE_SCORING) && (testParams.headingLimitData.fWideHeadingAngulerLimit > testParams.headingLimitData.fHeadingAngularLimit) )
		{
			// If the target is neutral, widen the search and see if there is a better ped to select
			if(((fBestThreatScore / ADDITIONAL_TARGET_WEIGHTING) <= ms_Tunables.m_PrioPotentialThreat) && !testParams.pfThreatScore)
			{
				bool bTargetSwitched = false;
				if(fBestThreatScore > 0.0f && 
					!(iFlags & TS_IGNORE_LOW_OBJECTS))
				{
					float fNewBestThreatScore = fBestThreatScore;
					s32 iNewFlags = iFlags;
					iNewFlags |= TS_IGNORE_LOW_OBJECTS;
					CEntity* pNewTarget = FindTarget(pTargetingPed, iNewFlags, testParams.fMaxDistance, testParams.fMaxRejectionDistance, testParams.pCurrentTarget, &fNewBestThreatScore, 
						testParams.headingLimitData.fWideHeadingAngulerLimit, testParams.fPitchLimitMin, testParams.fPitchLimitMax, testParams.fDistanceWeight, testParams.fHeadingWeight, testParams.fDistanceFallOff);
					if((fNewBestThreatScore > fBestThreatScore) && ((fNewBestThreatScore / ADDITIONAL_TARGET_WEIGHTING) > ms_Tunables.m_PrioPotentialThreat))
					{
						bTargetSwitched = true;
						pBestTarget = pNewTarget;
					}
				}

				if(fBestLosBlockedScore > fBestThreatScore)
				{
					// If there are other threat targets, null this one as its too close.
					if(!bTargetSwitched && ((fBestLosBlockedScore / ADDITIONAL_TARGET_WEIGHTING) > (ms_Tunables.m_PrioNeutral + 0.1f)))
					{
						pBestTarget = NULL;
					}
				}
			}
		}
	}

#if DEBUG_DRAW && __BANK

	if(DEBUG_TARGETSCORING_ENABLED)
	{
		atFixedArray<sTargetEntity, MAX_ENTITIES>::iterator iterEnd = ms_aEntityTargets.end();

		for (atFixedArray<sTargetEntity, MAX_ENTITIES>::iterator iter = ms_aEntityTargets.begin(); iter != iterEnd; iter++)
		{
			sTargetEntity &targetEntity = *iter;

			CEntity *pEntity = targetEntity.pEntity;
			Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vEntityPosition);

			Color32 textColour = Color_blue;
			if(pBestTarget == pEntity)
			{
				textColour = Color_green;
			}

			bool bUseMeleeLOSPos = (iFlags & TS_USE_MELEE_LOS_POSITION)!=0 && (pEntity != pLockOnEntity); 

			char debugText[100];
			if(pEntity->GetIsTypePed())
			{
				if (CDebugScene::bDisplayTargetingEntities)
				{
					CPed *pVMPed = (CPed*)pEntity;
					if (pVMPed->GetMyVehicle() && pVMPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
					{
						CVectorMap::DrawVehicle(pVMPed->GetMyVehicle(), textColour);
					}
					else
					{
						CVectorMap::DrawPed(pVMPed, textColour);
					}
				}

				CPedGeometryAnalyser::GetModifiedTargetPosition(*pTargetingPed, 
					*((CPed*)pEntity), 
					false, 
					false, 
					(iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, 
					(iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS)!=0, 
					bUseMeleeLOSPos,
					vEntityPosition);
			}
			else
			{
				if (CDebugScene::bDisplayTargetingEntities)
				{
					CVectorMap::DrawPed(pEntity, textColour);
				}

				Vector3 vStart = VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetPosition())+ZAXIS;

				if ((iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0)
				{
					CEntity::GetEntityModifiedTargetPosition(*pEntity, vStart, vEntityPosition);
				}
			}

			if(!GetNeedsLineOfSightToTarget(pEntity))
			{
				CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTargetingPedPosition), RCC_VEC3V(vEntityPosition), Color_yellow, 100);
			}
			else if(GetIsLineOfSightBlocked(pTargetingPed, pEntity, false, (iFlags & TS_IGNORE_LOW_OBJECTS) != 0, (iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, (iFlags & TS_IGNORE_VEHICLES_IN_LOS) != 0, (iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS)!=0,bUseMeleeLOSPos, bTestForPotentialPeds, iFlags))
			{
				CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTargetingPedPosition), RCC_VEC3V(vEntityPosition), Color_red, 100);
			}
			else
			{
				CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTargetingPedPosition), RCC_VEC3V(vEntityPosition), Color_green, 100);
			}

			switch(DEBUG_TARGETSCORING_MODE)
			{
			case(eDSM_Full):
				{
					int iNoOfTexts = 0;

					if(iFlags & TS_USE_NON_NORMALIZED_SCORE)
					{
						sprintf(debugText, "T(%.4f)", targetEntity.GetTotalScoreNonNormalized());
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "=((H(%.2f)", targetEntity.fHeadingScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "+D(%.2f)", targetEntity.fDistanceScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);

						if(iFlags & TS_USE_MELEE_SCORING)
						{
							sprintf(debugText, "+M(%.2f))", targetEntity.fMeleeScore);
							CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						}
						if(iFlags & TS_DO_AIMVECTOR_BONUS_AND_TASK_MODIFIER)
						{
							if(targetEntity.fTaskModifier != 1.0f)
							{
								sprintf(debugText, "*TM(%.2f)", targetEntity.fTaskModifier);
								CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
							}
							if(targetEntity.fAimModifier != 1.0f)
							{
								sprintf(debugText, "*AM(%.2f))", targetEntity.fAimModifier);
								CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
							}
						}

						sprintf(debugText, "+TH(%.2f)", targetEntity.fThreatScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
					}
					else
					{
						sprintf(debugText, "T(%.4f)", targetEntity.GetTotalScore());
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "=H(%.2f)", targetEntity.fHeadingScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "*D(%.2f)", targetEntity.fDistanceScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "*TM(%.2f)", targetEntity.fTaskModifier);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "*AM(%.2f)", targetEntity.fAimModifier);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
						sprintf(debugText, "+TH(%.2f)", targetEntity.fThreatScore);
						CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText, textColour, 100);
					}
				}
				break;
			case(eDSM_Concise):
				{
					if(iFlags & TS_USE_NON_NORMALIZED_SCORE)
						sprintf(debugText, "T(%.4f)", targetEntity.GetTotalScoreNonNormalized());
					else 
						sprintf(debugText, "T(%.4f)", targetEntity.GetTotalScore());
					CTask::ms_debugDraw.AddText(RCC_VEC3V(vEntityPosition), 0, 0, debugText, textColour, 100);
				}
				break;
			default:
				break;
			}
		}
	}
#endif // DEBUG_DRAW

	if(NetworkInterface::IsGameInProgress())
	{
		// By default you can lock on to civilians in network games
		if(pTargetingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableLockonToRandomPeds ) && pBestTarget && pBestTarget->GetIsTypePed() )
		{
			if(static_cast<CPed*>(pBestTarget)->PopTypeIsMission() || static_cast<CPed*>(pBestTarget)->IsPlayer())
			{
				// Keep mission chars & players as targets
			}
			else
			{
				bool bKeepTarget = false;

				if( iFlags & TS_ALWAYS_KEEP_BEST_TARGET )
				{
					bKeepTarget = true;
				}
				else
				{
					// Keep ambient peds who are attacking us
					const CQueriableInterface* pQueriableInterface = static_cast<CPed*>(pBestTarget)->GetPedIntelligence()->GetQueriableInterface();
					if (pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
					{
						if (pQueriableInterface->GetHostileTarget() == pTargetingPed)
						{
							bKeepTarget = true;
						}
					}
				}

				if (!bKeepTarget)
				{
					// Null out ambient peds that aren't in combat with us
					pBestTarget = NULL;
				}
			}	
		}
	}

	//! reset this array - it's static, we don't want any external systems accessing this.
	ms_aEntityTargets.Reset();

	return pBestTarget;
}

CEntity* CPedTargetEvaluator::FindTarget(const CPed* pTargetingPed, 
										 s32 iFlags, 
										 float fMaxDistance,
										 float fMaxRejectionDistance,
										 const CEntity* pCurrentTarget, 
										 float* pfThreatScore, 
										 const CPedTargetEvaluator::tHeadingLimitData &headingLimitData, 
										 float fPitchLimitMin, 
										 float fPitchLimitMax,
										 float fDistanceWeight,
										 float fHeadingWeight, 
										 float fDistanceFallOff)
{
	tFindTargetParams testParams;
	testParams.pTargetingPed = pTargetingPed;
	testParams.iFlags = iFlags;
	testParams.fMaxDistance = fMaxDistance;
	testParams.fMaxRejectionDistance = fMaxRejectionDistance;
	testParams.pCurrentTarget = pCurrentTarget;
	testParams.pfThreatScore = pfThreatScore;
	testParams.headingLimitData = headingLimitData;
	testParams.fPitchLimitMin = fPitchLimitMin;
	testParams.fPitchLimitMax = fPitchLimitMax;
	testParams.fDistanceWeight = fDistanceWeight;
	testParams.fHeadingWeight = fHeadingWeight;
	testParams.fDistanceFallOff = fDistanceFallOff;
	return FindTarget(testParams);
}

void CPedTargetEvaluator::FindTargetInternal(const tFindTargetParams &testParams, CEntity* pEntity, const CPed *pTargetingPed, u32& iFlags, const Vector3 vTargetingPedPosition, const CWeaponInfo *pWeaponInfo, const bool bAcceptHeadingFailures, const bool bDoClosestPointToLineTest, const Vector3 vClosestStart, const Vector3 vClosestEnd, const bool bIsOnFootHoming ASSERT_ONLY(, int& nNumUncheckedEntities))
{
	
#if __BANK
		ms_NumDebugTexts = 0;
#endif

		float fMaxDistanceForEntity = testParams.fMaxDistance;
		float fMaxRejectionDistanceForEntity = testParams.fMaxRejectionDistance;

		// B*2276642: Airbourne aircraft lock-on range multiplier.
		if (pEntity->GetIsTypeVehicle() && pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsHoming() && pTargetingPed->GetEquippedWeaponInfo()->GetAirborneAircraftLockOnMultiplier() > 1.0f)
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
			if (pVehicle && pVehicle->GetIsAircraft() && pVehicle->IsInAir())
			{
				fMaxDistanceForEntity *= pTargetingPed->GetEquippedWeaponInfo()->GetAirborneAircraftLockOnMultiplier();
				fMaxRejectionDistanceForEntity *= pTargetingPed->GetEquippedWeaponInfo()->GetAirborneAircraftLockOnMultiplier();
			}
		}

		if(fMaxDistanceForEntity > fMaxRejectionDistanceForEntity)
		{
			fMaxRejectionDistanceForEntity = fMaxDistanceForEntity;
		}

		bool bRejectedInVehicle = false;
		bool bOutsideLockOnRange = false;
		if(!CheckBasicTargetExclusions(pTargetingPed, pEntity, iFlags, fMaxDistanceForEntity, fMaxRejectionDistanceForEntity, &bRejectedInVehicle, &bOutsideLockOnRange))
		{
			if(!bRejectedInVehicle)
			{
				//pEntity = entityIterator.GetNext();
				//continue;
				return;
			}
		}

		float fHeadingTargetScore = 0.0f;
		float fTargetDistanceWeighting = 1.0f;
		float fTargetThreatWeighting = 1.0f;
		float fTargetMeleeWeighting = 1.0f;
		float fHeadingScoreReduction = 2.0f;
		float fHeadingDifference = 0.0f;
		bool bOutsideMinHeading = false;
		bool bInInclusionRangeFailedHeading = false;

		Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vEntityPosition);

		if(iFlags & TS_USE_THREAT_SCORING)
		{
			fTargetThreatWeighting = ADDITIONAL_TARGET_WEIGHTING * GetScoreBasedOnThreatAssesment(pTargetingPed, pEntity);

			//! if no threat, ignore target.
			if(fTargetThreatWeighting <= 0.0f)
			{
				targetingDebugf(pEntity, "iFlags & TS_USE_THREAT_SCORING - no threat");
				//pEntity = entityIterator.GetNext();
				//continue;
				return;
			}
		}

		if(iFlags & TS_USE_MELEE_SCORING)
		{
			//! Note: Ignore low threats if this is a melee lock on (signified by TS_CALCULATE_MELEE_ACTION not being set).
			fTargetMeleeWeighting = GetScoreBasedOnMeleeAssesment(pTargetingPed, pEntity, !(iFlags & TS_CALCULATE_MELEE_ACTION));

			//! if no threat, ignore target.
			if(fTargetMeleeWeighting <= 0.0f)
			{
				targetingDebugf(pEntity, "iFlags & TS_USE_MELEE_SCORING - no threat");
				//pEntity = entityIterator.GetNext();
				//continue;
				return;
			}
		}

		bool bUsePedHeading = (iFlags & TS_PED_HEADING) != 0;
		bool bUseCameraHeading = (iFlags & TS_CAMERA_HEADING) != 0;
		bool bUseStickDirection = (iFlags & TS_DESIRED_DIRECTION_HEADING) != 0;
		bool bUseVehWeaponBoneDirection = (iFlags & TS_VEH_WEP_BONE_HEADING) != 0;


#if FPS_MODE_SUPPORTED
		bool bFPSAcceptHeadingFailuresForCurrentPedEntity = false;

		if((iFlags & (TS_CYCLE_LEFT | TS_CYCLE_RIGHT)) && pTargetingPed->IsLocalPlayer() && pTargetingPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pWeaponInfo && pWeaponInfo->GetIsMelee() && pEntity->GetIsTypePed())
		{
			const CPed *pTargetPed = static_cast<const CPed*>(pEntity);
			if(pTargetPed)
			{
				const CQueriableInterface* pQueriableInterface = pTargetPed->GetPedIntelligence()->GetQueriableInterface();
				if (pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
				{
					if (pQueriableInterface->GetHostileTarget() == pTargetingPed)
					{
						bFPSAcceptHeadingFailuresForCurrentPedEntity = true;
					}
				}
			}
		}

		if(!bAcceptHeadingFailures)
		{
			if(bFPSAcceptHeadingFailuresForCurrentPedEntity)
			{
				//Set flag for this iteration
				iFlags |= TS_ACCEPT_HEADING_FAILURES;
			}
			else
			{
				//Clear flag for this iteration
				iFlags &= ~TS_ACCEPT_HEADING_FAILURES;
			}
		}
#endif

		//! Check if we want to use camera heading as well.
		if( bUseStickDirection && (pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetPedConfigFlag(CPED_CONFIG_FLAG_UseCameraHeadingForDesiredDirectionLockOnTest)))
		{
			bUseCameraHeading = true;
		}

		// Passed in "IsTargetValid" checks, unused here. Used in CPlayerPedTargeting::ProcessOnFootHomingLockOn.
		bool bOnFootHomingTargetInDeadzone = false;

		// Score player heading
		if(bUsePedHeading)
		{
			Vector3 vHeading = VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetB());

			// B*2148150: Set on foot homing flag and get aiming pitch (used to determine if target is valid from aiming pose).
			bool bIsFiring = false;
			bool bTargetIsAlreadyLockOnTarget = false;
			if (bIsOnFootHoming)
			{
				if (pTargetingPed && pTargetingPed->GetPedIntelligence() && pTargetingPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
				{
					// Use expanded limits if we just fired (due to animation/camera moving up slightly).
					CTaskAimGunOnFoot* pTaskAimGunOnFoot = static_cast<CTaskAimGunOnFoot*>(pTargetingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
					if (pTaskAimGunOnFoot && pTaskAimGunOnFoot->GetState() == CTaskAimGunOnFoot::State_Firing)
					{
						bIsFiring = true;
					}
				}
				CPlayerPedTargeting &targeting = pTargetingPed->GetPlayerInfo()->GetTargeting();
				bTargetIsAlreadyLockOnTarget = targeting.GetLockOnTarget() && (pEntity == targeting.GetLockOnTarget());
			}
			if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vHeading, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax, bIsOnFootHoming, bIsFiring, bTargetIsAlreadyLockOnTarget ) )
			{
				if(!testParams.headingLimitData.bPedHeadingNeedsTargetOnScreen || pEntity->GetIsOnScreen())
				{
					float fScore = GetScoreBasedOnHeading( pEntity, vTargetingPedPosition, iFlags, vHeading, fHeadingDifference, HD_BOTH, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
					fScore *= testParams.headingLimitData.fPedHeadingWeighting;
					fHeadingTargetScore += fScore;
					fHeadingScoreReduction *= 0.5f;
				}
			}
		}

		// Score camera
		if(bUseCameraHeading)
		{
			Vector3 vTargetingHeading = camInterface::GetPlayerControlCamAimFrame().GetFront();
			Vector3 vScoringHeading = vTargetingHeading;
			CPedTargetEvaluator::tHeadingLimitData testParamsHeadingLimitData = testParams.headingLimitData;

#if FPS_MODE_SUPPORTED
			if(pTargetingPed->IsLocalPlayer() && pTargetingPed->GetCoverPoint() != NULL && pTargetingPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				CTaskCover* pCoverTask = static_cast<CTaskCover*>(pTargetingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
				const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
				if(pCoverTask != NULL && pCamera != NULL)
				{
					bool bFacingLeftInCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft);
					bool bInHighCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint);
					bool bAtCorner = CTaskInCover::CanPedFireRoundCorner(*pTargetingPed, bFacingLeftInCover);

					float fCoverHeading = CTaskMotionInCover::ComputeCoverHeading(*pTargetingPed);
					float fCamHeading;
					float fCamPitch;
					camFrame::ComputeHeadingAndPitchFromFront(vTargetingHeading, fCamHeading, fCamPitch);

					const float fAngleBetweenCoverAndCamera = SubtractAngleShorter(fCamHeading, fCoverHeading);

					// Don't want to expand the aiming cone if the cover task thinks we're aiming directly or the camera thinks we're aiming directly
					// (The camera has a slightly expanded zone in which we pretend we're aiming directly and change the camera behavior to act as if
					// the cover task had told us we were aiming directly)
					if((!bInHighCover || bAtCorner) && !pCoverTask->IsCoverFlagSet(CTaskCover::CF_AimDirectly) && 
						!pCamera->IsAimingDirectly(*pTargetingPed, fAngleBetweenCoverAndCamera, fCamPitch, bFacingLeftInCover, bInHighCover, bAtCorner, false))
					{
						float fHeadingCorrectionAimHeading = pTargetingPed->GetPlayerInfo()->GetCoverHeadingCorrectionAimHeading();
						if(fHeadingCorrectionAimHeading == FLT_MAX)
						{
							fHeadingCorrectionAimHeading = fCoverHeading;
						}

						// If using assisted free aim then we don't want to expand the cone but instead only do targeting at the corrected aim heading
						if(CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG) == TARGETING_OPTION_ASSISTED_FREEAIM)
						{
							vTargetingHeading.Set(-Sinf(fHeadingCorrectionAimHeading), Cosf(fHeadingCorrectionAimHeading), 0.0f);
						}
						else
						{
							Vector3 vDelta = vEntityPosition - vTargetingPedPosition;

							// Use a wider angle if trying to switch between targets
							float fMaxHeadingDifference = (testParamsHeadingLimitData.GetAngularLimitForDistance(vDelta.Mag()) * DtoR);

							// Push the camera heading and the aim correction heading out by the angular limit
							float fAdjustedCamHeading = fCamHeading;
							float fAdjustedHeadingCorrectionAimHeading = fHeadingCorrectionAimHeading;
							float fAngleDiff = SubtractAngleShorter(fCamHeading, fHeadingCorrectionAimHeading);
							if(fAngleDiff <= 0.0f)
							{
								fAdjustedHeadingCorrectionAimHeading = fwAngle::LimitRadianAngle(fAdjustedHeadingCorrectionAimHeading + fMaxHeadingDifference);
								fAdjustedCamHeading = fwAngle::LimitRadianAngle(fAdjustedCamHeading - fMaxHeadingDifference);
							}
							else
							{
								fAdjustedHeadingCorrectionAimHeading = fwAngle::LimitRadianAngle(fAdjustedHeadingCorrectionAimHeading - fMaxHeadingDifference);
								fAdjustedCamHeading = fwAngle::LimitRadianAngle(fAdjustedCamHeading + fMaxHeadingDifference);
							}

							const float fFacingHeadingForCoverDirection = CTaskMotionInCover::ComputeFacingHeadingForCoverDirection(fCoverHeading, bFacingLeftInCover);
					
							float fMaxClamp = fFacingHeadingForCoverDirection;
							fMaxClamp += bFacingLeftInCover ? fMaxHeadingDifference : -fMaxHeadingDifference;
							fMaxClamp = fwAngle::LimitRadianAngle(fMaxClamp);
					
							// Clamp the camera heading and the aim correction heading to be between the cover facing heading and the cover direction (for high cover) 
							// or the negative cover facing direction (for low cover)
							float fMinClamp = fCoverHeading;
							if(!bInHighCover)
							{
								fMinClamp = fwAngle::LimitRadianAngle(fFacingHeadingForCoverDirection - PI);
								fMinClamp += bFacingLeftInCover ? -fMaxHeadingDifference : fMaxHeadingDifference;
								fMinClamp = fwAngle::LimitRadianAngle(fMinClamp);
							}

							fAdjustedHeadingCorrectionAimHeading = FacingAngleClamp(bFacingLeftInCover, fCoverHeading, fAdjustedHeadingCorrectionAimHeading, fMinClamp, fMaxClamp);
							fAdjustedCamHeading = FacingAngleClamp(bFacingLeftInCover, fCoverHeading, fAdjustedCamHeading, fMinClamp, fMaxClamp);

							// Figure out the new limit
							// When finding the difference between the adjusted camera heading and the adjusted corrected aim heading we need to find the angle that goes through both the original camera
							// heading and the original corrected aim heading as it might end up being the longer way around
							testParamsHeadingLimitData.fHeadingAngularLimit = 0.5f * (SubtractAngleShorter(fHeadingCorrectionAimHeading, fAdjustedHeadingCorrectionAimHeading) + SubtractAngleShorter(fCamHeading, fHeadingCorrectionAimHeading) + SubtractAngleShorter(fAdjustedCamHeading, fCamHeading));
							float fAverageHeading = fwAngle::LimitRadianAngle(fAdjustedHeadingCorrectionAimHeading + testParamsHeadingLimitData.fHeadingAngularLimit);
							testParamsHeadingLimitData.fHeadingAngularLimit = Abs(testParamsHeadingLimitData.fHeadingAngularLimit) * RtoD;
							testParamsHeadingLimitData.fHeadingAngularLimitClose = 0.0f;

							// Figure out the mid-point between the pushed out camera heading and aim correction heading
							vTargetingHeading.Set(-Sinf(fAverageHeading), Cosf(fAverageHeading), 0.0f);
						}
					}
				}
			}
#endif
			if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vTargetingHeading, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParamsHeadingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax, bIsOnFootHoming ) )
			{
				if(!testParamsHeadingLimitData.bCamHeadingNeedsTargetOnScreen || pEntity->GetIsOnScreen())
				{
					float fScore = GetScoreBasedOnHeading( pEntity, vTargetingPedPosition, iFlags, vScoringHeading, fHeadingDifference, HD_BOTH, bInInclusionRangeFailedHeading, testParamsHeadingLimitData.fHeadingFalloffPower, pTargetingPed );
					fScore *= testParamsHeadingLimitData.fCameraHeadingWeighting;
					fHeadingTargetScore += fScore;
					fHeadingScoreReduction *= 0.5f;
				}
			}

			if(pTargetingPed->IsLocalPlayer() && !pTargetingPed->GetIsInVehicle() && !(iFlags & TS_USE_MELEE_SCORING) && !(iFlags & TS_CYCLE_LEFT) && !(iFlags & TS_CYCLE_RIGHT))
			{
				float fXYDistLimit = CPlayerPedTargeting::ms_Tunables.GetXYDistLimitFromAimVector();
				float fZDistLimit = CPlayerPedTargeting::ms_Tunables.GetZDistLimitFromAimVector();

				if(fHeadingTargetScore > 0.0f)
				{
					Vector3 vCamPos = camInterface::GetPlayerControlCamAimFrame().GetPosition();
					Vector3 vCamHeading = camInterface::GetPlayerControlCamAimFrame().GetFront();

					//! Check sticky pos is within bounds.
					Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

					float fLineLength = testParams.fMaxDistance > 200.0f ? testParams.fMaxDistance : 200.0f;

					Vector3 vClosest;
					fwGeom::fwLine line(vCamPos, vCamPos + (vCamHeading * fLineLength) );
					line.FindClosestPointOnLine(vEntityPos, vClosest);

					Vector3 vDiff = vClosest - vEntityPos;

					float fXYMag = vDiff.XYMag();
					float fZNag = abs(vDiff.z);

					if(fXYDistLimit >= 0.0f && (fXYMag > fXYDistLimit))
					{
						fHeadingTargetScore = 0.0f;
					}

					if(fZDistLimit >= 0.0f && (fZNag > fZDistLimit))
					{
						fHeadingTargetScore = 0.0f;
					}
				}
			}
		}

		// Desired direction heading
		if(bUseStickDirection)
		{
			CControl* pControl = const_cast<CControl*>(pTargetingPed->GetControlFromPlayer());
			if( pControl && !CControlMgr::IsDisabledControl( pControl ) )
			{
				// Check the desired direction input
				Vector2 vStickInput2D; 
				if(testParams.pvDesiredStickOverride)
				{
					vStickInput2D = *testParams.pvDesiredStickOverride;
				}
				else
				{
					vStickInput2D.Set( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
				}
				
				Vector3 vecStickInput( vStickInput2D.x, vStickInput2D.y, 0.0f );
				const float fInputMag = vecStickInput.Mag2();
				if( fInputMag > rage::square( 0.5f ) )
				{
					float fCamOrient = camInterface::GetHeading();
					vecStickInput.RotateZ(fCamOrient);
					Vector3 vHeading = vecStickInput;

					if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vHeading, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
					{
						if(!testParams.headingLimitData.bStickHeadingNeedsTargetOnScreen || pEntity->GetIsOnScreen())
						{
							float fScore = GetScoreBasedOnHeading( pEntity, vTargetingPedPosition, iFlags, vHeading, fHeadingDifference, HD_BOTH, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
							fScore *= testParams.headingLimitData.fStickHeadingWeighting;
							fHeadingTargetScore += fScore;
							fHeadingScoreReduction *= 0.5f;
						}
					}
				}
			}
		}
	
		// Use direction of vehicle weapon bone
		if (bUseVehWeaponBoneDirection && pTargetingPed->GetIsInVehicle())
		{
			CVehicle *pVehicle = pTargetingPed->GetVehiclePedInside();
			if (pVehicle)
			{
				const CVehicleWeapon *pVehWeapon = pTargetingPed->GetWeaponManager()->GetEquippedVehicleWeapon();
				if (pVehWeapon)
				{
					CVehicleWeaponBattery* pVehWeaponBattery = (CVehicleWeaponBattery*)pVehWeapon;
					{
						if (pVehWeaponBattery)
						{
							CFixedVehicleWeapon* pFixedVehWeapon = (CFixedVehicleWeapon*)pVehWeaponBattery->GetVehicleWeapon(0);	//pVehWeaponBattery->m_iCurrentWeaponCounter?
							if (pFixedVehWeapon)
							{
								eHierarchyId eBoneId = pFixedVehWeapon->GetWeaponBone();
								s32 nWeaponBoneIndex = pVehicle->GetBoneIndex(eBoneId);
								if (nWeaponBoneIndex >= 0)
								{
									Matrix34 weaponMat;
									pVehicle->GetGlobalMtx(nWeaponBoneIndex, weaponMat);

									Vector3 vHeading = weaponMat.b;
									if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vHeading, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
									{
										float fScore = GetScoreBasedOnHeading( pEntity, vTargetingPedPosition, iFlags, vHeading, fHeadingDifference, HD_BOTH, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
										fScore *= testParams.headingLimitData.fCameraHeadingWeighting;
										fHeadingTargetScore += fScore;
										fHeadingScoreReduction *= 0.5f;
									}
								}
							}
						}
					}
				}
			}
		}

		// Reduce the heading score if it included both the camera and player heading
		fHeadingTargetScore *= fHeadingScoreReduction;

		// Score left or right heading conditions
		if(iFlags & (TS_CYCLE_LEFT | TS_CYCLE_RIGHT))
		{
			const camFrame& gameplayFrame = camInterface::GetPlayerControlCamAimFrame();
			Vector3 vStartingOrientation = gameplayFrame.GetFront();
			Vector3 vCamPosition = gameplayFrame.GetPosition();
			if(testParams.pCurrentTarget)
			{
				vStartingOrientation =
					VEC3V_TO_VECTOR3(testParams.pCurrentTarget->GetTransform().GetPosition()) - vTargetingPedPosition;

				if (!USE_FLAT_HEADING)
					vStartingOrientation.Normalize();
			}

			if(iFlags & TS_CYCLE_LEFT)
			{
				if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vStartingOrientation, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
				{
					if(!testParams.headingLimitData.bCamHeadingNeedsTargetOnScreen || pEntity->GetIsOnScreen())
					{
						// On-Foot Homing Launcher: Pick vehicle to the left of the current target with the smallest heading difference.
						if (bIsOnFootHoming)
						{
							float fCurrentTargetHeadingDifference = -1.0f;
							CEntity *pCurrentTarget = pTargetingPed->GetPlayerInfo() ? pTargetingPed->GetPlayerInfo()->GetTargeting().GetTarget() : NULL;
							if (pCurrentTarget && pCurrentTarget != pEntity)
							{
								// Get current target heading difference.
								if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), pCurrentTarget->GetTransform().GetPosition(), iFlags, vStartingOrientation, fMaxRejectionDistanceForEntity, fCurrentTargetHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
								{
									// Heading difference is to the left of the current target.
									if (fHeadingDifference > fCurrentTargetHeadingDifference)
									{
										// Scale score; larger score for targets with smaller heading difference to current target.
										float fDelta = Abs(fHeadingDifference - fCurrentTargetHeadingDifference);
										float fMult = 2 - fDelta;
										const float fCurrentTargetHeadingScore = GetScoreBasedOnHeading( pEntity, vCamPosition, iFlags, vStartingOrientation, fCurrentTargetHeadingDifference, HD_JUST_LEFT, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
										fHeadingTargetScore = fCurrentTargetHeadingScore + fMult;
									}
								}						
							}
						}
						else
						{
							const float fHeadingScore = GetScoreBasedOnHeading( pEntity, vCamPosition, iFlags, vStartingOrientation, fHeadingDifference, HD_JUST_LEFT, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
							fHeadingTargetScore = (fHeadingScore <= 0.0f) ? fHeadingScore : fHeadingTargetScore + fHeadingScore;
						}
					}
				}				
			}

			if(iFlags & TS_CYCLE_RIGHT)
			{
				if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), VECTOR3_TO_VEC3V(vEntityPosition), iFlags, vStartingOrientation, fMaxRejectionDistanceForEntity, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
				{
					if(!testParams.headingLimitData.bCamHeadingNeedsTargetOnScreen || pEntity->GetIsOnScreen())
					{
						// On-Foot Homing Launcher: Pick vehicle to the right of the current target with the smallest heading difference.
						if (bIsOnFootHoming)
						{
							float fCurrentTargetHeadingDifference = -1.0f;
							CEntity *pCurrentTarget = pTargetingPed->GetPlayerInfo() ? pTargetingPed->GetPlayerInfo()->GetTargeting().GetTarget() : NULL;
							if (pCurrentTarget && pCurrentTarget != pEntity)
							{
								// Get current target heading difference.
								if( IsTargetValid( VECTOR3_TO_VEC3V(vTargetingPedPosition), pCurrentTarget->GetTransform().GetPosition(), iFlags, vStartingOrientation, fMaxRejectionDistanceForEntity, fCurrentTargetHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, testParams.headingLimitData, testParams.fPitchLimitMin, testParams.fPitchLimitMax ) )
								{
									// Heading difference is to the left of the current target.
									if (fHeadingDifference < fCurrentTargetHeadingDifference)
									{
										// Scale score; larger score for targets with smaller heading difference to current target.
										float fDelta = Abs(fHeadingDifference - fCurrentTargetHeadingDifference);
										float fMult = 2 - fDelta;
										const float fCurrentTargetHeadingScore = GetScoreBasedOnHeading( pEntity, vCamPosition, iFlags, vStartingOrientation, fCurrentTargetHeadingDifference, HD_JUST_RIGHT, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
										fHeadingTargetScore = fCurrentTargetHeadingScore + fMult;
									}
								}						
							}
						}
						else
						{
							const float fHeadingScore = GetScoreBasedOnHeading( pEntity, vCamPosition, iFlags, vStartingOrientation, fHeadingDifference, HD_JUST_RIGHT, bInInclusionRangeFailedHeading, testParams.headingLimitData.fHeadingFalloffPower, pTargetingPed );
							fHeadingTargetScore = (fHeadingScore <= 0.0f) ? fHeadingScore : fHeadingTargetScore + fHeadingScore;
						}
					}
				}
			}
		}

		//! DMKH. Add global scale factor to heading.
		fHeadingTargetScore *= testParams.fHeadingWeight;

		// Ignore any targets that don't meet the heading criteria
		if(fHeadingTargetScore <= 0.0f && !(iFlags & TS_ACCEPT_HEADING_FAILURES))
		{
			targetingDebugf(pEntity, "fHeadingTargetScore < 0.0f");
			//pEntity = entityIterator.GetNext();
			//continue;
			return;
		}

		// Do distance weighting
		if(iFlags & TS_USE_DISTANCE_SCORING)
		{
			fTargetDistanceWeighting = testParams.fDistanceWeight * GetScoreBasedOnDistance(pTargetingPed, pEntity, testParams.fDistanceFallOff, iFlags);
		}

		float fClosestPointToLineModifier = 1.0f;
		float fActiveTaskModifier = 1.0f;
		if( (iFlags & TS_DO_AIMVECTOR_BONUS_AND_TASK_MODIFIER))
		{
			if(bDoClosestPointToLineTest)
			{
				fwGeom::fwLine line(vClosestStart, vClosestEnd);
				Vector3 vClosest;
				line.FindClosestPointOnLine(vEntityPosition, vClosest);

				Vector3 vDistanceToLine = vEntityPosition - vClosest;
				float fDistance = vDistanceToLine.Mag();
				if(fDistance <= ms_Tunables.m_ClosestPointToLineDist)
				{
					fClosestPointToLineModifier = (1.0f + ( (1.0f - (fDistance/ms_Tunables.m_ClosestPointToLineDist))*ms_Tunables.m_ClosestPointToLineBonusModifier));
				}
			}

			bool bDissalowAimBonus = false;
			fActiveTaskModifier = GetScoreBasedOnTask(pTargetingPed, pEntity, iFlags, bDissalowAimBonus);
			if(bDissalowAimBonus)
			{
				fClosestPointToLineModifier = 1.0f;
			}
		}

		sTargetEntity newEntity;
		newEntity.pEntity = pEntity;
		newEntity.fHeadingScore = fHeadingTargetScore;
		newEntity.fDistanceScore = fTargetDistanceWeighting;
		newEntity.fMeleeScore = fTargetMeleeWeighting;
		newEntity.fTaskModifier = fActiveTaskModifier;
		newEntity.fAimModifier = fClosestPointToLineModifier;
		newEntity.fThreatScore = fTargetThreatWeighting;
		newEntity.bRejectedInVehicle = bRejectedInVehicle;
		newEntity.bRejectLockOnForDistance = bOutsideLockOnRange;
		newEntity.bTargetBehindLowVehicle = false;
		newEntity.bOutsideMinHeading = bOutsideMinHeading;
		newEntity.bCantSeeFromCover = false;

		pedAssertf(rage::FPIsFinite((iFlags & TS_USE_NON_NORMALIZED_SCORE) ? newEntity.GetTotalScoreNonNormalized() : newEntity.GetTotalScore()), "Invalid targeting score: (%f:%f:%f:%f:%f:%f)", fHeadingTargetScore, fTargetDistanceWeighting, fTargetMeleeWeighting, fActiveTaskModifier, fClosestPointToLineModifier, fTargetThreatWeighting);
		if(ms_aEntityTargets.GetCount() < ms_aEntityTargets.GetMaxCount())
		{
			ms_aEntityTargets.Push(newEntity);
		}
#if __ASSERT
		else
		{
			nNumUncheckedEntities++;
		}
#endif

}

bool CPedTargetEvaluator::IsTargetValid( Vec3V_In vPosition, 
										Vec3V_In vTargetPosition, 
										s32 iFlags, 
										const Vector3& vComparisonHeading, 
										float fMaxDistance, 
										float& fHeadingDifferenceOut, 
										bool& bOutsideMinHeading,
										bool& bInInclusionRangeFailedHeading,
										bool& bOnFootHomingTargetInDeadzone,
										const CPedTargetEvaluator::tHeadingLimitData &headingLimitData, 									
										float fPitchLimitMinInDegrees, 
										float fPitchLimitMaxInDegrees,
										bool  bIsOnFootHoming,
										bool  bIsFiring,
										bool  bAlreadyLockedOnToThisTarget,
										bool  bDoHomingBoxCheck)
{
	Vector3 vDelta = VEC3V_TO_VECTOR3( vTargetPosition - vPosition );
	float fDistanceSq = vDelta.Mag2();
	if( fDistanceSq > rage::square( fMaxDistance ) )
	{
		return false;
	}

	// Max angular heading. 
	float fMaxHeadingDifference;
	float fMinHeadingDifference;

	// Use a wider angle if trying to switch between targets
	fMaxHeadingDifference = (DtoR * headingLimitData.GetAngularLimitForDistance(rage::Sqrtf(fDistanceSq)));
	fMinHeadingDifference = (DtoR * headingLimitData.GetAngularLimit());

	// B*2160636: Use expanded heading check when firing on-foot homing launcher (fixes target flickering issue due to fire animation, same as pitch checks below).
	if (bIsOnFootHoming && bIsFiring)
	{
		fMaxHeadingDifference = fMaxHeadingDifference * 1.25f;
		fMinHeadingDifference = fMinHeadingDifference * 1.25f;
	}

	// If heading failures are accepted, take any angle
	if( iFlags & TS_ACCEPT_HEADING_FAILURES )
	{
		fMaxHeadingDifference = TWO_PI;
		fMinHeadingDifference = TWO_PI;
	}	

	// Take the relative orientation into account.
	// Flatten the heading vectors to obtain only the heading difference
	float fOrientation = 0.0f;
	float fOrientationDiff = 0.0f;

	if (!USE_FLAT_HEADING)
	{
		vDelta.Normalize();
		fOrientation = rage::Atan2f( -vDelta.x, vDelta.y );
		fOrientationDiff = fOrientation - rage::Atan2f( -vComparisonHeading.x, vComparisonHeading.y );
	}
	else
	{
		Vector3 vFlatCompHeading = vComparisonHeading;
		vFlatCompHeading.z = 0.0f; 
		vFlatCompHeading.Normalize();
		Vector3 vFlatDelta = vDelta;
		vFlatDelta.z = 0.0f;
		vFlatDelta.Normalize();
		fOrientation = rage::Atan2f( -vFlatDelta.x, vFlatDelta.y );
		fOrientationDiff = fOrientation - rage::Atan2f( -vFlatCompHeading.x, vFlatCompHeading.y );
	}
	fOrientationDiff = fwAngle::LimitRadianAngle( fOrientationDiff );

	if( ABS( fOrientationDiff ) > fMaxHeadingDifference)
	{
		bInInclusionRangeFailedHeading = true;
		return false;
	}

	if( ABS( fOrientationDiff ) > fMinHeadingDifference)
	{
		bOutsideMinHeading = true;
	}

	// Check the vertical delta
	if( !(iFlags & TS_ACCEPT_HEADING_FAILURES) )
	{
		// Set up the min/max pitch values
		float fMinPitchDifference = CTorsoIkSolver::ms_torsoInfo.minPitch;
		float fMaxPitchDifference = CTorsoIkSolver::ms_torsoInfo.maxPitch;
		if( fPitchLimitMinInDegrees != fPitchLimitMaxInDegrees )
		{
			fMinPitchDifference = (DtoR * fPitchLimitMinInDegrees);
			fMaxPitchDifference = (DtoR * fPitchLimitMaxInDegrees);
		}

		// Calculate and limit the pitch
		float fPitch = rage::Atan2f( vDelta.z, vDelta.XYMag() );
		fPitch = fwAngle::LimitRadianAngle( fPitch );
		if( fPitch > fMaxPitchDifference || fPitch < fMinPitchDifference )
		{
			return false;
		}

		// B*2148150: Homing on-foot: Check pitch limits based on aiming pitch
		if (bIsOnFootHoming)
		{
			float fPitchDelta = Abs(camInterface::GetPitch() - fPitch);
			float fAngularPitchLimit = fMinHeadingDifference;
			float fPitchThreshold = bIsFiring ? (fAngularPitchLimit * 1.25f) : fAngularPitchLimit;
			if (fPitchDelta > fPitchThreshold)
			{
				return false;
			}
		}

		//! Disallow if we have exceeded aim pitch.
		float fCurrentAngle = ((PI*0.5f)-AcosfSafe(vComparisonHeading.z)) * RtoD;
		float fPitchDegs = fPitch * RtoD;
		float fAimPitchDiff = fCurrentAngle - fPitchDegs;
		if( (fAimPitchDiff < -headingLimitData.fAimPitchLimitMin) || (fAimPitchDiff > headingLimitData.fAimPitchLimitMax) ) 
		{
			return false; 
		}
	}

	// B*2161163: Homing on-foot: Check if target is within the reticule box (in screen coords).
	TUNE_GROUP_BOOL(ON_FOOT_HOMING_LAUNCHER, bEnableReticuleBoxTargetValidCheck, true);
	if (bIsOnFootHoming && bEnableReticuleBoxTargetValidCheck)
	{
		//Hard-coded offset from reticule dot in scaleform.
		const float fStandardUIBoxOffset = 0.15f;

		//Keep same as value in CUIReticule::Update; "OVERRIDE_HOMING_SCOPE".
		TUNE_GROUP_FLOAT(ON_FOOT_HOMING_LAUNCHER, fUIBoxScaler, 1.4f, 0.0f, 10.0f, 0.01f);
		float fLockOnBoxOffset = fStandardUIBoxOffset * fUIBoxScaler;

		// Expand the box when firing to avoid jittering lock issues due to fire anim.
		if (bIsFiring)
		{
			fLockOnBoxOffset *= 1.25f;
		}

		// If we're already locked onto this target, allow a bit of extra slack around the box.
		float fLockOnBoxOffsetWithDeadzone = 0.0f;
		if (bAlreadyLockedOnToThisTarget)
		{
			TUNE_GROUP_FLOAT(ON_FOOT_HOMING_LAUNCHER, fLockedOnDeadzone, 1.5f, 0.0f, 2.0f, 0.01f);
			fLockOnBoxOffsetWithDeadzone = fLockOnBoxOffset * fLockedOnDeadzone;
		}

		float fScreenRatio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
		float fBoxOffsetX = fLockOnBoxOffset * fScreenRatio;
		float fBoxOffsetY = fLockOnBoxOffset;

		Vector2 vBoxMin(0.5f - fBoxOffsetX, 0.5f - fBoxOffsetY);
		Vector2 vBoxMax(0.5f + fBoxOffsetX, 0.5f + fBoxOffsetY);

		Vector3 vTargetPositionWorld = VEC3V_TO_VECTOR3(vTargetPosition);
		Vector2 vTargetPositionScreen(0.5f, 0.5f);
		
		bool bTargetIsValid = true;
		CUIReticule::TransformReticulePosition(vTargetPositionWorld, vTargetPositionScreen, bTargetIsValid);

		bool bOutsideOfLockOnBox = (vTargetPositionScreen.x > vBoxMax.x || vTargetPositionScreen.y > vBoxMax.y || vTargetPositionScreen.x < vBoxMin.x || vTargetPositionScreen.y < vBoxMin.y);
		
		if (bOutsideOfLockOnBox && bDoHomingBoxCheck)
		{
			// Target is still valid as we were already locked on within the specified deadzone.
			bool bInsideLockOnBoxWithDeadzone = false;
			if (bAlreadyLockedOnToThisTarget)
			{
				float fDeadzoneBoxOffsetX = fLockOnBoxOffsetWithDeadzone * fScreenRatio;
				float fDeadzoneBoxOffsetY = fLockOnBoxOffsetWithDeadzone;

				Vector2 vDeadzoneBoxMin(0.5f - fDeadzoneBoxOffsetX, 0.5f - fDeadzoneBoxOffsetY);
				Vector2 vDeadzoneBoxMax(0.5f + fDeadzoneBoxOffsetX, 0.5f + fDeadzoneBoxOffsetY);

				bool bOutsideOfLockOnBoxWithDeadzone = (vTargetPositionScreen.x > vDeadzoneBoxMax.x || vTargetPositionScreen.y > vDeadzoneBoxMax.y || vTargetPositionScreen.x < vDeadzoneBoxMin.x || vTargetPositionScreen.y < vDeadzoneBoxMin.y);

				// We're in the "deadzone" region, so don't return false as the target is still valid.
				if (!bOutsideOfLockOnBoxWithDeadzone)
				{
					// This flag allows us to swap targets instantly if we get another target within the main lock-on window in CPlayerPedTargeting::ProcessOnFootHomingLockOn.
					bOnFootHomingTargetInDeadzone = true;
					bInsideLockOnBoxWithDeadzone = true;
				}
			}
			
			if (!bInsideLockOnBoxWithDeadzone)
			{
				return false;
			}
		}
	}

	fHeadingDifferenceOut = fOrientationDiff;

	return true;
}

bool CPedTargetEvaluator::GetIsTargetingDisabled(const CPed* pTargetingPed, const CEntity* pEntity, s32 iFlags)
{
	pedFatalAssertf(pTargetingPed, "pTargetingPed is NULL");

	// don't target ghosted players
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsDamageDisabledInMP(*pTargetingPed, *pEntity))
	{
		return true;
	}

	eLockType eTargetLockType = CPlayerPedTargeting::ms_Tunables.GetLockType();
	bool bUsingOnFootHomingLauncher = pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsOnFootHoming();
	if(pTargetingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisablePlayerLockon ) || pTargetingPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerLockon ) || ((eTargetLockType==LT_None) && !(iFlags & TS_ALLOW_LOCKON_EVEN_IF_DISABLED) && !bUsingOnFootHomingLauncher))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CPedTargetEvaluator::GetCanPedBeTargetedInAVehicle(const CPed* pTargetingPed, const CPed* pTargetPed, s32 iFlags)
{
	pedFatalAssertf(pTargetingPed, "pTargetingPed is NULL");
	pedFatalAssertf(pTargetPed, "pPed is NULL");

	if(!pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AttachedToVehicle))
	{
		// Target ped not in a vehicle, so can be targeted
		return true;
	}

	if((CPlayerPedTargeting::ms_Tunables.m_AllowDriverLockOnToAmbientPedsInSP && !CNetwork::IsGameInProgress()) && pTargetingPed->GetIsDrivingVehicle())
	{
		return true;
	}

	if(pTargetPed->GetMyVehicle())
	{
		//! Don't allow lock on to a ped in a respotted (respawning) vehicle.
		if(CNetwork::IsGameInProgress() && pTargetPed->GetMyVehicle()->IsBeingRespotted())
		{
			return false;
		}

		const CWeaponInfo* pWeaponInfo = pTargetingPed->GetWeaponManager() ? CWeaponInfoManager::GetInfo<CWeaponInfo>(pTargetingPed->GetWeaponManager()->GetEquippedWeaponHash()) : NULL;
		bool bUsingMeleeWeapon = pWeaponInfo && pWeaponInfo->GetIsMelee();

		if(pTargetPed->GetMyVehicle()->InheritsFromBike() || pTargetPed->GetMyVehicle()->InheritsFromQuadBike() || pTargetPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike())
		{			
			// Allow targeting players on bikes with melee weapons when in a multiplayer game
			if (!bUsingMeleeWeapon || (NetworkInterface::IsGameInProgress() && pTargetPed->IsPlayer()) )
			{
				return true;
			}
		}
		
		// For all other cases, reject in-vehicle targeting when using a melee weapon
		if (bUsingMeleeWeapon)
		{
			return false;
		}	

		if (pTargetPed->GetMyVehicle()->InheritsFromBoat())
		{
			return true;
		}

		if(!pTargetPed->GetMyVehicle()->CarHasRoof())
		{
			return true;
		}

		if(pTargetPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ALLOW_TARGETING_OF_OCCUPANTS))
		{
			return true;
		}

		const CVehicleSeatInfo *pSeatInfo = pTargetPed->GetMyVehicle()->GetSeatInfo(pTargetPed);
		if(pSeatInfo && pSeatInfo->GetPedInSeatTargetable())
		{
			return true;
		}

		if(pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowToBeTargetedInAVehicle))
		{
			return true;
		}

		if( pTargetPed->IsAPlayerPed() && pTargetingPed->IsAPlayerPed() && (iFlags&CPedTargetEvaluator::TS_LOCKON_TO_PLAYERS_IN_VEHICLES) )
		{
			return true;
		}

		if(pTargetPed->GetMyVehicle()->m_nVehicleFlags.bVehicleCanBeTargetted)
		{
			return true;
		}

		if(pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) &&
			pTargetPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE) >= CTaskExitVehicle::State_ExitSeat)
		{
			return true;
		}

		// Can always target peds hanging off helis
		if( pTargetPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI )
		{
			// Can only target peds in helis from the front
			const Vec3V vTargetToPlayer = pTargetingPed->GetTransform().GetPosition() - pTargetPed->GetTransform().GetPosition();

			Vec3V vTestDirection = pTargetPed->GetTransform().GetB();

			bool bAllowMPLockon = false;
			s32 attachSeatIndex = pTargetPed->GetAttachCarSeatIndex();
			if(attachSeatIndex > -1)
			{
				const CVehicleSeatInfo *pSeatInfo = pTargetPed->GetMyVehicle()->GetSeatInfo(attachSeatIndex);
				if(pSeatInfo)
				{
					bool bRearSeat = !pSeatInfo->GetIsFrontSeat();
					s32 nEntryPoint = pTargetPed->GetMyVehicle()->GetDirectEntryPointIndexForPed(pTargetPed);
					if(bRearSeat && nEntryPoint > -1)
					{
						bAllowMPLockon = true;

						vTestDirection = pTargetPed->GetTransform().GetA();

						const CVehicleEntryPointInfo *pEntryInfo = pTargetPed->GetMyVehicle()->GetEntryInfo(nEntryPoint);
						if(pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
						{
							vTestDirection *= ScalarV(V_NEGONE);
						}
					}
				}
			}

			//! In MP, only allow targeting of peds in rear seats. Previously we didn't allow any lockon.
			if(!NetworkInterface::IsGameInProgress() || bAllowMPLockon)
			{
				if(IsGreaterThanAll(Dot(vTargetToPlayer, vTestDirection), ScalarV(V_ZERO)))
				{
					return true;	
				}
			}
		}

		if( !CNetwork::IsGameInProgress() )
		{
			CTask* pTask = pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
			if(pTask)
			{
				s32 iState = static_cast<CTaskAimGunVehicleDriveBy*>(pTask)->GetState();
				if(iState >= CTaskAimGunVehicleDriveBy::State_Intro && iState <= CTaskAimGunVehicleDriveBy::State_Outro)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AreNetworkSystemFriends(const CPed *pTargetingPed, const CPed *pPed)
{
	TUNE_GROUP_BOOL(PLAYER_TARGETING, bIgnoreNetworkSystemFriendCheck, false);

	if(!bIgnoreNetworkSystemFriendCheck && pTargetingPed->IsLocalPlayer() && pPed->GetPlayerInfo())
	{
		if (pPed->GetPlayerInfo()->m_FriendStatus == CPlayerInfo::FRIEND_UNKNOWN)
		{
			// This is a slow call and should only be done once per ped, and then cached. Cannot be polled every frame.
			bool bIsFriend = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), pPed->GetPlayerInfo()->m_GamerInfo.GetGamerHandle());
			pPed->GetPlayerInfo()->m_FriendStatus = bIsFriend ? CPlayerInfo::FRIEND_ISFRIEND : CPlayerInfo::FRIEND_NOTFRIEND;
			return bIsFriend;
		}
		else if (pPed->GetPlayerInfo()->m_FriendStatus == CPlayerInfo::FRIEND_ISFRIEND)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

bool IsDisablingMPFriendlyLockon(const CPed *pTargetingPed, const CPed *pPed)
{
	pedAssertf(NetworkInterface::IsGameInProgress(), "Calling IsDisablingMPFriendlyLockon from single player");

	//! Don't lock on if we aren't allowed to lock on to friendly driver.
	// Script have to set a flag to disable friendly (as in PSN/LIVE friend) lockon.
	if(pTargetingPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMPFriendlyLockon))
	{
		return AreNetworkSystemFriends(pTargetingPed, pPed);
	}

	return false;
}

bool CPedTargetEvaluator::CheckBasicTargetExclusions(const CPed* pTargetingPed, 
													 const CEntity* pEntity, 
													 s32 iFlags, 
													 float fMaxLockOnDistance,
													 float fMaxDistance,
													 bool* pbRejectedDueToVehicle,
													 bool* pbOutsideLockOnRange)
{
	Vector3 vLockOnPosition;
	CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vLockOnPosition);

	Vec3V vDelta = VECTOR3_TO_VEC3V(vLockOnPosition) - pTargetingPed->GetTransform().GetPosition();

	// Test the distance
	float fDistanceSq = MagSquared(vDelta).Getf();

#if __BANK
	//! DMKH. Debug to pass all targeting checks to force lock on.
	TUNE_GROUP_BOOL(PLAYER_TARGETING, bByPassBasicTargetExclusionsForPeds, false);
	if(bByPassBasicTargetExclusionsForPeds && pEntity->GetIsTypePed())
	{
		return true;
	}
#endif

	if(fDistanceSq > rage::square(fMaxDistance))
	{
		targetingDebugf(pEntity, "fDistanceSq > rage::square(fMaxDistance) %f:%f", rage::Sqrtf(fDistanceSq), fMaxDistance);
		return false;
	}

	if(pbOutsideLockOnRange && (fDistanceSq > rage::square(fMaxLockOnDistance)) )
	{
		*pbOutsideLockOnRange = true;
	}

	if(GetIsTargetingDisabled(pTargetingPed, pEntity, iFlags))
	{
		targetingDebugf(pEntity, "Targeting disabled");
		return false;
	}

	// PED TARGET EXCLUSIONS
	if(pEntity->GetIsTypePed())
	{
		const CPed* pPed = static_cast<const CPed*>(pEntity);

		if(pPed == pTargetingPed)
		{
			targetingDebugf(pEntity, "can't target ourselves");
			return false;
		}

		if (pTargetingPed->GetMyMount()==pPed) //don't target your mount
		{
			targetingDebugf(pEntity, "can't target own mount");
			return false;
		}

		//! Change entity to rider if ped has one. If no rider, just use as normal.
		//! TO DO - Need to test.
		/*CPed* pRider = pPed->GetSeatManager() && ? pPed->GetSeatManager()->GetDriver() : NULL;
		if(pRider && pRider->IsPlayer())
		{
			pPed = pRider;
		}*/

		if(!pPed->GetIsVisible())
		{
			targetingDebugf(pEntity, "Entity isn't visible");
			return false;
		}
		
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed ))
		{
			targetingDebugf(pEntity, "CPED_CONFIG_FLAG_NeverEverTargetThisPed set");
			return false;
		}

		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableAssistedAimLockon) && pTargetingPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			targetingDebugf(pEntity, "CPED_RESET_FLAG_DisableAssistedAimLockon set");
			return false;
		}

		if(iFlags & TS_HOMING_MISSLE_CHECK)
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHomingMissileLockon))
			{
				targetingDebugf(pEntity, "PCF_DisableHomingMissileLockon set on ped");
				return false;
			}
		}

		if(iFlags & TS_USE_MELEE_SCORING)
		{
			// For melee, water status must match.
			if(pPed->GetIsSwimming() != pTargetingPed->GetIsSwimming())
			{
				targetingDebugf(pEntity, "Water status doesn't match");
				return false;
			}
		}

		// Never allow lockon to other players if we're in a car in mp
		if(NetworkInterface::IsGameInProgress() && pTargetingPed->GetIsDrivingVehicle() && pPed->IsAPlayerPed())
		{
			targetingDebugf(pEntity, "Can't lock onto other players whilst driving a vehicle");
			return false;
		}

		//! Check if we are allowing lock on for friendly players. Note: TS_CALCULATE_MELEE_ACTION indicates we
		//! are trying to perform an actual attack, so don't allow target lock in this case.
		bool bAllowFriendlyLockon = 
			pTargetingPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers) || 
			(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly) && (iFlags & TS_ALLOW_LOCK_ON_IF_TARGET_IS_FRIENDLY));
		
		if(NetworkInterface::IsGameInProgress())
		{
			if(IsDisablingMPFriendlyLockon(pTargetingPed, pPed))
			{
				targetingDebugf(pEntity, "IsDisablingMPFriendlyLockon(pTargetingPed, pPed)");
				return false;
			}
		}
		else
		{
			// Never allow us to lockon to any peds that we are friendly with (in single player only).
			if(!bAllowFriendlyLockon && pTargetingPed->GetPedIntelligence()->IsFriendlyWith(*pPed))
			{
				targetingDebugf(pEntity, "Friendly lockon");
				return false;	
			}
		}

		if (!CPlayerPedTargeting::ms_Tunables.m_AllowDriverLockOnToAmbientPeds && pTargetingPed->GetIsDrivingVehicle() && !(iFlags & TS_ALLOW_DRIVER_LOCKON_TO_AMBIENT_PEDS))
		{
			if (!(iFlags & TS_HOMING_MISSLE_CHECK) && NetworkInterface::IsGameInProgress())
			{
				targetingDebugf(pEntity, "No driveby lockon in MP");
				return false;
			}

			bool bTreatAsAmbientPed = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsAmbientPedForDriverLockOn);

			if (pPed->PopTypeIsMission() && !bTreatAsAmbientPed)
			{
				// Don't allow us to lockon to mission peds that we are friendly with 
				// Note: this won't prevent us from locking on to peds that we are neutral with who like us
				if (pTargetingPed->GetPedIntelligence()->IsFriendlyWith(*pPed))
				{
					targetingDebugf(pEntity, "No driveby lockon to friendly peds");
					return false;
				}
			}
			else
			{
				//! Don't lock on to animals from inside a vehicle. Some are always a threat to you. 
				if(pPed->GetPedType() == PEDTYPE_ANIMAL)
				{
					targetingDebugf(pEntity, "No driveby lockon to animals");
					return false;
				}

				// Only allow lockon to ped threats when driving a vehicle 
				if (CTaskAimGunVehicleDriveBy::ms_OnlyAllowLockOnToPedThreats && !pTargetingPed->GetPedIntelligence()->IsThreatenedBy(*pPed) &&
					!pPed->GetPedIntelligence()->IsThreatenedBy(*pTargetingPed))
				{
					targetingDebugf(pEntity, "No driveby lockon to non threats");
					return false;
				}
			}
		}

		// Can't lock on to peds knocked to the floor
		if(!CLocalisation::KickingWhenDown() && pPed->GetPedResetFlag( CPED_RESET_FLAG_KnockedToTheFloorByPlayer ) && pPed->PopTypeIsRandom())
		{
			targetingDebugf(pEntity, "Entity knocked to the floor");
			return false;
		}

		if(CPedGroups::AreInSameGroup(pTargetingPed, pPed) && !bAllowFriendlyLockon)
		{
			targetingDebugf(pEntity, "In same group");
			return false;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			if(pTargetingPed->IsPlayer())
			{
				if(pPed->IsPlayer())
				{
					if (!NetworkInterface::IsFriendlyFireAllowed(pPed,pTargetingPed))
					{
						targetingDebugf(pEntity, "In MP - no friendly player fire allowed");
						return false;
					}

					// Ignore friendly players in a network game
					if(!pTargetingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers ) && pPed->IsAPlayerPed() )
					{
						if(!NetworkInterface::FriendlyFireAllowed() || pTargetingPed->GetPedResetFlag(CPED_RESET_FLAG_PreventLockonToFriendlyPlayers))
						{
							if (!NetworkInterface::IsPedAllowedToDamagePed(*pTargetingPed, *pPed))
							{
								targetingDebugf(pEntity, "In MP - not allowed to damage player ped");
								return false;
							}
						}
					}
				}
				else
				{
					// Lock on to non-player peds if we can damage them.
					if( !pTargetingPed->IsAllowedToDamageEntity(NULL, pPed) )
					{
						targetingDebugf(pEntity, "In MP - not allowed to damage non player ped");
						return false;
					}
				}
			}

			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			if (pPedObj)
			{
				//Some peds can only be targetted by certain player teams
				if (!pPedObj->GetIsTargettableByTeam(NetworkInterface::GetLocalPlayer()->GetTeam()))
				{
					targetingDebugf(pEntity, "In MP - !GetIsTargettableByTeam");
					return false;
				}

				//Some peds can only be targetted by certain player
				if (!pPedObj->GetIsTargettableByPlayer(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex()))
				{
					targetingDebugf(pEntity, "In MP - !GetIsTargettableByPlayer");
					return false;
				}

                // ignore players in a different tutorial space
                if(pPed->IsPlayer() && pPedObj->GetPlayerOwner() && pPedObj->GetPlayerOwner()->IsInDifferentTutorialSession())
                {
					targetingDebugf(pEntity, "In MP - !IsInDifferentTutorialSession");
                    return false;
				}
			}
		}

		if(!(iFlags & TS_ACCEPT_INJURED_AND_DEAD_PEDS))
		{	
			//! Allow lock on to peds you have just melee killed. B* 1371715.
			bool bAllowLockOnToTakeDownPed = false;
			if(!(iFlags & TS_USE_MELEE_SCORING) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ))
			{
				bool bTargetingPedIsKiller = pPed->GetSourceOfDeath()==pTargetingPed;
				if(!bTargetingPedIsKiller)
				{
					CTask* pMeleeTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT );
					if( pMeleeTask )
					{
						CTaskMeleeActionResult* pMeleeActionTask = static_cast<CTaskMeleeActionResult*>(pMeleeTask);
						if(pMeleeActionTask->GetTargetEntity() == pTargetingPed)
						{
							bTargetingPedIsKiller = true;
						}
					}
				}

				if(bTargetingPedIsKiller && (!pPed->IsDead() || (fwTimer::GetTimeInMilliseconds() < (pPed->GetTimeOfDeath() + ms_Tunables.m_TimeForTakedownTargetAcquiry))))
				{
					bAllowLockOnToTakeDownPed = true;
				}
			}

			if(!bAllowLockOnToTakeDownPed)
			{
				if( pPed->IsDead())
				{
					targetingDebugf(pEntity, "Ped is dead");
					return false;
				}

				// Do not allow peds that are about to die via melee
				if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) ||
					pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
					pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) )
				{
					targetingDebugf(pEntity, "Ped is dying via melee");
					return false;
				}

				if( pPed->IsInjured() && !pPed->GetTargetWhenInjuredAllowed() )
				{
					targetingDebugf(pEntity, "pPed->IsInjured() && !pPed->GetTargetWhenInjuredAllowed()");
					return false;
				}	
			
				pedAssertf(pPed->GetHealth() > 0, "Targeting ped with zero health! Health = %.2f", pPed->GetHealth());
				pedAssertf(!pPed->IsFatallyInjured(), "Targeting ped that is fatally injured! Health = %.2f", pPed->GetHealth());
			}
		}

		if(!GetCanPedBeTargetedInAVehicle(pTargetingPed, pPed, iFlags))
		{
			if(pbRejectedDueToVehicle)
			{
				*pbRejectedDueToVehicle = true;
				targetingDebugf(pEntity, "!GetCanPedBeTargetedInAVehicle - rejected in vehicle = true");
			}
			else
			{
				targetingDebugf(pEntity, "!GetCanPedBeTargetedInAVehicle - rejected in vehicle = false");
			}

			return false;
		}

		//B*1783434 - if we're in the submarine car in sub mode (in water) and flagged to check for peds, only target them if they are also in water
		if (pTargetingPed->GetIsInVehicle() && pTargetingPed->GetVehiclePedInside()->InheritsFromSubmarineCar() && (iFlags & TS_CHECK_PEDS))
		{
			CSubmarineCar* pSubCar = static_cast<CSubmarineCar*>(pTargetingPed->GetVehiclePedInside());
			if (pSubCar && pSubCar->IsInSubmarineMode() && !pPed->GetIsSwimming())
			{
				targetingDebugf(pEntity, "Sub car can only target in water peds");
				return false;
			}
		}

		// Weapons flagged with OnlyLockOnToAquaticVehicles will only target peds in aquatic vehicles or swimming peds
		if (pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetOnlyLockOnToAquaticVehicles() && (iFlags & TS_CHECK_PEDS))
		{
			if (pPed->GetIsInVehicle() ? !pPed->GetVehiclePedInside()->GetIsAquatic() : !pPed->GetIsSwimming())
			{
				targetingDebugf(pEntity, "Submarine can only target in water peds or aquatic vehicles");
				return false;
			}
		}
	}
	// VEHICLE TARGET EXCLUSIONS
	else if(pEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

		// Don't target invisible vehicles.
		if(!pEntity->GetIsVisible())
		{
			targetingDebugf(pEntity, "vehicle not visible");
			return false;
		}

		// Don't target vehicles that are being re-spotted (respawned).
		if(NetworkInterface::IsGameInProgress() && pVehicle->IsBeingRespotted())
		{
			targetingDebugf(pEntity, "vehicle being respotted");
			return false;
		}

		const CVehicle* pTargetingPedVehicle = pTargetingPed->GetVehiclePedInside();
		if (pTargetingPedVehicle)
		{
			// Don't target the vehicle your inside of
			if(pTargetingPedVehicle == pEntity)
			{
				targetingDebugf(pEntity, "targeting ped is inside vehicle");
				return false;
			}

			// Don't target our parent vehicle if in a trailer.
			if (pTargetingPedVehicle && pTargetingPedVehicle->InheritsFromTrailer())
			{
				const CTrailer* pTrailer = static_cast<const CTrailer*>(pTargetingPedVehicle);
				const CVehicle* pTrailerParentVehicle = pTrailer->GetAttachedToParent();
				if (pTrailerParentVehicle && pTrailerParentVehicle == pEntity)
				{
					targetingDebugf(pEntity, "inside attached trailer");
					return false;
				}
			}
		}

		// Allow scripters to mark homing targets as untargetable
		if(iFlags & TS_HOMING_MISSLE_CHECK)
		{
			// For now do not allow homing weapons to lock-on trains
			if( pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN )
			{
				targetingDebugf(pEntity, "homing missile - can't target train");
				return false;
			}

			if (!pVehicle->m_nVehicleFlags.bAllowHomingMissleLockon)
			{
				targetingDebugf(pEntity, "homing missile - bAllowHomingMissleLockon is false");
				return false;
			}

			if (!pVehicle->m_bAllowHomingMissleLockOnSynced)
			{
				targetingDebugf(pEntity, "homing missile - m_bAllowHomingMissleLockOnSynced is false");
				return false;
			}
		}

		bool bVehicleWrecked = pVehicle->GetStatus()==STATUS_WRECKED;
		if(!bVehicleWrecked)
		{
			aiTask *pCrashTask = pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH);
			if(pCrashTask)
			{
				bVehicleWrecked = true;
			}
		}

		// Intact vehicles
		bool bAlwayTarget = pVehicle->m_nVehicleFlags.bIsATargetPriority || pVehicle->m_nVehicleFlags.bTargettableWithNoLos;
		if( !bAlwayTarget && (iFlags & TS_PREFER_INTACT_VEHICLES) && bVehicleWrecked )
		{
			targetingDebugf(pEntity, "Prefer intact vehicles - vehicle wrecked");
			return false;
		}

		// Check vehicle targeting flags
		if(!(iFlags & TS_ACCEPT_ALL_VEHICLES))
		{
			// only allow lock-on if the vehicle is targettable, not wrecked, and there are no peds inside (or else the ped lock-on will use the bVehicleCanBeTargetted flag and will allow lock on to the ped inside the vehicle)
			if (!pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted)
			{
				targetingDebugf(pEntity, "!pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted");
				return false;
			}

			if(bVehicleWrecked)
			{
				targetingDebugf(pEntity, "vehicle wrecked");
				return false;
			}

			for (s32 iSeat= 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
			{
				CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
				if (pPedInSeat && !pPedInSeat->IsInjured() && pPedInSeat->GetIsVisible() )
				{
					targetingDebugf(pEntity, "pVehicle->HasAlivePedsInIt()");
					return false;
				}
			}
		}

		// B*2406765: Check if script have flagged any of the peds in the vehicle to block locking on to the vehicle they're inside.
		if (iFlags & TS_HOMING_MISSLE_CHECK)
		{
			for (s32 iSeat= 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
			{
				CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
				if (pPedInSeat && pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHomingMissileLockForVehiclePedInside))
				{
					targetingDebugf(pEntity, "pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHomingMissileLockForVehiclePedInside) - Set by script");
					return false;
				}
			}
		}

		// B*2234940: Don't allow homing launcher to lock on to bicycles
		if (pVehicle->InheritsFromBicycle() && pTargetingPed->GetWeaponManager() && pTargetingPed->GetWeaponManager()->GetEquippedWeaponInfo() && pTargetingPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsOnFootHoming())
		{
			targetingDebugf(pEntity, "Homing Launcher - don't lock on to bicycles");
			return false;
		}

		CPed* pDriver =	pVehicle->GetDriver();

		//! If the vehicle has no driver, loop through the seats to see if a passenger exists in the vehicle (rare, but happens)
		if (!pDriver)
		{
			for(s32 iSeat = 1; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
			{
				CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
				if(pPassenger)
				{
					pDriver = pPassenger;
				}
			}
		}
		
		//! Ignore driver if they are dead and we aren't allowing lock on to dead/injured peds.
		if(pDriver && pDriver->IsDead() && !(iFlags & TS_ACCEPT_INJURED_AND_DEAD_PEDS))
		{
			pDriver = NULL;
		}

		if(!pDriver && (pVehicle->m_nVehicleFlags.bAllowNoPassengersLockOn || (pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsOnFootHoming())))
		{
			//! If we have no driver, and are allowing lock on with no passengers, this is fine.
		}
		else if(pDriver && (iFlags & TS_HOMING_MISSLE_CHECK) && pDriver->GetPedResetFlag(CPED_RESET_FLAG_AllowHomingMissileLockOnInVehicle))
		{
			//! If homing missile, allow lock on if ped is flagged as such.
		}
		else
		{
			if( (iFlags & TS_ACCEPT_ALL_OCCUPIED_VEHICLES))
			{
				if( !pDriver )
				{
					targetingDebugf(pEntity, "vehicle - has no driver");
					return false;
				}

				//! Don't lock on if we aren't allowed to lock on to friendly driver (due to being on friend list).
				if(NetworkInterface::IsGameInProgress())
				{
					if(IsDisablingMPFriendlyLockon(pTargetingPed, pDriver))
					{
						targetingDebugf(pEntity, "vehicle - IsDisablingMPFriendlyLockon");
						return false;
					}
				}

				// On-Foot Homing Launcher: Don't lock on to vehicle if we aren't allowed to damage the driver.
				if (pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsOnFootHoming() && !pTargetingPed->IsAllowedToDamageEntity(pTargetingPed->GetEquippedWeaponInfo(), pDriver))
				{
					targetingDebugf(pEntity, "vehicle - OnFootHoming - !IsAllowedToDamageEntity");
					return false;
				}

				// Only allows us to lock onto aquatic vehicles with flag OnlyLockOnToAquaticVehicles
				if(pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetOnlyLockOnToAquaticVehicles() && !pVehicle->GetIsAquatic())
				{
					targetingDebugf(pEntity, "vehicle - OnlyLockOnToAquaticVehicles");
					return false;
				}

				//! If the ped we have isn't a player, see if any passengers are players and use them for evaluation instead  (i.e. players in back of script taxi)
				if (!pDriver->IsPlayer() && pVehicle->GetSeatManager()->GetNumPlayers() > 0)
				{
					for(s32 iSeat = 1; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
					{
						CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
						if(pPassenger && pPassenger->IsPlayer())
						{
							pDriver = pPassenger;
						}
					}
				}

				bool bCanDamagePlayer = pDriver->IsPlayer() && pTargetingPed->IsAllowedToDamageEntity(NULL, pDriver);

				//! Note: Accept all occupied aircraft if non friendly (and driver is in an aircraft).
				bool bBetweenAirCraft = pTargetingPed->GetVehiclePedInside() && pTargetingPed->GetVehiclePedInside()->GetIsAircraft() && pVehicle->GetIsAircraft();
				bool bBetweenVehicleAndOnFootHomingWeapon = pVehicle && !pTargetingPed->GetIsInVehicle() && pTargetingPed->GetWeaponManager() && pTargetingPed->GetWeaponManager()->GetEquippedWeaponInfo() && pTargetingPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsOnFootHoming();
				if(bBetweenAirCraft || bBetweenVehicleAndOnFootHomingWeapon)
				{
					if (!bCanDamagePlayer && pTargetingPed->GetPedIntelligence()->IsFriendlyWith(*pDriver))
					{
						targetingDebugf(pEntity, "vehicle - friendly aircraft");
						return false;
					}
				}
				else
				{
					bool bCheckRelationships = true;
					
					//! If we are targeting a player, 1st check targeting pref we have set up. If these return false, fall back to rel groups.
					if(bCanDamagePlayer && pDriver->IsPlayer() && IsPlayerAThreat(pTargetingPed, pDriver) )
					{
						bCheckRelationships = false;
					}

					const CRelationshipGroup* pRelGroup = pTargetingPed->GetPedIntelligence()->GetRelationshipGroup();
					if( bCheckRelationships && pRelGroup )
					{
						const int driverRelationshipGroupIndex = pDriver->GetPedIntelligence()->GetRelationshipGroupIndex();
						bool bDisliked = pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_DISLIKE, driverRelationshipGroupIndex ) ||
										 pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_WANTED, driverRelationshipGroupIndex ) ||
										 pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_HATE, driverRelationshipGroupIndex );

						bool bThreatened = pTargetingPed->GetPedIntelligence()->IsThreatenedBy(*pDriver) || pDriver->GetPedIntelligence()->IsThreatenedBy(*pTargetingPed);

						if( !bDisliked && !bThreatened)
						{
							targetingDebugf(pEntity, "vehicle - non threat");
							return false;
						}
					}
				}
			}

			// Otherwise we only care about friendlies
			else if(!pTargetingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers ) &&
					 pDriver && 
					 pTargetingPed->GetPedIntelligence()->IsFriendlyWith( *pDriver ) )
			{
				targetingDebugf(pEntity, "vehicle - friendly with driver");
				return false;
			}
		}
	}
	// OBJECT TARGET EXCLUSIONS
	else if(pEntity->GetIsTypeObject())
	{
		const CObject* pObj = static_cast<const CObject*>(pEntity);
		if(!pObj->CanBeTargetted() || pObj->m_nFlags.bHasExploded)
		{
			targetingDebugf(pEntity, "Object cannot be targeted");
			return false;
		}
	}
	else
	{
		targetingDebugf(pEntity, "unsupported entity type");
		return false;
	}

	return true;
}

bool CPedTargetEvaluator::GetNeedsLineOfSightToTarget(const CEntity* pEntity)
{
	if(pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			if(static_cast<const CPed*>(pEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_TargettableWithNoLos ))
			{
				return false;
			}
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			if (static_cast<const CVehicle*>(pEntity)->m_nVehicleFlags.bTargettableWithNoLos)
			{
				return false;
			}
		}
		else if (pEntity->GetIsTypeObject())
		{
			if (static_cast<const CObject*>(pEntity)->m_nObjectFlags.bTargettableWithNoLos)
			{
				return false;
			}
		}
	}

	return true;
}

bool CPedTargetEvaluator::GetIsLineOfSightBlocked(const CPed* pTargetingPed, 
												  const CEntity* pEntity, 
												  const bool bIgnoreTargetsCover, 
												  const bool bAlsoIgnoreLowObjects, 
												  const bool bUseTargetableDistance, 
												  const bool bIgnoreVehiclesForLOS, 
												  const bool bUseCoverVantagePointForLOS,
												  const bool bUseMeleePosForLOS,
												  const bool bTestPotentialPedTargets,
												  s32 iFlags)
{
	bool bLosBlocked = false;
	if(pEntity->GetIsTypePed())
	{
		const CPed* pPed = static_cast<const CPed*>(pEntity);

		// Don't ignore vehicles if the ped is standing on/in a vehicle (but not technically "inside" one).
		bool bShouldIgnoreVehiclesForLOS = bIgnoreVehiclesForLOS;
		if (pPed && !pPed->GetIsInVehicle() && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
		{
			bShouldIgnoreVehiclesForLOS = false;
		}

		bLosBlocked = GetIsLineOfSightBlockedBetweenPeds(pTargetingPed, 
			pPed, 
			bIgnoreTargetsCover, 
			bAlsoIgnoreLowObjects, 
			bUseTargetableDistance, 
			bShouldIgnoreVehiclesForLOS, 
			bUseCoverVantagePointForLOS,
			bUseMeleePosForLOS,
			bTestPotentialPedTargets);
	}
	else
	{
		Vector3 vStart = VEC3V_TO_VECTOR3(pTargetingPed->GetTransform().GetPosition())+ZAXIS;
		Vector3 vEnd;
		CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vEnd);

		// Change the end position to take into account the los targeting distance
		if (bUseTargetableDistance)
		{
			CEntity::GetEntityModifiedTargetPosition(*pEntity, vStart, vEnd);
		}

		const s32 LOCAL_NUM_INTERSECTIONS = 2;

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<LOCAL_NUM_INTERSECTIONS> probeResults;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetExcludeEntity(pTargetingPed);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		if( bIgnoreVehiclesForLOS )
		{
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_OBJECT_TYPE);
		}
		else
		{
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		}

		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// Check whether any of the intersections are not our target
			WorldProbe::ResultIterator it;
			for(it = probeResults.begin(); it < probeResults.last_result() && !bLosBlocked; ++it)
			{
				CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());

				// Ignore our own vehicle
				if(pHitEntity != pEntity && (!pHitEntity || pHitEntity != pTargetingPed->GetMyVehicle()))
				{
					//B*3190447: Check that hit entity isn't attached to our target (like prop objects in the back of a truck!)
					bool bChildObject = pHitEntity->GetIsTypeObject() && pHitEntity->GetAttachParent() == pEntity;
					if (!bChildObject)
					{
						// There's something other than the other instance in the way.
						bLosBlocked = true;
					}
				}
			}
		}
	}

	return bLosBlocked;
}

bool CPedTargetEvaluator::GetIsLineOfSightBlockedBetweenPeds(const CPed* pPed1, 
															 const CPed* pPed2, 
															 const bool bIgnoreTargetsCover, 
															 const bool bAlsoIgnoreLowObjects, 
															 const bool bUseTargetableDistance, 
															 const bool bIgnoreVehiclesForLOS, 
															 const bool bUseCoverVantagePointForLOS,
															 const bool bUseMeleePosForLOS,
															 const bool bTestPotentialPedTargets)
{
	s32 iTargetOptions = 0;
	if(bIgnoreTargetsCover)		iTargetOptions |= TargetOption_IgnoreTargetsCover;
	if(bAlsoIgnoreLowObjects)	iTargetOptions |= TargetOption_IgnoreLowObjects;
	if(bUseTargetableDistance)	iTargetOptions |= TargetOption_UseTargetableDistance;
	if(bIgnoreVehiclesForLOS)	iTargetOptions |= TargetOption_IgnoreVehicles;
	if(bUseCoverVantagePointForLOS) iTargetOptions |= TargetOption_UseCoverVantagePoint;
	if(bTestPotentialPedTargets) iTargetOptions |= TargetOption_TestForPotentialPedTargets;
	if(bUseMeleePosForLOS) 
	{
		iTargetOptions |= TargetOption_UseMeleeTargetPosition;
	}
	else
	{
		iTargetOptions |= TargetOption_IgnoreLowVehicles;
	}

	iTargetOptions |= TargetOption_IgnoreSmoke;

	return !CPedGeometryAnalyser::CanPedTargetPed(*pPed1, *pPed2, iTargetOptions);
}

float CPedTargetEvaluator::GetScoreBasedOnHeading(const CEntity* pEntity, 
												  const Vector3& vStartPos, 
												  s32 iFlags, 
												  const Vector3& vComparisonHeading, 
												  float fOrientationDiff, 
												  HeadingDirection eDirection,
												  bool bInInclusionRangeFailedHeading,
												  float fHeadingFalloff,
												  const CPed* pTargetingPed)
{
	Vector3 vLockOnPosition;
	CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vLockOnPosition);

	Vector3 vDelta = vLockOnPosition - vStartPos;
	float fDistanceSq = vDelta.Mag2();

	// See if (and how much) we care about relative orientation.
	// This is done to compensate to make melee targeting of close peds
	// make more sense since the close melee attack target is less sensitive
	// to facing direction than a gun would be (since the melee system will
	// an appropriate attack on that target based on their pose and position).
	float fScoreWeighting = 1.0f;
	if(iFlags & TS_ACCEPT_HEADING_FAILURES)
	{
		// Create a blending zone in which we interpolate between
		// not caring about orientation and caring about it as normal to
		// hopefully make the transition between the two be intuitive to the
		// player.
		const float fDistAtWhichDontCare		= 1.0f;	// Close range.
		const float fDistAtWhichToUseScoredVal	= 2.0f;	// Further range.
		const float fDistance = rage::Sqrtf(fDistanceSq);
		if(fDistance < fDistAtWhichDontCare)
		{
			return 1.0f;
		}
		else if(fDistance < fDistAtWhichToUseScoredVal)
		{
			fScoreWeighting = (fDistance - fDistAtWhichDontCare) / (fDistAtWhichToUseScoredVal - fDistAtWhichDontCare);
		}
		else // implies (dist >= distAtWhichToUseScoredVal)
		{
			fScoreWeighting = 1.0f;
		}
	}

	static dev_float MAX_TARGET_SWITCHING_HEIGHT_DIFF = 0.5f;

	vDelta.Normalize();
	float fDot = vComparisonHeading.Dot(vDelta);

	//! Nerf heading if it is in inclusion range behind ped. Don't want to lock on unless there are no targets in proper 
	//! targeting cone.
	if(bInInclusionRangeFailedHeading)
	{
		fDot *= 0.5f;
	}

	if(eDirection == HD_JUST_LEFT)
	{
		if(fOrientationDiff > 0.0f && ABS(vComparisonHeading.z - vDelta.z) < MAX_TARGET_SWITCHING_HEIGHT_DIFF)
		{
			float fScore = rage::Lerp(fScoreWeighting, 1.0f, fDot);
			return fScore;
		}
	}
	else if(eDirection == HD_JUST_RIGHT)
	{
		if(fOrientationDiff < 0.0f && ABS(vComparisonHeading.z - vDelta.z) < MAX_TARGET_SWITCHING_HEIGHT_DIFF)
		{
			float fScore = rage::Lerp(fScoreWeighting, 1.0f, fDot);
			return fScore;
		}
	}
	else if(eDirection == HD_BOTH)
	{
		float fScore = rage::Lerp(fScoreWeighting, 1.0f, fDot);
		fScore = Sign(fScore) * Powf(rage::Abs(fScore), fHeadingFalloff);	// Don't use a negative number when calling Powf - can return nan.
		return fScore;
	}

	return 0.0f;
}

float CPedTargetEvaluator::GetScoreBasedOnDistance(const CPed* pTargetingPed, const CEntity* pEntity, float fDistanceFallOff, s32 iFlags)
{
	Vector3 vLockOnPosition;
	CPedTargetEvaluator::GetLockOnTargetAimAtPos(pEntity, pTargetingPed, iFlags, vLockOnPosition);

	// Take the relative distance into account
	Vec3V vDelta = VECTOR3_TO_VEC3V(vLockOnPosition) - pTargetingPed->GetTransform().GetPosition();
	float fDistance = Mag(vDelta).Getf();
	if (fDistance <= 1.0f)
	{
		return 1.0f;
	}

	return 1.0f / rage::Powf(fDistance, fDistanceFallOff);
}

float CPedTargetEvaluator::GetScoreBasedOnThreatAssesment(const CPed* pTargetingPed, const CEntity* pEntity)
{
	float fResult = ms_Tunables.m_PrioNeutral;

	switch(pEntity->GetType())
	{
	case ENTITY_TYPE_PED:
		{
			fResult = GetPedThreatAssessment( pTargetingPed, pEntity );
		}
		break;

	case ENTITY_TYPE_OBJECT:
		{
			const CObject* pObject = static_cast<const CObject*>(pEntity);
			if (pObject->m_nObjectFlags.bIsATargetPriority)
			{
				fResult = ms_Tunables.m_PrioScriptedHighPriority;
			}
			else if(pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
			{
				fResult = ms_Tunables.m_PrioHarmless;
			}
			else
			{
				fResult = ms_Tunables.m_PrioPotentialThreat;
			}
		}
		break;

	case ENTITY_TYPE_VEHICLE:
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
			
			// Use the driver threat as the vehicle threat
			const CPed* pDriver = pVehicle->GetDriver();
			if( pDriver )
			{	
				fResult = GetPedThreatAssessment( pTargetingPed, pDriver );
			}

			// B*2154004: On-foot homing rocket targeting improvements.
			if (pTargetingPed && pTargetingPed->GetEquippedWeaponInfo() && pTargetingPed->GetEquippedWeaponInfo()->GetIsOnFootHoming() && pTargetingPed->GetPlayerInfo())
			{
				fResult = ms_Tunables.m_PrioPotentialThreat;

				// If we have a valid free aim target and we're considering it for lock-on, score it higher to improve responsiveness.
				CPlayerPedTargeting &targeting = pTargetingPed->GetPlayerInfo()->GetTargeting();
				
				if (targeting.GetFreeAimTarget())
				{
					bool bFreeAimTargetIsTargetEntity = false;
					if (targeting.GetFreeAimTarget()->GetIsTypeVehicle() && targeting.GetFreeAimTarget() == pEntity)
					{
						bFreeAimTargetIsTargetEntity = true;
					}
					else if (targeting.GetFreeAimTarget()->GetIsTypePed())
					{
						CPed *pPed = static_cast<CPed*>(targeting.GetFreeAimTarget());
						if (pPed && pPed->GetIsInVehicle() && pPed->GetVehiclePedInside() == pVehicle)
						{
							bFreeAimTargetIsTargetEntity = true;
						}
					}

					if (bFreeAimTargetIsTargetEntity)
					{
						static dev_float fFreeAimTargetMult = 2.0f;
						fResult *= fFreeAimTargetMult;
					}
				}

				// If we have a driver or the driver has just left the target vehicle, score it accordingly so it's easier to lock on to.
				if (pVehicle->GetDriver() || pVehicle->GetLastDriver())
				{
					CPed *pDriverPed = pVehicle->GetDriver() ? pVehicle->GetDriver() : pVehicle->GetLastDriver();
					if (pDriverPed)
					{
						static dev_float fMaxEmptyVehicleTime = 5000.0f;
						u32 fTimeSinceLasthadADriver = NetworkInterface::GetSyncedTimeInMilliseconds() - pVehicle->m_LastTimeWeHadADriver;
						if (fTimeSinceLasthadADriver < fMaxEmptyVehicleTime)
						{
							fResult = GetPedThreatAssessment( pTargetingPed, pDriverPed);
						}
					}
				}
			}

			// Special scripted override case
			if( pVehicle->m_nVehicleFlags.bIsATargetPriority )
				fResult = ms_Tunables.m_PrioScriptedHighPriority;
		}
		break;

	default:
		pedAssertf(0, "Unhandled entity type [%d]", pEntity->GetType());
		break;
	}

	return fResult;
}

float CPedTargetEvaluator::GetScoreBasedOnMeleeAssesment(const CPed* pTargetingPed, const CEntity* pEntity, bool bIgnoreLowThreats)
{
	float fResult = ms_Tunables.m_PrioHarmless;

	switch(pEntity->GetType())
	{
		case ENTITY_TYPE_PED:
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if( pPed->IsDead() || pPed->IsDeadByMelee() )
			{
				fResult = ms_Tunables.m_PrioMeleeDead;
			}
			else if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ThisPedIsATargetPriority ))
			{
				fResult = ms_Tunables.m_PrioScriptedHighPriority;
			}
			else
			{
				Assert( pPed->GetPedIntelligence() );
				
				//! Choose combat state as opposed to melee state. Should be a bit more forgiving.
				const bool bInCombat = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT );
				const bool bThreat = pPed->GetPedIntelligence()->IsThreatenedBy(*pTargetingPed) || pTargetingPed->GetPedIntelligence()->IsThreatenedBy(*pPed);
				if( bInCombat || bThreat)
				{
					fResult = IsDownedPedThreat( pPed, true ) ? ms_Tunables.m_PrioMeleeDownedCombatThreat : ms_Tunables.m_PrioMeleeCombatThreat;
				}
				else if( IsDownedPedThreat( pPed, true ) )
					fResult = ms_Tunables.m_PrioMeleeInjured;
				else
					fResult = ms_Tunables.m_PrioMeleePotentialThreat;

				if(bIgnoreLowThreats && pPed->GetPedModelInfo()->GetTargetingThreatModifier() < 1.0f)
				{
					//! Ignore lower threats for melee purposes. These are typically things we don't want to lock on to (e.g. fish, rats etc).
					fResult = 0.0f;
				}
			}
		}
		break;
		default:
			pedAssertf(0, "Unhandled entity type [%d]", pEntity->GetType());
			break;
	}

	return fResult;
}

bool CPedTargetEvaluator::IsPlayerAThreat(const CPed* pTargetingPed, const CPed *pOtherPLayer)
{
	Assert(pOtherPLayer->IsPlayer());

	CPlayerPedTargeting::ePlayerTargetLevel eTargetLevel = CPlayerPedTargeting::GetPlayerTargetLevel();

	const bool bNetworkFriends = AreNetworkSystemFriends(pTargetingPed, pOtherPLayer);

	u32 uLastTimeDamagedLocalPlayer = pOtherPLayer->GetPlayerInfo() ? pOtherPLayer->GetPlayerInfo()->GetLastTimeDamagedLocalPlayer() : 0;

	switch(eTargetLevel)
	{
	case(CPlayerPedTargeting::PTL_TARGET_EVERYONE):
		return true;
	case(CPlayerPedTargeting::PTL_TARGET_STRANGERS):
		{
			if(!bNetworkFriends)
			{
				return true;
			}
		}
		break;
	case(CPlayerPedTargeting::PTL_TARGET_ATTACKERS):
		{
			if(!bNetworkFriends && 
				(uLastTimeDamagedLocalPlayer > 0) &&
				(fwTimer::GetTimeInMilliseconds() < (uLastTimeDamagedLocalPlayer + ms_Tunables.m_TimeToIncreaseFriendlyFirePlayer2PlayerPriority)) )
			{
				return true;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

float CPedTargetEvaluator::GetPedThreatAssessment( const CPed* pTargetingPed, const CEntity* pEntity )
{
	float fResult = ms_Tunables.m_PrioPotentialThreat;

	const CPed* pPed = static_cast<const CPed*>(pEntity);

	bool bFriendlyWithPlayer = pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetingPed) && pTargetingPed->GetPedIntelligence()->IsFriendlyWith(*pPed);
	
	bool bHatesPlayer;

	//! Prioritise animals if they move to a combat state with the player.
	if(pPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		bHatesPlayer = false;
	}
	else
	{
		bHatesPlayer = pPed->GetPedIntelligence()->IsThreatenedBy(*pTargetingPed) || pTargetingPed->GetPedIntelligence()->IsThreatenedBy(*pPed);
	}

	bool bDoDownedPedModifier = true;

	// Override the friendly setting so players can target each other
	if(pTargetingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers ))
	{
		if(pPed && pPed->IsAPlayerPed())
		{
			bFriendlyWithPlayer = false;
			bHatesPlayer = true;
		}
	}

	bool bInjured = pPed->IsInjured();
	// Do not allow peds that are about to die via melee
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) ||
		pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
		pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) )
	{
		bInjured = true;
	}

	const bool bNetworkFriends = AreNetworkSystemFriends(pTargetingPed, pPed);

	const bool bPlayerToPlayer = (pPed->IsAPlayerPed() && pTargetingPed->IsAPlayerPed());

	const bool bInCombat = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT);
	const bool bPriorityTarget = (!bInjured || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_TargetWhenInjuredAllowed )) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ThisPedIsATargetPriority );

	u32 uLastTimeDamagedLocalPlayer = pPed->GetPlayerInfo() ? pPed->GetPlayerInfo()->GetLastTimeDamagedLocalPlayer() : 0;
	
	if(CPedGroups::AreInSameGroup(pTargetingPed, pPed))
	{
		fResult = ms_Tunables.m_PrioIngangOrFriend;
		bDoDownedPedModifier = false;
	}
	else if(bPriorityTarget)
	{
		fResult = ms_Tunables.m_PrioScriptedHighPriority;
	}
	else if(!bInjured && bFriendlyWithPlayer && bPlayerToPlayer)
	{
		CPlayerPedTargeting::ePlayerTargetLevel eTargetLevel = CPlayerPedTargeting::GetPlayerTargetLevel();

#if __BANK
		TUNE_GROUP_INT(PLAYER_TARGETING, iDebugTargetLevel, -1, -1, CPlayerPedTargeting::PTL_MAX, 1);
		if(iDebugTargetLevel >= 0)
		{
			eTargetLevel = static_cast<CPlayerPedTargeting::ePlayerTargetLevel>(iDebugTargetLevel);
		}
#endif

		switch(eTargetLevel)
		{
		case(CPlayerPedTargeting::PTL_TARGET_EVERYONE):
			fResult = ms_Tunables.m_PrioPlayer2PlayerEveryone;
			break;
		case(CPlayerPedTargeting::PTL_TARGET_STRANGERS):
			if(!bNetworkFriends)
			{
				fResult = ms_Tunables.m_PrioPlayer2PlayerStrangers;
			}
			break;
		case(CPlayerPedTargeting::PTL_TARGET_ATTACKERS):
			{
				if(!bNetworkFriends && 
					(uLastTimeDamagedLocalPlayer > 0) &&
					(fwTimer::GetTimeInMilliseconds() < (uLastTimeDamagedLocalPlayer + ms_Tunables.m_TimeToIncreaseFriendlyFirePlayer2PlayerPriority)) )
				{
					fResult = ms_Tunables.m_PrioPlayer2PlayerAttackers;
					break;
				}
			}
		default:
			break;
		}
	}
	else if(!bInjured && !bFriendlyWithPlayer && (bInCombat || bHatesPlayer || bPlayerToPlayer))
	{
		if (pTargetingPed->IsLocalPlayer() && !pTargetingPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			if (bPlayerToPlayer)
			{
				fResult = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) ? ms_Tunables.m_PrioPlayer2PlayerCuffed : ms_Tunables.m_PrioPlayer2Player;
			}
			else
			{
				fResult = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) ? ms_Tunables.m_PrioMissionThreatCuffed : ms_Tunables.m_PrioMissionThreat;
			}
		}
	}
	else if( bInjured )
	{	
		if (pTargetingPed->IsLocalPlayer() && !pTargetingPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			fResult = ms_Tunables.m_PrioNeutralInjured;
		}
	}

	//! Modify threat value, but only if script haven't flagged this ped as being a high priority target.
	if(!bPriorityTarget)
	{
		if(pPed->GetTargetThreatOverride() >= 0.0f)
		{
			fResult = pPed->GetTargetThreatOverride();
		}

		fResult *= pPed->GetPedModelInfo()->GetTargetingThreatModifier();
	}

	if(bDoDownedPedModifier && IsDownedPedThreat(pPed))
	{
		fResult *= ms_Tunables.m_DownedThreatModifier;
	}

	return fResult;
}

float CPedTargetEvaluator::GetScoreBasedOnTask(const CPed* pTargetingPed, const CEntity* pEntity, s32 iFlags, bool &bDisallowAimBonus)
{
	if(pEntity->GetIsTypePed())
	{
		//! Don't do cover modifier if we aren't using vantage point to aim at (as we'll just use LOS instead, which is better).
		if(iFlags & TS_USE_COVER_VANTAGE_POINT_FOR_LOS)
		{
			const CPed *pTargetPed = static_cast<const CPed*>(pEntity);
			if(pTargetPed->GetIsInCover())
			{
				const CTaskInCover* pTaskInCover = static_cast<const CTaskInCover*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				if(pTaskInCover && !pTaskInCover->IsInDangerousTargetingState() && GetNeedsLineOfSightToTarget(pTargetPed))
				{
					if(GetIsLineOfSightBlocked(pTargetingPed, 
						pEntity, 
						false, 
						(iFlags & TS_IGNORE_LOW_OBJECTS) != 0, 
						(iFlags & TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0, 
						(iFlags & TS_IGNORE_VEHICLES_IN_LOS) != 0, 
						false,
						(iFlags & TS_USE_MELEE_LOS_POSITION)!=0,
						false,
						iFlags))
					{
						bDisallowAimBonus = true;
						return ms_Tunables.m_InCoverScoreModifier;
					}
				}
			}
		}
	}

	return 1.0f;
}

bool CPedTargetEvaluator::IsDownedPedThreat(const CPed* pPed, bool bConsiderFlinch)
{
	Assert(pPed);

	//! Don't consider if we specifically want to ignore it.
	if(pPed == ms_pIgnoreDownedPed)
	{
		return false;
	}

	//! Don't treat as a downed ped if in combat writhe.
	CTaskWrithe* pTaskWrithe = pPed->GetPedIntelligence()->GetTaskWrithe();
	if(pTaskWrithe)
	{
		if(!pTaskWrithe->IsInCombat())
		{
			return true;
		}
	}

	//! Don't treat as a downed ped if we have an active gun task.
	CTaskGun* pTaskGun = pPed->GetPedIntelligence()->GetTaskGun();
	if(pTaskGun && (pTaskGun->GetIsAiming() || pTaskGun->GetIsFiring()))
	{
		return false;
	}

	CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
	if(pNMTask)
	{
		bool bRagdollBalanceFailed = false;
		CTaskGetUp* pGetUpTask = static_cast<CTaskGetUp*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP));
		if(pGetUpTask)
		{
			bRagdollBalanceFailed = pGetUpTask->IsSlopeFixupActive(pPed);
		}
		else
		{
			bRagdollBalanceFailed = pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE);
		}

		if(!bRagdollBalanceFailed && bConsiderFlinch)
		{
			CTaskNMFlinch* pNMFlinch = smart_cast<CTaskNMFlinch*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_FLINCH));
			if(pNMFlinch)
			{
				bRagdollBalanceFailed = true;
			}
		}

		if(bRagdollBalanceFailed)
		{
			return true;
		}
	}

	return false;
}

bool CPedTargetEvaluator::CanBeTargetedInRejectionDistance(const CPed* pTargetingPed, const CWeaponInfo* pWeaponInfo, const CEntity *pEntity)
{
	if(pEntity && pWeaponInfo)
	{
		//! Extend lock on range for peds in heli's.
		if(pEntity->GetIsTypePed() && !pWeaponInfo->GetIsMelee())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if(pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->InheritsFromHeli())
			{
				return true;
			}
		}

		//! Extend lock on range if target is in an aircraft & we are also in an aircraft.
		if(pWeaponInfo->GetIsHoming() && pTargetingPed->GetVehiclePedInside() && pTargetingPed->GetVehiclePedInside()->GetIsAircraft())
		{
			if (pTargetingPed->IsLocalPlayer())
			{
				if (!pTargetingPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled())
					return false;
			}

			if(pEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pEntity);
				if(pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->GetIsAircraft())
				{
					return true;
				}
			}

			if(pEntity->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
				if(pVehicle->GetIsAircraft())
				{
					return true;
				}
			}
		}
	}

	return false;
}

#if __BANK
void CPedTargetEvaluator::AddWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Player Tunables");
	if (pBank)
	{
		bkBank& bank = *pBank;
		bank.AddToggle("Use flat heading for lockon angle check", &CPedTargetEvaluator::USE_FLAT_HEADING);
		bank.AddToggle("Allow vehicle lockon",					&CPedTargetEvaluator::CHECK_VEHICLES);
		bank.AddToggle("Debug Player Scoring", &DEBUG_LOCKON_TARGET_SCRORING_ENABLED);
		bank.AddToggle("Debug Vehicle Scoring", &DEBUG_VEHICLE_LOCKON_TARGET_SCRORING_ENABLED);
		bank.AddToggle("Debug Melee Scoring", &DEBUG_MELEE_TARGET_SCRORING_ENABLED);
		bank.AddToggle("Debug Target Switch Scoring", &DEBUG_TARGET_SWITCHING_SCORING_ENABLED);
		bank.AddToggle("Debug Focus Entity Logging", &DEBUG_FOCUS_ENTITY_LOGGING);
		bank.AddToggle("Process Rejection String", &PROCESS_REJECTION_STRING);
		bank.AddToggle("Display Rejection String Peds", &DISPLAY_REJECTION_STRING_PEDS);
		bank.AddToggle("Display Rejection String Objects", &DISPLAY_REJECTION_STRING_OBJECTS);
		bank.AddToggle("Display Rejection String Vehicles", &DISPLAY_REJECTION_STRING_VEHICLES);

		static const char * g_pNamesModes[eDSM_Max] =
		{
			"None",
			"Full",
			"Concise",
		};

		bank.AddCombo("Debug Scoring Mode:", (int*)&DEBUG_TARGETSCORING_MODE, eDSM_Max, g_pNamesModes, NullCB);

		static const char * g_pNamesDirs[eDSD_Max] =
		{
			"None",
			"Left",
			"Right",
		};

		bank.AddCombo("Debug Scoring Direction:", (int*)&DEBUG_TARGETSCORING_DIRECTION, eDSD_Max, g_pNamesDirs, NullCB);
	}
}
#endif
