#include "RopePacket.h"


#if GTA_REPLAY
#include "fwpheffects/ropemanager.h"
#include "Objects/object.h"
#include "physics/physics.h"
#include "scene/Physical.h"
#include "vehicles/Heli.h"
#include "Vehicles/vehicle.h"

atFixedArray<CReplayRopeManager::sRopeData, MAX_ROPES_TO_TRACK> CReplayRopeManager::m_ropeData;
atFixedArray<CReplayRopeManager::sRopePinData, MAX_ROPES_TO_TRACK> CReplayRopeManager::m_ropePinData;
//////////////////////////////////////////////////////////////////////////

void CReplayRopeManager::Reset()
{
	for( u32 i = 0; i < MAX_ROPES_TO_TRACK; i++ )
	{
		m_ropeData[i].ropeId = -1;
		m_ropePinData[i].ropeId = -1;
	}
}

void CReplayRopeManager::DeleteRope(int ropeId)
{
	for(int i=m_ropeData.size()-1;i>=0;i--)
	{
		if( m_ropeData[i].ropeId == ropeId)
			m_ropeData.Delete(i);
	}
}

void CReplayRopeManager::AttachEntites(int ropeId, CEntity* pEntityA, CEntity* pEntityB)
{
	for( u32 i = 0; i < m_ropeData.size(); i++)
	{
		if( m_ropeData[i].ropeId == ropeId )
		{
			m_ropeData[i].pAttachA = pEntityA;
			m_ropeData[i].pAttachB = pEntityB;
			m_ropeData[i].attachedA = true;
			m_ropeData[i].attachedB = true;
			return;
		}
	}

	if( m_ropeData.size() >= MAX_ROPES_TO_TRACK )
	{
		AssertMsg(false, "CReplayRopeManager::m_ropeData run out of space - probably not freeing up correctly");
		return;
	}

	sRopeData &ropeData = m_ropeData.Append();
	ropeData.ropeId = ropeId;
	ropeData.pAttachA = pEntityA;
	ropeData.pAttachB = pEntityB;
	ropeData.attachedA = true;
	ropeData.attachedB = true;
}

void CReplayRopeManager::DetachEntity(int ropeId, CEntity* pEntity)
{
	for( u32 i = 0; i < m_ropeData.size(); i++)
	{
		if( m_ropeData[i].ropeId == ropeId )
		{
			if( m_ropeData[i].pAttachA == pEntity )
				m_ropeData[i].attachedA = false;
			if( m_ropeData[i].pAttachB == pEntity)
				m_ropeData[i].attachedB = false;
		}
	}
}

CEntity* CReplayRopeManager::GetAttachedEntityA(int ropeId)
{
	for( u32 i = 0; i < m_ropeData.size(); i++)
	{
		if( m_ropeData[i].ropeId == ropeId && m_ropeData[i].attachedA )
		{
			return m_ropeData[i].pAttachA;
		}
	}
	return NULL;
}

CEntity* CReplayRopeManager::GetAttachedEntityB(int ropeId)
{
	for( u32 i = 0; i < m_ropeData.size(); i++)
	{
		if( m_ropeData[i].ropeId == ropeId && m_ropeData[i].attachedB )
		{
			return m_ropeData[i].pAttachB;
		}
	}
	return NULL;
}

void CReplayRopeManager::CachePinRope(int idx, const Vector3& pos, bool pin, int ropeID, bool rapelling)
{
	for( u32 i = 0; i < m_ropePinData.size(); i++)
	{
		if( m_ropePinData[i].ropeId == ropeID && m_ropePinData[i].idx == idx )
		{
			m_ropePinData[i].idx = idx;
			m_ropePinData[i].pos = pos;
			m_ropePinData[i].pin = pin;
			m_ropePinData[i].rapelling = rapelling;
			m_ropePinData[i].ropeId = ropeID;
			return;
		}
	}

	if( m_ropePinData.size() >= MAX_ROPES_TO_TRACK )
	{
		AssertMsg(false, "CReplayRopeManager::m_ropeData run out of space - probably not freeing up correctly");
		return;
	}

	sRopePinData &ropePinData = m_ropePinData.Append();
	ropePinData.idx = idx;
	ropePinData.pos = pos;
	ropePinData.pin = pin;
	ropePinData.rapelling = rapelling;
	ropePinData.ropeId = ropeID;
}

void CReplayRopeManager::ClearCachedPinRope(int ropeID, int idx)
{
	for (int i=m_ropePinData.size()-1; i>=0; i--)
	{
		if( m_ropePinData[i].ropeId == ropeID && (m_ropePinData[i].idx == idx || idx == -1) )
		{
			m_ropePinData.DeleteFast(i);
		}
	}
}

