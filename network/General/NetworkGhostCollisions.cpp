//
// filename:    NetworkGhostCollisions.cpp
// description: Used to resolve collisions between ex-ghost (ie passive mode) vehicles
// written by:  John Gurney
//

// --- Include Files ------------------------------------------------------------
#include "NetworkGhostCollisions.h"

// C includes
#include <functional>

// game includes
#include "Network/Objects/Entities/NetObjPhysical.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "peds/ped.h"
#include "vehicles/trailer.h"
#include "vehicles/vehicle.h"

// framework includes
#include "fwsys/timer.h"
#include "fwnet/netlogutils.h"

RAGE_DECLARE_CHANNEL(ghost)
RAGE_DEFINE_CHANNEL(ghost)

#define ghostAssert(cond)						RAGE_ASSERT(ghost,cond)
#define ghostAssertf(cond,fmt,...)				RAGE_ASSERTF(ghost,cond,fmt,##__VA_ARGS__)
#define ghostFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(ghost,cond,fmt,##__VA_ARGS__)
#define ghostVerify(cond)						RAGE_VERIFY(ghost,cond)
#define ghostVerifyf(cond,fmt,...)				RAGE_VERIFYF(ghost,cond,fmt,##__VA_ARGS__)
#define ghostErrorf(fmt,...)					RAGE_ERRORF(ghost,fmt,##__VA_ARGS__)
#define ghostWarningf(fmt,...)					RAGE_WARNINGF(ghost,fmt,##__VA_ARGS__)
#define ghostDisplayf(fmt,...)					RAGE_DISPLAYF(ghost,fmt,##__VA_ARGS__)
#define ghostDebugf1(fmt,...)					RAGE_DEBUGF1(ghost,fmt,##__VA_ARGS__)
#define ghostDebugf2(fmt,...)					RAGE_DEBUGF2(ghost,fmt,##__VA_ARGS__)
#define ghostDebugf3(fmt,...)					RAGE_DEBUGF3(ghost,fmt,##__VA_ARGS__)
#define ghostLogf(severity,fmt,...)				RAGE_LOGF(ghost,severity,fmt,##__VA_ARGS__)
#define ghostCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ghost,severity,fmt,##__VA_ARGS__)

//////////////////////////////////////////////////////////
// CNetworkGhostCollisions::CGhostCollision
//////////////////////////////////////////////////////////

CNetworkGhostCollisions::CGhostCollision CNetworkGhostCollisions::ms_aGhostCollisions[MAX_GHOST_COLLISIONS];

CNetObjPhysical* CNetworkGhostCollisions::CGhostCollision::GetPhysicalNetObj() const 
{
	return m_pPhysical ? static_cast<CNetObjPhysical*>(m_pPhysical->GetNetworkObject()) : NULL; 
}

const char* CNetworkGhostCollisions::CGhostCollision::GetPhysicalLogName() const
{ 
	CNetObjPhysical* pNetObj = GetPhysicalNetObj(); 

	if (pNetObj)
	{
		return pNetObj->GetLogName();
	}

	return "?"; 
}

void CNetworkGhostCollisions::CGhostCollision::Clear()
{
	CNetObjPhysical* pNetObj = GetPhysicalNetObj();

	if (pNetObj)
	{
		pNetObj->ClearGhostCollisionSlot();
	}

	m_pPhysical = NULL;
	m_collisionFlags = 0;
	m_frameAdded = 0;
	m_numFreeAttempts = 0;
}

