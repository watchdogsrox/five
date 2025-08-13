#include "ai\debug\types\RagdollDebugInfo.h"
#include "Task/Physics/TaskNMShot.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "task/Combat/TaskDamageDeath.h"

#if AI_DEBUG_OUTPUT_ENABLED

CRagdollDebugInfo::CRagdollDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CRagdollDebugInfo::Print()
{
	if(!ValidateInput())
	{
		return;
	}

	WriteLogEvent("RAGDOLL");
	PushIndent(3);
	PrintRagdollText();
	PrintRagdollFlags();
}

bool CRagdollDebugInfo::ValidateInput()
{
	if (!m_Ped)
	{
		return false;
	}

	return true;
}

void CRagdollDebugInfo::PrintRagdollText()
{
	u8 iAlpha = 255;
	Color32 colour = CRGBA(255,192,128,iAlpha);

	ColorPrintLn(colour,"Ragdoll State(%s)", CPed::GetRagdollStateName(m_Ped->GetRagdollState()));

	ColorPrintLn(colour,"Ragdoll pool: current(%s) desired(%s)", CTaskNMBehaviour::GetRagdollPoolName(m_Ped->GetCurrentRagdollPool()), CTaskNMBehaviour::GetRagdollPoolName(m_Ped->GetDesiredRagdollPool()));

	ColorPrintLn(colour,"Current physics inst: %s", m_Ped->GetCurrentPhysicsInst()==m_Ped->GetAnimatedInst() ? "ANIMATED" : "RAGDOLL");

#if __ASSERT
	ColorPrintLn(colour,"Last ragdoll controlling task: %s", TASKCLASSINFOMGR.GetTaskName(m_Ped->GetLastRagdollControllingTaskType()));
#endif //__ASSERT

	if(m_Ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_SHOT))
	{
		CTaskNMShot* task = (CTaskNMShot*)m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_SHOT);
		if(task)
		{
			ColorPrintLn(colour, "Shot type: %s", task->GetShotTypeName());
		}
	}

	// How did the ped die (Animated? Ragdolled? Was snap to ground used? etc.)
	if (m_Ped->IsDead())
	{
		CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
		if( pTaskDead )
		{
			char debugText[128];
			formatf(debugText, "TaskDyingDead State History:");
			ColorPrintLn(colour, "%s", debugText);
			PushIndent(2);
			CTaskDyingDead::StateData stateElem;
			atQueue<CTaskDyingDead::StateData, STATE_HISTORY_BUFFER_SIZE> tempQueue;
			while (!pTaskDead->GetStateHistory().IsEmpty())
			{
				stateElem = pTaskDead->GetStateHistory().Pop();
				switch(stateElem.m_state)
				{
				case CTaskDyingDead::State_Start:
					formatf(debugText, " State_Start: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_StreamAssets:
					formatf(debugText, " State_StreamAssets: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingAnimated:
					formatf(debugText, " State_DyingAnimated: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingRagdoll:
					formatf(debugText, " State_DyingRagdoll: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_RagdollAborted:
					formatf(debugText, " State_RagdollAborted: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadAnimated:
					formatf(debugText, " State_DeadAnimated: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadRagdoll:
					formatf(debugText, " State_DeadRagdoll: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadRagdollFrame:
					formatf(debugText, " State_DeadRagdollFrame: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_FallOutOfVehicle:
					formatf(debugText, " State_FallOutOfVehicle: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingAnimatedFall:
					formatf(debugText, " State_DyingAnimatedFall: %u", stateElem.m_startTimeInStateMS);
					break;
				default:
					Assertf(0, "Unknown State");
					break;
				}
				ColorPrintLn(colour, "%s", debugText);
				tempQueue.Push(stateElem);
			}
			while (!tempQueue.IsEmpty())
			{
				pTaskDead->GetStateHistory().Push(tempQueue.Pop());
			}

			// Print the snap state
			switch(pTaskDead->GetSnapToGroundStage())
			{
			case CTaskDyingDead::kSnapNotBegun:
				formatf(debugText, " Snap state: kSnapNotBegun");
				break;
			case CTaskDyingDead::kSnapFailed:
				formatf(debugText, " Snap state: kSnapFailed");
				break;
			case CTaskDyingDead::kSnapPoseRequested:
				formatf(debugText, " Snap state: kSnapPoseRequested");
				break;
			case CTaskDyingDead::kSnapPoseReceived:
				formatf(debugText, " Snap state: kSnapPoseReceived");
				break;
			default:
				Assertf(0, "Unknown Snap State");
				break;
			}
			ColorPrintLn(colour, "%s", debugText);
			PopIndent(2);
		}
	}

	// Kinematic physics mode
	ColorPrintLn(colour,"CanUseKinematicPhysics : %s", m_Ped->CanUseKinematicPhysics() ? "yes" : "no");

	ColorPrintLn(colour,"IsUsingKinematicPhysics : %s", m_Ped->IsUsingKinematicPhysics() ? "yes" : "no");

	// Gravity switched off (aka can extract Z)
	ColorPrintLn(colour,"UseExtZVel : %s", m_Ped->GetUseExtractedZ() ? "on" : "off");

	// bUsesCollision/bIsFixed
	ColorPrintLn(colour,"Collision: %s", m_Ped->IsCollisionEnabled() ? "on" : "off");

	Color32 fixedColour = colour;
	char szFixedState[128];
	if ( m_Ped->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed waiting for collision!");
	}
	else if( m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed by network!");
	}
	else if( m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed by code/script!");
	}
	else
	{
		formatf(szFixedState, "Not fixed");
	}
	ColorPrintLn(fixedColour, "Fixed: %s", szFixedState);

	CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
	if (pNMTask)
	{
		ColorPrintLn(colour, "Balance failed: %s", pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE) ? "yes" : "no");
	}

	ColorPrintLn(colour, "Load collision: %s", m_Ped->ShouldLoadCollision() ? "yes" : "no");
	ColorPrintLn(colour, "phInst->IsInLevel() : %s", m_Ped->GetCurrentPhysicsInst()->IsInLevel() ? "true" : "false");
	ColorPrintLn(colour, "phInst->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) : %s", m_Ped->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) ? "true" : "false");

	Mat34V pedMat = m_Ped->GetMatrix();
	ColorPrintLn(colour, "Matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(pedMat.GetCol3()));

	const phInst* pInst = m_Ped->GetInstForKinematicMode();
	if (pInst->HasLastMatrix())
	{
		const Vec3V lastPos = PHLEVEL->GetLastInstanceMatrix(pInst).GetCol3();
		ColorPrintLn(colour, "Last matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(lastPos));
	}

	pInst = m_Ped->GetRagdollInst();
	if (pInst->HasLastMatrix())
	{
		const Vec3V lastPos = PHLEVEL->GetLastInstanceMatrix(pInst).GetCol3();
		ColorPrintLn(colour, "Ragdoll Last matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(lastPos));
	}

	if (m_Ped->GetRagdollInst())
	{
		// Ragdoll LOD
		const char * ragDollLODNames[fragInst::PHYSICS_LOD_MAX] =
		{
			"HIGH",
			"MEDIUM",
			"LOW"
		};
		ColorPrintLn(colour, "Ragdoll LOD : %s", ragDollLODNames[m_Ped->GetRagdollInst()->GetCurrentPhysicsLOD()]);

		// NM Art asset ID
		ColorPrintLn(colour, "NM Art Asset ID : %s (%d)",
			m_Ped->GetRagdollInst()->GetARTAssetID() == -1 ? "non-NM physics rig" :
			m_Ped->GetRagdollInst()->GetARTAssetID() == 0 ? "ragdoll_type_male" :
			m_Ped->GetRagdollInst()->GetARTAssetID() == 1 ? "ragdoll_type_female" :
			m_Ped->GetRagdollInst()->GetARTAssetID() == 2 ? "ragdoll_type_male_large" :
			"unknown art asset", m_Ped->GetRagdollInst()->GetARTAssetID());

		if (m_Ped->GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			// NM agent?
			ColorPrintLn(colour, "NM Agent / Rage Ragdoll : %s", m_Ped->GetRagdollInst()->GetNMAgentID() != -1 ? "NM Agent" : "Rage Ragdoll");

			// Time as a ragdoll
			u32 activationTime = m_Ped->GetRagdollInst()->GetActivationStartTime();
			Assert(activationTime > 0 && (float)(fwTimer::GetTimeInMilliseconds() - activationTime) >= 0);
			ColorPrintLn(colour, "Time Spent As Ragdoll : %.2f", ((float)(fwTimer::GetTimeInMilliseconds() - activationTime)) / 1000.0f);
		}

		ColorPrintLn(colour, "Is Prone : %s", m_Ped->IsProne() ? "TRUE" : "FALSE");

		// Ragdoll collision mask
		fragInstNMGta* pRagdollInst = m_Ped->GetRagdollInst();
		if (pRagdollInst)
		{
			// Ragdoll mask
			const bool bRAGDOLL_BUTTOCKS				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_BUTTOCKS)) ? true : false;
			const bool bRAGDOLL_THIGH_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_THIGH_LEFT)) ? true : false;
			const bool bRAGDOLL_SHIN_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SHIN_LEFT)) ? true : false;
			const bool bRAGDOLL_FOOT_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_FOOT_LEFT)) ? true : false;
			const bool bRAGDOLL_THIGH_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_THIGH_RIGHT)) ? true : false;
			const bool bRAGDOLL_SHIN_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SHIN_RIGHT)) ? true : false;
			const bool bRAGDOLL_FOOT_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_FOOT_RIGHT)) ? true : false;
			const bool bRAGDOLL_SPINE0					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE0)) ? true : false;
			const bool bRAGDOLL_SPINE1					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE1)) ? true : false;
			const bool bRAGDOLL_SPINE2					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE2)) ? true : false;
			const bool bRAGDOLL_SPINE3					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE3)) ? true : false;
			const bool bRAGDOLL_CLAVICLE_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_CLAVICLE_LEFT)) ? true : false;
			const bool bRAGDOLL_UPPER_ARM_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_UPPER_ARM_LEFT)) ? true : false;
			const bool bRAGDOLL_LOWER_ARM_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_LOWER_ARM_LEFT)) ? true : false;
			const bool bRAGDOLL_HAND_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HAND_LEFT)) ? true : false;
			const bool bRAGDOLL_CLAVICLE_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_CLAVICLE_RIGHT)) ? true : false;
			const bool bRAGDOLL_UPPER_ARM_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_UPPER_ARM_RIGHT)) ? true : false;
			const bool bRAGDOLL_LOWER_ARM_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_LOWER_ARM_RIGHT)) ? true : false;
			const bool bRAGDOLL_HAND_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HAND_RIGHT)) ? true : false;
			const bool bRAGDOLL_NECK					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_NECK)) ? true : false;
			const bool bRAGDOLL_HEAD					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HEAD)) ? true : false;

			const bool bAtLeastOneFlagSet				= ( bRAGDOLL_BUTTOCKS || bRAGDOLL_THIGH_LEFT || bRAGDOLL_SHIN_LEFT || bRAGDOLL_FOOT_LEFT || bRAGDOLL_THIGH_RIGHT || bRAGDOLL_SHIN_RIGHT || bRAGDOLL_FOOT_RIGHT ||
				bRAGDOLL_SPINE0 || bRAGDOLL_SPINE1 || bRAGDOLL_SPINE2 || bRAGDOLL_SPINE3 || bRAGDOLL_CLAVICLE_LEFT || bRAGDOLL_UPPER_ARM_LEFT || bRAGDOLL_LOWER_ARM_LEFT ||
				bRAGDOLL_HAND_LEFT || bRAGDOLL_CLAVICLE_RIGHT || bRAGDOLL_UPPER_ARM_RIGHT || bRAGDOLL_LOWER_ARM_RIGHT || bRAGDOLL_HAND_RIGHT || bRAGDOLL_NECK || bRAGDOLL_HEAD) ? true : false;
			ColorPrintLn(colour, "---------- Ragdoll Disabled Collision Mask ----------");
			ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "%s | %s | %s | %s",
				bRAGDOLL_BUTTOCKS			? "RAGDOLL_BUTTOCKS" : "NULL",
				bRAGDOLL_THIGH_LEFT			? "RAGDOLL_THIGH_LEFT" : "NULL",
				bRAGDOLL_SHIN_LEFT			? "RAGDOLL_SHIN_LEFT" : "NULL",
				bRAGDOLL_FOOT_LEFT			? "RAGDOLL_FOOT_LEFT" : "NULL");

			ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "%s | %s | %s | %s",
				bRAGDOLL_THIGH_RIGHT		? "RAGDOLL_THIGH_RIGHT" : "NULL",
				bRAGDOLL_SHIN_RIGHT			? "RAGDOLL_SHIN_RIGHT" : "NULL",
				bRAGDOLL_FOOT_RIGHT			? "RAGDOLL_FOOT_RIGHT" : "NULL",
				bRAGDOLL_SPINE0				? "RAGDOLL_SPINE0" : "NULL");

			ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "%s | %s | %s | %s",
				bRAGDOLL_SPINE1				? "RAGDOLL_SPINE1" : "NULL",
				bRAGDOLL_SPINE2				? "RAGDOLL_SPINE2" : "NULL",
				bRAGDOLL_SPINE3				? "RAGDOLL_SPINE3" : "NULL",
				bRAGDOLL_CLAVICLE_LEFT		? "RAGDOLL_CLAVICLE_LEFT" : "NULL");

			ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "%s | %s | %s | %s",
				bRAGDOLL_UPPER_ARM_LEFT		? "RAGDOLL_UPPER_ARM_LEFT" : "NULL",
				bRAGDOLL_LOWER_ARM_LEFT		? "RAGDOLL_LOWER_ARM_LEFT" : "NULL",
				bRAGDOLL_HAND_LEFT			? "RAGDOLL_HAND_LEFT" : "NULL",
				bRAGDOLL_CLAVICLE_RIGHT		? "RAGDOLL_CLAVICLE_RIGHT" : "NULL");

			ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "%s | %s | %s | %s | %s",
				bRAGDOLL_UPPER_ARM_RIGHT	? "RAGDOLL_UPPER_ARM_RIGHT" : "NULL",
				bRAGDOLL_LOWER_ARM_RIGHT	? "RAGDOLL_LOWER_ARM_RIGHT" : "NULL",
				bRAGDOLL_HAND_RIGHT			? "RAGDOLL_HAND_RIGHT" : "NULL",
				bRAGDOLL_NECK				? "RAGDOLL_NECK" : "NULL",
				bRAGDOLL_HEAD				? "RAGDOLL_HEAD" : "NULL");
			ColorPrintLn(colour, "---------------------------------------------------");
		}
	}

	if (m_Ped->GetActivateRagdollOnCollision())
	{
		ColorPrintLn(colour, "Activate ragdoll on collision enabled - event:%s", m_Ped->GetActivateRagdollOnCollisionEvent()? m_Ped->GetActivateRagdollOnCollisionEvent()->GetName() : "none");
		ColorPrintLn(colour, "Allowed penetration: %.3f, Allowed slope: %.3f, Allowed parts: %d", m_Ped->GetRagdollOnCollisionAllowedPenetration(), m_Ped->GetRagdollOnCollisionAllowedSlope(), m_Ped->GetRagdollOnCollisionAllowedPartsMask());
		ColorPrintLn(colour, "Ignore Physical: (%s) - 0x%p", m_Ped->GetRagdollOnCollisionIgnorePhysical() ? m_Ped->GetRagdollOnCollisionIgnorePhysical()->GetModelName() : "NULL", m_Ped->GetRagdollOnCollisionIgnorePhysical());
	}

	if (CTaskNMBehaviour::m_pTuningSetHistoryPed==m_Ped)
	{
		for (s32 i=CTaskNMBehaviour::m_TuningSetHistory.GetCount()-1; i>-1; i--)
		{
			ColorPrintLn(colour, "%s: %.3f", CTaskNMBehaviour::m_TuningSetHistory[i].id.GetCStr(), ((float)(fwTimer::GetTimeInMilliseconds() - CTaskNMBehaviour::m_TuningSetHistory[i].time))/1000.0f);
		}
	}

	// Does this entity already have a scene update extension?
	const fwSceneUpdateExtension *extension = m_Ped->GetExtension<fwSceneUpdateExtension>();

	if (extension)
	{
		ColorPrintLn(colour,  "Scene update flags: %d", extension->m_sceneUpdateFlags);
	}

	ColorPrintLn(colour,  "Accumulated sideswipe magnitude: %.4f", m_Ped->GetAccumulatedSideSwipeImpulseMag());
	ColorPrintLn(colour,  "Time first under another ragdoll: %d", m_Ped->GetTimeOfFirstBeingUnderAnotherRagdoll());
}

