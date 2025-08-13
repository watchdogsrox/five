// FILE :    TaskTakeOffPedVariation.h

#ifndef TASK_TAKE_OFF_PED_VARIATION_H
#define TASK_TAKE_OFF_PED_VARIATION_H

// Rage headers
#include "entity/archetypemanager.h"
#include "fwanimation/boneids.h"

// Game headers
#include "Peds/rendering/PedVariationDS.h"
#include "streaming/streamingrequest.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Peds/QueriableInterface.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskTakeOffPedVariation
////////////////////////////////////////////////////////////////////////////////

class CTaskTakeOffPedVariation : public CTaskFSMClone
{
	friend class ClonedTakeOffPedVariationInfo;

private:

	enum RunningFlags
	{
		RF_WasInterrupted			= BIT0,
		RF_HasVelocityInheritance	= BIT1,
		RF_DestroyProp				= BIT2,
		RF_HasRestoredVariation		= BIT3,
		RF_NumFlags					= 4,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		PAR_PARSABLE;
	};
	
public:

	enum State
	{
		State_Start = 0,
		State_Stream,
		State_TakeOffPedVariation,
		State_Finish
	};
	
public:

	explicit CTaskTakeOffPedVariation(fwMvClipSetId clipSetId, fwMvClipId clipIdForPed, fwMvClipId clipIdForProp,
		ePedVarComp nVariationComponent, u8 uVariationDrawableId, u8 uVariationDrawableAltId, u8 uVariationTexId, atHashWithStringNotFinal hProp,
		eAnimBoneTag nAttachBone, CObject *pProp = NULL, bool bDestroyProp = true);
	virtual ~CTaskTakeOffPedVariation();

	CTaskTakeOffPedVariation() {}

public:

	void SetAttachOffset(Vec3V_In vOffset)				{ m_vAttachOffset = vOffset; }
	void SetAttachOrientation(QuatV_In qOrientation)	{ m_qAttachOrientation = qOrientation; }
	void SetBlendInDeltaForPed(float fDelta)			{ m_fBlendInDeltaForPed = fDelta; }
	void SetBlendInDeltaForProp(float fDelta)			{ m_fBlendInDeltaForProp = fDelta; }
	void SetBlendOutDelta(float fBlendOutDelta)			{ m_fBlendOutDelta = fBlendOutDelta; }
	void SetForceToApplyAfterInterrupt(float fForce)	{ m_fForceToApplyAfterInterrupt = fForce; }
	void SetPhaseToBlendOut(float fPhase)				{ m_fPhaseToBlendOut = fPhase; }

public:

	void SetVelocityInheritance(Vec3V_In vVelocity)
	{
		//Set the velocity.
		m_vVelocityInheritance = vVelocity;

		//Set the flag.
		m_uRunningFlags.SetFlag(RF_HasVelocityInheritance);
	}

	//! Static functions so that script can remove variation without playing anim (and to to maintain 1 code path for removing things like parachutes...)
	static CObject *CreateProp(CPed *pPed, ePedVarComp nVariationComponent, const fwModelId &modelID);
	static u32	GetTintIndexFromVariation(CPed *pPed, ePedVarComp nVariationComponent);
	static u32	GetTextureIndexFromVariation(CPed *pPed, ePedVarComp nVariationComponent);
	static fwModelId	GetModelIdForProp(const atHashWithStringNotFinal &hProp);
	static void AttachProp(CObject *pProp, CPed *pPed, eAnimBoneTag nAttachBone, const Vec3V& vAttachOffset, const QuatV& qAttachOrientation, const Vector3& vAdditionalOffset, const Quaternion& qAdditionalRotation);
	static void ClearVariation(CPed* pPed, ePedVarComp nVariationComponent, bool bShouldRestorePreviousVariation = true);

private:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_TAKE_OFF_PED_VARIATION; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

  // Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
    virtual bool 				IsInScope(const CPed* pPed);

public:

#ifndef __FINAL
	void						DebugRenderClonedInformation(void);
#endif /* __FINAL */

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Stream_OnUpdate();
	void		TakeOffPedVariation_OnEnter();
	FSM_Return	TakeOffPedVariation_OnUpdate();
	void		TakeOffPedVariation_OnExit();

private:

