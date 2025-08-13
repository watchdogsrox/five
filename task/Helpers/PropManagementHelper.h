#ifndef PROP_MANAGEMENT_HELPER_H
#define PROP_MANAGEMENT_HELPER_H

//Rage headers
#include "atl/ptr.h"
#include "fwtl/Pool.h"

//Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animhelpers.h"
#include "fwutil/Flags.h"

//Game headers
#include "scene/RegdRefTypes.h"


//Forward declarations
class CPed;
class CClipHelper;
class CConditionalAnimsGroup;
class CObject;

////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	A small helper class that tasks can use to claim an existing ambient ped prop
	that would otherwise be dropped or destroyed.
*/
class CPropManagementHelper : public rage::atReferenceCounter
{
public:
	friend class CTaskAmbientMigrateHelper;

	FW_REGISTER_CLASS_POOL(CPropManagementHelper);

	class Context
	{
	public:

		Context(CPed& in_Ped)
			: m_Ped(in_Ped)
		{			
		}

		CPed& m_Ped;

		CClipHelper* m_ClipHelper;
		fwMvClipSetId m_PropClipSetId;
		fwMvClipId m_PropClipId;


		void operator=(const Context& rhs)
		{
			m_ClipHelper				= rhs.m_ClipHelper;
			m_PropClipSetId				= rhs.m_PropClipSetId;
			m_PropClipId				= rhs.m_PropClipId;
		}

	};

	// smart ptr types
	typedef rage::atPtr<CPropManagementHelper>       SPtr;      
	typedef rage::atPtr<const CPropManagementHelper> SPtrToConst;

	CPropManagementHelper();
	~CPropManagementHelper();


	strStreamingObjectName GetPropNameHash() const { return m_PropStreamingObjectNameHash; }
	

	// update.  try to grab the existing prop
	void Process (Context& in_Context);

	// PURPOSE: called to process animation triggers - creating/destroying/dropping props
	void ProcessMoveSignals(Context& in_Context);

	// Do any processing needed when this clip is about to stop playing.
	void ProcessClipEnding (Context& in_Context);

	// task is starting anim and it's flagging a prop to stream
	void RequestProp (const strStreamingObjectName& in_PropStreamingObjectName);

	bool PickPermanentProp (CPed* pPed, const CConditionalAnimsGroup* pConditionalAnimsGroup, s16& iInOutConditionalAnimChosen, bool bForcePick);
	void UpdateExistingOrMissingProp (CPed& ped);
	static bool DisposeOfPropIfInappropriate(CPed& ped, s32 iScenarioIndex, CPropManagementHelper* pPropManagementHelper);
	void DisposeOfProp(CPed& ped);
	void DropProp(CPed& ped, bool activatePhys);

	bool IsPropReady()  const { return IsPropNotRequired() || IsPropLoaded(); }
	bool IsPropNotRequired() const { return m_PropStreamStatus == PS_NotRequired; }
	bool IsPropLoading()	 const { return m_PropStreamStatus == PS_Loading; }
	bool IsPropLoaded()		 const { return m_PropStreamStatus == PS_LoadedAndReffed; }

	void ReleasePropRefIfUnshared(CPed* pPed);

	void SetPropInEnvironment(CObject* prop)	{ m_PropInEnvironment = prop; }
	CObject* GetPropInEnvironment() const		{ return m_PropInEnvironment; }

	// PURPOSE: Give ped a prop
	bool GivePedProp (CPed& pPed);
	void GrabPropFromEnvironment(CPed& ped);

	void SetCreatePropInLeftHand (bool bOnOff) 
	{
		if (bOnOff)
		{
			m_Flags.SetFlag(Flag_CreatePropInLeftHand);
		}
		else 
		{
			m_Flags.ClearFlag(Flag_CreatePropInLeftHand);
		}
	}

	void SetUsePropFromEnvironment (bool bOnOff)
	{
		if (bOnOff)
		{
			m_Flags.SetFlag(Flag_UsePropFromEnvironment);
		}
		else
		{
			m_Flags.ClearFlag(Flag_UsePropFromEnvironment);
		}
	}

