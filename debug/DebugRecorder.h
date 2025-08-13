#ifndef INC_DEBUGRECORDER_H_
#define INC_DEBUGRECORDER_H_

//Game
#include "DebugAnimation.h"
#include "scene/RegdRefTypes.h"
#include "Network\Debug\NetworkDebugMsgs.h"

//RAGE
#include "parser/macros.h"
#include "physics/debugEvents.h"
#include "physics/debugPlayback.h"

#if DR_ENABLED
class DebugReplayObjectInfo : public debugPlayback::NewObjectData 
{
	Vec3V m_vBBMax;
	Vec3V m_vBBMin;
	DebugReplayObjectInfo *mp_NextInChain;
	const void *mp_UserData;		//TMS: Mainly as a sanity check, remove when sane
	u32 m_ClassType;
	int m_ChainIndex;				//Multiple objects linked together in a chain, -1 == not in chain
	ConstString m_ModelName;
	bool m_bIsPlayer;
	bool m_bPlayerIsDriving;
	static atArray<DebugReplayObjectInfo*> sm_ChainStarts;
	static atArray<debugPlayback::phInstId> sm_PendingChainListFrom;
	static atArray<debugPlayback::phInstId> sm_PendingChainListTo;
	static void GetHighestChainIndexCB(debugPlayback::DataBase& rData);
	static void RebuildChainsFromScratchCB(debugPlayback::DataBase& rData);
public:
	static RegdEnt m_rReplayObject;
	DebugReplayObjectInfo(phInst *pInst);
	DebugReplayObjectInfo();
	void DrawBox(Mat34V &rMat, Color32 col) const;
	static void UpdateChains();
	const DebugReplayObjectInfo *GetFirstInChain() const
	{
		if (m_ChainIndex >= 0)
		{
			return sm_ChainStarts[ m_ChainIndex ];
		}
		return this;
	}
	const DebugReplayObjectInfo *GetNextInChain() const
	{
		return mp_NextInChain;
	}
	static void RebuildChainsFromScratch();
	const void *GetUserDataPtr() const {return mp_UserData;}
	static void ClearReplayObject();
	void GetBoundingMinMax(Vec3V &vMin_Out, Vec3V &vMax_Out) const
	{
		vMin_Out = m_vBBMin;
		vMax_Out = m_vBBMax;
	}
	phInst *CreateObject(debugPlayback::TextOutputVisitor &rText, Mat34V &initialMatrix, bool bInvolvePlayer);
	void SetNextInst(const phInst *pNew);
	const char *GetObjectDescription(char *pBuffer, int iBuffersize) const;
	const char *GetModelName() const{ return m_ModelName.c_str(); }
	static NewObjectData *CreateNewObjectData(phInst *pInst);

	PAR_PARSABLE;
};



