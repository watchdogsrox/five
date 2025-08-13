//
// name:        NetObjTrailer.cpp
// description: Derived from netObject, this class handles all plane-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  Daniel Yelland
//

#include "network/objects/entities/NetObjTrailer.h"

// game headers
#include "network/network.h"
#include "network/debug/NetworkDebug.h"
#include "network/events/NetworkEventTypes.h"
#include "network/objects/prediction/NetBlenderVehicle.h"
#include "network/general/NetworkUtil.h"
#include "peds/ped.h"
#include "peds/pedintelligence.h"
#include "renderer/DrawLists/drawListNY.h"
#include "vehicles/Trailer.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

CNetObjTrailer::CNetObjTrailer(CTrailer* pTrailer,
                         const NetworkObjectType type,
                         const ObjectId objectID,
                         const PhysicalPlayerIndex playerIndex,
                         const NetObjFlags localFlags,
                         const NetObjFlags globalFlags) :
CNetObjAutomobile(pTrailer, type, objectID, playerIndex, localFlags, globalFlags),
m_indestructableConstraints(false)
{
}

bool CNetObjTrailer::AllowFixByNetwork() const
{
	CVehicle *parentVehicle = GetParentVehicle();

	// trailers have the same fixed state as their attach parent,
    // so we always allow this state to changed, so the state is the same
	if(parentVehicle && parentVehicle->GetNetworkObject())
	{
        return true;
	}

	return CNetObjAutomobile::AllowFixByNetwork();
}

void CNetObjTrailer::UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason)
{
    CVehicle *parentVehicle = GetParentVehicle();

    if(parentVehicle && parentVehicle->GetNetworkObject())
    {
        bool parentVehicleFixed = parentVehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK);

        CTrailer *trailer = GetTrailer();

        if(trailer && (trailer->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) != parentVehicleFixed))
        {
            SetFixedByNetwork(parentVehicleFixed, FBN_PARENT_VEHICLE_FIXED_BY_NETWORK, npfbnReason);
        }
    }
    else
    {
        CNetObjVehicle::UpdateFixedByNetwork(fixByNetwork, reason, npfbnReason);
    }
}

void CNetObjTrailer::ManageUpdateLevel(const netPlayer& player)
{
	CVehicle *parentVehicle = GetParentVehicle();
	
	// trailers have the same update level as their parent vehicle
	if (parentVehicle && !parentVehicle->IsNetworkClone() && AssertVerify(parentVehicle->GetNetworkObject()))
	{
		PhysicalPlayerIndex playerIndex = player.GetPhysicalPlayerIndex();

		SetUpdateLevel(playerIndex , parentVehicle->GetNetworkObject()->GetUpdateLevel(playerIndex));
	}
	else
	{
		CNetObjVehicle::ManageUpdateLevel(player);
	}
}

//
// name:        IsInScope
// description: This is used by the object manager to determine whether we need to create a
//              clone to represent this object on a remote machine.
// Parameters:  pPlayer - the player that scope is being tested against
bool CNetObjTrailer::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
	CTrailer* trailer = GetTrailer();
	gnetAssert(trailer);

	bool bInScope = CNetObjVehicle::IsInScope(player, scopeReason);

	bool bImportantScriptObject = IsScriptObject() && (IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT) || AlwaysClonedForPlayer(player.GetPhysicalPlayerIndex()));

	if (bInScope && !bImportantScriptObject)
	{
		CVehicle* parentVehicle = trailer->GetAttachParentVehicle();

		if (parentVehicle && parentVehicle->GetNetworkObject())
		{
			CNetObjVehicle* parentVehicleNetObj = static_cast<CNetObjVehicle*>(parentVehicle->GetNetworkObject());

			// If the vehicle the trailer is attached to is not in scope, then neither should the trailer be
			if (parentVehicleNetObj)
			{
				bool parentVehicleInScope = parentVehicleNetObj->IsInScope(player, scopeReason);

				if (!parentVehicleInScope)
				{
					bInScope = false;
					if (scopeReason)
					{
						*scopeReason = SCOPE_OUT_VEHICLE_ATTACH_PARENT_OUT_SCOPE;
					}
				}
			}
		}
	}	

	return bInScope;
}

