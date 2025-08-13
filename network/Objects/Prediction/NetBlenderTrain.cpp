//
// name:		NetBlenderTrain.cpp
// written by:	Daniel Yelland
// description:	Network blender used by trains
#include "network/objects/prediction/NetBlenderTrain.h"

#include "peds/ped.h"
#include "network/network.h"
#include "network/objects/entities/NetObjTrain.h"
#include "vehicles/Train.h"

CompileTimeAssert(sizeof(CNetBlenderTrain) <= LARGEST_NET_PHYSICAL_BLENDER_CLASS);

#define DEBUG_NETBLENDER_TRAIN __DEV && 0

NETWORK_OPTIMISATIONS()

// ===========================================================================================================================
// CNetBlenderTrain
// ===========================================================================================================================
CNetBlenderTrain::CNetBlenderTrain(CTrain *train, CVehicleBlenderData *blenderData) :
CNetBlenderVehicle(train, blenderData)
{
}

CNetBlenderTrain::~CNetBlenderTrain()
{
}

void CNetBlenderTrain::StoreLocalPedOffset(CTrain* ptrain)
{
	if (ptrain)
	{
		int tc = 0;
		bool bEngine = ptrain->m_nTrainFlags.bEngine;
		CTrain* ptraincar = ptrain;
		while(ptraincar)
		{
			int i = CPed::GetPool() ? CPed::GetPool()->GetSize() : 0;
			while(i--)
			{
				CPed* pPed = CPed::GetPool() ? CPed::GetPool()->GetSlot(i) : NULL;

				if(pPed && pPed->GetNetworkObject() && !pPed->IsNetworkClone() && pPed->GetGroundPhysical() && (pPed->GetGroundPhysical() == ptraincar))
				{
					CNetObjPed* pNetObjPed = (CNetObjPed*) pPed->GetNetworkObject();
					if (pNetObjPed)
					{					
						Vec3V diff = pPed->GetTransform().GetPosition() - ptraincar->GetTransform().GetPosition();
						Vector3 localOffset = VEC3V_TO_VECTOR3(ptraincar->GetTransform().UnTransform3x3(diff));
						Vector3 localVelocityDelta = pPed->GetVelocity() - ptraincar->GetVelocity();
						pNetObjPed->StoreLocalOffset(localOffset, localVelocityDelta);
					}
				}
			}

			ptraincar = bEngine ? ptraincar->GetLinkedToBackward() : NULL;
			tc++;
		}
	}
}

void CNetBlenderTrain::UpdateLocalPedOffset(CTrain* ptrain)
{
	if (ptrain)
	{
		int tc = 0;
		bool bEngine = ptrain->m_nTrainFlags.bEngine;
		CTrain* ptraincar = ptrain;
		while(ptraincar)
		{
			int i = CPed::GetPool() ? CPed::GetPool()->GetSize() : 0;
			while(i--)
			{
				CPed* pPed = CPed::GetPool() ? CPed::GetPool()->GetSlot(i) : NULL;

				if(pPed && pPed->GetNetworkObject() && !pPed->IsNetworkClone())
				{
					CPhysical* pground = pPed->GetGroundPhysical();
					if (pground && (pground == ptraincar))
					{
						CNetObjPed* pNetObjPed = (CNetObjPed*) pPed->GetNetworkObject();
						if (pNetObjPed)
						{
							//Store the current ground information
							CPhysical* pGroundPhysical = NULL;
							Vec3V vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal;
							int iGroundPhysicalComponent;
							pPed->GetGroundData(pGroundPhysical, vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal, iGroundPhysicalComponent);

							bool bWasAttached = pPed->GetAttachParent() != NULL ? true : false;

#if DEBUG_NETBLENDER_TRAIN
							Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							Vec3V spherepos = VECTOR3_TO_VEC3V(pos);
							CPhysics::ms_debugDrawStore.AddSphere(spherepos,0.05f,Color_white,0);	
#endif

							const Vector3& storedOffset = pNetObjPed->GetLocalOffset();
							const Vector3& stoodOnObjectPos = VEC3V_TO_VECTOR3(ptraincar->GetTransform().GetPosition());

							Vector3 localOffset = VEC3V_TO_VECTOR3(ptraincar->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(storedOffset)));

							Vector3 targetPosition = stoodOnObjectPos + localOffset;

							pPed->SetPosition(targetPosition);

							pPed->SetGroundPhysical(ptraincar);
							pPed->SetGroundPhysicalComponent(0); // Reset the ground physical component

							pPed->SetVelocity(ptraincar->GetVelocity() + pNetObjPed->GetLocalVelocityDelta());

							pPed->ProcessPhysics(fwTimer::GetTimeStep(),false,0);

							//Restore the cached ground information from above
							pPed->SetGroundData(pGroundPhysical, vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal, iGroundPhysicalComponent);

							if (bWasAttached && pPed->CanAttachToGround())
								pPed->AttachToGround();

#if DEBUG_NETBLENDER_TRAIN
							pos = MAT34V_TO_MATRIX34(pPed->GetMatrix()).d;
							spherepos = VECTOR3_TO_VEC3V(pos);
							CPhysics::ms_debugDrawStore.AddSphere(spherepos,0.05f,Color_blue,0);
#endif
						}
					}
				}
			}

			ptraincar = bEngine ? ptraincar->GetLinkedToBackward() : NULL;
			tc++;
		}
	}
}

