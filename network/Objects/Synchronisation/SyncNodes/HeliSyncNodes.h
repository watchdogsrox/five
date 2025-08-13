//
// name:        HeliSyncNodes.h
// description: Network sync nodes used by CNetObjHelis
// written by:    John Gurney
//

#ifndef HELI_SYNC_NODES_H
#define HELI_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/AutomobileSyncNodes.h"

class CHeliHealthDataNode;
class CHeliControlDataNode;
class CHeliGunDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IHeliNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the heli nodes.
// Any class that wishes to send/receive data contained in the heli nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IHeliNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IHeliNodeDataAccessor);

    // getter functions
    virtual void GetHeliHealthData(CHeliHealthDataNode& data) = 0;
    virtual void GetHeliControlData(CHeliControlDataNode& data) = 0;

    // setter functions
    virtual void SetHeliHealthData(const CHeliHealthDataNode& data) = 0;
    virtual void SetHeliControlData(const CHeliControlDataNode& data) = 0;
 
protected:

    virtual ~IHeliNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CHeliHealthDataNode
//
// Handles the serialization of a helicopter's health
///////////////////////////////////////////////////////////////////////////////////////
class CHeliHealthDataNode : public CPhysicalHealthDataNode
{
protected:

	NODE_COMMON_IMPL("Heli Health", CHeliHealthDataNode, IHeliNodeDataAccessor);

private:

   void ExtractDataForSerialising(IHeliNodeDataAccessor &heliNodeData)
    {
        heliNodeData.GetHeliHealthData(*this);
    }

    void ApplyDataFromSerialising(IHeliNodeDataAccessor &heliNodeData)
    {
        heliNodeData.SetHeliHealthData(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const; 
	virtual void SetDefaultState(); 

public:

    u32  m_mainRotorHealth; // health of the main rotor blade for the helicopter
    u32  m_rearRotorHealth; // health of the rear rotor blade for the helicopter
	bool m_canBoomBreak;    // can the boom break?
    bool m_boomBroken;      // is the boom broken?
	bool m_hasCustomHealth;
	bool m_disableExpFromBodyDamage;
	u32  m_bodyHealth;
	u32  m_gasTankHealth;
	u32  m_engineHealth;
	float m_mainRotorDamageScale;
	float m_rearRotorDamageScale;
	float m_tailBoomDamageScale;
};

///////////////////////////////////////////////////////////////////////////////////////
// CHeliControlDataNode
//
// Handles the serialization of a helicopter's control
///////////////////////////////////////////////////////////////////////////////////////
class CHeliControlDataNode : public CVehicleControlDataNode
{
protected:

	NODE_COMMON_IMPL("Heli Control Node", CHeliControlDataNode, IHeliNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IHeliNodeDataAccessor &heliNodeData)
    {
        heliNodeData.GetHeliControlData(*this);
    }

    void ApplyDataFromSerialising(IHeliNodeDataAccessor &heliNodeData)
    {
        heliNodeData.SetHeliControlData(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	float m_yawControl;                 // yaw control of the helicopter
	float m_pitchControl;               // pitch control of the helicopter
	float m_rollControl;                // roll control of the helicopter
	float m_throttleControl;            // throttle control of the helicopter
	bool  m_mainRotorStopped;           // is the main rotor stopped?
	u32	  m_landingGearState;           // landing gear state - if the helicopter has landing gear
	bool  m_bHasLandingGear;            // if the helicopter has landing gear
	bool  m_hasActiveAITask;            // should the heli be fixed if no collision around it?
	bool  m_hasJetpackStrafeForceScale; // does the heli have the jetpack effect
	float m_jetPackStrafeForceScale;    // force of jetpack strafe 
	float m_jetPackThrusterThrottle;    // force of jetpack thrusters 
	bool  m_lockedToXY;                 // anchor state for anchorable sea helis
};



#endif  // HELI_SYNC_NODES_H
