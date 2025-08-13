//
// name:        NetObjTrain.cpp
// description: Derived from netObject, this class handles all train-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjTrain.h"

// game headers
#include "peds/ped.h"
#include "network/network.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkUtil.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/prediction/NetBlenderTrain.h"
#include "vehicles/Train.h"

NETWORK_OPTIMISATIONS()

CTrainSyncTree *CNetObjTrain::ms_TrainSyncTree;

struct CObjectBlenderDataTrain : public CVehicleBlenderData
{
	CObjectBlenderDataTrain()
	{
		m_ApplyVelocity = false;
	}

	const char *GetName() const { return "TRAIN"; }
};

struct CObjectHighPrecisionBlenderDataTrain : public CObjectBlenderDataTrain
{
	CObjectHighPrecisionBlenderDataTrain()
	{
		m_MaxOnScreenPositionDeltaAfterBlend  = 0.1f;  // The maximum position the object is allowed to be from its target once blending has stopped
		m_MaxOffScreenPositionDeltaAfterBlend = 0.1f;  // The maximum position the object is allowed to be from its target once blending has stopped

		m_LowSpeedMode.m_SmallVelocitySquared  = 0.01f;  // velocity change for which we don't try to correct
		m_NormalMode.m_SmallVelocitySquared    = 0.01f;  // velocity change for which we don't try to correct
		m_HighSpeedMode.m_SmallVelocitySquared = 0.01f;  // velocity change for which we don't try to correct

		m_LowSpeedMode.m_PositionDeltaMaxLow     = 0.1f;
		m_LowSpeedMode.m_PositionDeltaMaxMedium  = 0.1f;
		m_LowSpeedMode.m_PositionDeltaMaxHigh    = 0.1f;
		m_NormalMode.m_PositionDeltaMaxLow       = 0.1f;
		m_NormalMode.m_PositionDeltaMaxMedium    = 0.1f;
		m_NormalMode.m_PositionDeltaMaxHigh      = 0.1f;
		m_HighSpeedMode.m_PositionDeltaMaxLow    = 0.1f;
		m_HighSpeedMode.m_PositionDeltaMaxMedium = 0.1f;
		m_HighSpeedMode.m_PositionDeltaMaxHigh   = 0.1f;
	}

	const char *GetName() const { return "HIGH PRECISION TRAIN"; }
};

CObjectHighPrecisionBlenderDataTrain s_trainHighPrecisionBlenderData;
CVehicleBlenderData* CNetObjTrain::ms_trainHighPrecisionBlenderData = &s_trainHighPrecisionBlenderData;


CObjectBlenderDataTrain s_trainBlenderData;
CVehicleBlenderData* CNetObjTrain::ms_trainBlenderData = &s_trainBlenderData;


CNetObjTrain::CTrainScopeData CNetObjTrain::ms_trainScopeData;

CNetObjTrain::CNetObjTrain(CTrain						*train,
                           const NetworkObjectType		type,
                           const ObjectId				objectID,
                           const PhysicalPlayerIndex    playerIndex,
                           const NetObjFlags			localFlags,
                           const NetObjFlags			globalFlags) :
CNetObjVehicle(train, type, objectID, playerIndex, localFlags, globalFlags)
, m_PendingEngineID(NETWORK_INVALID_OBJECT_ID)
, m_PendingLinkedToBackwardID(NETWORK_INVALID_OBJECT_ID)
, m_PendingLinkedToForwardID(NETWORK_INVALID_OBJECT_ID)
, m_nextReportingTime(0)
, m_UseHighPrecisionBlending(false)
{
	trainDebugf2("CNetObjTrain::CNetObjTrain %s train[%p][%s]",GetLogName(),GetTrain(), GetTrain() ? GetTrain()->GetModelName() : "");
}

