#include "debugwanted.h"
#include "physics/debugEvents.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "system/timemgr.h"

#if DR_ENABLED
using namespace rage;
using namespace debugPlayback;

static bool sb_IsRecording = false;
static int siFramesToWait=0;
static int siFramesBetweenCaptures = 30;

struct MatrixStruct
{
	Matrix34 m_Data;
	PAR_SIMPLE_PARSABLE;
};


void FindAndDrawSkeletonForInst(const debugPlayback::phInstId &rInst, Color32 col, const debugPlayback::Frame *pFrame, const debugPlayback::EventBase *pFromEvent=0)
{
	if (pFrame)
	{
		const debugPlayback::EventBase *pEvent = pFrame->mp_LastEvent;	//Start at back as skeletons are generally stored at the end of the frame
		while (pEvent)
		{
			if (StoreSkeleton::IsSkeletonEvent(pEvent))
			{
				const StoreSkeleton *pSkeletonEvent = (const StoreSkeleton*)pEvent;
				if (pSkeletonEvent->m_rInst == rInst)
				{
					pSkeletonEvent->CustomDebugDraw3d(col, col);
					break;
				}
			}
			pEvent = pEvent->mp_Prev;
		}
	}
	else if (pFromEvent)
	{
		while (pFromEvent)
		{
			if (StoreSkeleton::IsSkeletonEvent(pFromEvent))
			{
				const StoreSkeleton *pSkeletonEvent = (const StoreSkeleton*)pFromEvent;
				if (pSkeletonEvent->m_rInst == rInst)
				{
					pSkeletonEvent->CustomDebugDraw3d(col, col);
					break;
				}
			}
			pFromEvent = pFromEvent->mp_Next;
		}
	}
}

static u32 s_iLastDrawnFrame;
struct DrewItStruct
{
	debugPlayback::phInstId m_rInst;
	u32 m_iFrame;
};
static atFixedArray<DrewItStruct, 64> drewItList;

bool DidDrawThisFrame(debugPlayback::phInstId inst, u32 frame)
{
	for (int i=0 ; i<drewItList.GetCount() ; i++)
	{
		if (	(drewItList[i].m_rInst == inst)
			&&	(drewItList[i].m_iFrame == frame)
			)
		{
			return true;
		}
	}
	return false;
}

bool DrawItThisFrame(debugPlayback::phInstId inst, u32 frame)
{
	if (s_iLastDrawnFrame != TIME.GetFrameCount())
	{
		drewItList.Reset();
	}

	if (DidDrawThisFrame(inst, frame))
	{
		return false;	//Already drawn
	}

	if (drewItList.GetCount() < drewItList.GetMaxCount())
	{
		DrewItStruct &rNew = drewItList.Append();
		rNew.m_rInst = inst;
		rNew.m_iFrame = frame;
	}
	return true;
}

struct LawEntityEvent : public PhysicsEvent
{
	Vec3V m_TargetPos;
	atArray<atHashString> m_PrimaryTaskStack;
	atArray<atHashString> m_PrimaryTaskSubState;
	Mat34V *mp_VehicleMatrix;
	phInstId m_TargetId;
	phInstId m_VehicleId;
	u32 m_FrameIndex;
	bool m_IsClone;
	bool m_HasTarget;

	virtual bool IsInstInvolved(phInstId rInst) const
	{
		return (m_TargetId == rInst) ||  (m_VehicleId == rInst) ||  debugPlayback::PhysicsEvent::IsInstInvolved(rInst);
	}

	virtual void CustomDebugDraw3d(eDrawFlags drawFlags, Color32 col, bool bDrawTarget) const
	{
		if (m_HasTarget && bDrawTarget)
		{
			Color32 col2(col);
			//Draw the target
			col2.SetRed(255);
			if (drawFlags & eDrawSelected)
			{
				col2.SetRed(128);
			}
			col2.SetAlpha(128);
			DrawVelocity(m_TargetPos - m_Pos, m_Pos, ScalarV(V_ONE), col2, true);
		}

		if (m_VehicleId.IsValid() && mp_VehicleMatrix)
		{
			grcColor(col);

			debugPlayback::DebugRecorder::ModifyAlpha(col);

			if (DrawItThisFrame(m_VehicleId, m_FrameIndex))
			{
#if __PFDRAW
				m_VehicleId->GetArchetype()->GetBound()->Draw(*mp_VehicleMatrix);
#endif
				grcWorldIdentity();
			}
			
			if (drawFlags & eDrawSelected)
			{
				//Mark the position of this ped so its obvious (we didn't store the skeleton)
				Matrix34 mat(RCC_MATRIX34(*mp_VehicleMatrix));
				mat.d = RCC_VECTOR3(m_Pos);
				grcDrawAxis(1.0f, mat);
			}
		}
		else
		{
			if (DrawItThisFrame(m_rInst, m_FrameIndex))
			{
				FindAndDrawSkeletonForInst(m_rInst, col, 0, this);	//TMS: How to do this??
			}
		}

		grcWorldIdentity();
	}