CReplayRopeManager::sRopePinData* CReplayRopeManager::GetLastPinRopeData(int ropeID, int idx)
{
	for( u32 i = 0; i < m_ropePinData.size(); i++)
	{
		if( m_ropePinData[i].ropeId == ropeID && m_ropePinData[i].idx == idx )
		{
			return &m_ropePinData[i];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CPacketAddRope::CPacketAddRope(int ropeID, Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, int numSections, bool ppuOnly, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned)
	: CPacketEventTracked(PACKETID_ADDROPE, sizeof(CPacketAddRope))
	, CPacketEntityInfo()
{
	m_Position.Store(pos);
	m_Rotation.Store(rot);
	m_InitLength = initLength;
	m_MinLength = minLength;
	m_MaxLength = maxLength;
	m_LengthChangeRate = lengthChangeRate;
	m_TimeMultiplier = timeMultiplier;
	m_RopeType = ropeType;
	m_NumSections = numSections;
	m_PpuOnly = ppuOnly;
	m_LockFromFront = lockFromFront;
	m_Breakable = breakable;
	m_Pinned = pinned;
	m_InitLength = Selectf(m_InitLength, m_InitLength, m_MaxLength );

	m_Reference = ropeID;
}

void CPacketAddRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	eventInfo.AllowSetStale(false);

	bool recreateRope = false;
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if(eventInfo.GetIsFirstFrame())
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
			if( pRope )
				return; 

			recreateRope = true;
		}
	}

	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && !recreateRope)
	{
		if(eventInfo.m_pEffect[0] > 0)
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if(pRopeManager)
			{
				pRopeManager->RemoveRope((int)eventInfo.m_pEffect[0]);
			}
		}
		return;
	}

	Vec3V pos;
	m_Position.Load(pos);

	Vec3V rot;
	m_Rotation.Load(rot);

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{		
		ropeInstance* pExistingRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD && pExistingRope)
		{
			return;
		}

		ropeInstance* pRope = pRopeManager->AddRope(pos, rot, m_InitLength, m_MinLength, m_MaxLength, m_LengthChangeRate, m_RopeType, m_NumSections, m_PpuOnly, m_LockFromFront, m_TimeMultiplier, m_Breakable, m_Pinned);		
		if(pRope)
		{
			pRope->DisableCollision(true);
			// TODO: disabled by svetli
			// ropes will be marked GTA_ENVCLOTH_OBJECT_TYPE by default
			// 			if( collisionOn)
			pRope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
			
			pRope->SetActivateObjects(false);

			eventInfo.m_pEffect[0] = (ptxEffectRef)pRope->SetNewUniqueId();

			//Make sure the rope edge data gets updated with the correct length.
			pRope->RopeWindFrontDirect(m_InitLength);
		}
	}
}

bool CPacketAddRope::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	return !pRopeManager->FindRope( m_Reference );
}

void CPacketAddRope::UpdateMonitorPacket()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	ropeInstance* pRope = pRopeManager->FindRope( m_Reference );
	if( pRope )
	{
		m_InitLength = pRope->GetLength();
	}
}



//////////////////////////////////////////////////////////////////////////
CPacketDeleteRope::CPacketDeleteRope(int ropeID)
	: CPacketEventTracked(PACKETID_DELETEROPE, sizeof(CPacketDeleteRope))
	, CPacketEntityInfo()
{
	CReplayRopeManager::ClearCachedPinRope(ropeID, -1);
}

void CPacketDeleteRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();

	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		CPhysical *pEntityA = static_cast<CPhysical*>(eventInfo.pEntity[0]);
		CPhysical *pEntityB = static_cast<CPhysical*>(eventInfo.pEntity[1]);
		if(pEntityA && pEntityA->GetIsPhysical() &&  pEntityA->GetCurrentPhysicsInst() &&
			pEntityB && pEntityB->GetIsPhysical() &&  pEntityB->GetCurrentPhysicsInst())
		{
			int ropeId = (int)eventInfo.m_pEffect[0];
			ropeInstance* pRope = pRopeManager->FindRope(ropeId);
			if( !pRope )
			{
				Vec3V rot = Vec3V( 0.0f, 0.0f, -0.5f*PI );
				int numSections = (int)Clamp(m_AddData.length, 4.0f, 16.0f);
				pRope = pRopeManager->AddRope(m_AddData.pos, rot, m_AddData.length, m_AddData.minLength, m_AddData.maxLength, m_AddData.lengthChangeRate, m_AddData.type, numSections, false, true, 1.0, m_AddData.isBreakable, false);
				pRope->DisableCollision(true);
				if(pRope)
				{
					pRope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
					pRope->SetActivateObjects(false);
					eventInfo.m_pEffect[0] = pRope->SetNewUniqueId();
				}
			}
		
			if( m_AddData.m_AttachData.m_HadContraint && !pRope->GetConstraint() )
			{
				Matrix34 mat = MAT34V_TO_MATRIX34(pEntityA->GetMatrix());
				Vector3 objectSpacePosA;
				m_AddData.m_AttachData.m_ObjectSpacePosA.Load(objectSpacePosA);
				mat.Transform(objectSpacePosA);

				mat = MAT34V_TO_MATRIX34(pEntityB->GetMatrix());
				Vector3 objectSpacePosB;
				m_AddData.m_AttachData.m_ObjectSpacePosB.Load(objectSpacePosB);
				mat.Transform(objectSpacePosB);

				pRope->AttachObjects(VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), pEntityA->GetCurrentPhysicsInst(), pEntityB->GetCurrentPhysicsInst(), m_AddData.m_AttachData.m_ComponentA, m_AddData.m_AttachData.m_ComponentB, m_AddData.m_AttachData.m_MaxDistance, m_AddData.m_AttachData.m_AllowPenetration, 1.0f, 1.0f, m_AddData.m_AttachData.m_UsePushes);
			}
		}

		return;
	}
	
	if(pRopeManager)
	{
		int ropeId = (int)eventInfo.m_pEffect[0];
		ropeInstance* pRope = pRopeManager->FindRope(ropeId);
		if(	pRope )		
		{
			m_AddData.length = pRope->GetLength();
			m_AddData.minLength = pRope->GetMinLength();
			m_AddData.maxLength = pRope->GetMaxLength();
			m_AddData.lengthChangeRate = pRope->GetLengthChangeRate();
			m_AddData.type = pRope->GetType();		
			m_AddData.isBreakable = pRope->IsBreakable();	
			m_AddData.pos.Store(pRope->GetWorldPositionA());

			phConstraintDistance* pConstraint = static_cast<phConstraintDistance*>(pRope->GetConstraint());
			m_AddData.m_AttachData.m_HadContraint = false;
			if(pConstraint)
			{
				m_AddData.m_AttachData.m_HadContraint = true;
				if(pConstraint->GetInstanceA() && pConstraint->GetInstanceB())
				{
					m_AddData.m_AttachData.m_ComponentA = pConstraint->GetComponentA();
					m_AddData.m_AttachData.m_ComponentB = pConstraint->GetComponentB();
					m_AddData.m_AttachData.m_MaxDistance = pConstraint->GetMaxDistance();
					m_AddData.m_AttachData.m_AllowPenetration = pConstraint->GetAllowedPenetration();
					m_AddData.m_AttachData.m_UsePushes = pConstraint->GetUsePushes();

					Matrix34 matInverse = MAT34V_TO_MATRIX34(pConstraint->GetInstanceA()->GetMatrix());
					matInverse.Inverse();
					Vector3 objectSpacePosA = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosA());
					matInverse.Transform(objectSpacePosA);

					matInverse = MAT34V_TO_MATRIX34(pConstraint->GetInstanceB()->GetMatrix());
					matInverse.Inverse();
					Vector3 objectSpacePosB = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosB());
					matInverse.Transform(objectSpacePosB);

					m_AddData.m_AttachData.m_ObjectSpacePosA.Store(objectSpacePosA);
					m_AddData.m_AttachData.m_ObjectSpacePosB.Store(objectSpacePosB);
				}
			}
		}

		pRopeManager->RemoveRope((int)ropeId);
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketAttachRopeToEntity_V1 = 1;
CPacketAttachRopeToEntity::CPacketAttachRopeToEntity(int ropeID, Vec3V_In worldPosition, int componentPart, int heliAttachment, const Vector3 localOffset)
	: CPacketEventTracked(PACKETID_ATTACHROPETOENTITY, sizeof(CPacketAttachRopeToEntity), CPacketAttachRopeToEntity_V1)
	, CPacketEntityInfoStaticAware()
{
	m_WorldPosition.Store(worldPosition);
	m_ComponentPart = componentPart;	
	m_Reference = ropeID;
	m_HeliAttachment = heliAttachment;
	m_LocalOffset.Store(localOffset);
}

