// File header
#include "Task/Combat/Subtasks/VehicleChaseDirector.h"

// Game headers
#include "Peds/ped.h"
#include "task/combat/Subtasks/TaskVehicleChase.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/task/TaskVehiclePursue.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CVehicleChaseDirector
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CVehicleChaseDirector, CONFIGURED_FROM_FILE, atHashString("CVehicleChaseDirector",0xd2f56334));

CVehicleChaseDirector::CVehicleChaseDirector(const CPhysical& rTarget)
: m_aTasks()
, m_pTarget(&rTarget)
, m_fTimeSinceLastDesiredOffsetsUpdate(0.0f)
{
}

CVehicleChaseDirector::~CVehicleChaseDirector()
{
}

bool CVehicleChaseDirector::Add(CTaskVehicleChase* pTask)
{
	//Ensure the task is valid.
	if(!pTask)
	{
		return false;
	}
	
	//Ensure the tasks are not full.
	if(m_aTasks.IsFull())
	{
		return false;
	}
	
	//Add the task.
	m_aTasks.Append() = pTask;
	
	return true;
}

bool CVehicleChaseDirector::Remove(CTaskVehicleChase* pTask)
{
	//Ensure the task is valid.
	if(!pTask)
	{
		return false;
	}
	
	//Remove the task.
	for(int i = 0; i < m_aTasks.GetCount(); ++i)
	{
		//Ensure the task matches.
		if(m_aTasks[i] != pTask)
		{
			continue;
		}
		
		//Remove the task.
		m_aTasks.Delete(i);
		
		return true;
	}
	
	return false;
}

void CVehicleChaseDirector::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);
	
	//Remove the invalid tasks.
	RemoveInvalidTasks();
	
	//Update the behavior flags.
	UpdateBehaviorFlags();
	
	//Check if we should update the desired offsets.
	if(ShouldUpdateDesiredOffsets())
	{
		//Update the desired offsets.
		UpdateDesiredOffsets();
	}
}

CVehicleChaseDirector* CVehicleChaseDirector::FindVehicleChaseDirector(const CPhysical& rTarget)
{
	//Grab the pool of directors.
	CVehicleChaseDirector::Pool* pPool = CVehicleChaseDirector::GetPool();
	
	//Find the director for the target.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the director.
		CVehicleChaseDirector* pVehicleChaseDirector = pPool->GetSlot(i);
		if(!pVehicleChaseDirector)
		{
			continue;
		}
		
		//Ensure the target matches.
		if(pVehicleChaseDirector->GetTarget() != &rTarget)
		{
			continue;
		}
		
		return pVehicleChaseDirector;
	}
	
	//Ensure a new director can be created.
	if(pPool->GetNoOfFreeSpaces() == 0)
	{
		return NULL;
	}
	
	//Create a new director.
	return rage_new CVehicleChaseDirector(rTarget);
}

void CVehicleChaseDirector::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pool.
	CVehicleChaseDirector::InitPool(MEMBUCKET_GAMEPLAY);
}

void CVehicleChaseDirector::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pool.
	CVehicleChaseDirector::ShutdownPool();
}

void CVehicleChaseDirector::UpdateVehicleChaseDirectors()
{
	//Grab the pool of directors.
	CVehicleChaseDirector::Pool* pPool = CVehicleChaseDirector::GetPool();

	//Grab the time step.
	const float fTimeStep = fwTimer::GetTimeStep();

	//Update the active directors.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the director.
		CVehicleChaseDirector* pVehicleChaseDirector = pPool->GetSlot(i);
		if(!pVehicleChaseDirector)
		{
			continue;
		}
		
		//Check if the target is invalid.
		if(!pVehicleChaseDirector->GetTarget())
		{
			//Free the director.
			delete pVehicleChaseDirector;
		}
		else
		{
			//Update the director.
			pVehicleChaseDirector->Update(fTimeStep);
			
			//Check if the number of tasks is invalid.
			if(pVehicleChaseDirector->GetNumTasks() <= 0)
			{
				//Free the director.
				delete pVehicleChaseDirector;
			}
		}
	}
}

void CVehicleChaseDirector::RemoveInvalidTasks()
{
	//Iterate over the tasks.
	int iCount = m_aTasks.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the task is invalid.
		const CTaskVehicleChase* pTask = static_cast<const CTaskVehicleChase *>(m_aTasks[i].Get());
		if(pTask)
		{
			continue;
		}

		//Remove the task.
		m_aTasks.Delete(i);

		//Update the index.
		--i;

		//Update the count.
		iCount = m_aTasks.GetCount();
	}
}

