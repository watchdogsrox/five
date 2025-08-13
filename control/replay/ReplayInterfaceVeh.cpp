#include "ReplayInterfaceVeh.h"

#if GTA_REPLAY

#include "replay.h"

#include "camera/base/BaseCamera.h"
#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/system/CameraMetadata.h"
#include "entity/entity.h"
#include "entity/transform.h"
#include "Peds/ped.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "scene/world/GameWorld.h"
#include "Streaming/streaming.h"
#include "vehicles/Bike.h"
#include "vehicles/Bmx.h"
#include "Vehicles/Boat.h"
#include "vehicles/Heli.h"
#include "vehicles/Planes.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/Trailer.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "Effects/ParticleVehicleFxPacket.h"
#include "vector/vector3.h"
#include "vfx/misc/Fire.h"
#include "Control/Replay/Entity/FragmentPacket.h"
#include "Control/Garages.h"
#include "control/replay/Vehicle/WheelPacket.h"

#include "ReplayInterfaceImpl.h"

const char*	CReplayInterfaceTraits<CVehicle>::strShort = "VEH";
const char*	CReplayInterfaceTraits<CVehicle>::strLong = "VEHICLE";
const char*	CReplayInterfaceTraits<CVehicle>::strShortFriendly = "Veh";
const char*	CReplayInterfaceTraits<CVehicle>::strLongFriendly = "ReplayInterface-Vehicle";

const int	CReplayInterfaceTraits<CVehicle>::maxDeletionSize = MAX_VEHICLE_DELETION_SIZE;

const u32 VEH_RECORD_BUFFER_SIZE		= 256*1024;
const u32 VEH_CREATE_BUFFER_SIZE		= 224*1024;
const u32 VEH_FULLCREATE_BUFFER_SIZE	= 224*1024;

//////////////////////////////////////////////////////////////////////////
CReplayInterfaceVeh::CReplayInterfaceVeh()
	: CReplayInterface<CVehicle>(*CVehicle::GetPool(), VEH_RECORD_BUFFER_SIZE, VEH_CREATE_BUFFER_SIZE, VEH_FULLCREATE_BUFFER_SIZE)
{
	m_relevantPacketMin = PACKETID_VEHICLE_MIN;
	m_relevantPacketMax	= PACKETID_VEHICLE_MAX;

	m_createPacketID	= PACKETID_VEHICLECREATE;
	m_updatePacketID	= PACKETID_VEHICLEUPDATE;
	m_deletePacketID	= PACKETID_VEHICLEDELETE;
	m_packetGroup		= REPLAY_PACKET_GROUP_VEHICLE;

	m_enableSizing				= true;
	m_enableRecording			= true;
	m_enablePlayback			= true;

	m_recordDoors = m_recordWheels = m_recordFrags = m_recordFragBones = true;
	m_recordOldStyleWheels		= false;
	m_recordOnlyPlayerCars		= false;

	m_anomaliesCanExist			= false;

	m_LastDialVehicle			= NULL;

	// Set up the relevant packets for this interface
	AddPacketDescriptor<CPacketVehicleUpdate>(				PACKETID_VEHICLEUPDATE,	STRINGIFY(CPacketVehicleUpdate));
	AddPacketDescriptor<CPacketVehicleCreate>(				PACKETID_VEHICLECREATE,	STRINGIFY(CPacketVehicleCreate));
	AddPacketDescriptor<CPacketVehicleDelete>(				PACKETID_VEHICLEDELETE,	STRINGIFY(CPacketVehicleDelete));
	AddPacketDescriptor<CPacketCarUpdate, CAutomobile>(		PACKETID_CARUPDATE,		STRINGIFY(CPacketCarUpdate),		VEHICLE_TYPE_CAR);
	AddPacketDescriptor<CPacketBicycleUpdate, CBmx>(		PACKETID_BICYCLEUPDATE,	STRINGIFY(CPacketBicycleUpdate),	VEHICLE_TYPE_BICYCLE);
	AddPacketDescriptor<CPacketBikeUpdate, CBike>(			PACKETID_BIKEUPDATE,	STRINGIFY(CPacketBikeUpdate),		VEHICLE_TYPE_BIKE);
	AddPacketDescriptor<CPacketBoatUpdate, CBoat>(			PACKETID_BOATUPDATE,	STRINGIFY(CPacketBoatUpdate),		VEHICLE_TYPE_BOAT);
	AddPacketDescriptor<CPacketHeliUpdate, CHeli>(			PACKETID_HELIUPDATE,	STRINGIFY(CPacketHeliUpdate),		VEHICLE_TYPE_HELI);
	AddPacketDescriptor<CPacketBlimpUpdate, CBlimp>(		PACKETID_BLIMPUDPATE,	STRINGIFY(CPacketBlimpUpdate),		VEHICLE_TYPE_BLIMP);
	AddPacketDescriptor<CPacketPlaneUpdate, CPlane>(		PACKETID_PLANEUPDATE,	STRINGIFY(CPacketPlaneUpdate),		VEHICLE_TYPE_PLANE);
	AddPacketDescriptor<CPacketQuadBikeUpdate, CQuadBike>(	PACKETID_QUADBIKEUPDATE,STRINGIFY(CPacketQuadBikeUpdate),	VEHICLE_TYPE_QUADBIKE);
	AddPacketDescriptor<CPacketSubUpdate, CSubmarine>(		PACKETID_SUBUPDATE,		STRINGIFY(CPacketSubUpdate),		VEHICLE_TYPE_SUBMARINE);
	AddPacketDescriptor<CPacketSubCarUpdate, CSubmarineCar>(PACKETID_SUBCARUPDATE,	STRINGIFY(CPacketSubCarUpdate),		VEHICLE_TYPE_SUBMARINECAR);
	AddPacketDescriptor<CPacketTrailerUpdate, CTrailer>(	PACKETID_TRAILERUPDATE,	STRINGIFY(CPacketTrailerUpdate),	VEHICLE_TYPE_TRAILER);
	AddPacketDescriptor<CPacketTrainUpdate, CTrain>(		PACKETID_TRAINUPDATE,	STRINGIFY(CPacketTrainUpdate),		VEHICLE_TYPE_TRAIN);
	AddPacketDescriptor<CPacketAmphAutoUpdate, CAmphibiousAutomobile>(		PACKETID_AMPHAUTOUPDATE,	STRINGIFY(CPacketAmphAutoUpdate),		VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE);
	AddPacketDescriptor<CPacketAmphQuadUpdate, CAmphibiousQuadBike>(		PACKETID_AMPHQUADUPDATE,	STRINGIFY(CPacketAmphQuadUpdate),		VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);
	AddPacketDescriptor<CPacketVehicleWheelUpdate_Old>(		PACKETID_WHEELUPDATE_OLD,	STRINGIFY(CPacketVehicleWheelUpdate_Old));
	AddPacketDescriptor<CPacketVehicleDoorUpdate>(			PACKETID_DOORUPDATE,	STRINGIFY(CPacketVehicleDoorUpdate));
	AddPacketDescriptor<CPacketFragData>(					PACKETID_FRAGDATA,		STRINGIFY(CPacketFragData));
	AddPacketDescriptor<CPacketVehicleUpdate::tfragBoneDataType>(CPacketVehicleUpdate::fragBoneDataPacketId,	STRINGIFY(CPacketVehicleUpdate::tfragBoneDataType));
	AddPacketDescriptor<CPacketVehicleUpdate::tfragBoneDataType_HighQuality>(CPacketVehicleUpdate::fragBoneDataPacketIdHQ,	STRINGIFY(CPacketVehicleUpdate::tfragBoneDataType_HighQuality));
	AddPacketDescriptor<CPacketWheel>(						PACKETID_WHEEL,			STRINGIFY(CPacketWheel));
	AddPacketDescriptor<CPacketFragData_NoDamageBits>(		PACKETID_FRAGDATA_NO_DAMAGE_BITS, STRINGIFY(CPacketFragData_NoDamageBits));
}


