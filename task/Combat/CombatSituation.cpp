// File header
#include "Task/Combat/CombatSituation.h"

// Game headers
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/CombatDirector.h"
#include "Task/Combat/CombatOrders.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CCombatSituation
//////////////////////////////////////////////////////////////////////////

#define LARGEST_COMBAT_SITUATION_CLASS sizeof(CCombatSituation)

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CCombatSituation, CONFIGURED_FROM_FILE, 0.13f, atHashString("CCombatSituation",0x33818544), LARGEST_COMBAT_SITUATION_CLASS);

CompileTimeAssert(sizeof(CCombatSituation)			<= LARGEST_COMBAT_SITUATION_CLASS);
CompileTimeAssert(sizeof(CCombatSituationNormal)	<= LARGEST_COMBAT_SITUATION_CLASS);
CompileTimeAssert(sizeof(CCombatSituationFallBack)	<= LARGEST_COMBAT_SITUATION_CLASS);
CompileTimeAssert(sizeof(CCombatSituationLull)		<= LARGEST_COMBAT_SITUATION_CLASS);
CompileTimeAssert(sizeof(CCombatSituationEscalate)	<= LARGEST_COMBAT_SITUATION_CLASS);

CCombatSituation::CCombatSituation()
: m_fTotalTime(0.0f)
, m_fTimeSinceLastUpdate(0.0f)
, m_fTimeBetweenUpdates(1.0f)
{

}

CCombatSituation::~CCombatSituation()
{

}

void CCombatSituation::Initialize(CCombatDirector& rDirector)
{
	//Initialize the peds.
	int iCount = rDirector.GetNumPeds();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}
		
		//Initialize the ped.
		Initialize(rDirector, *pPed);
	}
}

void CCombatSituation::OnActivate(CCombatDirector& rDirector, CPed& rPed)
{
	//Initialize the ped.
	Initialize(rDirector, rPed);
}

void CCombatSituation::OnAdd(CCombatDirector& UNUSED_PARAM(rDirector), CPed& UNUSED_PARAM(rPed))
{

}

void CCombatSituation::OnDeactivate(CCombatDirector& UNUSED_PARAM(rDirector), CPed& UNUSED_PARAM(rPed))
{

}

void CCombatSituation::OnRemove(CCombatDirector& rDirector, CPed& rPed)
{
	//Uninitialize the ped.
	Uninitialize(rDirector, rPed);
}

void CCombatSituation::Uninitialize(CCombatDirector& rDirector)
{
	//Uninitialize the peds.
	int iCount = rDirector.GetNumPeds();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}

		//Uninitialize the ped.
		Uninitialize(rDirector, *pPed);
	}
}

void CCombatSituation::Update(CCombatDirector& UNUSED_PARAM(rDirector), const float fTimeStep)
{
	m_fTotalTime += fTimeStep;
	m_fTimeSinceLastUpdate += fTimeStep;
}

#if __BANK
const char* CCombatSituation::GetName() const
{
	return "Unknown";
}
#endif

bool CCombatSituation::CheckShouldUpdate()
{
	//Check if the update timer has expired.
	if(m_fTimeSinceLastUpdate < m_fTimeBetweenUpdates)
	{
		return false;
	}
	
	//Clear the update timer.
	m_fTimeSinceLastUpdate = 0.0f;
	
	return true;
}

const CPed* CCombatSituation::FindPlayerTarget(const CCombatDirector& rDirector) const
{
	//Iterate over the peds.
	for(int i = 0; i < rDirector.GetNumPeds(); ++i)
	{
		//Ensure the ped is valid.
		const CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}

		//Ensure the combat task is valid.
		CTaskCombat* pTask = GetCombatTask(*pPed);
		if(!pTask)
		{
			continue;
		}

		//Ensure the target is valid.
		const CPed* pTarget = pTask->GetPrimaryTarget();
		if(!pTarget)
		{
			continue;
		}

		//Ensure the target is a player.
		if(!pTarget->IsPlayer())
		{
			continue;
		}

		return pTarget;
	}

	return NULL;
}

