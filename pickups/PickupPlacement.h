#ifndef PICKUP_PLACEMENT_H
#define PICKUP_PLACEMENT_H

// Game headers
#include "network/Objects/Entities/NetObjPickupPlacement.h"
#include "pickups/Data/PickupIds.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptIds.h"

// framework includes
#include "fwscript/scripthandler.h"

class CPickup;
class CNetObjPickup;
class CPickupData;

//////////////////////////////////////////////////////////////////////////
// CPickupPlacement - Placement data for a pickup location. Exists whether 
//					  the actual pickup object is there or not. Spawns a 
//					  pickup object when the player gets close enough.
//////////////////////////////////////////////////////////////////////////

typedef u32 PlacementFlags;

class CPickupPlacement : public netGameObjectBase, public CGameScriptHandlerObject
{
public:

	// placement flags
	enum 
	{
		// global (broadcast over network):
		PLACEMENT_CREATION_FLAG_MAP							= (1<<0),		// map placements are treated as a special case due to their large number. These are the placements scattered around the map in a deathmatch, etc.
		PLACEMENT_CREATION_FLAG_FIXED						= (1<<1),		// if set the spawned pickup will not activate its physics
		PLACEMENT_CREATION_FLAG_REGENERATES					= (1<<2),		// if set the placement will regenerate the pickup after it has been collected
		PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND				= (1<<3),		// if set the placement will snap to the ground (within a range of 2.0 meters up/down)
		PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND			= (1<<4),		// if set the placement will orient to the ground normal (if ground is found)
		PLACEMENT_CREATION_FLAG_LOCAL_ONLY					= (1<<5),		// if set the placement will not be cloned on other machines
		PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE				= (1<<6),		// if set the placement has a simple blip
		PLACEMENT_CREATION_FLAG_BLIPPED_COMPLEX				= (1<<7),		// if set the placement has a complex blip
		PLACEMENT_CREATION_FLAG_UPRIGHT						= (1<<8),		// if set the spawned pickup upright
		PLACEMENT_CREATION_FLAG_ROTATE						= (1<<9),		// if set the spawned pickup will rotate
		PLACEMENT_CREATION_FLAG_FACEPLAYER					= (1<<10),		// if set the spawned pickup will always face the player
		PLACEMENT_CREATION_FLAG_HIDE_IN_PHOTOS				= (1<<11),		// if set the spawned pickup will not be rendered when the player is using the phone camera
		PLACEMENT_CREATION_FLAG_PLAYER_GIFT					= (1<<12),		// if set the pickup is being dropped as a gift to another player
		PLACEMENT_CREATION_FLAG_ON_OBJECT					= (1<<13),		// if set the pickup may be lying on an object - if so include objects in the probes when placing on ground
		PLACEMENT_CREATION_FLAG_GLOW_IN_TEAM				= (1<<14),
		PLACEMENT_CREATION_FLAG_AUTO_EQUIP					= (1<<15),	
		PLACEMENT_CREATION_FLAG_COLLECTABLE_IN_VEHICLE		= (1<<16),		// if set the pickup can be collected by a ped in a vehicle
		PLACEMENT_CREATION_FLAG_DISABLE_WEAPON_HD_MODEL		= (1<<17),		// if set the weapon pickup will render SD model only (HD<->SD model switch will be disabled)
		PLACEMENT_CREATION_FLAG_FORCE_DEFERRED_MODEL		= (1<<18),		// if set the pickup will render as deferred model (no transparency/alpha blending in this render mode)
#if __BANK
		PLACEMENT_CREATION_FLAG_DEBUG_CREATED				= (1<<19),		// if set the spawned pickup was created by a rag widget
		PLACEMENT_FLAG_NUM_CREATION_FLAGS					= 20,
#else
		PLACEMENT_FLAG_NUM_CREATION_FLAGS					= 19,
#endif // __BANK

		// If any more are added, you'll need to alter CPickupPlacementCreationDataNode::SIZEOF_PLACEMENT_FLAGS

		// REMEMBER ADDTIONAL INTERNAL FLAGS BELOW!!
	};

	static const float DEFAULT_GLOW_OFFSET;

	// a class holding custom script data that is only used for some pickups. The vast majority are map pickups, and we don't want to allocate this memory
	// for them
	class CPickupPlacementCustomScriptData
	{
	public:

		CPickupPlacementCustomScriptData() :
			m_customModelIndex(fwModelId::MI_INVALID),
			m_pickupGlowOffset(DEFAULT_GLOW_OFFSET),
			m_teamPermits(0),
			m_radarBlip(INVALID_BLIP_ID)/*,
			m_CloneToPlayersList(0)*/
		{
		}
		
		FW_REGISTER_CLASS_POOL(CPickupPlacementCustomScriptData);

