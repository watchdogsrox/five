/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedFactory.h
// PURPOSE :	The factory class for peds and dummy peds.
// AUTHOR :		Obbe.
//				Adam Croston.
// CREATED :	
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_PEDFACTORY_H_
#define INC_PEDFACTORY_H_

// rage includes
#include "debug/debug.h"
#include "vector/matrix34.h"

// fw includes
#include "entity/archetypemanager.h"
#include "fwutil/PtrList.h"

// game includes
#include "scene/RegdRefTypes.h"

// forward declarations
class CPed;
class CControlledByInfo;
class CVehicle;
class CPlayerInfo;
class CDynamicEntity;
class CPedVariationData;
class CPedPopulation;
class CLoadedModelGroup;

#if !__FINAL
	extern bool gbAllowPedGeneration;
#endif //!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// This class deals with creation and removal of ped objects in memory.
/////////////////////////////////////////////////////////////////////////////////
class CPedFactory
{
public:
			CPedFactory();
	virtual	~CPedFactory();

	static	void			CreateFactory();
	static	void			DestroyFactory();
	static	CPedFactory*	GetFactory() {return ms_pInstance;}

	virtual	CPed*			CreatePed(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, bool bApplyDefaultVariation, bool bShouldBeCloned, bool bCreatedByScript, bool bFailSilentlyIfOutOfPeds = false, bool bScenarioPedCreatedByConcealeadPlayer = false);
	virtual	CPed*			CreatePedFromSource(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, const CPed* pSourcePed, bool bStallForAssetLoad, bool bShouldBeCloned, bool bCreatedByScript);
	virtual CPed*			ClonePed(const CPed* source, bool bRegisterAsNetworkObject, bool bLinkBlends, bool bCloneCompressedDamage);
	virtual void			ClonePedToTarget(const CPed* source, CPed* target, bool bCloneCompressedDamage);

	virtual	CPed*			CreatePlayerPed(const CControlledByInfo& pedControlInfo, u32 modelIndex, const Matrix34 *pMat, CPlayerInfo* pPlayerInfo = NULL);
	virtual	void			DestroyPlayerPed(CPed* pPlayerPed);

	CPed*					GetLocalPlayer() { return m_pLocalPlayer; }
    void					SetLocalPlayer(CPed* pPed) { m_pLocalPlayer = pPed; }

#if __DEV
	static	bool			IsCurrentlyWithinFactory				() { return ms_bWithinPedFactory; }
	static	bool			IsCurrentlyInDestroyPedFunction			() { return ms_bInDestroyPedFunction; }
	static	bool			IsCurrentlyInDestroyPlayerPedFunction	() { return ms_bInDestroyPlayerPedFunction; }
#endif
#if __BANK
	static CPed*			GetLastCreatedPed() { return ms_pLastCreatedPed; }
#endif

	bool					DestroyPed(CPed* pPed, bool cached = false);
	
	bool					AddDestroyedPedToCache(CPed* ped);
	void					ClearDestroyedPedCache();
	static void				ProcessDestroyedPedsCache();
	bool					IsPedInDestroyedCache(const CPed* ped);
	void					RemovePedGroupFromCache(const CLoadedModelGroup& pedGroup);

	static bool				ms_reuseDestroyedPeds;
	BANK_ONLY(static bool	ms_reuseDestroyedPedsDebugOutput);

#if __BANK
	static void				LogDestroyedPed(CPed * pPed, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol);
	static void				LogCreatedPed(CPed * pPed, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol);

	static bool				ms_bLogCreatedPeds;
	static bool				ms_bLogDestroyedPeds;
	static u32				ms_iCreatedPedCount;
	static u32				ms_iDestroyedPedCount;
#endif

	void					InitCachedPed			(CPed* ped);
protected:
	CPed*					ConcreteCreatePed		(const CControlledByInfo& pedControlInfo, u32 modelIndex, bool bFailSilentlyIfOutOfPeds) const;
	CPed*					CreatePedFromDestroyedCache(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, bool bShouldBeCloned);
	static bool				EjectOnePedFromCache	();

	bool					DestroyPedInternal		(CPed* pPed, bool cached = false);

	void					CopyVariation			(const CPed* source, CPed* dest, bool stallforAssets);

	static	CPedFactory*	ms_pInstance;
#if __DEV
	static	bool			ms_bWithinPedFactory;
	static	bool			ms_bInDestroyPedFunction;
	static	bool			ms_bInDestroyPlayerPedFunction;
#endif

	struct sDestroyedPed
	{
		CPed* ped;
		u32 nukeTime;
		s8 framesUntilReuse;
	};
	enum { MAX_DESTROYED_PEDS_CACHED = 30 };
	static sDestroyedPed	ms_reuseDestroyedPedArray[MAX_DESTROYED_PEDS_CACHED];
	static u32				ms_reuseDestroyedPedCacheTime;
	static u32				ms_reuseDestroyedPedCount;

#if __BANK
	static RegdPed			ms_pLastCreatedPed;
#endif

private:
	CPed*		SetUpAsCivilianPed	(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, const bool bCreatedFromScript, bool bFailSilentlyIfOutOfPeds = false) const;
	CPed*		SetUpAsCopPed		(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, const bool bCreatedFromScript = false, bool bFailSilentlyIfOutOfPeds = false) const;
	CPed*		SetUpAsPlayerPed	(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, CPlayerInfo& playerInfo) const;

	void		VariationInitialisation(CPed* pPed);

	void		SetPedModelMatAndMovement(CPed* pPed, const u32 modelIndex, const Matrix34 * pMat) const;

	RegdPed		m_pLocalPlayer; // ptr to the local player
};

#endif // INC_PEDFACTORY_H_