void CPacketAttachRopeToEntity::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && eventInfo.m_pEffect[0] > 0)
	{
		if(eventInfo.GetIsFirstFrame())
			return; 

		ropeManager* pRopeManager = CPhysics::GetRopeManager();
		if(pRopeManager)
		{
			ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
			if(pRope) 
			{
				CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
				if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst())
				{
					pRope->DetachFromObject(pEntity->GetCurrentPhysicsInst());
				}
			}
		}
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{
			CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
			if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst() && !pRope->IsAttached(pEntity->GetCurrentPhysicsInst()))
			{
				Vec3V worldPosition;
				m_WorldPosition.Load(worldPosition);
				pRope->AttachToObject( worldPosition, pEntity->GetCurrentPhysicsInst(), m_ComponentPart, NULL, NULL );

				Vector3 localOffset;
				m_LocalOffset.Load(localOffset);
				pRope->SetLocalOffset(localOffset);
				if(m_HeliAttachment >= 0 && pEntity->GetIsTypeVehicle() && pRope)
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(eventInfo.pEntity[0]);
					if(pVehicle->InheritsFromHeli())
					{
						CHeli* pHeli = static_cast<CHeli*>(pVehicle);

						if(GetPacketVersion() >= CPacketAttachRopeToEntity_V1)
						{
							if(pHeli->GetRope((CHeli::eRopeID)m_HeliAttachment) == NULL)
							{
								pHeli->SetRope((CHeli::eRopeID)m_HeliAttachment, pRope);
							}
						}
						else
						{
							// Old clips only recorded 1-2 for driver and passenger ropes
 							if(m_HeliAttachment == 1 && pHeli->GetRope(true) == NULL)
 							{
 								pHeli->SetRope(true, pRope);
 							}
 							else if(m_HeliAttachment == 2 && pHeli->GetRope(false) == NULL)
 							{
 								pHeli->SetRope(false, pRope);
 							}
						}
					}
				}
			}
		}
	}
}

bool CPacketAttachRopeToEntity::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	return !pRopeManager->FindRope( m_Reference );
}


tPacketVersion gPacketAttachEntitiesToRope_v1 = 1;

//////////////////////////////////////////////////////////////////////////
CPacketAttachEntitiesToRope::CPacketAttachEntitiesToRope(int ropeID, Vec3V_In worldPositionA, Vec3V_In worldPositionB, int componentPartA, int componentPartB, float constraintLength, float allowedPenetration, 
														 float massInvScaleA, float massInvScaleB, bool usePushes, const char* boneNamePartA, const char* boneNamePartB, int towTruckRopeComponent, bool bReattachHook)
	: CPacketEventTracked(PACKETID_ATTACHENTITIESTOROPE, sizeof(CPacketAttachEntitiesToRope), gPacketAttachEntitiesToRope_v1)
	, CPacketEntityInfoStaticAware()
{
	m_WorldPositionA.Store(worldPositionA);
	m_WorldPositionB.Store(worldPositionB);
	m_ComponentPartA = componentPartA;
	m_ComponentPartB = componentPartB;
	m_ConstraintLength = constraintLength;
	m_AllowedPenetration = allowedPenetration;
	m_MassInvScaleA = massInvScaleA;
	m_MassInvScaleB = massInvScaleB;
	m_UsePushes = usePushes;
	strncpy(m_BoneNamePartA, boneNamePartA != NULL ? boneNamePartA : "", 64);
	strncpy(m_BoneNamePartB, boneNamePartB != NULL ? boneNamePartB : "", 64);
	m_Reference = ropeID;
	m_TowTruckRopeComponent = towTruckRopeComponent;
	m_bReattachHook = bReattachHook;
}

void CPacketAttachEntitiesToRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	bool reAttachHook = false;
	if( GetPacketVersion() >= gPacketAttachEntitiesToRope_v1 )
		reAttachHook = m_bReattachHook;

	//If we're reattaching the hook then call attach again when going backwards and dont detach it.
	if((eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK) && !reAttachHook)
	{
		if(eventInfo.GetIsFirstFrame())
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if(pRopeManager)
			{
				ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
				if(pRope && pRope->GetConstraint()) 
				{
					return; 
				}
			}			
		}
		else
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if(pRopeManager)
			{
				ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
				if(pRope) 
				{
					CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
					if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst())
					{
						pRope->DetachFromObject(pEntity->GetCurrentPhysicsInst());
					}
				}
			}
			return;
		}
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope && eventInfo.pEntity[0] && eventInfo.pEntity[1]) 
		{
			CEntity *pEntityA = static_cast<CEntity*>(eventInfo.pEntity[0]);
			CEntity *pEntityB = static_cast<CEntity*>(eventInfo.pEntity[1]);
			if(pEntityA && pEntityA->GetIsPhysical() &&  pEntityA->GetCurrentPhysicsInst() &&
			   pEntityB && pEntityB->GetIsPhysical() &&  pEntityB->GetCurrentPhysicsInst() )
			{
				//Handle the tow truck rope similar to the heli, allow the gadget to attach the rope its self.
				if(m_TowTruckRopeComponent != -1 && pEntityA->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntityA);
					if(pVehicle)
					{
						for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
						{
							CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

							if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
							{
								CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
								//Assuming the first component is ropeA, but it doesn't matter. 
								if(m_TowTruckRopeComponent == 1)
								{
									pTowArm->SetRopeA(pRope);
								}
								else if(m_TowTruckRopeComponent == 2)
								{
									pTowArm->SetRopeB(pRope);
								}

								//Add the prop object to the tow arm and get the gadget to attach the ropes as it would during recording
								if(pTowArm->GetRopeA() && pTowArm->GetRopeB())
								{
									CObject* pObject = static_cast<CObject*>(pEntityB);
									if(pTowArm->GetPropObject() && pTowArm->GetPropObject()->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY)
									{
										pTowArm->DeleteRopesHookAndConstraints();
											
										CObjectPopulation::DestroyObject(pTowArm->GetPropObject());
										pTowArm->SetPropObject(NULL);
									}
									replayAssert(pTowArm->GetPropObject() == NULL || pTowArm->GetPropObject() == pObject);
									pTowArm->SetPropObject(pObject);

									if(!eventInfo.GetIsFirstFrame())
									{
										pVehicle->WaitForAnyActiveAnimUpdateToComplete();
										if(pVehicle->GetSkeleton())
										{
											pVehicle->GetSkeleton()->InverseUpdate();
										}
									}

									pTowArm->AttachRopesToObject(pVehicle, pObject);
								}
								return;
							}
						}
					}
				}

				//Extract the world positions based off the entities current matrix
				Matrix34 mat = MAT34V_TO_MATRIX34(pEntityA->GetTransform().GetMatrix());
				Vector3 objectSpacePos;
				m_WorldPositionA.Load(objectSpacePos);
				mat.Transform(objectSpacePos);

				mat = MAT34V_TO_MATRIX34(pEntityB->GetTransform().GetMatrix());
				Vector3 objectSpacePosB;
				m_WorldPositionB.Load(objectSpacePosB);
				mat.Transform(objectSpacePosB);

				const char* boneNamePartA = NULL;
				const char* boneNamePartB = NULL;
				crSkeleton* pSkelPartA =  NULL;
				crSkeleton* pSkelPartB =  NULL;
				if(m_BoneNamePartA[0] != 0)
				{
					pSkelPartA = pEntityA->GetSkeleton();
					boneNamePartA = &m_BoneNamePartA[0];
				}

				if(m_BoneNamePartB[0] != 0)
				{
					pSkelPartB = pEntityB->GetSkeleton();
					boneNamePartB = &m_BoneNamePartB[0];
				}

				pRope->AttachObjects(VECTOR3_TO_VEC3V(objectSpacePos), VECTOR3_TO_VEC3V(objectSpacePosB), pEntityA->GetCurrentPhysicsInst(), pEntityB->GetCurrentPhysicsInst(), m_ComponentPartA, m_ComponentPartB, m_ConstraintLength, 0.0f, 1.0f, 1.0f, m_UsePushes, pSkelPartA, pSkelPartB, boneNamePartA, boneNamePartB);				
			}
		}
	}
}

