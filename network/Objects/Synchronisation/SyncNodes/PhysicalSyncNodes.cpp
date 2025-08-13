//
// name:        PhysicalSyncNodes.cpp
// description: Network sync nodes used by CNetObjPhysicals
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/PhysicalSyncNodes.h"

DATA_ACCESSOR_ID_IMPL(IPhysicalNodeDataAccessor);

void CPhysicalGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZE_OF_ALPHA_RAMP_TYPE = datBitsNeeded<IPhysicalNodeDataAccessor::ART_NUM_TYPES>::COUNT;

	SERIALISE_BOOL(serialiser, m_isVisible,           "Visible");
	SERIALISE_BOOL(serialiser, m_renderScorched,      "Render Scorched");
	SERIALISE_BOOL(serialiser, m_isInWater,           "Is In Water");
	SERIALISE_BOOL(serialiser, m_alteringAlpha,       "Altering Alpha");

	if (m_alteringAlpha || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_alphaType, SIZE_OF_ALPHA_RAMP_TYPE, "Alpha Type");
		bool bIsUsingCustomFadeDuration = m_customFadeDuration > 0;
		SERIALISE_BOOL(serialiser, bIsUsingCustomFadeDuration, "Using Custom Fade Duration");
		if (bIsUsingCustomFadeDuration || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_customFadeDuration, 16, "Custom Fade Duration")
		}
		else
		{
			m_customFadeDuration = 0;
		}
	}
	else
	{
		m_fadingOut = 0;
		m_alphaType = IPhysicalNodeDataAccessor::ART_NONE;
	}

	SERIALISE_BOOL(serialiser, m_allowCloningWhileInTutorial, "Allow Cloning While in Tutorial");
}

bool CPhysicalGameStateDataNode::HasDefaultState() const 
{ 
	return (m_renderScorched == 0
		&& m_isInWater == 0
		&& m_alteringAlpha == 0
		&& m_customFadeDuration == 0
		&& m_allowCloningWhileInTutorial == false);
}

void CPhysicalGameStateDataNode::SetDefaultState() 
{ 
	m_renderScorched = m_isInWater = m_alteringAlpha = 0; 
	m_customFadeDuration = 0;
	m_allowCloningWhileInTutorial = false;
}

const float CPhysicalScriptGameStateDataNode::MAX_MAX_SPEED = 600.0f;

void CPhysicalScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_RELGROUP_HASH = 32;

	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByAnything,			"Not damaged by anything");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.dontLoadCollision,				"Don't load collision");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.allowFreezeIfNoCollision,		"Allow Freeze If No Collision");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.damagedOnlyByPlayer,				"Damaged only by player");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByBullets,				"Not Damaged By Bullets");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByFlames,				"Not Damaged By Fires");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.ignoresExplosions,				"Not Damaged By Explosions");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByCollisions,			"Not Damaged By Collisions");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByMelee,				"Not Damaged By Melee");
    SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedBySmoke,				"Not Damaged By Smoke");
    SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedBySteam,				"Not Damaged By Steam");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.notDamagedByRelGroup,			"Not Damaged By Relationship Group");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.onlyDamageByRelGroup,			"Only Damaged By Relationship Group");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.onlyDamagedWhenRunningScript,	"Only Damaged When Running Script");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState,	"Don't reset damage flags on cleanup");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.noReassign,                    	"No Reassign");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.passControlInTutorial,			"Pass Control Of this Object in Tutorials");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.inCutscene,						"In Cutscene");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.ghostedForGhostPlayers,			"Ghosted For Ghost Players");
	SERIALISE_BOOL(serialiser, m_PhysicalFlags.pickUpByCargobobDisabled,		"Pickup By Cargobob Disabled");

	if(m_PhysicalFlags.notDamagedByRelGroup || serialiser.GetIsMaximumSizeSerialiser() || m_PhysicalFlags.onlyDamageByRelGroup)
	{
		SERIALISE_UNSIGNED(serialiser, m_RelGroupHash, SIZEOF_RELGROUP_HASH, "Relationship Group Hash");
	}
	else
	{
		m_RelGroupHash = 0;
	}

	bool alwaysClonedForPlayers = (m_AlwaysClonedForPlayer != 0);

	SERIALISE_BOOL(serialiser, alwaysClonedForPlayers);

	if(alwaysClonedForPlayers || serialiser.GetIsMaximumSizeSerialiser())
	{
		const unsigned SIZEOF_CLONED_FOR_PLAYERS = 32;
		SERIALISE_BITFIELD(serialiser, m_AlwaysClonedForPlayer, SIZEOF_CLONED_FOR_PLAYERS, "Always Cloned For Player");
	}
	else
	{
		m_AlwaysClonedForPlayer = 0;
	}

    SERIALISE_BOOL(serialiser, m_HasMaxSpeed);

    if(m_HasMaxSpeed || serialiser.GetIsMaximumSizeSerialiser())
    {
        const unsigned SIZEOF_MAX_SPEED = 16;
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MaxSpeed, MAX_MAX_SPEED, SIZEOF_MAX_SPEED, "Max Speed");
    }

	SERIALISE_BOOL(serialiser, m_AllowMigrateToSpectator, "Ignore Can Accept Control");
}