		strLocalIndex		m_customModelIndex;		// a custom model the pickup can use instead of the one specified in the pickups meta data
		s32					m_radarBlip;			// the radar blip id (if this placement is blipped)
		float				m_pickupGlowOffset;		// Offset applied to the glow, if any
		TeamFlags			m_teamPermits;			// flags indicating which teams are allowed to collect this pickup
		//PlayerFlags			m_CloneToPlayersList;	// used when we only want the pickup networked with a subset of players
	};

protected: 

	// internal flags
	enum
	{
		PLACEMENT_INTERNAL_FLAG_COLLECTED				= (1<<PLACEMENT_FLAG_NUM_CREATION_FLAGS),		// if set when the placement's pickup has been collected
		PLACEMENT_INTERNAL_FLAG_DESTROYED				= (1<<(PLACEMENT_FLAG_NUM_CREATION_FLAGS+1)),	// if set the placement is being destroyed
		PLACEMENT_INTERNAL_FLAG_PICKUP_DESTROYED		= (1<<(PLACEMENT_FLAG_NUM_CREATION_FLAGS+2)),	// if set the pickup was destroyed externally, via an explosion, etc
		PLACEMENT_INTERNAL_FLAG_UNCOLLECTABLE			= (1<<(PLACEMENT_FLAG_NUM_CREATION_FLAGS+3)),	// if set the pickup will be locally uncollectable
		PLACEMENT_INTERNAL_FLAG_FADE_WHEN_UNCOLLECTABLE	= (1<<(PLACEMENT_FLAG_NUM_CREATION_FLAGS+4)),	// if set the pickup will be faded out if it can't be collected (if blocked by script)
		PLACEMENT_INTERNAL_FLAG_HIDE_WHEN_UNCOLLECTABLE	= (1<<(PLACEMENT_FLAG_NUM_CREATION_FLAGS+5)),	// if set the pickup object will not spawn if it can't be collected (if blocked by script)
		PLACEMENT_FLAGS_LAST_FLAG						= PLACEMENT_INTERNAL_FLAG_HIDE_WHEN_UNCOLLECTABLE

		// If > 32, increase size of PlacementFlags 
	};

public:

	FW_REGISTER_CLASS_POOL(CPickupPlacement);

	CPickupPlacement(u32 pickupHash, Vector3& position, Vector3& orientation, PlacementFlags flags, u32 customModelIndex = fwModelId::MI_INVALID);
	virtual ~CPickupPlacement();

	u32					  GetPickupHash() const;
	const CPickupData*	  GetPickupData() const								{ return m_pPickupData; }
	CPickup*			  GetPickup() const									{ return m_pPickup; }
	void				  SetPickup(CPickup* pPickup);
	Vector3&			  GetPickupPosition() 								{ return m_pickupPosition; }
	Vector3&			  GetPickupOrientation()							{ return m_pickupOrientation; }
	float				  GetGenerationRange() const;
	u32				      GetRoomHash()	const								{ return m_roomHashkey; }
	void				  SetRoomHash(u32 hash)							    { m_roomHashkey = hash; }
	u32				      GetWeaponHash()	const							{ return m_customWeaponHash; }
	void				  SetWeaponHash(u32 hash)							{ m_customWeaponHash = hash; }
	void				  SetAmount(u16 amount);
	u32					  GetAmount() const									{ return m_amount; }
	u32					  GetCustomModelHash() const;
	s32				      GetRadarBlip() const								{ return m_pCustomScriptData ? m_pCustomScriptData->m_radarBlip : INVALID_BLIP_ID; }
	void				  SetRegenerationTime(u32 regenTime);
	u32					  GetRegenerationTime();
	u32					  GetCustomRegenerationTime() const					{ return m_customRegenTime; }
	TeamFlags			  GetTeamPermits() const							{ return m_pCustomScriptData ? m_pCustomScriptData->m_teamPermits : 0; }
	void				  SetTeamPermits(TeamFlags teams);							
	void				  AddTeamPermit(u8 team);
	bool				  GeneratesNetworkedPickups() const;
	void				  SetPickupGlowOffset(float f);
	float				  GetPickupGlowOffset() const						{ return m_pCustomScriptData ? m_pCustomScriptData->m_pickupGlowOffset : DEFAULT_GLOW_OFFSET; }
	void				  SetAmountCollected(u16 amount)					{ m_amountCollected = amount; }
	u32					  GetAmountCollected() const						{ return m_amountCollected; }
	//void				  SetPlayersAvailability(PlayerFlags players);
	//PlayerFlags			  GetPlayersAvailability() const					{ return m_pCustomScriptData ? m_pCustomScriptData->m_CloneToPlayersList : 0; }

