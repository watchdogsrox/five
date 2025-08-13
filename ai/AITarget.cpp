// File header
#include "AI/AITarget.h"

// Framework headers
#include "ai/aichannel.h" 
#include "fwscene/world/WorldLimits.h"

// Game headers
#include "Scene/Entity.h"
#include "Peds/Ped.h"
#include "Vehicles/Vehicle.h"

#include "data/aes_init.h"
AES_INIT_1;


// Macro to disable optimizations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CAITarget
//////////////////////////////////////////////////////////////////////////

CAITarget::CAITarget()
: m_vPosition(VEC3_ZERO)
, m_mode(Mode_Uninitialised)
, m_pEntity(NULL)
{
}

CAITarget::CAITarget(const CEntity* pTargetEntity)
{
	SetEntity(pTargetEntity);
}

CAITarget::CAITarget(const CEntity* pTargetEntity, const Vector3& vTargetOffset)
{
	SetEntityAndOffset(pTargetEntity, vTargetOffset);
}

CAITarget::CAITarget(const Vector3& vTargetPosition)
{
	SetPosition(vTargetPosition);
}

CAITarget::CAITarget(const CAITarget& other)
: m_vPosition(other.m_vPosition)
, m_mode(other.m_mode)
, m_pEntity(other.m_pEntity)
{
}

bool CAITarget::GetIsValid() const
{
	switch(m_mode)
	{
	case Mode_Entity:
	case Mode_EntityAndOffset:
		return m_pEntity.GetEntity() ? true : false;

	case Mode_WorldSpacePosition:
		return true;

	case Mode_Uninitialised:
		// Our target has not been initialised
		return false;

	default:
		// Not handled
		aiAssertf(0,"Mode [%d] not handled by CAITarget", m_mode);
		return false;
	}
}

bool CAITarget::GetPosition(Vector3& vTargetPosition) const
{
	bool bSuccess = false;

	const CEntity* pEntity = m_pEntity.GetEntity();

	switch(m_mode)
	{
	case Mode_Entity:
		if(pEntity)
		{
			// Get the world space lock on position of the entity
			pEntity->GetLockOnTargetAimAtPos(vTargetPosition);
			aiAssertf(!vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "CAITarget::GetPosition -- entity lock-on position is very close to the origin.  This is very likely an error.  Entity position: (%.2f, %.2f, %.2f)  Type:  %d.", pEntity->GetTransform().GetPosition().GetXf(), pEntity->GetTransform().GetPosition().GetYf(), pEntity->GetTransform().GetPosition().GetZf(), pEntity->GetType());

			// Success
			bSuccess = true;
		}
		break;

	case Mode_EntityAndOffset:
		if(pEntity)
		{
			// Get the world space lock on position of the entity
			pEntity->GetLockOnTargetAimAtPos(vTargetPosition);
			aiAssertf(!vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "CAITarget::GetPosition -- entity lock-on position is very close to the origin.  This is very likely an error.  Entity position: (%.2f, %.2f, %.2f)  Type:  %d.", pEntity->GetTransform().GetPosition().GetXf(), pEntity->GetTransform().GetPosition().GetYf(), pEntity->GetTransform().GetPosition().GetZf(), pEntity->GetType());

			// Transform the local space offset by the rotational part of the entities matrix
			// Add the offset to the return value
			vTargetPosition += VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vPosition)));

			// Success
			bSuccess = true;
		}
		break;

	case Mode_WorldSpacePosition:
		{
			// Return the value of m_vPosition
			vTargetPosition = m_vPosition;
			aiAssertf(!vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "CAITarget::GetPosition -- world space position is very close to the origin.  This is very likely an error.");

			// Success
			bSuccess = true;
		}
		break;

	case Mode_Uninitialised:
		{
			aiAssertf(0,"Trying to get a target position from an uninitialised CAITarget");
		}
		break;

	default:
		{
			// Not handled
			aiAssertf(0,"Mode [%d] not handled by CAITarget", m_mode);
		}
		break;
	}

	// Failed
	return bSuccess;
}

