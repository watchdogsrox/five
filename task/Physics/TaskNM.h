// Filename   :	TaskNM.h
// Description:	FSM base classes for all natural motion related tasks.
//              Contains CTaskNMBehaviour from which all natural motion tasks are derived and
//              CTaskNMControl which is a high level FSM derived class responsible for starting,
//              running and stopping each NM behaviour task.

#ifndef INC_TASKNM_H_
#define INC_TASKNM_H_

// --- Include Files ------------------------------------------------------------

// rage headers
#include "fragmentnm/nmbehavior.h"
#include "fragmentnm/nm_channel.h"
#include "fragmentnm/messageparams.h"

// Game headers
#include "animation/MovePed.h"
#include "peds/QueriableInterface.h"
#include "Task/Physics/AnimPoseHelper.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "system/FileMgr.h"
#include "Physics/gtaInst.h"

// ------------------------------------------------------------------------------

#if !__NO_OUTPUT
#define nmEntityDebugf(pEnt, fmt,...) if (pEnt) { nmDebugf3("[%u:%u] - %s(%p): " fmt, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), (pEnt)->GetModelName(), (pEnt), ##__VA_ARGS__);  }
#define nmTaskDebugf(pTask, fmt,...) if (pTask) { nmDebugf3("[%u:%u] - Task:%s %s(%p): " fmt, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), pTask->GetName().c_str() , pTask->GetEntity() ? pTask->GetEntity()->GetModelName() : "No entity", pTask->GetEntity(), ##__VA_ARGS__); }
#else
#define nmEntityDebugf(pEnt,fmt,...) do {} while(false)
#define nmTaskDebugf(pTask,fmt,...) do {} while(false)
#endif //__NO_OUTPUT

#if (__DEV) && (__ASSERT)
#define ASSERT_PARAMETER_NAME(s) Assertf(!strcmp(name, s), "%s expected got %s!", s, name)
#else
#define ASSERT_PARAMETER_NAME(s)
#endif

#define NMSTR_MSG(STRING_ENUM) CTaskNMBehaviour::GetMsgString(STRING_ENUM)
#define NMSTR_PARAM(STRING_ENUM) CTaskNMBehaviour::GetParamString(STRING_ENUM)

#define LOCAL_POS_MAG_LIMIT 10.0f

// Forward declarations:
class ARTFeedbackInterfaceGta;
class CTaskNMControl;
class CTaskNMBehaviour;

XPARAM(nmtuning);

// nm message list. Should be allocated on the stack, and populated from code / nm tuning sets 
class CNmMessageList
{
public:
	CNmMessageList()
	{
		
	}

	~CNmMessageList()
	{
		Clear();
	}

	// gets the appropriate message (adding a new one if necessary)
	ART::MessageParams& GetMessage(const char * messageName, s32 numFreeParams = 1)
	{
		atFinalHashString hash(messageName);
		return GetMessage(hash, numFreeParams);
	}

	ART::MessageParams& GetMessage(atFinalHashString messageHash, s32 numFreeParams = 1);

	bool HasMessage(const char * messageName)
	{
		atFinalHashString hash(messageName);
		return HasMessage(hash);
	}

	bool HasMessage(atFinalHashString messageHash);

	void Post(fragInstNMGta* pInst);
	void Clear();

	static ART::MessageParamsBase::Parameter* FindParam(ART::MessageParams& msg, const char* pParamName)
	{
		for (s32 i=msg.getUsedParamCount()-1; i>=0; i--)
		{
			if (!strcmp(msg.getParam(i).m_name,pParamName))
			{
				return &msg.getParam(i);
			}
		}
		return NULL;
	}

private:
	struct message{
		atFinalHashString m_name;
		ART::MessageParams* m_message;
	};
	atArray< message> m_messages;
};

class CNmParameter
{
public:

	static atHashString ms_ResetAllParameters;

	CNmParameter()	{ }
	virtual ~CNmParameter() { }

	atFinalHashString m_Name;
	
	virtual void AddTo(ART::MessageParams& UNUSED_PARAM(msg)) const
	{
		
	}

#if __BANK
	virtual void AddWidgets(bkGroup&, const NMBehavior&)
	{

	}
#endif //__BANK

	PAR_PARSABLE;
};

class CNmParameterResetMessage : public CNmParameter
{
public:
	CNmParameterResetMessage()	{ }
	virtual ~CNmParameterResetMessage() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.reset();
	}

#if __BANK
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK

	PAR_PARSABLE;
};

class CNmParameterFloat : public CNmParameter
{
public:
	CNmParameterFloat()	{ }
	virtual ~CNmParameterFloat() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addFloat(m_Name.GetCStr(), m_Value);
	}

#if __BANK
	CNmParameterFloat(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK


	float m_Value;

	PAR_PARSABLE;
};

class CNmParameterRandomFloat : public CNmParameter
{
public:
	CNmParameterRandomFloat()	{ }
	virtual ~CNmParameterRandomFloat() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addFloat(m_Name.GetCStr(), fwRandom::GetRandomNumberInRange(m_Min, m_Max));
	}

#if __BANK
	CNmParameterRandomFloat(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK

	float m_Min;
	float m_Max;

	PAR_PARSABLE;
};

class CNmParameterInt : public CNmParameter
{
public:
	CNmParameterInt()	{ }
	virtual ~CNmParameterInt() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addInt(m_Name.GetCStr(), m_Value);
	}

#if __BANK
	CNmParameterInt(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK


	int m_Value;

	PAR_PARSABLE;
};

class CNmParameterRandomInt : public CNmParameter
{
public:
	CNmParameterRandomInt()	{ }
	virtual ~CNmParameterRandomInt() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addInt(m_Name.GetCStr(), fwRandom::GetRandomNumberInRange(m_Min, m_Max));
	}

#if __BANK
	CNmParameterRandomInt(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK

	s32 m_Min;
	s32 m_Max;

	PAR_PARSABLE;
};

class CNmParameterBool : public CNmParameter
{
public:
	CNmParameterBool()	{ }
	virtual ~CNmParameterBool() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addBool(m_Name.GetCStr(), m_Value);
	}

#if __BANK
	CNmParameterBool(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK


	bool m_Value;

	PAR_PARSABLE;
};

class CNmParameterString : public CNmParameter
{
public:
	CNmParameterString()	{ }
	virtual ~CNmParameterString() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addString(m_Name.GetCStr(), m_Value.GetCStr());
	}

#if __BANK
	CNmParameterString(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK


	atFinalHashString m_Value;

	PAR_PARSABLE;
};

class CNmParameterVector : public CNmParameter
{
public:
	CNmParameterVector()	{ }
	virtual ~CNmParameterVector() { }

	virtual void AddTo(ART::MessageParams& msg) const
	{
		msg.addVector3(m_Name.GetCStr(), m_Value.x, m_Value.y, m_Value.z);
	}

#if __BANK
	CNmParameterVector(const NMParam* pParam);
	virtual void AddWidgets(bkGroup& bank, const NMBehavior& behavior);
#endif //__BANK


	Vector3 m_Value;

	PAR_PARSABLE;
};


