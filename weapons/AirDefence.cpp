// FILE:    AirDefence.cpp
// PURPOSE: Zones used by script to destroy bullets/projectiles.
// AUTHOR:  Blair T
// CREATED: 30-10-2015

#include "Weapons\AirDefence.h"

#include "bank\bank.h"
#include "bank\bkmgr.h"
#include "camera\CamInterface.h"
#include "grcore\debugdraw.h"
#include "Peds\Ped.h" 
#include "network\NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "vector\colors.h"
#include "vector\geometry.h"
#include "Weapons\Explosion.h"
#include "Weapons\Weapon.h"
#include "Vfx\ptfx\ptfxmanager.h"
#include "Vfx\VfxHelper.h"

// Macro to disable optimizations if set
WEAPON_OPTIMISATIONS()

// ------------------------------------------------------------------------------------
// CAirDefenceZone
// ------------------------------------------------------------------------------------

INSTANTIATE_RTTI_CLASS(CAirDefenceZone, 0x1c510839)

CAirDefenceZone::CAirDefenceZone(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId)
: m_vWeaponPosition(vWeaponPosition)
, m_uWeaponHash(uWeaponHash)
, m_uZoneId(uId)
, m_uIsPlayerTargettable(0)
{

}

void CAirDefenceZone::SetPlayerIsTargettable(int iPlayerIndex, bool bTargettable)
{
	if (gnetVerify(iPlayerIndex >= 0 && iPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS))
	{
		if (bTargettable)
		{
			m_uIsPlayerTargettable |= (1<<iPlayerIndex);
		}
		else
		{
			m_uIsPlayerTargettable &= ~(1<<iPlayerIndex);
		}
	}
}

void CAirDefenceZone::ResetPlayerTargettableFlags()
{
	m_uIsPlayerTargettable = 0;
}