bool CPhysicalScriptGameStateDataNode::ResetScriptStateInternal()
{
	m_PhysicalFlags.notDamagedByAnything			= false;
	m_PhysicalFlags.dontLoadCollision				= true;
	m_PhysicalFlags.allowFreezeIfNoCollision		= true;
	m_PhysicalFlags.damagedOnlyByPlayer				= false;
	m_PhysicalFlags.notDamagedByBullets				= false;
	m_PhysicalFlags.notDamagedByFlames				= false;
	m_PhysicalFlags.ignoresExplosions				= false;
	m_PhysicalFlags.notDamagedByCollisions			= false;
	m_PhysicalFlags.notDamagedByMelee				= false;
    m_PhysicalFlags.notDamagedBySmoke               = false;
    m_PhysicalFlags.notDamagedBySteam               = false;
	m_PhysicalFlags.notDamagedByRelGroup			= false;
	m_PhysicalFlags.onlyDamageByRelGroup			= false;
	m_PhysicalFlags.onlyDamagedWhenRunningScript	= false;
	m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState = false;
	m_PhysicalFlags.noReassign                      = false;
	m_PhysicalFlags.passControlInTutorial			= false;
	m_RelGroupHash									= 0;
	m_AlwaysClonedForPlayer							= 0;
    m_HasMaxSpeed                                   = false;

	m_AllowMigrateToSpectator						= false;
	return true;
}

bool CPhysicalScriptGameStateDataNode::HasDefaultState() const 
{
	return (m_PhysicalFlags.notDamagedByAnything			== false &&
		m_PhysicalFlags.dontLoadCollision				== true  &&
		m_PhysicalFlags.allowFreezeIfNoCollision		== false &&
		m_PhysicalFlags.damagedOnlyByPlayer				== false &&
		m_PhysicalFlags.notDamagedByBullets				== false &&
		m_PhysicalFlags.notDamagedByFlames				== false &&
		m_PhysicalFlags.ignoresExplosions				== false &&
		m_PhysicalFlags.notDamagedByCollisions			== false &&
		m_PhysicalFlags.notDamagedByMelee				== false &&
        m_PhysicalFlags.notDamagedBySmoke               == false &&
        m_PhysicalFlags.notDamagedBySteam               == false &&
		m_PhysicalFlags.notDamagedByRelGroup			== false &&
		m_PhysicalFlags.onlyDamageByRelGroup			== false &&
		m_PhysicalFlags.onlyDamagedWhenRunningScript	== false &&
		m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState == false &&
		m_PhysicalFlags.noReassign                      == false &&
		m_PhysicalFlags.passControlInTutorial			== false &&
		m_RelGroupHash									== 0     &&
		m_AlwaysClonedForPlayer							== 0     &&
		m_AllowMigrateToSpectator						== false &&
        !m_HasMaxSpeed);
}

void CPhysicalScriptGameStateDataNode::SetDefaultState() 
{ 
	ResetScriptStateInternal(); 
}

void CPhysicalVelocityDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_VELOCITY(serialiser, m_packedVelocityX, m_packedVelocityY, m_packedVelocityZ, SIZEOF_VELOCITY, "Velocity");
}

bool CPhysicalVelocityDataNode::HasDefaultState() const 
{ 
	return (m_packedVelocityX == 0 && m_packedVelocityY == 0 && m_packedVelocityZ == 0); 
}

void CPhysicalVelocityDataNode::SetDefaultState() 
{
	m_packedVelocityX = m_packedVelocityY = m_packedVelocityZ = 0; 
}

void CPhysicalAngVelocityDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_VELOCITY(serialiser, m_packedAngVelocityX, m_packedAngVelocityY, m_packedAngVelocityZ, SIZEOF_ANGVELOCITY, "Angular Velocity");
}

bool CPhysicalAngVelocityDataNode::HasDefaultState() const 
{ 
	return (m_packedAngVelocityX == 0 && m_packedAngVelocityY == 0 && m_packedAngVelocityZ == 0); 
}

void CPhysicalAngVelocityDataNode::SetDefaultState() 
{
	m_packedAngVelocityX = m_packedAngVelocityY = m_packedAngVelocityZ = 0; 
}

void CPhysicalHealthDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_HEALTH = 16;
	static const unsigned int SIZEOF_WEAPON_HASH = 32;//6; this is a hash now!
#if PH_MATERIAL_ID_64BIT
	static const unsigned int SIZEOF_MATERIAL_ID = 64;
#else
	static const unsigned int SIZEOF_MATERIAL_ID = 32;
#endif // PH_MATERIAL_ID_64BIT

	SERIALISE_BOOL(serialiser, m_hasMaxHealth, "Has max health");
	SERIALISE_BOOL(serialiser, m_maxHealthSetByScript, "Max health set by script");

	if (m_maxHealthSetByScript || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_scriptMaxHealth, SIZEOF_HEALTH, "Script Max Health");
	}
	else
	{
		m_scriptMaxHealth = 0;
	}

	if (!m_hasMaxHealth || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_health, SIZEOF_HEALTH, "Health");

		bool bHasDamageEntity = m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_BOOL(serialiser, bHasDamageEntity);

		if (bHasDamageEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_weaponDamageEntity, "Weapon Damage Entity");
			SERIALISE_UNSIGNED(serialiser, m_weaponDamageHash, SIZEOF_WEAPON_HASH, "Weapon Damage Hash");
		}
		else
		{
			m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
			m_weaponDamageHash   = 0;
		}

		bool hasLastDamageMaterial = m_lastDamagedMaterialId != 0;
		SERIALISE_BOOL(serialiser, hasLastDamageMaterial, "Has Last Damaged Material");
		if(hasLastDamageMaterial || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_lastDamagedMaterialId, SIZEOF_MATERIAL_ID, "Last Damaged Material Id");
		}
		else
		{
			m_lastDamagedMaterialId = 0;
		}
	}
	else
	{
		m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		m_weaponDamageHash = 0;
		m_lastDamagedMaterialId = 0;
	}
}

void CPhysicalAttachDataNode::ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
{
	physicalNodeData.GetPhysicalAttachmentData(m_attached,
		m_attachedObjectID,
		m_attachmentOffset,
		m_attachmentQuat,
		m_attachmentParentOffset,
		m_attachmentOtherBone,
		m_attachmentMyBone,
		m_attachmentFlags,
		m_allowInitialSeparation,
		m_InvMassScaleA,
		m_InvMassScaleB,
		m_activatePhysicsWhenDetached,
        m_IsCargoVehicle);
}

void CPhysicalAttachDataNode::ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
{
	physicalNodeData.SetPhysicalAttachmentData(m_attached,
		m_attachedObjectID,
		m_attachmentOffset,
		m_attachmentQuat,
		m_attachmentParentOffset,
		m_attachmentOtherBone,
		m_attachmentMyBone,
		m_attachmentFlags,
		m_allowInitialSeparation,
		m_InvMassScaleA,
		m_InvMassScaleB,
		m_activatePhysicsWhenDetached,
        m_IsCargoVehicle);
}