bool CVehicleChaseDirector::ShouldUpdateDesiredOffsets() const
{
	//Ensure the timer has expired.
	TUNE_GROUP_FLOAT(VEHICLE_CHASE_DIRECTOR, fTimeBetweenDesiredOffsetsUpdates, 0.5f, 0.0f, 10.0f, 0.1f);
	if(m_fTimeSinceLastDesiredOffsetsUpdate < fTimeBetweenDesiredOffsetsUpdates)
	{
		return false;
	}
	
	return true;
}

void CVehicleChaseDirector::UpdateBehaviorFlags()
{
	//Grab the target.
	const CEntity* pTarget = GetTarget();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Count the blocking.
	int iBlocking = 0;
	
	//Count the cruising in front to block.
	int iCruisingInFrontToBlock = 0;
	
	//Count the pursuing.
	int iPursuing = 0;
	
	//Count the making aggressive moves.
	int iMakingAggressiveMoves = 0;

	//Count the pulling alongside.
	int iPullingAlongside = 0;
	
	//Keep track of the closest pursuer.
	struct ClosestPursuer
	{
		ClosestPursuer()
		: m_pTask(NULL)
		, m_scDistSq(V_ZERO)
		{}
		
		CTaskVehicleChase*	m_pTask;
		ScalarV				m_scDistSq;
	};
	ClosestPursuer closestPursuer;
	
	//Iterate over the tasks.
	int iCount = m_aTasks.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the task.
		CTaskVehicleChase* pTask = static_cast<CTaskVehicleChase *>(m_aTasks[i].Get());
		if(!aiVerifyf(pTask, "The task is invalid."))
		{
			continue;
		}
		
		//Check if the task is blocking.
		if(pTask->IsBlocking())
		{
			//Increase the blocking.
			++iBlocking;
		}
		
		//Check if the task is cruising in front to block.
		if(pTask->IsCruisingInFrontToBlock())
		{
			//Increase the cruising in front to block.
			++iCruisingInFrontToBlock;
		}
		
		//Check if the task is pursuing.
		if(pTask->IsPursuing())
		{
			//Increase the pursuing.
			++iPursuing;
			
			//Grab the position.
			Vec3V vPosition = pTask->GetPed()->GetTransform().GetPosition();

			//Calculate the distance.
			ScalarV scDistSq = DistSquared(vPosition, vTargetPosition);
			
			//Check if this is the closest pursuer.
			if(!closestPursuer.m_pTask || IsLessThanAll(scDistSq, closestPursuer.m_scDistSq))
			{
				//Assign the closest pursuer.
				closestPursuer.m_pTask = pTask;
				closestPursuer.m_scDistSq = scDistSq;
			}
		}
		
		//Check if the task is making an aggressive move.
		if(pTask->IsMakingAggressiveMove())
		{
			//Increase the making aggressive moves.
			++iMakingAggressiveMoves;
		}

		//Check if the task is pulling alongside.
		if(pTask->IsPullingAlongside())
		{
			//Increase the pulling alongside.
			++iPullingAlongside;
		}
	}
	
	//Check if the blocking cap has been exceeded.
	bool bBlockCapExceeded = false;
	
	//Check if the block from pursue cap has been exceeded.
	bool bBlockFromPursueCapExceeded = (iCruisingInFrontToBlock != 0);
	
	//Check if the pursue cap has been exceeded.
	bool bPursueCapExceeded = false;
	
	//Check if the aggressive move cap has been exceeded.
	bool bAggressiveMoveCapExceeded = (iMakingAggressiveMoves != 0);

	//Check if the pull alongside cap has been exceeded.
	bool bPullAlongsideCapExceeded = (iPullingAlongside != 0);

	//Check if the target is a vehicle.
	bool bIsTargetAVehicle = pTarget->GetIsTypeVehicle();

	//Check if the target is in a vehicle.
	bool bIsTargetInVehicle = false;
	if(pTarget->GetIsTypePed())
	{
		bIsTargetInVehicle = static_cast<const CPed *>(pTarget)->GetIsInVehicle();
	}
	
	//Iterate over the tasks.
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the task is valid.
		CTaskVehicleChase* pTask = static_cast<CTaskVehicleChase *>(m_aTasks[i].Get());
		if(!aiVerifyf(pTask, "The task is invalid."))
		{
			continue;
		}
		
		//Check if we are the closest pursuer.
		bool bIsClosestPursuer = (pTask == closestPursuer.m_pTask);
		
		//Check if the task is blocking.
		bool bTaskIsBlocking = pTask->IsBlocking();
		
		//Check if the task is pursuing.
		bool bTaskIsPursuing = pTask->IsPursuing();
		
		//Check if the task is making an aggressive move.
		bool bTaskIsMakingAggressiveMove = pTask->IsMakingAggressiveMove();

		//Check if the task is pulling alongside.
		bool bTaskIsPullingAlongside = pTask->IsPullingAlongside();
		
		//Set the can't block flag.
		bool bTaskCantBlock = (!bTaskIsBlocking && bBlockCapExceeded) || (bTaskIsPursuing && bBlockFromPursueCapExceeded);
		pTask->ChangeBehaviorFlag(CTaskVehicleChase::Overrides::Director, CTaskVehicleChase::BF_CantBlock, bTaskCantBlock);
		
		//Set the can't pursue flag.
		bool bTaskCantPursue = !bTaskIsPursuing && bPursueCapExceeded;
		pTask->ChangeBehaviorFlag(CTaskVehicleChase::Overrides::Director, CTaskVehicleChase::BF_CantPursue, bTaskCantPursue);
		
		//Set the can't make aggressive move flag.
		bool bTaskCantMakeAggressiveMove = (!bIsTargetAVehicle || bIsTargetInVehicle) || (!bTaskIsMakingAggressiveMove && (bAggressiveMoveCapExceeded || !bIsClosestPursuer));
		pTask->ChangeBehaviorFlag(CTaskVehicleChase::Overrides::Director, CTaskVehicleChase::BF_CantMakeAggressiveMove, bTaskCantMakeAggressiveMove);

		//Set the 'use continuous ram' flag.
		bool bUseContinuousRam = (!bTaskCantMakeAggressiveMove && (iCruisingInFrontToBlock > 0));
		pTask->ChangeBehaviorFlag(CTaskVehicleChase::Overrides::Director, CTaskVehicleChase::BF_UseContinuousRam, bUseContinuousRam);

		//Set the 'can't pull alongside' flag.
		bool bTaskCantPullAlongside = (!bTaskIsPullingAlongside && bPullAlongsideCapExceeded);
		pTask->ChangeBehaviorFlag(CTaskVehicleChase::Overrides::Director, CTaskVehicleChase::BF_CantPullAlongside, bTaskCantPullAlongside);
	}
}