	// flags -  TODO: replace all of these methods with IsFlagSet, etc! And use fwFlags
	u32				      GetFlags() const									{ return static_cast<u32>(m_flags); }
	bool				  GetIsMapPlacement() const							{ return (m_flags & PLACEMENT_CREATION_FLAG_MAP) != 0; }
	bool				  GetIsFixed() const								{ return (m_flags & PLACEMENT_CREATION_FLAG_FIXED) != 0; }
	bool				  GetRegenerates() const							{ return (m_flags & PLACEMENT_CREATION_FLAG_REGENERATES) != 0; }
	bool				  GetIsCollected() const							{ return (m_flags & PLACEMENT_INTERNAL_FLAG_COLLECTED) != 0; }
	bool				  GetHasPickupBeenDestroyed() const					{ return (m_flags & PLACEMENT_INTERNAL_FLAG_PICKUP_DESTROYED) != 0; }
	bool				  GetLocalOnly() const								{ return (m_flags & PLACEMENT_CREATION_FLAG_LOCAL_ONLY) != 0; }
	void				  SetPickupHasBeenDestroyed(bool bLocallyDestroyed);
	void				  SetHasSimpleBlip() 								{ m_flags |= PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE; m_flags &= ~PLACEMENT_CREATION_FLAG_BLIPPED_COMPLEX; }
	bool				  GetHasSimpleBlip() const							{ return (m_flags & PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE) != 0; }
	void				  SetHasComplexBlip()								{ m_flags |= PLACEMENT_CREATION_FLAG_BLIPPED_COMPLEX; m_flags &= ~PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE; }
	bool				  GetHasComplexBlip() const							{ return (m_flags & PLACEMENT_CREATION_FLAG_BLIPPED_COMPLEX) != 0; }
	void				  SetIsBeingDestroyed()								{ m_flags |= PLACEMENT_INTERNAL_FLAG_DESTROYED; }
	bool				  GetIsBeingDestroyed() const						{ return (m_flags & PLACEMENT_INTERNAL_FLAG_DESTROYED) != 0; }
	void				  SetShouldSnapToGround()							{ m_flags |= PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND; }
	bool				  GetShouldSnapToGround() const						{ return (m_flags & PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND) != 0; }
	void				  SetShouldOrientToGround()							{ m_flags |= PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND; }
	bool				  GetShouldOrientToGround() const					{ return (m_flags & PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND) != 0; }
	void				  SetShouldBeUpright()								{ m_flags |= PLACEMENT_CREATION_FLAG_UPRIGHT; }
	bool				  GetShouldBeUpright() const						{ return (m_flags & PLACEMENT_CREATION_FLAG_UPRIGHT) != 0; }
	void				  SetShouldRotate()									{ m_flags |= PLACEMENT_CREATION_FLAG_ROTATE; }
	bool				  GetShouldRotate() const							{ return (m_flags & PLACEMENT_CREATION_FLAG_ROTATE) != 0; }
	void				  SetShouldFacePlayer()								{ m_flags |= PLACEMENT_CREATION_FLAG_FACEPLAYER; }
	bool				  GetShouldFacePlayer() const						{ return (m_flags & PLACEMENT_CREATION_FLAG_FACEPLAYER) != 0; }
	void				  SetGlowWhenInSameTeam()								{ m_flags |= PLACEMENT_CREATION_FLAG_GLOW_IN_TEAM; }
	bool				  GetGlowWhenInSameTeam() const						{ return (m_flags & PLACEMENT_CREATION_FLAG_GLOW_IN_TEAM) != 0; }
	void				  SetUncollectable(bool b);
	bool				  GetUncollectable() const							{ return (m_flags & PLACEMENT_INTERNAL_FLAG_UNCOLLECTABLE) != 0; }
	void				  SetTransparentWhenUncollectable(bool b);
	bool				  GetTransparentWhenUncollectable() const			{ return (m_flags & PLACEMENT_INTERNAL_FLAG_FADE_WHEN_UNCOLLECTABLE) != 0; }
	void				  SetHiddenWhenUncollectable(bool b);
	bool				  GetHiddenWhenUncollectable() const				{ return (m_flags & PLACEMENT_INTERNAL_FLAG_HIDE_WHEN_UNCOLLECTABLE) != 0; }
	bool				  GetIsHDWeaponModelDisabled() const				{ return (m_flags & PLACEMENT_CREATION_FLAG_DISABLE_WEAPON_HD_MODEL) != 0; }
	bool				  GetForceDeferredModel() const						{ return (m_flags & PLACEMENT_CREATION_FLAG_FORCE_DEFERRED_MODEL) != 0; }
#if __BANK
	void				  SetDebugCreated()									{ m_flags |= PLACEMENT_CREATION_FLAG_DEBUG_CREATED; }
	bool				  GetDebugCreated() const							{  return (m_flags & PLACEMENT_CREATION_FLAG_DEBUG_CREATED) != 0; }
#else
	void				  SetDebugCreated()									{ }
	bool				  GetDebugCreated() const							{  return false; }
# endif // __BANK