bool CNetObjTrailer::CanShowGhostAlpha() const
{
	CVehicle *parentVehicle = GetParentVehicle();
	CNetObjVehicle* pParentObj = parentVehicle ? static_cast<CNetObjVehicle*>(parentVehicle->GetNetworkObject()) : NULL;

	if (pParentObj)
	{
		// trailers inherit the ghosting of their parent vehicle
		return pParentObj->CanShowGhostAlpha();
	}

	return CNetObjVehicle::CanShowGhostAlpha();
}

void CNetObjTrailer::ProcessGhosting()
{
	CNetObjVehicle::ProcessGhosting();

	CTrailer *trailer = GetTrailer();

	if (!trailer->GetDriver() || !trailer->GetDriver()->IsAPlayerPed())
	{
		CVehicle *parentVehicle = GetParentVehicle();
		CNetObjVehicle* pParentObj = parentVehicle ? static_cast<CNetObjVehicle*>(parentVehicle->GetNetworkObject()) : NULL;
	
		if (pParentObj)
		{
			// trailers inherit the ghosting of their parent vehicle
			SetAsGhost(pParentObj->IsGhost() BANK_ONLY(, SPG_TRAILER_INHERITS_GHOSTING));
		}
	}
}

void CNetObjTrailer::HideForCutscene()
{
	CTrailer *trailer = GetTrailer();

	// fix for script trailers still having collision when hidden for cutscenes (due to GetCanMakeIntoDummy() returning true
	if (IsScriptObject() && trailer && !trailer->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		CNetObjPhysical::HideForCutscene();
	}
	else
	{
		CNetObjVehicle::HideForCutscene();
	}
}

bool CNetObjTrailer::Update()
{
    CTrailer *trailer = GetTrailer();
	CVehicle *parentVehicle = GetParentVehicle();

    if (trailer && parentVehicle && gnetVerifyf(parentVehicle->GetNetworkObject(), "%s is connected to a non-networked parent vehicle (%s(%p):is mission:%s)", GetLogName(), parentVehicle->GetDebugName(), parentVehicle, parentVehicle->PopTypeIsMission() ? "TRUE" : "FALSE"))
    {
#if __ASSERT
		char tmpTrailer[32];
		char tmpParent[32];
		if (trailer->IsSuperDummy())
		{
			sprintf(tmpTrailer, "SuperDummy");
		}
		else if (trailer->IsDummy())
		{
			sprintf(tmpTrailer, "Dummy");
		}
		else
		{
			sprintf(tmpTrailer, "Real");
		}
		if (parentVehicle->IsSuperDummy())
		{
			sprintf(tmpParent, "SuperDummy");
		}
		else if (parentVehicle->IsDummy())
		{
			sprintf(tmpParent, "Dummy");
		}
		else
		{
			sprintf(tmpParent, "Real");
		}
        gnetAssertf(trailer->IsDummy() == parentVehicle->IsDummy()
				   , "Trailer (%s -- %s) and parent (%s -- %s) have different dummy states! Trailer is %s, parent is %s"
				   , trailer->GetNetworkObject()->GetLogName(), trailer->pHandling->m_handlingName.TryGetCStr()
				   , parentVehicle->GetNetworkObject()->GetLogName(), parentVehicle->pHandling->m_handlingName.TryGetCStr()
				   , tmpTrailer, tmpParent);
#endif // __ASSERT
				
		if (!IsClone())
		{
			if (trailer->GetDriver() && trailer->GetDriver()->IsLocalPlayer() && parentVehicle->GetDriver() && parentVehicle->GetDriver()->IsPlayer() && parentVehicle->GetDriver()->IsNetworkClone())
			{
				CNetGamePlayer *pPlayer = parentVehicle->GetDriver()->GetNetworkObject()->GetPlayerOwner(); 
				// clear the script migration timer here, otherwise CanPassControl() will return false
				m_scriptMigrationTimer = 0;

				if (CanPassControl(*pPlayer, MIGRATE_FORCED))
				{
					CGiveControlEvent::Trigger(*pPlayer, this, MIGRATE_FORCED);
				}
			}			
		}
    }

    return CNetObjAutomobile::Update();
}