bool CPacketAttachEntitiesToRope::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	ropeInstance* pRope = pRopeManager->FindRope( m_Reference );
	if( pRope && pRope->GetConstraint() )
	{
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
CPacketAttachObjectsToRopeArray::CPacketAttachObjectsToRopeArray(int ropeID, Vec3V_In worldPositionA, Vec3V_In worldPositionB, float constraintLength, float constraintChangeRate)
	: CPacketEventTracked(PACKETID_ATTACHOBJECTSTOROPEARRAY, sizeof(CPacketAttachObjectsToRopeArray))
	, CPacketEntityInfoStaticAware()
{

	m_WorldPositionA.Store(worldPositionA);
	m_WorldPositionB.Store(worldPositionB);
	m_ConstraintLength = constraintLength;
	m_ConstraintChangeRate = constraintChangeRate;
	m_Reference = ropeID;
}

void CPacketAttachObjectsToRopeArray::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && eventInfo.m_pEffect[0] > 0)
	{
		if(eventInfo.m_pEffect[1])
		{
			ropeAttachment* pRopeAttachment = reinterpret_cast<ropeAttachment*>(eventInfo.m_pEffect[1]);
			if(pRopeAttachment)
			{
				pRopeAttachment->markedForDetach = true;
			}
		}
		return;
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{
			CPhysical *pEntityA = static_cast<CPhysical*>(eventInfo.pEntity[0]);
			CPhysical *pEntityB = static_cast<CPhysical*>(eventInfo.pEntity[1]);
			if(pEntityA && pEntityA->GetIsPhysical() &&  pEntityA->GetCurrentPhysicsInst() &&
			   pEntityB && pEntityB->GetIsPhysical() &&  pEntityB->GetCurrentPhysicsInst())
			{
				Vec3V worldPositionA;
				m_WorldPositionA.Load(worldPositionA);

				Vec3V worldPositionB;
				m_WorldPositionB.Load(worldPositionB);
			
				ropeAttachment* pRopeAttachment = &pRope->AttachObjectsToConstraintArray(worldPositionA, worldPositionB, pEntityA->GetCurrentPhysicsInst(), pEntityB->GetCurrentPhysicsInst(), 0, 0, m_ConstraintLength, m_ConstraintChangeRate);

				eventInfo.m_pEffect[1] = (ptxEffectRef)pRopeAttachment;
				if(pRopeAttachment)
				{
					pRopeAttachment->enableMovement = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketDetachRopeFromEntity::CPacketDetachRopeFromEntity(bool detachUsingAttachments)
	: CPacketEventTracked(PACKETID_DETACHROPEFROMENTITY, sizeof(CPacketDetachRopeFromEntity))
	, CPacketEntityInfoStaticAware()
{
	m_bDetachUsingAttachments = detachUsingAttachments;
}

void CPacketDetachRopeFromEntity::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		ropeManager* pRopeManager = CPhysics::GetRopeManager();
		if(pRopeManager)
		{
			ropeInstance* pRope = NULL;
			if( eventInfo.m_pEffect[0] > 0 )
				pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );

			if(pRope) 
			{
				if( m_AttachData.m_HadContraint )
				{
					CPhysical *pEntityA = static_cast<CPhysical*>(eventInfo.pEntity[0]);
					CPhysical *pEntityB = static_cast<CPhysical*>(eventInfo.pEntity[1]);
					if(pEntityA && pEntityA->GetIsPhysical() &&  pEntityA->GetCurrentPhysicsInst() &&
						pEntityB && pEntityB->GetIsPhysical() &&  pEntityB->GetCurrentPhysicsInst())
					{
						Matrix34 mat = MAT34V_TO_MATRIX34(pEntityA->GetMatrix());
						Vector3 objectSpacePosA;
						m_AttachData.m_ObjectSpacePosA.Load(objectSpacePosA);
						mat.Transform(objectSpacePosA);

						mat = MAT34V_TO_MATRIX34(pEntityB->GetMatrix());
						Vector3 objectSpacePosB;
						m_AttachData.m_ObjectSpacePosB.Load(objectSpacePosB);
						mat.Transform(objectSpacePosB);

						pRope->AttachObjects(VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), pEntityA->GetCurrentPhysicsInst(), pEntityB->GetCurrentPhysicsInst(), m_AttachData.m_ComponentA, m_AttachData.m_ComponentB, m_AttachData.m_MaxDistance, m_AttachData.m_AllowPenetration, 1.0f, 1.0f, m_AttachData.m_UsePushes);
					}
				}
				else
				{
					//Attached at one end, not sure its needed?
				}					
			}
		}
		return;
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{

			if(m_bDetachUsingAttachments )
			{
				if(eventInfo.m_pEffect[1])
				{
					ropeAttachment* pRopeAttachment = reinterpret_cast<ropeAttachment*>(eventInfo.m_pEffect[1]);
					if(pRopeAttachment)
					{
						pRopeAttachment->markedForDetach = true;
					}
				}
			}
			else
			{
				CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
				if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst())
				{
					phConstraintDistance* pConstraint = static_cast<phConstraintDistance*>(pRope->GetConstraint());
					m_AttachData.m_HadContraint = false;
					if(pConstraint)
					{
						m_AttachData.m_HadContraint = true;
						if(pConstraint->GetInstanceA() && pConstraint->GetInstanceB())
						{
							m_AttachData.m_ComponentA = pConstraint->GetComponentA();
							m_AttachData.m_ComponentB = pConstraint->GetComponentB();
							m_AttachData.m_MaxDistance = pConstraint->GetMaxDistance();
							m_AttachData.m_AllowPenetration = pConstraint->GetAllowedPenetration();
							m_AttachData.m_UsePushes = pConstraint->GetUsePushes();

							Matrix34 matInverse = MAT34V_TO_MATRIX34(pConstraint->GetInstanceA()->GetMatrix());
							matInverse.Inverse();
							Vector3 objectSpacePosA = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosA());
							matInverse.Transform(objectSpacePosA);

							matInverse = MAT34V_TO_MATRIX34(pConstraint->GetInstanceB()->GetMatrix());
							matInverse.Inverse();
							Vector3 objectSpacePosB = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosB());
							matInverse.Transform(objectSpacePosB);

							m_AttachData.m_ObjectSpacePosA.Store(objectSpacePosA);
							m_AttachData.m_ObjectSpacePosB.Store(objectSpacePosB);
						}
					}
 					else if( pRope->GetAttachedTo())
 					{
	 					//Attached at one end, not sure its needed?
 					}

					pRope->DetachFromObject( pEntity->GetCurrentPhysicsInst() );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion gPacketPinRope_v1 = 1;

CPacketPinRope::CPacketPinRope(int idx, Vec3V_In pos, int ropeID)
	: CPacketEventTracked(PACKETID_PINROPE, sizeof(CPacketPinRope), gPacketPinRope_v1)
	, CPacketEntityInfo()
{
	m_Pos.Store(pos);
	m_Idx = idx;

	m_hasPinData = false;

	CReplayRopeManager::sRopePinData *pPinData = CReplayRopeManager::GetLastPinRopeData(ropeID, idx);
	if( pPinData )
	{
		m_PinData.idx = pPinData->idx;
		m_PinData.pin = pPinData->pin;
		m_PinData.pos = pPinData->pos;
		m_PinData.rapelling = pPinData->rapelling;
		m_PinData.ropeId = pPinData->ropeId;
		m_hasPinData = true;
	}
	CReplayRopeManager::CachePinRope(idx, VEC3V_TO_VECTOR3(pos), false, ropeID, false);	
}

void CPacketPinRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( GetPacketVersion() >= gPacketPinRope_v1 )
		{
			if( m_hasPinData )
			{
				Vec3V pos;
				m_PinData.pos.Load(pos);
				CPacketPinRope::Pin(m_PinData.idx, pos, (int)eventInfo.m_pEffect[0] );
			}
			else
			{
				CPacketUnPinRope::UnPin(m_Idx, (int)eventInfo.m_pEffect[0]);
			}
		}

		return;
	}

	Vec3V pos;
	m_Pos.Load(pos);
	Pin(m_Idx, pos, (int)eventInfo.m_pEffect[0]);
}