class GameRecorder : public debugPlayback::DebugRecorder
{
	Vec3V m_vTeleportLocation;
	msgSyncDRDisplay m_LastSyncMsg;
	const debugPlayback::Frame *mp_ReplayFrame;
	int m_iLastModeSelected;
	int m_iDrawnFrameNetTime;
	bool m_bSyncNetDisplays;
	bool m_bSelectMode;
	bool m_bPauseOnEventReplay;
	bool m_bDestroyReplayObject;
	bool m_bCreateObjectForReplay;
	bool m_bInvolvePlayer;
	bool m_bHasTeleportLocation;
	bool m_bStoreDataThisFrame;
	atMap<size_t, StoredSkelData*> m_StoredSkeletonData;
	atMap<const crCreature*, const phInst *> m_CreatureToInstMap;	//Contains links between peds and creatures
	const phInst *m_CachedCreatureLookupInst;
	const crCreature *m_CachedCreatureLookupCreature;
	enum {kMaxInstsPerFrame = 128};
	static atFixedArray<debugPlayback::phInstId, kMaxInstsPerFrame> m_FrameInsts;
	static atFixedArray<debugPlayback::phInstId, kMaxInstsPerFrame> m_LastFrameInsts;
	bool m_bHighlightSelected;
	bool m_bShowHelpText;
	bool m_bUseScriptCallstack;
	bool m_bReplayCamera;
	bool m_bPosePedsIfPossible;
	bool m_bRecordSkeletonsEveryFrame;
	static void NewInstFunc(debugPlayback::phInstId rInst);
	static void RecordEntityAttachment(const phInst *pAttached, const phInst *pAttachedTo, const Matrix34 &matNew);
	virtual bool AddAppFilters(debugPlayback::OnScreenOutput &/*rOutput*/, debugPlayback::OnScreenOutput &/*rInfoText*/, int &iFilterIndex, bool /*bMouseDown*/);
	virtual bool AppOptions(debugPlayback::TextOutputVisitor &rText, debugPlayback::TextOutputVisitor &rInfoText, bool bMouseDown, bool bPaused);
	virtual void OnStartRecording();
	virtual void OnStartDebugDraw();
	virtual void OnEndDebugDraw(bool bMenuItemSelected);
	virtual void OnClearRecording();
	virtual void OnDebugDraw3d(const debugPlayback::EventBase * /*pHovered*/);
	virtual void OnDrawFrame(const debugPlayback::Frame *pFrame);
	virtual void Update();
	virtual void PreUpdate();
	virtual void OnEventSelection(const debugPlayback::EventBase *pEvent);
	virtual size_t RegisterCallstack(int iIgnorelevels);
	virtual void OnNewDataSelection();

	virtual DebugRecorder::DebugRecorderData *CreateSaveData() const
	{
		return rage_new AppLevelSaveData;
	}

	virtual void OnSave(DebugRecorder::DebugRecorderData & rData);
	virtual void OnLoad(DebugRecorder::DebugRecorderData & rData);

	bool LoadFilter(const char *pFilerFileName);
	bool SaveFilter(const char *pFilerFileName);
	void ClearFilter();

public:
	struct FilterInfo
	{
		atArray<ConstString> m_Filters;
		bool m_bPosePedsIfPossible;
		bool m_bRecordSkeletonsEveryFrame;
		bool m_bUseLowLodSkeletons;
		FilterInfo()
			:m_bPosePedsIfPossible(false)
			,m_bRecordSkeletonsEveryFrame(false)
			,m_bUseLowLodSkeletons(false)
		{

		}

		PAR_SIMPLE_PARSABLE;
	};

	struct AppLevelSaveData : public debugPlayback::DebugRecorder::DebugRecorderData
	{
		atArray<StoredSkelData *> m_StoredSkeletonData;
		atArray<size_t> m_StoredSkeletonDataIDs;

		virtual void DeleteTempIOMemory()
		{
			m_StoredSkeletonData.Reset();
			m_StoredSkeletonDataIDs.Reset();
			debugPlayback::DebugRecorder::DebugRecorderData::DeleteTempIOMemory();
		}

		AppLevelSaveData()
		{
		}

		virtual ~AppLevelSaveData()
		{
			m_StoredSkeletonData.Reset();
			m_StoredSkeletonDataIDs.Reset();
		}

		PAR_PARSABLE;
	};
	void SetUseScriptCallstack(bool bUseIt)
	{
		m_bUseScriptCallstack = bUseIt;
	}
	//Find out if another instance was previously used for the same object
	bool DoInstsShareAnEntity(debugPlayback::phInstId rCurrentInst, debugPlayback::phInstId rOtherInst) const;
	static GameRecorder* GetAppLevelInstance() { return (GameRecorder*)GetInstance(); }
	const DebugReplayObjectInfo* GetObjectInfo(debugPlayback::phInstId rInst) const;
	const phInst *GetInstFromCreature(const crCreature *pCreature);
	bool HasSkelData(u32 iSig);
	StoredSkelData *GetSkelData(u32 iSig);
	void AddSkelData(StoredSkelData *pData, u32 iSig);
	void ReposePedIfPossible(fwEntity *pEntity);
	bool PosePedsIfPossible() const { return m_bPosePedsIfPossible; }
	void Init();
	GameRecorder();
	virtual ~GameRecorder();
	