bool CAirDefenceZone::IsPlayerTargetable(CPed *pTargetPlayer) const
{
	if (pTargetPlayer && pTargetPlayer->IsPlayer() && pTargetPlayer->GetNetworkObject())
	{
		int iTargetPlayerIndex = pTargetPlayer->GetNetworkObject()->GetPhysicalPlayerIndex();
		gnetAssert(iTargetPlayerIndex > -1 && iTargetPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS);
		if ((m_uIsPlayerTargettable & (1<<iTargetPlayerIndex)) != 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool CAirDefenceZone::IsEntityTargetable(CEntity *pTargetEntity) const
{
	if (NetworkInterface::IsGameInProgress() && pTargetEntity)
	{
		CPed *pTargetPlayer = NULL;

		if (pTargetEntity->GetIsTypePed())
		{
			pTargetPlayer = static_cast<CPed*>(pTargetEntity);
		}
		else if (pTargetEntity->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pTargetEntity);
			if (pVehicle)
			{
				pTargetPlayer = pVehicle->GetDriver();
			}
		}

		if (IsPlayerTargetable(pTargetPlayer))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool CAirDefenceZone::ShouldFireWeapon() const
{
	// Don't fire if we don't have a valid weapon hash
	if (m_uWeaponHash == 0)
	{
		return false;
	}

	return true;
}


// ------------------------------------------------------------------------------------
// CAirDefenceSphere
// ------------------------------------------------------------------------------------

INSTANTIATE_RTTI_CLASS(CAirDefenceSphere, 0xb3fbf2d9)

CAirDefenceSphere::CAirDefenceSphere(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId, Vec3V vPosition, float fRadius) 
	: CAirDefenceZone(vWeaponPosition, uWeaponHash, uId)
	, m_vPosition(vPosition)	
	, m_fRadius(fRadius)
{
	// Weapon should be inside the sphere (if it exists)
	if (uWeaponHash)
	{
		Assert(IsPointInside(vWeaponPosition));
	}
}

CAirDefenceSphere::~CAirDefenceSphere()
{

}

bool CAirDefenceSphere::IsPointInside(Vec3V vPoint) const
{
	float fDist = Mag(m_vPosition - vPoint).Getf();
	if (fDist <= m_fRadius)
	{
		return true;
	}

	return false;
}

bool CAirDefenceSphere::IsInsideArea(Vec3V vAreaPosition, float fAreaRadius) const
{
	float fDistance = Mag(vAreaPosition - m_vPosition).Getf();
	fDistance -= m_fRadius;
	if (fDistance < fAreaRadius)
	{
		return true;
	}

	return false;
}

bool CAirDefenceSphere::DoesRayIntersect(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint) const
{
	if (IsPointInside(vStartPoint) && IsPointInside(vEndPoint))
	{
		return false;
	}

	Vec3V vCentre = m_vPosition;
	ScalarV sRadius = ScalarVFromF32(m_fRadius);
	ScalarV sSegmentT1;
	ScalarV sUnusedT2;

	if (geomSegments::SegmentToSphereIntersections(vCentre, vStartPoint, vEndPoint, sRadius, sSegmentT1, sUnusedT2))
	{
		vHitPoint = vStartPoint + ((vEndPoint - vStartPoint) * sSegmentT1);
		return true;
	}

	return false;
}

void CAirDefenceSphere::FireWeaponAtPosition(Vec3V vTargetPosition, bool bTriggerExplosionAtTargetPosition)
{
	if (m_uWeaponHash != 0)
	{
		CWeapon tempWeapon(m_uWeaponHash);

		Vec3V vDirection = vTargetPosition - m_vWeaponPosition;

		Matrix34 tempMatrix;
		tempMatrix.Identity();
		tempMatrix.d = VEC3V_TO_VECTOR3(m_vWeaponPosition);
		tempMatrix.a = VEC3V_TO_VECTOR3(vDirection);
		tempMatrix.a.Normalize();
		tempMatrix.b.Cross(tempMatrix.a, Vector3(0.0f, 0.0f, 1.0f));
		tempMatrix.b.Normalize();
		tempMatrix.c.Cross(tempMatrix.b, tempMatrix.a);
		tempMatrix.c.Normalize();

		tempWeapon.SetMuzzlePosition(tempMatrix.d);

		Vector3 vStart = tempMatrix.d;
		Vector3 vEnd = VEC3V_TO_VECTOR3(vTargetPosition);

		CWeapon::sFireParams fireParams(NULL, tempMatrix, &vStart, &vEnd);
		fireParams.bFiredFromAirDefence = true;
		fireParams.iFireFlags.SetFlag(CWeapon::FF_SetPerfectAccuracy);

		tempWeapon.Fire(fireParams);

		// Will wrap this behind "!OwnerEntity" checks when this is added shortly (as audio will be triggered from weapon fire if we have a valid entity).
		if(tempWeapon.GetAudioComponent().GetWeaponSettings())
		{
			// Play it locally
			audSoundInitParams initParams;
			initParams.Position = tempMatrix.d;
			g_WeaponAudioEntity.CreateDeferredSound(tempWeapon.GetAudioComponent().GetWeaponSettings()->FireSound, NULL, &initParams);

			if (NetworkInterface::IsGameInProgress())
			{
				CPlayAirDefenseFireEvent::Trigger(
					tempMatrix.d, 
					tempWeapon.GetAudioComponent().GetWeaponSettings()->FireSound,
					m_fRadius);
			}
		}

		if (bTriggerExplosionAtTargetPosition)
		{
			// Probe test to detect if exploding in air (copied from CProjectile::GetIsExplodingInAir).
			bool bIsExplodingInAir = false;

			// Work out how far the line test will be
			static dev_float DIST_BELOW = 2.5f;
			float fDistBelow = DIST_BELOW;

			Vector3 vTestPos = tempMatrix.d;

			// Move it up slightly
			static dev_float DIST_ABOVE = 0.05f;
			vTestPos.z += DIST_ABOVE;

			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vTestPos, vTestPos - Vector3(0.0f, 0.0f, fDistBelow));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				bIsExplodingInAir = true;
			}

			CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_AIR_DEFENCE, VEC3V_TO_VECTOR3(vTargetPosition));
			explosionArgs.m_pEntExplosionOwner = NULL;
			explosionArgs.m_pEntIgnoreDamage = NULL;
			explosionArgs.m_vDirection = tempMatrix.a;
			explosionArgs.m_weaponHash = m_uWeaponHash;
			explosionArgs.m_bInAir = bIsExplodingInAir;
			explosionArgs.m_activationDelay = CAirDefenceManager::GetExplosionDetonationTime();

			CExplosionManager::AddExplosion(explosionArgs, NULL, false);
		}
	}
}

#if __BANK

void CAirDefenceSphere::RenderDebug() 
{
	CPed *pPed = CGameWorld::FindLocalPlayer();

	Color32 sphereColour = Color_white;
	if (pPed && IsPlayerTargetable(pPed))
	{
		sphereColour = Color_orange;
	}

	grcDebugDraw::Sphere(m_vPosition, m_fRadius, sphereColour, false);
	float fWeaponSphereRadius = Min(0.5f, m_fRadius * 0.1f);
	grcDebugDraw::Sphere(m_vWeaponPosition, fWeaponSphereRadius, Color_red, false);
}

#endif // __BANK

// ------------------------------------------------------------------------------------
// CAirDefenceArea
// ------------------------------------------------------------------------------------

INSTANTIATE_RTTI_CLASS(CAirDefenceArea, 0xa6c17d1)

	CAirDefenceArea::CAirDefenceArea(Vec3V vWeaponPosition, u32 uWeaponHash, u8 uId, Vec3V vMin, Vec3V vMax, Mat34V mtx) 
	: CAirDefenceZone(vWeaponPosition, uWeaponHash, uId)
	, m_vMin(vMin)
	, m_vMax(vMax)
	, m_Mtx(mtx)
{
	// Weapon should be inside the area (if it exists)
	if (uWeaponHash)
	{
		Assert(IsPointInside(vWeaponPosition));
	}
}

CAirDefenceArea::~CAirDefenceArea()
{

}

bool CAirDefenceArea::IsPointInside(Vec3V vPoint) const
{
	Vec3V vLocalPoint = UnTransformOrtho(m_Mtx, vPoint);
	spdAABB alignedBounds(m_vMin, m_vMax);
	return alignedBounds.ContainsPoint(vLocalPoint);
}

bool CAirDefenceArea::IsInsideArea(Vec3V vAreaPosition, float fAreaRadius) const
{
	Vec3V vLocalPosition = UnTransformOrtho(m_Mtx, vAreaPosition);
	spdAABB alignedBounds(m_vMin, m_vMax);
	return alignedBounds.IntersectsSphere(spdSphere(vLocalPosition,ScalarV(fAreaRadius)));
}

bool CAirDefenceArea::DoesRayIntersect(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint) const
{
	if (IsPointInside(vStartPoint) && IsPointInside(vEndPoint))
	{
		return false;
	}

	float fSegmentT1;
	Vector3 vNormal1; 

	// Local copies
	Vector3 vPointA = VEC3V_TO_VECTOR3(vStartPoint);
	Vector3 vPointB = VEC3V_TO_VECTOR3(vEndPoint);
	Matrix34 mtx = MAT34V_TO_MATRIX34(m_Mtx);

	// Get segment points relative to bounding box
	mtx.UnTransform(vPointA);
	mtx.UnTransform(vPointB);

	Vector3 vPointAToPointB = vPointB - vPointA;
	if (geomBoxes::TestSegmentToBox(vPointA, vPointAToPointB, VEC3V_TO_VECTOR3(m_vMin), VEC3V_TO_VECTOR3(m_vMax), &fSegmentT1, &vNormal1, NULL, NULL, NULL, NULL))
	{
		// Transform intersection back into world space
		vPointAToPointB.Scale(fSegmentT1);
		Vector3 vIntersectionPoint = vPointA + vPointAToPointB;
		mtx.Transform(vIntersectionPoint);
		vHitPoint = VECTOR3_TO_VEC3V(vIntersectionPoint);

		return true;
	}

	return false;
}

void CAirDefenceArea::FireWeaponAtPosition(Vec3V vTargetPosition, bool bTriggerExplosionAtTargetPosition)
{
	if (m_uWeaponHash != 0)
	{
		CWeapon tempWeapon(m_uWeaponHash);

		Vec3V vDirection = vTargetPosition - m_vWeaponPosition;
		float fRadius = Mag(vDirection).Getf();

		Matrix34 tempMatrix;
		tempMatrix.Identity();
		tempMatrix.d = VEC3V_TO_VECTOR3(m_vWeaponPosition);
		tempMatrix.a = VEC3V_TO_VECTOR3(vDirection);
		tempMatrix.a.Normalize();
		tempMatrix.b.Cross(tempMatrix.a, Vector3(0.0f, 0.0f, 1.0f));
		tempMatrix.b.Normalize();
		tempMatrix.c.Cross(tempMatrix.b, tempMatrix.a);
		tempMatrix.c.Normalize();

		tempWeapon.SetMuzzlePosition(tempMatrix.d);

		Vector3 vStart = tempMatrix.d;
		Vector3 vEnd = VEC3V_TO_VECTOR3(vTargetPosition);

		CWeapon::sFireParams fireParams(NULL, tempMatrix, &vStart, &vEnd);
		fireParams.bFiredFromAirDefence = true;
		fireParams.iFireFlags.SetFlag(CWeapon::FF_SetPerfectAccuracy);

		tempWeapon.Fire(fireParams);

		// Will wrap this behind "!OwnerEntity" checks when this is added shortly (as audio will be triggered from weapon fire if we have a valid entity).
		if(tempWeapon.GetAudioComponent().GetWeaponSettings())
		{
			// Play it locally
			audSoundInitParams initParams;
			initParams.Position = tempMatrix.d;
			g_WeaponAudioEntity.CreateDeferredSound(tempWeapon.GetAudioComponent().GetWeaponSettings()->FireSound, NULL, &initParams);

			if (NetworkInterface::IsGameInProgress())
			{
				CPlayAirDefenseFireEvent::Trigger(
					tempMatrix.d, 
					tempWeapon.GetAudioComponent().GetWeaponSettings()->FireSound,
					fRadius);
			}
		}

		if (bTriggerExplosionAtTargetPosition)
		{
			// Probe test to detect if exploding in air (copied from CProjectile::GetIsExplodingInAir).
			bool bIsExplodingInAir = false;

			// Work out how far the line test will be
			static dev_float DIST_BELOW = 2.5f;
			float fDistBelow = DIST_BELOW;

			Vector3 vTestPos = tempMatrix.d;

			// Move it up slightly
			static dev_float DIST_ABOVE = 0.05f;
			vTestPos.z += DIST_ABOVE;

			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vTestPos, vTestPos - Vector3(0.0f, 0.0f, fDistBelow));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				bIsExplodingInAir = true;
			}

			CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_AIR_DEFENCE, VEC3V_TO_VECTOR3(vTargetPosition));
			explosionArgs.m_pEntExplosionOwner = NULL;
			explosionArgs.m_pEntIgnoreDamage = NULL;
			explosionArgs.m_vDirection = tempMatrix.a;
			explosionArgs.m_weaponHash = m_uWeaponHash;
			explosionArgs.m_bInAir = bIsExplodingInAir;
			explosionArgs.m_activationDelay = CAirDefenceManager::GetExplosionDetonationTime();

			CExplosionManager::AddExplosion(explosionArgs, NULL, false);
		}
	}
}