void CPacketPinRope::Pin(int idx, Vec3V pos, int ropeId)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeId );
		if(pRope) 
		{
			pRope->Pin(idx, pos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion gPacketRappelPinRope_v1 = 1;

CPacketRappelPinRope::CPacketRappelPinRope(int idx, const Vector3& pos, bool pin, int ropeID)
	: CPacketEventTracked(PACKETID_RAPPELPINROPE, sizeof(CPacketRappelPinRope), gPacketRappelPinRope_v1)
	, CPacketEntityInfo()
{	
	m_Idx = idx;
	m_Pos.Store(pos);
	m_bPin = (u8)pin;	
	m_hasPinData = false;
	CReplayRopeManager::sRopePinData *pPinData = CReplayRopeManager::GetLastPinRopeData(ropeID, idx);
	if( pPinData )
	{
		m_PinData.idx = pPinData->idx;
		m_PinData.pin = pPinData->pin;
		m_PinData.pos = pPinData->pos;
		m_PinData.rapelling = pPinData->rapelling;
		m_PinData.ropeId = pPinData->ropeId;
		m_hasPinData = true;
	}
	CReplayRopeManager::CachePinRope(idx, pos, pin, ropeID, true);
}

void CPacketRappelPinRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( GetPacketVersion() >= gPacketRappelPinRope_v1 )
		{
			if( m_hasPinData )
			{
				Vec3V pos;
				m_PinData.pos.Load(pos);
				CPacketRappelPinRope::Pin(m_PinData.idx, m_PinData.pin, pos, (int)eventInfo.m_pEffect[0]);
			}
			else
			{
				CPacketUnPinRope::UnPin(m_Idx, (int)eventInfo.m_pEffect[0]);
			}
		}
		return;
	}

	Vec3V pos;
	m_Pos.Load(pos);
	Pin(m_Idx, m_bPin, pos, (int)eventInfo.m_pEffect[0]);
}

void CPacketRappelPinRope::Pin(int idx, bool bPin, Vec3V pos, int ropeId)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeId );
		if(pRope) 
		{
			if(bPin == 1)
			{
				pRope->Pin(1, pos);
			}
			else
			{
				Assert( pRope->GetEnvCloth() && pRope->GetEnvCloth()->GetCloth() );

				phClothData& clotData = pRope->GetEnvCloth()->GetCloth()->GetClothData();

				Vec3V v0 = clotData.GetVertexPosition(idx);
				Vec3V v1 = clotData.GetVertexPosition(idx-1);
				ScalarV v0Z = v0.GetZ();
				ScalarV v1Z = v1.GetZ();
				v0 = pos;
				v1 = pos;
				v0.SetZ(v0Z);
				v1.SetZ(v1Z);
				clotData.SetVertexPosition(idx, v0);
				clotData.SetVertexPosition(idx-1, v1);
				if( idx > 3 )
				{
					Vec3V v2 = clotData.GetVertexPosition(idx-2);
					ScalarV v2Z = v2.GetZ();
					v2 = pos;
					v2.SetZ(v2Z);
					clotData.SetVertexPosition(idx-2, v2);
				}
			}

		}
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion gPacketUnPinRope_v1 = 1;

