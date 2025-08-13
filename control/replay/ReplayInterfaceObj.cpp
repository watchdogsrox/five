#include "ReplayInterfaceObj.h"

#if GTA_REPLAY

#include "replay.h"
#include "ReplayInterfaceImpl.h"
#include "Streaming/streaming.h"
#include "scene/world/GameWorld.h"

#include "animation/MoveObject.h"
#include "control/trafficlights.h"
#include "Control/Replay/Entity/FragmentPacket.h"
#include "control/replay/effects/ParticleMiscFxPacket.h"
#include "fwscene/stores/maptypesstore.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "objects/Object.h"
#include "objects/ObjectIntelligence.h"
#include "objects/ProcObjects.h"
#include "peds/ped.h"
#include "scene/animatedbuilding.h"
#include "Task/system/TaskTypes.h"
#include "weapons/WeaponFactory.h"
#include "Weapons/components/WeaponComponentFactory.h"
#include "Weapons/components/WeaponComponentManager.h"
#include "vfx/misc/LODLightManager.h"

const char*	CReplayInterfaceTraits<CObject>::strShort = "OBJ";
const char*	CReplayInterfaceTraits<CObject>::strLong = "OBJECT";
const char*	CReplayInterfaceTraits<CObject>::strShortFriendly = "Obj";
const char*	CReplayInterfaceTraits<CObject>::strLongFriendly = "ReplayInterface-Object";

const int	CReplayInterfaceTraits<CObject>::maxDeletionSize = MAX_OBJECT_DELETION_SIZE;

const u32 OBJ_RECORD_BUFFER_SIZE		= 1*1024*1024;	// There are a lot of Objects!
const u32 OBJ_CREATE_BUFFER_SIZE		= 256*1024;
const u32 OBJ_FULLCREATE_BUFFER_SIZE	= 256*1024;

#define OBJ_DESTROYED_OBJECTS_INITIAL_SIZE	30
#define OBJ_BRIEF_LIFETIME_OBJECTS_INITIAL_SIZE	30

//////////////////////////////////////////////////////////////////////////
CReplayInterfaceObject::CReplayInterfaceObject()
	: CReplayInterface<CObject>(*CObject::GetPool(), OBJ_RECORD_BUFFER_SIZE, OBJ_CREATE_BUFFER_SIZE, OBJ_FULLCREATE_BUFFER_SIZE)
{
	m_relevantPacketMin = PACKETID_OBJECT_MIN;
	m_relevantPacketMax	= PACKETID_OBJECT_MAX;

	m_createPacketID	= PACKETID_OBJECTCREATE;
	m_updatePacketID	= PACKETID_OBJECTUPDATE;
	m_updateDoorPacketID = PACKETID_OBJECTDOORUPDATE;
	m_deletePacketID	= PACKETID_OBJECTDELETE;
	m_packetGroup		= REPLAY_PACKET_GROUP_OBJECT;

	m_enableSizing		= true;
	m_enableRecording	= true;
	m_enablePlayback	= true;
	m_enabled			= true;

	m_mapObjectsToHide.Reserve((u32)CObject::GetPool()->GetSize());

	// Set up the relevant packets for this interface
	AddPacketDescriptor<CPacketObjectCreate>(		PACKETID_OBJECTCREATE, 		STRINGIFY(CPacketObjectCreate));
	AddPacketDescriptor<CPacketObjectUpdate>(		PACKETID_OBJECTUPDATE, 		STRINGIFY(CPacketObjectUpdate));
	AddPacketDescriptor<CPacketObjectDelete>(		PACKETID_OBJECTDELETE, 		STRINGIFY(CPacketObjectDelete));
	AddPacketDescriptor<CPacketFragData>(			PACKETID_FRAGDATA,			STRINGIFY(CPacketFragData));
	AddPacketDescriptor<CPacketFragData_NoDamageBits>(PACKETID_FRAGDATA_NO_DAMAGE_BITS, STRINGIFY(CPacketFragData_NoDamageBits));
	AddPacketDescriptor<CPacketFragBoneData>(		PACKETID_FRAGBONEDATA,		STRINGIFY(CPacketFragBoneData));
	AddPacketDescriptor<CPacketDoorUpdate, CDoor>(	PACKETID_OBJECTDOORUPDATE,	STRINGIFY(CPacketDoorUpdate), 1);
	AddPacketDescriptor<CPacketAnimatedObjBoneData>(PACKETID_ANIMATEDOBJDATA,	STRINGIFY(CPacketAnimatedObjBoneData));
	AddPacketDescriptor<CPacketHiddenMapObjects>(	PACKETID_OBJECTHIDDENMAP,	STRINGIFY(CPacketHiddenMapObjects));
	AddPacketDescriptor<CPacketDestroyedMapObjectsForClipStart>(PACKETID_DESTROYED_MAP_OBJECTS_FOR_CLIP_START,	STRINGIFY(CPacketDestroyedMapObjectsForClipStart));
	AddPacketDescriptor<CPacketBriefLifeTimeDestroyedMapObjects>(PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS,	STRINGIFY(CPacketBriefLifeTimeDestroyedMapObjects));

	m_DestroyedMapObjects.Init(OBJ_DESTROYED_OBJECTS_INITIAL_SIZE, OBJ_BRIEF_LIFETIME_OBJECTS_INITIAL_SIZE);
	m_objectToAddStamp = 0;
}