	bool		ArePlayerControlsDisabled() const;
	void		AttachProp_Internal();
	void		ClearVariation_Internal();
	void		CreateProp_Internal();
	void		DestroyProp();
	void		DetachProp(bool bUseVelocityInheritance);
	bool		IsPlayerTryingToAimOrFire() const;
	bool		IsPlayerTryingToSprint() const;
	bool		IsPropAttached() const;
	bool		IsVariationActive() const;
	void		ProcessStreaming();
	void		ProcessStreamingForClipSet();
	void		ProcessStreamingForModel();
	bool		ShouldDelayVariationRestore() const;
	bool		ShouldDetachProp() const;
	bool		ShouldProcessPhysics() const;
	void		StartAnimationForPed();
	void		StartAnimationForProp();
	void		StartAnimations();
	bool		UpdateInterrupts();
	void		UpdatePropAttachment();

private:

	Vec3V						m_vAttachOffset;
	QuatV						m_qAttachOrientation;
	Vec3V						m_vVelocityInheritance;
	fwClipSetRequestHelper		m_ClipSetRequestHelper;
	fwMvClipSetId				m_ClipSetId;
	fwMvClipId					m_ClipIdForPed;
	fwMvClipId					m_ClipIdForProp;
	strRequest					m_ModelRequestHelper;
	RegdObj						m_pExistingProp;
	RegdObj						m_pProp;
	float						m_fBlendInDeltaForPed;
	float						m_fBlendInDeltaForProp;
	float						m_fPhaseToBlendOut;
	float						m_fBlendOutDelta;
	float						m_fForceToApplyAfterInterrupt;
	atHashWithStringNotFinal	m_hProp;
	ePedVarComp					m_nVariationComponent;
	eAnimBoneTag				m_nAttachBone;
	fwFlags8					m_uRunningFlags;
	u8							m_uVariationDrawableId;
	u8							m_uVariationDrawableAltId;
	u8							m_uVariationTexId;
	
private:

	static Tunables	sm_Tunables;

};

class ClonedTakeOffPedVariationInfo : public CSerialisedFSMTaskInfo
{
	public:

		struct InitParams
		{
			InitParams(	fwMvClipSetId				clipSetId, 
						fwMvClipId					clipIdForPed, 
						fwMvClipId					clipIdForProp,
						ePedVarComp					nVariationComponent, 
						u8							uVariationDrawableId, 
						u8							uVariationDrawableAltId, 
						u8							uVariationTexId,
						atHashWithStringNotFinal	hProp, 
						eAnimBoneTag				nAttachBone,
						Vec3V_In					vVelocityInheritance,
						Vec3V_In					vAttachOffset,
						QuatV_In					qAttachOrientation,
						float						fBlendInDeltaForPed,
						float						fBlendInDeltaForProp,
						float						fPhaseToBlendOut,
						float						fBlendOutDelta,
						float						fForceToApplyAfterInterrupt,
						u8							uRunningFlags
						)
			{
				m_clipSetId						= clipSetId;
				m_clipIdForPed					= clipIdForPed;
				m_clipIdForProp					= clipIdForProp;
				m_nVariationComponent			= nVariationComponent;
				m_uVariationDrawableId			= uVariationDrawableId;
				m_uVariationDrawableAltId		= uVariationDrawableAltId;
				m_uVariationTexId				= uVariationTexId;
				m_hProp							= hProp;
				m_nAttachBone					= nAttachBone;
				m_vAttachOffset					= vAttachOffset;
				m_qAttachOrientation			= qAttachOrientation;
				m_fBlendInDeltaForPed			= fBlendInDeltaForPed;
				m_fBlendInDeltaForProp			= fBlendInDeltaForProp;
				m_fPhaseToBlendOut				= fPhaseToBlendOut;
				m_fBlendOutDelta				= fBlendOutDelta;
				m_fForceToApplyAfterInterrupt	= fForceToApplyAfterInterrupt;
				m_vVelocityInheritance			= vVelocityInheritance;
				m_uRunningFlags					= uRunningFlags;
			}