#if DEBUG_NETBLENDER_TRAIN
void CNetBlenderTrain::DebugUpdateTrainPositionA(const Vector3& vposition, const Vector3& vpredictedposition)
{
	trainDebugf2("CNetBlenderTrain::Update m_PredictionData.GetSnapshotPresent().m_Velocity[%f %f %f][%f]",m_PredictionData.GetSnapshotPresent().m_Velocity.x,m_PredictionData.GetSnapshotPresent().m_Velocity.y,m_PredictionData.GetSnapshotPresent().m_Velocity.z,m_PredictionData.GetSnapshotPresent().m_Velocity.Mag());

	Vector3 uppos = vposition + Vector3(0.f,0.f,5.f);
	Vec3V spherepos = VECTOR3_TO_VEC3V(uppos);
	CPhysics::ms_debugDrawStore.AddSphere(spherepos,0.1f,Color_white,0);

	uppos = GetLastPositionReceived() + Vector3(0.f,0.f,5.f);
	spherepos = VECTOR3_TO_VEC3V(uppos);
	CPhysics::ms_debugDrawStore.AddSphere(spherepos,0.1f,Color_orange,0);

	uppos = vpredictedposition + Vector3(0.f,0.f,5.f);
	spherepos = VECTOR3_TO_VEC3V(uppos);
	CPhysics::ms_debugDrawStore.AddSphere(spherepos,0.1f,Color_blue,0);
}

void CNetBlenderTrain::DebugUpdateTrainPositionB(CTrain* train)
{
	Vector3 pos = VEC3V_TO_VECTOR3(train->GetTransform().GetPosition());
	Vector3 posmatrixd = MAT34V_TO_MATRIX34(train->GetMatrix()).d;
	trainDebugf2("CNetBlenderTrain::Update POP updated pos[%f %f %f] matrix.d[%f %f %f]",pos.x,pos.y,pos.z,posmatrixd.x,posmatrixd.y,posmatrixd.z);	
}
#else
void CNetBlenderTrain::DebugUpdateTrainPositionA(const Vector3&, const Vector3&) { }
void CNetBlenderTrain::DebugUpdateTrainPositionB(CTrain*) { }
#endif


