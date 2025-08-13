//
// filename:	commands_physics.cpp
// description:
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "fwpheffects/ropemanager.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwscript/scriptinterface.h"
#include "script/wrapper.h"
#include "../../base/src/physics/physicsprofilecapture.h"

// Suite headers
#include "breakableglass/glassmanager.h"

// Game headers
#include "network/NetworkInterface.h"
#include "objects/object.h"
#include "physics/physics.h"
#include "physics/simulator.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "control/replay/misc/RopePacket.h"

// network includes

using namespace rage;

namespace physics_commands {
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------


// name:		CommandGetFirstRopeVertexCoord
// description:	Return the coordinates of the first rope vertex. Keep in mind that if rope wind/unwind first vertex may not be the first physical one
Vector3 CommandGetRopeFirstVertexCoord( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID ) ) 
		{
			return VEC3V_TO_VECTOR3(pRope->GetRopeFirstVtxWorldPosition());
		}
	}
	return VEC3V_TO_VECTOR3( Vec3V(V_ZERO) );
}


// name:		CommandGetLastRopeVertexCoord
// description:	Return the coordinates of the last rope vertex. Keep in mind that if rope wind/unwind last vertex may not be the last physical one
Vector3 CommandGetRopeLastVertexCoord( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			return VEC3V_TO_VECTOR3(pRope->GetRopeLastVtxWorldPosition());
		}
	}
	return VEC3V_TO_VECTOR3( Vec3V(V_ZERO) );
}


// name:		CommandGetRopeVertexCoord
// description:	Return the coordinates of specific rope vertex.
Vector3 CommandGetRopeVertexCoord( int ropeID, int vtxIndex )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			return VEC3V_TO_VECTOR3(pRope->GetWorldPosition( vtxIndex ));
		}
	}
	return VEC3V_TO_VECTOR3( Vec3V(V_ZERO) );
}


// name:		CommandGetRopeVertexCount
// description:	Return rope's vertex count.
int CommandGetRopeVertexCount( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			return pRope->GetVertsCount();
		}
	}
	return 0;			// obviously there is an issue, so don't return valid value
}


// name:		CommandAddRope
// description:	Create rope and return rope's unique ID.
int CommandAddRope(const scrVector & scrVecPos, const scrVector & scrVecRot, float maxLength, int typeOfRope, float initLength, float minLength, float lengthChangeRate, bool ppuOnly, bool collisionOn, bool lockFromFront, float timeMultiplier, bool breakable, const char* /*materialParamName*/ )
{
	Vector3 pos(scrVecPos);
	Vector3 rot(scrVecRot);

// TODO: expose 'pinned' as parameter to CommandAddRope
// TODO: use material name to access specific rope parameters from ropemanager

	bool pinned = true;
	CScriptResource_Rope newRope(VECTOR3_TO_VEC3V(pos), VECTOR3_TO_VEC3V(rot), initLength, minLength, maxLength, lengthChangeRate, typeOfRope, ppuOnly, collisionOn, lockFromFront, timeMultiplier, breakable, pinned );

    int ropeID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newRope);

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( pRope )
		{
			pRope->SetScriptHandler( CTheScripts::GetCurrentGtaScriptHandler() );
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(pRope->GetUniqueID(), VECTOR3_TO_VEC3V(pos), VECTOR3_TO_VEC3V(rot), initLength, minLength, maxLength, lengthChangeRate, typeOfRope, -1, ppuOnly, lockFromFront, timeMultiplier, breakable, pinned),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
				NULL, 
				true);
#endif
		}
	}

    if(NetworkInterface::IsGameInProgress())
    {
#if !__FINAL
        scriptDebugf1("%s: Creating rope over network: ID: %d Pos: (%.2f, %.2f, %.2f) Rot:(%.2f, %.2f, %.2f)",
            CTheScripts::GetCurrentScriptNameAndProgramCounter(),
            ropeID,
            pos.x, pos.y, pos.z,
            rot.x, rot.y, rot.z);
        scrThread::PrePrintStackTrace();
#endif // !__FINAL
        NetworkInterface::CreateRopeOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), ropeID, pos, rot, maxLength, typeOfRope, initLength, minLength, lengthChangeRate, ppuOnly, collisionOn, lockFromFront, timeMultiplier, breakable, pinned);
    }

	clothDebugf1("[CommandAddRope] Add rope with unique ID:  %d ", ropeID );
	return ropeID;
}

// name:		CommandDoesRopeExist
// description:	Check if rope exists
bool CommandDoesRopeExist( int &ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		if( pRopeManager->FindRope( ropeID ) ) 
			return true;
	}
	return false;
}

// name:		CommandIsRopeAttachedAtBothEnds
// description:	Check if rope is attached at both ends. Func return false if rope doesn't exist
bool CommandIsRopeAttachedAtBothEnds( int &ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( pRope )
		{
			return pRope->IsAttachedAtBothEnds();
		}
	}
	return false;
}

// name:		CommandRopeConvertToSimple
// description:	convert all ropes attached to this entity to simple ones, detach those from the entity and attach only to static world pos ( the last attach pos to the entity )
void CommandRopeConvertToSimple(int iEntityID)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityID);		
		if( scriptVerifyf(pEntity, "%s: Entity with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
		{
			if( scriptVerifyf(pEntity->GetIsPhysical(), "%s: Entity with ID: %d, is not physical or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID ) )
			{
				if( scriptVerifyf( pEntity->GetCurrentPhysicsInst(), "%s: Entity: %d doesn't have phInst, or phInst is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
				{
					pRopeManager->ConvertToSimple( pEntity->GetCurrentPhysicsInst() );
				}
			}
		}
	}
}


void CommandRopeLoadTextures()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeDataManager::LoadRopeTexures();
	}
}

bool CommandRopeAreTexturesLoaded()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		return ropeDataManager::AreRopeTexturesLoaded();
	}
	return false;
}

void CommandRopeUnloadTextures()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeDataManager::UnloadRopeTexures();
	}
}

// name:		CommandRopeDrawEnabled
// description:	Enabled/disable rope drawing
void CommandRopeDrawEnabled( int &ropeID, bool isDrawEnabled )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetDrawEnabled( isDrawEnabled );
		}
	}
}