void CNetObjTrailer::GetPhysicalAttachmentData(bool       &attached,
                                               ObjectId   &attachedObjectID,
                                               Vector3    &attachmentOffset,
                                               Quaternion &attachmentQuat,
											   Vector3    &attachmentParentOffset,
                                               s16        &attachmentOtherBone,
                                               s16        &attachmentMyBone,
                                               u32        &attachmentFlags,
                                               bool       &allowInitialSeparation,
                                               float      &invMassScaleA,
                                               float      &invMassScaleB,
											   bool		  &activatePhysicsWhenDetached,
                                               bool       &isCargoVehicle) const
{
	CTrailer *trailer = GetTrailer();

	CNetObjAutomobile::GetPhysicalAttachmentData(attached, attachedObjectID, attachmentOffset, attachmentQuat, attachmentParentOffset,
                                                 attachmentOtherBone, attachmentMyBone, attachmentFlags,
                                                 allowInitialSeparation, invMassScaleA, invMassScaleB, activatePhysicsWhenDetached, isCargoVehicle);

	if (trailer && GetIsAttachedForSync())
	{
		if (trailer->IsAttachedToParentVehicle() || trailer->GetDummyAttachmentParent())
		{
			attachmentFlags |= NETWORK_ATTACH_TO_PARENT_VEHICLE;
		}

		if (trailer->IsTrailerConstraintIndestructible())
		{
			attachmentFlags |= NETWORK_ATTACH_INDESTRUCTABLE;
		}	
	}
}

void CNetObjTrailer::SetPhysicalAttachmentData(const bool        attached,
												const ObjectId    attachedObjectID,
												const Vector3    &attachmentOffset,
												const Quaternion &attachmentQuat,
												const Vector3    &attachmentParentOffset,
												const s16         attachmentOtherBone,
												const s16         attachmentMyBone,
												const u32         attachmentFlags,
                                                const bool        allowInitialSeparation,
                                                const float       invMassScaleA,
                                                const float       invMassScaleB,
												const bool		  activatePhysicsWhenDetached,
                                                const bool        isCargoVehicle)
{
	m_indestructableConstraints = (attachmentFlags & NETWORK_ATTACH_INDESTRUCTABLE) != 0;

	CNetObjAutomobile::SetPhysicalAttachmentData(attached, attachedObjectID, attachmentOffset, attachmentQuat, attachmentParentOffset, attachmentOtherBone, attachmentMyBone, (attachmentFlags & ~NETWORK_ATTACH_INDESTRUCTABLE), allowInitialSeparation, invMassScaleA, invMassScaleB, activatePhysicsWhenDetached, isCargoVehicle);
}

bool CNetObjTrailer::AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason)
{
	bool bSuccess = false;

	if ((m_pendingAttachFlags & NETWORK_ATTACH_TO_PARENT_VEHICLE) != 0)
	{
		if (gnetVerifyf(pEntityToAttachTo->GetIsTypeVehicle(), "%s is trying to attach itself to a non-vehicle!(%s)", GetLogName(), pEntityToAttachTo->GetNetworkObject() ? pEntityToAttachTo->GetNetworkObject()->GetLogName() : "??"))
		{
            CTrailer *trailer = GetTrailer();

			bool skipClearanceCheck =  trailer->GetModelIndex() == MI_TRAILER_TRAILERLARGE
									&& trailer->PopTypeIsMission()
									&& trailer->GetVelocity().Mag2() < 0.01f;
			
            // we need to call the base version of TryToMakeFromDummy() as trailers don't usually
            // convert on their own - it's possible during network games for a trailer to be orphaned
            // as a dummy and we need to convert it back to full fragment physics before attempting to reattach
            if (trailer->IsDummy() && !trailer->CVehicle::TryToMakeFromDummy(skipClearanceCheck))
	        {
		        bSuccess = false;
#if ENABLE_NETWORK_LOGGING
				if(reason)
				{
					*reason = APA_TRAILER_IS_DUMMY;
				}
#endif // ENABLE_NETWORK_LOGGING
	        }
            else
            {
			    CVehicle *parentVehicle = SafeCast(CVehicle, pEntityToAttachTo);

                if(parentVehicle->IsDummy() && !parentVehicle->TryToMakeFromDummy())
                {
			        bSuccess = false;
#if ENABLE_NETWORK_LOGGING
					if(reason)
					{
						*reason = APA_TRAILER_PARENT_IS_DUMMY;
					}
#endif // ENABLE_NETWORK_LOGGING
                }
                else
				{
#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ATTACHING_TRAILER", GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attached to", pEntityToAttachTo->GetNetworkObject() ? pEntityToAttachTo->GetNetworkObject()->GetLogName() : "??");
#endif

                    bSuccess = trailer->AttachToParentVehicle(parentVehicle, false);
                    trailer->m_nVehicleFlags.bHasParentVehicle = true;

                    if(bSuccess && !parentVehicle->IsDummy())
                    {
                        parentVehicle->TryToMakeIntoDummy(VDM_DUMMY);
                    }
                }
            }
		}
	}
	else
	{
		bSuccess = CNetObjAutomobile::AttemptPendingAttachment(pEntityToAttachTo, reason);
	}

	return bSuccess;
}

