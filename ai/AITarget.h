#ifndef AI_TARGET_H
#define AI_TARGET_H

// Rage headers
#include "Vector/Vector3.h"

// Game headers
#include "Network\General\NetworkUtil.h"
#include "Scene/RegdRefTypes.h"

// Forward declarations
class CEntity;

//////////////////////////////////////////////////////////////////////////
// CAITarget
//////////////////////////////////////////////////////////////////////////

class CAITarget
{

public:

	// The type of targeting we are doing
	enum Mode
	{
		Mode_Entity = 0,
		Mode_EntityAndOffset,
		Mode_WorldSpacePosition,
		Mode_Uninitialised,

		Mode_Max,
	};

public:

	// Default constructor
	CAITarget();

	// Construct with a target entity
	CAITarget(const CEntity* pTargetEntity);

	// Construct with a target entity and a local space offset from that entity
	CAITarget(const CEntity* pTargetEntity, const Vector3& vTargetOffset);

	// Construct with a world space target position
	CAITarget(const Vector3& vTargetPosition);

	// Copy constructor
	CAITarget(const CAITarget& other);

	//
	// Accessors
	//

	// Is the target valid?
	bool GetIsValid() const;

	// Get the target position in world space
	bool GetPosition(Vector3& vTargetPosition) const;

	// Get the stored offset if mode is entity and offset
	bool GetPositionOffset(Vector3& vTargetPosition) const;

	// Get velocity
	bool GetVelocity(Vector3& vTargetVelocity) const;

	// Get the target entity
	const CEntity* GetEntity() const { return m_pEntity.GetEntity(); }
	const CEntity* GetEntity() { return m_pEntity.GetEntity(); }
	
	s32 GetMode() const { return m_mode; }

	//
	// Settors
	//

	// Set the target to be the entity
	void SetEntity(const CEntity* pTargetEntity);

	// Set the target to be the entity with a local space offset
	void SetEntityAndOffset(const CEntity* pTargetEntity, const Vector3& vTargetOffset);

	// SetEntityAndOffset is strange.  It has some weird assert and behavior for large offsets.
	// added this function to avoid weird side effects of just changing above function.  
	void SetEntityAndOffsetUnlimited(const CEntity* pTargetEntity, const Vector3& vTargetOffset);

	// Set the target to be a world space position
	void SetPosition(const Vector3& vTargetPosition);

	// Clears the target
	void Clear();

	// == and != operators
	bool operator==(const CAITarget& otherTarget) const;
	bool operator!=(const CAITarget& otherTarget) const { return !operator==(otherTarget); }

	void Serialise(CSyncDataBase& serialiser);

private:

	// The target position - 
	// If we are using a target entity, this stores the local space offset from that entity,
	// Otherwise, it stores a world space target position
	Vector3 m_vPosition;

	// The type of targeting we are doing
	Mode m_mode;

	// The target entity - will be NULL if we are using a world space position
	CSyncedEntity m_pEntity;
};

#endif // AI_TARGET_H
