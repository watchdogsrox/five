#ifndef __CWEAPON_WHEEL__
#define __CWEAPON_WHEEL__

#include "CSelectionWheel.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "weapons/info/WeaponInfo.h"

// forward declarations
class CPed;
class CWeapon;

#if __BANK
namespace rage
{
	class bkBank;
};
#endif

#define USE_DIRECT_SELECTION (RSG_ORBIS || KEYBOARD_MOUSE_SUPPORT)

// MUST BE KEPT IN SYNC WITH SCRIPT
struct SWeaponWheelScriptNode
{
	scrValue uWeaponHash;
	scrValue iAmmo;
	scrValue iWeaponMods;
	scrValue iTint;
	scrValue iCamo;

	SWeaponWheelScriptNode() {Clear();}
	void Clear() 
	{
		uWeaponHash.Int = 0;
		iAmmo.Int = 0;
		iWeaponMods.Int = 0;
		iTint.Int = 0;
		iCamo.Int = 0;
	}

};


class CWeaponWheel : public CSelectionWheel
{
protected:
	static const u8 MAX_WHEEL_WEAPONS_IN_SLOT = 20;

	struct sWeaponWheelSlot
	{
		sWeaponWheelSlot() { Clear(); }

		void Clear() {
			m_uWeaponHash.SetHash(0);
			m_HumanNameHashSuffix.SetHash(0);
			m_iAmmo = 0;
			m_iClipAmmo = 0;
			m_iClipSize = 0;
			m_iAmmoSpecialType = 0;
			m_bIsSelectable = true;
			m_bIsEquipped = false;
		}

		void SetFromWeapon( const CWeapon* pCurrentWeapon, bool bIsInfiniteAmmo );
		void SetFromHash( atHashWithStringNotFinal uWeaponHash, s32 iAmmoCount, const CPed* pPed = NULL, s32 iClipAmmoOverride = -1 );


		const bool IsEmpty() const { return m_uWeaponHash.IsNull();}
		const bool IsSelectable() const { return m_bIsSelectable; }

		void SetScaleform(bool bSelected, int iWeaponSlot, int iIndex) const;

		atHashWithStringNotFinal m_uWeaponHash;
		atHashWithStringNotFinal m_HumanNameHash;
		atHashWithStringNotFinal m_HumanNameHashSuffix;
		s32 m_iAmmo;
		s32 m_iClipAmmo;
		s32 m_iClipSize;
		s32 m_iAmmoSpecialType;
		bool m_bIsSelectable;
		bool m_bIsEquipped;

	protected:
		void Validate();
	};

	struct sWeaponWheelMemory : public datBase
	{
#if !__BANK
	// de-protecting them SOLELY for the laziness of bank widgets
	protected:
#endif
		atRangeArray<atHashWithStringNotFinal, MAX_WHEEL_SLOTS> m_WeaponHashSlots;
		atHashWithStringNotFinal m_uSelectedHash;

	public:
		sWeaponWheelMemory() : m_uSelectedHash(0u)
		{
			Clear();
		}

		void Clear()
		{
			for( u8 i=0; i < MAX_WHEEL_SLOTS; ++i)
				SetWeapon(i, 0u);
		}

		void SetWeapon(u8 iSlotIndex, atHashWithStringNotFinal uWeaponHash)
		{
			m_WeaponHashSlots[iSlotIndex] = uWeaponHash;
		}

		atHashWithStringNotFinal GetWeapon(u8 iSlotIndex) const
		{ 
			return m_WeaponHashSlots[iSlotIndex];
		}

		void SetSelected(atHashWithStringNotFinal uNewHash)
		{
			m_uSelectedHash = uNewHash;
		}
		atHashWithStringNotFinal GetSelected() const
		{
			return m_uSelectedHash;
		}
	};

	//	Do we need one of these for each character?
	sWeaponWheelMemory m_WheelMemory;

	atMultiRangeArray<sWeaponWheelSlot, MAX_WHEEL_SLOTS, MAX_WHEEL_WEAPONS_IN_SLOT> m_Slot; 	
	atRangeArray<u8, MAX_WHEEL_SLOTS> m_iIndexOfWeaponToShowInitiallyForThisSlot;

	atRangeArray<strRequest, MAX_WHEEL_SLOTS*2> m_aRequests;

	atHashWithStringNotFinal m_uLastEquippedWeaponWheelHash;
	atHashWithStringNotFinal m_uLastHighlightedWeaponWheelHash;
	s32 m_iHashReturnedId;
	s8 m_iFramesToWaitForAValidReturnedId;

	bool m_bIsActive;
	bool m_bScriptForcedOn;
	bool m_bSuppressWheelResults;
	bool m_bAllowWeaponDPadSelection;
	bool m_bAllowedToChangeWeapons;
	bool m_bSuppressWheelUntilButtonReleased;
	bool m_bSuppressWheelResultDueToQuickTap;
	bool m_bRebuild;
	bool m_bForcedOn;
	bool m_bForceResult;
	//Used to stop reloading weapon when tapping
	bool m_bDisableReload;

	bool m_bQueueingForceOff;
	bool m_bQueueParachute;
	bool m_bReceivedFadeComplete;
	bool m_bHasASlotWithMultipleWeapons;

	EdgeBool m_bVisible;
	EdgeBool m_bCarMode;
	EdgeInt m_iWeaponCount;
	EdgeInt	m_iAmmoCount;

	eWeaponWheelSlot	m_eLastSlotPointedAt;
#if USE_DIRECT_SELECTION
	eWeaponWheelSlot	m_eDirectSelectionSlot;
#endif // USE_DIRECT_SELECTION

	atHashWithStringNotFinal m_uWeaponInHandHash;
	s32 m_HolsterAmmoInClip;