CPacketUnPinRope::CPacketUnPinRope(int idx, int ropeID)
	: CPacketEventTracked(PACKETID_UNPINROPE, sizeof(CPacketUnPinRope), gPacketUnPinRope_v1)
	, CPacketEntityInfo()
{	
	m_Idx = idx;

	m_hasPinData = false;
	CReplayRopeManager::sRopePinData *pPinData = CReplayRopeManager::GetLastPinRopeData(ropeID, idx);
	if( pPinData )
	{
		m_PinData.idx = pPinData->idx;
		m_PinData.pin = pPinData->pin;
		m_PinData.pos = pPinData->pos;
		m_PinData.rapelling = pPinData->rapelling;
		m_PinData.ropeId = pPinData->ropeId;
		m_hasPinData = true;
	}
	CReplayRopeManager::ClearCachedPinRope(ropeID, idx);
}

void CPacketUnPinRope::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( GetPacketVersion() >= gPacketUnPinRope_v1 )
		{
			if( m_hasPinData )
			{
				Vec3V pos;
				m_PinData.pos.Load(pos);
				if( m_PinData.rapelling )
					CPacketRappelPinRope::Pin(m_PinData.idx, m_PinData.pin, pos, (int)eventInfo.m_pEffect[0]);
				else
					CPacketPinRope::Pin(m_PinData.idx, pos, (int)eventInfo.m_pEffect[0] );
			}
		}
		return;
	}

	UnPin(m_Idx, (int)eventInfo.m_pEffect[0]);
}

void CPacketUnPinRope::UnPin(int idx, int ropeID)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if(pRope) 
		{
			if(idx == -1)
			{
				pRope->UnpinAllVerts();
				return;
			}

			pRope->UnPin(idx);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketLoadRopeData::CPacketLoadRopeData(int ropeID, const char * filename)
	: CPacketEventTracked(PACKETID_LOADROPEDATA, sizeof(CPacketLoadRopeData))
	, CPacketEntityInfo()
{
	memset(m_Filename, 0, sizeof(m_Filename)); 
	strncpy(m_Filename, filename, 64);
	m_Reference = ropeID;
}

void CPacketLoadRopeData::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{
			pRope->Load(m_Filename);
		}
	}
}

bool CPacketLoadRopeData::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	return !pRopeManager->FindRope( m_Reference );
}


//////////////////////////////////////////////////////////////////////////
CPacketRopeWinding::CPacketRopeWinding(float lengthRate, bool windFront, bool unwindFront, bool unwindback)
	: CPacketEventTracked(PACKETID_ROPEWINDING, sizeof(CPacketRopeWinding))
	, CPacketEntityInfo()
{
	m_LengthRate = lengthRate;
	m_RopeWindFront = windFront;	
	m_RopeUnwindFront = unwindFront;
	m_RopeUnwindBack = unwindback;
}

void CPacketRopeWinding::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && eventInfo.m_pEffect[0] > 0)
	{
		return;
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{

			replayFatalAssertf((m_RopeWindFront + m_RopeUnwindFront + m_RopeUnwindBack) <= 1, "CPacketRopeWinding::Extract doesn't support this.");
			if(m_RopeWindFront == 1)
				pRope->StartWindingFront(m_LengthRate);
			else
				pRope->StopWindingFront();

			if(m_RopeUnwindFront == 1)
				pRope->StartUnwindingFront(m_LengthRate);
			else
				pRope->StopUnwindingFront();

			if(m_RopeUnwindBack == 1)
				pRope->StartUnwindingBack(m_LengthRate);
			else
				pRope->StopUnwindingBack();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketRopeUpdateOrder::CPacketRopeUpdateOrder(int idx, int updateOrder)
	: CPacketEventTracked(PACKETID_ROPEUPDATEORDER, sizeof(CPacketRopeUpdateOrder))
	, CPacketEntityInfo()
{
	m_UpdateOrder = updateOrder;
	m_Reference = idx;

}

void CPacketRopeUpdateOrder::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{
			//Set the update order of the rope to match recording
			pRope->SetUpdateOrder((enRopeUpdateOrder) m_UpdateOrder);
		}
	}
}

bool CPacketRopeUpdateOrder::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	return !pRopeManager->FindRope( m_Reference );
}

//////////////////////////////////////////////////////////////////////////
/// OLD VERSION TO REMOVE !!
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CPacketAddRope_OLD::CPacketAddRope_OLD(int ropeID, Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, int numSections, bool ppuOnly, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned)
	: CPacketEventTracked(PACKETID_ADDROPE_OLD, sizeof(CPacketAddRope_OLD))
	, CPacketEntityInfo()
{
	m_Position.Store(pos);
	m_Rotation.Store(rot);
	m_InitLength = initLength;
	m_MinLength = minLength;
	m_MaxLength = maxLength;
	m_LengthChangeRate = lengthChangeRate;
	m_TimeMultiplier = timeMultiplier;
	m_RopeType = ropeType;
	m_NumSections = numSections;
	m_PpuOnly = ppuOnly;
	m_LockFromFront = lockFromFront;
	m_Breakable = breakable;
	m_Pinned = pinned;
	m_InitLength = Selectf(m_InitLength, m_InitLength, m_MaxLength );

	m_Reference = ropeID;
}

void CPacketAddRope_OLD::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if(eventInfo.GetIsFirstFrame())
		{
			eventInfo.m_pEffect[0] = (ptxEffectRef)m_Reference;
			return; 
		}

		if(m_Reference > 0)
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if(pRopeManager)
			{
				pRopeManager->RemoveRope(m_Reference);
			}

			eventInfo.m_pEffect[0] = 0;
			m_Reference = 0;
		}
		return;
	}

	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD && m_Reference > 0)
	{
		return;
	}

	Vec3V pos;
	m_Position.Load(pos);

	Vec3V rot;
	m_Rotation.Load(rot);

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{		

		ropeInstance* pRope = pRopeManager->AddRope(pos, rot, m_InitLength, m_MinLength, m_MaxLength, m_LengthChangeRate, m_RopeType, m_NumSections, m_PpuOnly, m_LockFromFront, m_TimeMultiplier, m_Breakable, m_Pinned);
		pRope->DisableCollision(true);
		if(pRope)
		{
			// TODO: disabled by svetli
			// ropes will be marked GTA_ENVCLOTH_OBJECT_TYPE by default
			// 			if( collisionOn)
			pRope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
			
			pRope->SetActivateObjects(false);

			m_Reference = pRope->SetNewUniqueId();
		}
	}

	eventInfo.m_pEffect[0] = (ptxEffectRef)m_Reference;
}