bool CNetBlenderTrain::UpdateTrainPosition(CTrain* train, const Vector3& vposition, const Vector3& vpredictedposition, float& fdist)
{
	if (!train)
		return false;

	bool bChanged = false;
	bool bEngine = train->m_nTrainFlags.bEngine;

	if(fdist > GetPositionMaxForUpdateLevel())
	{
		StoreLocalPedOffset(train);

		Vector3 oldPosition = vposition;
		Vector3 newPosition = vpredictedposition;

#if DEBUG_NETBLENDER_TRAIN
		DebugUpdateTrainPositionA(vposition,vpredictedposition);
#endif

		float fCruiseSpeed  = CTrain::GetCarriageCruiseSpeed(train);
		OUTPUT_ONLY(Vector3 pos = VEC3V_TO_VECTOR3(train->GetTransform().GetPosition());)
		Vector3 posmatrixd = MAT34V_TO_MATRIX34(train->GetMatrix()).d;

#if !__NO_OUTPUT
		if ((Channel_train.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_train.TtyLevel >= DIAG_SEVERITY_DEBUG2))
		{
			unsigned time = netInterface::GetTimestampForStartOfFrame();
			if(time < m_PredictionData.GetSnapshotPresent().m_PositionTimestamp)
			{
				time = m_PredictionData.GetSnapshotPresent().m_PositionTimestamp;
			}

			float timeElapsed = (time - m_PredictionData.GetSnapshotPresent().m_PositionTimestamp) / 1000.0f;
			float maxPredictTime = GetBlenderData()->m_MaxPredictTime;

			trainDebugf2("CNetBlenderTrain::Update POP train[%p] model[%s] logname[%s] isengine[%d] isonscreen[%d] ispendingownerchange[%d] hasstoppedpredictingposition[%d] timeElapsed[%f] maxPredictTime[%f] status[%d] m_updateLODState[%d] fdist[%f] GetPositionMaxForUpdateLevel[%f] pos[%f %f %f] matrix.d[%f %f %f] requestedpos[%f %f %f]",
				train,
				train->GetModelName(),
				train->GetNetworkObject() ? train->GetNetworkObject()->GetLogName() : "NOPE",
				train->IsEngine(),
				train->GetIsOnScreen(),
				train->GetNetworkObject() ? train->GetNetworkObject()->IsPendingOwnerChange() : false,
				HasStoppedPredictingPosition(),
				timeElapsed,
				maxPredictTime,
				train->GetStatus(),
				train->m_updateLODState,
				fdist,
				GetPositionMaxForUpdateLevel(),
				pos.x,pos.y,pos.z,
				posmatrixd.x,posmatrixd.y,posmatrixd.z,
				newPosition.x,newPosition.y,newPosition.z);
		}
#endif

		CTrain::SetNewTrainPosition(train, newPosition);
		CTrain::SetCarriageCruiseSpeed(train, fCruiseSpeed);

		m_LastPopTime = fwTimer::GetTimeInMilliseconds();

		//Ensure this terminates
		u32 numCarriages = train->GetNumCarriages();
		u32 iterations = 0;

		CTrain* traincar = train;
		while(traincar && iterations < numCarriages)
		{
			traincar->ProcessPhysics(fwTimer::GetTimeStep(),false,0);

            CNetBlenderTrain* pNetBlenderTrain = traincar->GetNetworkObject() ? (CNetBlenderTrain*) traincar->GetNetworkObject()->GetNetBlender() : 0;
			if (pNetBlenderTrain)
				pNetBlenderTrain->SetLastPopTime(m_LastPopTime);

			traincar = bEngine ? traincar->GetLinkedToBackward() : (CTrain*) NULL; //if engine - cycle through all cars - otherwise just do this car and stop

			iterations++;
		}

		bChanged = true;

#if DEBUG_NETBLENDER_TRAIN
		DebugUpdateTrainPositionB(train);
#endif

		//-------------------------------------------------------------------------------------
		//TODO: If any ped is standing on the train and not attached they will need to be adjusted for the adjustment in train position otherwise they will move around / fall-off. lavalley.
		UpdateLocalPedOffset(train);
		//-------------------------------------------------------------------------------------				

		fdist = 0.f; //distance should be ZERO as the train has just popped to the adjusted position
	}

	return bChanged;
}

