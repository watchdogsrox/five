//
// weapons/weaponcomponentlasersight.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_LASER_SIGHT_H
#define WEAPON_COMPONENT_LASER_SIGHT_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"
#include "physics/WorldProbe/worldprobe.h"
#include "vector/vector3.h"


////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentLaserSightInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentLaserSightInfo, CWeaponComponentInfo, WCT_LaserSight);
public:

	CWeaponComponentLaserSightInfo();
	virtual ~CWeaponComponentLaserSightInfo();

	// PURPOSE: Get the corona size
	f32 GetCoronaSize() const;

	// PURPOSE: Get the corona intensity
	f32 GetCoronaIntensity() const;

	// PURPOSE: Get the laser sight bone helper
	const CWeaponBoneId& GetLaserSightBone() const;

private:

	//
	// Members
	//

	// PURPOSE: Corona size
	f32 m_CoronaSize;

	// PURPOSE: Corona intensity
	f32 m_CoronaIntensity;

	// PURPOSE: The corresponding bone index for the light emit point
	CWeaponBoneId m_LaserSightBone;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentLaserSight : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentLaserSight, CWeaponComponent, WCT_LaserSight);
public:

	CWeaponComponentLaserSight();
	CWeaponComponentLaserSight(const CWeaponComponentLaserSightInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentLaserSight();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Access the const info
	const CWeaponComponentLaserSightInfo* GetInfo() const;

protected:
	// PURPOSE: get the laser end point using probe results or possibly old results if async
	bool GetEndPoint(Vector3& oEnd);

	// PURPOSE: fire all fx, decals, coronas for a laser sight
	//void DrawLaserSight(Matrix34& inLaserSightMatrix,Vector3& inEnd);

	// PURPOSE: submit shape test probes for the laser, sync or async
	void SubmitProbe(CEntity* pFiringEntity, Vector3& inStart, Vector3& inEnd, bool inAsync);

private:
	
	//
	// Members
	//

	// PURPOSE: The corresponding bone index for the light emit point
	s32 m_iLaserSightBoneIdx;

	// PURPOSE: Current offset value
	f32 m_fOffset;

	// PURPOSE: Target offset
	f32 m_fOffsetTarget;

	// PURPOSE: store async shapetest probe results
	WorldProbe::CShapeTestSingleResult* m_pShapeTest;

	// PURPOSE: cache our latest results if async isn't ready after 1 frame
	Vector3*	m_pvOldProbeEnd;

	// PURPOSE: in the async case, are we using results older than 1 frame
	bool		m_bHaveOldProbeResults;
};

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentLaserSightInfo::GetCoronaSize() const
{
	return m_CoronaSize;
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentLaserSightInfo::GetCoronaIntensity() const
{
	return m_CoronaIntensity;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponBoneId& CWeaponComponentLaserSightInfo::GetLaserSightBone() const
{
	return m_LaserSightBone;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentLaserSightInfo* CWeaponComponentLaserSight::GetInfo() const
{
	return static_cast<const CWeaponComponentLaserSightInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_LASER_SIGHT_H