	PAR_PARSABLE;
};

namespace rage
{
	namespace debugPlayback
	{
		struct CameraEvent : public debugPlayback::EventBase
		{
			CameraEvent();
			CameraEvent(const debugPlayback::CallstackHelper rCallstack);
			virtual void DebugDraw3d(debugPlayback::eDrawFlags drawFlags) const;
			bool Replay(phInst *) const;

			Vec3V m_vCamPos;
			Vec3V m_vCamUp;
			Vec3V m_vCamFront;
			float m_fFarClip;
			float m_fTanFOV;
			float m_fTanVFOV;

			PAR_PARSABLE;
			DR_EVENT_DECL(CameraEvent);
		};

		struct PedCapsuleInfoEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(debugPlayback::eDrawFlags drawFlags) const;

			void SetData(Vec3V_In start, Vec3V_In end, Vec3V_In ground, float radius, Vec3V_In offset)
			{
				m_Start = start;
				m_End = end;
				m_Ground = ground;
				m_Offset = offset;
				m_Radius = radius;
			}

			PedCapsuleInfoEvent(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
			{
			}

			PedCapsuleInfoEvent() : m_Start(V_ZERO), m_End(V_ZERO), m_Ground(V_ZERO), m_Offset(V_ZERO), m_Radius(0.0f) {}

			Vec3V m_Start;
			Vec3V m_End;
			Vec3V m_Ground;
			Vec3V m_Offset;
			float m_Radius;

			PAR_PARSABLE;
			DR_EVENT_DECL(PedCapsuleInfoEvent)
		};

		void RecordQuadrupedSpringForce(const phInst *pInst, Vec3V_In pos, Vec3V_In force, Vec3V_In damping, float fMaxForce, float fCompression);
		void RecordPedSpringForce(const phInst *pInst, Vec3V_In force, Vec3V_In groundNormal, Vec3V_In groundVelocity, float fSpringCompression, float fSpringDamping);
		void RecordPedGroundInstance(const phInst *pInst, const phInst *pGroundInst, int iComponent, Vec3V_In extent, float fMinSize, int iPassedAxisCount, float fSpeedDiff, float fSpeedAllowance);
		void RecordEntityAttachment(const phInst *attached, const phInst *attachedTo, const Matrix34 &rNew);
		void RecordVehicleObstructionArray(const phInst *pVehicleInst, const void* AvoidanceInfoArray, int iArraySize, float fVehicleDriveOrientation, Vec3V_In vRight, Vec3V_In vVehForwards);
		void RecordPedDesiredVelocity(const phInst *pInst, Vec3V_In linear, Vec3V_In angular);
		void RecordPedCapsuleInfo(const phInst *pInst, Vec3V_In start, Vec3V_In end, Vec3V_In ground, float radius, Vec3V_In boundOffset);
		void RecordPedAnimatedVelocity(const phInst *pInst, Vec3V_In linear);
		void RecordBlenderState(const phInst *pInst, Mat34V_In lastReceivedMatrix, bool positionUpdated, bool orientationUpdated, bool velocityUpdated, bool angVelocityUpdated);
		void RecordPedFlag(const phInst *pInst, const char *pFlagName, bool bFlag);
		void SetUseScriptCallstack(bool bUseIt);
		void SetVisualSkeletonIfPossible(fwEntity *pEntity);
		void CurrentInstChange(phInstId rOld, const phInst *pNew);
		const phInst *GetInstFromCreature(const crCreature *pCreature);
		void TrackScriptGetsEntityForMod(fwEntity *pEntity);
		void SyncToNet(const msgSyncDRDisplay &rMsg);
	}
}
#endif //DR_ENABLED
#endif //INC_DEBUGRECORDER_H_