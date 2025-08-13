// This file is only ever compiled in __DEV builds
#if !__FINAL

// File header
#include "Task/Debug/TaskDebug.h"

// Game headers
#include "Peds/Ped.h"
#include "Task/Default/TaskPlayer.h"

// Disables optimisations if active
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskTestClipDistance
//////////////////////////////////////////////////////////////////////////

CTaskTestClipDistance::FSM_Return CTaskTestClipDistance::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				m_vPedPosAtStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				static float BLEND = NORMAL_BLEND_IN_DELTA;
				StartClipBySetAndClip(pPed, m_clipSetId, m_clipId, BLEND);
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::AnimFinished))
				{
					Vector3 vMoved(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vPedPosAtStart);
					Displayf("TestClipDistance Moved: (%.2f,%.2f,%.2f), Dist: (%.2f)\n", vMoved.x, vMoved.y, vMoved.z, vMoved.Mag());

					Vector3 vClipDist = fwAnimHelpers::GetMoverTrackTranslationDiff(m_clipSetId, m_clipId, 0.f, 1.f);
					vClipDist = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vClipDist)));

					Displayf("TestClipDistance Clip:  (%.2f,%.2f,%.2f), Dist: (%.2f)\n", vClipDist.x, vClipDist.y, vClipDist.z, vClipDist.Mag());

					return FSM_Quit;
				}
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


#endif //!__FINAL
