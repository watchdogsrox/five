// FILE:    AirDefence.h
// PURPOSE: Spheres used by script to destroy bullets/projectiles.
// AUTHOR:  Blair T
// CREATED: 30-10-2015

#ifndef AIR_DEFENCE_H
#define AIR_DEFENCE_H

#include "atl/array.h"
#include "entity/extensiblebase.h"
#include "scene/entity.h"
#include "vectormath/vec3v.h"

class CAirDefenceZone : public fwExtensibleBase
{
	DECLARE_RTTI_DERIVED_CLASS(CAirDefenceZone, fwExtensibleBase);
public:
	u8		GetZoneId					() const { return m_uZoneId; }

	Vec3V	GetWeaponPosition			() const { return m_vWeaponPosition; }
	void    SetWeaponPosition			(Vec3V vPosition) { m_vWeaponPosition = vPosition; }
	u32		GetWeaponHash				() const { return m_uWeaponHash; }
	void	SetWeaponHash				(u32 uWeaponHash) { m_uWeaponHash = uWeaponHash; }

	void	SetPlayerIsTargettable(int iPlayerIndex, bool bTargettable);
	void	ResetPlayerTargettableFlags();
	bool	IsPlayerTargetable(CPed *pTargetPlayer) const;
	bool	IsEntityTargetable(CEntity *pTargetEntity) const;
	bool	ShouldFireWeapon() const;

	virtual bool    IsPointInside(Vec3V vPoint) const = 0;
	virtual bool	IsInsideArea(Vec3V vAreaPosition, float fAreaRadius) const = 0;
	virtual bool	DoesRayIntersect(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint) const = 0;	
	virtual void	FireWeaponAtPosition(Vec3V vTargetPosition, bool bTriggerExplosionAtTargetPosition = false) = 0;

#if __BANK
	virtual void	RenderDebug() = 0;
#endif// __BANK
	
protected:

	CAirDefenceZone(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId);

	Vec3V		m_vWeaponPosition;		// "Weapon" position; ie point to trigger weapon fire/VFX from.
	u32			m_uWeaponHash;
	u8			m_uZoneId;
	u32			m_uIsPlayerTargettable;	// Flags for each player, indicating whether this player can be attacked by the sphere.
};

class CAirDefenceSphere : public CAirDefenceZone
{
	DECLARE_RTTI_DERIVED_CLASS(CAirDefenceSphere, fwExtensibleBase);

public:
	CAirDefenceSphere(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId, Vec3V vPosition, float fRadius);
	virtual ~CAirDefenceSphere();

	bool    IsPointInside(Vec3V vPoint) const;
	bool	IsInsideArea(Vec3V vAreaPosition, float fAreaRadius) const;
	bool	DoesRayIntersect(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint) const;
	void	FireWeaponAtPosition(Vec3V vTargetPosition, bool bTriggerExplosionAtTargetPosition = false);

#if __BANK
	void	RenderDebug();
#endif// __BANK

private:
	Vec3V		m_vPosition;		// Sphere centre position
	float		m_fRadius;			// Sphere radius
};

class CAirDefenceArea : public CAirDefenceZone
{
	DECLARE_RTTI_DERIVED_CLASS(CAirDefenceArea, fwExtensibleBase);

public:
	CAirDefenceArea(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId, Vec3V vMin, Vec3V vMax, Mat34V mtx);
	virtual ~CAirDefenceArea();

	bool    IsPointInside(Vec3V vPoint) const;
	bool	IsInsideArea(Vec3V vAreaPosition, float fAreaRadius) const;
	bool	DoesRayIntersect(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint) const;
	void	FireWeaponAtPosition(Vec3V vTargetPosition, bool bTriggerExplosionAtTargetPosition = false);

#if __BANK
	void	RenderDebug();
#endif// __BANK

private:
	Vec3V		m_vMin;				// Min point of AABB
	Vec3V		m_vMax;				// Max point of AABB
	Mat34V		m_Mtx;				// Rotation / translation matrix
};

class CAirDefenceManager
{
public:
	CAirDefenceManager();
	virtual ~CAirDefenceManager();

	static void Init();

	static u8	CreateAirDefenceSphere(Vec3V vPosition, float fRadius, Vec3V vWeaponPosition, u32 uWeaponHash = 0);
	static u8	CreateAirDefenceArea(Vec3V vMin, Vec3V vMax, Mat34V mtx, Vec3V vWeaponPosition, u32 uWeaponHash = 0);

	static bool DeleteAirDefenceZone(u8 uZoneIndex);
	static bool DeleteAirDefenceZone(CAirDefenceZone *pAirDefenceZone);
	static bool DeleteAirDefenceZonesInteresectingPosition(Vec3V vPosition);
	static void ResetAllAirDefenceZones();

	static CAirDefenceZone* GetAirDefenceZone(u8 uZoneId);
	static bool AreAirDefencesActive() { return !ms_AirDefenceZones.IsEmpty(); }
	static bool IsPositionInDefenceZone(Vec3V vPosition, u8 &uZoneId);
	static bool IsDefenceZoneInArea(Vec3V vAreaPosition, float fAreaRadius, u8 &uZoneId);
	static bool DoesRayIntersectDefenceZone(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint, u8 &uZoneId);

	static void SetPlayerTargetableForAllZones(int iPlayerIndex, bool bTargettable);
	static void ResetPlayerTargetableFlagsForAllZones();
	static void ResetPlayetTargetableFlagsForZone(u8 uZoneId);
	static void FireWeaponAtPosition(u8 uZoneId, Vec3V vPosition, Vec3V &vFiredFromPosition);
	static u32  GetExplosionDetonationTime() { return EXPLOSION_DETONATION_TIME; }

#if __BANK
	static void InitWidgets();
	static void	RenderDebug();
	static bool ShouldRenderDebug() { return ms_bDebugRender; }
#endif	// __BANK

private:

	static const int MAX_AIR_DEFENCE_ZONES = 10;
	static atFixedArray<CAirDefenceZone*, MAX_AIR_DEFENCE_ZONES> ms_AirDefenceZones;
	static u8 ms_iZoneIdCounter;

	// Time (in MS) after weapon is fired to trigger an explosion VFX.
	static const int EXPLOSION_DETONATION_TIME = 50;

#if __BANK
	static void		DebugCreateAirDefenceSphere();
	static void		DebugCreateAirDefenceArea();
	static void		DebugDestroyAllAirDefenceZones();
	static void		DebugIsAirDefenceZoneInAreaAroundPlayer();
	static void		DebugFireIntersectingZoneWeaponAtLocalPlayer();
	static float	ms_fDebugRadius;
	static float	ms_fDebugWidth;
	static float	ms_fDebugPlayerRadius;
	static bool		ms_bDebugRender;
	static bool		ms_bPlayerTargettable;
	static bool		ms_bAllRemotePlayersTargettable;
	static char		ms_sDebugWeaponName[64];
#endif	// __BANK
};

#endif // AIR_DEFENCE_H