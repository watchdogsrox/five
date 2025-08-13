#ifndef __INC_TRAILER_H__
#define __INC_TRAILER_H__

#include "network\objects\entities\netObjTrailer.h"
#include "physics\constrainthandle.h"
#include "vehicles/automobile.h"
#include "vehicles/VehicleGadgets.h"

//////////////////////////////////////////////////////////////////////////
// Class to encapsulate the collision on the 
class CTrailerLegs : public CVehicleGadget
{
public:
	CTrailerLegs();
	CTrailerLegs(int iBoneIndex, CVehicle* pParent);	

	virtual void ProcessPhysics(CVehicle* pParent, const float fTimestep, const int nTimeSlice);
	virtual bool WantsToBeAwake(CVehicle* ) { return (m_fTargetRatio != m_fCurrentRatio); }

	virtual s32 GetType() const { return VGT_TRAILER_LEGS; }

	void Init(int iBoneIndex, CVehicle* pParent);

	// 0.0f = Fully up
	// 1.0f = Fully down
	void SetTargetRatio(float fTargetRatio) { m_fTargetRatio = fTargetRatio; }
	void SetCurrentRatio(float fCurrentRatio) { m_fCurrentRatio = fCurrentRatio; }

	void UpdateToCurrentRatio(CVehicle & parentTrailer);

    void PreRender(CVehicle *pParent);

	float GetCurrentRatio() const { return m_fCurrentRatio; }

	int GetFragChild() const { return m_iFragChild; }

protected:
	int m_iBoneIndex;
	int m_iFragChild;

	float m_fTargetRatio;
	float m_fCurrentRatio;

    float m_fDisplaceDistance;
};

// Class to represent rear trailer on articulated trucks
class CTrailer : public CAutomobile
{
public:

    NETWORK_OBJECT_TYPE_DECL( CNetObjTrailer, NET_OBJ_TYPE_TRAILER );

	CTrailer(const eEntityOwnedBy ownedBy, u32 popType);
	virtual ~CTrailer();

// Init
	virtual void InitCompositeBound();
	virtual int  InitPhys();

    virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false);

// Process
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	void StartWheelIntegratorIfNecessary(ePhysicsResult automobilePhysicsResult, float fTimeStep);
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);

    virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void	PreRender2(const bool bIsVisibleInMainViewport = true);


	// Trailer interface
	// Overrides attachment system
#if __BANK
	#define AttachToParentVehicle(...) DebugAttachToParentVehicle(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	bool			DebugAttachToParentVehicle(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CVehicle* pParent, bool bWarp, float fInvMass = 1.0f, bool bForceReAttach = false);
#else // __BANK
	bool			AttachToParentVehicle(CVehicle* pParent, bool bWarp, float fInvMass = 1.0f, bool bForceReAttach = false);