//////////////////////////////////////////////////////////
// CNetworkGhostCollisions
//////////////////////////////////////////////////////////
bool CNetworkGhostCollisions::IsAGhostCollision(const CPhysical& physical1, const CPhysical& physical2)
{
	bool bGhostCollision = false;

	// is the collision already registered?
	if (IsInGhostCollision(physical1, physical2))
	{
		bGhostCollision = true;
	}
	else
	{
		if (physical1.GetIsTypePed() && physical2.GetIsTypePed())
		{
			bGhostCollision = IsAGhostCollision((const CPed&)physical1, (const CPed&)physical2);
		}
		else
		{
			const CPed* pPed = nullptr;
			const CVehicle* pVehicle = nullptr;

			if (physical1.GetIsTypePed() || physical2.GetIsTypePed())
			{
				if (physical1.GetIsTypePed())
				{
					pPed = SafeCast(const CPed, &physical1);
					const CObject* pPhysical2Object = physical2.GetIsTypeObject() ? SafeCast(const CObject, &physical2) : nullptr;

					if (pPed->IsPlayer() && pPed->GetNetworkObject() && SafeCast(CNetObjPhysical, pPed->GetNetworkObject())->IsGhost() &&
						physical2.GetNetworkObject() && SafeCast(CNetObjPhysical, physical2.GetNetworkObject())->IsGhostedForGhostPlayers())
					{
						bGhostCollision = true;
					}
					else if (physical2.GetIsTypeVehicle())
					{
						pVehicle = SafeCast(const CVehicle, &physical2);
					}
					else if(pPhysical2Object && pPhysical2Object->m_nObjectFlags.bIsNetworkedFragment && pPhysical2Object->GetFragParent() && pPhysical2Object->GetFragParent()->GetIsTypeVehicle())
					{
						pVehicle = SafeCast(const CVehicle, pPhysical2Object->GetFragParent());
					}
					else
					{
						const CEntity* pAttachParent = SafeCast(const CEntity, physical2.GetAttachParent());

						if (pAttachParent && pAttachParent->GetIsTypeVehicle())
						{
							pVehicle = SafeCast(const CVehicle, pAttachParent);
						}
					}
				}
				else if (physical2.GetIsTypePed())
				{
					pPed = SafeCast(const CPed, &physical2);
					const CObject* pPhysical1Object = physical2.GetIsTypeObject() ? SafeCast(const CObject, &physical2) : nullptr;

					if (pPed->IsPlayer() && pPed->GetNetworkObject() && SafeCast(CNetObjPhysical, pPed->GetNetworkObject())->IsGhost() &&
						physical1.GetNetworkObject() && SafeCast(CNetObjPhysical, physical1.GetNetworkObject())->IsGhostedForGhostPlayers())
					{
						bGhostCollision = true;
					}
					else if (physical1.GetIsTypeVehicle())
					{
						pVehicle = SafeCast(const CVehicle, &physical1);
					}
					else if(pPhysical1Object && pPhysical1Object->m_nObjectFlags.bIsNetworkedFragment && pPhysical1Object->GetFragParent() && pPhysical1Object->GetFragParent()->GetIsTypeVehicle())
					{
						pVehicle = SafeCast(const CVehicle, pPhysical1Object->GetFragParent());
					}
					else
					{
						const CEntity* pAttachParent = SafeCast(const CEntity, physical1.GetAttachParent());

						if (pAttachParent && pAttachParent->GetIsTypeVehicle())
						{
							pVehicle = SafeCast(const CVehicle, pAttachParent);
						}
					}
				}
			}
	
			if (!bGhostCollision)
			{
				if (pPed && pVehicle)
				{
					bGhostCollision = IsAGhostCollision(*pPed, *pVehicle);
				}
				else
				{
					const CVehicle* pVehicle1 = physical1.GetIsTypeVehicle() ? (const CVehicle*)&physical1 : nullptr;
					const CVehicle* pVehicle2 = physical2.GetIsTypeVehicle() ? (const CVehicle*)&physical2 : nullptr;

					if (!pVehicle1 && physical1.GetIsTypeObject())
					{
						const CEntity* pAttachParent =  static_cast<const CEntity*>(physical1.GetAttachParent());

						if (pAttachParent && pAttachParent->GetIsTypeVehicle())
						{
							pVehicle1 = static_cast<const CVehicle*>(pAttachParent);
						}
					}

					if (!pVehicle2 && physical2.GetIsTypeObject())
					{
						const CEntity* pAttachParent =  static_cast<const CEntity*>(physical2.GetAttachParent());

						if (pAttachParent && pAttachParent->GetIsTypeVehicle())
						{
							pVehicle2 = static_cast<const CVehicle*>(pAttachParent);
						}
					}

					CPed* pDriver1 = pVehicle1 ? GetDriverFromVehicle(*pVehicle1) : nullptr;
					CPed* pDriver2 = pVehicle2 ? GetDriverFromVehicle(*pVehicle2) : nullptr;

					if (pVehicle1 && pVehicle2 && pVehicle1 != pVehicle2)
					{
						if (pDriver1 && pDriver2 && pDriver1 != pDriver2 && IsAGhostCollision(*pDriver1, *pDriver2))
						{
							bGhostCollision = true;
						}
						else if (pDriver1)
						{
							bGhostCollision = IsAGhostCollision(*pDriver1, *pVehicle2);	
						}
						else if (pDriver2)
						{
							bGhostCollision = IsAGhostCollision(*pDriver2, *pVehicle1);	
						}
					}
					else if (pVehicle1 && physical2.GetNetworkObject() && static_cast<CNetObjPhysical*>(physical2.GetNetworkObject())->IsGhostedForGhostPlayers())
					{
						if (pDriver1 && pDriver1->IsPlayer() && pDriver1->GetNetworkObject() && static_cast<CNetObjPhysical*>(pDriver1->GetNetworkObject())->IsGhost())
						{
							bGhostCollision = true;
						}
					}
					else if (pVehicle2 && physical1.GetNetworkObject() && static_cast<CNetObjPhysical*>(physical1.GetNetworkObject())->IsGhostedForGhostPlayers())
					{
						if (pDriver2 && pDriver2->IsPlayer() && pDriver2->GetNetworkObject() && static_cast<CNetObjPhysical*>(pDriver2->GetNetworkObject())->IsGhost())
						{
							bGhostCollision = true;
						}
					}
				}
			}
		}
	}

	return bGhostCollision;
}