void CVehicleChaseDirector::UpdateDesiredOffsets()
{
	//Clear the timer.
	m_fTimeSinceLastDesiredOffsetsUpdate = 0.0f;

	//Calculate the target matrix.
	Mat34V mTarget = CTaskVehiclePursue::CalculateTargetMatrix(*m_pTarget);

	//Grab the target position.
	Vec3V vTargetPosition = mTarget.GetCol3();

	//Grab the target direction.
	Vec3V vTargetDirection = mTarget.GetCol1();

	//Keep track of the task data.
	struct TaskData
	{
		TaskData()
		{}

		CTaskVehicleChase*	m_pTask;
		const CVehicle*		m_pVehicle;
		Vec3V				m_vOffset;
		float				m_fVehicleWidth;
		float				m_fVehicleLength;
		bool				m_bAssigned;
	};
	TaskData aTaskData[sMaxTasks];
	int iTaskDataCount = 0;

	//Keep track of the max vehicle width/length.
	float fMaxVehicleWidth	= 0.0f;
	float fMaxVehicleLength	= 0.0f;

	//Iterate over the tasks.
	int iCount = m_aTasks.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the task is valid.
		CTaskVehicleChase* pTask = static_cast<CTaskVehicleChase *>(m_aTasks[i].Get());
		if(!aiVerifyf(pTask, "The task is invalid."))
		{
			continue;
		}
		
		//Ensure the vehicle is valid.
		const CVehicle* pVehicle = pTask->GetPed()->GetVehiclePedInside();
		if(!pVehicle)
		{
			continue;
		}
		
		//Ensure the model info is valid.
		CBaseModelInfo* pModelInfo = pVehicle->GetBaseModelInfo();
		if(!pModelInfo)
		{
			continue;
		}

		//Grab the vehicle position.
		Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

		//Calculate a vector from the target to the vehicle.
		Vec3V vTargetToVehicle = Subtract(vVehiclePosition, vTargetPosition);

		//Ensure the vehicle is behind the target.
		ScalarV scDot = Dot(vTargetToVehicle, vTargetDirection);
		if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
		{
			continue;
		}

		//Calculate the offset.
		Vec3V vOffset = UnTransformOrtho(mTarget, vVehiclePosition);

		//Grab the bounding boxes.
		const Vector3& vBoundingBoxMin = pModelInfo->GetBoundingBoxMin();
		const Vector3& vBoundingBoxMax = pModelInfo->GetBoundingBoxMax();

		//Calculate the vehicle dimensions.
		float fVehicleWidth		= (vBoundingBoxMax.x - vBoundingBoxMin.x);
		float fVehicleLength	= (vBoundingBoxMax.y - vBoundingBoxMin.y);

		//Add the task data.
		TaskData& rData	= aTaskData[iTaskDataCount++];

		//Set the task data.
		rData.m_pTask			= pTask;
		rData.m_pVehicle		= pVehicle;
		rData.m_vOffset			= vOffset;
		rData.m_fVehicleWidth	= fVehicleWidth;
		rData.m_fVehicleLength	= fVehicleLength;
		rData.m_bAssigned		= false;

		//Update the max vehicle width and length.
		fMaxVehicleWidth	= Max(fMaxVehicleWidth,		fVehicleWidth);
		fMaxVehicleLength	= Max(fMaxVehicleLength,	fVehicleLength);
	}

	//Keep track of the last desired offset.
	float fLastDesiredOffset = 0.0f;

	//Keep track of some flags.
	bool bIsFirst = true;

	//Iterate over the task data.
	while(true)
	{
		//Keep track of the best task.
		struct BestTask
		{
			BestTask()
			: m_iIndex(-1)
			, m_scScore(FLT_MAX)
			{}

			int		m_iIndex;
			ScalarV	m_scScore;
		};
		BestTask bestTask;

		//Calculate the desired offset.
		float fDesiredOffset = fLastDesiredOffset;

		//Check if this is the first.
		if(bIsFirst)
		{
			//Clear the flag.
			bIsFirst = false;
		}
		else
		{
			//Calculate the offset.
			static dev_float s_fIncrease = 5.0f;
			float fOffset = (fMaxVehicleLength + s_fIncrease);

			//Update the desired offset.
			fDesiredOffset += fOffset;
		}

		//Iterate over the task data.
		for(int i = 0; i < iTaskDataCount; ++i)
		{
			//Ensure the data has not been assigned.
			const TaskData& rData = aTaskData[i];
			if(rData.m_bAssigned)
			{
				continue;
			}

			//Calculate the score.
			ScalarV scScoreX	= Scale(Abs(Subtract(ScalarV(V_ZERO),					rData.m_vOffset.GetX())), ScalarVFromF32(1.2f));
			ScalarV scScoreY	= Scale(Abs(Subtract(ScalarVFromF32(fDesiredOffset),	rData.m_vOffset.GetY())), ScalarV(V_ONE));
			ScalarV scScore		= rage::Add(scScoreX, scScoreY);

			//Ensure the score is better.
			if((bestTask.m_iIndex >= 0) && IsGreaterThanOrEqualAll(scScore, bestTask.m_scScore))
			{
				continue;
			}

			//Set the best task.
			bestTask.m_iIndex = i;
			bestTask.m_scScore = scScore;
		}

		//Ensure the best task is valid.
		if(bestTask.m_iIndex < 0)
		{
			break;
		}

		//Grab the data.
		TaskData& rData = aTaskData[bestTask.m_iIndex];

		//Note that the data has been assigned.
		rData.m_bAssigned = true;

		//Assert that the task is valid.
		CTaskVehicleChase* pTask = rData.m_pTask;
		aiAssert(pTask);

		//Set the desired offset.
		pTask->SetDesiredOffsetForPursue(CTaskVehicleChase::Overrides::Director, fDesiredOffset);
		
		//Set the last desired offset.
		fLastDesiredOffset = fDesiredOffset;
	}
}

void CVehicleChaseDirector::UpdateTimers(float fTimeStep)
{
	//Update the timers.
	m_fTimeSinceLastDesiredOffsetsUpdate += fTimeStep;
}