bool CCombatSituation::GetAttackWindowAndDistanceFromTarget(const CPed& rPed, float& fAttackWindowMinOut, float& fAttackWindowMaxOut, float& fDistanceFromTargetOut) const
{
	//Grab the combat task.
	CTaskCombat* pTask = GetCombatTask(rPed);
	if(!pTask)
	{
		return false;
	}
	
	//Get the target.
	const CPed* pTarget = pTask->GetPrimaryTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the position.
	Vec3V vPosition = rPed.GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Calculate the distance.
	fDistanceFromTargetOut = Dist(vPosition, vTargetPosition).Getf();
	
	//Get the attack window min/max.
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(&rPed, fAttackWindowMinOut, fAttackWindowMaxOut, false);
	
	return true;
}

CTaskCombat* CCombatSituation::GetCombatTask(const CPed& rPed) const
{
	return static_cast<CTaskCombat *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
}

void CCombatSituation::Initialize(CCombatDirector& rDirector, CPed& rPed)
{
	//Ensure the combat orders are valid.
	CCombatOrders* pCombatOrders = rPed.GetPedIntelligence()->GetCombatOrders();
	if(!pCombatOrders)
	{
		return;
	}

	//Initialize the combat orders.
	Initialize(rDirector, rPed, *pCombatOrders);
}

void CCombatSituation::Initialize(CCombatDirector& UNUSED_PARAM(rDirector), CPed& UNUSED_PARAM(rPed), CCombatOrders& rCombatOrders)
{
	//Clear the orders.
	rCombatOrders.Clear();
}

void CCombatSituation::Uninitialize(CCombatDirector& rDirector, CPed& rPed)
{
	//Ensure the combat orders are valid.
	CCombatOrders* pCombatOrders = rPed.GetPedIntelligence()->GetCombatOrders();
	if(!pCombatOrders)
	{
		return;
	}

	//Uninitialize the combat orders.
	Uninitialize(rDirector, rPed, *pCombatOrders);
}

void CCombatSituation::Uninitialize(CCombatDirector& UNUSED_PARAM(rDirector), CPed& UNUSED_PARAM(rPed), CCombatOrders& rCombatOrders)
{
	//Clear the orders.
	rCombatOrders.Clear();
}

//////////////////////////////////////////////////////////////////////////
// CCombatSituationNormal
//////////////////////////////////////////////////////////////////////////

namespace
{
	const float MIN_TIME_FOR_LULL		= 10.0f;
	const float MIN_TIME_IN_NORMAL		= 0.0f;
	const float PCT_PEDS_TO_BLOCK_COVER	= 0.0f;
}

CCombatSituationNormal::CCombatSituationNormal()
: CCombatSituation()
{

}

CCombatSituationNormal::~CCombatSituationNormal()
{

}

void CCombatSituationNormal::Update(CCombatDirector& rDirector, const float fTimeStep)
{
	//Call the base class version.
	CCombatSituation::Update(rDirector, fTimeStep);
	
	//Ensure the update should run.
	if(!CheckShouldUpdate())
	{
		return;
	}
	
	//Limit cover.
	LimitCover(rDirector);
	
	//Check if we have not yet exceeded the minimum time.
	if(m_fTotalTime < MIN_TIME_IN_NORMAL)
	{
		//Nothing else to do, stay in the state.
	}
	//Check if the fall back situation is valid.
	else if(CCombatSituationFallBack::IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationFallBack());
	}
	//Check if the lull situation is valid.
	else if((m_fTotalTime >= MIN_TIME_FOR_LULL) && CCombatSituationLull::IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationLull());
	}
	//Check if the escalate situation is valid.
	else if(CCombatSituationEscalate::IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationEscalate());
	}
}

#if __BANK
const char* CCombatSituationNormal::GetName() const
{
	return "Normal";
}
#endif

