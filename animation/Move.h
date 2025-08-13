
#ifndef MOVE_H
#define MOVE_H

// rage includes
#include "atl/functor.h"
#include "Move_config.h"
#include "fwtl/pool.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "crmotiontree/iterator.h"
#include "crmotiontree/motiontree.h"
#include "fwutil/Flags.h"
#include "move/move.h"
#include "move/move_fixedheap.h"
#include "move/move_parameterbuffer.h"
#include "move/move_subnetwork.h"

// game includes
#include "animation/anim_channel.h"
#include "control/replay/replay.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "fwanimation/move.h"

namespace rage {
	class fwAnimDirector;
}

class CDynamicEntity;
class CSimpleBlendHelper;
class CGenericClipHelper;
class CClipHelper;



class CClipNetworkMoveInfo
{
public:

	static fwMvNetworkDefId ms_NetworkAnimatedBuilding;
	static fwMvNetworkDefId ms_NetworkTaskCowerScenario;
	static fwMvNetworkDefId ms_NetworkGenericClip;
	static fwMvNetworkDefId ms_NetworkHelicopterRappel;
	static fwMvNetworkDefId ms_NetworkTaskRappel;
	static fwMvNetworkDefId ms_NetworkMotionDiving;
	static fwMvNetworkDefId ms_NetworkMotionPed;
	static fwMvNetworkDefId ms_NetworkMotionPedLowLod;
	static fwMvNetworkDefId ms_NetworkMotionAnimal;
	static fwMvNetworkDefId ms_NetworkMotionSwimming;
	static fwMvNetworkDefId ms_NetworkObject;
	static fwMvNetworkDefId ms_NetworkWeapon;
	static fwMvNetworkDefId ms_NetworkPed;
	static fwMvNetworkDefId ms_NetworkPm;
	static fwMvNetworkDefId ms_NetworkSyncedScene;
	static fwMvNetworkDefId ms_NetworkTask;
	static fwMvNetworkDefId ms_NetworkTaskAimAndThrowProjectile;	
	static fwMvNetworkDefId ms_NetworkTaskAimAndThrowProjectileCover;
	static fwMvNetworkDefId ms_NetworkTaskAimAndThrowProjectileThrow;
	static fwMvNetworkDefId ms_NetworkTaskAimGunBlindFire;
	static fwMvNetworkDefId ms_NetworkTaskAimGunDriveBy;	
	static fwMvNetworkDefId ms_NetworkTaskAimGunDriveByHeli;
	static fwMvNetworkDefId ms_NetworkTaskAimGunOnFoot;
	static fwMvNetworkDefId ms_NetworkTaskAimGunScripted;
	static fwMvNetworkDefId ms_NetworkTaskBlendFromNM;
	static fwMvNetworkDefId ms_NetworkTaskCover;
	static fwMvNetworkDefId ms_NetworkTaskUseCover;
	static fwMvNetworkDefId ms_NetworkTaskMotionInCover;
	static fwMvNetworkDefId ms_NetworkTaskDismountAnimal;
	static fwMvNetworkDefId ms_NetworkTaskDyingDead;
	static fwMvNetworkDefId ms_NetworkTaskAlign;
	static fwMvNetworkDefId ms_NetworkTaskEnterVehicle;
	static fwMvNetworkDefId ms_NetworkTaskExitVehicle;
	static fwMvNetworkDefId ms_NetworkVehicleBicycle;
	static fwMvNetworkDefId ms_NetworkTaskMotionOnBicycle;
	static fwMvNetworkDefId ms_NetworkTaskInVehicle;
	static fwMvNetworkDefId ms_NetworkTaskInVehicleSeatShuffle;
	static fwMvNetworkDefId ms_NetworkTaskMountAnimal;
	static fwMvNetworkDefId ms_NetworkTaskMountedThrow;
	static fwMvNetworkDefId ms_NetworkTaskMountedDriveBy;
	static fwMvNetworkDefId ms_NetworkTaskOnFoot;
	static fwMvNetworkDefId ms_NetworkTaskOnFootHuman;
	static fwMvNetworkDefId ms_NetworkTaskOnFootHumanLowLod;
	static fwMvNetworkDefId ms_NetworkTaskOnFootBird;	
	static fwMvNetworkDefId ms_NetworkTaskOnFootQuad;
	static fwMvNetworkDefId ms_NetworkTaskFish;
	static fwMvNetworkDefId ms_NetworkTaskOnFootFlightlessBird;
	static fwMvNetworkDefId ms_NetworkTaskOnFootHorse;
	static fwMvNetworkDefId ms_NetworkTaskJumpMounted;
	static fwMvNetworkDefId ms_NetworkTaskOnFootOpenDoor;
	static fwMvNetworkDefId ms_NetworkTaskOnFootStrafing;
	static fwMvNetworkDefId ms_NetworkTaskOnFootAiming;
	static fwMvNetworkDefId ms_NetworkTaskMotionTennis;
	static fwMvNetworkDefId ms_NetworkTaskParachute; 
	static fwMvNetworkDefId ms_NetworkTaskParachuteObject;
	static fwMvNetworkDefId ms_NetworkTaskReactAimWeapon; 
	static fwMvNetworkDefId ms_NetworkTaskReactInDirection; 
	static fwMvNetworkDefId ms_NetworkTaskRideHorse;
	static fwMvNetworkDefId ms_NetworkTaskScriptedAnimation;
	static fwMvNetworkDefId ms_NetworkTaskVault;
	static fwMvNetworkDefId ms_NetworkTaskDropDown;
	static fwMvNetworkDefId ms_NetworkTaskFall;
	static fwMvNetworkDefId ms_NetworkTaskJetpack;
	static fwMvNetworkDefId ms_NetworkTaskFallHorse;
	static fwMvNetworkDefId ms_NetworkTaskClimbLadder; 
	static fwMvNetworkDefId ms_NetworkTaskReloadGun;
	static fwMvNetworkDefId ms_NetworkTaskCutScene; 
	static fwMvNetworkDefId ms_NetworkTaskMobilePhone;
	static fwMvNetworkDefId ms_NetworkTaskArrest;
	static fwMvNetworkDefId ms_NetworkTaskCuffed;
	static fwMvNetworkDefId ms_NetworkTaskWrithe;
	static fwMvNetworkDefId ms_NetworkTaskRepositionMove;
	static fwMvNetworkDefId ms_NetworkTaskMotionAimingToOnFootTransition;
	static fwMvNetworkDefId ms_NetworkTaskMotionOnFootToAimingTransition;
	static fwMvNetworkDefId ms_NetworkTaskCombatRoll;
	static fwMvNetworkDefId ms_TaskIncapacitatedNetwork;
	static fwMvNetworkDefId ms_TaskIncapacitatedInVehicleNetwork;

