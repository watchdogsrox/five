#ifndef PICKUP_DATA_H
#define PICKUP_DATA_H

// Rage headers
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwanimation/boneids.h"
#include "fwtl/pool.h"
#include "parser/macros.h"
#include "vector/vector3.h"
#include "Weapons/WeaponEnums.h"

// game headers
#include "pickups/Data/PickupIds.h"

// Forward declarations
class CBaseModelInfo;
class CPickupActionData;
class CPickupDataManager;
class CPickupRewardData;

//////////////////////////////////////////////////////////////////////////
// CPickupData
//////////////////////////////////////////////////////////////////////////

class CPickupData : public rage::datBase
{
public:
	FW_REGISTER_CLASS_POOL(CPickupData);

	// The maximum pickup types we can allocate
	static const s32 MAX_STORAGE = 165;

	CPickupData() : 
		Model((u32)0), RegenerationTime(10.0f), CollectionRadius(10.0f), 
		Scale(2.0f), LoopingAudioRef("NULL_SOUND",0xE38FCF16), GlowRed(1.0f), 
		GlowGreen(1.0f), GlowBlue(1.0f), GlowIntensity(1.0f), DarkGlowIntensity(-1.0f),MPGlowIntensity(-1.0f),MPDarkGlowIntensity(-1.0),
		AttachmentBone(BONETAG_INVALID), AttachmentOffset(VEC3_ZERO), AttachmentRotation(VEC3_ZERO),
		PickupFlags(0), ExplosionTag(EXP_TAG_DONTCARE)	{}

	u32		GetHash() const					{ return m_Name.GetHash(); }

	// Get the model hash
	u32		GetModelHash() const			{ return Model.GetHash(); }

#if !__FINAL
	// Get the name
	const char* GetName() const				{ return m_Name.GetCStr(); }
	// Get the model name
	const char* GetModelName() const		{ return Model.GetCStr(); }
#elif !__NO_OUTPUT
	// Final with output: Return empty string since names are stored as hash strings
	const char* GetName() const				{ return ""; }
	const char* GetModelName() const		{ return ""; }
#endif // !__FINAL

	// Get the model index from the model hash
	u32		GetModelIndex() const;
	u32		GetModelIndex();

	//
	// Type querying
	//
	float	GetGenerationRange() const				{ return GenerationRange; }
	float	GetRegenerationTime() const				{ return RegenerationTime; }
	float	GetCollectionRadius() const				{ return CollectionRadius; }
	float	GetCollectionRadiusFirstPerson() const	{ return CollectionRadiusFirstPerson > 0.0f ? CollectionRadiusFirstPerson : CollectionRadius; }
	float	GetScale() const						{ return Scale; }
	u32     GetLoopingSoundHash() const				{ return LoopingAudioRef.GetHash(); }

	bool    GetCollectableOnFoot() const			{ return (PickupFlags & (1<<kCollectableOnFoot)) != 0; }
	bool    GetCollectableInCar() const				{ return (PickupFlags & (1<<kCollectableInCar)) != 0; }
	bool    GetCollectableInBoat() const			{ return (PickupFlags & (1<<kCollectableInBoat)) != 0; }
	bool    GetCollectableInPlane() const			{ return (PickupFlags & (1<<kCollectableInPlane)) != 0; }
	bool    GetCollectableInVehicle() const			{ return GetCollectableInCar() || GetCollectableInBoat() || GetCollectableInPlane(); }
	bool    GetCollectableOnShot() const			{ return (PickupFlags & (1<<kCollectableOnShot)) != 0; }
	bool    GetHasGlow() const						{ return GlowIntensity > 0.0f; }
	bool    GetLowPriority() const					{ return (PickupFlags & (1<<kLowPriority)) != 0; }
	bool    GetAlwaysFixed() const					{ return (PickupFlags & (1<<kAlwaysFixed)) != 0; }
	bool    GetManualPickUp() const					{ return (PickupFlags & (1<<kManualPickUp)) != 0; }
	bool    GetInvisibleWhenCarried() const			{ return (PickupFlags & (1<<kInvisibleWhenCarried)) != 0; }
	bool    GetRequiresButtonPressToPickup() const	{ return (PickupFlags & (1<<kRequiresButtonPressToPickup)) != 0; }
	bool    GetCanBeDamaged() const					{ return (PickupFlags & (1<<kCanBeDamaged)) != 0; }
	bool    GetRotates() const						{ return (PickupFlags & (1<<kRotates)) != 0; }
	bool    GetFacePlayer() const					{ return (PickupFlags & (1<<kFacePlayer)) != 0; }
	bool    GetShareWithPassengers() const			{ return (PickupFlags & (1<<kShareWithPassengers)) != 0; }
	bool    GetIgnoreAmmoCheck() const				{ return (PickupFlags & (1<<kIgnoreAmmoCheck)) != 0; }
	bool	GetDoesntCollideWithRagdolls() const	{ return (PickupFlags & (1<<kDoesntCollideWithRagdolls)) != 0; }
	bool	GetDoesntCollideWithPeds() const		{ return (PickupFlags & (1<<kDoesntCollideWithPeds)) != 0; }
	bool	GetDoesntCollideWithVehicles() const	{ return (PickupFlags & (1<<kDoesntCollideWithVehicles)) != 0; }
	bool	GetOrientateUpright() const				{ return (PickupFlags & (1<<kOrientateUpright)) != 0; }
	bool    GetIsAirbornePickup() const				{ return (PickupFlags & (1<<kAirbornePickup)) != 0; }
	bool	GetCollectableInCarByPassengers() const { return (PickupFlags & (1<<kCollectableInCarByPassengers)) != 0; }
	bool	GetAutoCollectIfInInventory() const		{ return (PickupFlags & (1<<kAutoCollectIfInInventory)) != 0; }