// name:		CommandRopeDrawShadowEnabled
// description:	Enabled/disable rope shadow drawing
void CommandRopeDrawShadowEnabled( int &ropeID, bool isDrawEnabled )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetDrawShadowEnabled( isDrawEnabled );
		}
	}
}


// name:		CommandStartRopeWinding
// description:	Start rope winding
void CommandStartRopeWinding( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StartWindingFront();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		IncreaseConstraintDistance
// description:	Increase rope's constraint max distance.
void IncreaseConstraintDistance( int ropeID, float distanceChange )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetConstraintLengthBuffer( distanceChange );
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandStopRopeWinding
// description:	Stop rope winding
void CommandStopRopeWinding( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StopWindingFront();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandStartRopeUnwindingFront
// description:	Start rope unwinding from the front
void CommandStartRopeUnwindingFront( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StartUnwindingFront();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandStopRopeUnwindingFront
// description:	Stop rope unwinding from front
void CommandStopRopeUnwindingFront( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StopUnwindingFront();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandStartRopeUnwindingBack
// description:	Start rope unwinding from the back
void CommandStartRopeUnwindingBack( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StartUnwindingBack();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandStopRopeUnwindingBack
// description:	Stop rope unwinding from back
void CommandStopRopeUnwindingBack( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->StopUnwindingBack();
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = pRope;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandRopeDetachVirtualBound
// description:	Attach virtual capsule bound to the rope.
void CommandRopeDetachVirtualBound( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );			// don't assert if rope is not found ... might have been broken already
		if(pRope)
		{
			pRope->DetachVirtualBound();
		}
	}
}


// name:		CommandDeleteChildRope
// description:	Delete child rope
void CommandDeleteChildRope( int ropeID )
{
	clothDebugf1("[CommandDeleteChildRope] Delete rope ( child ) with parent unique ID:  %d ", ropeID );

// NOTE: we make the following assumptions:
// 1. Child rope is not registered with any script - TRUE so far

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		pRopeManager->RemoveChildRope( ropeID );			
	}	
}


// name:		CommandDeleteRope
// description:	Delete rope
void CommandDeleteRope( int &ropeID )
{
	clothDebugf1("[CommandDeleteRope] Delete rope with unique ID:  %d ", ropeID );

	CommandRopeDetachVirtualBound( ropeID );

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ropeManager* pRopeManager = CPhysics::GetRopeManager();
			if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
			{
				ropeInstance* pRope = pRopeManager->FindRope( ropeID );
				if( pRope )
				{
					CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(ropeID);
					CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(ropeID);
					if(pAttachedA || pAttachedB)
					{
						CReplayMgr::RecordPersistantFx<CPacketDeleteRope>(CPacketDeleteRope(ropeID), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope), pAttachedA, pAttachedB, false);
					}
				}
			}

			CReplayRopeManager::DeleteRope(ropeID);
		}
#endif

	bool res = CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ROPE, ropeID);
	if( !res )
	{
// TODO: added command for registering rope with another script ... so if delete falls most likely rope is not registered with the correct script
#if 1
	#if (__DEV || __BANK) && __ASSERT
		ropeManager* pRopeManager = CPhysics::GetRopeManager();
		if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
		{
			ropeInstance* pRope = pRopeManager->FindRope( ropeID );
			if( pRope )
			{
				CGameScriptHandler* pScriptHandler = (CGameScriptHandler*)pRope->GetScriptHandler();
				Assert( pScriptHandler );
				const char* ropeScriptName =  pScriptHandler ? pScriptHandler->GetScriptName(): "UNKNOWN SCRIPT NAME";

				scriptAssertf(0,"Rope is registered with script: %s , but is attempted to be deleted from script: %s", ropeScriptName, CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName() );
			}			
		}		
	#endif // __DEV || __BANK
#else
		// NOTE: if the scripts resource hasn't been found then most likely the resource has been registered with another script thread ( LAME ! )
		// force delete the rope directly through ropemanager
		ropeManager* pRopeManager = CPhysics::GetRopeManager();
		if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
		{
			pRopeManager->RemoveRope( ropeID );
		}
#endif
	}

	const CGameScriptId scriptID =  CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId();
	if(scriptInterface::IsNetworkScript(scriptID))
	{
#if !__FINAL
        scriptDebugf1("%s: Deleting rope over network: ID: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID);
        scrThread::PrePrintStackTrace();
#endif // !__FINAL
		NetworkInterface::DeleteRopeOverNetwork(scriptID, ropeID);
	}	

	ropeID = NULL_IN_SCRIPTING_LANGUAGE;
}

// name:		CommandLoadRopeData
// description:	Load rope verlet data from file
void CommandLoadRopeData( int ropeID, const char* fileName )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->Load( fileName );

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())	
			{
				CReplayMgr::RecordPersistantFx<CPacketLoadRopeData>(
					CPacketLoadRopeData(pRope->GetUniqueID(), fileName),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL, 
					false);
			}
#endif
		}
	}
}

// name:		CommandPinRopeVertex
// description:	Pin specific vertex from the rope
void CommandPinRopeVertex( int ropeID, int vtx, const scrVector & scrVecPos)
{
	Vector3 pos(scrVecPos);
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->Pin( vtx, VECTOR3_TO_VEC3V(pos) );

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
#if RSG_GEN9
				if(CReplayRopeManager::IsRopeKnown(pRope->GetUniqueID()))
#endif
				{
					CReplayMgr::RecordPersistantFx<CPacketPinRope>(
						CPacketPinRope(vtx, VECTOR3_TO_VEC3V(pos), ropeID),
						CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
						NULL,
						false);
				}
			}
#endif
		}
	}
}

// name:		CommandPinRopeVertex
// description:	UnPin specific vertex from the rope
void CommandUnPinRopeVertex( int ropeID, int vtx )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->UnPin( vtx );

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
#if RSG_GEN9
				if(CReplayRopeManager::IsRopeKnown(pRope->GetUniqueID()))
#endif
				{
					CReplayMgr::RecordPersistantFx<CPacketUnPinRope>(
						CPacketUnPinRope(vtx, pRope->GetUniqueID()),
						CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
						NULL,
						false);
				}
			}
#endif
		}
	}
}

// name:		CommandPinRopeVertexComponentA
// description:	Pin specific vertex from the rope for componentA
void CommandPinRopeVertexComponentA( int ropeID, int vtx )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetVertexComponentA( vtx );
		}
	}
}