	static fwMvNetworkDefId ms_NetworkBeginScriptedMiniGames;
	static fwMvNetworkDefId ms_NetworkMgBenchPress;
	static fwMvNetworkDefId ms_NetworkMgSquats;
	static fwMvNetworkDefId ms_NetworkEndScriptedMiniGames;

	static fwMvNetworkDefId ms_NetworkTaskOnFootDuckAndCover;
	static fwMvNetworkDefId ms_NetworkTaskOnFootSlopeScramble;
	static fwMvNetworkDefId ms_NetworkTaskGeneralSweep;

	static fwMvNetworkDefId ms_NetworkTaskGunBulletReactions;
	static fwMvNetworkDefId ms_NetworkTaskReactAndFlee;
	static fwMvNetworkDefId ms_NetworkTaskShellShocked;

	static fwMvNetworkDefId ms_NetworkTaskShockingEventReact;
	static fwMvNetworkDefId ms_NetworkTaskShockingEventBackAway;

	static fwMvNetworkDefId ms_NetworkUpperBodyAimPitch;

	static fwMvNetworkDefId ms_NetworkTaskMeleeUpperbodyAnims;
	static fwMvNetworkDefId ms_NetworkTaskMeleeActionResult;

	static fwMvNetworkDefId ms_NetworkTaskSwapWeapon;
	static fwMvNetworkDefId ms_NetworkProjectileFPS;

	static fwMvNetworkDefId ms_NetworkTaskAmbientLowriderArm;

#if __BANK
	static fwMvNetworkDefId ms_NetworkTaskAttachedVehicleAnimDebug;
#endif // __BANK
};

//////////////////////////////////////////////////////////////////////////

class CMove : public fwMove
{
public:

	CMove(fwEntity& dynamicEntity) : fwMove(dynamicEntity) {}

	// PURPOSE: Called post update
	virtual void PostUpdate();

	// PURPOSE: Called prior to output buffer swap, once motion tree finished
	virtual void FinishUpdate();