bool CNetworkGhostCollisions::IsInGhostCollision(const CPhysical& physical1, const CPhysical& physical2)
{
	CNetObjPhysical* netObj1 = static_cast<CNetObjPhysical*>(physical1.GetNetworkObject());
	CNetObjPhysical* netObj2 = static_cast<CNetObjPhysical*>(physical2.GetNetworkObject());

	if (netObj1 && netObj2)
	{
		int ghostSlot1 = netObj1->GetGhostCollisionSlot();
		int ghostSlot2 = netObj2->GetGhostCollisionSlot();

		if (ghostSlot1 != INVALID_GHOST_COLLISION_SLOT && ghostSlot2 != INVALID_GHOST_COLLISION_SLOT)
		{
			if (ms_aGhostCollisions[ghostSlot1].IsCollidingWith(ghostSlot2))
			{
				ghostAssert(ms_aGhostCollisions[ghostSlot2].IsCollidingWith(ghostSlot1));
				return true;
			}
		}
	}

	return false;
}

bool CNetworkGhostCollisions::IsInGhostCollision(const CPhysical& physical)
{
	for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
	{
		if (ms_aGhostCollisions[i].GetPhysical() == &physical)
		{
			return true;
		}
	}

	return false;
}

bool CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(const CPhysical& physical) 
{
	if (NetworkInterface::IsGameInProgress())
	{
		CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

		CPhysical* pLocalPhysical = pLocalPlayer;

		if (pLocalPlayer->GetVehiclePedInside())
		{
			pLocalPhysical = pLocalPlayer->GetVehiclePedInside();
		}

		// don't show the entity ghosted if a collision with the local player is not treated as a ghost collision
		if (pLocalPhysical && CNetworkGhostCollisions::IsAGhostCollision(*pLocalPhysical, physical))
		{
			return true;
		}
	}

	return false;
}

