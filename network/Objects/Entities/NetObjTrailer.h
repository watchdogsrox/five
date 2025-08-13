//
// name:		NetObjTrailer.h
// description:	Derived from netObject, this class handles all plane-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	Daniel Yelland
//

#ifndef NETOBJTRAILER_H
#define NETOBJTRAILER_H

#include "network/objects/entities/NetObjAutomobile.h"

class CNetObjTrailer : public CNetObjAutomobile
{
public:

    enum
    {
        NETWORK_ATTACH_TO_PARENT_VEHICLE = BIT(NETWORK_ATTACH_FLAGS_START),
		NETWORK_ATTACH_INDESTRUCTABLE = BIT(NETWORK_ATTACH_FLAGS_START+1)
    };

    CNetObjTrailer(class CTrailer				*trailer,
                    const NetworkObjectType		type,
                    const ObjectId				objectID,
                    const PhysicalPlayerIndex   playerIndex,
                    const NetObjFlags			localFlags,
                    const NetObjFlags			globalFlags);

    CNetObjTrailer() {SetObjectType(NET_OBJ_TYPE_TRAILER);}

    virtual const char *GetObjectTypeName() const { return IsOrWasScriptObject() ? "SCRIPT_TRAILER" : "TRAILER"; }

    CTrailer* GetTrailer() const { return (CTrailer*)GetGameObject(); }

    CVehicle* GetParentVehicle() const;

	virtual bool AllowFixByNetwork() const;
    virtual void UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason);
	virtual void ManageUpdateLevel(const netPlayer& player);
	virtual bool IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual bool PreventMigrationDuringAttachmentStateMismatch() const { return true;}
	virtual bool CanShowGhostAlpha() const;
	virtual void ProcessGhosting();
	virtual void HideForCutscene();

    virtual bool Update();

    void GetPhysicalAttachmentData(bool       &attached,
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
                                   bool       &isCargoVehicle) const;

	void SetPhysicalAttachmentData(const bool        attached,
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
								   const bool		 activatePhysicsWhenDetached,
								   const bool        isCargoVehicle);
private:

	virtual bool AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason = nullptr);
	virtual void AttemptPendingDetachment(CPhysical* pEntityToAttachTo);
	virtual void UpdatePendingAttachment() {} // does nothing
    virtual void ResetBlenderDataWhenAttached();

#if __BANK
    virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const;
#endif // __BANK

	bool m_indestructableConstraints; // if set the trailer can never detach
};

#endif  // NETOBJTRAILER_H