	// PURPOSE: Called post output buffer swap, once motion tree finished
	virtual void PostFinishUpdate();

	// PURPOSE: Get a pointer to the correct blend helper to use to 
	//			blend in a particular anim helper
	virtual CSimpleBlendHelper* FindAppropriateBlendHelperForClipHelper(CClipHelper&) { return NULL; }


	//////////////////////////////////////////////////////////////////////////
	// Legacy helper searching functions to support script commands
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: 
	virtual CClipHelper* FindClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim) = 0;
	virtual CClipHelper* FindClipHelper(s32 dictIndex, u32 animHash) = 0;
	virtual CGenericClipHelper* FindGenericClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim) = 0;

protected:

	class CNetworkSlot
	{
	public:
		CNetworkSlot()
			: m_pPlayer(NULL)
		{

		}

		~CNetworkSlot()
		{
			SetNetworkPlayer(NULL);
		}

		void SetNetworkPlayer(fwMoveNetworkPlayer* pPlayer)
		{
			if (m_pPlayer)
			{
				m_pPlayer->SetBlendingOut();
				m_pPlayer->Release();
			}
			m_pPlayer = pPlayer;
			if (m_pPlayer)
			{
				m_pPlayer->AddRef();
			}
		}

		fwMoveNetworkPlayer* GetNetworkPlayer()
		{
			return m_pPlayer;
		}

		const fwMoveNetworkPlayer* GetNetworkPlayer() const
		{
			return m_pPlayer;
		}

	private:
		fwMoveNetworkPlayer* m_pPlayer;
	};
};


//////////////////////////////////////////////////////////////////////////


class CMoveAnimatedBuilding: public CMove
{
public:

	// PURPOSE: 
	CMoveAnimatedBuilding(CDynamicEntity& dynamicEntity);

	// PURPOSE:
	virtual ~CMoveAnimatedBuilding();

	CClipHelper* FindClipHelper(atHashWithStringNotFinal , atHashWithStringNotFinal ) { return NULL; }
	CClipHelper* FindClipHelper(s32 , u32 ) { return NULL; }
	CGenericClipHelper* FindGenericClipHelper(atHashWithStringNotFinal , atHashWithStringNotFinal ) { return NULL; }

	// PURPOSE: Sets the anim to playback on this animated building
	void SetPlaybackAnim( s16 dictionaryIndex, u32 animHash, float startTime = 0.0f );

	// PURPOSE: Set the current phase of the anim
	void SetPhase( float phase );

	// PURPOSE: Get the current phase of the anim
	float GetPhase();

	// PURPOSE: Set the playback rate of the anim
	void SetRate( float rate );

	// PURPOSE: Get the playback rate of the anim
	float GetRate();

	// PURPOSE: Set whether or not the anim should loop
	void SetLooped( bool looped );
	
#if GTA_REPLAY
	// PURPOSE: Set the current phase of the anim
	void SetReplayPhase( float phase ) { m_replayPhase = phase; SetPhase(m_replayPhase); }
	// PURPOSE: Set the current phase of the anim
	void SetReplayRate( float rate ) { m_replayRate = rate; SetRate(m_replayRate);}
#endif //GTA_REPLAY

protected:

	inline void SetDictionaryIndex(s16 dictionaryIndex)
	{
		if (m_dictionaryIndex!=-1)
		{
			fwAnimManager::RemoveRefWithoutDelete(strLocalIndex(m_dictionaryIndex));
		}

		m_dictionaryIndex = dictionaryIndex;

		if (m_dictionaryIndex!=-1)
		{
			fwAnimManager::AddRef(strLocalIndex(m_dictionaryIndex));
		}
	}
	
	// determines the maximum number of output params we can read out in a single frame (per network)
	enum 
	{
		kBufferedHashCount = 4,
	};

	s16 m_dictionaryIndex;
#if GTA_REPLAY
	float m_replayPhase;
	float m_replayRate;
#endif //GTA_REPLAY
};

////////////////////////////////////////////////////////////////////////////////

class CMoveAnimatedBuildingPooledObject : public CMoveAnimatedBuilding
{
public:

	CMoveAnimatedBuildingPooledObject(CDynamicEntity& dynamicEntity)
		: CMoveAnimatedBuilding(dynamicEntity) 
	{
	}

	FW_REGISTER_CLASS_POOL(CMoveAnimatedBuildingPooledObject);
};


#endif // MOVE_H