bool CNetworkGhostCollisions::IsLocalPlayerInAnyGhostCollision() 
{
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

	CPhysical* pLocalPhysical = pLocalPlayer;

	if (pLocalPlayer->GetVehiclePedInside())
	{
		pLocalPhysical = pLocalPlayer->GetVehiclePedInside();
	}

	for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
	{
		if (ms_aGhostCollisions[i].GetPhysical() == pLocalPhysical)
		{
			return true;
		}
	}

	return false;
}

void CNetworkGhostCollisions::GetAllGhostCollisions(const CPhysical& physical, const CPhysical* collisions[MAX_GHOST_COLLISIONS], u32& numCollisions)
{
	numCollisions = 0;

	CNetObjPhysical* netObj = static_cast<CNetObjPhysical*>(physical.GetNetworkObject());

	if (netObj)
	{
		int ghostSlot = netObj->GetGhostCollisionSlot();

		if (ghostSlot != INVALID_GHOST_COLLISION_SLOT)
		{
			for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
			{
				if ((int)i!=ghostSlot && ms_aGhostCollisions[ghostSlot].IsCollidingWith(i) && ghostVerify(ms_aGhostCollisions[i].IsSet()))
				{
					collisions[numCollisions++] = ms_aGhostCollisions[i].GetPhysical();
				}
			}
		}
	}
}


void CNetworkGhostCollisions::RegisterGhostCollision(const CPhysical& physical1, const CPhysical& physical2)
{
	// ignore ped/ped collisions - they can resolve themselves when intersecting without any bad effects
	if (physical1.GetIsTypePed() && physical2.GetIsTypePed())
	{
		return;
	}

	CNetObjPhysical* netObj1 = static_cast<CNetObjPhysical*>(physical1.GetNetworkObject());
	CNetObjPhysical* netObj2 = static_cast<CNetObjPhysical*>(physical2.GetNetworkObject());

	if (netObj1 && netObj2)
	{
		int ghostSlot1 = netObj1->GetGhostCollisionSlot();
		int ghostSlot2 = netObj2->GetGhostCollisionSlot();

		bool bAlreadyColliding = IsInGhostCollision(physical1, physical2);

		if (!bAlreadyColliding)
		{
			ghostDebugf3("#### Register new collision between %s and %s (slots: %u, %u) ####", netObj1->GetLogName(), netObj2->GetLogName(), ghostSlot1, ghostSlot2);

			if (ghostSlot1 == INVALID_GHOST_COLLISION_SLOT)
			{
				ghostSlot1 = FindFreeCollisionSlot(physical1);

				if (ghostSlot1 != INVALID_GHOST_COLLISION_SLOT)
				{
					ghostDebugf3("%s : Added to collision slot %d", netObj1->GetLogName(), ghostSlot1);
					netObj1->SetGhostCollisionSlot(ghostSlot1);
				}
			}
			else if(ms_aGhostCollisions[ghostSlot1].GetPhysical() != &physical1)
			{
				gnetAssertf(0, "Slot 1 is already assigned to physical. NetObj:%s", netObj1->GetLogName());
			}

			if (ghostSlot2 == INVALID_GHOST_COLLISION_SLOT)
			{
				ghostSlot2 = FindFreeCollisionSlot(physical2);

				if (ghostSlot2 != INVALID_GHOST_COLLISION_SLOT)
				{
					ghostDebugf3("%s : Added to collision slot %d", netObj2->GetLogName(), ghostSlot2);
					netObj2->SetGhostCollisionSlot(ghostSlot2);
				}
			}
			else if(ms_aGhostCollisions[ghostSlot2].GetPhysical() != &physical2)
			{
				gnetAssertf(0, "Slot 2 is already assigned to physical. NetObj:%s", netObj2->GetLogName());
			}
		}

		if (ghostSlot1 != INVALID_GHOST_COLLISION_SLOT && ghostSlot2 != INVALID_GHOST_COLLISION_SLOT)
		{
			u32 frame = fwTimer::GetFrameCount();

			ms_aGhostCollisions[ghostSlot1].SetCollidingWith(ghostSlot2, frame);
			ms_aGhostCollisions[ghostSlot2].SetCollidingWith(ghostSlot1, frame);

			if (!bAlreadyColliding)
			{
				ghostDebugf3("%s collision flags now %lu. Frame added: %u", netObj1->GetLogName(), ms_aGhostCollisions[ghostSlot1].GetCollisionFlags(), ms_aGhostCollisions[ghostSlot1].GetFrameAdded());
				ghostDebugf3("%s collision flags now %lu. Frame added: %u", netObj2->GetLogName(), ms_aGhostCollisions[ghostSlot2].GetCollisionFlags(), ms_aGhostCollisions[ghostSlot2].GetFrameAdded());
			}
		}
	}
}