void CPhysicalAttachDataNode::Serialise(CSyncDataBase& serialiser)
{
	const unsigned SIZEOF_ATTACH_OFFSET  = 15;
	const unsigned SIZEOF_ATTACH_QUAT    = 16;
	const unsigned SIZEOF_ATTACH_BONE    = 8;
	const unsigned SIZEOF_ATTACH_FLAGS   = NUM_EXTERNAL_ATTACH_FLAGS + 2; // + 2 is for the extra attachment flags used by CNetObjTrailer
	const unsigned SIZEOF_INV_MASS_SCALE = 5;
	const float MAX_ATTACHMENT_OFFSET	 = 80.0f;

	SERIALISE_BOOL(serialiser, m_attached, "Attached");

	if(m_attached || serialiser.GetIsMaximumSizeSerialiser())
	{
		bool bHasAttachmentOffset = !m_attachmentOffset.IsZero();
		bool bHasAttachmentQuat = m_attachmentQuat.x != 0.0f || m_attachmentQuat.y != 0.0f || m_attachmentQuat.z != 0.0f;
		bool bHasAttachmentParentOffset = !m_attachmentParentOffset.IsZero();
		bool bHasAttachmentBones = m_attachmentMyBone != 0 || m_attachmentOtherBone != 0;

		SERIALISE_OBJECTID(serialiser, m_attachedObjectID, "Attached To");

		SERIALISE_BOOL(serialiser, bHasAttachmentOffset, "Has offset");

		if (bHasAttachmentOffset || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentOffset.x, MAX_ATTACHMENT_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset X");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentOffset.y, MAX_ATTACHMENT_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentOffset.z, MAX_ATTACHMENT_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset Z");
		}
		else
		{
			m_attachmentOffset.Zero();
		}

		SERIALISE_BOOL(serialiser, bHasAttachmentQuat, "Has orientation");

		if (bHasAttachmentQuat || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentQuat.x, 1.0f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion X");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentQuat.y, 1.0f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentQuat.z, 1.0f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion Z");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentQuat.w, 1.0f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion W");
		}
		else
		{
			m_attachmentQuat.Identity();
		}

		SERIALISE_BOOL(serialiser, bHasAttachmentParentOffset, "Has parent offset");

		if (bHasAttachmentParentOffset || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentParentOffset.x, 4.0f, SIZEOF_ATTACH_OFFSET, "Attachment Parent Offset X");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentParentOffset.y, 4.0f, SIZEOF_ATTACH_OFFSET, "Attachment Parent Offset Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachmentParentOffset.z, 4.0f, SIZEOF_ATTACH_OFFSET, "Attachment Parent Offset Z");
		}
		else
		{
			m_attachmentParentOffset.Zero();
		}

		SERIALISE_BOOL(serialiser, bHasAttachmentBones, "Has attach bones");

		if (bHasAttachmentBones || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_attachmentOtherBone,   SIZEOF_ATTACH_BONE,  "Other Attachment Bone");
			SERIALISE_INTEGER(serialiser, m_attachmentMyBone,   SIZEOF_ATTACH_BONE,  "My Attachment Bone");
		}
		else
		{
			m_attachmentOtherBone = m_attachmentMyBone = 0;
		}

		SERIALISE_UNSIGNED(serialiser, m_attachmentFlags, SIZEOF_ATTACH_FLAGS, "Attachment Flags");

		SERIALISE_BOOL(serialiser, m_allowInitialSeparation, "Allow Initial Separation");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_InvMassScaleA, 1.0f, SIZEOF_INV_MASS_SCALE, "Inv Mass Scale A");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_InvMassScaleB, 1.0f, SIZEOF_INV_MASS_SCALE, "Inv Mass Scale A");
        SERIALISE_BOOL(serialiser, m_IsCargoVehicle, "Is Cargo Vehicle");
	}
	else if (m_syncPhysicsActivation)
	{
		SERIALISE_BOOL(serialiser, m_activatePhysicsWhenDetached, "Activate Physics When Detached");
        m_IsCargoVehicle = false;
	}
	else
	{
		m_activatePhysicsWhenDetached = true;
        m_IsCargoVehicle              = false;
	}
}

bool CPhysicalAttachDataNode::HasDefaultState() const 
{ 
	return (m_attached == false && m_activatePhysicsWhenDetached == true); 
}

void CPhysicalAttachDataNode::SetDefaultState() 
{
	m_attached = false; m_activatePhysicsWhenDetached = true; 
}

void CPhysicalMigrationDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_isDead, "Is Dead");
}

void CPhysicalScriptMigrationDataNode::Serialise(CSyncDataBase& serialiser)
{
	//static const unsigned SIZEOF_PARTICIPANT_ID = 8;

	SERIALISE_BOOL(serialiser, m_HasData, "Has Data");

	if (m_HasData)
	{
		SERIALISE_UNSIGNED(serialiser, m_ScriptParticipants, sizeof(PlayerFlags)<<3, "Script Participants");
		SERIALISE_UNSIGNED(serialiser, m_HostToken, sizeof(HostToken)<<3, "Host Token");
	}
}
