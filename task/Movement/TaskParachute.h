#ifndef __TASK_PARACHUTE_H__
#define __TASK_PARACHUTE_H__
/********************************************************************
	filename:	TaskParachute.h
	created:	2008/09/24
	author:		jack.potter

	purpose:	Tasks related to skydiving and parachuting
*********************************************************************/

// Game headers
#include "peds/ped.h"
#include "Peds/QueriableInterface.h"
#include "physics/PhysicsHelpers.h"
#include "Scene/RegdRefTypes.h"
#include "streaming/streamingrequest.h"
#include "Task/Physics/TaskNMPose.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"
#include "network/Debug/NetworkDebug.h"

//-----------------------------------------------------------------------------------------
RAGE_DECLARE_CHANNEL(parachute)

#define parachuteAssert(cond)						RAGE_ASSERT(parachute,cond)
#define parachuteAssertf(cond,fmt,...)				RAGE_ASSERTF(parachute,cond,fmt,##__VA_ARGS__)
#define parachuteFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(parachute,cond,fmt,##__VA_ARGS__)
#define parachuteVerifyf(cond,fmt,...)				RAGE_VERIFYF(parachute,cond,fmt,##__VA_ARGS__)
#define parachuteErrorf(fmt,...)					RAGE_ERRORF(parachute,fmt,##__VA_ARGS__)
#define parachuteWarningf(fmt,...)					RAGE_WARNINGF(parachute,fmt,##__VA_ARGS__)
#define parachuteDisplayf(fmt,...)					RAGE_DISPLAYF(parachute,fmt,##__VA_ARGS__)
#define parachuteDebugf1(fmt,...)					RAGE_DEBUGF1(parachute,fmt,##__VA_ARGS__)
#define parachuteDebugf2(fmt,...)					RAGE_DEBUGF2(parachute,fmt,##__VA_ARGS__)
#define parachuteDebugf3(fmt,...)					RAGE_DEBUGF3(parachute,fmt,##__VA_ARGS__)
#define parachuteLogf(severity,fmt,...)			RAGE_LOGF(parachute,severity,fmt,##__VA_ARGS__)
#define parachuteCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,parachute,severity,fmt,##__VA_ARGS__)

//-----------------------------------------------------------------------------------------

// Forward declarations
class CTaskParachuteObject;

//////////////////////////////////////////////////////////////////////////
// Class:	CTaskParachute
// Purpose:	Task to handle skydiving, parachuting and landing
//			using an animated solution
class CTaskParachute : public CTaskFSMClone
{

private:

	enum ProbeFlags
	{
		PrF_HasCheckedInitialIntersections	= BIT0,
		PrF_IsClearOfPeds					= BIT1,
		PrF_IsClearOfParachutes				= BIT2,
		PrF_IsClearOfLastVehicle			= BIT3,
		PrF_HasInvalidIntersection			= BIT4,
	};

	enum RunningFlags
	{
		RF_IsDependentOnParachute = BIT0,
	};

public:
	
	struct PedVariation
	{
		PedVariation()
		: m_Component(PV_COMP_INVALID)
		, m_DrawableId(0)
		, m_DrawableAltId(0)
		, m_TexId(0)
		{}
		
		ePedVarComp		m_Component;
		u32				m_DrawableId;
		u32				m_DrawableAltId;
		u32				m_TexId;
		
		PAR_SIMPLE_PARSABLE;
	};

	struct PedVariationSet
	{
		PedVariationSet()
		: m_Component(PV_COMP_INVALID)
		{}

		ePedVarComp		m_Component;
		atArray<u32>	m_DrawableIds;

		PAR_SIMPLE_PARSABLE;
	};
	
	struct ParachutePackVariation
	{
		ParachutePackVariation()
		{}

		~ParachutePackVariation()
		{}
		
		atArray<PedVariationSet>	m_Wearing;
		PedVariation				m_ParachutePack;

		PAR_SIMPLE_PARSABLE;
	};

	struct ParachutePackVariations
	{
		ParachutePackVariations()
		{}

		~ParachutePackVariations()
		{}
		
		atHashWithStringNotFinal		m_ModelName;
		atArray<ParachutePackVariation> m_Variations;

		PAR_SIMPLE_PARSABLE;
	};

	struct Tunables : CTuning
	{
		struct ChangeRatesForSkydiving
		{
			ChangeRatesForSkydiving()
			{}
			