void CNetworkGhostCollisions::RemoveGhostCollision(int slot, bool LOGGING_ONLY(allowAssert))
{
	if (gnetVerify(slot >= 0 && slot < MAX_GHOST_COLLISIONS))
	{
		LOGGING_ONLY(gnetAssertf(!allowAssert || ms_aGhostCollisions[slot].IsSet(), "Ghostcollision owner is not set in slot %d", slot););
		ghostDebugf3("#### %s : Remove ghost collision in slot %u ####", (ms_aGhostCollisions[slot].GetPhysical() && ms_aGhostCollisions[slot].GetPhysical()->GetNetworkObject()) ? ms_aGhostCollisions[slot].GetPhysical()->GetNetworkObject()->GetLogName() : "?", slot);

		if (ms_aGhostCollisions[slot].IsColliding())
		{
			for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
			{
				if (ms_aGhostCollisions[slot].IsCollidingWith(i))
				{
					ms_aGhostCollisions[i].ClearCollidingWith(slot);
					ms_aGhostCollisions[slot].ClearCollidingWith(i);
				}
			}
		}

		ms_aGhostCollisions[slot].Clear();
	}
}

void CNetworkGhostCollisions::Update()
{
	static u32 lastFrameChecked = 0;

	u32 currFrame = fwTimer::GetFrameCount();

	bool bTryToFreeGhostCollision = false;

	if (currFrame - lastFrameChecked >= FRAME_COUNT_BETWEEN_FREE_CHECKS)
	{
		bTryToFreeGhostCollision = true;
		lastFrameChecked = currFrame;
	}

	for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
	{
		if (ms_aGhostCollisions[i].IsSet())
		{
			if (!ms_aGhostCollisions[i].IsColliding())
			{
				ghostDebugf3("#### %s : Clear ghost collision in slot %d - collision flags are 0 ####", ms_aGhostCollisions[i].GetPhysicalLogName(), i);
				ms_aGhostCollisions[i].Clear();
			}

			if (bTryToFreeGhostCollision && currFrame - ms_aGhostCollisions[i].GetFrameAdded() >= FRAME_COUNT_BEFORE_FREE_CHECK)
			{
				TryToFreeGhostCollision(i);
				bTryToFreeGhostCollision = false;
			}
		}
	}
}