CNetObjTrain::~CNetObjTrain()
{
	trainDebugf2("CNetObjTrain::~CNetObjTrain train[%s] owner[%s]",GetLogName(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");

	if (!IsClone())
	{
		CTrain* train = GetTrain();
		if (train && train->IsEngine())
		{
			int trackIndex = train->GetTrackIndex();
			if (CTrain::GetTrackActive(trackIndex))
			{
				trainDebugf2("CNetObjTrain::~CNetObjTrain -- local train active - ensure train is cleaned up properly trackIndex[%d]",trackIndex);
				CTrain::SetTrackActive(trackIndex,false);
				CNetworkTrainReportEvent::Trigger(trackIndex, false);
			}
		}
	}
}

void CNetObjTrain::CreateSyncTree()
{
	ms_TrainSyncTree = rage_new CTrainSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjTrain::DestroySyncTree()
{
	ms_TrainSyncTree->ShutdownTree();
	delete ms_TrainSyncTree;
	ms_TrainSyncTree = 0;
}

netINodeDataAccessor *CNetObjTrain::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == ITrainNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (ITrainNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjVehicle::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

bool CNetObjTrain::Update()
{
    UpdatePendingEngine();
    
	if (!IsClone() && m_nextReportingTime < fwTimer::GetTimeInMilliseconds())
	{
		CTrain* train = GetTrain();
		if (train && train->IsEngine())
		{
			int trackIndex = train->GetTrackIndex();
			CTrain::SetTrackActive(trackIndex,true); //need to handle local too
			CNetworkTrainReportEvent::Trigger(trackIndex,true,255);
			m_nextReportingTime = fwTimer::GetTimeInMilliseconds() + CTrain::ms_iTrainNetworkHearbeatInterval;
		}
	}

    return CNetObjVehicle::Update();
}

void CNetObjTrain::CreateNetBlender()
{
	CTrain *train = GetTrain();
	gnetAssert(train);
	gnetAssert(!GetNetBlender());

	netBlender* blender = rage_new CNetBlenderTrain(train, ms_trainHighPrecisionBlenderData);
	if(AssertVerify(blender))
	{
		blender->Reset();
		SetNetBlender(blender);
	}
}

void CNetObjTrain::UpdateBlenderData()
{
	netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, GetNetBlender());

	if (!blender)
		return;

	if(m_UseHighPrecisionBlending)
	{
		blender->SetBlenderData(ms_trainHighPrecisionBlenderData);
		blender->SetUseLogarithmicBlending(true);
	}
	else
	{
		blender->SetBlenderData(ms_trainBlenderData);
		blender->SetUseLogarithmicBlending(false);
	}
}

void CNetObjTrain::SetUseHighPrecisionBlending(bool useHighPrecisionBlending)
{
	m_UseHighPrecisionBlending = useHighPrecisionBlending;
	UpdateBlenderData();
}


bool CNetObjTrain::ProcessControl()
{
	CNetObjVehicle::ProcessControl(); //Important to do this as the portal interior location of the vehicle will not update and the remote will not get interior locations when in tunnels. lavalley.

	if (IsClone())
	{
		CTrain *train = GetTrain();

        if(train)
        {
            // we can't process control carriages waiting for their associated engine
            // to be cloned
            if(m_PendingEngineID == NETWORK_INVALID_OBJECT_ID)
            {
                if(train->m_nTrainFlags.bEngine || train->GetEngine())
                {
                    train->ProcessControl();
                }
            }
        }
	}

	return true;
}

bool CNetObjTrain::CanPassControl(const netPlayer &player, eMigrationType migrationType, unsigned *resultCode) const
{
	CTrain *train = GetTrain();
	VALIDATE_GAME_OBJECT(train);
	int numOfCarriages = train->GetNumCarriages();
	int carriageCounter = 0;

	if(train)
	{
		if(train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation)
		{
			if (CNetObjVehicle::CanPassControl(player,migrationType,resultCode))
			{
				// do not allow an engine to migrate to a machine that does not have all the carriages yet
				CTrain* nextCar = train->GetLinkedToBackward();
				while (nextCar && carriageCounter <= numOfCarriages)
				{
					carriageCounter++;
					
					if (!nextCar->GetNetworkObject() || !nextCar->GetNetworkObject()->HasBeenCloned(player) || nextCar->GetNetworkObject()->IsPendingRemoval(player))
					{
						return false;
					}

					nextCar = nextCar->GetLinkedToBackward();
				}
#if !__FINAL
				if (nextCar != NULL)
					Quitf("Looping in linked list through more carriages than the train has!");
#endif /* !__FINAL */

				//Note: While watching trains I'm seeing odd behavior here for proximity migratable objects, there might be an issue with them in general - but this fixes things for trains explicitly without tearing things up completely.
				//When watching a train I would see it change ownership based on proximity, but then immediately change to another owner then back again.  It seems to not be based around proximity at all - in fact when investigating
				//in NetObjProximityMigratable it seems there it doesn't do this check below which seems required.  Also it seems that it only does the local scope check within the timer - otherwise allowing it to return true possibly
				//when outside of the timer check for scope (seems bad) / will discuss this with jgurney and dyelland. lavalley.
				if (IsProximityMigration(migrationType))
				{
					Vector3 localPlayerPosition  = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
					Vector3 remotePlayerPosition = NetworkUtils::GetNetworkPlayerPosition(static_cast<const CNetGamePlayer&>(player));

					float distToLocalSqr  = (VEC3V_TO_VECTOR3(train->GetTransform().GetPosition()) - localPlayerPosition).XYMag2();
					float distToRemoteSqr = (VEC3V_TO_VECTOR3(train->GetTransform().GetPosition()) - remotePlayerPosition).XYMag2();

					if (distToRemoteSqr < distToLocalSqr)
					{
						//trainDebugf3("CNetObjTrain::CanPassControl engine distToRemoteSqr[%f] distToLocalSqr[%f] --> return true",distToRemoteSqr,distToLocalSqr);
						return true;
					}
				}
				else
				{
					return true; //other migration types should return true here because CanPassControl is true
				}
			}

			return false;
		}
		else
		{
			CTrain *engine = train->GetEngine();
			if(engine)
			{
				CNetObjTrain* pNetObjTrain = (CNetObjTrain*) engine->GetNetworkObject();
				if (pNetObjTrain)
				{
					if (engine->IsNetworkClone())
					{
						if (player.IsValid() && pNetObjTrain->GetPlayerOwner() && pNetObjTrain->GetPlayerOwner()->IsValid())
						{
							if (player.GetRlPeerId() == pNetObjTrain->GetPlayerOwner()->GetRlPeerId())
								return CNetObjVehicle::CanPassControl(player,migrationType,resultCode);
						}
					}
					else if (pNetObjTrain->IsPendingOwnerChange())
					{
						if (player.IsValid() && pNetObjTrain->GetPendingPlayerOwner() && pNetObjTrain->GetPendingPlayerOwner()->IsValid())
						{
							if (player.GetRlPeerId() == pNetObjTrain->GetPendingPlayerOwner()->GetRlPeerId())
								return CNetObjVehicle::CanPassControl(player,migrationType,resultCode);
						}
					}
				}
			}
		}
	}
	
	if(resultCode)
	{
		*resultCode = CPC_FAIL_TRAIN_ENGINE_LOCAL;
	}
	return false;
}

void CNetObjTrain::TryToPassControlProximity()
{
	CTrain *train = GetTrain();
	VALIDATE_GAME_OBJECT(train);
	int numOfCarriages = train->GetNumCarriages();
	int carriageCounter = 0;

	if (train)
	{
		gnetAssert(!IsClone());

		if (train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation)
		{
			CNetObjVehicle::TryToPassControlProximity();

			if (train->GetNetworkObject() && (train->IsNetworkClone() || train->GetNetworkObject()->GetPendingPlayerOwner()))
			{
				netPlayer* owner = train->IsNetworkClone() ? train->GetNetworkObject()->GetPlayerOwner() : train->GetNetworkObject()->GetPendingPlayerOwner();
				if (owner)
				{
					CTrain* nextCar = train->GetLinkedToBackward();
					while (nextCar && carriageCounter <= numOfCarriages)
					{
						carriageCounter++;

						if (!nextCar->IsNetworkClone() && nextCar->GetNetworkObject() && !nextCar->GetNetworkObject()->IsPendingOwnerChange())
							CGiveControlEvent::Trigger(*owner,nextCar->GetNetworkObject(),MIGRATE_FORCED);

						nextCar = nextCar->GetLinkedToBackward();
					}
#if !__FINAL
					if (nextCar != NULL)
						Quitf("Looping in linked list through more carriages than the train has!");
#endif /* !__FINAL */
				}
			}
		}
		else if (!IsClone())
		{
			//carriage - only allow it to be sent to the owner of the engine - but need to have this here otherwise if the carriage isn't on the same machine as the engine it won't trigger
			//we are checking to ensure that the engine is set above so it should have valid data at this point
			CTrain* engine = train->GetEngine();
			if (engine && engine->GetNetworkObject())
			{
				netPlayer* owner = engine->IsNetworkClone() ? engine->GetNetworkObject()->GetPlayerOwner() : engine->GetNetworkObject()->GetPendingPlayerOwner();
				if ( owner && (owner != GetPlayerOwner())  && train->GetNetworkObject() && !train->GetNetworkObject()->IsPendingOwnerChange() )
				{
					CGiveControlEvent::Trigger(*owner,this,MIGRATE_FORCED);
				}
			}
		}
	}
}

bool CNetObjTrain::TryToPassControlOutOfScope()
{
	bool retc = false;

	CTrain *train = GetTrain();
	VALIDATE_GAME_OBJECT(train);
	int numOfCarriages = train->GetNumCarriages();
	int carriageCounter = 0;

	if (train)
	{
		if (train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation)
		{
			retc = CNetObjVehicle::TryToPassControlOutOfScope();

			if (train->GetNetworkObject() && (train->IsNetworkClone() || train->GetNetworkObject()->GetPendingPlayerOwner()))
			{
				netPlayer* owner = train->IsNetworkClone() ? train->GetNetworkObject()->GetPlayerOwner() : train->GetNetworkObject()->GetPendingPlayerOwner();
				if (owner)
				{
					CTrain* nextCar = train->GetLinkedToBackward();
					while (nextCar && carriageCounter <= numOfCarriages)
					{
						carriageCounter++;

						if (!nextCar->IsNetworkClone() && nextCar->GetNetworkObject() && !nextCar->GetNetworkObject()->IsPendingOwnerChange())
							CGiveControlEvent::Trigger(*owner,nextCar->GetNetworkObject(),MIGRATE_OUT_OF_SCOPE);

						nextCar = nextCar->GetLinkedToBackward();
					}
#if !__FINAL
					if (nextCar != NULL)
						Quitf("Looping in linked list through more carriages than the train has!");
#endif /* !__FINAL */
				}
			}
		}
		else if (!IsClone())
		{
			//only invoke this if the carriage isn't already pending an ownership change - otherwise disregard until it ends up on a particular machine.
			if(!IsPendingOwnerChange())
			{
				//carriage - only allow it to be sent to the owner of the engine - but need to have this here otherwise if the carriage isn't on the same machine as the engine it won't trigger
				//we are checking to ensure that the engine is set above so it should have valid data at this point
				CTrain* engine = train->GetEngine();
				if (engine && engine->GetNetworkObject())
				{
					netPlayer* owner = engine->IsNetworkClone() ? engine->GetNetworkObject()->GetPlayerOwner() : engine->GetNetworkObject()->GetPendingPlayerOwner();
					if (owner && (owner != GetPlayerOwner()))
					{
						CGiveControlEvent::Trigger(*owner,this,MIGRATE_OUT_OF_SCOPE);
					}
				}
			}
		}
	}

	return retc;
}

bool CNetObjTrain::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
    bool inScope = false;

    CTrain *train = GetTrain();
    VALIDATE_GAME_OBJECT(train);
	int numOfCarriages = train->GetNumCarriages();
	int carriageCounter = 0;

    if (train)
    {
		if (IsScriptObject())
		{
			inScope = CNetObjVehicle::IsInScope(player, scopeReason); 
		}
		else if(train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation)
		{
			if (!HasBeenCloned(player))
			{
				// do not allow the engine to go into scope if any of the carriages are trying to remove themselves on this players machine
				CTrain* nextCar = train;
				while (nextCar && carriageCounter <= numOfCarriages)
				{
					carriageCounter++;						

					if (nextCar->GetNetworkObject() && (nextCar->GetNetworkObject()->HasBeenCloned(player) || nextCar->GetNetworkObject()->IsPendingRemoval(player)))
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_TRAIN_ENGINE_WAIING_ON_CARRIAGES;
						}

						return false;
					}

					nextCar = nextCar->GetLinkedToBackward();
				}
#if !__FINAL
				if (nextCar != NULL)
					Quitf("Looping in linked list through more carriages than the train has!");
#endif /* !__FINAL */
			}
			
			inScope = CNetObjVehicle::IsInScope(player, scopeReason); 
		}
		else 
		{
			// carriages go in/out of scope with the engine
			CTrain      *engine       = train->GetEngine();
			CNetObjGame *engineNetObj = engine ? engine->GetNetworkObject() : 0;

			if (engineNetObj)
			{
				if(!engineNetObj->IsClone())
				{
					inScope = engineNetObj->IsPendingCloning(player) || (engineNetObj->HasBeenCloned(player) && !engineNetObj->IsPendingRemoval(player));

					if (scopeReason)
					{
						*scopeReason = SCOPE_TRAIN_USE_ENGINE_SCOPE;
					}
				}
				else if (engineNetObj->GetPlayerOwner() == &player)
				{
					inScope = true;

					if (scopeReason)
					{
						*scopeReason = SCOPE_TRAIN_USE_ENGINE_SCOPE;
					}
				}
				else
				{
					if (scopeReason)
					{
						*scopeReason = SCOPE_TRAIN_ENGINE_NOT_LOCAL;
					}

				}
			}
			else
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_NO_TRAIN_ENGINE;
				}
			}
		}
	}

    return inScope;
}

