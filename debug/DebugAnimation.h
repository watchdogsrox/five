#ifndef DEBUGANIMATIONS_H
#define DEBUGANIMATIONS_H

#include "physics/debugplayback.h"
#include "physics/debugevents.h"

#if DR_ENABLED
#define DR_ANIMATIONS_ENABLED 1
#define DR_ANIMATIONS_ENABLED_ONLY(x) x

#include "parser/macros.h"
#include "math/float16.h"
#include "move/move_state.h"
#include "vectormath/vec3v.h"
#include "vectormath/quatv.h"

namespace rage
{
	class fwEntity;
	class phInst;
	class crCreature;
	class crFrameFilter;
	class mvNetwork;
	class mvNodeStateBaseDef;
	class fwMoveDumpNetwork;
}

extern void UpdateAnimEventRecordingState();

#if __DEV || __BANK
struct DR_TrackMoveInterface : public mvDRTrackInterface
{
	virtual void RecordGetAnimValue(const phInst *pInst, const char *pValueName, float fValue) const;
	virtual void RecordSetAnimValue(const phInst *pInst, const char *pValueName, float fValue) const;
	virtual void RecordSetAnimValue(const phInst *pInst, const char *pStringName, 
		const char *pString1id, const char *pString1, 
		const char *pString2id=0, const char *pString2=0) const;

	virtual void RecordCondition(const mvNetworkDef* networkDef, const crCreature* creature, int iPassed, int iConditionType, int iConditionIndex, const mvNodeStateBaseDef* /*from*/, const mvNodeStateBaseDef* to);
	virtual void RecordEndConditions() const;
	virtual void RecordTransition(const mvNetworkDef* networkDef, const crCreature* creature, const mvNodeStateBaseDef* stateMachine, const mvNodeStateBaseDef* fromState, const mvNodeStateBaseDef* toState);
	DR_TrackMoveInterface(){}
	virtual ~DR_TrackMoveInterface(){}
};
#endif

struct StoredSkelData
{
	struct BoneLink
	{ 
		unsigned short m_iBone0; 
		unsigned short m_iBone1;
		PAR_SIMPLE_PARSABLE;
	};
	atArray<BoneLink> m_BoneLinks;
	atArray<u16> m_BoneIndexToFilteredIndex;
	atArray<u16> m_FilteredIndexToBoneIndex;
	u32 m_SkelSignature;
	void Create(const crSkeletonData &rData, crFrameFilter *pFilter);
	PAR_SIMPLE_PARSABLE;
};

struct StoreSkeleton : public debugPlayback::PhysicsEvent
{
	struct f16Vector
	{
		Float16 m_X;
		Float16 m_Y;
		Float16 m_Z;
		void Set(Vec3V_In vSet)
		{
			m_X.SetFloat16_FromFloat32( vSet.GetXf() );
			m_Y.SetFloat16_FromFloat32( vSet.GetYf() );
			m_Z.SetFloat16_FromFloat32( vSet.GetZf() );
		}
		Vec3V_Out Get() const
		{
			return Vec3V(m_X.GetFloat32_FromFloat16(), m_Y.GetFloat32_FromFloat16(), m_Z.GetFloat32_FromFloat16());
		}
		PAR_SIMPLE_PARSABLE;
	};

	struct u32Quaternion
	{
		u32 m_uPackedData;
		void Set(QuatV_In r);
		QuatV_Out Get() const;
		PAR_SIMPLE_PARSABLE;
	};

	atHashString m_SkeletonName;
	u32 m_DataSignature;
	u32 m_iColor;
	atArray<f16Vector> m_BonePositions;
	atArray<u32Quaternion> m_BoneRotations;
	static bool ms_bUseLowLodSkeletons;
	virtual void DebugDraw3d(debugPlayback::eDrawFlags drawFlags) const;
	virtual bool Replay(phInst *) const;
	const char *GetEventSubType() const { return m_SkeletonName.GetCStr(); }
	bool DebugEventText(debugPlayback::TextOutputVisitor &rOutput) const;
	void CustomDebugDraw3d(Color32 colLine, Color32 colJoint) const;
	static bool IsSkeletonEvent(const debugPlayback::EventBase *pEvent);
	StoreSkeleton(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst, const fwEntity *pEntity, const char *pSkeletonName, Color32 color);
	StoreSkeleton()
		:m_DataSignature(0)
	{	}

	PAR_PARSABLE;
};

struct StoreSkeletonPreUpdate : public StoreSkeleton
{
	StoreSkeletonPreUpdate(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst, const fwEntity *pEntity, const char *pSkeletonName, Color32 color)
		:StoreSkeleton(rCallstack, pInst, pEntity, pSkeletonName, color)
	{	}
	StoreSkeletonPreUpdate()
	{	}
	DR_EVENT_DECL(StoreSkeletonPreUpdate)
	PAR_PARSABLE;
};
struct StoreSkeletonPostUpdate : public StoreSkeleton
{
	StoreSkeletonPostUpdate(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst, const fwEntity *pEntity, const char *pSkeletonName, Color32 color)
		:StoreSkeleton(rCallstack, pInst, pEntity, pSkeletonName, color)
	{	}
	StoreSkeletonPostUpdate()
	{	}
	DR_EVENT_DECL(StoreSkeletonPostUpdate)
	PAR_PARSABLE;
};

struct AnimMotionTreeTracker : public debugPlayback::PhysicsEvent
{
	fwMoveDumpNetwork *m_DumpNetwork;
	atHashString m_Label;
	static int sm_LineOffset;
	static u32 sm_iHoveredFrame;

	void PrintToTTY() const;
	void AddEventOptions(const debugPlayback::Frame &frame, debugPlayback::TextOutputVisitor &rText, bool &bMouseDown) const;
	bool DebugEventText(debugPlayback::TextOutputVisitor &rOutput) const;
	const char *GetEventSubType() const { return m_Label.GetCStr(); }
	void Init();
	AnimMotionTreeTracker(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst, const char *pLabel)
		:debugPlayback::PhysicsEvent(rCallstack, pInst)
		,m_DumpNetwork(0)
		,m_Label(pLabel)
	{
		Init();
	}
	AnimMotionTreeTracker()
		:m_DumpNetwork(0)
	{	}
	~AnimMotionTreeTracker();
	PAR_PARSABLE;
	DR_EVENT_DECL(AnimMotionTreeTracker)
};
#else
#define DR_ANIMATIONS_ENABLED 0
#define DR_ANIMATIONS_ENABLED_ONLY(x)
#endif //DR_ENABLED && __DEV

#endif //DEBUGANIMATIONS_H