			float	m_Pitch;
			float	m_Roll;
			float	m_Yaw;
			float	m_Heading;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct ChangeRatesForParachuting
		{
			ChangeRatesForParachuting()
			{}
			
			float	m_Pitch;
			float	m_Roll;
			float	m_Yaw;
			float	m_Brake;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct FlightAngleLimitsForSkydiving
		{
			FlightAngleLimitsForSkydiving()
			{}
			
			float	m_MinPitch;
			float	m_InflectionPitch;
			float	m_MaxPitch;
			float	m_MaxRoll;
			float	m_MaxYaw;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct FlightAngleLimitsForParachuting
		{
			FlightAngleLimitsForParachuting()
			{}
			
			float	m_MinPitch;
			float	m_MaxPitch;
			float	m_MaxRollFromStick;
			float	m_MaxRollFromBrake;
			float	m_MaxRoll;
			float	m_MaxYawFromStick;
			float	m_MaxYawFromRoll;
			float	m_RollForMinYaw;
			float	m_RollForMaxYaw;
			float	m_MaxYaw;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct PedAngleLimitsForSkydiving
		{
			PedAngleLimitsForSkydiving()
			{}
			
			float	m_MinPitch;
			float	m_MaxPitch;
			
			PAR_SIMPLE_PARSABLE;
		};

		struct MoveParameters
		{
			struct Parachuting
			{
				struct InterpRates
				{
					InterpRates()
					{}

					float m_StickX;
					float m_StickY;
					float m_TotalStickInput;
					float m_CurrentHeading;

					PAR_SIMPLE_PARSABLE;
				};

				Parachuting()
				{}

				InterpRates m_InterpRates;

				float m_MinParachutePitch;
				float m_MaxParachutePitch;
				float m_MaxParachuteRoll;

				PAR_SIMPLE_PARSABLE;
			};

			MoveParameters()
			{}

			Parachuting m_Parachuting;

			PAR_SIMPLE_PARSABLE;
		};

		struct ForcesForSkydiving
		{
			ForcesForSkydiving()
			{}

			float	m_MaxThrust;
			float	m_MaxLift;

			PAR_SIMPLE_PARSABLE;
		};
		
		struct ParachutingAi
		{
			struct Target
			{
				Target()
				{}
				
				float	m_MinDistanceToAdjust;
				float	m_Adjustment;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Brake
			{
				Brake()
				{}
				
				float	m_MaxDistance;
				float	m_DistanceToStart;
				float	m_DistanceForFull;
				float	m_AngleForMin;
				float	m_AngleForMax;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Roll
			{
				Roll()
				{}
				
				float	m_AngleDifferenceForMin;
				float	m_AngleDifferenceForMax;
				float	m_StickValueForMin;
				float	m_StickValueForMax;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Pitch
			{
				Pitch()
				{}
				
				float	m_DesiredTimeToResolveAngleDifference;
				float	m_DeltaForMaxStickChange;
				float	m_MaxStickChangePerSecond;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Drop
			{
				Drop()
				{}
				
				float	m_MinDistance;
				float	m_MaxDistance;
				float	m_MinHeight;
				float	m_MaxHeight;
				float	m_MaxDot;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			ParachutingAi()
			{}
			
			Target	m_Target;
			Brake	m_Brake;
			Roll	m_RollForNormal;
			Roll	m_RollForBraking;
			Pitch	m_PitchForNormal;
			Pitch	m_PitchForBraking;
			Drop	m_Drop;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Landing
		{
			struct NormalThreshold
			{
				NormalThreshold()
				{}

				float m_Forward;
				float m_Collision;

				PAR_SIMPLE_PARSABLE;
			};

			struct NormalThresholds
			{
				NormalThresholds()
				{}

				NormalThreshold m_Normal;
				NormalThreshold m_Braking;

				PAR_SIMPLE_PARSABLE;
			};

			Landing()
			{}

			NormalThresholds m_NormalThresholds;
			
			float	m_MaxVelocityForSlow;
			float	m_MinVelocityForFast;
			float	m_ParachuteProbeRadius;
			float	m_MinStickMagnitudeForEarlyOutMovement;
			float	m_FramesToLookAheadForProbe;
			float	m_BlendDurationForEarlyOut;
			float	m_AngleForRunway;
			float	m_LookAheadForRunway;
			float	m_DropForRunway;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct CrashLanding
		{
			CrashLanding()
			{}
			
			float	m_NoParachuteTimeForMinCollisionNormalThreshold;
			float	m_NoParachuteMaxCollisionNormalThreshold;
			float	m_NoParachuteMinCollisionNormalThreshold;
			float	m_NoParachuteMaxPitch;
			float	m_ParachuteProbeRadius;
			float	m_ParachuteUpThreshold;
			float	m_FramesToLookAheadForProbe;

			PAR_SIMPLE_PARSABLE;
		};
		
		struct Allow
		{
			Allow()
			{}
			
			float	m_MinClearDistanceBelow;
			float	m_MinFallingSpeedInRagdoll;
			float	m_MinTimeInRagdoll;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct CameraSettings
		{
			CameraSettings()
			{}

			atHashString	m_SkyDivingCamera;
			atHashString	m_ParachuteCamera;
			atHashString	m_ParachuteCloseUpCamera;

			PAR_SIMPLE_PARSABLE;
		};
		
		struct ParachutePhysics
		{
			ParachutePhysics()
			{}
			
			float	m_ParachuteInitialVelocityY;
			float	m_ParachuteInitialVelocityZ;
			
			PAR_SIMPLE_PARSABLE;
		};

		struct ExtraForces
		{
			struct Value
			{
				Value()
				{}

				Vector3 ToVector3() const
				{
					return Vector3(m_X, m_Y, m_Z);
				}

				float m_X;
				float m_Y;
				float m_Z;

				PAR_SIMPLE_PARSABLE;
			};

			struct FromValue
			{
				FromValue()
				{}

				float	m_ValueForMin;
				float	m_ValueForMax;
				Value	m_MinValue;
				Value	m_ZeroValue;
				Value	m_MaxValue;
				bool	m_IsLocal;
				bool	m_ClearZ;

				PAR_SIMPLE_PARSABLE;
			};

			struct FromStick
			{
				FromStick()
				{}

				FromValue	m_FromValue;
				bool		m_UseVerticalAxis;

				PAR_SIMPLE_PARSABLE;
			};

			struct Parachuting
			{
				struct Normal
				{
					Normal()
					{}

					FromStick m_TurnFromStick;
					FromValue m_TurnFromRoll;

					PAR_SIMPLE_PARSABLE;
				};

				struct Braking
				{
					Braking()
					{}

					FromStick m_TurnFromStick;

					PAR_SIMPLE_PARSABLE;
				};

				Parachuting()
				{}

				Normal	m_Normal;
				Braking m_Braking;

				PAR_SIMPLE_PARSABLE;
			};

			ExtraForces()
			{}

			Parachuting m_Parachuting;

			PAR_SIMPLE_PARSABLE;
		};

		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_RunwayProbes;
			bool m_ValidityProbes;

			PAR_SIMPLE_PARSABLE;
		};

		struct LowLod
		{
			LowLod()
			{}

			float m_MinDistance;

			bool m_AlwaysUse;
			bool m_NeverUse;

			PAR_SIMPLE_PARSABLE;
		};

		struct ParachuteBones
		{
			struct Attachment
			{
				Attachment()
				{}

				float	m_X;
				float	m_Y;
				float	m_Z;
				bool	m_UseOrientationFromParachuteBone;

				PAR_SIMPLE_PARSABLE;
			};

			ParachuteBones()
			{}

			Attachment m_LeftGrip;
			Attachment m_RightGrip;
			Attachment m_LeftWire;
			Attachment m_RightWire;

			PAR_SIMPLE_PARSABLE;
		};

		struct Aiming
		{
			Aiming()
			{}

			bool m_Disabled;

			PAR_SIMPLE_PARSABLE;
		};

		struct PadShake
		{
			PadShake()
			{}

			struct Falling
			{
				Falling()
				{}

				u32		m_Duration;
				float	m_PitchForMinIntensity;
				float	m_PitchForMaxIntensity;
				float	m_MinIntensity;
				float	m_MaxIntensity;

				PAR_SIMPLE_PARSABLE;
			};

			struct Deploy
			{
				Deploy()
				{}

				u32		m_Duration;
				float	m_Intensity;

				PAR_SIMPLE_PARSABLE;
			};

			Falling	m_Falling;
			Deploy	m_Deploy;

			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		ChangeRatesForSkydiving			m_ChangeRatesForSkydiving;
		ChangeRatesForParachuting		m_ChangeRatesForParachuting;
		FlightAngleLimitsForSkydiving	m_FlightAngleLimitsForSkydiving;
		FlightAngleLimitsForParachuting	m_FlightAngleLimitsForParachutingNormal;
		FlightAngleLimitsForParachuting	m_FlightAngleLimitsForParachutingBraking;
		PedAngleLimitsForSkydiving		m_PedAngleLimitsForSkydiving;
		MoveParameters					m_MoveParameters;
		ForcesForSkydiving				m_ForcesForSkydiving;
		ParachutingAi					m_ParachutingAi;
		Landing							m_Landing;
		CrashLanding					m_CrashLanding;
		Allow							m_Allow;
		CameraSettings					m_CameraSettings;
		ParachutePhysics				m_ParachutePhysics;
		ExtraForces						m_ExtraForces;
		Rendering						m_Rendering;
		LowLod							m_LowLod;
		ParachuteBones					m_ParachuteBones;
		Aiming							m_Aiming;
		PadShake						m_PadShake;
		
		float	m_BrakingDifferenceForLinearVZMin;
		float	m_BrakingDifferenceForLinearVZMax;
		float	m_LinearVZForBrakingDifferenceMin;
		float	m_LinearVZForBrakingDifferenceMax;
		float	m_PitchRatioForLinearVZMin;
		float	m_PitchRatioForLinearVZMax;
		float	m_LinearVZForPitchRatioMin;
		float	m_LinearVZForPitchRatioMax;
		float	m_MinBrakeForCloseUpCamera;
		float	m_ParachuteMass;
		float	m_ParachuteMassReduced;
		float	m_MaxTimeToLookAheadForFutureTargetPosition;
		float	m_MaxDifferenceToAverageBrakes;

		atHashWithStringNotFinal	m_ModelForParachuteInSP;
		atHashWithStringNotFinal	m_ModelForParachuteInMP;
		
		atArray<ParachutePackVariations> m_ParachutePackVariations;

		Vector3		m_FirstPersonDriveByIKOffset;

		PAR_PARSABLE;
	};
	
	struct LandingData
	{
		enum Type
		{
			Invalid = -1,
			Slow,
			Regular,
			Fast,
			Crash,
			Water,
			Max
		};
		
		struct CrashData
		{
			CrashData()
			: m_vCollisionNormal(V_ZERO)
			, m_bWithParachute(false)
			, m_bPedCollision(false)
			, m_bInWater(false)
			{
			
			}
			
			Vec3V	m_vCollisionNormal;
			bool	m_bWithParachute;
			bool	m_bPedCollision;
			bool	m_bInWater;
		};
		
		LandingData()
		: m_CrashData()
		, m_nType(Invalid)
		, m_pLandedOn(NULL)
		, m_uParachuteLandingFlags(0)
		{
		
		}
		
		CrashData		m_CrashData;
		Type			m_nType;
		RegdConstEnt	m_pLandedOn;
		fwFlags8		m_uParachuteLandingFlags;
	};

private:

	struct CalculateDesiredPitchFromStickInput
	{
		CalculateDesiredPitchFromStickInput(float fMinPitch, float fMaxPitch)
		: m_fMinPitch(fMinPitch)
		, m_fMaxPitch(fMaxPitch)
		, m_bUseInflectionPitch(false)
		, m_fInflectionPitch(0.0f)
		{}

		float	m_fMinPitch;
		float	m_fMaxPitch;
		bool	m_bUseInflectionPitch;
		float	m_fInflectionPitch;
	};

public:

	static const u32 PARACHUTE_AND_PED_LOD_DISTANCE = 300;

	// Parachute Flags
	enum
	{
		// Config Flags
		PF_HasTeleported	= BIT0,
		PF_GiveParachute	= BIT1,
		PF_SkipSkydiving	= BIT2,
		PF_UseLongBlend		= BIT3,
		PF_InstantBlend		= BIT4,
		
		// Running Flags
		PF_HasTarget	= BIT5,
		
		PF_MAX_Flags
	};

	enum State
	{
		State_Start,
		State_WaitingForBaseToStream,
		State_BlendFromNM,
		State_WaitingForMoveNetworkToStream,
		State_TransitionToSkydive,
		State_WaitingForSkydiveToStream,
		State_Skydiving, 
		State_Deploying,
		State_Parachuting,
		State_Landing, 
		State_CrashLand,
		State_Quit
	};

	enum SkydiveTransition
	{
		ST_None,
		ST_FallGlide,
		ST_JumpInAirL,
		ST_JumpInAirR,
		ST_Max
	};
	
	enum eParachuteBoneId
	{
		P_Para_S_L_Grip	= 56993,
		P_Para_S_R_Grip	= 47861,

		P_Para_S_top_wing_LR5 = 46681,
		P_Para_S_top_wing_RR5 = 61529,

		P_Para_S_LU1_Wire	= 416,
		P_Para_S_LU2_Wire	= 6320,
		P_Para_S_RU1_Wire	= 2560,
		P_Para_S_RU2_Wire	= 8464,

		P_Para_S_LF_pack2 = 58476,
		P_Para_S_RF_pack2 = 56524,
	};
	
	enum ParachuteLandingFlags
	{
		PLF_Crumple					= BIT0,
		PLF_ClearHorizontalVelocity	= BIT1,
		PLF_ReduceMass				= BIT2,
	};
	
public:

	CTaskParachute(s32 iFlags = 0);
	virtual ~CTaskParachute();

public:

	void TaskSetState(const s32 iState);

	LandingData::Type GetLandingType() const { return m_LandingData.m_nType; }
	
public:

	bool CalcDesiredVelocity(const Matrix34& mUpdatedPed, float fTimeStep, Vector3& vDesiredVelocity) const;
	void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) const;
	bool IsCrashLanding() const;
	bool IsLanding() const;
	void ProcessPedImpact(const CEntity* pEntity, Vec3V_In vCollisionNormal);
	bool ShouldStickToFloor() const;
	bool ShouldUseCloseUpCamera() const;
	
public:

	static void		ClearParachutePackVariationForPed(CPed& rPed);
	static CObject*	GetParachuteForPed(const CPed& rPed);
	static bool		GetParachutePackVariationForPed(const CPed& rPed, ePedVarComp& nComponent, u8& uDrawableId, u8& uDrawableAltId, u8& uTexId);
	static u32		GetTintIndexFromParachutePackVariationForPed(const CPed& rPed);
	static bool		IsParachutePackVariationActiveForPed(const CPed& rPed);
	static bool		IsPedDependentOnParachute(const CPed& rPed);
	static void		ReleaseStreamingOfParachutePackVariationForPed(CPed& rPed);
	static void		RequestStreamingOfParachutePackVariationForPed(CPed& rPed);
	static void		SetParachutePackVariationForPed(CPed& rPed);
	static CObject* CreateParachuteBagObject(CPed *pPed, bool bAttach, bool bClearVariation);

private:
	 
	void AllowLodChangeThisFrame()
	{
		m_bIsLodChangeAllowedThisFrame = true;
	}

public:

	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	virtual aiTask*	Copy() const;
	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_PARACHUTE; }	

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	virtual bool HandlesRagdoll(const CPed*) const { return true; }

	virtual bool UseCustomGetup() { return GetState() == State_BlendFromNM; }
	virtual bool AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	bool GetHasLandedProperly() const { return m_bLandedProperly; }

	void RemoveParachuteForScript() { m_bRemoveParachuteForScript = true; }

	void ForcePedToOpenChute() { m_bForcePedToOpenChute = true; }
	
	float GetParachuteThrust() const { return m_fParachuteThrust; }
	void SetParachuteThrust(float fThrust) { m_fParachuteThrust = fThrust; }

	inline fwFlags32 &GetParachuteFlags() { return m_iFlags; }
	inline const fwFlags32 &GetParachuteFlags() const { return m_iFlags; }
	
	const SkydiveTransition GetSkydiveTransition() const { return m_nSkydiveTransition; }
	void SetSkydiveTransition(const SkydiveTransition nSkydiveTransition) { m_nSkydiveTransition = nSkydiveTransition; }

	CObject* GetParachute() const { return m_pParachuteObject; }
	
	void SetTarget(const CPhysical* pEntity, Vec3V_In vPosition);
	void SetTargetEntity(const CPhysical* pEntity);
	void SetTargetPosition(Vec3V_In vPosition);

	bool IsParachuteOut() const;

public:

   // Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool				OverridesNetworkBlender(CPed*) { return GetState() >= State_Parachuting; }
	virtual bool				OverridesNetworkHeadingBlender(CPed*) { return true; }
	virtual bool                OverridesNetworkAttachment(CPed* pPed);
	virtual bool				OverridesVehicleState() const	{ return true; }
    virtual bool				UseInAirBlender() const	{ return true; }
    virtual bool                HasSyncedVelocity() const { return true; }
    virtual Vector3             GetSyncedVelocity() const { return m_vSyncedVelocity; }
    virtual bool 				IsInScope(const CPed* pPed);
	
	bool						IsStateChangeHandledByNetwork(s32 iState, s32 iStateFromNetwork) const;
	
private:
	
	void GetSmokeTrailDataForClone(bool& bCanLeaveSmokeTrail, Color32& smokeTrailColor) const;
	void SetSmokeTrailDataOnClone(const bool bCanLeaveSmokeTrail, const Color32 smokeTrailColor);

	void IncrementStatSmokeTrailDeployed(const u32 color);
	
public:

	static bool IsPedInStateToParachute(const CPed& rPed);
	static bool CanPedParachute(const CPed& rPed, float fAngle = 0.0f);
	static bool CanPedParachuteFromPosition(const CPed& rPed, Vec3V_In vPosition, bool bEnsurePositionCanBeReached, Vec3V_Ptr pOverridePedPosition = NULL, float fAngle = 0.0f);
	static bool DoesPedHaveParachute(const CPed& rPed);
	static bool GivePedParachute(CPed* pPed);

	static const PedVariation* FindParachutePackVariationForPed(const CPed& rPed);
	static u32 GetTintIndexForParachute(const CPed& rPed);
	static u32 GetTintIndexForParachutePack(const CPed& rPed);

	void SetPedChangingOwnership(bool val) { m_pedChangingOwnership = val; }
	bool IsPedChangingOwnership() { return m_pedChangingOwnership; }

	void CacheParachuteState(s32 oldState, ObjectId oldParachuteObjectId) { m_cachedParachuteState = oldState; m_cachedParachuteObjectId = oldParachuteObjectId; }

	ObjectId GetParachuteObjectId() { return m_parachuteObjectId; }

protected:

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual FSM_Return	ProcessPostFSM();
	virtual bool		ProcessPostPreRender();
	virtual bool		ProcessPostCamera();
	virtual s32	GetDefaultStateAfterAbort() const { return State_Quit; }

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	void ProcessMovement();
	
	//Pitch the capsule based on the state
	void PitchCapsuleBound(); 

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();

	FSM_Return	WaitingForBaseToStream_OnUpdate();

	void		BlendFromNM_OnEnter();
	FSM_Return	BlendFromNM_OnUpdate();

	FSM_Return	WaitingForMoveNetworkToStream_OnUpdate();
	
	void		TransitionToSkydive_OnEnter();
	FSM_Return	TransitionToSkydive_OnUpdate();

	void		WaitingForSkydiveToStream_OnEnter();
	FSM_Return	WaitingForSkydiveToStream_OnUpdate();

	FSM_Return SkyDiving_OnEnter(CPed* pPed);
	FSM_Return SkyDiving_OnUpdate(CPed* pPed);
	
	FSM_Return Deploying_OnEnter(CPed* pPed);
	FSM_Return Deploying_OnUpdate(CPed* pPed);
	
	void		Parachuting_OnEnter();
	FSM_Return	Parachuting_OnUpdate(CPed* pPed);
	void		Parachuting_OnExit();

	FSM_Return Landing_OnEnter(CPed*); 
	FSM_Return Landing_OnUpdate(CPed* pPed);
	FSM_Return Landing_OnUpdateClone(CPed* pPed);
	FSM_Return Landing_OnExit();

	FSM_Return CrashLanding_OnEnter(CPed*); 
	FSM_Return CrashLanding_OnUpdate(CPed* pPed);

	inline void SetCloneSubTaskLaunched(bool _launched)		{ m_cloneSubtaskLaunched = _launched;	}
	inline bool GetCloneSubTaskLaunched(void) const			{ return m_cloneSubtaskLaunched;		}

	void SetParachuteAndPedLodDistance(void);
	void ClearParachuteAndPedLodDistance(void);

	void SetParachuteNetworkObjectBlender(void);

	void ProcessCloneValueSmoothing(void);
	void ProcessCloneVehicleCollision(bool force = false);

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 

	// move states
	static const fwMvStateId ms_TransitionToSkydiveState;
	static const fwMvStateId ms_WaitingForSkydiveToStreamState;
	static const fwMvStateId ms_SkydivingState;
	static const fwMvStateId ms_DeployParachuteState;
	static const fwMvStateId ms_ParachutingState;
	static const fwMvStateId ms_LandingState;

	//move control params
	static const fwMvFloatId ms_NormalDirection;
	static const fwMvFloatId ms_Direction;
	static const fwMvFloatId ms_xDirection;
	static const fwMvFloatId ms_yDirection;
	static const fwMvFloatId ms_Phase;
	static const fwMvFloatId ms_LeftBrake;
	static const fwMvFloatId ms_RightBrake;
	static const fwMvFloatId ms_CombinedBrake;
	static const fwMvFloatId ms_BlendMotionWithBrake;
	static const fwMvFloatId ms_ParachutePitch;
	static const fwMvFloatId ms_ParachuteRoll;
	static const fwMvFloatId ms_FirstPersonCameraHeading;
	static const fwMvFloatId ms_FirstPersonCameraPitch;

	//move flags
	static const fwMvFlagId ms_TransitionToSkydiveFromFallGlide;
	static const fwMvFlagId ms_TransitionToSkydiveFromJumpInAirL;
	static const fwMvFlagId ms_TransitionToSkydiveFromJumpInAirR;
	static const fwMvFlagId ms_LandSlow;
	static const fwMvFlagId ms_LandRegular;
	static const fwMvFlagId ms_LandFast;
	static const fwMvFlagId ms_UseLowLod;
	static const fwMvFlagId ms_FirstPersonMode;
	
	//move requests
	static const fwMvRequestId ms_TransitionToSkydive;
	static const fwMvRequestId ms_WaitForSkydiveToStream;
	static const fwMvRequestId ms_Skydive;
	static const fwMvRequestId ms_DeployParachute; 
	static const fwMvRequestId ms_Land; 
	static const fwMvRequestId ms_Parachuting;
	static const fwMvRequestId ms_ClipSetChanged;

	//move events
	static const fwMvBooleanId ms_TransitionToSkydiveEnded;
	static const fwMvBooleanId ms_OpenChuteDone;  
	static const fwMvBooleanId ms_LandingComplete;
	static const fwMvBooleanId ms_TransitionToSkydiveOnEnter;
	static const fwMvBooleanId ms_WaitingForSkydiveToStreamOnEnter;
	static const fwMvBooleanId ms_SkyDiveOnEnter;
	static const fwMvBooleanId ms_DeployParachuteOnEnter;
	static const fwMvBooleanId ms_ParachutingOnEnter;
	static const fwMvBooleanId ms_LandingOnEnter;
	static const fwMvBooleanId ms_CanEarlyOutForMovement;

	static void InitialisePedInAir(CPed *pPed);
	void CreateParachuteInInventory();
	void RemoveParachuteFromInventory();
	void DestroyParachute();
	
	void CalculateAttachmentOffset(CObject* pObject, Vector3& vAttachmentOut);

	void CreateParachute();
	void ConfigureCloneParachute();
	
	void AttachPedToParachute();
	
	void AttachParachuteToPed();
	
	void DetachParachuteFromPed();
	
	void DetachPedFromParachute();
	
	void DetachPedAndParachute(bool alertAudio = false);
	
	void GiveParachuteControlOverPed();
	
	void CrumpleParachute();
	
	void DeployParachute();
	
	CTaskParachuteObject* GetParachuteObjectTask() const;
	
	void		AddPhysicsToParachute();
	void		ApplyDamageForCrashLanding();
	void		ApplyExtraForcesForParachuting();
	void		ApplyExtraForcesFromStick(float fScale, CPhysical* pPhysical, const Tunables::ExtraForces::FromStick& rForce);
	void		ApplyExtraForcesFromValue(float fScale, float fValue, CPhysical* pPhysical, const Tunables::ExtraForces::FromValue& rForce);
	bool		AreCameraSwitchesDisabled() const;
	void		AttachMoveNetwork();
	void		AttachMoveNetworkIfNecessary();
	void		AverageBrakes(float& fLeftBrake, float& fRightBrake) const;
	float		CalculateBlendForIdleAndMotion() const;
	float		CalculateBlendForMotionAndBrake() const;
	float		CalculateBlendForXAxisTurns() const;
	float		CalculateBlendForYAxisTurns() const;
	float		CalculateBlendForXAndYAxesTurns() const;
	float		CalculateBrakeForParachutingAi(float fFlatDistanceFromPedToTargetSq, float fAngleToTargetZ) const;
	void		CalculateDesiredFlightAnglesForParachuting(float fBrake, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const;
	void		CalculateDesiredFlightAnglesForParachutingWithLimits(const Tunables::FlightAngleLimitsForParachuting& rLimits, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const;
	void		CalculateDesiredFlightAnglesForSkydiving(float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const;
	void		CalculateDesiredFlightAnglesForSkydivingWithLimits(const Tunables::FlightAngleLimitsForSkydiving& rLimits, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const;
	float		CalculateDesiredPitchFromStick(const CalculateDesiredPitchFromStickInput& rInput) const;
	float		CalculateDesiredRollFromBrake(float fMaxRoll) const;
	float		CalculateDesiredRollFromStick(float fMaxRoll) const;
	float		CalculateDesiredYawFromRoll(float fMaxYaw, float fRollForMinYaw, float fRollForMaxYaw) const;
	float		CalculateDesiredYawFromStick(float fMaxYaw) const;
	float		CalculateLiftFromPitchForSkydiving() const;
	Vec3V_Out	CalculateLinearV(float fBrake) const;
	Vec3V_Out	CalculateLinearVBraking() const;
	Vec3V_Out	CalculateLinearVNormal() const;
	float		CalculateParachutePitchForMove() const;
	float		CalculateParachuteRollForMove() const;
	float		CalculateStickForPitchForParachutingAi(float fBrake, float fFlatDistanceFromPedToTargetSq, float fAngleDifference, float fAngleDifferenceLastFrame) const;
	float		CalculateStickForPitchForParachutingAiWithParameters(const Tunables::ParachutingAi::Pitch& rParameters, float fFlatDistanceFromPedToTargetSq, float fAngleDifference, float fAngleDifferenceLastFrame) const;
	float		CalculateStickForRollForParachutingAi(float fBrake, float fFlatDistanceFromPedToTargetSq, float fAngleDifference) const;
	float		CalculateStickForRollForParachutingAiWithParameters(const Tunables::ParachutingAi::Roll& rParameters, float fFlatDistanceFromPedToTargetSq, float fAngleDifference) const;
	Vec3V_Out	CalculateTargetPosition() const;
	Vec3V_Out	CalculateTargetPositionFuture() const;
	float		CalculateThrustFromPitchForSkydiving() const;
	bool		CanAim() const;
	bool		CanDetachParachute() const;
	bool		CanUseFirstPersonCamera() const;
	bool		CanUseLowLod(bool bUseLowLod) const;
	int			ChooseStateForSkydiving() const;
	void		ClearParachutePackVariation();
	CTask*		CreateTaskForAmbientClips() const;
	CTask*		CreateTaskForAiming() const;
	CTask*		CreateTaskForCrashLanding();
	CTask*		CreateTaskForWaterLanding();
	CTask*		CreateTaskForCrashLandingWithNoParachute();
	CTask*		CreateTaskForCrashLandingWithParachute(const Vector3& vCollisionNormal);
	void		DependOnParachute(bool bDepend);
	u32			GetTintIndexFromParachutePackVariation() const;
	void		HandleParachuteLanding(fwFlags8 uParachuteLandingFlags);
	bool		HasClearRunwayForLanding() const;
	bool		IsAiming() const;
	bool		IsDependentOnParachute() const;
	bool		IsParachuteInWater() const;
	bool		IsParachutePackVariationActive() const;
	bool		IsPedAttachedToParachute() const;
	bool		IsPedInWater() const;
	bool		IsPositionInWater(Vec3V_In vPosition) const;
	bool		IsUsingFirstPersonCamera() const;
	void		KillMovePlayer();
	void		ProbeForParachuteIntersections();
	void		ProcessAverageVelocity();
	void		ProcessCamera();
	void		ProcessCameraSwitch();
	void		ProcessDeployingPhysics();
	void		ProcessFlags();
	void		ProcessGiveParachute();
	void		ProcessHasTeleported();
	void		ProcessMoveParameters();
	void		ProcessMoveParametersParachuting();
	void		ProcessMoveParametersSkydiving();
	void		ProcessPadShake();
	bool		ProcessParachute();
	void		ProcessParachuteBones();
	void		ProcessParachuteObject();
	void		ProcessCloneParachuteObjectCollision();
	void		ProcessParachutingControls();
	void		ProcessParachutingControls(const Vector2& vStick, float fLeftBrake, float fRightBrake, bool bLeaveSmokeTrail, bool bDetachParachute, bool bAiming);
	void		ProcessParachutingControlsForAi();
	void		ProcessParachutingControlsForAiming(bool bAiming);
	void		ProcessParachutingControlsForPlayer();
	void		ProcessParachutingPhysics();
	void		ProcessPed();
	void		ProcessPostLod();
	void		ProcessPreLod();
	void		ProcessSkydivingControls();
	void		ProcessSkydivingControls(const Vector2& vStick);
	void		ProcessSkydivingControlsForAi();
	void		ProcessSkydivingControlsForPlayer();
	void		ProcessSkydivingPhysics(float fTimeStep);
	void		ProcessStreaming();
	void		ProcessStreamingForClipSets();
	void		ProcessStreamingForDrivebyClipsets();
	void		ProcessStreamingForModel();
	void		ProcessStreamingForMoveNetwork();
	void		RemovePhysicsFromParachute();
	void		SetClipSetForState();
	void		SetInactiveCollisions(bool bValue);
	void		SetParachutePackVariation();
	void		SetParachuteTintIndex();
	void		SetStateForSkydiving();
	bool		ShouldBlockWeaponSwitching() const;
	bool		ShouldDisableProcessProbes() const;
	bool		ShouldEarlyOutForMovement() const;
	bool		ShouldIgnoreParachuteCollisionWithBuilding(Vec3V_In vCollisionNormal) const;
	bool		ShouldIgnoreParachuteCollisionWithEntity(const CEntity& rEntity, Vec3V_In vCollisionNormal) const;
	bool		ShouldIgnoreParachuteCollisionWithObject(const CObject& rObject) const;
	bool		ShouldIgnoreParachuteCollisionWithPed(const CPed& rPed) const;
	bool		ShouldIgnoreParachuteCollisionWithVehicle(const CVehicle& rVehicle) const;
	bool		ShouldIgnorePedCollisionWithEntity(const CEntity& rEntity, Vec3V_In vCollisionNormal) const;
	bool		ShouldIgnorePedCollisionWithObject(const CObject& rObject) const;
	bool		ShouldIgnorePedCollisionWithPed(const CPed& rPed) const;
	bool		ShouldIgnorePedCollisionWithVehicle(const CVehicle& rVehicle) const;
	bool		ShouldIgnoreProbeCollisionWithEntity(const CEntity& rEntity, Vec3V_In vCollisionPosition, float fTimeStep) const;
	bool		ShouldProcessDeployingPhysics() const;
	bool		ShouldProcessParachuteObject() const;
	bool		ShouldProcessParachutingPhysics() const;
	bool		ShouldProcessSkydivingPhysics() const;
	bool		ShouldStreamInClipSetForParachuting() const;
	bool		ShouldStreamInClipSetForSkydiving() const;
	bool		ShouldUpdateParachuteBones() const;
	bool		ShouldUseAiControls() const;
	bool		ShouldUseLeftGripIk() const;
	bool		ShouldUseLowLod() const;
	bool		ShouldUsePlayerControls() const;
	bool		ShouldUseRightGripIk() const;
	void		SmoothValue(float& fCurrentValue, float fDesiredValue, float fMaxChange) const;
	void		StartFirstPersonCameraShake(atHashWithStringNotFinal hName);
	void		StartFirstPersonCameraShakeForDeploy();
	void		StartFirstPersonCameraShakeForLanding();
	void		StartFirstPersonCameraShakeForParachuting();
	void		StartFirstPersonCameraShakeForSkydiving();
	void		StopFirstPersonCameraShaking(bool bStopImmediately = true);
	void		UseLowLod(bool bUseLowLod);
	bool		ShouldAbortTaskDueToLowLodLanding(void) const;
	
	void		ProcessFirstPersonParams();
	
#if !__FINAL
	void		ValidateNetworkBlender(bool const shouldBeOverridden) const;
#endif /* !__FINAL */

	void UpdateParachuteBones();
	
	void UpdateParachuteBone(eParachuteBoneId nParachuteBoneId, eAnimBoneTag nPedBoneId, const Tunables::ParachuteBones::Attachment& rAttachment, float fWeight, bool bSlerpOrienation = false);
	
	bool ScanForCrashLanding();
	bool ScanForCrashLandingWithNoParachute();
	bool ScanForCrashLandingWithParachute();
	
	bool ScanForLanding();
	bool ScanForLandingWithParachute();
	
	Vec3V_Out GenerateFeetPosition() const;
	Vec3V_Out GenerateHeadPosition() const;
	Vec3V_Out AdjustPositionBasedOnParachuteVelocity(Vec3V_In vPos, float fTimeStep) const;
	
	void UpdateMoVE(const Vector2 &vStick);

	void ApplyCgOffsetForces(CPed* pPed);
	
	bool HasDeployBeenRequested() const;
	
	bool CanLeaveSmokeTrail() const;
	void UpdateVfxSmokeTrail();
	void UpdateVfxPedTrails();
	void UpdateVfxCanopyTrails();
	void TriggerVfxDeploy();
	
	bool CanCreateParachute();
	bool CanConfigureCloneParachute() const;
	bool CanDeployParachute() const;
	bool CheckForDeploy();

	LandingData::Type ChooseLandingToUse() const;

	fwMvClipSetId GetClipSetForBase() const;
	fwMvClipSetId GetClipSetForParachuting() const;
	fwMvClipSetId GetClipSetForSkydiving() const;
	fwMvClipSetId GetClipSetForState() const;

#if __ASSERT
	void ValidateSceneUpdateFlags();
#endif

private:

	bool UpdateClonePedRagdollCorrection(CPed* pPed);

public:

#if DEBUG_DRAW
	void DebugCloneTaskInformation(void);
	void DebugCloneDataSmoothing(void);
#endif /* DEBUG_DRAW */

private:

	bool IsMoveNetworkHelperInTargetState() const
	{
		if(!m_networkHelper.IsInTargetState())
		{
			if(GetPed() && GetPed()->IsNetworkClone())
			{
			//	parachuteDebugf1("%s : MoveNetwork NOT in target state!", __FUNCTION__);
			}

			return false;
		}

		if(GetPed() && GetPed()->IsNetworkClone())
		{
		//	parachuteDebugf1("%s : MoveNetwork IN target state!", __FUNCTION__);
		}

		return true;
	}

#if __BANK

	void SetMoveNetworkState(const fwMvRequestId& requestId, const fwMvBooleanId& onEnterId, const fwMvStateId& stateId, bool ignoreCloneCheck = false)
	{
		parachuteDebugf1("CTaskParachute::SetMoveNetworkState : Task %p : Ped %p %s : State %s : request %s : onEnterId %s : stateId %s", this, GetPed(), GetPed()->GetDebugName(), GetStaticStateName(GetState()), requestId.GetCStr(), onEnterId.GetCStr(), stateId.GetCStr());
		/*CMoveNetworkHelperFunctions::*/SetMoveNetworkState_Internal(requestId, onEnterId, stateId, this, *GetPed(), ignoreCloneCheck);
	}
	bool IsMoveNetworkStateValid() const
	{
		return CMoveNetworkHelperFunctions::IsMoveNetworkStateValid(this, *GetPed());
	}
	bool IsMoveNetworkStateFinished(const fwMvBooleanId& animFinishedEventId, const fwMvFloatId& phaseId) const
	{
		return CMoveNetworkHelperFunctions::IsMoveNetworkStateFinished(animFinishedEventId, phaseId, this, *GetPed());
	}

	// just a temporary plain splat of CMoveNetworkHelperFunctions::SetMoveNetworkState for catching a bug....
	void SetMoveNetworkState_Internal(const fwMvRequestId& requestId, const fwMvBooleanId& onEnterId, const fwMvStateId& stateId, CTask* pTask, const CPed& BANK_ONLY(ped), bool ignoreCloneCheck = false)
	{
		taskFatalAssertf(pTask, "NULL task pointer in CTaskEnterVehicle::SetAnimState");

		CMoveNetworkHelper* pMoveNetworkHelper = pTask->GetMoveNetworkHelper();

		if (taskVerifyf(pMoveNetworkHelper, "Expected a valid move network helper, did you forget to implement GetMoveNetworkHelper()?"))
		{
			if (pTask->IsMoveTransitionAvailable(pTask->GetState()))
			{
				parachuteDebugf1("Move Transition available - send request %s", requestId.GetCStr());

				pMoveNetworkHelper->SendRequest(requestId);
				BANK_ONLY(taskDebugf2("Frame : %u - %s%s sent request %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), requestId.GetCStr(), pTask->GetStateName(pTask->GetState()), onEnterId.GetCStr()));
			}
			else
			{
				parachuteDebugf1("Move Transition available - Force state change %s", stateId.GetCStr());

				if (!ignoreCloneCheck)
				{
					taskAssertf(ped.IsNetworkClone(), "Forcing the move state for a non-network clone!");
				}
				
				pMoveNetworkHelper->ForceStateChange(stateId);
				BANK_ONLY(taskDebugf2("Frame : %u - %s%s forced to state %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), stateId.GetCStr(), pTask->GetStateName(pTask->GetState()), onEnterId.GetCStr()));
			}

			parachuteDebugf1("wait for network %s", onEnterId.GetCStr());

			pMoveNetworkHelper->WaitForTargetState(onEnterId);
		}
	}

#else 

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

#endif /* __BANK */

private:

	virtual CMoveNetworkHelper*	GetMoveNetworkHelper()
	{
		return &m_networkHelper;
	}

private:

	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;
	
private:

	static const ParachutePackVariations*	FindParachutePackVariationsForModel(u32 uModelName);
	
private:

	fwClipSetRequestHelper m_ClipSetRequestHelperForBase;
	fwClipSetRequestHelper m_ClipSetRequestHelperForSkydiving;
	fwClipSetRequestHelper m_ClipSetRequestHelperForParachuting;

	fwClipSetRequestHelper m_ClipSetRequestHelperForFirstPersonBase;
	fwClipSetRequestHelper m_ClipSetRequestHelperForFirstPersonSkydiving;
	fwClipSetRequestHelper m_ClipSetRequestHelperForFirstPersonParachuting;

	fwClipSetRequestHelper m_ClipSetRequestHelperForLowLod;

	static const s32 MAX_DRIVE_BY_CLIPSETS = 10;
	CMultipleClipSetRequestHelper<MAX_DRIVE_BY_CLIPSETS> m_MultipleClipSetRequestHelper;

	WorldProbe::CShapeTestResults	m_FallingGroundProbeResults;	// Used for asynch probing of the ground while falling
	
	LandingData m_LandingData;
	
	Vec3V m_vTargetPosition;
	
	Vector3 m_vSyncedVelocity; // The velocity the owner is travelling at - needed to get the clone at the same velocity on startup...

	Vec3V m_vAverageVelocity;

	Vector2 m_vRawStick;
	
	float m_fRawLeftBrake;
	float m_fRawRightBrake;

	float m_fStickX;
	float m_fStickY;
	float m_fTotalStickInput;
	float m_fCurrentHeading;
	
	SkydiveTransition m_nSkydiveTransition;

	RegdObj m_pParachuteObject;
	strRequest m_ModelRequestHelper;
	
	RegdConstPhy m_pTargetEntity;

	// In air control variables
	// Need to store because they are read in FSM update
	// but applied in physics update
	float m_fLeftBrake;
	float m_fRightBrake;
	float m_fPitch;
	float m_fRoll;
	float m_fYaw;
	
	float m_fParachuteThrust;

	fwFlags32 m_iFlags;
	u32 m_iParachuteModelIndex;

	u32 m_iPedLodDistanceCache;

	u32 m_uNextTimeToProbeForParachuteIntersections;

	float m_fBrakingMult;

	float m_fAngleDifferenceZLastFrame;

	float m_fLeftHandGripWeight;
	float m_fRightHandGripWeight;

	float m_fFirstPersonCameraHeading;
	float m_fFirstPersonCameraPitch;

	fwFlags8 m_uProbeFlags;
	fwFlags8 m_uRunningFlags;

	ObjectId m_parachuteObjectId;

	bool m_bIsBraking : 1;
	bool m_bLandedProperly : 1;
	bool m_bRemoveParachuteForScript : 1;
	bool m_bForcePedToOpenChute : 1;
	bool m_cloneSubtaskLaunched : 1;
	bool m_bLeaveSmokeTrail : 1;
	bool m_bParachuteHasControlOverPed : 1;
	bool m_bCanEarlyOutForMovement : 1;
	bool m_bIncrementedStatSmokeTrail : 1;
	bool m_bIsLodChangeAllowedThisFrame : 1;
	bool m_bShouldUseLowLod : 1;
	bool m_bIsUsingLowLod : 1;
	bool m_bInactiveCollisions : 1;
	bool m_bCanCheckForPadShakeOnDeploy : 1;
	bool m_bIsUsingFirstPersonCameraThisFrame : 1;
	bool m_bDriveByClipSetsLoaded : 1;

	bool m_bCloneParachuteConfigured : 1;	// have we configured this parachute or not?
	bool m_bCloneRagdollTaskRelaunch : 1;	// has the clone been in a collision and we're trying to get it back to animated and syncing asap....
	s8	 m_uCloneRagdollTaskRelaunchState;	// what state should we go back to after blending in from NM....
	
	bool m_pedChangingOwnership : 1;
	s32 m_cachedParachuteState;
	ObjectId m_cachedParachuteObjectId;

	// Air defence flags/explosion timer
	bool m_bAirDefenceExplosionTriggered;
	u32  m_uTimeAirDefenceExplosionTriggered;
	Vec3V m_vAirDefenceFiredFromPosition;

	static bank_float ms_fBrakingMult;
	static bank_float ms_fMaxBraking;
public:
	static atHashWithStringNotFinal GetModelForParachute(const CPed *pPed);
	static atHashWithStringNotFinal GetModelForParachutePack(const CPed *pPed);
	static bool IsParachute(const CObject& rObject);

	static const Vector3 &GetFirstPersonDrivebyIKOffset() { return sm_Tunables.m_FirstPersonDriveByIKOffset; }

#if __BANK
	static void ParachuteTestCB();
	
public:

	static void InitWidgets();
	static s32 GetParachuteModelForCurrentLevel();
	static void ForcePedToOpenChuteCB();
	static void ParachuteFromVehicleTestCB();
	static void ApplySmokeTrailPropertiesToFocusPedCB();
protected:
#endif

public:

	enum DataLerpes
	{
		LEFT_BRAKE		= 0,
		RIGHT_BRAKE		= 1,
		CURRENT_X		= 2,
		CURRENT_Y		= 3,
		CURRENT_HEADING = 4,
		NUM_LERPERS		= 5
	};

	void UpdateDataLerps(void)
	{
		for(int i = 0; i < NUM_LERPERS; ++i)
		{
			if(m_dataLerps[i].GetValue())
			{
				m_dataLerps[i].Update();
			}
		}
	}

	struct DataLerp
	{
		DataLerp()
		:
			m_numFrames(0),
			m_value(NULL),
			m_target(0)
		{}

		void SetValue(float* value)
		{
			m_value = value;
		}

		float* GetValue(void)
		{
			return m_value;
		}

		void SetNumFrames(u8 numFrames)
		{
			m_numFrames = numFrames;
		}

		void SetTarget(float target)
		{
			m_target = target;
		}

		float GetTarget(void)
		{
			return m_target;
		}

		void Update(void)
		{
			if((m_value) && (m_numFrames > 0))
			{
				*m_value = Lerp(1.0f / (float)m_numFrames, *m_value, m_target);;
			}
		}

	private:

		float*	m_value;
		float	m_target;
		u8		m_numFrames;
	};

	DataLerp m_dataLerps[NUM_LERPERS];

	//-------------------------------------
	// Static vars
	//-------------------------------------

	static bank_s32 ms_iSeatToParachuteFromVehicle;

	//target heading
	static bank_float ms_interpRateHeadingSkydive;

	static bank_float ms_fAttachDepth;
	static bank_float ms_fAttachLength;

	// Braking/Landing modifiers
	static Vector3 ms_vAdditionalAirSpeedNormalMin;
	static Vector3 ms_vAdditionalAirSpeedNormalMax;
	static Vector3 ms_vAdditionalDragNormalMin;
	static Vector3 ms_vAdditionalDragNormalMax;
	static Vector3 ms_vAdditionalAirSpeedBrakingMin;
	static Vector3 ms_vAdditionalAirSpeedBrakingMax;
	static Vector3 ms_vAdditionalDragBrakingMin;
	static Vector3 ms_vAdditionalDragBrakingMax;
	
	// Skydiving stuff
	static CFlightModelHelper ms_SkydivingFlightModelHelper;
	static CFlightModelHelper ms_DeployingFlightModelHelper;

	// Flying statics
public:
	static CFlightModelHelper ms_FlightModelHelper;
	static CFlightModelHelper ms_BrakingFlightModelHelper;
protected:
	static bank_float ms_fApplyCgOffsetMult;

#if __BANK
	static bool ms_bVisualiseAltitude;
	static bool ms_bVisualiseLandingScan;
	static bool ms_bVisualiseCrashLandingScan;
	static bool ms_bVisualiseControls;
	static bool ms_bVisualiseInfo;
	static float ms_fDropHeight;
	static bool ms_bCanLeaveSmokeTrail;
	static Color32 ms_SmokeTrailColor;
#endif

private:

public:

	float   m_finalDistSkydivingToDeploy;
	float   m_finalDistDeployingToParachuting;
	Vector3 m_preSkydivingVelocity;
	Vector3 m_postSkydivingVelocity;
	Vector3 m_preDeployingVelocity;
	Vector3 m_postDeployingVelocity;

	bool	m_isOwnerParachuteOut;

#if !__FINAL

	float m_maxdist;
	float m_total;
	u32 m_count;

#if __BANK
	void RenderCloneSkydivingVelocityInfo(void);
#endif /* BANK */

	void PrintAllDataValues() const;

#endif /* !__FINAL */

private:

	static Tunables	sm_Tunables;

};

//-------------------------------------------------------------------------
// Task info for parachute
//-------------------------------------------------------------------------
class CClonedParachuteInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			s32 const		state, 
			CEntity* const	parachute,
			u8 const		iFlags, 
			float const		currentHeading,
			float const		currentX,
			float const		currentY,
			float const		totalStickInput,
			float const		roll,
			float const		pitch,
			float const		leftBrake,
			float const		rightBrake,
			bool const		leaveSmokeTrail,
			bool const		canLeaveSmokeTrail,
			Color32 const	smokeTrailColor,
			u8 const		iSkydiveTransition,
			s8 const		landingType,
			u8 const		parachuteLandingFlags,
			bool const		crashLandWithParachute,
			Vector3 const&	pedVelocity,
			bool const		isParachuteOut
		)
		:
			m_state(state),
			m_parachute(parachute),
			m_iFlags(iFlags),
			m_fCurrentHeading(currentHeading),
			m_fCurrentX(currentX),
			m_fCurrentY(currentY),
			m_fTotalStickInput(totalStickInput),
			m_fRoll(roll),
			m_fPitch(pitch),
			m_fLeftBrake(leftBrake),
			m_fRightBrake(rightBrake),
			m_bLeaveSmokeTrail(leaveSmokeTrail),
			m_bCanLeaveSmokeTrail(canLeaveSmokeTrail),
			m_SmokeTrailColor(smokeTrailColor),
			m_iSkydiveTransition(iSkydiveTransition),
			m_landingType(landingType),
			m_parachuteLandingFlags(parachuteLandingFlags),
			m_crashLandWithParachute(crashLandWithParachute),
			m_pedVelocity(pedVelocity),
			m_bIsParachuteOut(isParachuteOut)
		{}

		s32 const		m_state;
		CEntity* const	m_parachute;
		u8 const		m_iFlags;
		float const		m_fCurrentHeading;
		float const		m_fCurrentX;
		float const		m_fCurrentY;
		float const		m_fTotalStickInput;
		float const		m_fRoll;
		float const		m_fPitch;
		float const		m_fLeftBrake;
		float const		m_fRightBrake;
		bool const		m_bLeaveSmokeTrail;
		bool const		m_bCanLeaveSmokeTrail;
		Color32 const	m_SmokeTrailColor;
		u8	const		m_iSkydiveTransition;
		u8	const		m_landingType;
		u8	const		m_parachuteLandingFlags;
		bool const		m_crashLandWithParachute;
		Vector3 const	m_pedVelocity;
		bool const		m_bIsParachuteOut;
	};

public:

    CClonedParachuteInfo( InitParams const & _initParams );

    CClonedParachuteInfo();

    ~CClonedParachuteInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_PARACHUTE;}
	virtual CTaskFSMClone *CreateCloneFSMTask();

    virtual bool			HasState() const					{ return true; }
	virtual u32				GetSizeOfState() const				{ return datBitsNeeded<CTaskParachute::State_Quit>::COUNT; }
    virtual const char*		GetStatusName(u32) const			{ return "Status";}

	inline s32				GetFlags(void)						{ return m_iFlags; }

	inline float 			GetCurrentHeading(void) const		{ return m_fCurrentHeading;		}
	inline float			GetCurrentX(void) const				{ return m_fCurrentX;			}
	inline float 			GetCurrentY(void) const				{ return m_fCurrentY;			}
	inline float 			GetTotalStickInput(void) const		{ return m_fTotalStickInput;	}
	inline float 			GetRoll(void) const					{ return m_fRoll;				}
	inline float 			GetPitch(void) const				{ return m_fPitch;				}
	inline float			GetLeftBrake(void) const			{ return m_fLeftBrake;			}
	inline float			GetRightBrake(void) const			{ return m_fRightBrake;			}

	inline const CEntity*	GetParachute()	const				{ return m_pParachuteObject.GetEntity();	}
	inline CEntity*			GetParachute()						{ return m_pParachuteObject.GetEntity();	}
	
	inline ObjectId			GetParachuteId()					{ return m_pParachuteObject.GetEntityID();  }

	inline bool				GetLeaveSmokeTrail() const			{ return m_bLeaveSmokeTrail; }
	inline bool				GetCanLeaveSmokeTrail() const		{ return m_bCanLeaveSmokeTrail; }
	inline Color32			GetSmokeTrailColor() const			{ return m_SmokeTrailColor; }
	
	inline s32				GetSkydiveTransition() const		{ return m_iSkydiveTransition; }

	inline s8				GetLandingType() const				{ return GetUnPackedLandingType(m_landingType);	}
	inline u8				GetParachuteLandingFlags() const	{ return m_parachuteLandingFlags; }
	inline bool				GetCrashLandWithParachute() const	{ return m_crashLandWithParachute; }

	inline Vector3 const&	GetPedVelocity() const				{ return m_pedVelocity; }

	inline bool				IsParachuteOut() const				{ return m_bIsParachuteOut; }

	void Serialise(CSyncDataBase& serialiser);

private:

    u8 GetPackedLandingType(s8 landingType) const;
    s8 GetUnPackedLandingType(u8 landingType) const;
		
    CClonedParachuteInfo(const CClonedParachuteInfo &);
    CClonedParachuteInfo &operator=(const CClonedParachuteInfo &);

	static const float MAXIMUM_VELOCITY_VALUE; 

	static const unsigned int SIZEOF_MODEL_INDEX		= 32;
	static const unsigned int SIZEOF_CURRENT_HEADING	= 8;
	static const unsigned int SIZEOF_CURRENT_X			= 8;
	static const unsigned int SIZEOF_CURRENT_Y			= 8;
	static const unsigned int SIZEOF_TOTAL_STICK_INPUT	= 8;
	static const unsigned int SIZEOF_ROLL				= 8;
	static const unsigned int SIZEOF_PITCH				= 8;
	static const unsigned int SIZEOF_COLOR_COMPONENT	= 5;
	static const unsigned int SIZEOF_LEFT_RIGHT_BRAKE   = 8;
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_X= 24;
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_Y= 24;
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_Z= 24;

	CSyncedEntity	m_pParachuteObject;

	Vector3 m_pedVelocity;			//we need to make sure the owner and clone are falling at the same velocity to stay closer in sync
									//otherwise the clone parachute could "drift" in odd directions (i.e up and backwards) as
									//the clone is lerped towards the owner when the parachute has been deployed.

	float m_fCurrentHeading;		//current heading based on the stick input
	float m_fCurrentX;				//current input from the X axis
	float m_fCurrentY;				//current input from the Y axis
	float m_fTotalStickInput;		//total stick input
	float m_fRoll;
	float m_fPitch;
	float m_fLeftBrake;
	float m_fRightBrake;
	
	Color32 m_SmokeTrailColor;
	
	u8 m_landingType:3;
	u8 m_iSkydiveTransition:2;
	u8 m_parachuteLandingFlags:3;
	u8 m_iFlags:6;
	bool m_crashLandWithParachute:1;
	bool m_bLeaveSmokeTrail:1;
	bool m_bCanLeaveSmokeTrail:1;
	bool m_bIsParachuteOut:1;
};

#endif