// name:		CommandPinRopeVertexComponentB
// description:	Pin specific vertex from the rope for componentB
void CommandPinRopeVertexComponentB( int ropeID, int vtx )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetVertexComponentB( vtx );
		}
	}
}


// name:		CommandUnPinAllRopeVerts
// description:	UnPin all rope vertices
void CommandUnPinAllRopeVerts( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->UnpinAllVerts();
		}
	}
}


// name:		CommandRopeSetSmoothReelIn
// description:	Set smoothreelin on a rope
void CommandRopeSetSmoothReelIn( int ropeID, bool bSmoothReelIn )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetSmoothReelIn(bSmoothReelIn);
		}
	}
}


// name:		CommandRopeUpdatePinVerts
// description:	force update rope's pin verts
void CommandRopeUpdatePinVerts( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->UpdateControlVertices();
		}
	}
}


// name:		CommandRopeSetUpdateOrder
// description:	Set update order of a rope ( i.e. when in the frame the physics update is called )
void CommandRopeSetUpdateOrder( int ropeID, int iUpdateOrder )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			pRope->SetUpdateOrder((enRopeUpdateOrder)iUpdateOrder);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())	
			{
				CReplayMgr::RecordPersistantFx<CPacketRopeUpdateOrder>(
					CPacketRopeUpdateOrder(pRope->GetUniqueID(), iUpdateOrder),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
					NULL,
					true);
			}
#endif

		}
	}
}


