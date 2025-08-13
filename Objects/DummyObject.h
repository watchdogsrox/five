// Title	:	DummyObject.h
// Author	:	Richard Jobling
// Started	:	19/04/2000

#ifndef _DUMMYOBJECT_H_
#define _DUMMYOBJECT_H_

// Rage include:
#include "diag/tracker.h"		// RAGE_TRACK()
#include "fwtl/pool.h"

// game include:
#include "control/replay/ReplaySettings.h"
#include "Scene/Entity.h"

class CObject;

class CDummyObject : public CEntity
{
	friend class CWorldRepSectorArray;

private:

	void Hide(const bool visible, const bool skipUpdateWorldFromEntity = false);

protected:

	void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex);

	void Add();
	void Add(const fwRect& rect);
	void Remove();

public:
	FW_REGISTER_CLASS_POOL(CDummyObject);


	CDummyObject(const eEntityOwnedBy ownedBy);
	CDummyObject(const eEntityOwnedBy ownedBy, CObject* pObject);
	~CDummyObject();

	CObject* CreateObject();
	void UpdateFromObject(CObject* pObject, const bool skipUpdateWorldFromEntity = false);

	void Enable();
	void Disable();

	void ForceIsVisibleExtension();

#if GTA_REPLAY
public:
	void	GenerateHash(const Vector3 &position);
	u32		GetHash() const		{	return m_hash;	}
private:
	u32		m_hash;
#endif // GTA_REPLAY
};

#if __BANK
// forward declare so we don't get multiple definitions
template<> void fwPool<CDummyObject>::PoolFullCallback();
#endif

#endif
