
// Rage headers
#include "script/wrapper.h"
// Game headers
#include "physics/WorldProbe/worldprobe.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "security/plugins/scripthook.h"
#include "network/NetworkInterface.h"
#include "vehicles/vehicle.h"
#include "peds/ped.h"
#include "vfx/misc/fire.h"
#include "weapons/explosion.h"

#include "network/Live/NetworkTelemetry.h"
#include "network/General/NetworkAssetVerifier.h"
#include "network/Network.h"


namespace fire_commands
{

static bool sm_beenBusted = false;
int CommandStartScriptFire(const scrVector &scrVecCoors, int numGenerations, bool isPetrolFire)
{
    int fireID = scriptResource::INVALID_RESOURCE_ID;

    if(SCRIPT_VERIFY(numGenerations <= FIRE_DEFAULT_NUM_GENERATIONS, "Number of fire generations is too high!"))
    {
	    Vector3 VecPos = Vector3 (scrVecCoors);

	    if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	    {
		    VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
	    }

	    CScriptResource_Fire fire(VecPos, numGenerations, isPetrolFire);

	    fireID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(fire);
    }

    return fireID;
}


void CommandRemoveScriptFire(int FireIndex)
{
	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(FireIndex, false, true, CGameScriptResource::SCRIPT_RESOURCE_FIRE);
}


int CommandStartEntityFire(int EntityIndex)
{
	CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex);
	if(pEntity)
	{
		Vec3V TempCoors = pEntity->GetTransform().GetPosition();

		return g_fireMan.StartScriptFire(TempCoors, pEntity);
	}

	return -1;
}

void CommandStopEntityFire(int EntityIndex)
{
	CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex);

	if(pEntity)
	{
		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			pVehicle->ExtinguishCarFire();
		}
		else
		{
			//@@:  location FIRE_COMMANDS_COMMANDSTOPENTITYFIRE_EXTINGUISH_FIRES
			g_fireMan.ExtinguishEntityFires(pEntity);
		}
	}
}


bool CommandIsEntityOnFire(int EntityIndex)
{
	bool bOnFire = false;

	const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if(pEntity)
	{	
		if (pEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

			if(pVehicle->IsOnFire())
				bOnFire = true;
		}
		else if (g_fireMan.IsEntityOnFire(pEntity))
		{
			bOnFire = true;
		}
	}

	return bOnFire;
}

void CommandSetScriptFireAudio(int UNUSED_PARAM(FireIndex), bool UNUSED_PARAM(bAudioOn))
{
//	g_fireMan.SetScriptFireAudio(CTheScripts::GetActualScriptThingIndex(FireIndex, UNIQUE_SCRIPT_FIRE), bAudioOn);
}


int CommandGetNumberOfFiresInRange(const scrVector &scrVecCoors, float Radius)
{
	return g_fireMan.GetNumFiresInRange(VECTOR3_TO_VEC3V((Vector3)scrVecCoors), Radius);	
}


void CommandSetFlammabilityMultiplier(float multiplier)
{
	g_fireMan.SetScriptFlammabilityMultiplier(multiplier, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}

void CommandStopFireInRange(const scrVector &scrVecCoors, float fRadius)
{
	g_fireMan.ExtinguishArea(VECTOR3_TO_VEC3V((Vector3)scrVecCoors), fRadius);
}

/*
bool CommandGetClosestFirePos(float& closestFirePosX, float& closestFirePosY, float& closestFirePosZ, const scrVector &scrVecTestPos)
{
	Vec3f closestFirePos(closestFirePosX, closestFirePosY, closestFirePosZ);
	Vec3f testPos = RCC_VEC3F(scrVecTestPos);
	if (g_fireMan.GetClosestFirePos(closestFirePos, testPos))
	{
		closestFirePosX = closestFirePos.GetX();
		closestFirePosY = closestFirePos.GetY();
		closestFirePosZ = closestFirePos.GetZ();
		return true;
	}

	return false;
}*/


bool CommandGetClosestFirePos(Vector3& closestFirePos, const scrVector &scrVecTestPos)
{
	Vec3V vClosestFirePos2 = RCC_VEC3V(closestFirePos);
	Vec3V vTestPos = VECTOR3_TO_VEC3V((Vector3)scrVecTestPos);
	if (g_fireMan.GetClosestFirePos(vClosestFirePos2, vTestPos))
	{
		closestFirePos = RCC_VECTOR3(vClosestFirePos2);
		return true;
	}

	return false;
}

bool CanCreateExplosionTypeFromScript(int ExplosionTag)
{
	const CGameScriptId& scriptId = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId());

	// Only allow the orbital cannon explosion to be spawned via the correct script
	if (ExplosionTag == EXP_TAG_ORBITAL_CANNON && !(scriptId.GetScriptNameHash() == ATSTRINGHASH("am_mp_defunct_base", 0xcab8a943) || 
													scriptId.GetScriptNameHash() == ATSTRINGHASH("am_mp_orbital_cannon", 0x6c25dccb)))
	{
		// Do bad stuff here if you want...
		scriptAssertf(0, "Trying to create orbital cannon explosion from invalid script %s!", scriptId.GetScriptName());
		return false;
	}
	return true;
}