// name:		CommandAttachEntitiesToRope
// description:	Attach two entities to a rope. Entities need to be physical.
void CommandAttachEntitiesToRope( int ropeID, int iEntityAID, int iEntityBID, const scrVector & scrVecWorldPositionA, const scrVector & scrVecWorldPositionB, float ropeLength, int componentPartA, int componentPartB, const char* boneNamePartA = NULL, const char* boneNamePartB = NULL )
{
	Vector3 worldPositionA(scrVecWorldPositionA);
	Vector3 worldPositionB(scrVecWorldPositionB);
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );		
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID ) ) 
		{
			CEntity* pEntityA = CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityAID);
			CEntity* pEntityB = CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityBID);

			if( scriptVerifyf(pEntityA, "%s: EntityA with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityAID) )
			{
				if( scriptVerifyf(pEntityB, "%s: EntityB with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityBID ) )
				{												
					if( scriptVerifyf(pEntityA->GetIsPhysical(), "%s: EntityA with ID: %d, is not physical or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityAID ) )
					{
						if( scriptVerifyf(pEntityB->GetIsPhysical(), "%s: EntityB with ID: %d, is not physical or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityBID ) )
						{
							if( !scriptVerifyf( pEntityA->GetCurrentPhysicsInst(), "%s: EntityA: %d doesn't have phInst, or phInst is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityAID) )
								return;

							if( !scriptVerifyf( pEntityB->GetCurrentPhysicsInst(), "%s: EntityB: %d doesn't have phInst, or phInst is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityBID ) )
								return;
					
							const crSkeleton* pSkelPartA = (boneNamePartA != NULL ? pEntityA->GetSkeleton(): NULL);
							const crSkeleton* pSkelPartB = (boneNamePartB != NULL ? pEntityB->GetSkeleton(): NULL);

							pRope->AttachObjects( VECTOR3_TO_VEC3V(worldPositionA), VECTOR3_TO_VEC3V(worldPositionB), pEntityA->GetCurrentPhysicsInst(), pEntityB->GetCurrentPhysicsInst(), componentPartA, componentPartB, ropeLength, 0.0f, 1.0f, 1.0f, true, pSkelPartA, pSkelPartB, boneNamePartA, boneNamePartB );

#if GTA_REPLAY
							if(CReplayMgr::ShouldRecord())	
							{
								Matrix34 matInverse = MAT34V_TO_MATRIX34(pEntityA->GetMatrix());
								matInverse.Inverse();
								Vector3 objectSpacePosA = worldPositionA;
								matInverse.Transform(objectSpacePosA);
	
								matInverse = MAT34V_TO_MATRIX34(pEntityB->GetMatrix());
								matInverse.Inverse();
								Vector3 objectSpacePosB = worldPositionB;
								matInverse.Transform(objectSpacePosB);
								
								CReplayMgr::RecordPersistantFx<CPacketAttachEntitiesToRope>(
									CPacketAttachEntitiesToRope( pRope->GetUniqueID(), VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), componentPartA, componentPartB, ropeLength, 0.0f, 1.0f, 1.0f, true, boneNamePartA, boneNamePartB, 0, false),
									CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
									pEntityA,
									pEntityB,
									true);

								CReplayRopeManager::AttachEntites(ropeID, pEntityA, pEntityB);
							}
#endif
						}
					}
				}
			}
		}
	}
}

// name:		CommandAttachRopeToEntity
// description:	Attach rope to an entity. Entity needs to be physical.
void CommandAttachRopeToEntity( int ropeID, int iEntityID, const scrVector & scrVecWorldPosition, int componentPart )
{
	Vector3 worldPosition(scrVecWorldPosition);
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
			if( scriptVerifyf(pEntity,"%s: Entity with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID ) )
			{
				if( scriptVerifyf(pEntity->GetIsPhysical(),"%s: Entity with ID: %d, is not physical or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
				{
					if( scriptVerifyf( pEntity->GetCurrentPhysicsInst(), "%s: Entity: %d doesn't have phInst, or phInst is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
					{
						pRope->AttachToObject( VECTOR3_TO_VEC3V(worldPosition), pEntity->GetCurrentPhysicsInst(), componentPart, NULL, NULL );
#if GTA_REPLAY
							CReplayMgr::RecordPersistantFx<CPacketAttachRopeToEntity>(
								CPacketAttachRopeToEntity(pRope->GetUniqueID(), VECTOR3_TO_VEC3V(worldPosition), componentPart, 0, pRope->GetLocalOffset()),
								CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
								pEntity,
								false);

							CReplayRopeManager::AttachEntites(ropeID, pEntity, NULL);
#endif

					}
				}
			}
		}
	}
}

// name:		CommandDetachRopeFromEntity
// description:	Detach rope from an entity. Rope should have been attached to this entity( not mandatory ).
void CommandDetachRopeFromEntity( int ropeID, int iEntityID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( pRope )
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
			if( scriptVerifyf(pEntity,"%s: Entity with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
			{
				if( scriptVerifyf(pEntity->GetIsPhysical(),"%s: Entity with ID: %d, is not physical or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
				{
					if( scriptVerifyf( pEntity->GetCurrentPhysicsInst(), "%s: Entity: %d doesn't have phInst, or phInst is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iEntityID) )
					{
						pRope->DetachFromObject( pEntity->GetCurrentPhysicsInst() );
#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							CEntity *pAttachedA = CReplayRopeManager::GetAttachedEntityA(ropeID);
							CEntity *pAttachedB = CReplayRopeManager::GetAttachedEntityB(ropeID);
							CEntity* otherAttachedEntity = NULL;
							if( pAttachedA == pEntity )
								otherAttachedEntity = pAttachedB;
							else if( pAttachedB == pEntity )
								otherAttachedEntity = pAttachedA;

							CReplayMgr::RecordPersistantFx<CPacketDetachRopeFromEntity>(
								CPacketDetachRopeFromEntity(),
								CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
								pEntity, otherAttachedEntity,
								false);

							CReplayRopeManager::DetachEntity(ropeID, pEntity);
						}
#endif
					}
				}
			}
		}
	}
}

// name:		CommandBreakRope
// description:	Break rope into two new ropes. Delete/remove the rope if creation of ropeA and ropeB is successful.
void CommandBreakRope( int& ropeID, int& ropeA_ID, int& ropeB_ID, float lenA, float lenB, float minLength, int numSections )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged! This is bad !") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) ) 
		{
			ropeInstance* ropeA = NULL;
			ropeInstance* ropeB = NULL;
			pRopeManager->BreakRope( pRope, ropeA, ropeB, lenA, lenB, minLength, numSections );
		
			if( SCRIPT_VERIFY(ropeA, "Rope A creation failed!") && 
				SCRIPT_VERIFY(ropeB, "Rope B creation failed!")
			  ) 
			{
				CScriptResource_Rope RopeA_ScriptResource(ropeA);
				CScriptResource_Rope RopeB_ScriptResource(ropeB);

				ropeA_ID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(RopeA_ScriptResource);
				ropeB_ID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(RopeB_ScriptResource);

				if( SCRIPT_VERIFY( ropeA_ID != NULL_IN_SCRIPTING_LANGUAGE, "Rope A unique ID is not valid!") && 
					SCRIPT_VERIFY( ropeB_ID != NULL_IN_SCRIPTING_LANGUAGE, "Rope B unique ID is not valid!")
				  )
				{
#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						ropeManager* pRopeManager = CPhysics::GetRopeManager();
						if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
						{
							ropeInstance* pRope = pRopeManager->FindRope( ropeID );
							if( pRope )
							{
								CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(ropeID);
								CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(ropeID);
								CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(ropeID),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope), pAttachedA, pAttachedB, false);
							}
						}
					}
#endif
					CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ROPE, ropeID);
					ropeID = NULL_IN_SCRIPTING_LANGUAGE;
				}
			}
		}
	}
}

// name:		CommandRopeSetConstraintInvmassScale
// description:	Set mas inv scale A on the rope constraint.
void CommandRopeSetConstraintMassInvScaleA( int ropeID, float massInvScale )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			phConstraintBase* constriant = pRope->GetConstraint();
			if( scriptVerifyf(constriant, "%s: Rope with ID: %d, constraint is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
			{
				constriant->SetMassInvScaleA(massInvScale);
			}	
		}
	}
}

// name:		CommandRopeSetConstraintInvmassScale
// description:	Set mas inv scale A on the rope constraint.
void CommandRopeSetConstraintMassInvScaleB( int ropeID, float massInvScale )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			phConstraintBase* constriant = pRope->GetConstraint();
			if( scriptVerifyf(constriant, "%s: Rope with ID: %d, constraint is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
			{
				constriant->SetMassInvScaleB(massInvScale);
			}	
		}
	}
}

// name:		CommandRopeSetConstraintInvmassScale
// description:	Get mas inv scale A on the rope constraint.
float CommandRopeGetConstraintMassInvScaleA( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			const phConstraintBase* constriant = pRope->GetConstraint();
			if( scriptVerifyf(constriant, "%s: Rope with ID: %d, constraint is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
			{
				return constriant->GetMassInvScaleA();
			}
		}
	}
	return -1.0f;
}

// name:		CommandRopeSetConstraintInvmassScale
// description:	Get mas inv scale B on the rope constraint.
float CommandRopeGetConstraintMassInvScaleB( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			const phConstraintBase* constriant = pRope->GetConstraint();
			if( scriptVerifyf(constriant, "%s: Rope with ID: %d, constraint is not valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
			{
				return constriant->GetMassInvScaleB();
			}
		}
	}
	return -1.0f;
}

// name:		CommandRopeDistanceBetweenEnds
// description: Return rope distance between start and end of the rope
float CommandRopeDistanceBetweenEnds( int ropeID )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			return pRope->GetDistanceBetweenEnds();
		}
	}
	return -1.0f;
}

// name:		CommandRopeForceLength
// description: Force rope length
void CommandRopeForceLength( int ropeID, float len )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			pRope->ForceLength(len);
		}
	}
}

// name:		CommandRopeResetLength
// description: Reset rope length
void CommandRopeResetLength( int ropeID, bool isResetLength )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			pRope->SetIsResetLength(isResetLength);
		}
	}
}

// name:		CommandRopeSetRefFrameVelocityColliderOrder
// description:	0 - don't set ref frame velocity , 1 - set ref frame velocity A to B, 2 - ref frame velocity B to A
void CommandRopeSetRefFrameVelocityColliderOrder(int ropeID, int colliderOrder)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			pRope->SetUpdateRefFrameVelocityOrder(colliderOrder);
		}
	}
}

// name:		CommandRopeAttachVirtualBoundGeometry
// description:	Attach virtual geometry bound to the rope.
void CommandRopeAttachVirtualBoundGeometry( int ropeID, int boundIdx, const scrVector & pt0, const scrVector & pt1, const scrVector & pt2, const scrVector & pt3 /*, int numVerts, int numPolys, const Vec3V* RESTRICT verts, const phPolygon::Index** RESTRICT triangles */ )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
// TODO: fix it
			Vec3V verts[4] = { Vec3V(Vector3(pt0)), Vec3V(Vector3(pt1)), Vec3V(Vector3(pt2)), Vec3V(Vector3(pt3)) };
			phPolygon::Index triangles[6] =
			{
				0, 1, 2,	// first triangle
				3, 2, 1,	// second triangle
			};
			pRope->AttachVirtualBoundGeometry( 4/*numVerts*/, 2/*numPolys*/, verts, (const phPolygon::Index*)triangles, boundIdx);
		}
	}
}