bool CPacketAddRope_OLD::HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	return !pRopeManager->FindRope( m_Reference );
}

eShouldExtract CPacketAddRope_OLD::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const		
{	
	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);	
}

//////////////////////////////////////////////////////////////////////////
CPacketDeleteRope_OLD::CPacketDeleteRope_OLD()
	: CPacketEventTracked(PACKETID_DELETEROPE_OLD, sizeof(CPacketDeleteRope_OLD))
	, CPacketEntityInfo()
{
}

void CPacketDeleteRope_OLD::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && eventInfo.m_pEffect[0] > 0)
	{
		return;
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager && eventInfo.m_pEffect[0] > 0)
	{
		pRopeManager->RemoveRope((int)eventInfo.m_pEffect[0]);
		eventInfo.m_pEffect[0] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketDetachRopeFromEntity_OLD::CPacketDetachRopeFromEntity_OLD(bool detachUsingAttachments)
	: CPacketEventTracked(PACKETID_DETACHROPEFROMENTITY_OLD, sizeof(CPacketDetachRopeFromEntity_OLD))
	, CPacketEntityInfoStaticAware()
{
	m_bDetachUsingAttachments = detachUsingAttachments;
}

void CPacketDetachRopeFromEntity_OLD::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		// Try and attach the rope again
		if(eventInfo.m_pEffect[0] > 0)
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if(pRopeManager)
			{
				ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
				if(pRope) 
				{
					CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
					if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst() && !pRope->IsAttached(pEntity->GetCurrentPhysicsInst())
						&& m_AttachData.m_InstanceA && m_AttachData.m_InstanceB)
					{
						Matrix34 mat = MAT34V_TO_MATRIX34(m_AttachData.m_InstanceA->GetMatrix());
						Vector3 objectSpacePosA;
						m_AttachData.m_ObjectSpacePosA.Load(objectSpacePosA);
						mat.Transform(objectSpacePosA);

						mat = MAT34V_TO_MATRIX34(m_AttachData.m_InstanceB->GetMatrix());
						Vector3 objectSpacePosB;
						m_AttachData.m_ObjectSpacePosB.Load(objectSpacePosB);
						mat.Transform(objectSpacePosB);

						pRope->AttachObjects(VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), m_AttachData.m_InstanceA, m_AttachData.m_InstanceB, m_AttachData.m_ComponentA, m_AttachData.m_ComponentB, m_AttachData.m_MaxDistance, m_AttachData.m_AllowPenetration, 1.0f, 1.0f, m_AttachData.m_UsePushes);
					}
				}
			}
		}
		return;
	}

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = pRopeManager->FindRope( (int)eventInfo.m_pEffect[0] );
		if(pRope) 
		{

			if(m_bDetachUsingAttachments )
			{
				if(eventInfo.m_pEffect[1])
				{
					ropeAttachment* pRopeAttachment = reinterpret_cast<ropeAttachment*>(eventInfo.m_pEffect[1]);
					if(pRopeAttachment)
					{
						pRopeAttachment->markedForDetach = true;
					}
				}
			}
			else
			{
				CPhysical *pEntity = static_cast<CPhysical*>(eventInfo.pEntity[0]);
				if(pEntity && pEntity->GetIsPhysical() &&  pEntity->GetCurrentPhysicsInst())
				{
					phConstraintDistance* pConstraint = static_cast<phConstraintDistance*>(pRope->GetConstraint());
					if(pConstraint)
					{
						m_AttachData.m_InstanceA = pConstraint->GetInstanceA();
						m_AttachData.m_InstanceB = pConstraint->GetInstanceB();

						if(m_AttachData.m_InstanceA && m_AttachData.m_InstanceB)
						{
							m_AttachData.m_ComponentA = pConstraint->GetComponentA();
							m_AttachData.m_ComponentB = pConstraint->GetComponentB();
							m_AttachData.m_MaxDistance = pConstraint->GetMaxDistance();
							m_AttachData.m_AllowPenetration = pConstraint->GetAllowedPenetration();
							m_AttachData.m_UsePushes = pConstraint->GetUsePushes();

							Matrix34 matInverse = MAT34V_TO_MATRIX34(m_AttachData.m_InstanceA->GetMatrix());
							matInverse.Inverse();
							Vector3 objectSpacePosA = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosA());
							matInverse.Transform(objectSpacePosA);

							matInverse = MAT34V_TO_MATRIX34(m_AttachData.m_InstanceB->GetMatrix());
							matInverse.Inverse();
							Vector3 objectSpacePosB = VEC3V_TO_VECTOR3(pConstraint->GetWorldPosB());
							matInverse.Transform(objectSpacePosB);

							m_AttachData.m_ObjectSpacePosA.Store(objectSpacePosA);
							m_AttachData.m_ObjectSpacePosB.Store(objectSpacePosB);
						}
					}
						
					pRope->DetachFromObject( pEntity->GetCurrentPhysicsInst() );
				}
			}
		}
	}
}

#endif // GTA_REPLAY