#if __BANK

void CAirDefenceArea::RenderDebug() 
{
	CPed *pPed = CGameWorld::FindLocalPlayer();

	Color32 areaColour = Color_white;
	if (pPed && IsPlayerTargetable(pPed))
	{
		areaColour = Color_orange;
	}

	grcDebugDraw::Sphere(m_vWeaponPosition, 0.5f, Color_red, false);
	grcDebugDraw::BoxOriented(m_vMin, m_vMax, m_Mtx, Color_red, false);
}
#endif // __BANK

// ------------------------------------------------------------------------------------
// CAirDefenceManager
// ------------------------------------------------------------------------------------

atFixedArray<CAirDefenceZone*, CAirDefenceManager::MAX_AIR_DEFENCE_ZONES> CAirDefenceManager::ms_AirDefenceZones;
u8 CAirDefenceManager::ms_iZoneIdCounter = 0;

CAirDefenceManager::CAirDefenceManager()
{
}

CAirDefenceManager::~CAirDefenceManager()
{
}

void CAirDefenceManager::Init()
{
	ResetAllAirDefenceZones();
}

u8 CAirDefenceManager::CreateAirDefenceSphere(Vec3V vPosition, float fRadius, Vec3V vWeaponPosition, u32 uWeaponHash)
{
	if (ms_AirDefenceZones.IsFull())
		return 0;

	ms_iZoneIdCounter++;
	u8 uZoneId = ms_iZoneIdCounter;
	CAirDefenceZone *pDefZone = rage_new CAirDefenceSphere(vWeaponPosition, uWeaponHash, uZoneId, vPosition, fRadius);
	ms_AirDefenceZones.Push(pDefZone);
	return uZoneId;
}