bool CNetworkGhostCollisions::IsAGhostCollision(const CPed& ped1, const CPed& ped2)
{
	bool bGhostCollision = false;

	CNetObjPed* pNetPed1 = static_cast<CNetObjPed*>(ped1.GetNetworkObject());
	CNetObjPed* pNetPed2 = static_cast<CNetObjPed*>(ped2.GetNetworkObject());

	if (pNetPed1 && pNetPed2 && pNetPed1->GetPlayerOwner() && pNetPed2->GetPlayerOwner())
	{
		if (ped1.IsAPlayerPed() && ped2.IsAPlayerPed())
		{
			bool bPlayer1IsGhost = pNetPed1->IsGhost();
			bool bPlayer2IsGhost = pNetPed2->IsGhost(); 

			if (bPlayer1IsGhost || bPlayer2IsGhost)
			{	
				bGhostCollision = true;

				// some players can specify which other players they want to have ghost collisions with. Take this into account here
				PhysicalPlayerIndex player1Index = pNetPed1->GetPlayerOwner()->GetPhysicalPlayerIndex();
				PhysicalPlayerIndex player2Index = pNetPed2->GetPlayerOwner()->GetPhysicalPlayerIndex();

				PlayerFlags ghostPlayer1Flags = static_cast<CNetObjPlayer*>(pNetPed1)->GetGhostPlayerFlags();
				PlayerFlags ghostPlayer2Flags = static_cast<CNetObjPlayer*>(pNetPed2)->GetGhostPlayerFlags();

				bool player1IsFullGhost = bPlayer1IsGhost && ghostPlayer1Flags == 0; 
				bool player2IsFullGhost = bPlayer2IsGhost && ghostPlayer2Flags == 0;

				ghostAssert(player1Index != INVALID_PLAYER_INDEX);
				ghostAssert(player2Index != INVALID_PLAYER_INDEX);

				bool player2GhostedForPlayer1 = player2IsFullGhost || ((ghostPlayer1Flags & (1<<player2Index)) != 0);
				bool player1GhostedForPlayer2 = player1IsFullGhost || ((ghostPlayer2Flags & (1<<player1Index)) != 0);

				if (!player2GhostedForPlayer1 && !player1GhostedForPlayer2)
				{
					bGhostCollision = false;
				}
			}
		}
		else if (pNetPed1->IsGhost() && pNetPed2->IsGhostedForGhostPlayers())
		{
			bGhostCollision = true;
		}
		else if (pNetPed2->IsGhost() && pNetPed1->IsGhostedForGhostPlayers())
		{
			bGhostCollision = true;
		}
	}

	return bGhostCollision;
}



bool CNetworkGhostCollisions::IsAGhostCollision(const CPed& ped, const CVehicle& vehicle)
{
	bool bHasOccupants = false;
	bool bDriverHasGhostCollision = false;
	bool bAllOccupantsHaveGhostCollision = false;

	CPed* pDriver = GetDriverFromVehicle(vehicle);

	if (pDriver && &ped != pDriver && IsAGhostCollision(ped, *pDriver))
	{
		bDriverHasGhostCollision = true;
	}

	if (!bDriverHasGhostCollision)
	{
		bAllOccupantsHaveGhostCollision = true;

		for(s32 seatIndex = 0; seatIndex < vehicle.GetSeatManager()->GetMaxSeats(); seatIndex++)
		{
			CPed *pOccupant = vehicle.GetSeatManager()->GetPedInSeat(seatIndex);

			if (pOccupant && pOccupant != &ped)
			{
				bHasOccupants = true;

				if (!IsAGhostCollision(ped, *pOccupant))
				{
					bAllOccupantsHaveGhostCollision = false;
					break;
				}
			}
		}
	}

	if (bDriverHasGhostCollision || (bHasOccupants && bAllOccupantsHaveGhostCollision))
	{
		return true;
	}

	if (ped.IsPlayer() && ped.GetNetworkObject() && SafeCast(CNetObjPhysical, ped.GetNetworkObject())->IsGhost() &&
		vehicle.GetNetworkObject() && SafeCast(CNetObjPhysical, vehicle.GetNetworkObject())->IsGhostedForGhostPlayers())
	{
		return true;
	}

	return false;
}

int CNetworkGhostCollisions::SortCollisions(const void *paramA, const void *paramB)
{
	const CGhostCollision* collisionA = *((CGhostCollision**)(paramA));
	const CGhostCollision* collisionB = *((CGhostCollision**)(paramB));

	if (collisionA->GetNumFreeAttempts() > collisionB->GetNumFreeAttempts())
	{
		return -1;
	}
	else if(collisionA->GetNumFreeAttempts() < collisionB->GetNumFreeAttempts())
	{
		return 1;
	}

	return 0;
}