void CommandAddExplosion(const scrVector &scrVecCoors, int ExplosionTag, float sizeScale, bool makeSound, bool noFx, float camShakeMult, bool noDamage)
{
	CExplosionManager::CExplosionArgs explosionArgs(static_cast<eExplosionTag>(ExplosionTag), scrVecCoors);

	explosionArgs.m_sizeScale = sizeScale;
	explosionArgs.m_bMakeSound = makeSound;
	explosionArgs.m_bNoFx = noFx;
	explosionArgs.m_fCamShake = camShakeMult;
	explosionArgs.m_bNoDamage = noDamage;
	// Need to set weapon hash for smoke grenades to make sure peds to react properly.
	if(ExplosionTag == EXP_TAG_SMOKEGRENADELAUNCHER || ExplosionTag == EXP_TAG_SMOKEGRENADE)
	{
		explosionArgs.m_weaponHash = WEAPONTYPE_SMOKEGRENADE;
	}
	SCRIPT_CHECK_CALLING_FUNCTION
	if (CanCreateExplosionTypeFromScript(ExplosionTag))
	{
		CExplosionManager::AddExplosion(explosionArgs);	
	}
}

void CommandAddOwnedExplosion(int explosionOwner, const scrVector &scrVecCoors, int ExplosionTag, float sizeScale, bool makeSound, bool noFx, float camShakeMult)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(explosionOwner);
	if (SCRIPT_VERIFY(pEntity, "ADD_OWNED_EXPLOSION called with invalid entity as owner"))
	{
		CExplosionManager::CExplosionArgs explosionArgs(static_cast<eExplosionTag>(ExplosionTag), scrVecCoors);

		explosionArgs.m_pEntExplosionOwner = pEntity;
		explosionArgs.m_sizeScale = sizeScale;
		explosionArgs.m_bMakeSound = makeSound;
		explosionArgs.m_bNoFx = noFx;
		explosionArgs.m_fCamShake = camShakeMult;

		if(!sm_beenBusted)
		{
			// Lets get local player information
			CPed* localPlayer = CGameWorld::FindLocalPlayer();
		
			// Now lets get the ped-id
			int localPlayerId = CTheScripts::GetGUIDFromEntity(*localPlayer);
			// And get a bool indicating if it's the local player, or not
			bool isOwnedByLocalPlayer = (explosionOwner == localPlayerId);
			// Fetch multiplayer information
			bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
				&& NetworkInterface::IsGameInProgress() 
				&& !NetworkInterface::IsInSinglePlayerPrivateSession();

			if(!isOwnedByLocalPlayer && isMultiplayer)
			{
				// Assert for our QA people to know what's up
				scriptAssertf(0, "Mis-assigned explosion fired in multi-player against %d", explosionOwner);
				// Trigger a telemetry event
				CNetworkTelemetry::AppendInfoMetric(0x8AD6B442, 0xD917AC05, 0xFFA776AA);
				// Smash the match making key
				CNetwork::GetAssetVerifier().ClobberTamperCrcAndRefresh();
				// Kick the player
				CNetwork::Bail(sBailParameters(BAIL_NETWORK_ERROR, BAIL_CTX_NETWORK_ERROR_CHEAT_DETECTED));
				sm_beenBusted = true;
			}
			else if (CanCreateExplosionTypeFromScript(ExplosionTag))
			{
				CExplosionManager::AddExplosion(explosionArgs);	
			}
		}
		else if (CanCreateExplosionTypeFromScript(ExplosionTag))
		{
			CExplosionManager::AddExplosion(explosionArgs);	
		}
	}
}


void CommandAddExplosionWithUserVfx(const scrVector &scrVecCoors , int ExplosionTag, int vfxTagHash, float sizeScale, bool makeSound, bool noFx, float camShakeMult)
{
	CExplosionManager::CExplosionArgs explosionArgs(static_cast<eExplosionTag>(ExplosionTag), scrVecCoors);

	explosionArgs.m_sizeScale = sizeScale;
	explosionArgs.m_bMakeSound = makeSound;
	explosionArgs.m_bNoFx = noFx;
	explosionArgs.m_fCamShake = camShakeMult;
	explosionArgs.m_vfxTagHash = (u32)vfxTagHash;

	if (CanCreateExplosionTypeFromScript(ExplosionTag))
	{
		CExplosionManager::AddExplosion(explosionArgs);	
	}
}