			fwMvClipSetId				m_clipSetId;
			fwMvClipId					m_clipIdForPed;
			fwMvClipId					m_clipIdForProp;
			ePedVarComp					m_nVariationComponent;
			u8							m_uVariationDrawableId;
			u8							m_uVariationDrawableAltId;
			u8							m_uVariationTexId;
			atHashWithStringNotFinal	m_hProp;
			eAnimBoneTag				m_nAttachBone;	
			Vec3V						m_vAttachOffset;
			Vec3V						m_vVelocityInheritance;
			QuatV						m_qAttachOrientation;
			float						m_fBlendInDeltaForPed;
			float						m_fBlendInDeltaForProp;
			float						m_fPhaseToBlendOut;
			float						m_fBlendOutDelta;
			float						m_fForceToApplyAfterInterrupt;
			u8							m_uRunningFlags;
		};

	public:	
	
		ClonedTakeOffPedVariationInfo(InitParams const& initParams)
		:
			m_clipSetId(initParams.m_clipSetId),
			m_clipIdForPed(initParams.m_clipIdForPed),
			m_clipIdForProp(initParams.m_clipIdForProp),
			m_nVariationComponent(initParams.m_nVariationComponent),
			m_uVariationDrawableId(initParams.m_uVariationDrawableId),
			m_uVariationDrawableAltId(initParams.m_uVariationDrawableAltId),
			m_uVariationTexId(initParams.m_uVariationTexId),
			m_hProp(initParams.m_hProp),
			m_nAttachBone(initParams.m_nAttachBone),
			m_vAttachOffset(initParams.m_vAttachOffset),
			m_qAttachOrientation(initParams.m_qAttachOrientation),
			m_fBlendInDeltaForPed(initParams.m_fBlendInDeltaForPed),
			m_fBlendInDeltaForProp(initParams.m_fBlendInDeltaForProp),
			m_fPhaseToBlendOut(initParams.m_fPhaseToBlendOut),
			m_fBlendOutDelta(initParams.m_fBlendOutDelta),
			m_fForceToApplyAfterInterrupt(initParams.m_fForceToApplyAfterInterrupt),
			m_vVelocityInheritance(initParams.m_vVelocityInheritance),
			m_uRunningFlags(initParams.m_uRunningFlags)
		{}

		ClonedTakeOffPedVariationInfo()
		:
			m_clipSetId(CLIP_SET_ID_INVALID),
			m_clipIdForPed(CLIP_ID_INVALID),
			m_clipIdForProp(CLIP_ID_INVALID),
			m_nVariationComponent(PV_COMP_INVALID),
			m_uVariationDrawableId(0),
			m_uVariationDrawableAltId(0),
			m_uVariationTexId(0),
			m_hProp(""),
			m_nAttachBone(BONETAG_INVALID),
			m_fBlendInDeltaForPed(0.0f),
			m_fBlendInDeltaForProp(0.0f),
			m_fPhaseToBlendOut(0.0f),
			m_fBlendOutDelta(0.0f),
			m_fForceToApplyAfterInterrupt(0.0f),
			m_uRunningFlags(0)
		{
			m_vVelocityInheritance.ZeroComponents();
			m_vAttachOffset.ZeroComponents();
			m_qAttachOrientation.ZeroComponents();
		}

		~ClonedTakeOffPedVariationInfo()	
		{}

		virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_TAKE_OFF_PED_VARIATION;}
		virtual CTaskFSMClone *CreateCloneFSMTask();

		void Serialise(CSyncDataBase& serialiser)
		{
			static const u8 SIZEOF_HASH						= 32;
			static const u8 SIZEOF_VARIATION_DRAWABLE_ID	= 8;
			static const u8 SIZEOF_VARIATION_DRAWABLE_ALT_ID= 8;
			static const u8 SIZEOF_VARIATION_TEX_ID			= 8;
			static const u8 SIZEOF_BONE_TAG					= 16;
			static const u8 SIZEOF_BLEND_IN_DELTA			= 8;
			static const u8 SIZEOF_FORCE_TO_APPLY			= 8;
			static const u8 SIZEOF_OFFSET					= 8;
			static const u8 SIZEOF_OFFSET_QUAT				= 32;
			static const u8 SIZEOF_VELOCITY					= 8;
			static const u8 SIZEOF_RUNNING_FLAGS			= CTaskTakeOffPedVariation::RF_NumFlags;
			static const u8 SIZEOF_PHASE_TO_BLEND_OUT = 8;

			CSerialisedFSMTaskInfo::Serialise(serialiser);

			SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "Clip Set ID");

			u32 hash = m_clipIdForPed.GetHash();
			SERIALISE_UNSIGNED(serialiser, hash,		SIZEOF_HASH,													"Clip ID For Ped");
			m_clipIdForPed.SetHash(hash);

			hash = m_clipIdForProp.GetHash();
			SERIALISE_UNSIGNED(serialiser, hash,		SIZEOF_HASH,													"Clip ID For Prop");
			m_clipIdForProp.SetHash(hash);

			s32 varComp = (s32)m_nVariationComponent;
			SERIALISE_INTEGER(serialiser, varComp,		datBitsNeeded<PV_MAX_COMP-1>::COUNT + 1,	"Variation Component");
			m_nVariationComponent = (ePedVarComp)varComp;
			
			SERIALISE_UNSIGNED(serialiser, m_uVariationDrawableId,	SIZEOF_VARIATION_DRAWABLE_ID,					"Variation Drawable Id");

			SERIALISE_UNSIGNED(serialiser, m_uVariationDrawableAltId,	SIZEOF_VARIATION_DRAWABLE_ALT_ID,				"Variation Drawable Alt Id");

			SERIALISE_UNSIGNED(serialiser, m_uVariationTexId,	SIZEOF_VARIATION_TEX_ID,				"Variation Tex Id");

			SERIALISE_UNSIGNED(serialiser, m_uRunningFlags,			SIZEOF_RUNNING_FLAGS,							"Running Flags");

			hash = m_hProp.GetHash();
			SERIALISE_UNSIGNED(serialiser, hash,						SIZEOF_HASH,									"Prop Hash");
			m_hProp.SetHash(hash);

			s32 boneTag = (s32)m_nAttachBone;
			SERIALISE_INTEGER(serialiser, boneTag,					SIZEOF_BONE_TAG,								"Attach Bone Tag");
			m_nAttachBone = eAnimBoneTag(boneTag);

			SERIALISE_PACKED_FLOAT(serialiser, m_fBlendInDeltaForPed,	FAST_BLEND_IN_DELTA, SIZEOF_BLEND_IN_DELTA,		"Blend In Delta For Ped");
			SERIALISE_PACKED_FLOAT(serialiser, m_fBlendInDeltaForProp, FAST_BLEND_IN_DELTA, SIZEOF_BLEND_IN_DELTA,		"Blend In Delta For Prop");
			SERIALISE_PACKED_FLOAT(serialiser, m_fBlendOutDelta,		FAST_BLEND_IN_DELTA, SIZEOF_BLEND_IN_DELTA,		"Blend Out Delta");
			
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fForceToApplyAfterInterrupt,	300.0f, SIZEOF_FORCE_TO_APPLY,	"Force To Apply");

			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fPhaseToBlendOut,		1.0f, SIZEOF_PHASE_TO_BLEND_OUT,		"Phase To Blend Out");

			float x = m_vAttachOffset.GetXf();
			float y = m_vAttachOffset.GetYf();
			float z = m_vAttachOffset.GetZf();
			SERIALISE_PACKED_FLOAT(serialiser, x,		1.0f, SIZEOF_OFFSET,		"Attach Offset X");
			SERIALISE_PACKED_FLOAT(serialiser, y,		1.0f, SIZEOF_OFFSET,		"Attach Offset Y");
			SERIALISE_PACKED_FLOAT(serialiser, z,		1.0f, SIZEOF_OFFSET,		"Attach Offset Z");
			m_vAttachOffset.SetXf(x);
			m_vAttachOffset.SetYf(y);
			m_vAttachOffset.SetZf(z);

			x = m_vVelocityInheritance.GetXf();
			y = m_vVelocityInheritance.GetYf();
			z = m_vVelocityInheritance.GetZf();
			SERIALISE_PACKED_FLOAT(serialiser, x,		5.0f, SIZEOF_VELOCITY,		"Velocity X");
			SERIALISE_PACKED_FLOAT(serialiser, y,		5.0f, SIZEOF_VELOCITY,		"Velocity Y");
			SERIALISE_PACKED_FLOAT(serialiser, z,		5.0f, SIZEOF_VELOCITY,		"Velocity Z");
			m_vVelocityInheritance.SetXf(x);
			m_vVelocityInheritance.SetYf(y);
			m_vVelocityInheritance.SetZf(z);

			Quaternion temp = QUATV_TO_QUATERNION(m_qAttachOrientation); 
			SERIALISE_QUATERNION(serialiser, temp, SIZEOF_OFFSET_QUAT, "Attach Offset Rotation");
			m_qAttachOrientation = QUATERNION_TO_QUATV(temp);
		}

		inline fwMvClipSetId			GetClipSetId(void) const				{	return m_clipSetId;					}
		inline fwMvClipId				GetClipIdForPed(void) const				{	return m_clipIdForPed;				}
		inline fwMvClipId				GetClipIdForProp(void) const			{	return m_clipIdForProp;				}
		inline ePedVarComp				GetVariationComponent(void) const		{	return m_nVariationComponent;		}
		inline u8						GetVariationDrawableId(void) const		{	return m_uVariationDrawableId;		}	
		inline u8						GetVariationDrawableAltId(void) const	{	return m_uVariationDrawableAltId;	}	
		inline u8						GetVariationTexId(void) const			{	return m_uVariationTexId;			}
		inline atHashWithStringNotFinal	GetPropHash(void) const					{	return m_hProp;						}
		inline eAnimBoneTag				GetAttachBone(void) const				{	return m_nAttachBone;				}
		inline Vec3V_Out				GetAttachOffset(void) const				{	return m_vAttachOffset;				}
		inline Vec3V_Out				GetVelocityInheritance(void) const		{	return m_vVelocityInheritance;		}
		inline QuatV_Out				GetAttachOrientation(void) const		{	return m_qAttachOrientation;		}
		inline float					GetBlendInDeltaForPed(void) const		{	return m_fBlendInDeltaForPed;		}
		inline float					GetBlendInDeltaForProp(void) const		{	return m_fBlendInDeltaForProp;		}
		inline float					GetBlendOutDelta(void) const			{	return m_fBlendOutDelta;			}
		inline float					GetForceToApply(void) const				{	return m_fForceToApplyAfterInterrupt;}
		inline u8						GetRunningFlags(void) const				{	return m_uRunningFlags;				}
		inline float					GetPhaseToBlendOut(void) const			{	return m_fPhaseToBlendOut;	}			

	private:

		Vec3V						m_vAttachOffset;
		Vec3V						m_vVelocityInheritance;
		QuatV						m_qAttachOrientation;
		fwMvClipSetId				m_clipSetId;
		fwMvClipId					m_clipIdForPed;
		fwMvClipId					m_clipIdForProp;
		ePedVarComp					m_nVariationComponent;
		atHashWithStringNotFinal	m_hProp;
		eAnimBoneTag				m_nAttachBone;
		float						m_fBlendInDeltaForPed;
		float						m_fBlendInDeltaForProp;
		ATTR_UNUSED float			m_fPhaseToBlendOut;
		float						m_fBlendOutDelta;
		float						m_fForceToApplyAfterInterrupt;
		u8							m_uVariationDrawableId;
		u8							m_uVariationDrawableAltId;
		u8							m_uVariationTexId;
		u8							m_uRunningFlags;

		static fwMvClipId m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
}; 

#endif //TASK_TAKE_OFF_PED_VARIATION_H
