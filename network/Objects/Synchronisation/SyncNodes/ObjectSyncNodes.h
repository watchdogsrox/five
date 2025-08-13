//
// name:        ObjectSyncNodes.h
// description: Network sync nodes used by CNetObjObjects
// written by:    John Gurney
//

#ifndef OBJECT_SYNC_NODES_H
#define OBJECT_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/PhysicalSyncNodes.h"

#include "network/general/NetworkUtil.h"
#include "script/handlers/GameScriptIds.h"
#include "script/handlers/GameScriptMgr.h"

class CObject;
class CObjectCreationDataNode;
class CObjectSectorPosNode;
class CObjectOrientationNode;
class CObjectGameStateDataNode;
class CObjectScriptGameStateDataNode;

struct SyncGroupFlags
{
	SyncGroupFlags() : m_NumBitsUsed(0), m_Flags(0) {}

	static const unsigned MAX_NUM_GROUP_FLAGS_BITS = 128;
	atFixedBitSet<MAX_NUM_GROUP_FLAGS_BITS, u8> m_Flags;
	unsigned m_NumBitsUsed;

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_NUM_USED = datBitsNeeded<MAX_NUM_GROUP_FLAGS_BITS>::COUNT;

		bool hasGroupFlags = m_Flags.AreAnySet();

		if (serialiser.GetIsMaximumSizeSerialiser())
		{
			hasGroupFlags = true;
			m_NumBitsUsed = MAX_NUM_GROUP_FLAGS_BITS;
		}

		SERIALISE_BOOL(serialiser, hasGroupFlags, "Has Group Flags");
		if (hasGroupFlags)
		{
			SERIALISE_UNSIGNED(serialiser, m_NumBitsUsed, SIZEOF_NUM_USED, "Num Group Flags");

			unsigned syncedBits = 0;
			LOGGING_ONLY(char buf[128]);

			for (int i = 0; i < m_Flags.NUM_BLOCKS; i++)
			{
				if (syncedBits < m_NumBitsUsed)
				{
					LOGGING_ONLY(formatf(buf, "GroupFlags_%d", i));
					u8 block = m_Flags.GetRawBlock(i);
					int bitsRemaining = m_NumBitsUsed - syncedBits;
					SERIALISE_BITFIELD(serialiser, block, MIN((m_Flags.BLOCK_SIZE << 3), bitsRemaining), buf);
					m_Flags.SetRawBlock(i, block);

					syncedBits += (m_Flags.BLOCK_SIZE << 3);
				}
				else
				{
					m_Flags.SetRawBlock(i, 0);
				}
			}
		}
		else
		{
			m_Flags.Reset();
			m_NumBitsUsed = 0;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////
// IObjectNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the object nodes.
// Any class that wishes to send/receive data contained in the object nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IObjectNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IObjectNodeDataAccessor);

    // sync parser getter functions
    virtual void GetObjectCreateData(CObjectCreationDataNode& data) const = 0;

    virtual void GetObjectSectorPosData(CObjectSectorPosNode& data) const = 0;

    virtual void GetObjectOrientationData(CObjectOrientationNode& data) const = 0;

    virtual void GetObjectGameStateData(CObjectGameStateDataNode& data) const = 0;

    virtual void GetObjectScriptGameStateData(CObjectScriptGameStateDataNode& data) const = 0;


    // sync parser setter functions
    virtual void SetObjectCreateData(const CObjectCreationDataNode& data) = 0;

    virtual void SetObjectSectorPosData(CObjectSectorPosNode& data) = 0;

    virtual void SetObjectOrientationData(CObjectOrientationNode& data) = 0;

    virtual void SetObjectGameStateData(const CObjectGameStateDataNode& data) = 0;

    virtual void SetObjectScriptGameStateData(const CObjectScriptGameStateDataNode& data) = 0;

protected:

    virtual ~IObjectNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CObjectCreationDataNode
//
// Handles the serialization of an object's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CObjectCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Object Creation", CObjectCreationDataNode, IObjectNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.GetObjectCreateData(*this);
    }

    void ApplyDataFromSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.SetObjectCreateData(*this);
    }

    // used to avoid including NetObjObject.h, avoiding circular dependency
    bool IsAmbientObjectType(u32 type);
    bool IsFragmentObjectType(u32 type);

    void Serialise(CSyncDataBase& serialiser);

public:

	ObjectId		m_fragParentVehicle;	  // if set, this object is a vehicle fragment and it belongs to this vehicle id
	Matrix34		m_objectMatrix;			  // the position of the object (used when the network object has no game object)
    Vector3         m_dummyPosition;          // position of the dummy object this object was instanced from
	Vector3         m_objectPosition;         // position of the object (used when there is no game object)
    Vector3         m_ScriptGrabPosition;     // world position script grabbed this object from
    mutable CObject*m_pRandomObject;          // the random world object that exists at the dummy position
    float           m_ScriptGrabRadius;       // radius used by script to grab this object
    u32             m_ownedBy;                // created by
    u32             m_modelHash;              // model index
    u32             m_fragGroupIndex;         // the frag group index (used by fragment cache objects)
	u32				m_ownershipToken;		  // used when there is no associated prop (and sync data) for this network object
	u32				m_lodDistance;
	bool			m_noReassign;			  // stop the object changing owner
	bool			m_hasGameObject;		  // is there a prop object associated with this network object?
    bool            m_playerWantsControl;     // does the creating player want control of this object
    bool			m_hasInitPhysics;         // has this object initialized physics?
	bool            m_ScriptGrabbedFromWorld; // has script grabbed this object from a world position?
	bool            m_IsFragObject;			  // this object is a frag object
	bool			m_IsBroken;				  // the object is broken / damaged
	bool			m_IsAmbientFence;		  // the object is an uprooted fence
	bool			m_HasExploded;			  // the object has exploded
	bool			m_KeepRegistered;		  // the object must remain registered
	bool			m_DestroyFrags;			  // if the object is breakable, destroy any frags created by the breaking
	bool			m_lodOrphanHd;
    bool            m_CanBlendWhenFixed;      // indicates the network blender can run when the object is using fixed physics
};

