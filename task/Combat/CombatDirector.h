#ifndef COMBAT_DIRECTOR_H
#define COMBAT_DIRECTOR_H

// Rage headers
#include "atl/array.h"
#include "atl/queue.h"
#include "fwtl/pool.h"
#include "fwtl/regdrefs.h"
#include "fwutil/Flags.h"
#include "scene/RegdRefTypes.h"

// Game headers
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward Declarations
class CCombatOrders;
class CCombatSituation;
class CEventGunShot;
class CEventGunShotBulletImpact;
class CEventGunShotWhizzedBy;

// Typedefs
typedef	fwRegdRef<class CCombatDirector> RegdCombatDirector;

//////////////////////////////////////////////////////////////////////////
// CCombatDirector
//////////////////////////////////////////////////////////////////////////

class CCombatDirector : public fwRefAwareBase
{
private:
	
	enum CombatDirectorActionFlags
	{
		CDAF_GunShot				= BIT0,
		CDAF_GunShotWhizzedBy		= BIT1,
		CDAF_GunShotBulletImpact	= BIT2,
	};

private:

	struct CoordinatedShooting
	{
		enum State
		{
			Start,
			WaitBeforeReady,
			WaitBeforeShoot,
		};

		CoordinatedShooting()
		: m_nState(Start)
		, m_fTimeInState(0.0f)
		, m_fTimeToWait(0.0f)
		, m_pLeader(NULL)
		{}

		State	m_nState;
		float	m_fTimeInState;
		float	m_fTimeToWait;
		RegdPed	m_pLeader;
	};
	
	struct PedInfo
	{
		PedInfo()
		: m_pPed(NULL)
		, m_fInactiveTimer(0.0f)
		, m_bActive(false)
		{
		
		}
		
		RegdPed	m_pPed;
		float	m_fInactiveTimer;
		bool	m_bActive;
	};

	FW_REGISTER_CLASS_POOL(CCombatDirector);
	
	CCombatDirector();
	virtual ~CCombatDirector();
	
	CCombatDirector(const CCombatDirector& other);
	CCombatDirector& operator=(const CCombatDirector& other);

public:

	const CPed* GetLastPedKilled() const { return m_pLastPedKilled; }
	
public:

	const float	GetAction() const { return m_fAction; }
	CCombatSituation* GetSituation() const { return m_pCombatSituation; }
	
	void	Activate(CPed& rPed);
	void	ChangeSituation(CCombatSituation* pCombatSituation, const bool bAssertIfInvalid = true);
	int		CountPedsAdvancingWithSeek(const CEntity& rTarget, int iMaxCount = -1) const;
	void	Deactivate(CPed& rPed);
	bool	DoesAnyoneHaveClearLineOfSight(const CEntity& rTarget) const;
	bool	DoesPedHaveGun(const CPed& rPed) const;
	int		GetNumPeds() const;
	u32		GetNumPedsKilledWithinTime(const float fTime) const;
	CPed*	GetPed(const int iIndex) const;
	bool	IsFull() const;
	void	OnGunShot(const CEventGunShot& rEvent);
	void	OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent);
	void	OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);
	void	Update(const float fTimeStep);

	u8		GetNumPedsWith1HWeapons() const { return m_uNumPedsWith1HWeapons; }
	u8		GetNumPedsWith2HWeapons() const { return m_uNumPedsWith2HWeapons; }
	u8		GetNumPedsWithGuns() const { return m_uNumPedsWithGuns; }

	CoverVariationsInfo& GetCoverPeekingVariations1HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverPeekingVariationsInfo(false, bIsHigh); }
	CoverVariationsInfo& GetCoverPeekingVariations2HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverPeekingVariationsInfo(true, bIsHigh); }

	CoverVariationsInfo& GetCoverPinnedVariations1HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverPinnedVariationsInfo(false, bIsHigh); }
	CoverVariationsInfo& GetCoverPinnedVariations2HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverPinnedVariationsInfo(true, bIsHigh); }

	CoverVariationsInfo& GetCoverReactOutroVariations1HInfo() { return m_CoverClipVariationHelper.GetCoverReactOutroVariationsInfo(false); }
	CoverVariationsInfo& GetCoverReactOutroVariations2HInfo() { return m_CoverClipVariationHelper.GetCoverReactOutroVariationsInfo(true); }

	CoverVariationsInfo& GetCoverIdleVariations1HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverIdleVariationsInfo(false, bIsHigh); }
	CoverVariationsInfo& GetCoverIdleVariations2HInfo(bool bIsHigh) { return m_CoverClipVariationHelper.GetCoverIdleVariationsInfo(true, bIsHigh); }
	
	static CCombatDirector* FindNearbyCombatDirector(const CPed& rPed, CCombatDirector* pException = NULL);
	static void				Init(unsigned initMode);
	static void				RefreshCombatDirector(CPed& rPed);
	static void				ReleaseCombatDirector(CPed& rPed);
	static void				RequestCombatDirector(CPed& rPed);
	static void				Shutdown(unsigned shutdownMode);
	static void				UpdateCombatDirectors();

	static CPed*			GetPlayerPedIsArresting(const CPed* pPed);
	
private:

	bool	AddPed(CPed& rPed);
	int		FindIndex(const CPed& rPed) const;
	CPed*	FindLeaderForCoordinatedShooting() const;
	void	InitializeSituation();
	void	OnActivate(CPed& rPed);
	void	OnAdd(CPed& rPed);
	void	OnDeactivate(CPed& rPed);
	void	OnPedKilled(const CPed& rPed);
	void	OnRemove(CPed& rPed);
	bool	RemoveIndex(const int iIndex);
	bool	RemovePed(CPed& rPed);
	void	UninitializeSituation();
	void	UpdateAction(const float fTimeStep);
	void	UpdateCoordinatedShooting(float fTimeStep);
	void	UpdatePeds(const float fTimeStep);
	void	UpdateSituation(const float fTimeStep);
	void	UpdateWeaponStats();
	void	UpdateWeaponStreaming();
	bool	WasPedKilledWithScaryWeapon(const CPed& rPed) const;
	
private:

	static const int sMaxPeds			= 32;
	static const u32 sMaxPedKillTimes	= 8;

private:

	atFixedArray<PedInfo, sMaxPeds>		m_aPeds;
	atQueue<u32, sMaxPedKillTimes>		m_aPedKillTimes;
	CoordinatedShooting					m_CoordinatedShooting;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelperForBuddyShotPistol;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelperForBuddyShotRifle;
	CCombatSituation*					m_pCombatSituation;
	RegdConstPed						m_pLastPedKilled;
	float								m_fAction;
	fwFlags8							m_uActionFlags;
	u8									m_uNumPedsWith1HWeapons;
	u8									m_uNumPedsWith2HWeapons;
	u8									m_uNumPedsWithGuns;

private:

	CAiCoverClipVariationHelper m_CoverClipVariationHelper;
};

#endif // COMBAT_DIRECTOR_H