u32 CNetworkGhostCollisions::FindFreeCollisionSlot(const CPhysical& physical)
{
	u32 newSlot = INVALID_GHOST_COLLISION_SLOT;

	for (int i=0; i<MAX_GHOST_COLLISIONS; i++)
	{
		if (!ms_aGhostCollisions[i].IsSet())
		{
			newSlot = i;
			break;
		}
	}
	
	if (newSlot == INVALID_GHOST_COLLISION_SLOT)
	{
		ghostDebugf3("#### RAN OUT OF COLLISION SLOTS ####");

		// sort the collisions based on the length of time they have been trying to free themselves. The entity in the oldest slot that is an empty vehicle will be removed.
		CGhostCollision* sortedCollisions[MAX_GHOST_COLLISIONS];

		for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
		{
			sortedCollisions[i] = &ms_aGhostCollisions[i];
		}

		qsort(sortedCollisions, MAX_GHOST_COLLISIONS, sizeof(CGhostCollision*), &SortCollisions);

		for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
		{
			CNetObjPhysical* pNetObj = sortedCollisions[i]->GetPhysicalNetObj();
			const CPhysical* pPhysical = sortedCollisions[i]->GetPhysical();

			if (pPhysical && 
				pPhysical->GetIsTypeVehicle() && 
				pNetObj &&
				!pNetObj->IsGhost() && 
				!pPhysical->IsAScriptEntity() && 
				pPhysical->CanBeDeleted())
			{
				CVehicle* pVehicle = (CVehicle*)pPhysical;

				if (!pVehicle->ContainsPlayer())
				{
					pVehicle->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);

					newSlot = (u32)(sortedCollisions[i] - &ms_aGhostCollisions[0]);

					ghostDebugf3("#### REMOVING OLDEST SLOT %d - %s ####", newSlot, pNetObj->GetLogName());

#if ENABLE_NETWORK_LOGGING
					netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();

					NetworkLogUtils::WriteLogEvent(log, "REMOVING_EX_GHOST_VEHICLE", pNetObj->GetLogName());
#endif
					RemoveGhostCollision(newSlot);

					break;
				}
			}
		}
	}


	if (newSlot == INVALID_GHOST_COLLISION_SLOT)
	{
		ghostDebugf3("#### FAILED TO FIND FREE SLOT ####");
		ghostAssertf(0, "Ran out of ghost collision slots");
	}
	else
	{
		ms_aGhostCollisions[newSlot].SetPhysical(physical);
	}

	return newSlot;
}