u8 CAirDefenceManager::CreateAirDefenceArea(Vec3V vMin, Vec3V vMax, Mat34V mtx, Vec3V vWeaponPosition, u32 uWeaponHash)
{
	if (ms_AirDefenceZones.IsFull())
		return 0;

	ms_iZoneIdCounter++;
	u8 uZoneId = ms_iZoneIdCounter;
	CAirDefenceZone *pDefZone = rage_new CAirDefenceArea(vWeaponPosition, uWeaponHash, uZoneId, vMin, vMax, mtx);
	ms_AirDefenceZones.Push(pDefZone);
	return uZoneId;
}

bool CAirDefenceManager::DeleteAirDefenceZone(u8 uZoneIndex)
{
	if (!AreAirDefencesActive())
		return false;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->GetZoneId() == uZoneIndex)
		{
			ms_AirDefenceZones.Delete(i);
			return true;
		}
	}

	return false;
}

bool CAirDefenceManager::DeleteAirDefenceZone(CAirDefenceZone *pAirDefenceZone)
{
	if (!AreAirDefencesActive())
		return false;

	if (pAirDefenceZone)
	{
		for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
		{
			CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
			if (pDefZone && pDefZone == pAirDefenceZone)
			{
				ms_AirDefenceZones.Delete(i);
				return true;
			}
		}
	}

	return false;
}