	void				  Update();

	// returns true if in scope with any players or the given player. When in scope the placement spawns a pickup object.
	bool				  GetInScope(CPed* pPlayerPed = NULL) const;

	// creates the pickup object to represent this placement
	CPickup* 			  CreatePickup(CNetObjPickup* pNetObj = NULL);

	// called when the pickup object representing this placement is collected 
	void				  Collect(CPed* pPedWhoGotIt);

	// called when the pickup object representing this placement has to be regenerated 
	bool				  Regenerate();

	// Destroys the pickup object representing this placement. Called when placement goes out of scope
	void				  DestroyPickup();

	// creates a blip for the placement if necessary (used by script commands)
	s32				      CreateBlip();

	// removes any blip that exists for the placement (used by script commands)
	void				  RemoveBlip();

	// used by network code 
	bool				  IsScriptObject() const	{ return false; }

	// called when the placement is registered with the network object manager as a network game starts
	void				  OnNetworkRegistration() {}

	// returns true if this placement can be collected (not blocked by script)
	bool				  CanCollectScript() const;

	// static functions
	static bool			  GetInScope(const CPed& playerPed, const Vector3& placementPos, float range);

	static void			  SetGenerationRangeMultiplier(float mult) { ms_generationRangeMultiplier = mult; }
	static float		  GetGenerationRangeMultiplier() { return ms_generationRangeMultiplier; }

	// scriptHandlerObject methods:
	virtual unsigned					GetType() const { return SCRIPT_HANDLER_OBJECT_TYPE_PICKUP; }
	virtual void						CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken);
	virtual void						SetScriptInfo(const scriptObjInfoBase& info);
	virtual scriptObjInfoBase*			GetScriptInfo();
	virtual const scriptObjInfoBase*	GetScriptInfo() const;
	virtual void						SetScriptHandler(scriptHandler* handler);
	virtual scriptHandler*				GetScriptHandler() const { return m_pHandler; }
	virtual void						ClearScriptHandler() { m_pHandler = NULL; }
	virtual void						OnRegistration(bool newObject, bool hostObject);
	virtual void						OnUnregistration();
	virtual bool						IsCleanupRequired() const;
	virtual void						Cleanup();
	virtual netObject*					GetNetObject() const { return GetNetworkObject(); }
	virtual fwEntity*					GetEntity() const { return NULL; }
	virtual bool						HostObjectCanBeCreatedByClient() const { return GetIsMapPlacement(); }

#if __BANK
	virtual void						SpewDebugInfo(const char* scriptName) const;
#endif

	virtual bool GetNoLongerNeeded() const { return false; }
	virtual const char* GetLogName() const { return "PICKUP_PLACEMENT"; }

protected:

	CPickupPlacementCustomScriptData* GetOrCreateCustomScriptData();

public:

	// blip settings that can be altered by script commands
	static float	ms_customBlipScale;
	static int		ms_customBlipDisplay;
	static int		ms_customBlipPriority;
	static int		ms_customBlipSprite;
	static int		ms_customBlipColour;

	static float	ms_generationRangeMultiplier;

protected:

	// Not allowed to copy construct or assign
	CPickupPlacement(const CPickupPlacement &);
	CPickupPlacement &operator=(const CPickupPlacement &);
	
	CGameScriptObjInfo					m_ScriptInfo;			// the script that created this placement
	u32									m_roomHashkey;			// the key of the room this placement is in
	u32									m_customWeaponHash;			// weapon hash
	Vector3								m_pickupPosition;		// position of the pickup object when generated
	Vector3								m_pickupOrientation;	// euler angle orientation of the pickup object when generated 	
	CPickup*							m_pPickup;				// ptr to the pickup object which this placement has generated
	const CPickupData*					m_pPickupData;			// a pointer to the pickup data
	scriptHandler*						m_pHandler;				// the script handler that this placement is registered with
	CPickupPlacementCustomScriptData*	m_pCustomScriptData;	// some placements can set additional custom data
	PlacementFlags						m_flags;				// placement flags
	u16									m_customRegenTime;		// a custom regeneration time in seconds, specific only to this placement. Overrides the value set in the pickup data.
	u16									m_amount;				// a variable amount used by some pickup types (eg money). This is used to get around the hard coded values specified in the Rave data. 
	u16									m_amountCollected;		// the amount that is actually picked up when we consume this pickup placement.

public:

	// declare as a network object
	NETWORK_OBJECT_PTR_DECL_EX(CNetObjGame);
	NETWORK_OBJECT_TYPE_DECL( CNetObjPickupPlacement, NET_OBJ_TYPE_PICKUP_PLACEMENT );
};

#endif // PICKUP_PLACEMENT_H