#endif

	virtual void	DetachFromParent(u16 nDetachFlags);
    bool			IsAttachedToParentVehicle() const  { return m_isAttachedToParent; }

    //This should no longer be used, except in exceptional circumstances
    void            SetTrailerConstraintIndestructible(bool indestructibleTrailerConstraint);
	bool			IsTrailerConstraintIndestructible() const;

	//Warp the trailer to a position using the attach bone offsets
	void			WarpToPosition( CVehicle* pParent, bool translateOnly = false );
	void			WarpParentToPosition();

	Vec3V_Out CalcTrailerVirtualWheelAtAttachmentPoint(Vec3V_In vParentAttach, Vec3V_In vRearWheelContactPoint, Vec3V_In vRearWheelDesired, float fWheelRadius) const;

	//Cargo vehicles
	void			AddCargoVehicle(u32 index, CVehicle* veh);
	u32				RemoveCargoVehicle(CVehicle* veh);
	void			DetachCargoVehicles();
	int				FindCargoVehicle(CVehicle* veh) const;
	bool			HasCargoVehicles() const {return m_cargoVehiclesAttached;}
	CVehicle*		GetCargoVehicle(u32 index) const {return m_cargoVehicles[index];}
	bool			GetCollidedWithAttachParent() const {return m_bCollidedWithAttachParent;}
	void			SetCollidedWithAttachParent(bool collided) {m_bCollidedWithAttachParent = collided;}

	virtual bool	UsesDummyPhysics(const eVehicleDummyMode vdm) const;
	virtual bool	GetCanMakeIntoDummy(eVehicleDummyMode dummyMode);
	virtual void	SwitchToFullFragmentPhysics(const eVehicleDummyMode prevMode);
	virtual void	SwitchToDummyPhysics(const eVehicleDummyMode prevMode);
	virtual void	SwitchToSuperDummyPhysics(const eVehicleDummyMode prevMode);

	virtual bool	TryToMakeFromDummy(const bool bSkipClearanceTest=false);
	virtual void	ProcessPreComputeImpacts(phContactIterator impacts);

    float           GetTotalMass();//get mass of trailer and attached children.
	void			SetInverseMassScale(float fInvMass);

	bool			AreTrailerLegsLoweredFully() const { return m_trailerLegs.GetCurrentRatio() >= 1.0f; }

	void			RaiseTrailerLegsInstantly() { m_trailerLegs.SetCurrentRatio(0.0f); m_trailerLegs.UpdateToCurrentRatio(*this); }
	void			LowerTrailerLegsInstantly() { m_trailerLegs.SetCurrentRatio(1.0f); m_trailerLegs.UpdateToCurrentRatio(*this); }
	bool			HasBreakableExtras() const { return m_bHasBreakableExtras; }

	const CVehicle* GetAttachedToParent() const { return (const CVehicle*) m_AttachedParent; }

	void			SetLocalPlayerCanAttach( bool canAttach ) { m_bCanLocalPlayerAttach = canAttach; }
	bool			GetLocalPlayerCanAttach() { return m_bCanLocalPlayerAttach; }

	const CTrailerLegs* GetTrailerLegs() { return &m_trailerLegs; }

	//void			SetOverrideExendableRatio( bool overrideRatio ) { m_scriptOverridingExtendableRatio = overrideRatio; }
	//void			SetExendableTargetRatio( float targetRatio ) { m_extendingSidesTargetRatio = targetRatio; }
	//void			SetExendableRatio( float targetRatio );

protected:
	CTrailerLegs m_trailerLegs;

	// Constraints for attaching trailer to cab
	phConstraintHandle m_posConstraintHandle;

	// Rotational constraints
	phConstraintHandle m_rotXConstraintHandle;
	phConstraintHandle m_rotYConstraintHandle;
	phConstraintHandle m_rotZConstraintHandle;

private:

    static const unsigned MAX_ATTACHMENTS = 10;
    void GetPhysicallyAttachedObjects(CPhysical *attachedObj, atFixedArray<CPhysical *, MAX_ATTACHMENTS> &attachmentList);
	//void UpdateExtendingSides( float fTimeStep );

	RegdVeh m_AttachedParent;

    Vector3 m_vLastFramesCrossParentUpTrailerUp;
    bool m_isAttachedToParent; // required for network code to detect attachment to a parent vehicle (attach flags not sufficient)

	atRangeArray<CVehicle*,MAX_CARGO_VEHICLES> m_cargoVehicles;
	bool m_cargoVehiclesAttached;
	bool m_bCollidedWithAttachParent;
	bool m_bHasBreakableExtras;


	//bool m_bHasExtendingSides;
	//bool m_scriptOverridingExtendableRatio;
	//float m_extendingSidesRatio;
	//float m_extendingSidesTargetRatio;
	//float m_stationaryDuration;

	bool m_bIsTrailerAttachmentEnabled; // Flag for script to block a trailer from being able to attach to vehicles
	bool m_bCanLocalPlayerAttach;

	bool m_bDisableCollisionsWithObjectsBasicallyAttachedToParent;
	bool m_bDisablingImpacts;

public:
	void SetTrailerAttachmentEnabled(bool bNewValue) { m_bIsTrailerAttachmentEnabled = bNewValue; }
	bool GetTrailerAttachmentEnabled() { return m_bIsTrailerAttachmentEnabled; }

	void SetDisableCollisionsWithParentChildren(bool bNewValue) { m_bDisableCollisionsWithObjectsBasicallyAttachedToParent = bNewValue; }
	bool GetDisableCollisionsWithParentChildren() { return m_bDisableCollisionsWithObjectsBasicallyAttachedToParent; }

	// Debug statics
public:
#if __BANK
	static void InitWidgets(bkBank* pBank);

	static bank_bool ms_bDebugDrawTrailers;
#endif

	static const Vector3 sm_offsets[MAX_CARGO_VEHICLES];
	static float sm_fTrailerYOpposeDetach;
	static float sm_fTrailerYOpposeDetachSphericalConstraintInMP;
	static float sm_fAttachOffsetHauler;
	
};

#endif