bool CAirDefenceManager::DeleteAirDefenceZonesInteresectingPosition(Vec3V vPosition)
{
	if (!AreAirDefencesActive())
		return false;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->IsPointInside(vPosition))
		{
			ms_AirDefenceZones.Delete(i);
			i--;
		}
	}

	return true;
}

void CAirDefenceManager::ResetAllAirDefenceZones()
{
	ms_AirDefenceZones.Reset();
	ms_iZoneIdCounter = 0;
}

CAirDefenceZone* CAirDefenceManager::GetAirDefenceZone(u8 uZoneId)
{
	if (!AreAirDefencesActive())
		return NULL;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->GetZoneId() == uZoneId)
		{
			return pDefZone;
		}
	}

	return NULL;
}

bool CAirDefenceManager::IsPositionInDefenceZone(Vec3V vPosition, u8 &uZoneId)
{
	uZoneId = 0;

	if (!AreAirDefencesActive())
		return false;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->IsPointInside(vPosition))
		{
			uZoneId = pDefZone->GetZoneId();
			return true;
		}
	}

	return false;
}

bool CAirDefenceManager::IsDefenceZoneInArea(Vec3V vAreaPosition, float fAreaRadius, u8 &uZoneId)
{
	uZoneId = 0;

	if (!AreAirDefencesActive())
		return false;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->IsInsideArea(vAreaPosition, fAreaRadius))
		{
			uZoneId = pDefZone->GetZoneId();
			return true;
		}
	}

	return false;
}

bool CAirDefenceManager::DoesRayIntersectDefenceZone(Vec3V vStartPoint, Vec3V vEndPoint, Vec3V &vHitPoint, u8 &uZoneId)
{
	if (!AreAirDefencesActive())
		return false;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone && pDefZone->DoesRayIntersect(vStartPoint, vEndPoint, vHitPoint))
		{
			uZoneId = pDefZone->GetZoneId();
			return true;
		}
	}

	return false;
}

void CAirDefenceManager::SetPlayerTargetableForAllZones(int iPlayerIndex, bool bTargettable)
{
	if (!AreAirDefencesActive())
		return;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone)
		{
			pDefZone->SetPlayerIsTargettable(iPlayerIndex, bTargettable);
		}
	}
}