	virtual void DebugDraw3d(eDrawFlags drawFlags) const
	{
		Color32 col(255,0,0,255);
		if (drawFlags & eDrawSelected)
		{
			col.SetRed(0);
			col.SetGreen(255);
		}
		if (drawFlags & eDrawHovered)
		{
			col.SetGreen(255);
		}

		CustomDebugDraw3d(drawFlags, col, true);
	}

	void DebugText(TextOutputVisitor &rText) const
	{
		rText.AddLine("m_TargetId: 0x%x", m_TargetId.m_AsU32);
		rText.AddLine("m_VehicleId: 0x%x", m_VehicleId.m_AsU32);
		if (mp_VehicleMatrix)
		{
			rText.PushIndent();
			rText.AddLine("Col0: <%f,%f,%f>", VEC3V_ARGS(mp_VehicleMatrix->GetCol0()));
			rText.AddLine("Col1: <%f,%f,%f>", VEC3V_ARGS(mp_VehicleMatrix->GetCol1()));
			rText.AddLine("Col2: <%f,%f,%f>", VEC3V_ARGS(mp_VehicleMatrix->GetCol2()));
			rText.AddLine("Col3: <%f,%f,%f>", VEC3V_ARGS(mp_VehicleMatrix->GetCol3()));
			rText.PopIndent();
		}
		rText.AddLine("Tasks");
		rText.PushIndent();
		for (int i=0 ; i<m_PrimaryTaskStack.GetCount() ; i++)
		{
			rText.AddLine("%s (state: %s)", m_PrimaryTaskStack[i].GetCStr(), m_PrimaryTaskSubState[i].GetCStr());
		}
		rText.PopIndent();
	}

	void SetData(CPed &rLaw, CEntity *pTarget)
	{	
		DR_MEMORY_HEAP();

		m_FrameIndex = TIME.GetFrameCount();
		m_rInst.Set(rLaw.GetCurrentPhysicsInst());
		m_Pos = rLaw.GetMatrix().GetCol3();

		m_IsClone = rLaw.IsNetworkClone();
		if (rLaw.GetIsInVehicle())
		{
			CVehicle *pVehicle = rLaw.GetVehiclePedInside();
			if(pVehicle)
			{
				m_VehicleId.Set(pVehicle->GetCurrentPhysicsInst());

				//Makes sure we can draw it in the replay
				RecordInst(m_VehicleId);

				//Allocate the vehicle matrix
				mp_VehicleMatrix = rage_new Mat34V;
				*mp_VehicleMatrix = pVehicle->GetMatrix();
			}
		}
		else
		{
			//Makes sure we can draw it in the replay
			RecordInst(m_rInst);
		}

		//Get task stack, somehow...
		aiTask *pTask = rLaw.GetPedIntelligence()->GetTaskActive();
		aiTask *pTaskCounter = pTask;
		int iCount = 0;
		while (pTaskCounter)
		{
			iCount++;
			pTaskCounter = pTaskCounter->GetSubTask();
		}

		if (iCount)
		{
			m_PrimaryTaskStack.Resize(iCount);
			m_PrimaryTaskSubState.Resize(iCount);
			while(pTask)
			{
				--iCount;
				m_PrimaryTaskStack[ iCount ] = pTask->GetTaskName();
				m_PrimaryTaskSubState[ iCount ] = pTask->GetStateName( pTask->GetState() );
				pTask = pTask->GetSubTask();
			}
		}

		//And lastly record the stack
		m_HasTarget = false;
		if (pTarget)
		{
			m_TargetId.Set(pTarget->GetCurrentPhysicsInst());
			m_TargetPos = pTarget->GetMatrix().GetCol3();
			m_HasTarget = true;


			//Makes sure we can draw it in the replay
			RecordInst(m_TargetId);
		}
	}

	LawEntityEvent()
		:mp_VehicleMatrix(0)
		,m_FrameIndex(0)
	{	}

	~LawEntityEvent()
	{
		delete mp_VehicleMatrix;
	}

	PAR_PARSABLE;
	DR_EVENT_DECL(LawEntityEvent)
};

struct LawUpdateEvent : public debugPlayback::EventBase
{
	int m_iWantedLevel;
	LawUpdateEvent()
		:m_iWantedLevel(0)
	{

	}
	PAR_PARSABLE;
	DR_EVENT_DECL(LawUpdateEvent)
};

DR_EVENT_IMP(LawEntityEvent);
DR_EVENT_IMP(LawUpdateEvent)

namespace debugWanted
{
	void StartWantedRecording(int iFrameStep)
	{
		sb_IsRecording = true;
		siFramesBetweenCaptures = iFrameStep;
		siFramesToWait = 0;
	}

	void StopWantedRecording()
	{
		sb_IsRecording = false;
	}