void CNetworkGhostCollisions::TryToFreeGhostCollision(int slot)
{
	CGhostCollision& thisGhostCollision = ms_aGhostCollisions[slot];

	if (ghostVerify(thisGhostCollision.IsSet()) && ghostVerify(thisGhostCollision.IsColliding()))
	{
		const CPhysical* pPhysical = thisGhostCollision.GetPhysical();
#if !__NO_OUTPUT
		const char* entityLogName = thisGhostCollision.GetPhysicalLogName();
#endif
		ghostDebugf3("%s : Trying to free ghost collision in slot %d. Current frame: %d", entityLogName, slot, fwTimer::GetFrameCount());

		Matrix34 tempMat = MAT34V_TO_MATRIX34(pPhysical->GetTransform().GetMatrix());

		WorldProbe::CShapeTestBoundDesc boundTestDesc;
		WorldProbe::CShapeTestFixedResults<> boundTestResults;
		boundTestDesc.SetResultsStructure(&boundTestResults);
		boundTestDesc.SetBoundFromEntity(pPhysical);
		boundTestDesc.SetTransform(&tempMat);
		boundTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE);

		static const int MAX_INCLUDE_ENTITIES = MAX_GHOST_COLLISIONS; 

		const CEntity* includeEntities[MAX_INCLUDE_ENTITIES];
		u32 numIncludeEntities = 0;

		for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
		{
			if (thisGhostCollision.IsCollidingWith(i))
			{
				ghostDebugf3("Include entity in slot %d (%s)", i, ms_aGhostCollisions[i].GetPhysicalLogName());

				if (ghostVerify(ms_aGhostCollisions[i].IsSet()))
				{
					includeEntities[numIncludeEntities++] = ms_aGhostCollisions[i].GetPhysical();		
				}
			}
		}

		boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		boundTestDesc.SetIncludeEntities(includeEntities, numIncludeEntities, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

		if (!WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
		{
			ghostDebugf3("#### %s IS FREE ####", entityLogName);
			RemoveGhostCollision(slot);
		}	
		else
		{
			ghostDebugf3("** %s IS STILL COLLIDING **", entityLogName);

			u32 frame = fwTimer::GetFrameCount();

			thisGhostCollision.IncNumFreeAttempts();

			for (u32 i=0; i<MAX_GHOST_COLLISIONS; i++)
			{
				if (thisGhostCollision.IsCollidingWith(i))
				{
					bool bFoundEntity = false;
					bool bFoundPedInCar = false;

					const CPhysical* pOtherPhysical = ms_aGhostCollisions[i].GetPhysical();

					if (pOtherPhysical)
					{
						const CPed* pPed = pPhysical->GetIsTypePed() ? SafeCast(const CPed, pPhysical) : (pOtherPhysical->GetIsTypePed() ? SafeCast(const CPed, pOtherPhysical) : NULL);
						const CVehicle* pVehicle = pPhysical->GetIsTypeVehicle() ? SafeCast(const CVehicle, pPhysical) : (pOtherPhysical->GetIsTypeVehicle() ? SafeCast(const CVehicle, pOtherPhysical) : NULL);

						if (pPed && pVehicle && pPed->GetVehiclePedInside() == pVehicle)
						{
							ghostDebugf3("Ghost collision being cleared: ped in vehicle");
							bFoundPedInCar = true;
						}
					}
						
					if (!bFoundPedInCar)
					{
						for(WorldProbe::ResultIterator it = boundTestResults.begin(); it < boundTestResults.last_result(); ++it)
						{
							if(it->GetHitDetected())
							{
								if (it->GetHitEntity() == ms_aGhostCollisions[i].GetPhysical())
								{
									// set the frame count again, so the entities get left for a while before trying to free themselves again
									ms_aGhostCollisions[slot].SetCollidingWith(i, frame);
									ms_aGhostCollisions[i].SetCollidingWith(slot, frame);

									bFoundEntity = true;

									ghostDebugf3("%s still colliding with %s in slot %u. Set frame count to %u", entityLogName, ms_aGhostCollisions[i].GetPhysicalLogName(), i, ms_aGhostCollisions[i].GetFrameAdded());
									break;
								}
							}
						}
					}

					if (!bFoundEntity)
					{
						ghostDebugf3("%s stopped colliding with %s in slot %u", entityLogName, ms_aGhostCollisions[i].GetPhysicalLogName(), i);
						
						thisGhostCollision.ClearCollidingWith(i);
						ms_aGhostCollisions[i].ClearCollidingWith(slot);

						ghostDebugf3("%s collision flags in slot %u now %lu", entityLogName, slot, thisGhostCollision.GetCollisionFlags());
						ghostDebugf3("%s collision flags in slot %u now %lu", ms_aGhostCollisions[i].GetPhysicalLogName(), slot, ms_aGhostCollisions[i].GetCollisionFlags());
					}
				}
			}
		}
	}
}


CPed* CNetworkGhostCollisions::GetDriverFromVehicle(const CVehicle& vehicle)
{
	CPed* pDriver = vehicle.GetDriver();

	if (!pDriver)
	{
		if (vehicle.InheritsFromTrailer() && vehicle.GetAttachParent() && vehicle.GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
		{
			pDriver = static_cast<CVehicle *>(vehicle.GetAttachParent())->GetDriver();
		}
		else if (vehicle.GetAttachedTrailer())
		{
			pDriver = vehicle.GetAttachedTrailer()->GetDriver();
		}
	}

	return pDriver;
}