void CNetObjTrailer::AttemptPendingDetachment(CPhysical* pEntityAttachedTo)
{
	if (GetTrailer()->IsAttachedToParentVehicle() || GetTrailer()->GetDummyAttachmentParent())
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DETACHING_TRAILER", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Detached from", (pEntityAttachedTo && pEntityAttachedTo->GetNetworkObject()) ? pEntityAttachedTo->GetNetworkObject()->GetLogName() : "??");
#endif
		GetTrailer()->DetachFromParent(0);
	}
	else
	{
		CNetObjAutomobile::AttemptPendingDetachment(pEntityAttachedTo);
	}
}

void CNetObjTrailer::ResetBlenderDataWhenAttached()
{
    // attached trailers need to retain their orientation data
    CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

    if(GetParentVehicle() && netBlenderVehicle)
    {
        Matrix34 lastMatrix = netBlenderVehicle->GetLastMatrixReceived();

        netBlenderVehicle->Reset();

        Matrix34 newMatrix = netBlenderVehicle->GetLastMatrixReceived();
        newMatrix.Set3x3(lastMatrix);

        netBlenderVehicle->UpdateMatrix(newMatrix, netBlenderVehicle->GetLastSyncMessageTime());
    }
    else
    {
        CNetObjPhysical::ResetBlenderDataWhenAttached();
    }
}

CVehicle* CNetObjTrailer::GetParentVehicle() const
{
	CTrailer *trailer = GetTrailer();

	CVehicle *parentVehicle = NULL;

	if(trailer && trailer->GetAttachParent() && trailer->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
	{
		parentVehicle = static_cast<CVehicle *>(trailer->GetAttachParent());	
	}

	return parentVehicle;
}

#if __BANK

bool CNetObjTrailer::ShouldDisplayAdditionalDebugInfo()
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if (displaySettings.m_displayTargets.m_displayTrailers && GetTrailer())
	{
		return true;
	}

	return CNetObjVehicle::ShouldDisplayAdditionalDebugInfo();
}

void CNetObjTrailer::DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const
{
    CNetBlenderVehicle *netBlender = static_cast<CNetBlenderVehicle *>(GetNetBlender());

    if(netBlender)
    {
        const unsigned DEBUG_STR_LEN = 200;
	    char debugStr[DEBUG_STR_LEN];

        if(netBlender->IsBlendingTrailer())
        {
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "BLENDING TRAILER"));
	        screenCoords.y += debugYIncrement;
        }

        bool  blendedTooFar = false;
        float positionDelta = netBlender->GetParentVehicleOffsetDelta(blendedTooFar);
        formatf(debugStr, DEBUG_STR_LEN, "Parent Offset Delta: %.2f%s", positionDelta, blendedTooFar ? " (TOO FAR)" : "");
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
        screenCoords.y += debugYIncrement;

        CTrailer *trailer = GetTrailer();

        if(trailer)
        {
            Vector3 lastEulersReceived = netBlender->GetLastMatrixReceived().GetEulers();
            Vector3 currentEulers      = MAT34V_TO_MATRIX34(trailer->GetTransform().GetMatrix()).GetEulers();
            Vector3 eulersDiff         = currentEulers - lastEulersReceived;
            formatf(debugStr, DEBUG_STR_LEN, "Orientation Delta: %.2f, %.2f, %.2f", eulersDiff.x, eulersDiff.y, eulersDiff.z);
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;

            float targetSpeed = netBlender->GetLastVelocityReceived().Mag();
            float actualSpeed = trailer->GetVelocity().Mag();
            formatf(debugStr, DEBUG_STR_LEN, "Target Speed: %.2f Actual Speed: %.2f Diff: %.2f", targetSpeed, actualSpeed, actualSpeed - targetSpeed);
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;
        }
    }
}

#endif // __BANK