class CNmMessage : datBase
{
public:
	CNmMessage()
#if __BANK
		: m_pWidgets(NULL)
		, m_SelectedParameter(0)
#endif //__BANK
	{ }

	virtual ~CNmMessage() 
	{
#if __BANK
		m_pWidgets=NULL;
#endif //__BANK
	}

	void AddTo(CNmMessageList& list, CTaskNMBehaviour* pTask) const;
	void Post(CPed& ped, CTaskNMBehaviour* pTask) const;

	// adds the editing widgets for the message
#if __BANK
	void AddWidgets(bkGroup& bank);

	bkGroup* m_pWidgets;
	s32 m_SelectedParameter;

	void AddSingleParameter();
	void AddRandomRange();
	void AddAllParameters();
	void RemoveSingleParameter();
	void RemoveDefaultValueParameters();
	void RemoveAllParameters();

	void PromoteSelectedParam();
	void DemoteSelectedParam();

	void AddParameter(const char * name, bool withRandomRange = false);
	void RemoveParameter(const char * name);

	void RegenerateParamWidgets();

#endif //__BANK

	atFinalHashString m_Name;
	atArray<CNmParameter*> m_Params;
	bool m_ForceNewMessage;
	bool m_TaskMessage;

	PAR_SIMPLE_PARSABLE;
};

class CNmTuningSet
{
public:
	CNmTuningSet();
	virtual ~CNmTuningSet();

	void AddTo(CNmMessageList& list, CTaskNMBehaviour* pTask) const;
	void Post(CPed& ped, CTaskNMBehaviour* pTask) const;
	void Post(CPed& ped, CTaskNMBehaviour* pTask, atHashString messageHash) const;

	bool IsEmpty() const { return m_Messages.GetCount()==0;}
	bool IsEnabled() const { return m_Enabled;}
	// adds the editing widgets for the message
#if __BANK
	void AddWidgets(bkBank& bank) { AddWidgetsInternal(*static_cast<bkGroup*>(bank.GetCurrentGroup())); }
	void AddWidgetsInternal(bkGroup& bank);

	bkGroup* m_pWidgets;

	static s32 sm_SelectedMessage;
	static atArray<const char *> sm_MessageNames;
	static atMap<u32, atArray<const char *> * > sm_ParameterNamesMap;

	void AddSingleMessage();
	void RemoveSingleMessage();
	void RemoveAllMessages();

	void PromoteSelectedMessage();
	void DemoteSelectedMessage();

	void AddMessage(const char * name);
	void RemoveMessage(const char * name);

	void RegenerateMessageWidgets();

	static NMBehaviorPool sm_TaskMessageDefs;

#endif //__BANK

	atHashString		m_Id;
	int					m_Priority;
	bool				m_Enabled;
	atArray<CNmMessage*>	m_Messages;
	
	PAR_SIMPLE_PARSABLE;
};

class CNmTuningSetGroup
{
public:
	CNmTuningSet* Get(const atHashString& setName)
	{
		return m_sets.Access(setName);
	}
	void Append(CNmTuningSet* newItem, atHashString key)
	{
		if(!m_sets.Access(key))
		{
			m_sets.Insert(key,*newItem);
			m_sets.FinishInsertion();
		}
	}
	void Revert(atHashString key)
	{
		m_sets.Remove(m_sets.GetIndexFromDataPtr(m_sets.Access(key)));
#if __BANK
		bkWidget* child = m_pWidgets->GetChild();
		while(child)
		{
			bkWidget*lastChild = child;
			child = child->GetNext();
			if (atStringHash(lastChild->GetTitle())==key)
			{
				lastChild->Destroy();
			}
		}
#endif //__BANK
	}
private:
	atBinaryMap< CNmTuningSet, atHashString > m_sets;
#if __BANK
public:
	void AddParamWidgets(bkBank& bank);

	bkGroup* m_pWidgets;
	
	static char ms_SetName[128];

	void AddSet();
	void RemoveSet();
#endif //__BANK

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////
// FSM version of the base class of all natural motion behaviour tasks.
////////////////////////////////////////////////////////////////////////
class CTaskNMBehaviour : public CTaskFSMClone
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		CNmTuningSet m_animPoseDefault;
		CNmTuningSet m_animPoseAttachDefault;
		CNmTuningSet m_animPoseAttachToVehicle;
		CNmTuningSet m_animPoseHandsCuffed;

		CNmTuningSet m_forceFall;

		struct TunableForce
		{
			bool ShouldApply(u32 startTime);

			// Pass in a normalised direction vector for the force.
			void GetImpulseVec(Vec3V_InOut forceVec, Vec3V_In velVec, const CPed* pPed);

			bool m_Enable;

			float m_Mag;

			bool m_ScaleWithVelocity;
			float m_VelocityMin;
			float m_VelocityMax;
			float m_ForceAtMinVelocity;
			float m_ForceAtMaxVelocity;

			bool m_ClampImpulse;
			float m_MinImpulse;
			float m_MaxImpulse;

			u32 m_Delay;
			u32 m_Duration;

			bool m_ScaleWithUpright;

			PAR_SIMPLE_PARSABLE;
		};

		struct InverseMassScales
		{
			bool Apply(phContactIterator* pImpacts);

			bool m_ApplyVehicleScale;
			float m_VehicleScale;
			bool m_ApplyPedScale;
			float m_PedScale;

			PAR_SIMPLE_PARSABLE;
		};

		struct Damping
		{
			bool Apply(CPed* pPed);

			bool m_ApplyLinear;
			Vector3 m_Constant;
			Vector3 m_Velocity;
			Vector3 m_Velocity2;
			float m_Max;

			PAR_SIMPLE_PARSABLE;
		};

		struct ActivationLimitModifiers
		{
			float m_BumpedByCar;
			float m_BumpedByCarFriendly;
			float m_PlayerBumpedByCar;
			float m_MinVehicleWarning;

			float m_BumpedByPedMinVel;
			float m_BumpedByPedMinDotVel;
			float m_BumpedByPed;
			float m_BumpedByPlayerRagdoll;
			float m_BumpedByPedRagdoll;
			float m_BumpedByPedIsQuadruped;
			float m_BumpedByPedFriendly;
			float m_BumpedByObject;

			float m_Walking;
			float m_Running;
			float m_Sprinting;

			float m_MaxPlayerActivationLimit;
			float m_MaxAiActivationLimit;

			PAR_SIMPLE_PARSABLE;
		};

		struct BoundWeight
		{
			RagdollComponent m_Bound;
			float m_Weight;

			PAR_SIMPLE_PARSABLE;
		};

		struct RagdollUnderWheelTuning
		{
			float m_fMinSpeedForPush;
			float m_fImpulseMultLimbs;
			float m_fImpulseMultSpine;
			float m_fFastCarPushImpulseMult;

			PAR_SIMPLE_PARSABLE;
		};

		RagdollUnderWheelTuning m_RagdollUnderWheelTuning;

		struct KickOnGroundTuning
		{
			float m_fPronePedKickImpulse;

			PAR_SIMPLE_PARSABLE;
		};