///////////////////////////////////////////////////////////////////////////////////////
// CObjectGameStateDataNode
//
// Handles the serialization of an object's game state.
///////////////////////////////////////////////////////////////////////////////////////
class CObjectGameStateDataNode : public CSyncDataNodeInfrequent
{
public:

	typedef void (*fnTaskDataLogger)(netLoggingInterface *log, u32 taskType, const u8 *taskData, const int numBits);

	static void SetTaskDataLogFunction(fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

    NODE_COMMON_IMPL("Object Game State", CObjectGameStateDataNode, IObjectNodeDataAccessor);

private:

    void ExtractDataForSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.GetObjectGameStateData(*this);
    }

    void ApplyDataFromSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.SetObjectGameStateData(*this);
	}

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

	static const unsigned int MAX_TASK_DATA_SIZE = 150;

public:

	SyncGroupFlags m_groupFlags;
	u32  m_taskType;
	u32	 m_taskDataSize;
	u32  m_brokenFlags;
	u8   m_taskSpecificData[MAX_TASK_DATA_SIZE];
	bool m_objectHasExploded;      
	bool m_hasAddedPhysics;
	bool m_visible;
    bool m_HasBeenPickedUpByHook;
	bool m_popTires;

	static fnTaskDataLogger m_taskDataLogFunction;
};

///////////////////////////////////////////////////////////////////////////////////////
// CObjectScriptGameStateDataNode
//
// Handles the serialization of an object's script game state.
///////////////////////////////////////////////////////////////////////////////////////
class CObjectScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Object Script Game State", CObjectScriptGameStateDataNode, IObjectNodeDataAccessor);

private:

    void ExtractDataForSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.GetObjectScriptGameStateData(*this);
    }

    void ApplyDataFromSerialising(IObjectNodeDataAccessor &objectNodeData)
    {
        objectNodeData.SetObjectScriptGameStateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

    virtual bool ResetScriptStateInternal();

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	Vector3     m_TranslationDamping;              // scripted translational damping
    u32         m_OwnedBy;                         // created by
	u32		    m_ScopeDistance;				   // script adjusted scope distance
    u32		    m_TintIndex;					   // tint color of object
    ObjectId    m_DamageInflictorId;
	u8          m_objSpeedBoost;                   // value of speed boost for boost pads
	u8		    m_jointToDriveIndex;
	bool        m_IsStealable;                     // is this object stealable?
    bool        m_ActivatePhysicsAsSoonAsUnfrozen; // activate the physics on this object as soon as it is unfrozen
    bool        m_UseHighPrecisionBlending;        // does this object require high precision blending (i.e. minigame objects, such as a golf ball)
	bool        m_UsingScriptedPhysicsParams;      // does this object use scripted physics params (currently only modifying translational damping is supported/used)
	bool        m_BreakingDisabled;				   // has breaking been disabled on this object
	bool        m_DamageDisabled;				   // has damage been disabled on this object
	bool	    m_IgnoreLightSettings;
	bool        m_CanBeTargeted;
	bool	    m_bDriveToMaxAngle;
	bool	    m_bDriveToMinAngle;
	bool	    m_bWeaponImpactsApplyGreaterForce;
    bool        m_bIsArticulatedProp;
    bool        m_bIsArenaBall;
    bool        m_bNoGravity;
	bool        m_bObjectDamaged;
	bool		m_bObjectFragBroken;
};

///////////////////////////////////////////////////////////////////////////////////////
// CObjectSectorPosNode
//
// Handles the serialization of a CObject's sector position. This supports the option
// of syncing the position with high precision, generally used for script objects that
// the extra accuracy
///////////////////////////////////////////////////////////////////////////////////////
class CObjectSectorPosNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Object Sector Pos", CObjectSectorPosNode, IObjectNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IObjectNodeDataAccessor &nodeData);

    void ApplyDataFromSerialising(IObjectNodeDataAccessor &nodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

#if __DEV
	static RecorderId ms_bandwidthRecorderId;
#endif

	float    m_SectorPosX;       // X position of this object within the current sector
    float    m_SectorPosY;       // Y position of this object within the current sector
    float    m_SectorPosZ;       // Z position of this object within the current sector
    bool     m_UseHighPrecision; // Indicates whether the position should be synced with high precision
};

///////////////////////////////////////////////////////////////////////////////////////
// CObjectOrientationNode
//
// Handles the serialization of a CObject's orientation. This supports the option
// of syncing the orientation with high precision
///////////////////////////////////////////////////////////////////////////////////////
class CObjectOrientationNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Object Orientation", CObjectOrientationNode, IObjectNodeDataAccessor);

    PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

    bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IObjectNodeDataAccessor &nodeData);

    void ApplyDataFromSerialising(IObjectNodeDataAccessor &nodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

#if __DEV
    static RecorderId ms_bandwidthRecorderId;
#endif

    Matrix34 m_orientation; // current orientation of the object
    bool m_bUseHighPrecision; // Indicates whether the orientation should be synced with high precision
};

#endif  // OBJECT_SYNC_NODES_H