void CCombatSituationNormal::LimitCover(CCombatDirector& rDirector)
{
	//Count the number of peds.
	int iCount = rDirector.GetNumPeds();

	//Calculate the number of peds to block cover on.
	int iNumPedsBlockCover = (int)(PCT_PEDS_TO_BLOCK_COVER * iCount);

	//Count the number of peds who are currently blocking cover.
	int iNumPedsBlockingCover = 0;
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is valid.
		const CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}

		//Ensure the combat orders are valid.
		CCombatOrders* pCombatOrders = pPed->GetPedIntelligence()->GetCombatOrders();
		if(!pCombatOrders)
		{
			continue;
		}

		//Check if the ped is blocking cover.
		if(pCombatOrders->GetPassiveFlags().IsFlagSet(CCombatOrders::PF_BlockCover))
		{
			++iNumPedsBlockingCover;
		}
	}

	//Calculate the difference.
	int iBlockCoverDiff = iNumPedsBlockCover - iNumPedsBlockingCover;

	//Randomly choose the start index.
	if(iCount > 0)
	{
		SemiShuffledSequence shuffler(iCount);
		for(int i = 0; i < iCount; ++i)
		{
			//Check if we have reached equilibrium.
			if(iBlockCoverDiff == 0)
			{
				break;
			}

			//Generate the index.
			int iIndex = shuffler.GetElement(i);

			//Ensure the ped is valid.
			const CPed* pPed = rDirector.GetPed(iIndex);
			if(!pPed)
			{
				continue;
			}

			//Ensure the combat orders are valid.
			CCombatOrders* pCombatOrders = pPed->GetPedIntelligence()->GetCombatOrders();
			if(!pCombatOrders)
			{
				continue;
			}

			//Clear or set the flag.
			if(iBlockCoverDiff > 0 && !pCombatOrders->GetPassiveFlags().IsFlagSet(CCombatOrders::PF_BlockCover))
			{
				pCombatOrders->GetPassiveFlags().SetFlag(CCombatOrders::PF_BlockCover);
				--iBlockCoverDiff;
			}
			else if(iBlockCoverDiff < 0 && pCombatOrders->GetPassiveFlags().IsFlagSet(CCombatOrders::PF_BlockCover))
			{
				pCombatOrders->GetPassiveFlags().ClearFlag(CCombatOrders::PF_BlockCover);
				++iBlockCoverDiff;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// CCombatSituationFallBack
//////////////////////////////////////////////////////////////////////////

namespace
{
	const float MIN_TIME_IN_FALL_BACK					= 5.0f;
	const float PCT_PEDS_KILLED_IN_WINDOW_FOR_FALL_BACK	= 0.25f;
	const float PED_KILL_TIME_WINDOW_FOR_FALL_BACK		= 30.0f;
}

CCombatSituationFallBack::CCombatSituationFallBack()
: CCombatSituation()
{

}

CCombatSituationFallBack::~CCombatSituationFallBack()
{

}

void CCombatSituationFallBack::Update(CCombatDirector& rDirector, const float fTimeStep)
{
	//Call the base class version.
	CCombatSituation::Update(rDirector, fTimeStep);

	//Ensure the update should run.
	if(!CheckShouldUpdate())
	{
		return;
	}
	
	//Check if we have not yet exceeded the minimum time.
	if(m_fTotalTime < MIN_TIME_IN_FALL_BACK)
	{
		//Nothing else to do, stay in the state.
	}
	//Check if the fallback situation is no longer valid.
	else if(!IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationNormal());
	}
}

bool CCombatSituationFallBack::IsValid(const CCombatDirector& rDirector)
{
	//Check if lots of peds have died recently.
	if(HaveLotsOfPedsDiedRecently(rDirector))
	{
		return true;
	}
	
	//Check if a target has a scary weapon.
	if(DoesTargetHaveScaryWeapon(rDirector))
	{
		return true;
	}
	
	return false;
}

#if __BANK
const char* CCombatSituationFallBack::GetName() const
{
	return "Fall Back";
}
#endif

void CCombatSituationFallBack::Initialize(CCombatDirector& rDirector)
{
	//Call the base class version.
	CCombatSituation::Initialize(rDirector);

	//Ensure the player target is valid.
	const CPed* pPlayerTarget = FindPlayerTarget(rDirector);
	if(!pPlayerTarget)
	{
		return;
	}

	//Ensure the closest ped is valid.
	CPed* pClosestPed = CNearestPedInCombatHelper::Find(*pPlayerTarget,
		CNearestPedInCombatHelper::OF_MustBeOnFoot | CNearestPedInCombatHelper::OF_MustBeOnScreen);
	if(pClosestPed)
	{
		pClosestPed->NewSay("FALL_BACK");
	}
}

void CCombatSituationFallBack::Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders)
{
	// Don't do this situation for mission peds
	if(rPed.PopTypeIsMission())
	{
		return;
	}

	//Call the base class version.
	CCombatSituation::Initialize(rDirector, rPed, rCombatOrders);

	//Use the far attack range.
	rCombatOrders.SetAttackRange(CCombatData::CR_VeryFar);
}

bool CCombatSituationFallBack::DoesTargetHaveScaryWeapon(const CCombatDirector& rDirector)
{
	//In 95% of cases, every ped in the combat director has the same target.
	//We can speed this function up a bit by caching off the last target,
	//and ignoring the result if the next target matches.
	const CEntity* pLastTarget = NULL;
	
	//Check if any of the targets have a scary weapon.
	int iCount = rDirector.GetNumPeds();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}

		//Ensure the targeting is valid.
		CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
		if(!pTargeting)
		{
			continue;
		}

		//Ensure the target is valid.
		const CEntity* pTarget = pTargeting->GetCurrentTarget();
		if(!pTarget)
		{
			continue;
		}
		
		//Ensure the target does not match the last target.
		if(pTarget == pLastTarget)
		{
			continue;
		}
		
		//Update the last target.
		pLastTarget = pTarget;
		
		//Ensure the target is a ped.
		if(!pTarget->GetIsTypePed())
		{
			continue;
		}
		
		//Ensure the weapon manager is valid.
		const CPed* pTargetPed = static_cast<const CPed *>(pTarget);
		const CPedWeaponManager* pWeaponManager = pTargetPed->GetWeaponManager();
		if(!pWeaponManager)
		{
			continue;
		}
		
		//Ensure the equipped weapon info is valid.
		const CWeaponInfo* pInfo = pWeaponManager->GetEquippedWeaponInfo();
		if(!pInfo)
		{
			continue;
		}
		
		//Ensure the equipped weapon is scary.
		if(!pInfo->GetIsScary())
		{
			continue;
		}
		
		//The equipped weapon is scary.
		return true;
	}
	
	return false;
}