bool IsExplosionInArea( eExplosionTag ExplosionTag, const scrVector &scrVecCoorsMin, const scrVector &scrVecCoorsMax, bool bIncludeDelayedExplosions )
{
	bool LatestCmpFlagResult = false;
	Vector3 VecPosMin = Vector3 (scrVecCoorsMin);
	Vector3 VecPosMax = Vector3 (scrVecCoorsMax);

	float temp_float;

	if (VecPosMin.x > VecPosMax.x)
	{
		temp_float = VecPosMin.x;
		VecPosMin.x = VecPosMax.x;
		VecPosMax.x = temp_float;
	}

	if (VecPosMin.y > VecPosMax.y)
	{
		temp_float = VecPosMin.y;
		VecPosMin.y = VecPosMax.y;
		VecPosMax.y = temp_float;
	}

	if (VecPosMin.z > VecPosMax.z)
	{
		temp_float = VecPosMin.z;
		VecPosMin.z = VecPosMax.z;
		VecPosMax.z = temp_float;
	}

	if( CExplosionManager::TestForExplosionsInArea( ExplosionTag, VecPosMin, VecPosMax, bIncludeDelayedExplosions ) )
	{
		LatestCmpFlagResult = TRUE;
	}

	return LatestCmpFlagResult;
}


bool CommandIsExplosionInArea( int ExplosionTag, const scrVector &scrVecCoorsMin, const scrVector &scrVecCoorsMax )
{
	eExplosionTag ExplosionTagEnum = (eExplosionTag) ExplosionTag;
	if( SCRIPT_VERIFY( ( ExplosionTagEnum == EXP_TAG_DONTCARE ) || 
					   ( ( ExplosionTagEnum >= EXP_TAG_GRENADE ) && 
					     ( ExplosionTagEnum < EEXPLOSIONTAG_MAX ) ), "IS_EXPLOSION_IN_AREA - First parameter must be taken from the explosion type list (or \"EXP_TAG_DONTCARE\" for any type)"))
	{
		return IsExplosionInArea( ExplosionTagEnum, scrVecCoorsMin, scrVecCoorsMax, true );
	}

	return false;
}

bool CommandIsExplosionActiveInArea( int ExplosionTag, const scrVector &scrVecCoorsMin, const scrVector &scrVecCoorsMax )
{
	eExplosionTag ExplosionTagEnum = (eExplosionTag) ExplosionTag;
	if( SCRIPT_VERIFY( ( ExplosionTagEnum == EXP_TAG_DONTCARE ) || 
					   ( ( ExplosionTagEnum >= EXP_TAG_GRENADE ) && 
					      ( ExplosionTagEnum < EEXPLOSIONTAG_MAX ) ), "IS_ACTIVE_EXPLOSION_IN_AREA - First parameter must be taken from the explosion type list (or \"EXP_TAG_DONTCARE\" for any type)"))
	{
		return IsExplosionInArea( ExplosionTagEnum, scrVecCoorsMin, scrVecCoorsMax, false );
	}

	return false;
}


bool CommandIsExplosionInSphere( int ExplosionTag, const scrVector &scrVecCoors,float fRadius)
{
	eExplosionTag ExplosionTagEnum = (eExplosionTag) ExplosionTag;
	bool LatestCmpFlagResult = false;
	if(SCRIPT_VERIFY( (ExplosionTagEnum == EXP_TAG_DONTCARE) || ( (ExplosionTagEnum >= EXP_TAG_GRENADE) && (ExplosionTagEnum < EEXPLOSIONTAG_MAX) ), "IS_EXPLOSION_IN_SPHERE - First parameter must be taken from the explosion type list (or \"EXP_TAG_DONTCARE\" for any type)"))
	{
		Vector3 vCentre = Vector3(scrVecCoors);

		if (CExplosionManager::TestForExplosionsInSphere( ExplosionTagEnum, vCentre, fRadius))
		{
			LatestCmpFlagResult = TRUE;
		}
	}

	return LatestCmpFlagResult;
}