		KickOnGroundTuning m_KickOnGroundTuning;

		struct BlendOutThreshold
		{
			float m_MaxLinearVelocity;
			float m_MaxAngularVelocity;

			u32 GetSettledTime(u32 seed) const;

		private:

			u32	m_SettledTimeMS;
			bool m_RandomiseSettledTime;
			u32 m_SettledTimeMinMS;

		public:

			PAR_SIMPLE_PARSABLE;
		};

		struct StandardBlendOutThresholds
		{
			const BlendOutThreshold& PickBlendOut(const CPed* pPed) const;

			BlendOutThreshold m_Ai;
			BlendOutThreshold m_Player;
			BlendOutThreshold m_PlayerMp;

			PAR_SIMPLE_PARSABLE;
		};

		struct PedCapsuleVehicleImpactTuning
		{
			bool m_EnableActivationsFromCapsuleImpacts;
			float m_VehicleVelToImpactNormalMinDot;

			bool m_EnableSideSwipeActivations;
			bool m_EnableSideSwipeActivationsFirstPerson;
			float m_MinSideNormalForSideSwipe;
			float m_MinVelThroughNormalForSideSwipe;
			float m_MinAccumulatedImpactForSideSwipe;
			float m_MinVehVelMagForSideSwipe;
			float m_MinVehVelMagForBicycleSideSwipe;

			float m_MinAccumulatedImpactForSideSwipeCNC;
			float m_MinVehVelMagForSideSwipeCNC;
			float m_MinVehVelMagForBicycleSideSwipeCNC;

			PAR_SIMPLE_PARSABLE;
		};

		PedCapsuleVehicleImpactTuning m_CapsuleVehicleHitTuning;

		void ParserPostLoad();

		bool m_EnableRagdollPooling;
		u32 m_MaxGameplayNmAgents;
		u32 m_MaxRageRagdolls;
		bool m_ReserveLocalPlayerNmAgent;

		bool m_EnableRagdollPoolingMp;
		u32 m_MaxGameplayNmAgentsMp;
		u32 m_MaxRageRagdollsMp;
		bool m_ReserveLocalPlayerNmAgentMp;

		bool m_BlockOffscreenShotReactions;

		bool m_UsePreEmptiveEdgeActivation;
		bool m_UsePreEmptiveEdgeActivationMp;
		bool m_UseBalanceForEdgeActivation;
		float m_PreEmptiveEdgeActivationMaxVel;
		float m_PreEmptiveEdgeActivationMaxHeadingDiff;
		float m_PreEmptiveEdgeActivationMinDotVel;
		float m_PreEmptiveEdgeActivationMaxDistance;
		float m_PreEmptiveEdgeActivationMinDesiredMBR2;

		void OnMaxPlayerAgentsChanged();
		void OnMaxGameplayAgentsChanged();
		void OnMaxRageRagdollsChanged();

		StandardBlendOutThresholds m_StandardBlendOutThresholds;

		atArray<BoundWeight> m_CamAttachPositionWeights;

		ActivationLimitModifiers m_SpActivationModifiers;
		ActivationLimitModifiers m_MpActivationModifiers;

		float m_PlayerBumpedByCloneCarActivationModifier; // this one isn't needed for single player.
		float m_ClonePlayerBumpedByCarActivationModifier; // this one isn't needed for single player.
		float m_ClonePedBumpedByCarActivationModifier; // this one isn't needed for single player.

		float m_MaxVehicleCapsulePushTimeForRagdollActivation;
		float m_MaxVehicleCapsulePushTimeForPlayerRagdollActivation;
		float m_VehicleMinSpeedForContinuousPushActivation;
		float m_MinContactDepthForContinuousPushActivation;
		float m_DurationRampDownCapsulePushedByVehicle;
		float m_VehicleMinSpeedForAiActivation;
		float m_VehicleMinSpeedForStationaryAiActivation;
		float m_VehicleMinSpeedForPlayerActivation;
		float m_VehicleMinSpeedForStationaryPlayerActivation;
		float m_VehicleMinSpeedForWarningActivation;
		float m_VehicleFallingSpeedWeight;

		float m_VehicleActivationForceMultiplierDefault;
		float m_VehicleActivationForceMultiplierBicycle;
		float m_VehicleActivationForceMultiplierBike;
		float m_VehicleActivationForceMultiplierBoat;
		float m_VehicleActivationForceMultiplierPlane;
		float m_VehicleActivationForceMultiplierQuadBike;
		float m_VehicleActivationForceMultiplierHeli;
		float m_VehicleActivationForceMultiplierTrain;
		float m_VehicleActivationForceMultiplierRC;

		bool m_ExcludePedBumpAngleFromPushCalculation;
		float m_PedActivationForceMultiplier;

		u32 m_MaxPlayerCapsulePushTimeForRagdollActivation;
		float m_PlayerCapsuleMinSpeedForContinuousPushActivation;

		float m_ObjectMinSpeedForActivation;
		float m_ObjectActivationForceMultiplier;

		float m_StuckOnVehicleMaxTime;

		StandardBlendOutThresholds m_StuckOnVehicleBlendOutThresholds;

		CNmTuningSet m_Start;

		CNmTuningSet m_TeeterControl;