	float	GetGlowRed() const						{ return GlowRed; }
	float	GetGlowGreen() const					{ return GlowGreen; }
	float	GetGlowBlue() const						{ return GlowBlue; }
	float	GetGlowIntensity() const				{ return GlowIntensity; }
	float	GetGlowRange() const					{ return GlowRange; }
	float	GetDarkGlowIntensity() const			{ return DarkGlowIntensity; }
	void	SetDarkGlowIntensity(float value)		{ DarkGlowIntensity = value; }
	float	GetMPGlowIntensity() const				{ return MPGlowIntensity; }
	void	SetMPGlowIntensity(float value)			{ MPGlowIntensity = value; }
	float	GetMPDarkGlowIntensity() const			{ return MPDarkGlowIntensity; }
	void	SetMPDarkGlowIntensity(float value)		{ MPDarkGlowIntensity = value; }

	u32     GetNumOnFootActions() const				{ return OnFootPickupActions.GetCount(); }
	u32     GetNumInCarActions() const				{ return InCarPickupActions.GetCount(); }
	u32     GetNumOnShotActions() const				{ return OnShotPickupActions.GetCount(); }
	u32     GetNumRewards() const					{ return Rewards.GetCount(); }

	u32		GetFirstWeaponReward() const;

	eAnimBoneTag GetAttachmentBone() const			{ return AttachmentBone; }
	const Vector3 GetAttachmentOffset() const		{ return AttachmentOffset; }
	const Vector3 GetAttachmentRotation() const		{ return AttachmentRotation; }

	const CPickupActionData* GetOnFootAction(u32 index) const;
	const CPickupActionData* GetInCarAction(u32 index) const;
	const CPickupActionData* GetOnShotAction(u32 index) const;
	const CPickupRewardData* GetReward(u32 index) const;

	eExplosionTag GetExplosionTag() const {return ExplosionTag;}

protected:
	rage::atHashWithStringNotFinal m_Name;
	rage::u32 PickupFlags;
	rage::f32 GlowRange;

	// Pickup data
	rage::Vector3 AttachmentOffset;
	rage::Vector3 AttachmentRotation;
	rage::atHashWithStringNotFinal Model;
	rage::f32 GenerationRange;
	rage::f32 RegenerationTime;
	rage::f32 CollectionRadius;
	rage::f32 CollectionRadiusFirstPerson;
	rage::f32 Scale;
	rage::atHashString LoopingAudioRef;
	rage::f32 GlowRed;
	rage::f32 GlowGreen;
	rage::f32 GlowBlue;
	rage::f32 GlowIntensity;
	rage::f32 DarkGlowIntensity;
	rage::f32 MPGlowIntensity;
	rage::f32 MPDarkGlowIntensity;

	static const rage::u8 MAX_ONFOOTPICKUPACTIONSS = 10;
	atFixedArray<rage::atHashString, MAX_ONFOOTPICKUPACTIONSS> OnFootPickupActions;

	static const rage::u8 MAX_INCARPICKUPACTIONSS = 10;
	atFixedArray<rage::atHashString, MAX_INCARPICKUPACTIONSS> InCarPickupActions;

	static const rage::u8 MAX_ONSHOTPICKUPACTIONSS = 10;
	atFixedArray<rage::atHashString, MAX_ONSHOTPICKUPACTIONSS> OnShotPickupActions;

	static const rage::u8 MAX_REWARDSS = 10;
	atFixedArray<rage::atHashString, MAX_REWARDSS> Rewards;

	enum PickupFlagsEnum{
		kCollectableOnFoot,
		kCollectableInCar,
		kCollectableInBoat,
		kCollectableInPlane,
		kCollectableOnShot,
		kLowPriority,
		kAlwaysFixed,
		kManualPickUp,
		kInvisibleWhenCarried,
		kRequiresButtonPressToPickup,
		kCanBeDamaged,
		kRotates,
		kFacePlayer,
		kShareWithPassengers,
		kIgnoreAmmoCheck,
		kDoesntCollideWithRagdolls,
		kDoesntCollideWithPeds,
		kDoesntCollideWithVehicles,
		kOrientateUpright,
		kAirbornePickup,
		kCollectableInCarByPassengers,
		kAutoCollectIfInInventory
	};

	eAnimBoneTag AttachmentBone;	
	eExplosionTag ExplosionTag;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CPickupData(const CPickupData& other);
	CPickupData& operator=(const CPickupData& other);
};


#endif // PICKUP_DATA_H
