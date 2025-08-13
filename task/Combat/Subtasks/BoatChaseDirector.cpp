// File header
#include "Task/Combat/Subtasks/BoatChaseDirector.h"

// Game headers
#include "Peds/ped.h"
#include "task/combat/Subtasks/TaskBoatChase.h"
#include "vehicleAi/task/TaskVehiclePursue.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CBoatChaseDirector
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CBoatChaseDirector, CONFIGURED_FROM_FILE, atHashString("CBoatChaseDirector",0xa53ca289));

CBoatChaseDirector::CBoatChaseDirector(const CPhysical& rTarget)
: m_aTasks()
, m_pTarget(&rTarget)
, m_fTimeSinceLastDesiredOffsetsUpdate(0.0f)
{
}

CBoatChaseDirector::~CBoatChaseDirector()
{
}

bool CBoatChaseDirector::Add(CTaskBoatChase* pTask)
{
	aiAssert(pTask);

	//Ensure the tasks are not full.
	if(m_aTasks.IsFull())
	{
		return false;
	}
	
	//Add the task.
	m_aTasks.Append() = pTask;
	
	return true;
}

bool CBoatChaseDirector::Remove(CTaskBoatChase* pTask)
{
	aiAssert(pTask);

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

void CBoatChaseDirector::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);
	
	//Remove the invalid tasks.
	RemoveInvalidTasks();
	
	//Check if we should update the desired offsets.
	if(ShouldUpdateDesiredOffsets())
	{
		//Update the desired offsets.
		UpdateDesiredOffsets();
	}
}

CBoatChaseDirector* CBoatChaseDirector::Find(const CPhysical& rTarget)
{
	//Grab the pool of directors.
	CBoatChaseDirector::Pool* pPool = CBoatChaseDirector::GetPool();
	
	//Find the director for the target.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the director.
		CBoatChaseDirector* pDirector = pPool->GetSlot(i);
		if(!pDirector)
		{
			continue;
		}
		
		//Ensure the target matches.
		if(pDirector->GetTarget() != &rTarget)
		{
			continue;
		}
		
		return pDirector;
	}
	
	//Ensure a new director can be created.
	if(pPool->GetNoOfFreeSpaces() == 0)
	{
		return NULL;
	}
	
	//Create a new director.
	return rage_new CBoatChaseDirector(rTarget);
}

void CBoatChaseDirector::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pool.
	CBoatChaseDirector::InitPool(MEMBUCKET_GAMEPLAY);
}

void CBoatChaseDirector::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pool.
	CBoatChaseDirector::ShutdownPool();
}

void CBoatChaseDirector::UpdateAll()
{
	//Grab the pool of directors.
	CBoatChaseDirector::Pool* pPool = CBoatChaseDirector::GetPool();

	//Grab the time step.
	const float fTimeStep = fwTimer::GetTimeStep();

	//Update the active directors.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the director.
		CBoatChaseDirector* pDirector = pPool->GetSlot(i);
		if(!pDirector)
		{
			continue;
		}
		
		//Check if the target is invalid.
		if(!pDirector->GetTarget())
		{
			//Free the director.
			delete pDirector;
		}
		else
		{
			//Update the director.
			pDirector->Update(fTimeStep);
			
			//Check if the number of tasks is invalid.
			if(pDirector->GetNumTasks() <= 0)
			{
				//Free the director.
				delete pDirector;
			}
		}
	}
}

void CBoatChaseDirector::RemoveInvalidTasks()
{
	//Iterate over the tasks.
	int iCount = m_aTasks.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the task is invalid.
		const CTask* pTask = m_aTasks[i].Get();
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

bool CBoatChaseDirector::ShouldUpdateDesiredOffsets() const
{
	//Ensure the timer has expired.
	TUNE_GROUP_FLOAT(BOAT_CHASE_DIRECTOR, fTimeBetweenDesiredOffsetsUpdates, 0.5f, 0.0f, 10.0f, 0.1f);
	if(m_fTimeSinceLastDesiredOffsetsUpdate < fTimeBetweenDesiredOffsetsUpdates)
	{
		return false;
	}
	
	return true;
}

void CBoatChaseDirector::UpdateDesiredOffsets()
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

		CTaskBoatChase*	m_pTask;
		const CVehicle*	m_pVehicle;
		Vec3V			m_vOffset;
		float			m_fVehicleWidth;
		float			m_fVehicleLength;
		bool			m_bAssigned;
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
		CTaskBoatChase* pTask = static_cast<CTaskBoatChase *>(m_aTasks[i].Get());
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
	bool bIsFirst		= true;
	bool bPlaceOnLeft	= true;

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
			float fOffset = (fMaxVehicleLength + 3.0f);

			//Toggle the placement.
			bPlaceOnLeft = !bPlaceOnLeft;

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
		CTaskBoatChase* pTask = rData.m_pTask;
		aiAssert(pTask);

		//Set the desired offset.
		pTask->SetDesiredOffsetForPursue(fDesiredOffset);
		
		//Set the last desired offset.
		fLastDesiredOffset = fDesiredOffset;
	}
}

void CBoatChaseDirector::UpdateTimers(float fTimeStep)
{
	//Update the timers.
	m_fTimeSinceLastDesiredOffsetsUpdate += fTimeStep;
}