void CNetObjTrain::PostCreate()
{
	CTrain *train = GetTrain();
	VALIDATE_GAME_OBJECT(train);

	if (train && (train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation))
	{
		// if this is an engine find all associated carriages and hook up the engine ptr immediately 
		netObject* objects[MAX_NUM_NETOBJECTS];
		unsigned numObjects = netInterface::GetObjectManager().GetAllObjects(objects, MAX_NUM_NETOBJECTS, true, false);

		for (u32 i=0; i<numObjects; i++)
		{
			netObject* pNetObj = objects[i];

			if (pNetObj->GetObjectType() == NET_OBJ_TYPE_TRAIN)
			{
				CNetObjTrain *pTrainObj = SafeCast(CNetObjTrain, pNetObj);

				if (pTrainObj->m_PendingEngineID == GetObjectID() && pTrainObj->GetTrain())
				{
					pTrainObj->GetTrain()->SetEngine( train, false );

					if (pTrainObj->GetTrain()->GetEngine())
					{
						pTrainObj->m_PendingEngineID = NETWORK_INVALID_OBJECT_ID;
					}
				}
			}
		}

#if !__FINAL
		CTrain::ValidateLinkedLists(train);
#endif
	}
}

void CNetObjTrain::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
#if !__NO_OUTPUT
	bool bWasClone = IsClone();
