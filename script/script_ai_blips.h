#ifndef __SCRIPT_AI_BLIPS_H__
#define __SCRIPT_AI_BLIPS_H__

#include "Frontend/MiniMapCommon.h"
#include "Peds/ped.h"

#define AI_BLIP_NOTICABLE_RADIUS	(100.0f)

enum eBLIP_GANG
{
	BLIP_GANG_NONE = 0,
	BLIP_GANG_PROFESSIONAL,
	BLIP_GANG_FRIENDLY,
	BLIP_GANG_ENEMY
};

class CScriptPedAIBlips
{
public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	// Sets a Ped to use AI blips (or not)
	static void	SetPedHasAIBlip(s32 threadId, s32 PedID, bool bSet, s32 colourOverride = BLIP_COLOUR_MAX);

	// Returns if a ped has an AI blip
	static bool DoesPedhaveAIBlip(s32 PedID);

	// Updates and draws all the peds that are registered for AI blips
	static void	UpdatePedAIBlips();

	// Set AI Blip Gang ID
	static void	SetPedAIBlipGangID(s32 PedID, s32 GangID);
	
	// Tell the Ped blip it has cone (or not)
	static void	SetPedHasConeAIBlip(s32 PedID, bool bSet);

	// Tell the Ped blip it is forced (or not)
	static void	SetPedHasForcedBlip(s32 PedID, bool bOnOff);

	// Tell the Ped blip what it's noticeable range is.
	static void	SetPedAIBlipNoticeRange(s32 PedID, float range);

	// Tell the Ped blip to change colour
	static void SetPedAIBlipChangeColour(s32 PedID);

	// Set the sprite
	static void SetPedAIBlipSprite(s32 PedID, int spriteID);

	// Get the blip Index of the current PED blip attached to this ped
	static s32	GetAIBlipPedBlipIDX(s32 PedID);

	// Get the blip Index of the current VEHICLE blip attached to this ped
	static s32	GetAIBlipVehicleBlipIDX(s32 PedID);

public:

	// Tune vars
	static float WEAPON_RANGE_MULTIPLIER_FOR_BLIPS;
	static float HEARD_RADIUS;
	static float BLIP_SIZE_VEHICLE;
	static float BLIP_SIZE_NETWORK_VEHICLE;
	static float BLIP_SIZE_PED;
	static float BLIP_SIZE_NETWORK_PED;
	static float BLIP_SIZE_OBJECT;
	static float BLIP_SIZE_NETWORK_OBJECT;
	static int	 NOTICABLE_TIME;
	static int	 BLIP_FADE_PERIOD;

private:

	class AIBlip
	{
	public:
		void	Init(s32 PedID, s32 threadId, s32 colourOverride = BLIP_COLOUR_MAX);

		// TODO: Consolidate any of these that don't rely on contained data into CScriptPedAIBlips as static members, then make the calls to commands_hud call down
		bool	Update();
		bool	IsPedNoticableToPlayer(CPed *pPed, CPed *pPlayerPed, float fNoticableRadius, bool bRespondingToSpecialAbility, int iResponseTimer, int iResponseTime);
		bool	IsPlayerFreeAimingAtEntity( CPed *pPlayer, CEntity *pTarget );
		float	GetMaxRangeOfCurrentPedWeaponBlip(CPed *pPed);

		bool	DoesBlipExist(int iBlipIndex);
		int		GetBlipFromEntity(CEntity *pEntity);
		int		CreateBlipForVehicle(CVehicle *pVehicle,  bool isEnemyVehicle = false, bool useGreenVehicleBlip = false);
		int		CreateBlipOnEntity(CEntity *pEntity, bool bIsFriendly = true, bool useGreenVehicleBlip = false);
		int		AddBlipForEntity(CEntity *pEntity);
		void	RemoveBlip(s32 &blipID, bool bCheckExclusivity = false);

		s32		GetCorrectBlipPriority(eBLIP_PRIORITY_TYPE type);
		eHUD_COLOURS GetBlipHudColour(s32 iBlipIndex);
		void	SetBlipNameFromTextFile(s32 iBlipIndex, const char *pTextLabel);
		void	CleanupBlip();
		void	HandleBlipFade( bool isVehicle );

		bool	IsPlayerTargettingEntity( CPed *pPlayer, CEntity *pTarget );

		int		GetGameTimer()			{ return (int)fwTimer::GetTimeInMilliseconds(); }
		int		GetNetworkGameTimer()	{ return static_cast<int>(CNetwork::GetNetworkTimeLocked()); }

		// The pedID that is using the blip
		s32		pedID;
		// The threadID (for script resources)
		s32		scrThreadID;
		// Update params that can be set by script
		int		iBlipGang;			// = -1					// Friendly/Enemy (Neil)... no, 3 states & -1!
		bool	bForceBlipOn;		// = FALSE				// Neil said not needed, but some script calls do use it
		bool	bShowCone;			// = FALSE				// Needed
		float	fNoticableRadius;	// = -1.0				// Needed
		// I suspect this can be set by script?
		bool	bChangeColour;
		eBLIP_COLOURS blipColourOverride;
		// Everything related to the blip
		s32		BlipID;				// BLIP_INDEX
		s32		VehicleBlipID;		// BLIP_INDEX
		s32		iNoticableTimer;	// TIME_DATATYPE
		s32		iFadeOutTimer;		// TIME_DATATYPE
		int		iNoticableTimer_SP;	
		int		iFadeOutTimer_SP;
		bool	bIsFadingOut;
		//	Special Ability "Things"
		int		iResponseTime;
		int		iResponseTimer;
		bool	bRespondingToSpecialAbility;
	};

	static void Reset();

	static s32	FindPedIDIndex(s32 pedID);

	static void UpdatePedAIBlip(AIBlip &blip);

	
	friend class AIBlip; // so that they can use this function below
	// needed to prevent Vehicles with multiple riders from de-blipping themselves
	static bool	IsExclusiveOwnerOfBlip(const AIBlip* referencingObject, s32 iBlipId);

	// The peds that have AI blips (Make fixed array if there's to be a limit).
	static	atArray<AIBlip> m_PedsAndBlips;
};


#endif	//__SCRIPT_AI_BLIPS_H__