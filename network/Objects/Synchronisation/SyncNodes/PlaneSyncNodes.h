//
// name:        PlaneSyncNodes.h
// description: Network sync nodes used by CNetObjPlane
// written by:  
//

#ifndef PLANE_SYNC_NODES_H
#define PLANE_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

#define NUM_PLANE_DAMAGE_SECTIONS 13
#define NUM_PLANE_LANDING_GEAR_DAMAGE_SECTIONS 6

class CPlaneGameStateDataNode;
class CPlaneControlDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IPlaneNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the plane nodes.
// Any class that wishes to send/receive data contained in the plane nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPlaneNodeDataAccessor : public netINodeDataAccessor
{
public:

	DATA_ACCESSOR_ID_DECL(IPlaneNodeDataAccessor);

	virtual void GetPlaneGameState(CPlaneGameStateDataNode& data) = 0;
	virtual void SetPlaneGameState(const CPlaneGameStateDataNode& data) = 0;
	
	virtual void GetPlaneControlData(CPlaneControlDataNode& data) = 0;
	virtual void SetPlaneControlData(const CPlaneControlDataNode& data) = 0;

protected:

    virtual ~IPlaneNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlaneGameStateDataNode
//
// Handles the serialization of a plane's game state
///////////////////////////////////////////////////////////////////////////////////////
class CPlaneGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Plane Game State", CPlaneGameStateDataNode, IPlaneNodeDataAccessor);

	virtual bool IsCreateNode() const { return true; }

	bool IsAlwaysSentWithCreate() const { return true; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IPlaneNodeDataAccessor &planeNodeData)
    {
		planeNodeData.GetPlaneGameState(*this);
    }

    void ApplyDataFromSerialising(IPlaneNodeDataAccessor &planeNodeData)
    {
		planeNodeData.SetPlaneGameState(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	u32		 m_LandingGearPublicState;					                              // Landing Gear Public State
	float	 m_SectionDamage[NUM_PLANE_DAMAGE_SECTIONS];                              // damage fraction values for each plane section
	float	 m_SectionDamageScale[NUM_PLANE_DAMAGE_SECTIONS];                         // damage scale for each plane section
	float	 m_LandingGearSectionDamageScale[NUM_PLANE_LANDING_GEAR_DAMAGE_SECTIONS]; // damage scale for each plane section
	u32		 m_DamagedSections;							                              // flags indicating which sections are damaged
	u32		 m_BrokenSections;							                              // flags indicating which sections have broken off
	u32		 m_RotorBroken;								                              // flags indicating which rotors are broken off
	ObjectId m_LockOnTarget;                                                          // ID of network object this plane is locked-on to
	unsigned m_LockOnState;                                                           // Lockon state (none, acquiring, acquired)
	u8		 m_IndividualPropellerFlags;				                              // flags indicating state of individual propellers
	bool	 m_AIIgnoresBrokenPartsForHandling;			                              // if AI can fly plane well with damaged parts
	bool     m_ControlSectionsBreakOffFromExplosions;                                 
	bool     m_HasCustomSectionDamageScale;                                           // Do we have custom damage scales for our sections?
	bool     m_HasCustomLandingGearSectionDamageScale;								  // Do we have custom damage scales for our landing gear sections?
	float    m_EngineDamageScale;                                                     // damage scale for engine (overall)
	u32		 m_LODdistance;															  // LOD distance of pickup
	bool     m_disableExpFromBodyDamage;											  // Does this plane take damage from body impacts?
	bool	 m_AllowRollAndYawWhenCrashing;											  // When set, planes will spiral while crashing 
	bool	 m_dipStraightDownWhenCrashing;
	bool	 m_disableExlodeFromBodyDamageOnCollision;
};


///////////////////////////////////////////////////////////////////////////////////////
// CPlaneControlDataNode
//
// Handles the serialization of a planes control
///////////////////////////////////////////////////////////////////////////////////////
class CPlaneControlDataNode : public CVehicleControlDataNode
{
protected:

	NODE_COMMON_IMPL("Plane Control Node", CPlaneControlDataNode, IPlaneNodeDataAccessor);

public:

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

	void ExtractDataForSerialising(IPlaneNodeDataAccessor &planeNodeData)
	{
		planeNodeData.GetPlaneControlData(*this);
	}

	void ApplyDataFromSerialising(IPlaneNodeDataAccessor &planeNodeData)
	{
		planeNodeData.SetPlaneControlData(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	float m_yawControl;      // yaw control of the plane
	float m_pitchControl;    // pitch control of the plane
	float m_rollControl;     // roll control of the plane
	float m_throttleControl; // throttle control of the plane
	float m_brake;	 // break control of the plane
	float m_verticalFlightMode; // whether the plane is in vertical or horizontal flight mode
	bool  m_hasActiveAITask;
};

#endif  //PLANE_SYNC_NODES_H