void CNetBlenderTrain::UpdateTrainVelocity(CTrain* train, const Vector3& vpositiondelta, float& /*fdist*/)
{
	if (!train)
		return;

	if (train->m_nTrainFlags.bEngine)
	{
		//-------------------------------------------------------------------
		//Important: adjust the remote velocity of the engine to compensate for position difference and keep the remote 
		//engine aligned.  Without this the remote engine will pop frequently causing lots of issues for riding players.  lavalley.
		Vector3 vSetVelocity = GetLastVelocityReceived();

		if (vSetVelocity.Mag2() > 0.04f)
		{
			vSetVelocity += vpositiondelta;
		}

		const float kfVelocityMax = 100.0f;
		if (vSetVelocity.Mag() > kfVelocityMax)
		{
			vSetVelocity.Normalize();
			vSetVelocity *= kfVelocityMax;
		}

		train->SetVelocity(vSetVelocity);
		CTrain::SetCarriageSpeed(train, vSetVelocity.Mag());

		//Ensure this terminates
		u32 numCarriages = train->GetNumCarriages();
		u32 iterations = 0;

		CTrain* traincar = train;
		while(traincar && iterations < numCarriages)
		{
			traincar->SetVelocity(vSetVelocity);
			CTrain::SetCarriageSpeed(traincar, vSetVelocity.Mag());
			traincar = traincar->GetLinkedToBackward();

			iterations++;
		}
		//-------------------------------------------------------------------
	}
	else
	{
		CTrain* engine = train->FindEngine(train);
		if (engine)
		{
			train->SetVelocity(engine->GetVelocity());
			CTrain::SetCarriageSpeed(train, engine->GetVelocity().Mag());
		}
	}
}

void CNetBlenderTrain::OnPredictionPop() const
{
	trainDebugf2("CNetBlenderTrain::OnPredictionPop");
}

void CNetBlenderTrain::Update()
{
    // train specific blending, position of the engine is controlled by the blender
    // and the other carriages are positioned accordingly. Orientation and angular velocity
    // are blended for all carriages. As the train speed is synced and trains have limited interactions
    // with other objects in the world trains are let to run independently on all machines and
    // the network blender only repositions the train if it gets too far out of sync
    CTrain *train = SafeCast(CTrain, m_pPhysical);
	if(train)
	{
		Vector3 vposition    = GetPositionFromObject();
		Vector3 vpredictedposition = GetCurrentPredictedPosition();
		Vector3 vpositiondelta     = vpredictedposition - vposition;
		float fdist				   = vpositiondelta.Mag();

#if __BANK
		m_LastPredictedPos = GetPredictedNextFramePosition(vpredictedposition, m_PredictionData.GetSnapshotPresent().m_Velocity);
#endif

		UpdateTrainPosition(train,vposition,vpredictedposition,fdist);

		UpdateTrainVelocity(train,vpositiondelta,fdist);

		UpdateAngularVelocity();
	}
}


void CNetBlenderTrain::OnOwnerChange(blenderResetFlags resetFlag)
{
	CTrain *train = SafeCast(CTrain, GetPhysical());
	
	//Ensure the LastReceivedVelocity is set to zero the operating speed is zero here - in that way the last received velocity will not cause the train to move incorrectly.
	//But, if the train is actively moving we will not reset the last received velocity and the train will be able to use it correctly and not cause weird slowdowns.
	
	if (train)
	{
		float fOperatingSpeed = abs(train->m_trackForwardSpeed);
		if (fOperatingSpeed > 0.1f)
		{
			resetFlag = RESET_POSITION; //Only allow position reset on owner change - don't alter velocities as this will zero out remote velocity incorrectly. Leave velocity as is on owner change.
			trainDebugf3("CNetBlenderTrain::OnOwnerChange %s set resetFlag(2) = RESET_POSITION", train->GetNetworkObject()->GetLogName());
		}
		else
		{
			resetFlag = RESET_POSITION_AND_VELOCITY; //ensure the LastReceivedVelocity is also reset to zero.
			trainDebugf3("CNetBlenderTrain::OnOwnerChange %s set resetFlag(0) = RESET_POSITION_AND_VELOCITY", train->GetNetworkObject()->GetLogName());
		}
	}

	CNetBlenderVehicle::OnOwnerChange(resetFlag);
}

//Override this for trains because we don't want trains to invoke CNetBlenderPhysical::ProcessPostPhysics which internally invokes UpdateOrientationBlending and causes the train to glitch remotely.
void CNetBlenderTrain::ProcessPostPhysics()
{
	netBlenderLinInterp::ProcessPostPhysics();

	m_IsPedStandingOnObject = false;
}