bool CAITarget::GetPositionOffset(Vector3& vTargetPosition) const
{
	bool bSuccess = false;

	const CEntity* pEntity = m_pEntity.GetEntity();

	switch(m_mode)
	{
	case Mode_EntityAndOffset:
		if(pEntity)
		{
			vTargetPosition = m_vPosition;

			// Success
			bSuccess = true;
		}
		break;

	case Mode_WorldSpacePosition:
	case Mode_Uninitialised:
	case Mode_Entity:
		break;

	default:
		{
			// Not handled
			aiAssertf(0,"Mode [%d] not handled by CAITarget", m_mode);
		}
		break;
	}

	// Failed
	return bSuccess;
}


bool CAITarget::GetVelocity(Vector3& vTargetVelocity) const
{
	bool bSuccess = false;

	const CEntity* pEntity = m_pEntity.GetEntity();

	switch(m_mode)
	{
	case Mode_EntityAndOffset:
	case Mode_Entity:
		if(pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				const CPed* pPed = SafeCast(const CPed, pEntity);
				const CVehicle* pPedVehicle = pPed->GetVehiclePedInside();
				vTargetVelocity = pPedVehicle ? pPedVehicle->GetVelocity() : pPed->GetVelocity();
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = SafeCast(const CVehicle, pEntity);
				vTargetVelocity = pVehicle->GetVelocity();
			}

			// Success
			bSuccess = true;
		}
		break;

	case Mode_WorldSpacePosition:
	case Mode_Uninitialised:
		break;

	default:
		{
			// Not handled
			aiAssertf(0,"Mode [%d] not handled by CAITarget", m_mode);
		}
		break;
	}

	// Failed
	return bSuccess;
}


void CAITarget::SetEntity(const CEntity* pTargetEntity)
{
	AssertMsg( pTargetEntity, "Target entity should, at least, not start NULL.  Missed a check in client code?" );
	m_mode      = Mode_Entity;
	m_pEntity.SetEntity(const_cast<CEntity*>(pTargetEntity));
	m_vPosition = VEC3_ZERO;
}

void CAITarget::SetEntityAndOffset(const CEntity* pTargetEntity, const Vector3& vTargetOffset)
{
	m_mode      = Mode_EntityAndOffset;
	m_pEntity.SetEntity(const_cast<CEntity*>(pTargetEntity));

	static dev_float MAX_OFFSET = 10.0f;
	if(aiVerifyf(vTargetOffset.Mag2() < rage::square(MAX_OFFSET), "CAITarget::SetEntityAndOffset: Offset is greater than %f - probably not an offset", MAX_OFFSET))
	{
		m_vPosition = vTargetOffset;
	}
	else
	{
		m_vPosition = VEC3_ZERO;
	}
}

// the above function is stupid.  Can't imagine why it does what it does
// added this function to avoid weird side effect of changing above function
void CAITarget::SetEntityAndOffsetUnlimited(const CEntity* pTargetEntity, const Vector3& vTargetOffset)
{
	m_mode      = Mode_EntityAndOffset;
	m_pEntity.SetEntity(const_cast<CEntity*>(pTargetEntity));

	m_vPosition = vTargetOffset;
}


void CAITarget::SetPosition(const Vector3& vTargetPosition)
{
	m_mode      = Mode_WorldSpacePosition;
	m_pEntity.Invalidate();
	m_vPosition = vTargetPosition;

	aiAssertf(!vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "CAITarget::SetPosition -- position is very close to the origin.  This is very likely an error.");
}

void CAITarget::Clear()
{
	m_mode = Mode_Uninitialised;
}

bool CAITarget::operator==(const CAITarget& otherTarget) const
{
	return (m_pEntity == otherTarget.m_pEntity) &&
			(m_vPosition.IsClose(otherTarget.m_vPosition, FLT_EPSILON)) &&
			(m_mode == otherTarget.m_mode);
}

void CAITarget::Serialise(CSyncDataBase& serialiser)
{
	m_pEntity.Serialise(serialiser, "Target");

	bool bPositionNonZero = !m_vPosition.IsZero();

	SERIALISE_BOOL(serialiser, bPositionNonZero);

	if (bPositionNonZero || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_vPosition, "Target Position");
	}
	else
	{
		m_vPosition.Zero();
	}

	static const int SIZEOF_MODE = datBitsNeeded<Mode_Max>::COUNT;
	u32 mode = (u32)m_mode;

	SERIALISE_UNSIGNED(serialiser, mode, SIZEOF_MODE, "Mode");

	m_mode = (Mode)mode;
}