		PAR_PARSABLE;
	};

	enum
	{
		ANIM_POSE_DEFAULT,
		ANIM_POSE_ATTACH_DEFAULT,
		ANIM_POSE_ATTACH_TO_VEHICLE,
		ANIM_POSE_HANDS_CUFFED,
	};

	enum
	{
		State_Start = 0,
		State_StreamingClips,
		State_BehaviourRunning,
		State_AnimatedFallback,
		State_Finish
	};

	enum eMonitorState
	{
		MONITOR_RELAX = 0,
		MONITOR_FALL, 
		MONITOR_STAND, 
		MONITOR_SUBMERGED,
		MONITOR_NUM_STATES
	};
	enum eMonitorType
	{
		MONITOR_AI = 0,
		MONITOR_PLAYER,
		MONITOR_PLAYER_MP,
		MONITOR_NUM_TYPES
	};
	enum eMonitorFlags {
		FLAG_VEL_CHECK				= BIT(0), // Finish the task once the peds velocity drops below a certain threshold
		FLAG_SKIP_DEAD_CHECK		= BIT(1), // Don't abort the task immediately on death
		FLAG_SKIP_WATER_CHECK		= BIT(2), // unused
		FLAG_RELAX_AP_LOW_HEALTH	= BIT(3), // When exiting the task, tell nm control to do a relax if the peds health is low (below 110)
		FLAG_QUIT_IN_WATER			= BIT(4),
		FLAG_SKIP_MIN_TIME_CHECK	= BIT(5)
	}; 

	enum eNMBehaviourFlags
	{
		ALL_FLAGS_CLEAR						= 0,
		// In case an clipPose behaviour is required for a particular behaviour (but not meant to
		// carry over to any other NM task which might get started), we include the ability to
		// set this flag through the behaviour task.
		DO_CLIP_POSE						= BIT(0),
		DONT_SWITCH_TO_ANIMATED_ON_ABORT	= BIT(1),
		DONT_FINISH							= BIT(2)	//	used for clone peds to keep the task running until told to stop via a network update
	};

	enum eRagdollPool
	{
		kRagdollPoolNmGameplay = 0,
		kRagdollPoolRageRagdoll,
		kRagdollPoolLocalPlayer,
		kNumRagdollPools,
		kRagdollPoolInvalid,
		kRagdollPoolMax = kRagdollPoolInvalid
	};
	CompileTimeAssert(kRagdollPoolMax < BIT(3));

	class CRagdollPool
	{
	public:
		CRagdollPool();
		~CRagdollPool();

		void	AddToPool(CPed& ped);
		void	RemoveFromPool(CPed& ped);

		inline s32		GetMaxRagdolls() { return m_MaxRagdolls; }
		inline void		SetMaxRagdolls(s32 max) {m_MaxRagdolls = max; }

		inline s32		GetRagdollCount() { return m_Peds.GetCount(); }
		inline s32		GetFreeSlots() { return GetMaxRagdolls() - GetRagdollCount(); }

		CPed* 			GetPed(s32 i)  { 
			if(i>=0 && i<m_Peds.GetCount())
				return m_Peds[i];
			else
				return NULL;
		}

	private:

		u32 m_MaxRagdolls;
		atArray<RegdPed> m_Peds;
	};

	static void AddToRagdollPool(eRagdollPool pool, CPed& ped);
	static void RemoveFromRagdollPool(CPed& ped);
	static bool RagdollPoolHasSpaceForPed(eRagdollPool pool, const CPed& ped, bool abortExisting = false, float priority = 0.0f);
	static bool IsPoolingEnabled()
	{
		if (NetworkInterface::IsGameInProgress())
		{
			if (!CTaskNMBehaviour::sm_Tunables.m_EnableRagdollPoolingMp)
				return false;
		}
		else if (!CTaskNMBehaviour::sm_Tunables.m_EnableRagdollPooling)
		{
			return false;
		}
		return true;
	}

	static float CalcRagdollEventScore(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue);
	static float CalcRagdollMultiplierScore(CPed* pPed, eRagdollTriggerTypes nTrigger);

	// PURPOSE: Called by the ragdoll priority system when activating a new ragdoll
	//			If the new incoming activation is blocked due to a lack of space in the ragdoll pool,
	//			The lowest priority ragdoll is aborted (assuming its score is lower than the incoming task)
	static float CalcRagdollScore(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue);

	static void StartNetworkGame();
	static void EndNetworkGame();

#if !__FINAL
	static const char * GetRagdollPoolName(eRagdollPool pool)
	{
		switch(pool)
		{
			case kRagdollPoolNmGameplay: return "Gameplay"; break;
			case kRagdollPoolRageRagdoll: return "RageRagdoll"; break;
			case kRagdollPoolLocalPlayer: return "Player"; break;
			case kRagdollPoolInvalid: return "None"; break;
			default: return "Unknown!"; break;
		}
	}
#endif //!__FINAL

	static bool IsMessageString(eNMStrings nString);
	static const char* GetMsgString(eNMStrings nString){Assertf(IsMessageString(nString), "NM msg enum %d is not a message", nString); return CNmDefines::ms_aNMStrings[nString];}
	static const char* GetParamString(eNMStrings nString){Assertf(!IsMessageString(nString), "NM param enum %d is not a param", nString); return CNmDefines::ms_aNMStrings[nString];}
	static eEffectorMaskStrings GetMaskEnumFromString(const char * maskString);

	// Use this to send start messages
	// Either gets run when the task starts updating or when the ragdoll first activates
	// Since for certain events there can be a physics update between the ragdoll first activating
	// and the task being run.
	void SendStartMessages(CPed* pPed);

	// Constructor / Destructor.
	CTaskNMBehaviour(u32 nMinTime, u32 nMaxTime);
	virtual ~CTaskNMBehaviour();

	///////////////////////
	// CTask functions:
	///////////////////////

	virtual bool IsNMBehaviourTask() const { return true; }

protected:
	CTaskNMBehaviour( const CTaskNMBehaviour& otherTask);

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool ControlPassingAllowed(CPed* pPed, const netPlayer& player, eMigrationType migrationType);
	virtual CTaskFSMClone* CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone* CreateTaskForLocalPed(CPed* pPed);
	virtual s32 GetDefaultStateAfterAbort() const {return State_Finish;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_StreamingClips",
			"State_BehaviourRunning",
			"State_AnimatedFallback",
			"State_Finish"
		};

		return aStateNames[iState];
	}

	// Especially useful for doing debug drawing while game is paused.
	virtual void Debug() const {};
#endif // !__FINAL

protected:
	// Helper functions for FSM state management:
	void       Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);
	void       StreamingClips_OnEnter(CPed* pPed);
	FSM_Return StreamingClips_OnUpdate(CPed* pPed);

	void       BehaviourRunning_OnEnter(CPed* pPed);
	FSM_Return BehaviourRunning_OnUpdate(CPed* pPed);
	void       BehaviourRunning_OnExit(CPed* pPed);

	void       AnimatedFallback_OnEnter(CPed* pPed);
	FSM_Return AnimatedFallback_OnUpdate(CPed* pPed);
	void       AnimatedFallback_OnExit(CPed* pPed);


public:
	// Functions to modify the behaviour's timing variables.
	void ResetStartTime();
	void ForceTimeout() {m_nMaxTime = 1; m_nStartTime = 1;}
#if __BANK
	static void InitCreateWidgetsButton();
	static void InitWidgets();

#endif
	// Accessors for the NM control flags.