// name:		CommandRopeCreateVirtualBound
// description:	Create virtual composite bound to the rope.
void CommandRopeCreateVirtualBound( int ropeID, int iEntityID, int numBounds )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
			if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
			{
				if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
				{
					if( SCRIPT_VERIFY(pEntity->GetCurrentPhysicsInst(), "Entity doesn't have phInst" ) )
					{
						pRope->CreateVirtualBound( numBounds, &pEntity->GetCurrentPhysicsInst()->GetMatrix() );
					}
				}
			}			
		}
	}
}


// name:		CommandRopeAttachVirtualBoundCapsule
// description:	Attach virtual capsule bound to the rope.
void CommandRopeAttachVirtualBoundCapsule( int ropeID, float capsuleRadius, float capsuleLen, int boundIdx, const scrVector & positionOffset )
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			Mat34V boundMat(V_IDENTITY);
			boundMat.SetCol3( Vec3V( Vector3(positionOffset) ) );
			pRope->AttachVirtualBoundCapsule( capsuleRadius, capsuleLen, boundMat, boundIdx );
		}
	}
}

// name:		CommandDoesScriptOwnRope
// description:	Check if the script the call is made from owns the rope
bool CommandDoesScriptOwnRope(int ropeID)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( pRope )
		{
			CGameScriptHandler* pScriptHandler = (CGameScriptHandler*)pRope->GetScriptHandler();
			if( scriptVerifyf( pScriptHandler, "Rope with ID: %d  doesn't have script handler", ropeID) )
			{				
				const GtaThread* pCreationThread = static_cast<const GtaThread*>(pScriptHandler->GetThread());
				if (scriptVerifyf(pCreationThread, "%s: DOES_SCRIPT_OWN_ROPE - creation thread not found", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{	
					if (pCreationThread == CTheScripts::GetCurrentGtaScriptThread())
					{
						return true;
					}
				} // if (scriptVerifyf(pCreationThread, ...
			} // if( pScriptHandler )
		} // if( scriptVerifyf(pRope, ...
		else
		{
			if( pRopeManager->HasChildRope( ropeID ) )
			{
				return true;		// child ropes are not registered with any script, but those will be deleted when the parent is attempted to be deleted !
			}
			else
			{
				scriptDisplayf("%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID);
			}
		}

	} // if( SCRIPT_VERIFY(pRopeManager ...

	return false;
}

// name:		CommandRopeChangeScriptOwner
// description:	switch rope script threads ownership
void CommandRopeChangeScriptOwner(int ropeID, bool /*bScriptHostObject*/, bool bGrabFromOtherScript)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "Rope Manager doesn't exist or is damaged!") )
	{
		ropeInstance* pRope = pRopeManager->FindRope( ropeID );
		if( scriptVerifyf(pRope, "%s: Rope with ID: %d, doesn't exist or is damaged!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID) )
		{
			CGameScriptHandler* pScriptHandler = (CGameScriptHandler*)pRope->GetScriptHandler();
			if( scriptVerifyf( pScriptHandler, "Rope with ID: %d  doesn't have script handler", ropeID) )
			{				
				const GtaThread* pCreationThread = static_cast<const GtaThread*>(pScriptHandler->GetThread());
				if (scriptVerifyf(pCreationThread, "%s: ROPE_CHANGE_SCRIPT_OWNER - creation thread not found", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{	
					if (pCreationThread != CTheScripts::GetCurrentGtaScriptThread())
					{
						if (bGrabFromOtherScript)
						{
							scriptDisplayf("%s: ROPE_CHANGE_SCRIPT_OWNER - taking control of a rope with ID:%d that belonged to script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID, const_cast<GtaThread*>(pCreationThread)->GetScriptName());

							scriptResource* pRopeRsc = pScriptHandler->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ROPE, ropeID);
							Assert( pRopeRsc );
							pScriptHandler->UnregisterScriptResource(pRopeRsc);
							CTheScripts::GetCurrentGtaScriptHandler()->RegisterExistingScriptResource( pRopeRsc );
							pRopeRsc->SetId(CTheScripts::GetCurrentGtaScriptHandler()->GetNextFreeResourceId(*pRopeRsc));

							pRope->SetScriptHandler( CTheScripts::GetCurrentGtaScriptHandler() );														
						}
						else
						{
							scriptAssertf(pCreationThread==CTheScripts::GetCurrentGtaScriptThread(), "%s: ROPE_CHANGE_SCRIPT_OWNER - rope with ID: %d  created by script %s, trying to change script owner to %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ropeID, const_cast<GtaThread*>(pCreationThread)->GetScriptName(), CTheScripts::GetCurrentScriptName());
						}
					}
				} // if (scriptVerifyf(pCreationThread, ...
			} // if( pScriptHandler )

		} // if( scriptVerifyf(pRope, ...
	} // if( SCRIPT_VERIFY(pRopeManager ...
}

// name:		CommandSetDamping
// description:	Set entity damping 
void CommandSetDamping( int iEntityID, int dampingType, float dampingValue )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			if(pEntity->GetCurrentPhysicsInst())
			{
				pEntity->SetDamping( dampingType, dampingValue );
			}
		}
	}
}

// name:		CommandGetDamping
// description:	Get entity damping 
Vector3 CommandGetDamping( int iEntityID, int dampingType )
{
	Vector3 vDamping = VEC3_ZERO;
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			if(pEntity->GetCurrentPhysicsInst())
			{
				vDamping = pEntity->GetDamping(dampingType);
			}
		}
	}
	return vDamping;
}

// name:		CommandPauseClothSimulation
// description:	PauseClothSimulation 
void CommandPauseClothSimulation()
{
	rage::clothManager::PauseClothSimulation();
}

// name:		CommandUnpauseClothSimulation
// description:	UnpauseClothSimulation 
void CommandUnpauseClothSimulation()
{
	rage::clothManager::UnpauseClothSimulation();
}