	CSimpleTimer m_iForceWheelShownTime;
	BankInt32 PC_FORCE_WHEEL_TIME;
	BankFloat CROSSFADE_TIME;
	BankInt32 CROSSFADE_ALPHA;

	float		m_fLastTimeWarp;
	int			m_iFailsafeTimer;
	int			m_iNumParachutes;

	bool		m_bSwitchWeaponInSniperScope;
	bool		m_bToggleSpecialVehicleAbility;
	bool		m_bSetSpecialVehicleCameraOverride;

public:
	CWeaponWheel();
	void Update(bool forceShow, bool isHudHidden);

	bool CheckIncomingFunctions(atHashWithStringBank uMethodName, const GFxValue args[] );

	void ClearAndHide(); // ClearBase with a bit more stuff
	void Clear(bool bFullyClear = false);

	void SetSpecialVehicleAbilityFromScript();
	void SetForcedByScript( bool bOnOrOff );
	void SetWeaponInMemory( atHashWithStringNotFinal uWeaponHash );
	atHashWithStringNotFinal GetWeaponInMemory(u8 uSlotIndex) const;

	void ClearPreviouslyActive() { m_bVisible.SetState(false); }
	bool IsVisible() const { return m_bVisible.GetState(); }
	bool IsActive() const { return m_bIsActive; }
	bool HasASlotWithMultipleWeapons() const { return m_bHasASlotWithMultipleWeapons; }
	bool HaventChangedWeaponYet() const { return (m_iHashReturnedId != 0); }

	void ForceSpecialVehicleStatusUpdate();

	atHashWithStringNotFinal GetCurrentlyHighlightedWeapon() const { return m_uLastHighlightedWeaponWheelHash; }

	u32 GetWeaponInHand() const { return m_uWeaponInHandHash; }
	void SetWeaponInHand(atHashWithStringNotFinal uWeaponInHand);

	bool GetDisabledReload() { return m_bDisableReload; }
	void SetDisabledReload(bool bDisabled) { m_bDisableReload = bDisabled; }

	s32  GetHolsterAmmoInClip();
	void SetHolsterAmmoInClip(s32 sAmmo);

	void SetContentsFromScript(SWeaponWheelScriptNode paScriptNodes[], int nodeCount);
	void SetSuppressResults(bool bSuppressed ) { m_bSuppressWheelResults = bSuppressed; }

	static s32 GetClipSizeFromComponent(const CPed* pPed, atHashWithStringNotFinal uWeaponHash );

#if __BANK
	void CreateBankWidgets(bkBank* pOwningBank);
#endif

#if __DEV
	bool SetDisabledReason(const char* pszReasonWhy);
	const char* GetDisabledReason() const { return m_pszDisabledReason; }
	const char* m_pszDisabledReason;
#endif

	bool IsBeingForcedOpen() const { return m_bVisible.GetState() && !m_bForcedOn; }
	void FadeComplete();

	void SetSelected(atHashWithStringNotFinal uNewHash) {m_WheelMemory.SetSelected(uNewHash);}

	atHashWithStringNotFinal GetWeaponHashFromMemory(u8 uSlotIndex) { return m_WheelMemory.GetWeapon(uSlotIndex); }

protected:
	eWeaponWheelSlot SetContentsFromPed( const CPed* pPlayerPed );

	eWeaponWheelSlot MapWeaponSlotToWeaponWheel(s32 iWeaponCodeSlot, atHashWithStringNotFinal uCurrentWeaponHash, const sWeaponWheelSlot& newSlot);
	void FillScaleform( eWeaponWheelSlot iInitialSlot, bool bCarMode );
	void START_CROSSFADE(float time, int alpha);
	void Hide(bool bCatchReturnValue);

	void SetWeapon(u8 iSlotIndex, atHashWithStringNotFinal uWeaponHash);
	void UpdateWeaponAttachment(atHashWithStringNotFinal uWeaponHash);
	void RequestHdWeapons();
	void ReleaseHdWeapons();
	void RequestHdWeaponAssets(u8 iSlotIndex, atHashWithStringNotFinal uWeaponHash);
	void FindAndUpdateWeaponWheelStats(bool bCarMode, atHashWithStringNotFinal uWeaponHash, CPed* pPlayerPed);
	void UpdateWeaponWheelStatsWithAttachments(bool bCarMode, atHashWithStringNotFinal uWeaponHash, s8 iAttachmentsDamage, s8 iAttachmentsHudSpeed, s8 iAttachmentsHudCapacity, s8 iAttachmentsHudRange);
	bool IsWeaponSelectable(atHashWithStringNotFinal WeaponHash) const;

	void SetSpecialVehicleWeapon( u32 uHash, const char* locKey );

	void ClearSpecialVehicleWeaponName(u32 uWeaponHash);
	void SetPointer( eWeaponWheelSlot iStickRotation );
	bool SelectRadialSegment(int iDirection);
	bool HandleControls(CPed* pPlayerPed, bool& bNeedsRefresh);

	bool IsForcedOpenByTimer();
	

#if __BANK
	void UpdateScaleform3DSettings();
	void DebugSetMemory();
	s32 m_iWeaponComboIndex;
	bool m_bDbgInvertButton;
	float m_perspFOV;
	float m_x;
	float m_y;
	float m_z;
	float m_xRotation;
	float m_yRotation;
	float m_maxRotation;
	float m_tweenDuration;
	float m_bgZ;
	bool m_bBGVisibility;
	bool m_bFiniteRotation;
#if RSG_PC
	void UpdateStereoDepthScalar();
	float m_StereoDepthScalar;
#endif
#endif
};



#endif // __CWEAPON_WHEEL__