void CRagdollDebugInfo::PrintRagdollFlags()
{
	ColorPrintLn(Color_WhiteSmoke, "RAGDOLL FLAGS:");
	PushIndent(2);

	// ragdoll blocking flags

	const bool bDontActivateRagdollFromAnyPedImpact = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact);
	ColorPrintLn(bDontActivateRagdollFromAnyPedImpact ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromAnyPedImpact: %s", bDontActivateRagdollFromAnyPedImpact ? "Y" : "N");

	const bool bDontActivateRagdollFromAnyPedImpactReset = m_Ped->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset);
	ColorPrintLn(bDontActivateRagdollFromAnyPedImpactReset ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromAnyPedImpactReset: %s", bDontActivateRagdollFromAnyPedImpactReset ? "Y" : "N");

	const bool bDontActivateRagdollFromVehicleImpact = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact);
	ColorPrintLn(bDontActivateRagdollFromVehicleImpact ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromVehicleImpact: %s", bDontActivateRagdollFromVehicleImpact ? "Y" : "N");

	const bool bDontActivateRagdollFromBulletImpact = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact);
	ColorPrintLn(bDontActivateRagdollFromBulletImpact ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromBulletImpact: %s", bDontActivateRagdollFromBulletImpact ? "Y" : "N");

	const bool bDontActivateRagdollFromRubberBullet = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromRubberBullet);
	ColorPrintLn(bDontActivateRagdollFromRubberBullet ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromRubberBullet: %s", bDontActivateRagdollFromRubberBullet ? "Y" : "N");

	const bool bDontActivateRagdollFromFire = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFire);
	ColorPrintLn(bDontActivateRagdollFromFire ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromFire: %s", bDontActivateRagdollFromFire ? "Y" : "N");

	const bool bDontActivateRagdollFromExplosions = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions);
	ColorPrintLn(bDontActivateRagdollFromExplosions ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromExplosions: %s", bDontActivateRagdollFromExplosions ? "Y" : "N");

	const bool bDontActivateRagdollFromElectrocution = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromElectrocution);
	ColorPrintLn(bDontActivateRagdollFromElectrocution ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromElectrocution: %s", bDontActivateRagdollFromElectrocution ? "Y" : "N");

	const bool bBlockWeaponReactionsUnlessDead = m_Ped->GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponReactionsUnlessDead);
	ColorPrintLn(bBlockWeaponReactionsUnlessDead ? Color_LimeGreen : Color_tomato, "BlockWeaponReactionsUnlessDead: %s", bBlockWeaponReactionsUnlessDead ? "Y" : "N");

	const bool bDontActivateRagdollFromImpactObject = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromImpactObject);
	ColorPrintLn(bDontActivateRagdollFromImpactObject ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromImpactObject: %s", bDontActivateRagdollFromImpactObject ? "Y" : "N");

	const bool bDontActivateRagdollFromMelee = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromMelee);
	ColorPrintLn(bDontActivateRagdollFromMelee ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromMelee: %s", bDontActivateRagdollFromMelee ? "Y" : "N");

	const bool bDontActivateRagdollFromWaterJet = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromWaterJet);
	ColorPrintLn(bDontActivateRagdollFromWaterJet ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromWaterJet: %s", bDontActivateRagdollFromWaterJet ? "Y" : "N");

	const bool bDontActivateRagdollFromFalling = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling);
	ColorPrintLn(bDontActivateRagdollFromFalling ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromFalling: %s", bDontActivateRagdollFromFalling ? "Y" : "N");

	const bool bDontActivateRagdollFromDrowning = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromDrowning);
	ColorPrintLn(bDontActivateRagdollFromDrowning ? Color_LimeGreen : Color_tomato, "DontActivateRagdollFromDrowning: %s", bDontActivateRagdollFromDrowning ? "Y" : "N");

	const bool bAllowBlockDeadPedRagdollActivation = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowBlockDeadPedRagdollActivation);
	ColorPrintLn(bAllowBlockDeadPedRagdollActivation ? Color_LimeGreen : Color_tomato, "AllowBlockDeadPedRagdollActivation: %s", bAllowBlockDeadPedRagdollActivation ? "Y" : "N");

	PopIndent(2);
}

#endif // AI_DEBUG_OUTPUT_ENABLED