public:
	u32 GetFlags() const {return m_nFlags;}
	void SetFlag(u32 nFlagToSet) {m_nFlags |= nFlagToSet;}
	void ClearFlag(u32 nFlagToClear) { m_nFlags &= ~nFlagToClear;}

	// Should this nm behaviour keep running after the ped dies?
	virtual bool ShouldContinueAfterDeath() const { return false; }
	// should this nm behaviour end when a particular type of damage is received, or keep running and handle it?
	virtual bool ShouldAbortForWeaponDamage(CEntity* UNUSED_PARAM(pFiringEntity), const CWeaponInfo* UNUSED_PARAM(pWeaponInfo), const f32 UNUSED_PARAM(fWeaponDamage), const fwFlags32& UNUSED_PARAM(flags), 
		const bool bWasKilledOrInjured, const Vector3& UNUSED_PARAM(vStart), WorldProbe::CShapeTestHitPoint* UNUSED_PARAM(pResult), 
		const Vector3& UNUSED_PARAM(vRagdollImpulseDir), const f32 UNUSED_PARAM(fRagdollImpulseMag)) 
	{ 
		if (bWasKilledOrInjured && ShouldContinueAfterDeath())
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	// Called by the physics system when the ragdoll is already active and a significant impact occurs.
	// Return true to indicate that this nm behaviour has handled the impact and there's no need to
	// generate a new ragdoll task. Return false to have ProcessRagdollImpact add a new
	// ragdoll task to deal with the collision.
	virtual bool HandleRagdollImpact(float UNUSED_PARAM(fMag), const CEntity* UNUSED_PARAM(pEntity), const Vector3& UNUSED_PARAM(vPedNormal), int UNUSED_PARAM(nComponent), phMaterialMgr::Id UNUSED_PARAM(nMaterialId)) { return false; }

	// Functions related to the clipPose helper.
public:
	CClipPoseHelper& GetClipPoseHelper();
	void SetClipPoseHelperClip(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId);
	void SetClipPoseHelperClip(s32 iClipDictIndex, u32 ClipHashKey);

	// Returns false if clip data is "invalid".
	bool GetClipPoseHelperClip(fwMvClipSetId &clipSetId, fwMvClipId &clipId);
	bool GetClipPoseHelperClip(s32 &nClipDictIndex, u32 &nClipHash);

	bool GetIsClipDictLoaded(s32 iClipDictIndex); 
	// Helper function to save checking and casting every time a reference to the NM control task is required from within a behaviour class.
	CTaskNMControl* GetControlTask();

	// The following functions (which don't have to be implemented) allow NM behaviour tasks to react to
	// feedback messages from the natural motion subsystem. Note: use QueryNmFeedbackMessage() to ensure the
	// feedback interface is for the correct behaviour.
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourStart(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

#if ART_ENABLE_BSPY
	// Handy functions for adding items to the bSpy scratchpad
	void SendTaskStateToBSpy();
	void SetbSpyScratchpadString(const char* pDebugString);
	void SetbSpyScratchpadVector3(const Vector3& pDebugVec);
	void SetbSpyScratchpadBool(bool debugBool);
	void SetbSpyScratchpadInt(int debugInt);
	void SetbSpyScratchpadFloat(float debugFloat);
#endif

	// Some public access functions to query the state of the current behaviour.
	bool GetHasAborted() {return m_bHasAborted;}
	bool GetHasFailed() {return m_bHasFailed;}
	bool GetHasSucceeded() {return m_bHasSucceeded;}
	bool GetHasStarted() {return m_nStartTime > 0;}

	virtual bool HasBalanceFailed() const { return false; }

	// PURPOSE: This function will be called by other systems when they want the ped to fall over immediately
	virtual void ForceFallOver();

	virtual bool HandlesRagdoll(const CPed*) const 
	{ 
		return true;
	}

	// PURPOSE: Applies a task tuning messsage from the tuning sets
	virtual void ApplyTaskTuningMessage(const CNmMessage& message);

	// Allow read only access to this behaviour's minimum and maximum time variables. Used when wrapping a
	// behaviour task by a CTaskNMControl.
	u32 GetMinTime() { return m_nMinTime; }
	u32 GetMaxTime() { return m_nMaxTime; }

	bool IsHighLODNMAgent(CPed* pPed);

	// helper stuff to decide what to do next - either next behaviour to run or how to blend back to clip
	fwMvClipId GetSuggestedNextClipId() const {return m_suggestedClipId;}
	fwMvClipSetId GetSuggestedNextClipGroup() const {return m_suggestedClipSetId;}
	eBlendFromNMOptions GetSuggestedNextBlendOption() const {return m_nSuggestedBlendOption;}
	CTaskTypes::eTaskType GetSuggestedNextTask() {return m_nSuggestedNextTask;}

	bool GetIsStuckOnVehicle();

	virtual float GetPlayerControlForce() {return ms_fStdPlayerRagdollControlForce;}

	// Static functions related to the NM message / parameter strings.
	static void InitStrings();
	static bool IsParamForThisMessage(eNMStrings nMsgString, eNMStrings nParamString);
	static int GetMessageStringIndex(eNMStrings nString);
	static eNMStrings GetMessageFromIndex(int nMessageIndex) {return CNmDefines::ms_nMessageList[nMessageIndex];}

	static CRagdollPool& GetRagdollPool(eRagdollPool pool) { taskAssert(pool>=kRagdollPoolNmGameplay && pool<kNumRagdollPools); return sm_RagdollPools[pool]; }

	static bool DoesResponseHandleRagdoll(const CPed* pPed, const aiTask* pResponseTask);
	static bool CanUseRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible=NULL, float fPushValue=0.0f);
	static bool CanUseRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, float fScore);
	static float WantsToRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible=NULL, float fPushValue=0.0f);
	static bool DoesGroundPhysicalMatch(const CPhysical* pGroundPhysical, const CVehicle* pVehicle);
	static float CalculateActivationForce(const CPed* pPed, eRagdollTriggerTypes trigger, const CEntity* pEntityResponsible, Vec3V_ConstPtr pHitPosition = NULL, Vec3V_ConstPtr pHitNormal = NULL, int nEntityComponent = 0, bool testRagdollInst = false);
	static bool ShouldUseArmedBumpResistance(const CPed* pPed);
	static float GetMinSpeedThreshold(const CPed* pPed, const Vector3& pedVel);
	
	virtual bool ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice));

	static bool LoadTaskParameters();

	// Helper function to test whether the feedback interface passed in corresponds to the
	// NM behaviour referenced by "paramName".
	static bool QueryNmFeedbackMessage(ARTFeedbackInterfaceGta* pFeedbackInterface, enum eNMStrings paramName);

	static bool ShouldReactToVehicleHit(CPed* pPed, CVehicle* pVehicle, const CCollisionRecord* pColRecord);

	static float GetPronePedKickImpulse() { return sm_Tunables.m_KickOnGroundTuning.m_fPronePedKickImpulse; } 

	static float GetWheelMinSpeedForPush() { return sm_Tunables.m_RagdollUnderWheelTuning.m_fMinSpeedForPush; } 
	static float GetWheelImpulseMultLimbs() { return sm_Tunables.m_RagdollUnderWheelTuning.m_fImpulseMultLimbs; } 
	static float GetWheelImpulseMultSpine() { return sm_Tunables.m_RagdollUnderWheelTuning.m_fImpulseMultSpine; } 
	static float GetWheelFastCarPushImpulseMult() { return sm_Tunables.m_RagdollUnderWheelTuning.m_fFastCarPushImpulseMult; } 

protected:
	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// Ensure the nm performance will continue for at least the specified amount of time
	void BumpPerformanceTime(u32 time);

	// returns a normalised float (from -1.0f to 1.0) giving a rough indication of how 'upright' the ragdoll currently is
	// 1.0f = upright
	// -1.0f = upside down
	// 0.0f = horizontal
	float CalculateUprightDot(const CPed * pPed);

	// All inherited NM behaviours need to implement StartBehaviour() and FinishConditions(). The latter returns a bool
	// to inform the caller if the behaviour will quit. A default implementation of ControlBehaviour() is provided, so
	// inherited tasks only need to override this if they want.
	virtual void StartBehaviour(CPed* pPed) = 0;
	virtual void ControlBehaviour(CPed* UNUSED_PARAM(pPed)) {;}
	virtual bool FinishConditions(CPed* pPed) = 0;

	void CreateAnimatedFallbackTask(CPed* pPed, bool bFalling);
	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);
	virtual void EndAnimatedFallback(CPed* UNUSED_PARAM(pPed)) {;}

	virtual void AddStartMessages(CPed* UNUSED_PARAM(pPed), CNmMessageList& UNUSED_PARAM(list)) {;}

	bool ProcessBalanceBehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	bool ProcessFinishConditionsBase(CPed* pPed, eMonitorState nState, int nFlags, const CTaskNMBehaviour::Tunables::BlendOutThreshold* pCustomBlendOutThreshold = NULL);

	bool CheckBlendOutThreshold(CPed* pPed, const CTaskNMBehaviour::Tunables::BlendOutThreshold& threshold);

	bool OnMovingGround(CPed* pPed);

	void AddTuningSet(const CNmTuningSet* pSet);
	void ClearTuningSets();
	void ApplyTuningSetsToList(CNmMessageList& list, bool bClearAfterSending =true);

	static s32 CompareTuningSetPriorityFunc(const CNmTuningSet* const* a, const CNmTuningSet* const* b)
	{
		return (*a)->m_Priority - (*b)->m_Priority;
	}