// name:		CommandActivatePhysics
// description:	Activate entity physics 
void CommandActivatePhysics( int iEntityID )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			pEntity->ActivatePhysics();
		}
	}
}

// name:		CommandSetCGOffset
// description:	Set center of gravity offset
void CommandSetCGOffset( int iEntityID, const scrVector & cgOffset )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			Vector3 vecCgOffset(cgOffset);

			Assert( pEntity->GetFragInst() );
			Assert( pEntity->GetFragInst()->GetArchetype() );
			Assert( pEntity->GetFragInst()->GetArchetype()->GetBound() );
			pEntity->GetFragInst()->GetArchetype()->GetBound()->SetCGOffset( RCC_VEC3V(vecCgOffset) );
			phCollider *pCollider = pEntity->GetCollider();
			if( pCollider )
				pCollider->SetColliderMatrixFromInstance();
		}
	}
}

// name:		CommandGetCGOffset
// description:	Get center of gravity offset
Vector3 CommandGetCGOffset( int iEntityID )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			phInst* pInst = pEntity->GetFragInst();
			Assert( pInst );
			Assert( pInst->GetArchetype() );
			Assert( pInst->GetArchetype()->GetBound() );
			return VEC3V_TO_VECTOR3( pInst->GetArchetype()->GetBound()->GetCGOffset() );
		}
	}
	return VEC3V_TO_VECTOR3( Vec3V(V_ZERO) );
}

// name:
// description:
void CommandSetCGAtWorldCentroid( int iEntityID )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsPhysical(), "Entity is not physical" ) )
		{
			phInst* pInst = pEntity->GetFragInst();
			Assert( pInst );
			Assert( pInst->GetArchetype() );
			Assert( pInst->GetArchetype()->GetBound() );

			Vec3V cgOffset = pInst->GetArchetype()->GetBound()->GetCGOffset();
			Vec3V offs = Subtract(pInst->GetWorldCentroid(), pInst->GetCenterOfMass());
			Vec3V newCgOffset = Subtract( cgOffset, offs );
			pInst->GetArchetype()->GetBound()->SetCGOffset(newCgOffset);
		}
	}
}

// name:
// description:
void CommandClothApplyImpulse( const scrVector & posOld, const scrVector & posNew, float impulse )
{
	clothManager* pClothMgr = CPhysics::GetClothManager();
	Assert(pClothMgr);
	pClothMgr->ComputeClothImpacts( VECTOR3_TO_VEC3V(Vector3(posOld)), VECTOR3_TO_VEC3V(Vector3(posNew)), impulse );
}

// name:		CommandEntityBreakGlass
// description:	Break breakable glass panels
//              When silent is true VFX, falling shards or audio are disabled
void CommandEntityBreakGlass(int iEntityId, const scrVector & position, float radius, const scrVector & impulse, float damage, int crackType, bool silent)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityId);	
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") && SCRIPT_VERIFY(pEntity->GetIsDynamic(),"Entity is not dynamic or is damaged") )
	{
		CDynamicEntity *dyna = (CDynamicEntity*)pEntity;
		if( dyna->m_nDEflags.bIsBreakableGlass )
		{
			Vector3 pos(position);
			Vector3 imp(impulse);

			if(silent)
			{
				bgGlassManager::SetSilentHit(true);
			}

			CPhysics::BreakGlass(dyna,RCC_VEC3V(pos),radius, RCC_VEC3V(imp), damage, crackType, false);

			if(silent)
			{
				bgGlassManager::SetSilentHit(false);
			}
		}
	}
}

enum extraPhysics
{
	CLEAR_MANIFOLDS = 1 << 0  //Set physics engine to an interior mode where we clear contacts every frame in order to improve small rolling physics (f.ex. for billiards)
};

// name:		SetExtraPhysics
// description:	Enables special physics modes. Standard is to require constant calls from the script in order to upkeep the desired special state.
//				This ensures the scripts cannot leave any of these on for long, and allows them to call these without fear of wiping out another scripts state.
void SetExtraPhysics(int extraPhysicsMask)
{
	if((extraPhysicsMask & CLEAR_MANIFOLDS) != 0)
	{
		const int clearManifoldsFrames = 10; // Arbitrary number to give some leeway
		CPhysics::SetClearManifoldsFramesLeft(clearManifoldsFrames);
	}
}

// name: GetIsFrag
// description: Checks whether an object is a frag
//				
bool GetIsFrag(int iEntityID)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( pEntity->GetIsTypeObject() && pEntity->m_nFlags.bIsFrag)
		{
			return true;
		}
	}
	return false;
}


// name: SetDisableBreaking
// description: allows the user to force a fragment instance to never break, or to override the fragType level flag 
//				that prevent breaking.
void SetDisableBreaking(int iEntityID, bool disableBreaking)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsTypeObject(), "SET_DISABLE_BREAKING was intended to be used for props only." ) )
		{
			fragInst* instance = pEntity->GetFragInst();
			if(SCRIPT_VERIFY(instance, "Entity doesn't have a frag instance"))
			{
				instance->SetDisableBreakable(disableBreaking);

				if (pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					SafeCast(CNetObjObject, pEntity->GetNetworkObject())->SetDisableBreaking(disableBreaking);
				}
			}
		}
	}
}

// name: SetResetDisableBreaking
// description: resets the disable breaking status of a fragment instance to its default as defined by the tuning data.
void ResetDisableBreaking(int iEntityID)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsTypeObject(), "RESET_DISABLE_BREAKING was intended to be used for props only." ) )
		{
			fragInst* instance = pEntity->GetFragInst();
			if(SCRIPT_VERIFY(instance, "Entity doesn't have a frag instance"))
			{
				instance->ResetDisableBreakable();

				if (pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					SafeCast(CNetObjObject, pEntity->GetNetworkObject())->SetDisableBreaking(instance->IsBreakingDisabled());
				}
			}
		}
	}
}