#endif

	CNetObjVehicle::ChangeOwner(player, migrationType);

#if !__NO_OUTPUT
	trainDebugf2("CNetObjTrain::ChangeOwner train[%p] model[%s] logname[%s] isengine[%d] bWasClone[%d] IsClone[%d] owner[%s]",GetTrain(),GetTrain() ? GetTrain()->GetModelName() : "",GetLogName(),GetTrain() ? GetTrain()->IsEngine() : false,bWasClone,IsClone(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");
#endif
}

void  CNetObjTrain::PlayerHasJoined(const netPlayer& player)
{
	if (!IsClone())
	{
		CTrain* train = GetTrain();
		if (train)
			train->InitiateTrainReportOnJoin(player.GetPhysicalPlayerIndex());
	}
}

void CNetObjTrain::GetTrainGameState(CTrainGameStateDataNode& data) const
{
    CTrain *train = GetTrain();
    VALIDATE_GAME_OBJECT(train);

    if(train == 0)
    {
        data.m_IsEngine                   = false;
		data.m_AllowRemovalByPopulation	  = false;
        data.m_IsCaboose                  = false;
        data.m_IsMissionTrain             = false;
        data.m_Direction                  = false;
        data.m_HasPassengerCarriages      = false;
        data.m_RenderDerailed             = false;
		data.m_StopForStations			  = false;
        data.m_EngineID                   = NETWORK_INVALID_OBJECT_ID;
        data.m_TrainConfigIndex           = -1;
        data.m_CarriageConfigIndex        = -1;
        data.m_TrackID                    = -1;
        data.m_DistFromEngine             = 0.0f;
        data.m_CruiseSpeed                = 0.0f;
		data.m_LinkedToBackwardID		  = NETWORK_INVALID_OBJECT_ID;
		data.m_LinkedToForwardID		  = NETWORK_INVALID_OBJECT_ID;
		data.m_TrainState				  = CTrain::TS_Moving;
		data.m_doorsForcedOpen			  = false;
		data.m_UseHighPrecisionBlending   = false;
    }
    else
    {
        data.m_IsEngine                 = train->m_nTrainFlags.bEngine;
		data.m_AllowRemovalByPopulation = train->m_nTrainFlags.bAllowRemovalByPopulation;
		data.m_IsCaboose                = train->m_nTrainFlags.bCaboose;
		data.m_IsMissionTrain           = false; //train->m_nTrainFlags.bMissionTrain;
		data.m_Direction                = train->m_nTrainFlags.bDirectionTrackForward;
		data.m_HasPassengerCarriages    = train->m_nTrainFlags.bHasPassengerCarriages;
		data.m_RenderDerailed           = train->m_nTrainFlags.bRenderAsDerailed;
		data.m_EngineID                 = m_PendingEngineID;
		data.m_TrainConfigIndex         = train->m_trainConfigIndex;
		data.m_CarriageConfigIndex      = train->m_trainConfigCarriageIndex;
		data.m_TrackID                  = train->m_trackIndex;
		data.m_DistFromEngine           = train->m_trackDistFromEngineFrontForCarriageFront;
		data.m_CruiseSpeed              = CTrain::GetCarriageCruiseSpeed(train);
		data.m_TrainState			    = (unsigned) train->GetTrainState();
		data.m_doorsForcedOpen		    = train->GetDoorsForcedOpen();
		data.m_StopForStations		    = train->m_nTrainFlags.bStopForStations;
		data.m_UseHighPrecisionBlending = m_UseHighPrecisionBlending;

        CTrain *engine = CTrain::FindEngine(train);

        if(engine && !engine->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		{
			if (engine->GetNetworkObject())
	        {
		        data.m_EngineID = engine->GetNetworkObject()->GetObjectID();
			}
#if __BANK
			else
			{
				// Only assert if the network is not in the process of shutting down
				gnetAssertf(CNetwork::IsNetworkClosing(), "Train [%p][%s] engine [%p][%s] is not registered with the network!",train,train ? train->GetModelName() : "",engine,engine ? engine->GetModelName() : "");
			}
#endif // __BANK
		}

		data.m_LinkedToBackwardID = train->GetLinkedToBackward() && train->GetLinkedToBackward()->GetNetworkObject() ? train->GetLinkedToBackward()->GetNetworkObject()->GetObjectID() : m_PendingLinkedToBackwardID;
		data.m_LinkedToForwardID = train->GetLinkedToForward() && train->GetLinkedToForward()->GetNetworkObject() ? train->GetLinkedToForward()->GetNetworkObject()->GetObjectID() : m_PendingLinkedToForwardID;
    }
}

void CNetObjTrain::SetTrainGameState(const CTrainGameStateDataNode& data)
{
    CTrain *train = GetTrain();
    VALIDATE_GAME_OBJECT(train);

    if(train)
    {
		train->m_nTrainFlags.bEngine                      = data.m_IsEngine;
		train->m_nTrainFlags.bAllowRemovalByPopulation	  = data.m_AllowRemovalByPopulation;
        train->m_nTrainFlags.bCaboose                     = data.m_IsCaboose;
        train->m_nTrainFlags.bDirectionTrackForward       = data.m_Direction;
        train->m_nTrainFlags.bHasPassengerCarriages       = data.m_HasPassengerCarriages;
        train->m_nTrainFlags.bRenderAsDerailed            = data.m_RenderDerailed;
		train->m_nTrainFlags.bStopForStations			  = data.m_StopForStations;
        train->m_trainConfigIndex                         = data.m_TrainConfigIndex;
        train->m_trainConfigCarriageIndex                 = data.m_CarriageConfigIndex;
        train->m_trackIndex                               = data.m_TrackID;
        train->m_trackDistFromEngineFrontForCarriageFront = data.m_DistFromEngine;
        CTrain::SetCarriageCruiseSpeed(train, data.m_CruiseSpeed);

		train->ChangeTrainState((CTrain::TrainState)data.m_TrainState);

		train->SetDoorsForcedOpen(data.m_doorsForcedOpen);

        m_PendingEngineID = data.m_EngineID;
		m_PendingLinkedToBackwardID = data.m_LinkedToBackwardID;
		m_PendingLinkedToForwardID = data.m_LinkedToForwardID;

        if(train->m_nTrainFlags.bEngine || train->m_nTrainFlags.bAllowRemovalByPopulation)
        {
			train->SetEngine(train);
        }

		m_UseHighPrecisionBlending = data.m_UseHighPrecisionBlending;

        UpdatePendingEngine();
    }
}

bool CNetObjTrain::CanBlendWhenFixed() const
{
    return true;
}

void CNetObjTrain::UpdatePendingEngine()
{
    CTrain *train = GetTrain();
    VALIDATE_GAME_OBJECT(train);

    if(train)
    {
        if(m_PendingEngineID != NETWORK_INVALID_OBJECT_ID)
        {
            netObject *trainNetObj = NetworkInterface::GetNetworkObject(m_PendingEngineID);

			CTrain *engine = NetworkUtils::GetTrainFromNetworkObject(trainNetObj);

			if(engine)
			{
				train->SetEngine( engine );
            
				if(train->GetEngine())
				{
					m_PendingEngineID = NETWORK_INVALID_OBJECT_ID;
				}
			}
        }

		if(m_PendingLinkedToBackwardID != NETWORK_INVALID_OBJECT_ID)
		{
			netObject *trainNetObj = NetworkInterface::GetNetworkObject(m_PendingLinkedToBackwardID);
			if (trainNetObj)
			{
				CTrain *backwardTrain = NetworkUtils::GetTrainFromNetworkObject(trainNetObj);

				if(backwardTrain)
				{
					train->SetLinkedToBackward(backwardTrain);
					backwardTrain->SetLinkedToForward(train);

					if(train->GetLinkedToBackward())
					{
						gnetAssertf(train != train->GetLinkedToBackward(), "Train has been linked to itself!");

						//Don't allow the train to link back to itself
						if (train == train->GetLinkedToBackward())
							train->SetLinkedToBackward( NULL );

						m_PendingLinkedToBackwardID = NETWORK_INVALID_OBJECT_ID;
					}
				}
			}
		}

		if(m_PendingLinkedToForwardID != NETWORK_INVALID_OBJECT_ID)
		{
			netObject *trainNetObj = NetworkInterface::GetNetworkObject(m_PendingLinkedToForwardID);
			if (trainNetObj)
			{
				CTrain *forwardTrain = NetworkUtils::GetTrainFromNetworkObject(trainNetObj);

				if(forwardTrain)
				{
					train->SetLinkedToForward(forwardTrain);
					forwardTrain->SetLinkedToBackward(train);

					if(train->GetLinkedToForward())
					{
						gnetAssertf(train != train->GetLinkedToForward(), "Train has been linked to itself!");

						//Don't allow the train to link back to itself
						if (train == train->GetLinkedToForward())
							train->SetLinkedToForward( NULL );

						m_PendingLinkedToForwardID = NETWORK_INVALID_OBJECT_ID;
					}
				}
			}
		}
    }
}