protected:
	bool m_bHasAborted		: 1;
	bool m_bHasFailed		: 1;
	bool m_bHasSucceeded	: 1;
	bool m_bDoBehaviorStart : 1;
	bool m_bCuffedClipLoaded : 1;
	bool m_RefFrameVelChecked : 1;
	bool m_bUseAdaptiveAngularVel : 1;
	bool m_bStartMessagesSent : 1;

	fwMvClipId m_suggestedClipId;
	fwMvClipSetId m_suggestedClipSetId;
	float m_suggestedClipPhase;
	eBlendFromNMOptions m_nSuggestedBlendOption:8;
	CTaskTypes::eTaskType m_nSuggestedNextTask:16;

	// Store the clip data for the clipPose helper.
	fwMvClipSetId m_clipPoseSetId;
	fwMvClipId m_clipPoseId;

	s32 m_iClipDictIndex; 
	s32 m_iClipHash; 

	atArray<const CNmTuningSet*> m_Sets;

	u16 m_nMinTime;
	u16 m_nMaxTime;
	u32 m_nStartTime;

	u32 m_nSettledStartTime; // used when blending from nm. Tracks when the ragdoll first becomes 'settled' enough to blend from nm (based on the threshold passed into processfinishconditionsbase)
									
	u32 m_nFlags;

	float m_AdaptiveAngVelMinVel;
	float m_AdaptiveAngVelMaxVel;
	float m_AdaptiveAngVelDampingVel2;

	float m_ContinuousContactTime;

#if __DEV
	static bool ms_bDisplayDebug;
#endif

private:

	static char sm_pParameterFile[128];

	static float ms_fStdPlayerRagdollControlForce;

	// some global tunable stuff
public:

	static Tunables sm_Tunables;

	static CRagdollPool sm_RagdollPools[kNumRagdollPools];

	static bool sm_OnlyUseRageRagdolls;
	static bool sm_DoOverrideBulletImpulses;
	static float sm_OverrideImpulse;
	static float sm_ArmsImpulseCap;
	static float sm_DontFallUntilDeadStrength;
	static float sm_MaxShotUprightForce;
	static float sm_MaxShotUprightTorque;
	static float sm_ThighImpulseMin;
	static float sm_CharacterHealth;
	static float sm_CharacterStrength;
	static float sm_StayUprightMagnitude;
	static float sm_RigidBodyImpulseRatio;
	static float sm_ShotRelaxAmount;
	static float sm_BulletPopupImpulse;
	static float sm_SuccessiveImpulseIncreaseScale;
	static float sm_LastStandMaxTimeBetweenHits;
	static bool sm_DoLastStand;

	static bool ms_bLeanInDirApplyAsForce;
	static bool ms_bUseParameterSets;

	static bool ms_bDisableBumpGrabForPlayer;

#if __BANK
	struct TuningSetEntry
	{
		atHashString	id;
		u32				time;
	};

	static s32 SortTuningSetHistoryFunc(TuningSetEntry const* a, TuningSetEntry const* b)
	{
		return a->time - b->time;
	}
	static RegdPed m_pTuningSetHistoryPed;
	static atArray<TuningSetEntry> m_TuningSetHistory;
#endif //__BANK
};

////////////////////////////////////////////////////////////////////////////////////////////
// Callback function for setting up grabbing parameters when creating a clone NM task
////////////////////////////////////////////////////////////////////////////////////////////
typedef void (*fnGetGrabParamsCallback)(aiTask *pTaskRequestingGrab, aiTask *pTaskNM);

////////////////////////////////////////////////////////////////////////////////////////////
// This task is used to control all NM behaviour tasks (used to be the old CTaskNMComplex).
////////////////////////////////////////////////////////////////////////////////////////////
class CTaskNMControl : public CTaskFSMClone
{
	friend class CTaskNMBehaviour;

public:

	struct Tunables : CTuning
	{
		Tunables();

		struct DriveToGetup
		{
			bool m_AllowDriveToGetup;
			bool m_OnlyAllowForShot;
			bool m_AllowWhenBalanced;
			float m_MinHealth;
			float m_MaxSpeed;
			float m_MaxUprightRatio;
			u32 m_MatchTimer;

			PAR_SIMPLE_PARSABLE;
		};

		DriveToGetup m_DriveToGetup;
		CNmTuningSet m_OnEnableDriveToGetup;
		CNmTuningSet m_OnDisableDriveToGetup;

		PAR_PARSABLE;
	};

	enum
	{
		State_Start = 0,
		State_ControllingTask,
		State_DecidingOnNextTask,
		State_Finish
	};

	enum eNMControlFlags
	{
		ALL_FLAGS_CLEAR						= 0,
		// It may not be desirable to allow a blend back to clip. By clearing the DO_BLEND_FROM_NM flag
		// a task can cause the blend to be skipped if, for example, the ped was killed and shouldn't
		// get up.
		DO_BLEND_FROM_NM					= BIT(0),
		// This flag is checked before calling the main NM behaviour task's start, control and finish methods.
		// If set, we use the clip pose helper class defined below to play an authored clip on a masked subset
		// of the ped's skeleton while the main NM task controls the rest of the ragdoll.
		DO_CLIP_POSE						= BIT(1),
		// Use this when the behavior is already running and you don't want to re-run it
		ALREADY_RUNNING						= BIT(2),
		// Tells the control task that it has switched to an underwater behavior already
		//IN_WATER_INITD					= BIT(3),
		// Tells the control task that it is running on the motion task tree, rather than the main one
		// can be used to stop any child tasks from forcing motion task tree and hence deleting themselves.
		ON_MOTION_TASK_TREE					= BIT(4),

		// clone task flags :

		// Used when the task is running as a clone task, set when the ped is forced into a relax to get him on the ground.
		// This happens when the master ped has fallen but the clone is still standing.
		DO_RELAX							= BIT(5),
		DOING_RELAX							= BIT(6),
		// Only used in clone tasks to force the ped to drop
		FORCE_FALL_OVER						= BIT(7),