int CommandGetOwnerOfExplosionInSphere( int ExplosionTag, const scrVector &scrVecCoors, float fRadius)
{
	eExplosionTag ExplosionTagEnum = (eExplosionTag) ExplosionTag;
	if(SCRIPT_VERIFY( (ExplosionTagEnum == EXP_TAG_DONTCARE) || ( (ExplosionTagEnum >= EXP_TAG_GRENADE) && (ExplosionTagEnum < EEXPLOSIONTAG_MAX) ), "IS_EXPLOSION_IN_SPHERE - First parameter must be taken from the explosion type list (or \"EXP_TAG_DONTCARE\" for any type)"))
	{
		Vector3 vCentre = Vector3(scrVecCoors);
		CEntity* pEntity = CExplosionManager::GetOwnerOfExplosionInSphere(ExplosionTagEnum, vCentre, fRadius);
		if(pEntity)
		{
			return CTheScripts::GetGUIDFromEntity(*pEntity);
		}
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}

bool CommandIsExplosionInAngledArea(int ExplosionTag, const scrVector &scrVecCoors1, const scrVector &scrVecCoors2, float AreaWidth)
{
	Vector3 pos1 = Vector3(scrVecCoors1);
	Vector3 pos2 = Vector3(scrVecCoors2);
	return CExplosionManager::TestForExplosionsInAngledArea((eExplosionTag)ExplosionTag, pos1, pos2, AreaWidth);
}

int CommandGetOwnerOfExplosionInAngledArea(int ExplosionTag, const scrVector &scrVecCoors1, const scrVector &scrVecCoors2, float AreaWidth)
{
	Vector3 pos1 = Vector3(scrVecCoors1);
	Vector3 pos2 = Vector3(scrVecCoors2);
	CEntity* pEntity = CExplosionManager::GetOwnerOfExplosionInAngledArea((eExplosionTag)ExplosionTag, pos1, pos2, AreaWidth);
	if(!pEntity)
	{
		return NULL_IN_SCRIPTING_LANGUAGE;
	}
	else
	{
		return CTheScripts::GetGUIDFromEntity(*pEntity);
	}
}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(START_SCRIPT_FIRE,0x8669ffc323be936c, CommandStartScriptFire);
		SCR_REGISTER_SECURE(REMOVE_SCRIPT_FIRE,0xd97dcb0ed4ff04f8, CommandRemoveScriptFire);

		SCR_REGISTER_SECURE(START_ENTITY_FIRE,0xa4dff3132a6b8cbe, CommandStartEntityFire);
		SCR_REGISTER_SECURE(STOP_ENTITY_FIRE,0x92767b52c3080045, CommandStopEntityFire);
		SCR_REGISTER_SECURE(IS_ENTITY_ON_FIRE,0xef9410c312f2b117, CommandIsEntityOnFire);

		SCR_REGISTER_SECURE(GET_NUMBER_OF_FIRES_IN_RANGE,0x3fde5764a07ba515, CommandGetNumberOfFiresInRange);
		SCR_REGISTER_SECURE(SET_FLAMMABILITY_MULTIPLIER,0x911276e90cdc84c5, CommandSetFlammabilityMultiplier);
		SCR_REGISTER_SECURE(STOP_FIRE_IN_RANGE,0x85050cac8798cdd0, CommandStopFireInRange);

		SCR_REGISTER_SECURE(GET_CLOSEST_FIRE_POS,0xd0773d0c37d0a612, CommandGetClosestFirePos);

		SCR_REGISTER_SECURE(ADD_EXPLOSION,0x45b4bdaa12353e7d, CommandAddExplosion);
		SCR_REGISTER_SECURE(ADD_OWNED_EXPLOSION,0x5e0a11b95ee2e0c6, CommandAddOwnedExplosion);
		SCR_REGISTER_SECURE(ADD_EXPLOSION_WITH_USER_VFX,0x160873c52c1196be, CommandAddExplosionWithUserVfx);
		SCR_REGISTER_SECURE(IS_EXPLOSION_IN_AREA,0x8fe34bf4165f5104, CommandIsExplosionInArea);
		SCR_REGISTER_SECURE(IS_EXPLOSION_ACTIVE_IN_AREA,0x3791ad930b5f2dba, CommandIsExplosionActiveInArea);
		SCR_REGISTER_SECURE(IS_EXPLOSION_IN_SPHERE,0x89d0e0233f6e59a7, CommandIsExplosionInSphere);
		SCR_REGISTER_SECURE(GET_OWNER_OF_EXPLOSION_IN_SPHERE,0xc89c451fdf36b99a, CommandGetOwnerOfExplosionInSphere);
		SCR_REGISTER_SECURE(IS_EXPLOSION_IN_ANGLED_AREA,0x44808d06c0ff7d30, CommandIsExplosionInAngledArea);
		SCR_REGISTER_SECURE(GET_OWNER_OF_EXPLOSION_IN_ANGLED_AREA,0xda5270cbba70753a, CommandGetOwnerOfExplosionInAngledArea);
	}
}	//	end of namespace fire_commands