void CAirDefenceManager::ResetPlayerTargetableFlagsForAllZones()
{
	if (!AreAirDefencesActive())
		return;

	for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
	{
		CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
		if (pDefZone)
		{
			pDefZone->ResetPlayerTargettableFlags();
		}
	}
}

void CAirDefenceManager::ResetPlayetTargetableFlagsForZone(u8 uZoneId)
{
	if (!AreAirDefencesActive())
		return;

	CAirDefenceZone *pDefZone = GetAirDefenceZone(uZoneId);
	if (pDefZone)
	{
		pDefZone->ResetPlayerTargettableFlags();
	}
}

void CAirDefenceManager::FireWeaponAtPosition(u8 uZoneId, Vec3V vPosition, Vec3V &vFiredFromPosition)
{
	if (!AreAirDefencesActive())
		return;

	CAirDefenceZone *pDefZone = GetAirDefenceZone(uZoneId);
	if (pDefZone)
	{
		pDefZone->FireWeaponAtPosition(vPosition);
		vFiredFromPosition = pDefZone->GetWeaponPosition();
	}
}

#if __BANK
float	CAirDefenceManager::ms_fDebugRadius = 10.0f;
float	CAirDefenceManager::ms_fDebugWidth = 10.0f;
float	CAirDefenceManager::ms_fDebugPlayerRadius = 5.0f;
bool	CAirDefenceManager::ms_bDebugRender = false;
bool	CAirDefenceManager::ms_bPlayerTargettable = true;
bool	CAirDefenceManager::ms_bAllRemotePlayersTargettable = true;
char	CAirDefenceManager::ms_sDebugWeaponName[64] = "WEAPON_AIR_DEFENCE_GUN"; 

void CAirDefenceManager::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("Air Defence", false);
		{
			pBank->AddText("Weapon Name", ms_sDebugWeaponName, 64, false);
			pBank->AddSlider("Sphere Radius", &ms_fDebugRadius, 0.01f, 100.0f, 0.01f);
			pBank->AddButton("Create Sphere At Camera Pos", &CAirDefenceManager::DebugCreateAirDefenceSphere);
			pBank->AddSlider("Angled Area Width", &ms_fDebugWidth, 0.01f, 100.0f, 0.01f);
			pBank->AddButton("Create Angled Area At Player Pos", &CAirDefenceManager::DebugCreateAirDefenceArea);
			pBank->AddButton("Destroy All Zones", &CAirDefenceManager::DebugDestroyAllAirDefenceZones);
			pBank->AddToggle("Render Debug", &ms_bDebugRender);
			pBank->AddToggle("Set Local Player Targetable (MP only)", &ms_bPlayerTargettable);
			pBank->AddToggle("Set All Remote Players Targetable (MP only)", &ms_bAllRemotePlayersTargettable);
			pBank->AddSlider("Area Around Player Radius", &ms_fDebugPlayerRadius, 0.01f, 100.0f, 0.01f);
			pBank->AddButton("Is Zone In Area Around Player", CAirDefenceManager::DebugIsAirDefenceZoneInAreaAroundPlayer);
			pBank->AddButton("Fire Intersecting Zone's Weapon at Local Player", CAirDefenceManager::DebugFireIntersectingZoneWeaponAtLocalPlayer);
		}
		pBank->PopGroup();
	}
}

void CAirDefenceManager::DebugCreateAirDefenceSphere()
{
	Vec3V vPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	u32 uWeaponHash = atHashString(ms_sDebugWeaponName);
	u8 uZoneId = CAirDefenceManager::CreateAirDefenceSphere(vPos, ms_fDebugRadius, vPos, uWeaponHash);

	if (NetworkInterface::IsGameInProgress())
	{
		CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uZoneId);
		if (pDefZone && NetworkInterface::GetLocalPlayer())
		{
			if (ms_bAllRemotePlayersTargettable)
			{
				unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
				const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
				for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
				{
					const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
					if(remotePlayer && remotePlayer->GetPlayerPed() && remotePlayer->GetPhysicalPlayerIndex() != NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex())
					{
						pDefZone->SetPlayerIsTargettable(remotePlayer->GetPhysicalPlayerIndex(), ms_bAllRemotePlayersTargettable);
					}
				}
			}

			if (ms_bPlayerTargettable)
			{
				pDefZone->SetPlayerIsTargettable(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex(), ms_bAllRemotePlayersTargettable);
			}
		}
	}

	// Stream in VFX assets (done script-side normally)
	const char* pPtFxAssetName = "scr_apartment_mp";
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atHashValue(pPtFxAssetName));
	if (slot.IsValid())
	{
		ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_DONTDELETE);
	}
}