		// When true, the ped is in the process of driving to the getup pose
		DRIVING_TO_GETUP_POSE				= BIT(8),
		// set this to disable driving to the getup pose
		BLOCK_DRIVE_TO_GETUP				= BIT(9),
		// ONLY set this when aborting the nm control task for another ragdoll task
		ABORTING_FOR_RAGDOLL_EVENT			= BIT(10),
		// can be set by tasks who want to move on to a high fall task in order to
		// block the optional quick getup.
		BLOCK_QUICK_GETUP_ON_HIGH_FALL		= BIT(11),

		// Un-hide the weapon object
		// We hide the weapon object for ragdolling player's with 
		// two-handed weapons equipped when starting NM so this flag lets us know we need 
		// to un-hide the weapon object
		EQUIPPED_WEAPON_OBJECT_VISIBLE		= BIT(12),

		// ONLY set this when clone task is being aborted and replaced with a local task, or vice versa
		CLONE_LOCAL_SWITCH					= BIT(13),

		NUM_SYNCED_CONTROL_FLAGS			= 1, // only sync DO_BLEND_FROM_NM 
	};

	// Definitions of the NM behaviour feedback flags.
	enum eNMFeedbackFlags
	{
		ALL_FEEDBACK_FLAGS_CLEAR = 0,
		BALANCE_FAILURE = BIT(0),
		BALANCE_STARTED = BIT(1)
	};

	enum eNMHandPose
	{
		HAND_POSE_NONE = 0,
		HAND_POSE_LOOSE,
		HAND_POSE_HOLD_WEAPON,
		HAND_POSE_GRAB,
		HAND_POSE_BRACED,
		HAND_POSE_FLAIL,
		HAND_POSE_IMPACT,
		NUM_HAND_POSES		
	};

	CTaskNMControl(u32 nMinTime, u32 nMaxTime, aiTask* pForceFirstSubTask, u32 nNMControlFlags, float fDamageTaken=0.0f);
	~CTaskNMControl();

#if !__NO_OUTPUT
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;
#endif //!__NO_OUTPUT

	virtual aiTask* Copy() const;

	// Accessors for the NM control flags.
public:
	u32 GetFlags() const {return m_nFlags;}
	void SetFlag(u32 nFlagToSet) {m_nFlags |= nFlagToSet;}
	void ClearFlag(u32 nFlagToClear) { m_nFlags &= ~nFlagToClear;}
	void SetDontSwitchToAnimatedOnAbort(bool b)
	{
		if (GetSubTask() && GetSubTask()->IsNMBehaviourTask())
		{
			if (b)
			{
				smart_cast<CTaskNMBehaviour*>(GetSubTask())->SetFlag(CTaskNMBehaviour::DONT_SWITCH_TO_ANIMATED_ON_ABORT);
			}
			else
			{
				smart_cast<CTaskNMBehaviour*>(GetSubTask())->ClearFlag(CTaskNMBehaviour::DONT_SWITCH_TO_ANIMATED_ON_ABORT);
			}
		}
	}

	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	virtual bool MakeAbortable( const AbortPriority iPriority, const aiEvent* pEvent);
	void CleanUp();

	// used by the dead task to determine if it's safe to  
	bool IsDoingRelax()
	{
		if (GetSubTask())
		{
			if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_RELAX)
			{
				return true;
			}
			else if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_RAGE_RAGDOLL)
			{
				return true;
			}
		}
		return false;
	}

	virtual bool HandlesRagdoll(const CPed* pPed) const 
	{ 
		if (m_ForceNextSubTask.GetTask())
			return true;
		if (GetSubTask())
			return GetSubTask()->HandlesRagdoll(pPed);
		else
			return true; 
	}

#if !__FINAL
	virtual atString GetName() const;
#endif //!__FINAL

	void SendStartMessages(CPed* pPed)
	{
		if (m_ForceNextSubTask.GetTask() && ((CTask*)m_ForceNextSubTask.GetTask())->IsNMBehaviourTask())
		{
			CTaskNMBehaviour* pBehaviourTask = static_cast<CTaskNMBehaviour*>(m_ForceNextSubTask.GetTask());
			pBehaviourTask->SendStartMessages(pPed);
		}
	}

	// Accessors for the NM behaviour feedback flags (which are read-only -- can be set and cleared by the behaviour tasks
	// because they are "friend" classes).
public:
	u32 GetFeedbackFlags() {return m_nFeedbackFlags;}
	bool IsFeedbackFlagSet(u32 nQueryFlags) const { return (m_nFeedbackFlags & nQueryFlags) != 0; }
private:
	void SetFeedbackFlags(CPed* pPed, u32 nFeedbackFlagsToSet);
	void ClearFeedbackFlag(u32 nFlagToClear) {m_nFeedbackFlags &= ~nFlagToClear;}

public:
	CClipPoseHelper& GetClipPoseHelper() {return m_clipPoseHelper;}

	int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_CONTROL;}

	void SwitchClonePedToRagdoll(CPed* pPed);

	////////////////////////////
	// CTask functions:
protected:
	FSM_Return ProcessPreFSM();
	FSM_Return ProcessPostFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32 GetDefaultStateAfterAbort() const {return State_Finish;}
	virtual bool MayDeleteOnAbort() const {return true;} 	// This task doesn't resume if it gets aborted, should be safe to delete.
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_ControllingTask",
			"State_DecidingOnNextTask",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

	void ProcessWeaponHiding(CPed* pPed);

	// Add debug draw objects to the control class so that they persist better across different NM behaviour tasks.
#if DEBUG_DRAW
public:
	void AddDebugLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0)
	{
		ms_debugDraw.AddLine(vStart, vEnd, colour, uExpiryTime, uKey);
	}
	void AddDebugSphere(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0)
	{
		ms_debugDraw.AddSphere(vPos, fRadius, colour, uExpiryTime, uKey);
	}
#endif //DEBUG_DRAW

protected:

	// Clone task implementation
	// When cloned, this task continually syncs to the remote state
	virtual bool                OverridesNetworkBlender(CPed *pPed);
	virtual	bool				OverridesNetworkHeadingBlender(CPed* pPed);
	virtual bool				OverridesVehicleState() const	{ return true; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool				IsInScope(const CPed* pPed);
	bool						HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo);
	bool						CanRetainLocalTaskOnFailedSwitch() { return true; }
	virtual bool                RequiresTaskInfoToMigrate() const { return false; }
	virtual bool                IgnoresCloneTaskPriorities() const { return true; }

	void						HandleDriveToGetup(CPed* pPed);
	void						ClearDriveToGetup(CPed* pPed);
public:
	CTaskFSMClone*				CreateTaskForClonePed(CPed *pPed);
	CTaskFSMClone*				CreateTaskForLocalPed(CPed *pPed);

    void                        WarpCloneRagdollingPed(CPed *pPed, const Vector3 &newPosition);