	void RecordLawEnforcementEntity(CPed &rLaw)
	{
		LawEntityEvent *pEvent = AddEvent<LawEntityEvent>();
		if (pEvent)
		{
			CEntity *pTarget = 0;
			if( rLaw.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{
				pTarget = rLaw.GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
			}

			pEvent->SetData(rLaw, pTarget);
		}
	}

	void UpdateRecording(int iWantedLevel)
	{
		if (!sb_IsRecording || (iWantedLevel==0))
		{
			return;
		}
	
		if (siFramesToWait > 0)
		{
			--siFramesToWait;
			return;
		}

		//Record the basic state
		LawUpdateEvent *pLUE = rage::debugPlayback::AddEvent<LawUpdateEvent>();
		if (pLUE)
		{
			pLUE->m_iWantedLevel = iWantedLevel;
		}

		//Record law entities
		CPed::Pool *pPedPool = CPed::GetPool();
		s32 i=pPedPool->GetSize();
		while(i--)
		{
			CPed *pPed = pPedPool->GetSlot(i);

			if (pPed && (pPed->GetPedType() == PEDTYPE_COP || pPed->GetPedType() == PEDTYPE_SWAT) && !pPed->GetIsDeadOrDying())
			{
				RecordLawEnforcementEntity(*pPed);
			}
		}

		siFramesToWait = siFramesBetweenCaptures;
	}

	void DrawHistoryForLawEntity(const LawEntityEvent &rSelectedEvent, const debugPlayback::Frame &rPriorFrame, Color32 col, int alpaDecrease)
	{
		Vector3 vPrevPos = RCC_VECTOR3(rSelectedEvent.m_Pos);
		const debugPlayback::Frame *pPriorFrame = &rPriorFrame;
		while (pPriorFrame && (col.GetAlpha() > 0))
		{
			const debugPlayback::EventBase *pPriorEvent = pPriorFrame->mp_FirstEvent;
			while (pPriorEvent)
			{
				if (pPriorEvent->GetEventSubType() == LawEntityEvent::GetStaticEventType())
				{
					const LawEntityEvent *pPriorLawEvent = (const LawEntityEvent*)pPriorEvent;
					if (pPriorLawEvent->m_rInst == rSelectedEvent.m_rInst)
					{
						pPriorLawEvent->CustomDebugDraw3d(debugPlayback::eDrawNormal, col, false);

						grcDrawLine(vPrevPos, RCC_VECTOR3(pPriorLawEvent->m_Pos), col, col);
						vPrevPos = RCC_VECTOR3(pPriorLawEvent->m_Pos);
						if (col.GetAlpha() >= alpaDecrease)
						{
							col.SetAlpha(col.GetAlpha() - alpaDecrease);
						}
						else
						{
							col.SetAlpha(0);
						}

						break;	//Assume only one entity event per frame
					}
				}
				pPriorEvent = pPriorEvent->mp_Next;
			}
			pPriorFrame = pPriorFrame->mp_Prev;
		}
	}

	void OnDebugDraw3d(const debugPlayback::Frame * /*pSelectedFrame*/, const debugPlayback::EventBase *pSelectedEvent, const debugPlayback::EventBase *pHoveredEvent)
	{
		//
		if (!sb_IsRecording)
			return;

		//Do we have a law entity event selected?
		//If so draw some context....
		if (pSelectedEvent && (pSelectedEvent->GetEventSubType() == LawEntityEvent::GetStaticEventType()))
		{
			const debugPlayback::Frame *pSameFrame = DebugRecorder::GetInstance()->FindFrame( pSelectedEvent );
			if (!pSameFrame)
			{
				return;
			}
			
			//Draw other entities this frame
			const debugPlayback::EventBase *pSameFrameEvent = pSameFrame->mp_FirstEvent;
			Color32 col(100,100,150,160);
			while (pSameFrameEvent)
			{
				if (pSameFrameEvent != pSelectedEvent)
				{
					if (pSameFrameEvent->GetEventSubType() == LawEntityEvent::GetStaticEventType())
					{
						const LawEntityEvent *pSameFrameLawEvent = (const LawEntityEvent*)pSameFrameEvent;
						pSameFrameLawEvent->CustomDebugDraw3d(debugPlayback::eDrawNormal, col, false);
					}
				}
				pSameFrameEvent = pSameFrameEvent->mp_Next;
			}

			//Now draw history for entity
			if (pSameFrame->mp_Prev)
			{
				Color32 col2(0,128,0,200);
				DrawHistoryForLawEntity(*(const LawEntityEvent *)pSelectedEvent, *pSameFrame->mp_Prev, col2, 20);
			}
		}

		if(pHoveredEvent && (pHoveredEvent->GetEventSubType() == LawEntityEvent::GetStaticEventType()))
		{
			//Frame drawing will draw all the skeletons etc, we need to draw the history for the hovered event
			const debugPlayback::Frame *pSameFrame = DebugRecorder::GetInstance()->FindFrame( pHoveredEvent );
			if (pSameFrame && pSameFrame->mp_Prev)
			{
				Color32 col(128,128,0,128);

				const LawEntityEvent *pHoveredLawEvent = (const LawEntityEvent*)pHoveredEvent;
				//Draw history of hovered entity type
				DrawHistoryForLawEntity(*pHoveredLawEvent, *pSameFrame->mp_Prev, col, 20);
			}
		}
	}
}

//Included last to get all the declarations within the CPP file
#include "debugwanted_parser.h"

#endif //DR_ENABLED