void CAirDefenceManager::DebugCreateAirDefenceArea()
{
	Vec3V vWeaponPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
	u32 uWeaponHash = atHashString(ms_sDebugWeaponName);

	// Create bounding box (at player pos)
	float extent = ms_fDebugWidth / 2.0f;
	Vec3V vMax = Vec3V(extent, extent, extent);
	Vec3V vMin = -vMax;
	Mat34V mtx = CGameWorld::FindLocalPlayer()->GetMatrix();

	u8 uZoneId = CAirDefenceManager::CreateAirDefenceArea(vMin, vMax, mtx, vWeaponPos, uWeaponHash);

	if (NetworkInterface::IsGameInProgress())
	{
		CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uZoneId);
		if (pDefZone && NetworkInterface::GetLocalPlayer())
		{
			if (ms_bAllRemotePlayersTargettable)
			{
				unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
				const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
				for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
				{
					const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
					if(remotePlayer && remotePlayer->GetPlayerPed() && remotePlayer->GetPhysicalPlayerIndex() != NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex())
					{
						pDefZone->SetPlayerIsTargettable(remotePlayer->GetPhysicalPlayerIndex(), ms_bAllRemotePlayersTargettable);
					}
				}
			}

			if (ms_bPlayerTargettable)
			{
				pDefZone->SetPlayerIsTargettable(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex(), ms_bAllRemotePlayersTargettable);
			}
		}
	}

	// Stream in VFX assets (done script-side normally)
	const char* pPtFxAssetName = "scr_apartment_mp";
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atHashValue(pPtFxAssetName));
	if (slot.IsValid())
	{
		ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_DONTDELETE);
	}
}

void CAirDefenceManager::DebugDestroyAllAirDefenceZones()
{
	CAirDefenceManager::ResetAllAirDefenceZones();

	// Release VFX assets (done script-side normally)
	const char* pPtFxAssetName = "scr_apartment_mp";
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atHashValue(pPtFxAssetName));
	if (slot.IsValid())
	{
		ptfxManager::GetAssetStore().ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);
	}
}

void CAirDefenceManager::DebugIsAirDefenceZoneInAreaAroundPlayer()
{
	CPed *pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		u8 uZoneIndex = 0;
		bool bZoneInArea = false;
		if (CAirDefenceManager::IsDefenceZoneInArea(pPed->GetTransform().GetPosition(), ms_fDebugPlayerRadius, uZoneIndex))
		{
			bZoneInArea = true;
		}

		Color32 color = bZoneInArea ? Color_red : Color_green;
		grcDebugDraw::Sphere(pPed->GetTransform().GetPosition(), ms_fDebugPlayerRadius, color, false, 200);
	}
}

void CAirDefenceManager::DebugFireIntersectingZoneWeaponAtLocalPlayer()
{
	CPed *pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		u8 uZoneId = 0;
		if (CAirDefenceManager::IsPositionInDefenceZone(pPed->GetTransform().GetPosition(), uZoneId))
		{
			CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uZoneId);
			if (pDefZone)
			{
				pDefZone->FireWeaponAtPosition(pPed->GetTransform().GetPosition(), true);
			}
		}
	}
}

void CAirDefenceManager::RenderDebug()
{
	if (ms_bDebugRender)
	{
		for (int i = 0; i < ms_AirDefenceZones.GetCount(); i++)
		{
			CAirDefenceZone *pDefZone = ms_AirDefenceZones[i];
			if (pDefZone)
			{
				pDefZone->RenderDebug();
			}
		}
	}
}
#endif	// __BANK