	void SetDontActivatePhysicsOnRelease(bool bOnOff)
	{
		if (bOnOff)
		{
			m_Flags.SetFlag(Flag_DontActivatePhysicsOnRelease);
		}
		else
		{
			m_Flags.ClearFlag(Flag_DontActivatePhysicsOnRelease);
		}
	}

	bool IsCreatePropInLeftHand() const { return m_Flags.IsFlagSet(Flag_CreatePropInLeftHand); }
	bool UsePropFromEnvironment() const { return m_Flags.IsFlagSet(Flag_UsePropFromEnvironment); }
	bool DontActivatePhysicsOnRelease() const { return m_Flags.IsFlagSet(Flag_DontActivatePhysicsOnRelease); }

	bool IsPropPersistent()   const		{ return m_Flags.IsFlagSet(Flag_PropPersistsOverClips); }

	void SetForcedLoading();
	bool IsForcedLoading()  const		{ return m_Flags.IsFlagSet(Flag_ForcedLoading); }

	bool IsHoldingProp() const			{ return m_Flags.IsFlagSet(Flag_HoldingProp); }

	bool IsPropSharedAmongTasks() const { return references > 1; }

	void SetDestroyProp(bool bDestroy)		{ if (bDestroy) { m_Flags.SetFlag(Flag_DestroyPropWhenFinished); } else { m_Flags.ClearFlag(Flag_DestroyPropWhenFinished); } }
	bool ShouldDestroyProp() const				{ return m_Flags.IsFlagSet(Flag_DestroyPropWhenFinished); }

private:

	// Prop streaming states
	enum PropStreamStatus
	{
		PS_NotRequired = 0,
		PS_Loading,
		PS_LoadedAndReffed,
		PS_MaxStatus,
	};

	enum PropManagementHelperFlags
	{
		Flag_CreatePropInLeftHand					= BIT0,
		Flag_PropPersistsOverClips					= BIT1,
		Flag_ForcedLoading							= BIT2,
		Flag_HoldingProp							= BIT3,
		Flag_DestroyPropWhenFinished				= BIT4,
		Flag_UsePropFromEnvironment					= BIT5,
		Flag_ReattemptSetWeaponObjectInfo			= BIT6,
		Flag_MoveCreatePropTag						= BIT7,
		Flag_MoveDestroyPropTag						= BIT8,
		Flag_MoveReleasePropTag						= BIT9,
		Flag_MoveFlashlightTag						= BIT10,
		Flag_DontActivatePhysicsOnRelease = BIT11
	};

	// PURPOSE: request the asset load if needed
	void ProcessPropLoading(Context& in_Context);
	// animation triggers creating/destroying/dropping prop
	void ProcessClipEvents (Context& in_Context);
	void ProcessReattemptWeaponObjectInfo (Context& in_Context);

	// clip event handlers for creating and destroying props
	void HandleClipEventCreateProp(Context& in_Context);
	void HandleClipEventDestroyProp(Context& in_Context);
	void HandleClipEventReleaseProp(Context& in_Context);
	void HandleClipEventFlashLight(Context& in_Context, atHashString lightHash, bool bOnOff);

	void HasDestroyOrReleaseTag(Context& in_Context, bool& hasDestroyOut, bool& hasReleaseOut) const;

	void SetPropName(strStreamingObjectName propName) { m_PropStreamingObjectNameHash = propName; }

	// release 
	void ReleasePropStreamingRef();

	void SetHoldingProp(CPed* ped, bool bHolding);

	void SetPropWeaponInstructionForDisposal(CPed& ped);

	void SetWeaponObjectPropInfo( CObject* pWeaponObject, CPed& pPed );

	// Prop status information
	PropStreamStatus m_PropStreamStatus;

	RegdObj m_PropInEnvironment;
	
	// Prop streaming object
	strStreamingObjectName m_PropStreamingObjectNameHash;

	//Context m_sContext;
	fwFlags32 m_Flags;

};

#endif // PROP_MANAGEMENT_HELPER_H