//////////////////////////////////////////////////////////////////////////
template<typename PACKETTYPE, typename VEHICLESUBTYPE>
void CReplayInterfaceVeh::AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 vehicleType)
{
	REPLAY_CHECK(m_numPacketDescriptors < maxPacketDescriptorsPerInterface, NO_RETURN_VALUE, "Max number of packet descriptors breached");
	m_packetDescriptors[m_numPacketDescriptors++] = CPacketDescriptor<CVehicle>(packetID, 
															vehicleType, 
															sizeof(PACKETTYPE), 
															packetName,
															&iReplayInterface::template PrintOutPacketDetails<PACKETTYPE>,
															&CReplayInterface<CVehicle>::template RecordElementInternal<PACKETTYPE, VEHICLESUBTYPE>,
															&CReplayInterface<CVehicle>::template ExtractPacket<PACKETTYPE, VEHICLESUBTYPE>);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::ClearWorldOfEntities()
{
	CPed* pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		CVehicle* pPlayerVehicle = pPlayer->GetMyVehicle();
		if(pPlayerVehicle)
		{
			pPlayer->SetPedOutOfVehicle();
		}
	}

	// Clear garages contents
	CGarages::ClearAll();

	// Clear all Replay owned Vehicles
	CReplayInterface<CVehicle>::ClearWorldOfEntities();

	CTrain::RemoveAllTrains();

	const Vector3 VecCoors(0.0f, 0.0f, 0.0f);
	const float Radius = 1000000.0f;
	CGameWorld::ClearCarsFromArea(VecCoors, Radius, false, false);//, false, true, true, true);

	m_HDVehicles.clear();

	m_LastDialVehicle = NULL;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::DisplayEstimatedMemoryUsageBreakdown()
{
	//replayDisplayf("base = %d, wheels = %d(%d), doors = %d, frag = %d\n,", m_basePackets, m_wheelPackets, m_noOfWheels, m_doorPackets, m_fragPackets);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::GetBlockDetails(char* pString, bool& err, bool recording) const
{
	CReplayInterface<CVehicle>::GetBlockDetails(pString, err, recording);

	size_t p = strlen(pString);
	sprintf(&pString[p], "\n   base = %d, wheels = %d(%d), doors = %d, frag = %d", m_basePackets, m_wheelPackets, m_noOfWheels, m_doorPackets, m_fragPackets);

	return true;
}


//////////////////////////////////////////////////////////////////////////
#if __BANK
bkGroup* CReplayInterfaceVeh::AddDebugWidgets()
{
	bkGroup* pGroup = CReplayInterface<CVehicle>::AddDebugWidgets();

	pGroup->AddToggle("Record Doors", &m_recordDoors);
	pGroup->AddToggle("Record Wheels", &m_recordWheels);
	pGroup->AddToggle("Record Frags", &m_recordFrags);
	pGroup->AddToggle("Record Frag Bones", &m_recordFragBones);
	pGroup->AddToggle("Record old style wheels", &m_recordOldStyleWheels);


	return pGroup;
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PreRecordFrame(CReplayRecorder&, CReplayRecorder&)
{
	m_basePackets = m_doorPackets = m_noOfWheels = m_wheelPackets = m_fragPackets = 0;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::ShouldRecordElement(const CVehicle* pVehicle) const
{
	return (pVehicle->GetIsInPopulationCache() == false) && (pVehicle->GetIsInReusePool() == false);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::RecordElement(CVehicle* pVehicle, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex)
{
	if(m_recordOnlyPlayerCars && FindPlayerPed()->GetVehiclePedInside() != pVehicle)
		return false;

	REPLAY_CHECK(pVehicle->GetReplayID() != ReplayIDInvalid && ReplayEntityExtension::HasExtension(pVehicle), false, "Can't record an %s because it doesn't have a replay extension (ID was %X)", GetShortStr(), pVehicle->GetReplayID().ToInt());

	CPacketVehicleUpdate* pVehicleUpdate = NULL;
	// Find the correct packet descriptor for the vehicle type and call the
	// record function
	const CPacketDescriptor<CVehicle>* pPacketDescriptor = NULL;
	if(FindPacketDescriptorForEntityType(pVehicle->GetVehicleType(), pPacketDescriptor))
	{
		pVehicleUpdate = (CPacketVehicleUpdate*)&(recorder.GetBuffer()[recorder.GetMemUsed()]);
		CPacketDescriptor<CVehicle>::tRecFunc recFunc = pPacketDescriptor->GetRecFunc();
		(this->*recFunc)(pVehicle, recorder, createPacketRecorder, fullCreatePacketRecorder, sessionBlockIndex);
		++m_basePackets;
	}

	int numWheels = 0;
	int numDoors = 0;
	int numFrags = 0;	

	if(m_recordWheels)
		numWheels	= RecordWheels(pVehicle, recorder, sessionBlockIndex);
	if(m_recordDoors)
		numDoors	= RecordDoors(pVehicle, recorder);
	if(m_recordFrags)
		numFrags	= RecordFragData(pVehicle, recorder);

	if(pVehicleUpdate)
	{
		pVehicleUpdate->ValidatePacket();
		pVehicleUpdate->UpdateAfterPartsRecorded(pVehicle, numWheels, numDoors, numFrags);
	}
	return true;
}

#if REPLAY_DELAYS_QUANTIZATION
//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PostRecordOptimize(CPacketBase* pPacket) const
{
#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	if (pPacket->GetPacketID() == CPacketVehicleUpdate::fragBoneDataPacketIdHQ)
	{
		CPacketVehicleUpdate::tfragBoneDataType_HighQuality* pFrag = static_cast<CPacketVehicleUpdate::tfragBoneDataType_HighQuality*>(pPacket);

		CPacketVehicleUpdate::tSkeletonBoneDataType_HighQuality* pSkeletonBoneData = pFrag->GetUndamaged();
		if (pSkeletonBoneData)
			pSkeletonBoneData->QuantizeQuaternions();
		pSkeletonBoneData = pFrag->GetDamaged();
		if (pSkeletonBoneData)
			pSkeletonBoneData->QuantizeQuaternions();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::ResetPostRecordOptimizations() const
{
#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	CPacketVehicleUpdate::tSkeletonBoneDataType_HighQuality::ResetDelayedQuaternions();
#endif
}
#endif // REPLAY_DELAYS_QUANTIZATION


//////////////////////////////////////////////////////////////////////////
ePlayPktResult CReplayInterfaceVeh::PlayPackets(ReplayController& controller, bool entityMayNotExist)
{
	ePlayPktResult result = CReplayInterface<CVehicle>::PlayPackets(controller, entityMayNotExist);

	if(m_trainEngines.GetCount() > 0)
	{
		// Loop through each train engine and link up the train set...
		for(int i = 0; i < m_trainEngines.GetCount(); ++i)
		{
			CReplayID engineID = m_trainEngines[i];
			replayAssertf(m_trainLinkInfos.Access(engineID) != NULL, "Can't find engine in train infos");
			if(m_trainLinkInfos.Access(engineID) == NULL)
			{
				continue;
			}

			const trainInfo engineInfo = *m_trainLinkInfos.Access(engineID);
			engineInfo.pTrain->SetEngine(engineInfo.pTrain, false);	// Set the engine as it's ummm engine...
			replayAssertf(engineInfo.pTrain->m_nTrainFlags.bEngine, "Engine isn't an engine?");

			const trainInfo* pTrainInfo = &engineInfo;
			while(pTrainInfo)	// While we have carriages...
			{
				CTrain* pTrain = pTrainInfo->pTrain;

				const trainInfo* pForeInfo = m_trainLinkInfos.Access(pTrainInfo->linkageFore);
				const trainInfo* pBackInfo = m_trainLinkInfos.Access(pTrainInfo->linkageBack);

				if(pForeInfo)
				{
					pTrain->SetLinkedToForward(pForeInfo->pTrain, false);
					pForeInfo->pTrain->SetLinkedToBackward(pTrain, false);
					m_trainLinkInfos.Delete(pTrainInfo->linkageFore);					
				}

				if(pBackInfo)
				{
					pBackInfo->pTrain->SetEngine(engineInfo.pTrain, false);
					pTrain->SetLinkedToBackward(pBackInfo->pTrain, false);
					pBackInfo->pTrain->SetLinkedToForward(pTrain, false);					
				}
				else
				{
					// Last train here...
					if(pTrain->m_nTrainFlags.bCaboose == false && pTrain->m_nTrainFlags.bEngine == false)
					{
						replayDebugf1("Last in this set isn't set as Caboose or Engine on its own...how strange");
					}
					
					m_trainLinkInfos.Delete(pTrain->GetReplayID());
				}

				pTrainInfo = pBackInfo;
			}
		}
		m_trainEngines.clear();
	}

	// I think there's a strange train linking issue in game...just clear these out here as we can't link them.
	m_trainLinkInfos.Reset();

	replayAssertf(m_trainEngines.empty(), "Train engine array is not empty!");
	replayAssertf(m_trainLinkInfos.GetNumUsed() == 0, "Train Link map is not empty!");

	return result;
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfaceVeh::JumpPackets(ReplayController& controller)
{
	m_HDVehicles.clear();
	CReplayInterface<CVehicle>::JumpPackets(controller);
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfaceVeh::PlaybackSetup(ReplayController& controller)
{
	//is this function needed?
	eReplayPacketId packetID = controller.GetCurrentPacketID();

	// Don't do PlaybackSetup() for the wheel update, door update or damage update packets
 	if(	packetID == PACKETID_WHEELUPDATE_OLD ||
 		packetID == PACKETID_DOORUPDATE ||
 		packetID == PACKETID_VEHICLEDMGUPDATE ||
 		packetID == PACKETID_FRAGDATA ||
 		packetID == CPacketVehicleUpdate::fragBoneDataPacketId ||
		packetID == CPacketVehicleUpdate::fragBoneDataPacketIdHQ ||
		packetID == PACKETID_WHEEL)
 		return;

	CReplayInterface<CVehicle>::PlaybackSetup(controller);
}


//////////////////////////////////////////////////////////////////////////
// This is performed to go through all the entities that did not have a 
// deletion packet and therefore did not get their fading handled in the
// CReplayInterface<T>::ApplyFades() function.
// In addition for the Vehicle we must handle the scorching
void CReplayInterfaceVeh::ApplyFadesSecondPass()
{
	for (s32 entity = 0; entity < m_pool.GetSize(); entity++)
	{
		CEntityData& rEntityData = m_entityDatas[entity];

		if (rEntityData.m_iInstID != -1)
		{
			for (s32 packet = 0; packet < CEntityData::NUM_FRAMES_BEFORE_SCORCHING; packet++)
			{
				// Move scorching 3 frames later (to tweak if needed)
				if (rEntityData.m_apvPacketScorched[packet])
				{
					bool bIsScorched = false;
					CPacketVehicleUpdate* pPacket = (CPacketVehicleUpdate*)rEntityData.m_apvPacketScorched[packet];
					
					bIsScorched = pPacket->IsScorched();

					if (bIsScorched && packet < CEntityData::NUM_FRAMES_BEFORE_SCORCHING)
					{
						bIsScorched = false;
					}

					pPacket->SetScorched(bIsScorched);
				}
			}

			rEntityData.Reset();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::HandleCreatePacket(const tCreatePacket* pCreatePacket, bool notDelete, bool firstFrame, const CReplayState& state)
{
	CReplayInterface<CVehicle>::HandleCreatePacket(pCreatePacket, notDelete, firstFrame, state);

	CEntityGet<CVehicle> entityGet(pCreatePacket->GetReplayID());
	GetEntity(entityGet);
	CVehicle* pVehicle = entityGet.m_pEntity;

	if(pVehicle)
		((CPacketVehicleCreate *)pCreatePacket)->ExtractReferenceWheelValuesIntoVehicleExtension(pVehicle);
}


//////////////////////////////////////////////////////////////////////////
// Create using a create packet
CVehicle* CReplayInterfaceVeh::CreateElement(CReplayInterfaceTraits<CVehicle>::tCreatePacket const* pPacket, const CReplayState& /*state*/)
{
	CVehicleStreamRequestGfx::Pool* pStreamReqPool = CVehicleStreamRequestGfx::GetPool();
	if (pStreamReqPool->GetNoOfFreeSpaces() == 0)
	{
		replayDebugf1("Unable to create vehicle 0x%08X right now as we have no CVehicleStreamRequestGfx", pPacket->GetReplayID().ToInt());
		return nullptr;
	}

	fwModelId modelID = m_modelManager.LoadModel(pPacket->GetModelNameHash(), pPacket->GetMapTypeDefIndex(), !pPacket->UseMapTypeDefHash(), true);
	if(modelID.IsValid() == false)
	{
		HandleFailedToLoadModelError(pPacket);
		return NULL;
	}

	CVehicle* pVehicle = NULL;

	Matrix34 creationMatrix;
	pPacket->LoadMatrix(creationMatrix);

	//todofive not sure this is entirely right but it might get us going...
	pVehicle = CVehicleFactory::GetFactory()->Create(modelID, ENTITY_OWNEDBY_REPLAY, POPTYPE_REPLAY, &creationMatrix, true);
	if(!pVehicle)
		return NULL;

	pVehicle->RemovePhysics();
	pVehicle->SetFixedPhysics(true);
	pVehicle->DisablePhysicsIfFixed();

	replayAssert(pVehicle);

	if (pVehicle)
	{
		pVehicle->SetStatus(STATUS_PLAYER);//todo4five this might need special replay statud the old one was		// Don't do any updating stuff
		pVehicle->DisableCollision();
		// TODO: Don't know if we need this still?
		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			((CBoat*)pVehicle)->GetAnchorHelper().Anchor(false);
		}

		CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE );

		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
		{
			pVehicle->GetPortalTracker()->ScanUntilProbeTrue();
		}

		pPacket->SetVariationData(pVehicle);

		pVehicle->SetLiveryId(pPacket->GetLiveryId());
		//pVehicle->SetLivery2Id(pPacket->GetLivery2Id()); // Have to do this in the extension
		pVehicle->UpdateBodyColourRemapping();
		pVehicle->m_nDisableExtras = pPacket->GetDisableExtras();

		CCustomShaderEffectVehicle* pCSEV = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
		pCSEV->SetLicensePlateText(pPacket->GetLicencePlateText());
		pCSEV->SetLicensePlateTexIndex(pPacket->GetLicencePlateTexIdx());

		//pVehicle->GetVehicleAudioEntity()->SetVehicleHornSoundHash(pPacket->GetHornHash());
		//pVehicle->GetVehicleAudioEntity()->SetVehicleHornSoundIndex(pPacket->GetHornIndex());

		pVehicle->GetVehicleAudioEntity()->SetAlarmType(static_cast<audCarAlarmType>(pPacket->GetAlarmType()));
		pVehicle->GetVehicleAudioEntity()->SetRadioEnabled(false);

		// Set the vehicle up with replay related info
		PostCreateElement(pPacket, pVehicle, pPacket->GetReplayID());

#if REPLAY_WAIT_FOR_HD_MODELS
		if(pPacket->GetPlayerInThisVehicle())
		{
			replayDebugf1("Add HD car %p, 0x%08X", pVehicle, pPacket->GetReplayID().ToInt());
			pVehicle->SetForceHd(true);
			m_HDVehicles.PushAndGrow(pVehicle);
		}
#endif // REPLAY_WAIT_FOR_HD_MODELS		

		pCSEV->SetEnvEffScaleU8(pPacket->GetEnvEffScale());
		pPacket->SetCustomColors(pCSEV);

		// Extract the wheel initial default values.
		((CPacketVehicleCreate *)pPacket)->ExtractReferenceWheelValuesIntoVehicleExtension(pVehicle);
	}

	return pVehicle;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::ResetEntity(CVehicle* pVehicle)
{
	for(int i = 0; i < pVehicle->GetNumDoors(); ++i)
	{
		CCarDoor* pDoor = pVehicle->GetDoor(i);
		pDoor->ResetLastAudioTime();
	}

	// Get rid of any attached fires
	g_fireMan.ExtinguishEntityFires(pVehicle, true);
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::StopEntitySounds(CVehicle* pVehicle)
{
	// Reset the vehicle audio now audNorthAudioEngine::StopAllSounds(); has been called
	if(pVehicle && pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->SetForcedGameObjectResetRequired();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	PrintOutPacketHeader(pBasePacket, mode, handle);

	const CPacketDescriptor<CVehicle>* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(pBasePacket->GetPacketID(), pPacketDescriptor))
	{
		pPacketDescriptor->PrintOutPacket(controller, mode, handle);
	}
	else
	{
		u32 packetSize = pBasePacket->GetPacketSize();
		replayAssert(packetSize != 0);
		controller.AdvanceUnknownPacket(packetSize);
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::SetVehicleVariationData(CVehicle* pVehicle)
{
	if(HasCreatePacket(pVehicle->GetReplayID()))
	{
		CPacketVehicleCreate* pCreatePacket = GetCreatePacket(pVehicle->GetReplayID());
		const CStoredVehicleVariationsREPLAY& currentRecorededVariation = pCreatePacket->GetLatestVariationData();
		

		const CVehicleVariationInstance& currentVehicleVariationInstance = pVehicle->GetVariationInstance();
		CStoredVehicleVariationsREPLAY currentVehicleVariation;

		//make sure we set the kit if to be the index here, like we do when we store the packets
		//otherwise the comparison below can fail causing it to think the variation is always changing
		u16 currentKitId = INVALID_VEHICLE_KIT_ID;
		if(currentVehicleVariationInstance.GetKitIndex() < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount())
		{
			currentKitId = CVehicleModelInfo::GetVehicleColours()->m_Kits[currentVehicleVariationInstance.GetKitIndex()].GetId();
		}

		currentVehicleVariation.StoreVariations(pVehicle);
		//currentVehicleVariation.SetKitIndex(currentKitId);
		CPacketVehicleCreate::VehicleCreateExtension* pExt = pCreatePacket->GetExtension();

		if(currentVehicleVariation.IsEqual(currentRecorededVariation) == false || pExt->m_latestKitId != currentKitId)
		{
			CReplayMgr::RecordFx<CPacketVehVariationChange>(CPacketVehVariationChange(pVehicle), pVehicle);
			
			pCreatePacket->GetLatestVariationData().StoreVariations(pVehicle);
			
			//Store the kit ID in the kit index to use to lookup the correct index.
			/*pCreatePacket->GetLatestVariationData().SetKitIndex(currentKitId);*/
			pExt->m_latestKitId = currentKitId;

			// Modified the contents of the extension so recompute the crc
			pCreatePacket->RecomputeExtensionCRC();
		}
 	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::WaitingOnLoading()
{
	static u32 timer = 0;
	static u32 count = 0;
	if(WaitingForHDVehicles())
	{
		timer += fwTimer::GetSystemTimeStepInMilliseconds();
		if(timer >= 5000)
		{
			replayDebugf1("Waiting for HD models... %u", count++);
			timer -= 5000;
		}
		return true;
	}
	timer = 0;
	count = 0;

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::IsWaitingOnDamageToBeApplied()
{
	//check to see if have any damage pending that hasn't been applied, if so wait until
	//it has, to avoid the damage appearing on the first frame of the replay
	for (s32 entity = 0; entity < m_pool.GetSize(); entity++)
	{
		CVehicle* pVehicle = m_pool.GetSlot(entity);

		if(pVehicle)
		{
			ReplayVehicleExtension *extension = ReplayVehicleExtension::GetExtension(pVehicle);	
			if(extension && !extension->HasFinishedApplyingDamage())
			{
				return true;
			}
			
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::WaitingForHDVehicles()
{
	for(int i = 0; i < m_HDVehicles.size();)
	{
		CVehicle* pVehicle = m_HDVehicles[i];
		if(pVehicle->GetVehicleLodState() != VLS_HD_NA && pVehicle->GetVehicleLodState() != VLS_HD_AVAILABLE)
		{
			pVehicle->Update_HD_Models();
			++i;
		}
		else
		{
			m_HDVehicles.DeleteFast(i);
		}
	}

	return m_HDVehicles.size() > 0;
}


//////////////////////////////////////////////////////////////////////////
u32	CReplayInterfaceVeh::GetUpdatePacketSize(CVehicle* pVehicle) const
{
	u32 packetSize = 0;
	if(!GetPacketSizeForEntityType((s32)(pVehicle->GetVehicleType()), packetSize))
		return 0;

	m_basePackets += packetSize;

	if (pVehicle)
	{
		if(m_recordWheels == true)
		{
			u32 wheelPacketSize = CPacketVehicleWheelUpdate_Old::EstimatePacketSize(pVehicle);
			packetSize += wheelPacketSize;
			m_noOfWheels += pVehicle->GetNumWheels();
			m_wheelPackets += wheelPacketSize;
		}

		if(m_recordDoors == true)
		{
			u32 doorPacketSize = NumDoorsToRecord(pVehicle) * CPacketVehicleDoorUpdate::EstimatePacketSize();
			packetSize += doorPacketSize;
			m_doorPackets += doorPacketSize;
		}

		if(m_recordFrags || m_recordFragBones)
		{
			u32 fragPacketSize = GetFragDataSize(pVehicle);			
			packetSize += fragPacketSize;
			m_fragPackets += fragPacketSize;
		}
	}

	if(HasCreatePacket(pVehicle->GetReplayID()) == false)
	{
		packetSize += sizeof(CPacketVehicleCreate);
	}

	return packetSize;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::ExtractVehicleType(CVehicle* pVehicle, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller)
{
	const CPacketDescriptor<CVehicle>* pPacketDescriptor = NULL;
	if(FindPacketDescriptorForEntityType(pVehicle->GetVehicleType(), pPacketDescriptor))
	{
		((this)->*(pPacketDescriptor->GetExtractFunc()))(pVehicle, pPrevPacket, pNextPacket, controller);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PreUpdatePacket(ReplayController& controller)
{
	CPacketVehicleUpdate const* pPacket = controller.ReadCurrentPacket<CPacketVehicleUpdate>();
	replayAssertf((pPacket->GetPacketID() >= PACKETID_CARUPDATE && pPacket->GetPacketID() <= PACKETID_SUBUPDATE) || pPacket->GetPacketID() == PACKETID_AMPHAUTOUPDATE || pPacket->GetPacketID() == PACKETID_AMPHQUADUPDATE || pPacket->GetPacketID() == PACKETID_SUBCARUPDATE, "Incorrect Packet");

	CPacketVehicleUpdate const* pNextPacket = NULL;
	if(pPacket->HasNextOffset())
		controller.GetNextPacket(pPacket, pNextPacket);

	m_interper.Init(pPacket, pNextPacket);
	INTERPER_VEH interper(m_interper, controller);

	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP) == false)
	{
		if(PlayUpdateBackwards(controller, pPacket))
			return;
	}

	PreUpdatePacket(pPacket, pNextPacket, controller.GetInterp(), controller.GetPlaybackFlags());
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PreUpdatePacket(const CPacketVehicleUpdate* pPacket, const CPacketVehicleUpdate* pNextPacket, float interpValue, const CReplayState& flags, bool canDefer)
{
	CVehicle* pVehicle = EnsureEntityIsCreated(pPacket->GetReplayID(), flags, !canDefer);
	if(pVehicle)
	{
		m_interper.Init(pPacket, pNextPacket);

		CInterpEventInfo<CPacketVehicleUpdate, CVehicle> info;
		info.SetEntity(pVehicle);
		info.SetNextPacket(pNextPacket);
		info.SetInterp(interpValue);
		info.SetPlaybackFlags(flags);

		pPacket->PreUpdate(info);
	}
	else if(canDefer)
	{
		deferredCreationData& data = m_deferredCreationList.Append();
		data.pPacket		= pPacket;
		data.pNextPacket	= pNextPacket;
		data.interpValue	= interpValue;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::JumpUpdatePacket(ReplayController& controller)
{
	CPacketVehicleUpdate const* pPacket = controller.ReadCurrentPacket<CPacketVehicleUpdate>();
	m_interper.Init(pPacket, NULL);
	INTERPER_VEH interper(m_interper, controller);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist)
{
	CPacketVehicleUpdate const* pPacket = controller.ReadCurrentPacket<CPacketVehicleUpdate>();
	
	CPacketVehicleUpdate const* pNextPacket = NULL;
	if(pPacket->HasNextOffset())
		controller.GetNextPacket(pPacket, pNextPacket);

	m_interper.Init(pPacket, pNextPacket);
	INTERPER_VEH interper(m_interper, controller);

	if(PlayUpdateBackwards(controller, pPacket))
		return;

	CEntityGet<CVehicle> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	CVehicle* pVehicle = entityGet.m_pEntity;

	if(!pVehicle && (entityGet.m_alreadyReported || entityMayNotExist))
	{
		// Entity wasn't created for us to update....but we know this already
		return;
	}

	if(Verifyf(pVehicle, "Failed to find Vehicle to update, but it wasn't reported as 'failed to create' - 0x%08X - Packet %p", (u32)pPacket->GetReplayID(), pPacket))
	{
 		CPacketVehicleUpdate const* pNextVehiclePacket = m_interper.GetNextPacket();

		// Early out
		if (pNextVehiclePacket == NULL)
		{
			return;
		}

		ExtractVehicleType(pVehicle, pPacket, pNextPacket, controller);

		replayAssert(pPacket->GetWheelCount() == pVehicle->GetNumWheels());

		const eHierarchyId doors[] = { VEH_BONNET };
		for(int i = 0; i < sizeof(doors) / sizeof(eHierarchyId); ++i)
		{
			int bonnetBoneIndex = pVehicle->GetBoneIndex(doors[i]);
			if(bonnetBoneIndex > -1)
			{
				pVehicle->GetSkeleton()->PartialReset(bonnetBoneIndex);
			}
		}

		// TRAINS
		if(pPacket->GetPacketID() == PACKETID_TRAINUPDATE)
		{
			CTrain* pTrain = (CTrain*)pVehicle;
			if(!pTrain->GetLinkedToBackward() && !pTrain->GetLinkedToForward())
			{	// If the train isn't linked up yet then we need to process that
				// Add some information so that we can link up the trains later
				CPacketTrainUpdate* pTrainUpdatePacket = (CPacketTrainUpdate*)pPacket;

				if((pTrainUpdatePacket->GetLinkedToForward() != NoEntityID) || (pTrainUpdatePacket->GetLinkedToBackward() != NoEntityID))
				{
					replayAssert(m_trainLinkInfos.Access(pTrainUpdatePacket->GetReplayID()) == NULL);

					trainInfo& info = m_trainLinkInfos[pTrainUpdatePacket->GetReplayID()];
					info.pTrain			= pTrain;
					info.linkageFore	= pTrainUpdatePacket->GetLinkedToForward();
					info.linkageBack	= pTrainUpdatePacket->GetLinkedToBackward();

					//replayDebugf1("Link train info... %p, 0x%08X, 0x%08X, 0x%08X", info.pTrain, info.pTrain->GetReplayID().ToInt(), info.linkageFore.ToInt(), info.linkageBack.ToInt());

					if(pTrain->m_nTrainFlags.bEngine)
					{
						m_trainEngines.PushAndGrow(pTrainUpdatePacket->GetReplayID());
						//replayDebugf1("  is Engine...");
					}
				}
			}
		}


		// FRAGMENTS
		if (m_interper.HasPrevFragData())
		{
			RightHandWheelBones rightWheelBones(pVehicle);
			CPacketBase const* pPrevFragDataPacket = m_interper.GetPrevFragDataPacket();

			bool playerVehicle = false;
			if(pVehicle->GetDriver() && pVehicle->GetDriver()->IsLocalPlayer())
				playerVehicle = true;

			atArray<sVehicleIgnoredBones> ignoredBones;
			if(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->HasDials() && !playerVehicle)
			{
				// Vehicle with dial should use recorded bone data
				// Any other vehicle should have it's bone matrix set to zero
				sVehicleIgnoredBones ignoreBone;
				Matrix34 mat;
				mat.Zero();

				ReplayVehicleExtension* pExt = ReplayVehicleExtension::ProcureExtension(pVehicle);
				if( pExt && m_LastDialVehicle == pVehicle)
				{
					pExt->GetVehicleDialBoneMtx(mat);

					Matrix34 vehicleMat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix());
					mat.Dot(vehicleMat);					
				}
				
				ignoreBone.m_ignoredBoneOverrideMtx = mat;
				ignoreBone.m_boneIdx = pVehicle->GetBoneIndex((eHierarchyId)VEH_DIALS);
				if(ignoreBone.m_boneIdx != (u32)-1)
					ignoredBones.PushAndGrow(ignoreBone); 
			}

			atArray<sVehicleIgnoredBones> zeroScaleIgnoredBones;
			// Disabling for now due to url:bugstar:3891825 and url:bugstar:3892226
			// Was originally put in for url:bugstar:2903491 but I think that is no longer necessary as
			// we're not checking the 'next' frames packet for the window disappearing.
			static bool b = false;	
			if(b)
			{
				for(int i = VEH_FIRST_WINDOW; i <= VEH_LAST_WINDOW; ++i)
				{
					int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)i);
					if(boneIndex != -1)
					{
						Matrix34 mat;
						pVehicle->GetGlobalMtx(boneIndex, mat);

						sVehicleIgnoredBones ignoreBone;
						ignoreBone.m_ignoredBoneOverrideMtx = mat;
						ignoreBone.m_boneIdx = boneIndex;
						zeroScaleIgnoredBones.PushAndGrow(ignoreBone);
					}
				}
			}

			atArray<sInterpIgnoredBone> noInterpBones;
			atArray<sInterpIgnoredBone>* pNoInterpBones = NULL;
			// These are for url:bugstar:3605847
			if(pVehicle->GetModelNameHash() == 0x2189d250/*"apc"*/)
			{
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2A));
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2B));
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2C));
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2D));
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2E));
				noInterpBones.PushAndGrow(pVehicle->GetBoneIndex(VEH_MISSILEFLAP_2F));

				pNoInterpBones = &noInterpBones;
			}

			if(pPrevFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS)
				(static_cast < CPacketFragData_NoDamageBits const * > (pPrevFragDataPacket))->Extract((CEntity*)pVehicle);
			else
				(static_cast < CPacketFragData const * > (pPrevFragDataPacket))->Extract((CEntity*)pVehicle);

			// Has the vehicle had damage repaired  ?
			if((m_interper.GetPrevFragHasDamagedBones() == true) && (m_interper.GetNextFragHasDamagedBones() == false))
			{
				// Use only the previous frame bone data (so we don`t interpolate into repaired state).
				CPacketBase const* pPacket = m_interper.GetPrevFragBonePacket();
				replayAssertf(pPacket, "CReplayInterfaceVeh::PlayUpdatePacket()...Expecting frag bone data.");
				
				if(pPacket->GetPacketID() == CPacketVehicleUpdate::tfragBoneDataType::PacketID)
				{
					CPacketVehicleUpdate::tfragBoneDataType const* pPrevFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType const*)pPacket;
					pPrevFragBonePacket->Extract(pVehicle, NULL, 0.0f, false, &rightWheelBones, &ignoredBones);
				}
				else if(pPacket->GetPacketID() == CPacketVehicleUpdate::tfragBoneDataType_HighQuality::PacketID)
				{
					CPacketVehicleUpdate::tfragBoneDataType_HighQuality const* pPrevFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType_HighQuality const*)pPacket;
					pPrevFragBonePacket->Extract(pVehicle, NULL, 0.0f, false, &rightWheelBones, &ignoredBones);
				}
			}
			else
			{
				// Interpolate between frames as normal.
				CPacketBase const* pPacket = m_interper.GetPrevFragBonePacket();
				CPacketBase const* pNextPacket = m_interper.GetNextFragBonePacket();
				replayAssertf(pPacket && pNextPacket, "CReplayInterfaceVeh::PlayUpdatePacket()...Expecting frag bone data for previous and next frame.");

				if(pPacket->GetPacketID() == CPacketVehicleUpdate::tfragBoneDataType::PacketID)
				{
					CPacketVehicleUpdate::tfragBoneDataType const* pPrevFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType const*)pPacket;
					CPacketVehicleUpdate::tfragBoneDataType const* pNextFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType const*)pNextPacket;
				
					pPrevFragBonePacket->Extract(pVehicle, pNextFragBonePacket, controller.GetInterp(), false, &rightWheelBones, &ignoredBones, &zeroScaleIgnoredBones, pNoInterpBones);
				}
				else if(pPacket->GetPacketID() == CPacketVehicleUpdate::tfragBoneDataType_HighQuality::PacketID)
				{
					CPacketVehicleUpdate::tfragBoneDataType_HighQuality const* pPrevFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType_HighQuality const*)pPacket;
					CPacketVehicleUpdate::tfragBoneDataType_HighQuality const* pNextFragBonePacket = (CPacketVehicleUpdate::tfragBoneDataType_HighQuality const*)pNextPacket;

					pPrevFragBonePacket->Extract(pVehicle, pNextFragBonePacket, controller.GetInterp(), false, &rightWheelBones, &ignoredBones, &zeroScaleIgnoredBones, pNoInterpBones);
				}
			}
		}
		else
		{
			// No Frag Data (and assume no Frag Bones)
			replayAssertf(!m_interper.GetPrevFragBonePacket() && !m_interper.GetNextFragBonePacket(), "CReplayInterfaceVeh::PlayUpdatePacket()...Not expecting bone data.");
		}


		static bool printOut = false;
		if(pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer())
		{
			if(printOut)
				replayDebugf1("Player Vehicle...\n");
		}
		// WHEELS
		if (m_interper.GetWheelCount() > 0)
		{
			if(m_interper.IsWheelDataOld())
			{
				for (s32 wheelIdx = 0; wheelIdx < m_interper.GetWheelCount(); wheelIdx++)
				{
					CWheel* pWheel = pVehicle->GetWheel(wheelIdx);
					replayAssert(pWheel);
					if(pWheel != NULL)
					{
						CWheelFullData prevWheel;
						CWheelFullData nextWheel;
						ASSERT_ONLY(bool gotPrevWheel = )m_interper.GetPrevWheelDataOld(wheelIdx, prevWheel);
						bool gotNextWheel = m_interper.GetNextWheelDataOld(wheelIdx, nextWheel);
						replayAssertf(gotPrevWheel, "CReplayInterfaceVeh::PreUpdatePacket()...Expected wheel data.");
						prevWheel.Extract(pVehicle, pWheel, gotNextWheel ? &nextWheel : NULL, controller.GetInterp());

						if(pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer() && printOut)
						{
							prevWheel.Print();
						}
					}
				}
			}
			else
			{
				CPacketWheel *pPrevious = m_interper.GetPrevWheelPacket();
				pPrevious->Extract(controller, pVehicle, m_interper.GetPrevWheelPacket(), controller.GetInterp());
			}
		}


		// DOORS
		if (m_interper.GetPrevDoorCount() != 0 || m_interper.GetNextDoorCount() != 0)
		{
			for(s32 prevDoorIdx = 0; prevDoorIdx < m_interper.GetPrevDoorCount(); prevDoorIdx++)
			{
				CPacketVehicleDoorUpdate const* pPrevDoorPacket = m_interper.GetPrevDoorPacket(prevDoorIdx);
				CPacketVehicleDoorUpdate const* pNextDoorPacket = NULL;

				for(s32 nextDoorIdx = 0; nextDoorIdx < m_interper.GetNextDoorCount(); nextDoorIdx++)
				{
					pNextDoorPacket = m_interper.GetNextDoorPacket(nextDoorIdx);
					if (pPrevDoorPacket->GetDoorIdx() == pNextDoorPacket->GetDoorIdx())
					{
						break;
					}
					pNextDoorPacket = NULL;
				}

				if (pNextDoorPacket != NULL)
				{
					replayAssert(pPrevDoorPacket->GetDoorIdx() == pNextDoorPacket->GetDoorIdx());
				}

				pPrevDoorPacket->Extract(pVehicle, pNextDoorPacket, controller.GetInterp());
			}
		}

		pPacket->FixupVehicleMods(pVehicle, pNextVehiclePacket, controller.GetInterp(), controller.GetPlaybackFlags());

		//Damage
		if( !controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) || controller.GetPlaybackFlags().IsSet(REPLAY_STATE_PAUSE))
		{
			ReplayVehicleExtension *extension = ReplayVehicleExtension::GetExtension(pVehicle);	
			if(extension)
			{
				extension->ApplyVehicleDeformation(pVehicle);
			}
		}

		
	} // if (pVehicle)
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::RemoveElement(CVehicle* pVehicle, const CPacketVehicleDelete* pDeletePacket, bool isRewinding)
{
	//remove any peds we have in the vehicle, they will be cleared by the ped clearing code
	for (s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
	{
		CPed* pPedInVehicle = pVehicle->GetSeatManager()->GetPedInSeat(i);
		if(pPedInVehicle)
		{
			pPedInVehicle->SetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle, false);
			pVehicle->RemovePedFromSeat(pPedInVehicle);
			pPedInVehicle->SetMyVehicle(NULL);
			pPedInVehicle->UpdateSpatialArrayTypeFlags();
		}
	}

	for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

		// May need to do something similar for other gadgets?
		if(pVehicleGadget && pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
		{
			CVehicleGadgetTowArm* pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
			if(pTowArm->GetPropObject() && pTowArm->GetPropObject()->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
			{
				pTowArm->DetachVehicle(pVehicle, false, true);
				if(pTowArm->IsRopeAndHookInitialised())
					pTowArm->DeleteRopesHookAndConstraints(pVehicle);
				pTowArm->SetPropObject(NULL);	// Prevent the gadget from 'deleting' the replay owned object...replay will handle that
			}
		}
	}

	if(m_LastDialVehicle == pVehicle)
	{
		m_LastDialVehicle = NULL;
	}

	// Remove the vehicle from this array if it happens to be there.
	m_HDVehicles.DeleteMatches(pVehicle);

	bool baseResult = CReplayInterface::RemoveElement(pVehicle, pDeletePacket, isRewinding);

	CVehicleFactory::GetFactory()->Destroy(pVehicle);

	return baseResult & true;
}

//////////////////////////////////////////////////////////////////////////
int CReplayInterfaceVeh::RecordWheels(CVehicle* pVehicle, CReplayRecorder& recorder, u16 sessionBlockIndex)
{
	replayAssertf(pVehicle, "CReplayMgr::Record: Invalid vehicle");

	int wheelsRecorded = pVehicle->GetNumWheels();
	m_noOfWheels += wheelsRecorded;

#if REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET
	if(wheelsRecorded != 0)
	{
		++m_wheelPackets;
	#if __BANK
		if(IsRecordingOldStyleWheels() == true)
			recorder.RecordPacketWithParam<CPacketVehicleWheelUpdate_Old>(pVehicle);
		else
	#endif // __BANK
			recorder.RecordPacketWithParams<CPacketWheel>(pVehicle, sessionBlockIndex);
	}
#else // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

	for (s32 wheelIdx = 0; wheelIdx < pVehicle->GetNumWheels(); wheelIdx++)
	{
		CWheel* pWheel = pVehicle->GetWheel(wheelIdx);

		if (pWheel)
		{
			recorder.RecordPacketWithParam<CPacketVehicleWheelUpdate_Old>(pWheel);
			++m_wheelPackets;
			++wheelsRecorded;
		}
	}
#endif // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

	return wheelsRecorded;
}

//////////////////////////////////////////////////////////////////////////
int CReplayInterfaceVeh::RecordDoors(CVehicle* pVehicle, CReplayRecorder& recorder)
{
	replayAssert(pVehicle && "CReplayMgr::Record: Invalid vehicle");

	if(!pVehicle->GetSkeleton())
		return 0;

	int numDoorPackets = 0;
	// Doors
	for (s32 doorIdx = 0; doorIdx < pVehicle->GetNumDoors(); doorIdx++)
	{
		if (ShouldRecordDoor(pVehicle, doorIdx))
		{
			recorder.RecordPacketWithParams<CPacketVehicleDoorUpdate>(pVehicle, doorIdx);
			++m_doorPackets;
			++numDoorPackets;
		}
	}

	return numDoorPackets;
}


//////////////////////////////////////////////////////////////////////////
int CReplayInterfaceVeh::RecordFragData(CEntity* pEntity, CReplayRecorder& recorder)
{
	replayAssert(pEntity && "CReplayMgr::RecordFragData: Invalid entity");

	int returnVal = 0;
	rage::fragInst* pFragInst = pEntity->GetFragInst();
	if (pFragInst && pFragInst->GetType())
	{
		recorder.RecordPacketWithParam<CPacketFragData_NoDamageBits>(pEntity);
		++m_fragPackets;
		++returnVal;

		if(m_recordFragBones)
			returnVal += RecordFragBoneData(pEntity, recorder);
	}

	return returnVal;
}


//////////////////////////////////////////////////////////////////////////
float g_VehTranslationEpsilon = 0.035f;
float g_VehQuaternionEpsilon = 0.0125f;


int CReplayInterfaceVeh::RecordFragBoneData(CEntity* pEntity, CReplayRecorder& recorder)
{
	replayAssert(pEntity && "CReplayMgr::RecordFragBoneData: Invalid entity");

	int returnVal = 0;
	rage::fragInst* pFragInst = pEntity->GetFragInst();
	rage::fragCacheEntry* pFragCacheEntry = pEntity->GetFragInst()->GetCacheEntry();
	if (pFragInst && pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
	{
		float quatEpsilon = g_VehQuaternionEpsilon;
		float transEpsilon = g_VehTranslationEpsilon;

		CVehicle *pVeh = reinterpret_cast < CVehicle * > (pEntity);

		bool vehicleHasTurret = false;
		if(pVeh->GetVehicleWeaponMgr())
		{
			vehicleHasTurret = pVeh->GetVehicleWeaponMgr()->GetNumTurrets() != 0;
		}

		// B*2245503 - Use tighter epsilons when recording bikes the player is on (in case they are viewed in FP).
		if(pVeh->GetModelNameHash() == 0x28ad20e1/*"boxville5"*/ ||
			pVeh->GetModelNameHash() == 0x36b4a8a9/*"xa21"*/ || 
			(pVeh->IsDriverAPlayer() && pVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE))
		{
			quatEpsilon *= 0.125f;
			transEpsilon *= 0.125f;
		}

		// url:bugstar:4507398 - turret jitter problems...lets just get rid of the tolerance
		if(vehicleHasTurret)
		{
			transEpsilon = 0.0f;
			quatEpsilon = 0.0f;
		}

		// If the vehicle is a car where the suspension has been modified,
		// don't have a translation tolerance as we'd miss it on some vehicles
		if(pVeh->GetVehicleType() == VEHICLE_TYPE_CAR)
		{
			CAutomobile* pCar = (CAutomobile*)pVeh;
			if(pCar->GetFakeSuspensionLoweringAmount() != 0.0f)
			{
				transEpsilon = 0.0f;
				quatEpsilon = 0.0f;
			}
		}

		recorder.RecordPacketWithParams<CPacketVehicleUpdate::tfragBoneDataType_HighQuality>(pEntity, transEpsilon, quatEpsilon);
		++returnVal;
	}

	return returnVal;
}

//////////////////////////////////////////////////////////////////////////
s32 CReplayInterfaceVeh::NumDoorsToRecord(const CVehicle* pVehicle)
{
	s32 doorCounter = 0;
	for (s32 doorIdx = 0; doorIdx < pVehicle->GetNumDoors(); doorIdx++)
	{
		if(pVehicle->GetSkeleton())
		{
			if (ShouldRecordDoor(pVehicle, doorIdx))
			{				
				doorCounter++;			
			}
		}
	}
	return doorCounter;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceVeh::ShouldRecordDoor(const CVehicle* pVehicle, s32 doorIdx)
{
	const CCarDoor* door = pVehicle->GetDoor(doorIdx);

	if(door)
	{
		if(!door->GetFlag(CCarDoor::IS_BROKEN_OFF) && (!door->GetFlag(CCarDoor::DRIVEN_SHUT) || door->GetDoorRatio() != 0 || door->GetOldAudioRatio() != 0 || door->GetOldRatio() != 0))
		{
			int nDoorBoneIndex = pVehicle->GetBoneIndex(door->GetHierarchyId());

			if(nDoorBoneIndex > -1)
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
s32 CReplayInterfaceVeh::GetFragDataSize(const CEntity* pEntity) const
{
	replayAssert(pEntity && "CReplayInterfaceVeh::GetFragDataSize: Invalid entity");

	s32 dataSize = 0;

	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		if (pFragInst->GetType() && m_recordFrags == true)
		{
			dataSize += sizeof(CPacketFragData);
		}

		if(m_recordFragBones == true)
		{
			dataSize += CPacketVehicleUpdate::tfragBoneDataType_HighQuality::EstimatePacketSize(pEntity);
		}
	}

	return dataSize;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::ApplyUpdateFadeSpecial(CEntityData& entityData, const CPacketVehicleUpdate& packet)
{
	if (!packet.IsScorched())
	{
		if (entityData.m_iScorchedCount == 0)
		{
			entityData.m_apvPacketScorched[entityData.m_iScorchedCount] = (void*)&packet;
			entityData.m_iScorchedCount++;
		}
	}
	else
	{
		if (entityData.m_iScorchedCount != 0 && entityData.m_iScorchedCount < CEntityData::NUM_FRAMES_BEFORE_SCORCHING)
		{
			entityData.m_apvPacketScorched[entityData.m_iScorchedCount] = (void*)&packet;
			entityData.m_iScorchedCount++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceVeh::Process()
{
	static bool bEnableHdVehicleNearCamera = true;
	static bool bEnableDialsForReplay = true;

	const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();

	if(renderedCamera && (bEnableDialsForReplay || bEnableHdVehicleNearCamera))
	{
		CPed* ped = CReplayMgr::GetMainPlayerPtr();

		// Get the max distance the free camera can be moved away from the player
		float maxDistanceFromPlayer = 30.0f;
		bool bfreeCameraActive = camInterface::GetReplayDirector().IsFreeCamActive();
		if(ped && bfreeCameraActive)
		{
			maxDistanceFromPlayer = camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer();
		}
		// Add an small offset to help streaming
		maxDistanceFromPlayer += 10.0f;

		const Vector3& camPos = renderedCamera->GetFrame().GetPosition();
		float closest = FLT_MAX;
		CVehicle* pClosestVeh = NULL;
		for (s32 entity = 0; entity < m_pool.GetSize(); entity++)
		{
			CVehicle* pVehicle = m_pool.GetSlot(entity); 
			if(pVehicle)
			{
				float dist = (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - camPos).Mag2();
				if(dist < closest)
				{
					closest = dist;
					pClosestVeh = pVehicle;
				}

				// Vehicle HD - export only
				if(bEnableHdVehicleNearCamera && CReplayCoordinator::IsExportingToVideoFile())
				{
					// Force HD for any vehicle within range of the camera
					if(dist < (maxDistanceFromPlayer * maxDistanceFromPlayer))
					{
						pVehicle->SetForceHd(true);
					}
					else
					{
						// Only unset the flag if it's not the players vehicle and the recorded data wasn't set
						if(pVehicle->GetDriver() != ped)
						{
							ReplayVehicleExtension* pExt = ReplayVehicleExtension::GetExtension(pVehicle);

							if(pExt && !pExt->GetForceHD())
							{
								pVehicle->SetForceHd(false);
							}
						}
					}
				}
			}
		}

		// Vehicle Dial - only one is allow in the world at a time.=
		if(bEnableDialsForReplay && (bfreeCameraActive || camInterface::IsDominantRenderedCameraAnyFirstPersonCamera()))
		{
			RequestDial(pClosestVeh);
		}
		else
		{
			// Request the player's vehicle to have dials
			CPed* pPed = CGameWorld::FindLocalPlayer();
			if(pPed && (pPed->GetIsInVehicle() || pPed->GetReplayEnteringVehicle()))
			{
				RequestDial(pPed->GetMyVehicle());
			}
		}
	}
}

void CReplayInterfaceVeh::RequestDial(CVehicle* pVehicle)
{
	if(pVehicle)
	{
		if(m_LastDialVehicle != pVehicle)
		{
			if(m_LastDialVehicle)
			{
				m_LastDialVehicle->GetVehicleModelInfo()->ReleaseDials();
				m_LastDialVehicle = NULL;
				return;
			}
			m_LastDialVehicle = pVehicle;
		}

		if(m_LastDialVehicle)
		{
			m_LastDialVehicle->RequestDials(true);
		}
	}
}

#endif // GTA_REPLAY