public:
	// this is just used by the task counter - uses const pointer
	const aiTask* GetForcedSubTask() const {return m_ForceNextSubTask.GetTask();}
	void SetDamageDone(float fDamage) {if(fDamage > m_fDamageTaken) m_fDamageTaken = fDamage;}

	static CTask*	FindBackgroundAiTask(CPed* pPed);
	static void		CleanupUnhandledRagdoll(CPed* pPed);
	static bool		IsValidNMControlSubTask(const CTask* pSubTask);

    void SetGetGrabParametersCallback(fnGetGrabParamsCallback pGetGrabParamsCallback, aiTask *pTask);
    void ClearGetGrabParametersCallback();

	void AlwaysAllowControlPassing() { m_alwaysAllowControlPassing = true; }

	void ResetStartTime();

	static float GetCNCRagdollDurationModifier();

private:
	//////////////////////////////////////////////
	// Helper functions for FSM state management:
	void       Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);
	FSM_Return Start_OnUpdateClone(CPed* pPed);
	void       ControllingTask_OnEnter(CPed* pPed);
	FSM_Return ControllingTask_OnUpdate(CPed* pPed);
	void       ControllingTask_OnExit(CPed* pPed);
	FSM_Return ControllingTask_OnUpdateClone(CPed* pPed);
	void       DecidingOnNextTask_OnEnter(CPed* pPed);
	void       DecidingOnNextTask_OnEnterClone(CPed* pPed);
	FSM_Return DecidingOnNextTask_OnUpdate(CPed* pPed);
	FSM_Return DecidingOnNextTask_OnUpdateClone(CPed* pPed);
	void       Finish_OnEnterClone(CPed* pPed);

	aiTask* CreateNewNMTask(const int iSubTaskType, CPed *pPed);
	aiTask* CreateNewNMTaskClone(CPed *pPed);

public:

	void ForceNewSubTask(aiTask* pNewSubTask);

	// PURPOSE: Sets the requested animation hand pose on the provided hand
	// PARAMS:
	//	pose - the pose to apply
	//	pedHand - the hand to apply the pose to
	void SetCurrentHandPose(eNMHandPose pose, CMovePed::ePedHand pedHand, float blendDuration=NORMAL_BLEND_DURATION);
	eNMHandPose GetCurrentHandPose(CMovePed::ePedHand pedHand) { return m_CurrentHandPoses[pedHand]; }

private:

	// This helper class is used to start an "clipPose" behaviour along with the main NM behaviour task.
	CClipPoseHelper m_clipPoseHelper;

	u32 m_nMinTime;
	u32 m_nMaxTime;
	u32 m_nStartTime;
	u32 m_nFlags; 
	u32 m_nFeedbackFlags;

	eNMHandPose m_CurrentHandPoses[CMovePed::kNumHands];

	u32 m_nDriveToGetupMatchTimer;
	eNmBlendOutSet m_DriveToGetupMatchedBlendOutSet;
	CNmBlendOutPoseItem* m_pDriveToGetupMatchedBlendOutPoseItem;
	CAITarget m_DriveToGetupTarget;
	RegdTask m_pDriveToGetupMoveTask;

    // callback and parameter for setting up grab parameters for a clone NM task launched by another AI task
    RegdaiTask              m_pTask;
    fnGetGrabParamsCallback m_pGetGrabParamsCallback;

public:
#if DEBUG_DRAW
	static bool m_bDisplayFlags;
#endif //DEBUG_DRAW
	static u32 m_DebugFlags; // For debugging only. 

	static bool ms_bTeeterEnabled;

	static Tunables sm_Tunables;

	
private:

	class CNextSubTask
	{
	public:
		CNextSubTask(aiTask* pNextSubTask) : m_pNextSubTask(pNextSubTask) {}
		~CNextSubTask() { if(m_pNextSubTask) delete m_pNextSubTask; }
		void SetTask(aiTask* pNextSubTask) { if(m_pNextSubTask) delete m_pNextSubTask; m_pNextSubTask = pNextSubTask; }
		const aiTask* GetTask() const { return m_pNextSubTask; }
		aiTask* GetTask() { return m_pNextSubTask; }
		aiTask* RelinquishTask() { aiTask* pTask = m_pNextSubTask; m_pNextSubTask = NULL; return pTask; }
	private:
		RegdaiTask m_pNextSubTask;
	};
	CNextSubTask m_ForceNextSubTask;

	float m_fDamageTaken;

	// used by clone task:
	s32 m_currentTask;
	s32 m_nextTask;
	bool m_waitForNextTask;
	bool m_bHasAborted;
	bool m_bGrabbed2Handed;
	bool m_bCloneTaskFinished;
	u16 m_randomSeed;
	bool m_alwaysAllowControlPassing;
};


//
// Task info for CTaskNMControl
//
class CClonedNMControlInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMControlInfo(s32 nmTaskType, u32 nControlFlags, bool bHasFallen, u16 randomSeed);
	CClonedNMControlInfo();
	~CClonedNMControlInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_CONTROL;}

	s32		GetNMTaskType() const		{ return m_nNMTaskType == -1 ? m_nNMTaskType : m_nNMTaskType+CTaskTypes::TASK_NM_RELAX; }
	bool	GetHasFallen() const		{ return m_bHasFallen; }
	u16		GetRandomSeed() const		{ return m_randomSeed; }

	// an NM control task running a generic attach is not networked as this will be handled by the animated attach task
	virtual bool IsNetworked() const	{ return m_nNMTaskType+CTaskTypes::TASK_NM_RELAX != CTaskTypes::TASK_NM_GENERIC_ATTACH; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 controlFlags = (u32)m_nControlFlags;
		s32 nmTaskType = (s32)m_nNMTaskType;

		SERIALISE_UNSIGNED(serialiser, m_randomSeed, SIZEOF_RANDOM_SEED, "Random Seed");
		SERIALISE_INTEGER(serialiser, nmTaskType, SIZEOF_TASK_TYPE, "Task type");
		SERIALISE_UNSIGNED(serialiser, controlFlags, SIZEOF_CONTROL_FLAGS, "Control flags");
		SERIALISE_BOOL(serialiser, m_bHasFallen, "Has fallen");

		m_nControlFlags = (u8)controlFlags;
		m_nNMTaskType = (s8)nmTaskType;
	}

private:

	CClonedNMControlInfo(const CClonedNMControlInfo &);
	CClonedNMControlInfo &operator=(const CClonedNMControlInfo &);

	static const unsigned int SIZEOF_RANDOM_SEED	= 16;
	static const unsigned int NUM_NM_TASKS			= CTaskTypes::TASK_RAGDOLL_LAST-CTaskTypes::TASK_NM_RELAX;
	static const unsigned int SIZEOF_TASK_TYPE		= datBitsNeeded<NUM_NM_TASKS>::COUNT + 1;
	static const unsigned int SIZEOF_CONTROL_FLAGS	= CTaskNMControl::NUM_SYNCED_CONTROL_FLAGS;

	u16	 m_randomSeed;
	s8	m_nNMTaskType; 
	u8  m_nControlFlags;
	bool m_bHasFallen;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline CTaskNMControl* CTaskNMBehaviour::GetControlTask()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssert(GetParent());
	taskAssertf(dynamic_cast<CTaskNMControl*>(GetParent()), "Parent task should be an NM Control task");
	return smart_cast<CTaskNMControl*>(GetParent());
}

#endif // !INC_TASKNM_H_