bool CCombatSituationFallBack::HaveLotsOfPedsDiedRecently(const CCombatDirector& rDirector)
{
	//Count the peds that are alive.
	int iAlive = rDirector.GetNumPeds();

	//Count the number of peds killed in the window.
	u32 uKilled = rDirector.GetNumPedsKilledWithinTime(PED_KILL_TIME_WINDOW_FOR_FALL_BACK);

	//Total the alive and killed peds.
	int iTotal = iAlive + uKilled;
	if(iTotal == 0)
	{
		return false;
	}

	//Ensure the percentage of peds killed in the window has exceeded the threshold.
	float fPct = (float)uKilled / iTotal;
	if(fPct < PCT_PEDS_KILLED_IN_WINDOW_FOR_FALL_BACK)
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// CCombatSituationLull
//////////////////////////////////////////////////////////////////////////

namespace
{
	const float MAX_TIME_IN_LULL						= 15.0f;
	const float MIN_TIME_IN_LULL						= 0.0f;
	const float SHOOT_RATE_MODIFIER_FOR_LULL			= 0.5f;
	const float TIME_BETWEEN_BURSTS_MODIFIER_FOR_LULL	= 2.0f;
}

CCombatSituationLull::CCombatSituationLull()
: CCombatSituation()
{
}

CCombatSituationLull::~CCombatSituationLull()
{

}

void CCombatSituationLull::Update(CCombatDirector& rDirector, const float fTimeStep)
{
	//Call the base class version.
	CCombatSituation::Update(rDirector, fTimeStep);

	//Ensure the update should run.
	if(!CheckShouldUpdate())
	{
		return;
	}
	
	//Check if we have not yet exceeded the minimum time.
	if(m_fTotalTime < MIN_TIME_IN_LULL)
	{
		//Nothing else to do, stay in the state.
	}
	//Check if the fall back situation is valid.
	else if(CCombatSituationFallBack::IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationFallBack());
	}
	//Check if the lull situation is no longer valid.
	else if(!IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationNormal());
	}
	//After a certain amount of time, attempt to escalate the situation.
	else if(m_fTotalTime >= MAX_TIME_IN_LULL)
	{
		rDirector.ChangeSituation(rage_new CCombatSituationEscalate());
	}
}