// name: SetDisableFragDamage
// description: allows the user to force a fragment instance to never break, or to override the fragType level flag 
//				that prevent damage.
void SetDisableFragDamage(int iEntityID, bool disableDamage)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"SET_DISABLE_FRAG_DAMAGE - Entity is invalid or damaged") )
	{
		if( SCRIPT_VERIFY(pEntity->GetIsTypeObject(), "SET_DISABLE_FRAG_DAMAGE - Entity is not an object") )
		{
			fragInst* instance = pEntity->GetFragInst();
			if(SCRIPT_VERIFY(instance, "SET_DISABLE_FRAG_DAMAGE - Entity doesn't have a frag instance"))
			{
				instance->SetDisableDamage(disableDamage);

				if (pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					SafeCast(CNetObjObject, pEntity->GetNetworkObject())->SetDisableDamage(disableDamage);
				}
			}
		}
	}
}

// name: SetUseKinematicPhysics
// description: allows the user to force a physical to use kinematic physics
void SetUseKinematicPhysics( int iEntityID, bool useKinematic )
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
	if( SCRIPT_VERIFY(pEntity,"SET_USE_KINEMATIC_PHYSICS - Entity is invalid or damaged") )
	{
		pEntity->SetShouldUseKinematicPhysics( useKinematic );
	}
}

// name:		StartPhysicsProfileCapture
// description:	Starts a physics profile capture.
void StartPhysicsProfileCapture()
{
	rage::PhysicsCaptureStart();
}

void SetInStuntMode( bool InStuntMode )
{
	CPhysics::ms_bInStuntMode = InStuntMode;
	if(CPhysics::ms_bInStuntMode)
	{
		g_MaxPushCollisionIterations = 1;
	}
	else
	{
		g_MaxPushCollisionIterations = 2;
	}
}

void SetInArenaMode( bool InArenaMode )
{
	CPhysics::ms_bInArenaMode = InArenaMode;
	NetworkInterface::HACK_CheckAllPlayersInteriorFlag();
}