//////////////////////////////////////////////////////////////////////////
template<typename PACKETTYPE, typename OBJECTSUBTYPE>
void CReplayInterfaceObject::AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 objectType)
{
	REPLAY_CHECK(m_numPacketDescriptors < maxPacketDescriptorsPerInterface, NO_RETURN_VALUE, "Max number of packet descriptors breached");
	m_packetDescriptors[m_numPacketDescriptors++] = CPacketDescriptor<CObject>(packetID, 
		objectType, 
		sizeof(PACKETTYPE), 
		packetName,
		&iReplayInterface::template PrintOutPacketDetails<PACKETTYPE>,
		&CReplayInterface<CObject>::template RecordElementInternal<PACKETTYPE, OBJECTSUBTYPE>,
		&CReplayInterface<CObject>::template ExtractPacket<PACKETTYPE, OBJECTSUBTYPE>);
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ClearWorldOfEntities()
{
	// Clear all Replay owned Objects
	CReplayInterface<CObject>::ClearWorldOfEntities();


	const Vector3 VecCoors(0.0f, 0.0f, 0.0f);
	const float Radius = 1000000.0f;
	CGameWorld::ClearObjectsFromArea(VecCoors, Radius, true, NULL, true, true);

	m_mapObjectsMap.Reset();
	m_mapObjectsToRecord.clear();
	m_mapObjectsToHide.clear();
	m_deferredMapObjectToHide.clear();
	m_mapObjectHideInfos.Reset();
	m_objectToAddStamp = 0;
	m_fragChildrenUnlinked.Reset();
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PreRecordFrame(CReplayRecorder& recorder, CReplayRecorder& storedOnBlockChangeRecorder)
{
	if(m_mapObjectsToHide.GetCount() > 0)
	{
		recorder.RecordPacketWithParam<CPacketHiddenMapObjects, const atArray<mapObjectID>&>(m_mapObjectsToHide);
	}
	if(m_DestroyedMapObjects.GetNoOfBriefLifeTimeMapObjects())
	{
		recorder.RecordPacketWithParam<CPacketBriefLifeTimeDestroyedMapObjects, atArray <DestroyedMapObjectsManager::DestuctionRecord> & >(m_DestroyedMapObjects.GetDestructionRecordsForBriefLifeTimeMapObjects());
		m_DestroyedMapObjects.ResetBriefLifeTimeMapObjects();
	}
	storedOnBlockChangeRecorder.RecordPacketWithParam<CPacketDestroyedMapObjectsForClipStart, atArray <DestroyedMapObjectsManager::DestuctionRecord> &>(m_DestroyedMapObjects.GetDestructionRecordsForClipStart());
}


//////////////////////////////////////////////////////////////////////////
// Record an Object - First do the common recording and then record the 
// fragment data
bool CReplayInterfaceObject::RecordElement(CObject* pObject, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex)
{
	replayAssert(ShouldRecordElement(pObject));

	bool recordSuccess = false;
	if(pObject->IsADoor())
	{
		// Find the correct packet descriptor for the object type and call the
		// record function
		const CPacketDescriptor<CObject>* pPacketDescriptor = NULL;
		if(FindPacketDescriptorForEntityType(1, pPacketDescriptor))
		{
			CPacketDescriptor<CObject>::tRecFunc recFunc = pPacketDescriptor->GetRecFunc();
			recordSuccess = (this->*recFunc)(pObject, recorder, createPacketRecorder, fullCreatePacketRecorder, sessionBlockIndex);
		}
	}
	else
	{
		recordSuccess = CReplayInterface<CObject>::RecordElementInternal<CPacketObjectUpdate, CObject>(pObject, recorder, createPacketRecorder, fullCreatePacketRecorder, sessionBlockIndex);
	}
	
	if(recordSuccess)
	{
		RecordFragData(pObject, recorder);
		RecordBoneData(pObject, recorder);
	}

	return recordSuccess;
}


#if REPLAY_DELAYS_QUANTIZATION
//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PostRecordOptimize(CPacketBase* pPacket) const
{
#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	if (pPacket->GetPacketID() == PACKETID_ANIMATEDOBJDATA)
	{
		CPacketAnimatedObjBoneData* pAnimData = static_cast<CPacketAnimatedObjBoneData*>(pPacket);

		CSkeletonBoneData<CPacketQuaternionL>* pSkeletonBoneData = pAnimData->GetBoneData();
		if (pSkeletonBoneData)
			pSkeletonBoneData->QuantizeQuaternions();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ResetPostRecordOptimizations() const
{
#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	CSkeletonBoneData<CPacketQuaternionL>::ResetDelayedQuaternions();
#endif
}
#endif // REPLAY_DELAYS_QUANTIZATION


//////////////////////////////////////////////////////////////////////////
CObject* CReplayInterfaceObject::CreateElement(CReplayInterfaceTraits<CObject>::tCreatePacket const* pPacket, const CReplayState& /*state*/)
{
	rage::strModelRequest streamingModelReq;
	rage::strRequest streamingReq;
	if(!m_modelManager.LoadModel(pPacket->GetModelNameHash(), pPacket->GetMapTypeDefIndex(), !pPacket->UseMapTypeDefHash(), true, streamingModelReq, streamingReq))
	{
		HandleFailedToLoadModelError(pPacket);
		return NULL;
	}

	fwArchetype* pArchtype = fwArchetypeManager::GetArchetypeFromHashKey(pPacket->GetModelNameHash(), NULL);
	replayAssert(pArchtype);
	fwModelId modelID = fwArchetypeManager::LookupModelId(pArchtype);

	if(modelID.IsValid() == false)
	{
		replayAssertf(false, "Can't create CObject because the model ID is invalid... 0x%08X", pPacket->GetReplayID().ToInt());
		return NULL;
	}

	bool bCanCreate = true;

	CEntity* pParent = NULL;
	CObject *pObject = NULL;

	if (pPacket->HasFragParent())
	{
		//have to get from the manager as this can be something that isn't a object
		CEntityGet<CEntity> eg(pPacket->GetFragParentID());
		CReplayMgrInternal::GetEntityAsEntity(eg);

		pParent = eg.m_pEntity;
		bCanCreate = (pParent != NULL);
		if(!bCanCreate)
		{
			bCanCreate = eg.m_recentlyUnlinked;
			if(bCanCreate)
			{
				replayDebugf3("Parent was recently unlinked...so allow the creation");
			}
			else
			{	// Parent not created yet...
				m_fragChildrenUnlinked[pPacket->GetReplayID()] = pPacket->GetFragParentID();
				bCanCreate = true;
			}
		}
	}

	//If this is a weapon component, check that the parent has been created. If not defer the creation.
	if(pPacket->IsWeaponComponent() && pPacket->HasWeaponParent())
	{
		CObject* pWeaponObject = static_cast<CObject*>(CReplayMgrInternal::GetEntityAsEntity(pPacket->GetWeaponParentID()));
		if(!pWeaponObject)
		{
			bCanCreate = false;
			replayDebugf2("Failed to create Object as its weapon parent hasn't been created first, Object 0x%08X, Parent 0x%08X...Parent should be created and we'll try again later...", pPacket->GetReplayID().ToInt(), pPacket->GetWeaponParentID());
		}
	}

	if(!bCanCreate)
	{
		replayDebugf2("Failed to create Object as its parent hasn't been created...see log");
	}
	else
	{
		CObjectPopulation::CreateObjectInput params(modelID, ENTITY_OWNEDBY_REPLAY, false);
		params.m_bInitPhys = false;
		params.m_bClone = true;
		params.m_iPoolIndex = -1;
		params.m_bCalcAmbientScales = false;
		params.m_bForceObjectType = !pPacket->GetIsADoor();

		pObject = CObjectPopulation::CreateObject(params);

		if (pObject)
		{
			pObject->SetStatus(STATUS_PLAYER);//todo4five this might need special replay status the old one was		// Don't do any updating stuff
			pObject->DisableCollision();

			Matrix34 creationMatrix;
			pPacket->LoadMatrix(creationMatrix);
			pObject->SetMatrix(creationMatrix);
			// Create the physics now we have a valid initial position (required for environment cloth).
			// NOTE:- Some objects already have their physics created and added to the world (See CObject::CreateDrawable()).
			pObject->InitPhys();
			pObject->CreateDrawable();

			if(pPacket->GetPacketVersion() >= CPacketObjectCreate_V1)
				pObject->m_nObjectFlags.bCarWheel = pPacket->GetIsAWheel();

			if(pPacket->GetPacketVersion() >= CPacketObjectCreate_V2)
				pObject->SetIsParachute(pPacket->GetIsAParachute());

			if (pParent)
			{
				SetFragParent(pObject, pParent);
			}
			
			if (pPacket->IsPropOverride())
			{
				
				pObject->uPropData.m_parentModelIdx = pPacket->GetParentModelID();
				pObject->uPropData.m_propIdxHash = pPacket->GetPropHash();
				pObject->uPropData.m_texIdxHash = pPacket->GetTexHash();

				if(pParent && pParent->GetIsTypePed())
				{
					pObject->m_nObjectFlags.bDetachedPedProp = true;

					CPed* pPed = static_cast<CPed*>(pParent);

					strLocalIndex	DwdIdx = strLocalIndex(pPed->GetPedModelInfo()->GetPropsFileIndex());
					if (DwdIdx != -1) 
					{
						// pack ped
						g_DwdStore.AddRef(DwdIdx, REF_OTHER);
						g_TxdStore.AddRef(g_DwdStore.GetParentTxdForSlot(DwdIdx), REF_OTHER);
					}
					else 
					{
						// streamed ped
						if(pPacket->GetPropHash() > 0)
						{
							strLocalIndex dwdIdx = strLocalIndex(g_DwdStore.FindSlot(pPacket->GetPropHash()));
							replayAssertf(dwdIdx != STRLOCALINDEX_INVALID, "Invalid Prop hash %u for object 0x%08X in packet %p", pPacket->GetPropHash(), pPacket->GetReplayID().ToInt(), this);
							if(dwdIdx != STRLOCALINDEX_INVALID)
							{
								rage::strRequest dwdStreamingReq;
								m_modelManager.LoadAsset(dwdIdx, g_DwdStore.GetStreamingModuleId(), true, dwdStreamingReq);
								g_DwdStore.AddRef(dwdIdx, REF_OTHER);
							}
						}

						if(pPacket->GetTexHash() > 0)
						{
							strLocalIndex txdIdx = strLocalIndex(g_TxdStore.FindSlot(pPacket->GetTexHash()));
							replayAssertf(txdIdx != STRLOCALINDEX_INVALID, "Invalid Tex hash %u for object 0x%08X in packet %p", pPacket->GetTexHash(), pPacket->GetReplayID().ToInt(), this);
							if (txdIdx != STRLOCALINDEX_INVALID)
							{
								rage::strRequest txdStreamingReq;
								m_modelManager.LoadAsset(txdIdx, g_TxdStore.GetStreamingModuleId(), true, txdStreamingReq);
								g_TxdStore.AddRef(txdIdx, REF_OTHER);
							}
						}
					}
				}
			}

			// If the object is a weapon then create it as such...
			if(pPacket->IsWeapon())
			{
				CWeapon* pWeapon = WeaponFactory::Create(pPacket->GetWeaponHash(), CWeapon::INFINITE_AMMO, pObject);
				if(replayVerify(pWeapon))
				{
					pObject->SetWeapon(pWeapon);

					pWeapon->UpdateShaderVariables(pPacket->GetWeaponTint());
				}	
			}

			// If the object is a weapon component then create it with the correct info and add it to the weapon
			if(pPacket->IsWeaponComponent() && pPacket->HasWeaponParent())
			{
				CObject* pWeaponObject = static_cast<CObject*>(CReplayMgrInternal::GetEntityAsEntity(pPacket->GetWeaponParentID()));
				if(pWeaponObject)
				{
					CWeapon* pWeapon = pWeaponObject->GetWeapon();
					if(pWeapon)
					{
						const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(pPacket->GetWeaponComponentHash());
						if(pComponentInfo)
						{
							CWeaponComponent* pComponent = WeaponComponentFactory::Create(pComponentInfo, pWeapon, pObject);
							if(pComponent)
							{
								pWeapon->AddComponent(pComponent, false);
								pComponent->UpdateShaderVariables(pPacket->GetWeaponComponentTint());
							}
						}
					}
				}
			}

			if(pObject->GetDrawHandlerPtr())
			{
				pObject->SetTintIndex(pPacket->GetTintIndex());
				fwCustomShaderEffect* pShader = pObject->GetDrawHandlerPtr()->GetShaderEffect();
				if( pShader )
				{
					if (pShader->GetType() == CSE_TINT)
					{
						CCustomShaderEffectTint *pCSETint = static_cast<CCustomShaderEffectTint *>(pShader);
						pCSETint->SelectTintPalette((u8)pPacket->GetTintIndex(), pObject);
					}
					else if(pShader->GetType() == CSE_PROP)
					{
						CCustomShaderEffectProp* pCustomShaderEffectProp = static_cast<CCustomShaderEffectProp *>(pShader);
						if(pPacket->GetTintIndex() < pCustomShaderEffectProp->GetMaxTintPalette())
						{
							pCustomShaderEffectProp->SelectTintPalette((u8)pPacket->GetTintIndex(), pObject);
						}
					}
				}
			}

			AddReplayEntityToWorld(pObject, pPacket->GetInteriorLocation());

			//pPacket->Load(pObject, pNextObjectPacket, 0.0f);

			// TODO StartObjectSounds( pObject );

			// Set the object up with replay related info
			PostCreateElement(pPacket, pObject, pPacket->GetReplayID());

			// Add traffic light extension if necessary
 			if(CTrafficLights::IsMITrafficLight(pObject->GetBaseModelInfo()))
 			{
				ReplayTrafficLightExtension* pExt = ReplayTrafficLightExtension::Add(pObject);
				pExt->SetTrafficLightCommands(pPacket->GetTrafficLightCommands());
 			}

			//Don't fade in objects
			pObject->GetLodData().SetResetDisabled(true);

			// If the object is an ITYP then add an object extension.
			// This will add a reference to the ITYP file and when the extension
			// is removed (on object deletion) the reference count will be
			// also removed.
			ReplayObjectExtension* pExtension = ReplayObjectExtension::Add(pObject);
			if(streamingModelReq.IsValid())
			{
				pExtension->SetStrModelRequest(streamingModelReq);
			}

			// Door start rotation
			if(pObject->IsADoor())
			{
				Quaternion quat = pPacket->GetDoorStartRot();
				// Identity means it's not been recorded so not override the start rot.
				if(!quat.IsEqual(Quaternion(Quaternion::IdentityType)))
				{
					(static_cast<CDoor*>(pObject))->SetDoorStartRot(quat);
				}
			}

			if((pPacket->GetMapHash() != REPLAY_INVALID_OBJECT_HASH))
			{
				{
					FrameRef frame;
					pObject->GetCreationFrameRef(frame);

					for(int i = 0; i < m_pool.GetSize(); ++i)
					{
						CObject* pMapObject = m_pool.GetSlot(i);
						if(pMapObject && pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetHash() == pPacket->GetMapHash())
						{
							pMapObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY,false,false);
							CLODLightManager::RegisterBrokenLightsForEntity(pMapObject);
							pMapObject->RemovePhysics();
							pMapObject->DeleteInst();
							if(pMapObject->GetAnimDirector())
							{
								pMapObject->DeleteAnimDirector();
							}
							pObject->ReplaySetMapObject(pMapObject);

							SetHideRecordForMapObject(pMapObject->GetRelatedDummy()->GetHash(), pPacket->GetReplayID(), frame);

							break;
						}
					}

					AddDeferredMapObjectToHide(mapObjectID(pPacket->GetMapHash()), pPacket->GetReplayID(), frame);
				}

				pExtension->SetMapHash(pPacket->GetMapHash());
			}

			// HACK - B*2240576 - Trevors TV displays static over the tv show
			// AC: Added the sign from url:bugstar:3984755
			else
			{
				u32 specificHashesToCheck[] = { ATSTRINGHASH("prop_tt_screenstatic",0xe2e039bc),
												ATSTRINGHASH("xm_prop_x17dlc_rep_sign_01a",0x1c64f828)};

				u32 hideGameObject = 0;
				CBaseModelInfo *pModelInfo = pObject->GetBaseModelInfo();
				for(int i = 0; i < sizeof(specificHashesToCheck) /sizeof(u32); ++i)
				{
					if(pModelInfo->GetModelNameHash() == specificHashesToCheck[i])
					{
						hideGameObject = specificHashesToCheck[i];
						break;
					}
				}

				if(hideGameObject != 0)
				{
					for(int i = 0; i < m_pool.GetSize(); ++i)
					{
						CObject* pOtherObject = m_pool.GetSlot(i);
						if( pOtherObject && pOtherObject != pObject )
						{
							CBaseModelInfo *pOtherModelInfo = pOtherObject->GetBaseModelInfo();
							if( pOtherModelInfo && pOtherModelInfo->GetModelNameHash() == hideGameObject )
							{
								pOtherObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY,false,false);
							}
						}
					}
				}
			}
			// HACK

		}
		else
		{
			replayAssertf(false, "CObjectPopulation::CreateObject returned a NULL object, %u, IsADoor:%s", modelID.ConvertToU32(), pPacket->GetIsADoor() ? "Yes" : "No");
		}
	}

	return pObject;
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfaceObject::AddReplayEntityToWorld(CEntity *pEntity, fwInteriorLocation interiorLocation)
{
	CGameWorld::Remove(pEntity, true);

	if( interiorLocation.GetInteriorProxyIndex() != INTLOC_INVALID_INDEX )
	{
		CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation( interiorLocation );
		if (AssertVerify(pInteriorProxy))
		{
			CInteriorInst* pIntInst = pInteriorProxy->GetInteriorInst();
			if( pIntInst == NULL || (pIntInst && !pIntInst->CanReceiveObjects()) )
			{
				//Don't add object that is attach to a portal
				if(!interiorLocation.IsAttachedToPortal())
				{
					//Add to the outside first so physics, etc.. get added
					//Then it'll get moved to the correct interior from the retain list.
					CGameWorld::Add(pEntity, CGameWorld::OUTSIDE);
					pInteriorProxy->MoveToRetainList( pEntity );
				}
				return;
			}
		}
	}

	// It's outside, or the interior is loaded
	CGameWorld::Add(pEntity, interiorLocation);
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfaceObject::CreateBatchedEntities(s32& createdSoFar, const CReplayState& state)
{
	if(m_CreatePostUpdate.GetCount() < 1)
		return;

	// Sort the batched entity list so that frag parents are created prior to children.
	bool hasChanged = false;

	do 
	{
		hasChanged = false;

		atArray<const tCreatePacket*>::iterator it = m_CreatePostUpdate.begin();
		atArray<const tCreatePacket*>::iterator end = m_CreatePostUpdate.end();

		for(; it != end; ++it)
		{
			const tCreatePacket* pCreatePacket = *it;
			if(pCreatePacket->GetFragParentID() == -1 && pCreatePacket->GetWeaponParentID() == -1)
			{
				continue;
			}

			atArray<const tCreatePacket*>::iterator innerIt = it;	++innerIt;
			for(; innerIt != end; ++innerIt)
			{
				const tCreatePacket* pParentPacket = *innerIt;
				if(pParentPacket->GetReplayID().ToInt() == pCreatePacket->GetFragParentID())
				{
					*it = pParentPacket;
					*innerIt = pCreatePacket;

					hasChanged = true;
				}

				if(pParentPacket->GetReplayID().ToInt() == pCreatePacket->GetWeaponParentID())
				{
					*it = pParentPacket;
					*innerIt = pCreatePacket;

					hasChanged = true;
				}
			}
		}

	} while (hasChanged);

	CReplayInterface<CObject>::CreateBatchedEntities(createdSoFar, state);
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfaceObject::PlaybackSetup(ReplayController& controller)
{
	//is this function needed?
	eReplayPacketId packetID = controller.GetCurrentPacketID();

	// Don't do PlaybackSetup() for the frag or fragbone packets
	if(	packetID == PACKETID_FRAGDATA ||
		packetID == PACKETID_FRAGBONEDATA)
		return;

	if(packetID == PACKETID_OBJECTCREATE)
	{
		const CPacketObjectCreate* pCreatePacket = controller.ReadCurrentPacket<CPacketObjectCreate>();
		m_currentCreatePacketVersion = pCreatePacket->GetPacketVersion();
	}

	CReplayInterface<CObject>::PlaybackSetup(controller);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ExtractObjectType(CObject* pObject, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller)
{
	
	if(pObject->IsADoor())
	{
		const CPacketDescriptor<CObject>* pPacketDescriptor = NULL;
		if(FindPacketDescriptorForEntityType(1, pPacketDescriptor))
		{
			((this)->*(pPacketDescriptor->GetExtractFunc()))(pObject, pPrevPacket, pNextPacket, controller);
		}
	}
	else
	{
		CInterpEventInfo<CPacketObjectUpdate, CObject> info;
		info.SetEntity(pObject);
		info.SetNextPacket((CPacketObjectUpdate*)pNextPacket);
		info.SetInterp(controller.GetInterp());
		((CPacketObjectUpdate*)pPrevPacket)->Extract(info);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceObject::RemoveElement(CObject* pElem, const CPacketObjectDelete* pDeletePacket, bool isRewinding)
{
	ReplayObjectExtension* pExtension = ReplayObjectExtension::GetExtension(pElem);

	//If it's a weapon component, release it from the weapon.
	if(pElem)
	{
		CWeaponComponent* pComponent = pElem->GetWeaponComponent();
		if(pComponent)
		{
			CWeapon* pWeapon = pComponent->GetWeapon();
			if(pWeapon)
			{
				pWeapon->ReleaseComponent(pComponent);
			}
		}
	}

	// Check if the object was waiting to be hidden 
	if(pExtension)
	{
		int index = m_deferredMapObjectToHide.Find(deferredMapObjectToHide(mapObjectID(pExtension->GetMapHash()), pDeletePacket ? pDeletePacket->GetReplayID() : ReplayIDInvalid));
		if(index != -1)
		{
			m_deferredMapObjectToHide.Delete(index);
		}
	}

	m_fragChildrenUnlinked.Delete(pElem->GetReplayID());
			
	if(pExtension != NULL && pExtension->GetMapHash() != REPLAY_INVALID_OBJECT_HASH)
	{
		{
			for(int i = 0; i < m_pool.GetSize(); ++i)
			{
				CObject* pMapObject = m_pool.GetSlot(i);

				if(pMapObject && pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetHash() == pExtension->GetMapHash())
				{
					// NOTE:- isRewinding is true going backwards.
					// When playing create packets backwards we reinstate the map objects.
					// When playing delete packets forwards we don't put the map objects back unless it was converted to a dummy on recording.
					//		Or it's a door.
					if((pDeletePacket && pDeletePacket->IsConvertedToDummyObject()) || pMapObject->IsADoor() || isRewinding == true)
					{
						RemoveHideRecordForMapObject(pMapObject->GetRelatedDummy()->GetHash(), pElem->GetReplayID());
						if(IsMapObjectExpectedToBeHidden(pMapObject->GetRelatedDummy()->GetHash()))
						{
							UnhideMapObject(pMapObject);
						}
					}
					pExtension->SetMapHash(REPLAY_INVALID_OBJECT_HASH);

					///Sort out the animated buildings
					CBaseModelInfo *pBaseModelInfo = pMapObject->GetBaseModelInfo();

					// auto-start animation if required
					if(pBaseModelInfo && pBaseModelInfo->GetAutoStartAnim())
					{
						RemoveElementAnimatedBuildings(pMapObject);
					}
					break;
				}
			}
		}
	}

	bool baseResult = CReplayInterface::RemoveElement(pElem, pDeletePacket, isRewinding);
	
	CObjectPopulation::DestroyObject(pElem);

	return baseResult & true;
}

void CReplayInterfaceObject::RemoveElementAnimatedBuildings(CObject* pMapObject)
{
	//Copied from CObject::CreateDrawable(), need to restore animated buildings

	///
	if (fragInst* pInst = pMapObject->GetFragInst())
	{
		if (pInst && !pInst->GetCached())
		{
			pInst->PutIntoCache();
		}
	}
	///

	if(pMapObject->GetDrawable())
	{
		if(!pMapObject->GetSkeleton())
		{
			pMapObject->CreateSkeleton();
		}
		if(!pMapObject->GetAnimDirector())
		{
			pMapObject->CreateAnimDirector(*(pMapObject->GetDrawable()));
			pMapObject->AddToSceneUpdate();
		}
	}

	// Object has to be on the process control list for the animation system to be updated
	if (!pMapObject->GetIsOnSceneUpdate())
	{
		pMapObject->AddToSceneUpdate();
	}

	CBaseModelInfo *pBaseModelInfo = pMapObject->GetBaseModelInfo();

	strLocalIndex clipDictionaryIndex = strLocalIndex(pBaseModelInfo->GetClipDictionaryIndex());
	if(clipDictionaryIndex.Get() != -1)
	{
		const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex.Get(), pBaseModelInfo->GetModelNameHash());
		if(Verifyf(pClip, "Couldn't get clip! '%s' '%s'", g_ClipDictionaryStore.GetName(clipDictionaryIndex), pBaseModelInfo->GetModelName()))
		{

			if(pMapObject->GetAnimDirector())
			{
				CGenericClipHelper& clipHelper = pMapObject->GetMoveObject().GetClipHelper();

				clipHelper.BlendInClipByDictAndHash(pMapObject, clipDictionaryIndex.Get(), pBaseModelInfo->GetModelNameHash(), INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, true, false);
				clipHelper.SetPhase(1.0f);
				clipHelper.SetRate(1.0f);
			}

			//Reset the phase and rate for every lod level on the map object
			if(pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetLodData().HasLod() )
			{
				CEntity* pLod = (CEntity*)pMapObject->GetRelatedDummy()->GetLod();

				while (pLod)
				{
					if(pLod->GetIsTypeAnimatedBuilding() && pLod->GetAnimDirector())
					{
						CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pLod);
						CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

						moveAnimatedBuilding.SetReplayPhase(-1.0f);
						moveAnimatedBuilding.SetReplayRate(-1.0f);
						pAnimatedBuilding->UpdateRateAndPhase();
					}

					if(!pLod->GetLodData().HasLod())
					{
						break;
					}

					pLod = (CEntity*)pLod->GetLod(); 
				} 
			}	
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::UnhideMapObject(CObject* pMapObject)
{
	// We need this override to allow map objects to have their
	// Physics turned back on when rewinding a clip.
	m_allowPhysicsOverride = true;
	pMapObject->InitPhys();
	pMapObject->AddPhysics();
	pMapObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY,true,false);
	CLODLightManager::UnregisterBrokenLightsForEntity(pMapObject);
	m_allowPhysicsOverride = false;
	if(pMapObject->IsADoor())
	{
		CDoor* pDoor = static_cast<CDoor*>(pMapObject);
		pDoor->SetOpenDoorRatio(FLT_MAX);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ResolveMapIDAndAndUnHide(u32 mapHashID, CReplayID const &replayID)
{
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		CObject* pMapObject = m_pool.GetSlot(i);

		if(pMapObject && pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetHash() == mapHashID)
		{
			RemoveHideRecordForMapObject(pMapObject->GetRelatedDummy()->GetHash(), replayID);
			UnhideMapObject(pMapObject);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ProcessRewindDeletionWithoutEntity(CPacketObjectCreate const *pCreatePacket, CReplayID const &replayID)
{
	// An object may not exist upon rewinding (when skipping over the whole lifetime of an object for example - destroyable bin bags etc),
	// but we still may need to un-hide the associated map object.
	replayIDFrameRefPair* pHider = m_mapObjectHideInfos.Access(pCreatePacket->GetMapHash());

	if(pHider && pHider->id == replayID)
	{
		ResolveMapIDAndAndUnHide(pCreatePacket->GetMapHash(), replayID);
	}

	CReplayInterface<CObject>::ProcessRewindDeletionWithoutEntity(pCreatePacket, replayID);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::SetHideRecordForMapObject(u32 hash, CReplayID replayID, FrameRef frameRef)
{
	replayIDFrameRefPair* pHider = m_mapObjectHideInfos.Access(hash);
	if(!pHider)
	{
		replayIDFrameRefPair newHider;
		newHider.id = replayID;
		newHider.frameRef = frameRef;
		m_mapObjectHideInfos.Insert(hash, newHider);
		replayDebugf3("Setting hide %u, 0x%08X, %u:%u", hash, replayID.ToInt(), frameRef.GetBlockIndex(), frameRef.GetFrame());
	}
	else
	{
		if(frameRef < pHider->frameRef)
		{
			pHider->id = replayID;
			pHider->frameRef = frameRef;
			replayDebugf3("Updating hide %u, 0x%08X, %u:%u", hash, replayID.ToInt(), frameRef.GetBlockIndex(), frameRef.GetFrame());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::RemoveHideRecordForMapObject(u32 hash, CReplayID replayID)
{
	replayIDFrameRefPair* pHider = m_mapObjectHideInfos.Access(hash);
	if(!pHider)
	{
		// This is fine.
		//replayAssertf(false, "Trying to decrement for an object we're not expecting to be hidden?, (%u, 0x%08X)", hash, replayID.ToInt());
	}
	else
	{
		if(pHider->id == replayID)
		{
			m_mapObjectHideInfos.Delete(hash);
			replayDebugf3("Removing hide %u, 0x%08X", hash, replayID.ToInt());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceObject::IsMapObjectExpectedToBeHidden(u32 hash)
{
	return m_mapObjectHideInfos.Access(hash) == NULL;
}


#if __BANK
//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceObject::IsMapObjectExpectedToBeHidden(u32 hash, CReplayID& replayID, FrameRef& frameRef)
{
	replayIDFrameRefPair* pHider = m_mapObjectHideInfos.Access(hash);
	if(!pHider)
	{
		return false;
	}
	else
	{
		replayID = pHider->id;
		frameRef = pHider->frameRef;
		return true;
	}
}
#endif // __BANK


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist)
{
	CPacketObjectUpdate const* pPacket = controller.ReadCurrentPacket<CPacketObjectUpdate>();

	m_interper.Init(controller, pPacket);
	INTERPER_OBJ interper(m_interper, controller);

	if(controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP_LINEAR))
	{
		EnsureEntityIsCreated(pPacket->GetReplayID(), controller.GetPlaybackFlags(), true);	
	}

	CEntityGet<CObject> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	CObject* pObject = entityGet.m_pEntity;

	if(!pObject && (entityGet.m_alreadyReported || entityMayNotExist))
	{
		// Entity wasn't created for us to update....but we know this already
		return;
	}

	if(!pObject)
	{
		// If we failed to get the entity then check whether it's in the deletionsOnRewind list.
		// If so then we're rewinding past an object that only existed for a single frame and
		// this didn't get a chance to be created
		if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && m_deletionsOnRewind.Access(pPacket->GetReplayID()) != NULL)
		{
			replayDebugf1("Won't update this entity as it was never created - 0x%08X", pPacket->GetReplayID().ToInt());
			return;	// This is understandable so just return out.
		}
	}

	CPacketObjectUpdate const* pNextObjectPacket = m_interper.GetNextPacket();

	if(Verifyf(pObject, "Failed to find Object to update, but it wasn't reported as 'failed to create' - 0x%08X - Packet %p", (u32)pPacket->GetReplayID(), pPacket))
	{
		//Check that the cached map object still exists.
		CObject* pMapObject = pObject->ReplayGetMapObject();
		if(pMapObject && !DoesObjectExist(pMapObject))
		{
			pObject->ReplaySetMapObject(NULL);
		}

		ExtractObjectType(pObject, pPacket, pNextObjectPacket, controller);

		// Don't extract bones for animated objects which we've recorded the phase for. 
		// Playback of these objects is done the same as recording using the recorded phase.
		if(!pPacket->IsUsingRecordAnimPhase())
		{
			if (m_interper.HasPrevFragData())
			{
				replayAssertf((m_interper.GetPrevFragDataPacket()->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS) || (m_interper.GetPrevFragDataPacket()->GetPacketID() == PACKETID_FRAGDATA), "CReplayInterfaceObject::PlayUpdatePacket()...Expecting frag data packet.");

				if(m_interper.GetPrevFragDataPacket()->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS)
					(static_cast < CPacketFragData_NoDamageBits const * > (m_interper.GetPrevFragDataPacket()))->Extract((CEntity*)pObject);
				else
					(static_cast < CPacketFragData const * > (m_interper.GetPrevFragDataPacket()))->Extract((CEntity*)pObject);

				if(m_interper.GetPrevFragBoneCount() > 0)
				{
					// Load Frag Bones
					if (m_interper.GetPrevFragBoneCount() > m_interper.GetNextFragBoneCount())
					{
						CPacketFragBoneData const* pPrevFragBonePacket = m_interper.GetPrevFragBonePacket();
						pPrevFragBonePacket->Extract(pObject, NULL, 0.0f, false);
					}
					else if (m_interper.GetPrevFragBoneCount() == m_interper.GetNextFragBoneCount())
					{
						CPacketFragBoneData const* pPrevFragBonePacket = m_interper.GetPrevFragBonePacket();
						CPacketFragBoneData const* pNextFragBonePacket = m_interper.GetNextFragBonePacket();

						replayAssert(pNextFragBonePacket);
						pPrevFragBonePacket->Extract(pObject, pNextFragBonePacket, controller.GetInterp(), false);
					}
				}
			}
			else
			{
				// No Frag Data (and assume no Frag Bones)
				replayAssert(m_interper.GetPrevFragBoneCount() == 0);
			}

			if(m_interper.GetPrevAnimObjBoneCount() > 0)
			{
				if (m_interper.GetPrevAnimObjBoneCount() > m_interper.GetNextAnimObjBoneCount())
				{
					CPacketAnimatedObjBoneData const* pPrevAnimObjBonePacket = m_interper.GetPrevAnimObjBonePacket();
					pPrevAnimObjBonePacket->Extract(pObject, NULL, 0.0f, false);
				}
				else if (m_interper.GetPrevAnimObjBoneCount() == m_interper.GetNextAnimObjBoneCount())
				{
					CPacketAnimatedObjBoneData const* pPrevAnimObjBonePacket = m_interper.GetPrevAnimObjBonePacket();
					CPacketAnimatedObjBoneData const* pNextAnimObjBonePacket = m_interper.GetNextAnimObjBonePacket();

					atArray<sInterpIgnoredBone> noInterpBones;
					// url:bugstar:4188795 - treasure chest lid is interpolating when we don't want it to.
					if(pObject->GetModelNameHash() == 0x5993a0e /*"xm_prop_x17_chest_closed"*/)
					{
						noInterpBones.PushAndGrow(sInterpIgnoredBone(3, false));
					}
					pPrevAnimObjBonePacket->Extract(pObject, pNextAnimObjBonePacket, controller.GetInterp(), false, &noInterpBones);
				}
			}
			// Hack for B*2341690! Call this here to update the bounding box used for visibility.
			CPacketObjectUpdate::UpdateBoundingBoxForLuxorPlane(pObject);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::SetFragParent(CObject* pObject, CEntity* pParent) const
{
	pObject->SetFragParent(pParent);
	pObject->SetAlpha(pParent->GetAlpha());

	if(m_currentCreatePacketVersion < CPacketObjectCreate_V1)
	{
		if(pParent && pParent->GetIsTypeVehicle())
		{
			CVehicle* pParentVehicle = static_cast<CVehicle*>(pParent);
			fragInst* pFragInst = pObject->GetFragInst();

			// go through wheel children and see if any are intact in this pFragInst, if so this must be a wheel
			for(int wheelIndex = 0; wheelIndex < pParentVehicle->GetNumWheels(); ++wheelIndex)
			{
				CWheel* pWheel = pParentVehicle->GetWheel(wheelIndex);
				int wheelChildIndex = pWheel->GetFragChild();
				if(wheelChildIndex > -1 && !pFragInst->GetChildBroken(wheelChildIndex))
				{
					pObject->m_nObjectFlags.bCarWheel = true;
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PostProcess()
{
	CReplayInterface::PostProcess(); 
	ProcessDeferredMapObjectToHide();

	// Go through any frag children that weren't linked properly...
	atArray<CReplayID> toRemove;
	atMap<CReplayID, CReplayID>::Iterator updateIter = m_fragChildrenUnlinked.CreateIterator();
	while(updateIter.AtEnd() == false)
	{
		// Find the parent first...
		CEntityGet<CEntity> parentEG(updateIter.GetData());
		CReplayMgrInternal::GetEntityAsEntity(parentEG);

		if(parentEG.m_pEntity)
		{	// Parent exists so find the child...
			CEntityGet<CObject> childEG(updateIter.GetKey());
			GetEntity(childEG);
			if(childEG.m_pEntity)
			{	// Both exist so set 'em up
				if(childEG.m_pEntity->GetFragParent() == nullptr)
				{
					SetFragParent(childEG.m_pEntity, parentEG.m_pEntity);
				}
			}

			// Add to the 'toRemove' list
			toRemove.PushAndGrow(updateIter.GetKey());
		}

		updateIter.Next();
	}

	// Remove the ones we've linked
	for(int i = 0; i < toRemove.size(); ++i)
	{
		m_fragChildrenUnlinked.Delete(toRemove[i]);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::ProcessDeferredMapObjectToHide()
{
	for(int i = 0; i < m_pool.GetSize() && m_deferredMapObjectToHide.GetCount() > 0; ++i)
	{
		CObject* pMapObject = m_pool.GetSlot(i);

		if(pMapObject && pMapObject->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY && pMapObject->GetRelatedDummy())
		{
			for(int j = 0; j < m_deferredMapObjectToHide.GetCount(); ++j)
			{
				if( pMapObject->GetRelatedDummy()->GetHash() == m_deferredMapObjectToHide[j].m_MapObjectId.objectHash)
				{
					if(pMapObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY))
					{
						pMapObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY,false,false);
						CLODLightManager::RegisterBrokenLightsForEntity(pMapObject);
						pMapObject->RemovePhysics();
						pMapObject->DeleteInst();
						if(pMapObject->GetAnimDirector())
						{
							pMapObject->DeleteAnimDirector();
						}
						
						// Set pointer to map object on replay object
						CEntityGet<CObject> childEG(m_deferredMapObjectToHide[j].m_ReplayObjectId);
						GetEntity(childEG);
						if(childEG.m_pEntity)
						{
							childEG.m_pEntity->ReplaySetMapObject(pMapObject);
						}

						// Add the count to the entry for this map object in the map
						if(m_deferredMapObjectToHide[j].m_replayIDFrameRefPair.id != ReplayIDInvalid)
						{
							SetHideRecordForMapObject(pMapObject->GetRelatedDummy()->GetHash(), m_deferredMapObjectToHide[j].m_replayIDFrameRefPair.id, m_deferredMapObjectToHide[j].m_replayIDFrameRefPair.frameRef);
						}


						// Do we have an objects to place into the world to replace the hidden man object?
						if(m_deferredMapObjectToHide[j].m_objectsToAddStamp != -1)
						{
							AddObjectsToWorldPostMapObjectHide(m_deferredMapObjectToHide[j].m_objectsToAddStamp, pMapObject);
							m_deferredMapObjectToHide[j].m_objectsToAddStamp = -1;
						}
					}

					m_deferredMapObjectToHide.DeleteFast(j);
					break;
				}
			}
		}
	}

	/*-----------------------------------------------------------------------------*/

	int cnt = m_counterpartlessObjectsToAdd.GetCount();

	// Process any deferred entries with no map counter-part.
	for(int i=cnt-1; i>=0; i--)
	{
		if(DoesObjectExist(m_counterpartlessObjectsToAdd[i].m_pObjectToAdd))
			AddReplayEntityToWorld(m_counterpartlessObjectsToAdd[i].m_pObjectToAdd, m_counterpartlessObjectsToAdd[i].m_location);

		m_counterpartlessObjectsToAdd.DeleteFast(i);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceObject::ShouldRecordElement(const CObject* pObject) const
{
	if(m_mapObjectsToRecord.Find((CObject*)pObject) == -1 && m_otherObjectsToRecord.Find((CObject*)pObject) == -1)
	{
		return false;
	}

	if( pObject->GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY )
	{
		// Don't record objects that are owned by a compentity (since they're covered by the mapstore)
		return false;
	}

	return true;
}


int CReplayInterfaceObject::AddDeferredMapObjectToHide(mapObjectID mapObjId, CReplayID replayID, FrameRef frameRef)
{
	deferredMapObjectToHide mapObjectToHide(mapObjId, replayID);

	int idx = m_deferredMapObjectToHide.Find(mapObjectToHide);

	if(idx == -1)
	{
		m_deferredMapObjectToHide.PushAndGrow(mapObjectToHide);
		idx = m_deferredMapObjectToHide.Find(mapObjectToHide);
	}
	
	if(replayID != ReplayIDInvalid)
	{
		replayAssert(frameRef != FrameRef());
		replayIDFrameRefPair& current = m_deferredMapObjectToHide[idx].m_replayIDFrameRefPair;
		if(frameRef < current.frameRef)
		{
			current.id			= replayID;
			current.frameRef	= frameRef;
		}
	}
	return idx;
}


void CReplayInterfaceObject::AddDeferredMapObjectToHideWithMoveToInterior(mapObjectID mapObjId, CObject *pObject, fwInteriorLocation interiorLocation)
{
	FrameRef frameRef;
	pObject->GetCreationFrameRef(frameRef);

	// Submit a deferred map object hide as usual.
	int existingIdx = AddDeferredMapObjectToHide(mapObjId, pObject->GetReplayID(), frameRef);
	// Promote it to a physics swap.
	if(m_deferredMapObjectToHide[existingIdx].m_objectsToAddStamp == -1) 
		m_deferredMapObjectToHide[existingIdx].m_objectsToAddStamp = m_objectToAddStamp++; // Issue a new stamp if this is the 1st use.

	// Link the object to the deferred map object to hide entry.
	ReplayObjectExtension *pExtension = ReplayObjectExtension::GetExtension(pObject);
	pExtension->m_addStamp = m_deferredMapObjectToHide[existingIdx].m_objectsToAddStamp;
	pExtension->m_location = interiorLocation;
}


void CReplayInterfaceObject::AddDeferredMapObjectToHideWithMoveToInterior(CObject *pObject, fwInteriorLocation interiorLocation)
{
	ReplayObjectExtension *pExtension = ReplayObjectExtension::GetExtension(pObject);

	if(pExtension)
	{
		if(pExtension->GetMapHash() != REPLAY_INVALID_OBJECT_HASH)
		{
			AddDeferredMapObjectToHideWithMoveToInterior(ReplayObjectExtension::GetExtension(pObject)->GetMapHash(), pObject, interiorLocation);
		}
		else
		{
			counterpartlessObjectToAdd objectToAdd(pObject, interiorLocation);
			m_counterpartlessObjectsToAdd.PushAndGrow(objectToAdd);
		}
	}
	else
	{
		counterpartlessObjectToAdd objectToAdd(pObject, interiorLocation);
		m_counterpartlessObjectsToAdd.PushAndGrow(objectToAdd);
	}
}


void CReplayInterfaceObject::AddObjectsToWorldPostMapObjectHide(s32 addStamp, CObject *pMapObject)
{
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		CObject* pObject = m_pool.GetSlot(i);

		if(pObject && pObject->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
		{
			ReplayObjectExtension *pExtension = ReplayObjectExtension::GetExtension(pObject);

			if(pExtension && pExtension->m_addStamp == addStamp)
			{
				AddReplayEntityToWorld(pObject, pExtension->m_location);
				pObject->ReplaySetMapObject(pMapObject);
				pExtension->m_addStamp = -1;
			}
		}
	}
}


bool CReplayInterfaceObject::DoesObjectExist(CObject *pObject)
{
	for(int i=0; i<m_pool.GetSize(); i++)
	{
		if(m_pool.GetSlot(i) == pObject)
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::AddMoveToInteriorLocation(CObject *pObject, fwInteriorLocation interiorLocation)
{
	// Use map object hide functionality.
	AddDeferredMapObjectToHideWithMoveToInterior(pObject, interiorLocation);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PlayDeletePacket(ReplayController& controller)
{
	CReplayInterfaceTraits<CObject>::tDeletePacket const* pPacket = controller.ReadCurrentPacket<CReplayInterfaceTraits<CObject>::tDeletePacket>();
	replayAssertf(pPacket->GetPacketID() == m_deletePacketID, "Incorrect Packet");

	if (pPacket->GetReplayID() == ReplayIDInvalid)
	{
		controller.AdvancePacket();
		return;
	}

	CReplayInterface<CObject>::PlayDeletePacket(controller);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PlayPacket(ReplayController& controller)
{
	const CPacketBase* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	switch(pBasePacket->GetPacketID())
	{
		case PACKETID_OBJECTHIDDENMAP:
		{
			const CPacketHiddenMapObjects* pHiddenMapObjectsPacket = controller.ReadCurrentPacket<CPacketHiddenMapObjects>();
			pHiddenMapObjectsPacket->Extract(m_mapObjectsToHide);
			break;
		}
		case PACKETID_DESTROYED_MAP_OBJECTS_FOR_CLIP_START:
			{
				const CPacketDestroyedMapObjectsForClipStart* pDestroyedObjects = controller.ReadCurrentPacket<CPacketDestroyedMapObjectsForClipStart>();
				if(controller.GetIsFirstFrame())
					pDestroyedObjects->Extract(this);
				break;
			}
		case PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS:
			{
				const CPacketBriefLifeTimeDestroyedMapObjects* pBriefLifeTimeMapObjects = controller.ReadCurrentPacket<CPacketBriefLifeTimeDestroyedMapObjects>();
					pBriefLifeTimeMapObjects->Extract(this, (controller.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK) != 0);
				break;
			}
		default:
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::OnDelete(CPhysical* pEntity)
{
	REPLAY_CHECK(pEntity->GetType() == ENTITY_TYPE_OBJECT, NO_RETURN_VALUE, "CReplayInterfaceObject calling OnDelete on a non-object!");

	// Remove the object from this list in this packet
	CPacketOverrideObjectLightColour::RemoveColourOverride(pEntity->GetReplayID());

	bool beingRecorded = IsCurrentlyBeingRecorded(pEntity->GetReplayID());
	if(beingRecorded == false)
	{
		CObject *pObject = static_cast<CObject*>(pEntity);
		// Are we recording, but missed this object?
		if(CReplayMgr::IsRecording() && m_mapObjectsToRecord.Find(pObject) != -1)
		{
			m_DestroyedMapObjects.AddBriefLifeTimeMapObject(pObject);
		}

		DeregisterEntity(pEntity);
		return;
	}

	CObject* pObject = (CObject*)pEntity;

	if(ReplayEntityExtension::HasExtension(pEntity) == false)
	{
		// Object isn't being recorded...
		replayAssertf(m_mapObjectsToRecord.Find(pObject) == -1 && m_otherObjectsToRecord.Find(pObject) == -1, "Object doesn't have a replay ID but is recorded...");
		return;
	}

	StopRecordingMapObject(pObject);
	StopRecordingObject(pObject);

	CReplayInterface<CObject>::OnDelete(pObject);

	// Remove this AFTER we've recorded the deletion (and joining creation) packet as it contains information we need
	ReplayObjectExtension::Remove(pObject);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	PrintOutPacketHeader(pBasePacket, mode, handle);

	const CPacketDescriptor<CObject>* pPacketDescriptor = NULL;
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
bool CReplayInterfaceObject::TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason))
{
	if(!CReplayInterface<CObject>::TryPreloadPacket(pPacket, preloadSuccess, currGameTime PRELOADDEBUG_ONLY(, failureReason)))
		return false;

	if(pPacket->GetPacketID() != PACKETID_OBJECTCREATE|| !preloadSuccess)
		return true;

	const CPacketObjectCreate *pCreatePacket = static_cast<const CPacketObjectCreate*>(pPacket);
	if(pCreatePacket->IsPropOverride() && pCreatePacket->ShouldPreloadProp())
	{
		// Load Prop Information...
		strLocalIndex dwdIdx = strLocalIndex(g_DwdStore.FindSlot(pCreatePacket->GetPropHash()));
		replayAssertf(dwdIdx != STRLOCALINDEX_INVALID, "Invalid Prop hash %u for object 0x%08X in packet %p", pCreatePacket->GetPropHash(), pCreatePacket->GetReplayID().ToInt(), this);
		if(dwdIdx != STRLOCALINDEX_INVALID)
		{
			bool dwdSuccess = false;
			m_modelManager.PreloadRequest(pCreatePacket->GetPropHash(), dwdIdx, !pCreatePacket->UseMapTypeDefHash(), g_DwdStore.GetStreamingModuleId(), dwdSuccess, currGameTime);
			preloadSuccess &= dwdSuccess;

#if PRELOADDEBUG
			if(!dwdSuccess && failureReason.GetLength() == 0)
			{
				char temp[256] = {0};
				sprintf(temp, "Failed to preload Object Prop Hash %u", pCreatePacket->GetPropHash());
				failureReason = temp;
			}
#endif // PRELOADDEBUG
		}


		strLocalIndex txdIdx = strLocalIndex(g_TxdStore.FindSlot(pCreatePacket->GetTexHash()));
		replayAssertf(txdIdx != STRLOCALINDEX_INVALID, "Invalid Tex hash %u for object 0x%08X in packet %p", pCreatePacket->GetTexHash(), pCreatePacket->GetReplayID().ToInt(), this);
		if(txdIdx != STRLOCALINDEX_INVALID)
		{
			bool txdSuccess = false;
			m_modelManager.PreloadRequest(pCreatePacket->GetTexHash(), txdIdx, !pCreatePacket->UseMapTypeDefHash(), g_TxdStore.GetStreamingModuleId(), txdSuccess, currGameTime);
			preloadSuccess &= txdSuccess;

#if PRELOADDEBUG
			if(!txdSuccess && failureReason.GetLength() == 0)
			{
				char temp[256] = {0};
				sprintf(temp, "Failed to preload Object Tex Hash %u", pCreatePacket->GetTexHash());
				failureReason = temp;
			}
#endif // PRELOADDEBUG
		}
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
// Get the space required for an update packet for a particular object
u32 CReplayInterfaceObject::GetUpdatePacketSize(CObject* pObj) const
{
	u32 packetSize = sizeof(CPacketObjectUpdate) + GetFragDataSize(pObj);
	
	if(pObj->IsADoor())
	{
		packetSize = sizeof(CPacketDoorUpdate) + GetFragDataSize(pObj);
	}

	//if we don't currently have a valid create packet, then the next update packet will contain one, so we have to account for the bigger size here
	if(HasCreatePacket(pObj->GetReplayID()) == false)
	{
		packetSize += sizeof(CPacketObjectCreate);
	}

	return packetSize;
}


//////////////////////////////////////////////////////////////////////////
// Get the frag data size for a particular object
u32 CReplayInterfaceObject::GetFragDataSize(const CEntity* pEntity) const
{
	replayAssert(pEntity && "CReplayInterfaceObject::GetFragDataSize: Invalid entity");
	u32 dataSize = 0;

	rage::fragInst* pFragInst = pEntity->GetFragInst();
	if (pFragInst && pFragInst->GetType())
	{
		dataSize += sizeof(CPacketFragData);

		dataSize += GetFragBoneDataSize(pEntity);
	}

	return dataSize;
}


//////////////////////////////////////////////////////////////////////////
// Get the frag bone data size for a particular object
u32 CReplayInterfaceObject::GetFragBoneDataSize(const CEntity* pEntity) const
{
	replayAssert(pEntity && "CReplayInterfaceObject::GetFragBoneDataSize: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
		{
			u32 numberOfBones	 = pFragCacheEntry->GetHierInst()->skeleton->GetBoneCount();
			u32 fragBoneDataSize = numberOfBones * sizeof(CPacketPositionAndQuaternion);
			if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
			{
				numberOfBones = pFragCacheEntry->GetHierInst()->damagedSkeleton->GetBoneCount();
				fragBoneDataSize += numberOfBones * sizeof(CPacketPositionAndQuaternion);
			}

#if REPLAY_GUARD_ENABLE
			fragBoneDataSize += sizeof(int);
#endif // REPLAY_GUARD_ENABLE
			return fragBoneDataSize + sizeof(CPacketFragBoneData);
		}
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::RecordFragData(CEntity* pEntity, CReplayRecorder& recorder)
{
	replayAssert(pEntity && "CReplayInterfaceObject::RecordFragData: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();
	if (pFragInst && pFragInst->GetType())
	{
		recorder.RecordPacketWithParam<CPacketFragData_NoDamageBits>(pEntity);

		RecordFragBoneData(pEntity, recorder);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::RecordFragBoneData(CEntity* pEntity, CReplayRecorder& recorder)
{
	replayAssert(pEntity && "CReplayInterfaceObject::RecordFragBoneData: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();
	rage::fragCacheEntry* pFragCacheEntry = pEntity->GetFragInst()->GetCacheEntry();
	if (pFragInst && pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
	{
		recorder.RecordPacketWithParam<CPacketFragBoneData>(pEntity);
	}
}

bool CReplayInterfaceObject::MatchesType(const CEntity* pEntity) const
{
	if(pEntity->GetType() == ENTITY_TYPE_OBJECT || pEntity->GetType() == ENTITY_TYPE_DUMMY_OBJECT)
	{
		const CObject* pObject = static_cast<const CObject*>(pEntity);
		if(!pObject->IsPickup())
		{
			return true;
		}
	}
	return false;
}

void CReplayInterfaceObject::RecordBoneData(CEntity* pEntity, CReplayRecorder& recorder)
{
	CObject* pObject = (CObject*)pEntity;

	if (pObject && pObject->GetSkeleton() && (pObject->GetSkeleton()->GetBoneCount() > 0) && ShouldRecordBoneData(pObject))
	{
		recorder.RecordPacketWithParam<CPacketAnimatedObjBoneData>(pObject);
	}
}

bool CReplayInterfaceObject::ShouldRecordBoneData(CObject* pObject)
{
	u32 excludeFromBoneRecording[] = { 0x38d1fdf5/*"stt_prop_stunt_bblock_huge_05"*/, 
										0x6901972/*"stt_prop_stunt_bblock_huge_04"*/,
										0x4f762b3d/*"stt_prop_stunt_bblock_huge_03"*/, 
										0x21e1d015/*"stt_prop_stunt_bblock_huge_02"*/,
										0xf3b76f46/*"sst_prop_race_start_line_01b"*/,
										0x4e8cee2c/*"stt_prop_stunt_bblock_mdm3"*/,
										0x9ee25faf/*"stt_prop_track_straight_s"*/,
										0x6393e913/*"stt_prop_track_straight_m"*/,
										0xe772c534/*"bkr_prop_biker_bblock_huge_01"*/,
										0x393268b6/*"bkr_prop_biker_bblock_huge_02"*/,
										0xcbca0de3/*"bkr_prop_biker_bblock_huge_03"*/,
										0x1e7eb34f/*"bkr_prop_biker_bblock_huge_04"*/,
										0xb6b7ab45/*"stt_prop_race_start_line_01b"*/,
										0xfcad85e0/*"stt_prop_race_start_line_02"*/,
										0x32ec61f6/*"sr_prop_special_bblock_x13"*/,
	};
	for(int i = 0; i < sizeof(excludeFromBoneRecording) / sizeof(u32); ++i)
	{
		if(pObject->GetModelNameHash() == excludeFromBoneRecording[i])
			return false;
	}

	// We record bone data for the parachute.
	if(pObject->GetIsParachute())
		return true;

	// Also objects which won't be placed into the frag cache...
	if(pObject->GetFragInst() == NULL)
		return true;

	// Also objects which have been given an animation task.
	const CObjectIntelligence* pIntelligence = pObject->GetObjectIntelligence();
	if (pIntelligence)
	{
		if(pIntelligence->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION))
		{
			return true;
		}
	}

	/*
	// TODO:- Put this in if we need it.
	// ...plus objects not on the fragment cache.
	if(pObject->GetFragInst()->GetCacheEntry() == NULL)
		return true;
	*/

	return false;
}


void CReplayInterfaceObject::RecordMapObject(CObject* pObject)
{
	mapObjectID id;
	bool recordObject = false;

	if( !pObject || pObject->GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY )
		return;

	// All objects initially created from a DummyObject will have the correct hash.

	if(pObject->GetRelatedDummy())
	{
		id.objectHash = pObject->GetRelatedDummy()->GetHash();
		if(FindHiddenMapObject(id) == -1)
		{
			m_mapObjectsToHide.Push(id);

			m_mapObjectsMap.Insert(pObject, id);

			recordObject = true;
		}
		else
		{
			//Check if there is an object with the same model hash and position 
			if(m_mapObjectsMap.Access(pObject) == NULL)
			{
				m_mapObjectsToHide.Push(id);

				m_mapObjectsMap.Insert(pObject, id);

				recordObject = true;
			}
		}
	}

	if(pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject() && ((CObject*)pObject->GetFragParent())->GetRelatedDummy())
	{
		id.objectHash = ((CObject*)pObject->GetFragParent())->GetRelatedDummy()->GetHash();
		if(FindHiddenMapObject(id) == -1)
		{
			m_mapObjectsToHide.Push(id);

			m_mapObjectsMap.Insert(pObject->GetFragParent(), id);

			m_mapObjectsToRecord.PushAndGrow((CObject*)(pObject->GetFragParent()));
			recordObject = true;
		}
	}

	if(recordObject)
	{
		m_mapObjectsToRecord.PushAndGrow(pObject);
	}
}


void CReplayInterfaceObject::StopRecordingMapObject(CObject* pObject)
{
	mapObjectID* pId = m_mapObjectsMap.Access(pObject);
	if(pId)
	{
		int i = FindHiddenMapObject(*pId);
		replayAssert(i != -1);
		if(i != -1)
			m_mapObjectsToHide.DeleteFast(i);

		m_mapObjectsMap.Delete(pObject);
		m_mapObjectsToRecord.DeleteMatches(pObject);
	}
	else
	{
		m_mapObjectsToRecord.DeleteMatches(pObject);
	}
}


bool CReplayInterfaceObject::IsRecordingMapObject(CObject* pObject) const
{
	mapObjectID id;
	if(pObject->GetRelatedDummy())
	{
		id.objectHash = pObject->GetRelatedDummy()->GetHash();
		return FindHiddenMapObject(id) != -1;
	}

	return false;
}


void CReplayInterfaceObject::RecordObject(CObject* pObject)
{
	if( !pObject || pObject->GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY )
		return;

	if(m_otherObjectsToRecord.Find(pObject) == -1)
	{
		m_otherObjectsToRecord.PushAndGrow(pObject);
	}
}
void CReplayInterfaceObject::StopRecordingObject(CObject* pObject)
{
	if(m_otherObjectsToRecord.Find(pObject) != -1)
	{
		m_otherObjectsToRecord.DeleteMatches(pObject);
	}
}


bool CReplayInterfaceObject::GetBlockDetails(char* pString, bool& err, bool recording) const
{
	sprintf(pString, "%s:\n   %d bytes for %d elements (max %d, in pool %d).\n Record Buffer: %d (c:%d%s)\n Create Buffer: %d (c:%d)\n Full Create Buffer: %d (c:%d)", 
		GetLongFriendlyStr(), 
		GetActualMemoryUsage(), 
		recording ? m_mapObjectsToRecord.GetCount() + m_otherObjectsToRecord.GetCount() : m_entitiesDuringPlayback, 
		(int)m_pool.GetSize(), (int)m_pool.GetNoOfUsedSpaces(),
		m_recordBufferUsed, m_recordBufferSize, m_recordBufferFailure ? " EXCEEDED" : "",
		m_createPacketsBufferUsed, m_createPacketsBufferSize,
		m_fullCreatePacketsBufferUsed, m_fullCreatePacketsBufferSize);

	err = m_recordBufferFailure | m_createPacketsBufferFailure | m_fullCreatePacketsBufferFailure;
	return true;
}


int CReplayInterfaceObject::FindHiddenMapObject(const mapObjectID& id) const
{
	return m_mapObjectsToHide.Find(id);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceObject::ShouldRelevantPacketBeProcessedDuringJumpPackets(eReplayPacketId packetID)
{
	// TODO:- Maybe mark registered packets to be processed during jumps more formally through packet descriptor system.
	if(packetID ==  PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS)
		return true;
	return false;
}


void CReplayInterfaceObject::ProcessRelevantPacketDuringJumpPackets(eReplayPacketId packetID, ReplayController &controller, bool isBackwards)
{
	replayAssertf(packetID == PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS, "CReplayInterfaceObject::ProcessRelevantPacketDuringJumpPackets()...Unexpected packet Id.");

	switch (packetID)
	{
	case PACKETID_BRIEF_LIFETIME_DESTROYED_MAP_OBJECTS:
		{
			CPacketBriefLifeTimeDestroyedMapObjects const *pPacket = controller.ReadCurrentPacket<CPacketBriefLifeTimeDestroyedMapObjects>();
			pPacket->Extract(this, isBackwards);
			break;		
		}
	default:
		{
			replayAssertf(0, "CReplayInterfaceObject::ProcessRelevantPacketDuringJumpPackets()...Unexpected packet Id.");
			break;
		}
	}
}



//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceObject::OnJumpDeletePacketForwardsObjectNotExisting(CPacketObjectCreate const *pCreatePacket)
{
	// Hide the map object as if we'd created/updated/destroyed the Replay object in normal play forwards lifetime.
	AddDeferredMapObjectToHide(pCreatePacket->GetMapHash());
}


void CReplayInterfaceObject::OnJumpCreatePacketBackwardsObjectNotExisting(CPacketObjectCreate const *pCreatePacket)
{
	// Un-hide the map object as if we'd un-deleted/updated/un-created the Replay object in normal rewind lifetime.
	ResolveMapIDAndAndUnHide(pCreatePacket->GetMapHash(), CReplayID());
}


#endif // GTA_REPLAY