bool CCombatSituationLull::IsValid(const CCombatDirector& rDirector)
{
	//In 95% of cases, every ped in the combat director has the same target.
	//We can speed this function up a bit by caching off the last target,
	//and ignoring the result if the next target matches.
	const CEntity* pLastTarget = NULL;
	
	//Check if any of the peds have a line of sight to their target.
	int iCount = rDirector.GetNumPeds();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = rDirector.GetPed(i);
		if(!pPed)
		{
			continue;
		}
		
		//Ensure the targeting is valid.
		CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
		if(!pTargeting)
		{
			continue;
		}
		
		//Ensure the target is valid.
		const CEntity* pTarget = pTargeting->GetCurrentTarget();
		if(!pTarget)
		{
			continue;
		}
		
		//Ensure the target does not match the last target.
		if(pTarget == pLastTarget)
		{
			continue;
		}

		//Update the last target.
		pLastTarget = pTarget;
		
		//Ensure the ped has a line of sight on the target.
		if(pTargeting->GetLosStatus(pTarget, false) != Los_clear)
		{
			continue;
		}
		
		//The ped has a line of sight to the target, a lull is not valid.
		return false;
	}
	
	return true;
}

#if __BANK
const char* CCombatSituationLull::GetName() const
{
	return "Lull";
}
#endif

void CCombatSituationLull::Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders)
{
	//Call the base class version.
	CCombatSituation::Initialize(rDirector, rPed, rCombatOrders);

	//Assign the shoot rate and burst modifiers.
	rCombatOrders.SetShootRateModifier(SHOOT_RATE_MODIFIER_FOR_LULL);
	rCombatOrders.SetTimeBetweenBurstsModifier(TIME_BETWEEN_BURSTS_MODIFIER_FOR_LULL);
}

//////////////////////////////////////////////////////////////////////////
// CCombatSituationEscalate
//////////////////////////////////////////////////////////////////////////

namespace
{
	const float MAX_TIME_IN_ESCALATE	= 30.0f;
	const float	MIN_TIME_IN_ESCALATE	= 0.0f;
}

CCombatSituationEscalate::CCombatSituationEscalate()
: CCombatSituation()
{

}

CCombatSituationEscalate::~CCombatSituationEscalate()
{

}

void CCombatSituationEscalate::Update(CCombatDirector& rDirector, const float fTimeStep)
{
	//Call the base class version.
	CCombatSituation::Update(rDirector, fTimeStep);

	//Ensure the update should run.
	if(!CheckShouldUpdate())
	{
		return;
	}
	
	//Check if we have not yet exceeded the minimum time.
	if(m_fTotalTime < MIN_TIME_IN_ESCALATE)
	{
		//Nothing else to do, stay in the state.
	}
	//Check if the fall back situation is valid.
	else if(CCombatSituationFallBack::IsValid(rDirector))
	{
		rDirector.ChangeSituation(rage_new CCombatSituationFallBack());
	}
	//After a certain amount of time, go back to the normal situation.
	else if(m_fTotalTime >= MAX_TIME_IN_ESCALATE)
	{
		rDirector.ChangeSituation(rage_new CCombatSituationNormal());
	}
}

bool CCombatSituationEscalate::IsValid(const CCombatDirector& UNUSED_PARAM(rDirector))
{
	return false;
}

#if __BANK
const char* CCombatSituationEscalate::GetName() const
{
	return "Escalate";
}
#endif

void CCombatSituationEscalate::Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders)
{
	//Call the base class version.
	CCombatSituation::Initialize(rDirector, rPed, rCombatOrders);
	
	//Randomly assign some peds to provide cover.
	if(fwRandom::GetRandomNumberInRange(1, 4) == 1)
	{
		//Order the ped to provide cover.
		rCombatOrders.GetActiveFlags().SetFlag(CCombatOrders::AF_CoverFire);
	}
	else
	{
		//Use the near attack range.
		rCombatOrders.SetAttackRange(CCombatData::CR_Near);
	}
}