// name:		SetupScriptCommands
// description:	Setup weapons script commands
void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(ADD_ROPE,0xf7c04299e67b897b, CommandAddRope);
	SCR_REGISTER_SECURE(DELETE_ROPE,0x5d6cd3fbca6b7301, CommandDeleteRope);
	SCR_REGISTER_SECURE(DELETE_CHILD_ROPE,0x880e5061bc41cb4a, CommandDeleteChildRope);
	SCR_REGISTER_UNUSED(BREAK_ROPE,0x5411565ea30afd30, CommandBreakRope);
	SCR_REGISTER_SECURE(DOES_ROPE_EXIST,0x20c4e97f4b681ef0, CommandDoesRopeExist);
	SCR_REGISTER_SECURE(ROPE_DRAW_ENABLED,0x3b076a2089069f6c, CommandRopeDrawEnabled);
	SCR_REGISTER_SECURE(ROPE_DRAW_SHADOW_ENABLED,0x789cc2fd07551ca2, CommandRopeDrawShadowEnabled);

	SCR_REGISTER_SECURE(LOAD_ROPE_DATA,0xb1ab9201aeca88b6, CommandLoadRopeData);
	SCR_REGISTER_SECURE(PIN_ROPE_VERTEX,0x6a36fc7bd95f1f71, CommandPinRopeVertex);
	SCR_REGISTER_SECURE(UNPIN_ROPE_VERTEX,0x09cfbcadf0f31fac, CommandUnPinRopeVertex);
	SCR_REGISTER_SECURE(GET_ROPE_VERTEX_COUNT,0xcd804af23829b282, CommandGetRopeVertexCount);

	SCR_REGISTER_UNUSED(PIN_ROPE_VERTEX_COMP_A,			0x4aedef77, CommandPinRopeVertexComponentA);
	SCR_REGISTER_UNUSED(PIN_ROPE_VERTEX_COMP_B,			0x389e4ad8, CommandPinRopeVertexComponentB);
	SCR_REGISTER_UNUSED(UNPIN_ALL_ROPE_VERTS,			0x711e7e56, CommandUnPinAllRopeVerts);
	
	SCR_REGISTER_SECURE(ATTACH_ENTITIES_TO_ROPE,0x3869f781e0645e49, CommandAttachEntitiesToRope);
	SCR_REGISTER_SECURE(ATTACH_ROPE_TO_ENTITY,0x02f4a045919dd893, CommandAttachRopeToEntity);
	SCR_REGISTER_SECURE(DETACH_ROPE_FROM_ENTITY,0x7efa568576f9adfb, CommandDetachRopeFromEntity);	

	SCR_REGISTER_SECURE(ROPE_SET_UPDATE_PINVERTS,0xbc187c1214ec35a1, CommandRopeUpdatePinVerts);
	SCR_REGISTER_SECURE(ROPE_SET_UPDATE_ORDER,0x666e1c85e933f6f4, CommandRopeSetUpdateOrder);
	SCR_REGISTER_SECURE(ROPE_SET_SMOOTH_REELIN,0x77a37eaf6485534f, CommandRopeSetSmoothReelIn);
	SCR_REGISTER_SECURE(IS_ROPE_ATTACHED_AT_BOTH_ENDS,0x16336eba6ff583c3, CommandIsRopeAttachedAtBothEnds);

	SCR_REGISTER_UNUSED(GET_ROPE_FIRST_VERTEX_COORD,0x9b8eb32a3bbfa5a1, CommandGetRopeFirstVertexCoord);
	SCR_REGISTER_SECURE(GET_ROPE_LAST_VERTEX_COORD,0x16bdd6f2bcc79411, CommandGetRopeLastVertexCoord);

	SCR_REGISTER_SECURE(GET_ROPE_VERTEX_COORD,0x7859eabd657d708d, CommandGetRopeVertexCoord);	
	SCR_REGISTER_SECURE(START_ROPE_WINDING,0x870e2d116133e4f9, CommandStartRopeWinding);
	SCR_REGISTER_SECURE(STOP_ROPE_WINDING,0xc4f13fc93388d0df, CommandStopRopeWinding);	
	SCR_REGISTER_SECURE(START_ROPE_UNWINDING_FRONT,0x133a8e2f66b92d11, CommandStartRopeUnwindingFront);
	SCR_REGISTER_SECURE(STOP_ROPE_UNWINDING_FRONT,0xf773fb3cb7e958db, CommandStopRopeUnwindingFront);
	SCR_REGISTER_UNUSED(START_ROPE_UNWINDING_BACK,0x1cd7759a01ed8c49, CommandStartRopeUnwindingBack);
	SCR_REGISTER_UNUSED(STOP_ROPE_UNWINDING_BACK,0xabb5da254529a113, CommandStopRopeUnwindingBack);
	SCR_REGISTER_SECURE(ROPE_CONVERT_TO_SIMPLE,0x8a043068ec9a5a88, CommandRopeConvertToSimple);

	SCR_REGISTER_SECURE(ROPE_LOAD_TEXTURES,0x44395b87a17466e1, CommandRopeLoadTextures);
	SCR_REGISTER_SECURE(ROPE_ARE_TEXTURES_LOADED,0x7cae494c630022d5, CommandRopeAreTexturesLoaded);
	SCR_REGISTER_SECURE(ROPE_UNLOAD_TEXTURES,0xf45e94c02fe9ab19, CommandRopeUnloadTextures);

	SCR_REGISTER_UNUSED(INCREASE_ROPE_CONSTRAINT_DISTANCE,0xad8dc3bf23f1b5bd, IncreaseConstraintDistance);
	
	SCR_REGISTER_SECURE(DOES_SCRIPT_OWN_ROPE,0x1cc33b026a2c4382, CommandDoesScriptOwnRope);	
	SCR_REGISTER_SECURE(ROPE_ATTACH_VIRTUAL_BOUND_GEOM,0x062587da74a0aa1c, CommandRopeAttachVirtualBoundGeometry);
	SCR_REGISTER_UNUSED(ROPE_ATTACH_VIRTUAL_BOUND_CAPSULE,0xd64c3d935024985b, CommandRopeAttachVirtualBoundCapsule);
	SCR_REGISTER_UNUSED(ROPE_CREATE_VIRTUAL_BOUND,0x2abfada14586e29c,	CommandRopeCreateVirtualBound);
	SCR_REGISTER_UNUSED(ROPE_DETACH_VIRTUAL_BOUND,0xeac0f572acb6f83f, CommandRopeDetachVirtualBound);
	SCR_REGISTER_SECURE(ROPE_CHANGE_SCRIPT_OWNER,0x5be8b68dc44472ef, CommandRopeChangeScriptOwner);		
	SCR_REGISTER_UNUSED(ROPE_SET_CONSTRAINT_MASSINVSCALE_A,0xbde16458dec50519, CommandRopeSetConstraintMassInvScaleA);
	SCR_REGISTER_UNUSED(ROPE_SET_CONSTRAINT_MASSINVSCALE_B,0xd674e2c06bbf1858, CommandRopeSetConstraintMassInvScaleB);
	SCR_REGISTER_UNUSED(ROPE_GET_CONSTRAINT_MASSINVSCALE_A,0x88178b3412236794, CommandRopeGetConstraintMassInvScaleA);
	SCR_REGISTER_UNUSED(ROPE_GET_CONSTRAINT_MASSINVSCALE_B,0xdcbdf617511e3a4d, CommandRopeGetConstraintMassInvScaleB);
	SCR_REGISTER_SECURE(ROPE_SET_REFFRAMEVELOCITY_COLLIDERORDER,0x434e46d8c7756749, CommandRopeSetRefFrameVelocityColliderOrder);
	SCR_REGISTER_SECURE(ROPE_GET_DISTANCE_BETWEEN_ENDS,0x229d1e7f61d9525f,	CommandRopeDistanceBetweenEnds);
	SCR_REGISTER_SECURE(ROPE_FORCE_LENGTH,0x3a5caa1de72a99d8,	CommandRopeForceLength);
	SCR_REGISTER_SECURE(ROPE_RESET_LENGTH,0xf32942ea2762aa8a,	CommandRopeResetLength);
	SCR_REGISTER_SECURE(APPLY_IMPULSE_TO_CLOTH,0x7d219e5a58c633cd,	CommandClothApplyImpulse);

	SCR_REGISTER_SECURE(SET_DAMPING,0xe18e3777851d0c80, CommandSetDamping);
	SCR_REGISTER_UNUSED(GET_DAMPING,0xfa61c96d55efca2b, CommandGetDamping);
	SCR_REGISTER_SECURE(ACTIVATE_PHYSICS,0xaa8c46c452582702,  CommandActivatePhysics);
	SCR_REGISTER_SECURE(SET_CGOFFSET,0x9b28494a72472300,  CommandSetCGOffset);
	SCR_REGISTER_SECURE(GET_CGOFFSET,0xed14d2dfe4e6457b,  CommandGetCGOffset);
	SCR_REGISTER_SECURE(SET_CG_AT_BOUNDCENTER,0x4463d0391b1f4e67,  CommandSetCGAtWorldCentroid);
	

	SCR_REGISTER_UNUSED(PAUSE_CLOTH_SIMULATION,0xa9f784207b4327fc, CommandPauseClothSimulation);
	SCR_REGISTER_UNUSED(UNPAUSE_CLOTH_SIMULATION,0x50616112e94822b1, CommandUnpauseClothSimulation);
	
	SCR_REGISTER_SECURE(BREAK_ENTITY_GLASS,0xbd8adbb181ac4e4f, CommandEntityBreakGlass);

	SCR_REGISTER_UNUSED(FORCE_EXTRA_PHYSICS,0x13c076fbd4384659, SetExtraPhysics);

	SCR_REGISTER_SECURE(GET_IS_ENTITY_A_FRAG,0x9e3b5869358b3477, GetIsFrag);
	SCR_REGISTER_SECURE(SET_DISABLE_BREAKING,0x9b08a23707ebc5ab, SetDisableBreaking);
	SCR_REGISTER_SECURE(RESET_DISABLE_BREAKING,0x5485f49bbef65cfb, ResetDisableBreaking);
	
	SCR_REGISTER_SECURE(SET_DISABLE_FRAG_DAMAGE,0x4133f882b7793ee5, SetDisableFragDamage);
	SCR_REGISTER_SECURE(SET_USE_KINEMATIC_PHYSICS,0xab901be3f45f3b33, SetUseKinematicPhysics);
	SCR_REGISTER_UNUSED(START_PHYSICS_PROFILE_CAPTURE,0x11c4e81c5f3b2709, StartPhysicsProfileCapture);

	SCR_REGISTER_SECURE(SET_IN_STUNT_MODE,0xe72a5b55545dd8d4, SetInStuntMode);
	SCR_REGISTER_SECURE( SET_IN_ARENA_MODE,0x57b8667937fda8bc, SetInArenaMode );
	
}

}; // namespace physics_commands
