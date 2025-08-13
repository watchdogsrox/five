// Filename   :	AnimPoseHelper.h
// Description:	Helper class embedded in CTaskNMControl to play an authored anim on part of a ragdoll
//              while NM controls the rest (FSM version)

#ifndef INC_ANIMPOSEHELPER_H_
#define INC_ANIMPOSEHELPER_H_

// --- Include Files ------------------------------------------------------------

#include "optimisations.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/NmDefines.h"
// ------------------------------------------------------------------------------

class CClipPoseHelper
{
public:
	CClipPoseHelper();
	virtual ~CClipPoseHelper();

	// Functions to configure the clipPose behaviour: //////////////////
	void SelectClip(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId);
	void SelectClip(s32 nClipDictionaryIndex, u32 nClipNameHash);
	void SelectClip(const strStreamingObjectName pClipDictName, const char *pClipName);

	void SetAnimPoseType(u32 eAnimPoseTypeHash) { m_eAnimPoseTypeHash = eAnimPoseTypeHash; }
	u32 GetAnimPoseType() const { return m_eAnimPoseTypeHash; }

	void SetAttachParent(CPhysical *pAttachParent) { m_pAttachParent = pAttachParent; }

	void SetEffectorMask(eEffectorMaskStrings nEffectorMask);
	void TaskSetsStiffnessValues(bool bValue);
	////////////////////////////////////////////////////////////////////

	fwMvClipSetId GetClipSet() {return m_clipSetId;}
	fwMvClipId GetClipId() {return m_clipId;}

	bool GetNeedToStreamClips() {return !m_bClipsStreamed;}

	// Use CClipRequestHelper to simplify streaming of the clip. Clip must have been preselected
	// using SelectClip().
	bool RequestClips();

	void SetConnectedLeftHand(int set) { m_connectedLeftHand = set; }
	void SetConnectedRightHand(int set) { m_connectedRightHand = set; }
	void SetConnectedLeftFoot(int set) { m_connectedLeftFoot = set; }
	void SetConnectedRightFoot(int set) { m_connectedRightFoot = set; }

	////////////////////////////////////
	// CTaskNMBehaviour-like functions (although this class does NOT inherit from CTaskNMBehaviour)
public:
	virtual void StartBehaviour(CPed *pPed);
	virtual void ControlBehaviour(CPed *pPed);
	virtual bool FinishConditions(CPed *pPed);

	////////////////////////////
	// Local functions and data

public:
	bool StartClip(CPed* pPed);
	void StopClip(float fBlendOutDelta);

	bool Enable(CPed* pPed);
	void Disable(CPed* pPed);

#if __BANK
	void ReadRagWidgets(float& sfAPmuscleStiffness, float& sfAPstiffness, float& sfAPdamping);
	void InitClipWeights(const CPed *pPed);
	static bool ms_bDebugClipMatch;
	static float ms_fClipMatchAcc; // Accuracy of the NM controlled ragdoll compared with the input clip.
	static bool ms_bLockClipFirstFrame;
	static bool ms_bUseWorldCoords;
#endif //__BANK

protected:

	void SetClip(const crClip* pClip)
	{
		if (pClip)
		{
			pClip->AddRef();
		}

		if (m_pClip)
		{
			m_pClip->Release();
		}

		m_pClip = pClip;
	}

	float GetPhase()
	{
		if (m_pClip)
		{
			float phase = m_pClip->ConvertTimeToPhase(((float)(fwTimer::GetTimeInMilliseconds() - m_ClipStartTime)/1000.0f));
			if (!m_bLooped)
			{
				return Min(phase, 1.0f);
			}
			else
			{
				float temp=0.0f;
				return Modf(phase, &temp);
			}
		}
		return 0.0f;
	}

private:
	// Use this helper class to simplify loading of the required clips into memory.
	fwClipSetRequestHelper m_ClipSetRequestHelper;

	// The clip to play.
	fwMvClipSetId	m_clipSetId;
	fwMvClipId	m_clipId;

	RegdPhy m_pAttachParent;

	pgRef<const crClip> m_pClip;
	u32 m_ClipStartTime;
	bool m_bLooped;

	// Have valid clips been streamed in yet?
	bool m_bClipsStreamed;

	bool m_bTaskSetsStiffnessValues;

	// Alternate way of describing which clip to play.
	s32 m_nClipDictionaryIndex;
	u32 m_nClipNameHash;

	eEffectorMaskStrings m_nEffectorMask;

	u32 m_eAnimPoseTypeHash;

	int m_connectedLeftHand;
	int m_connectedRightHand;
	int m_connectedLeftFoot;
	int m_connectedRightFoot;

#if __BANK
public:
#endif // __BANK
	static float m_sfAPmuscleStiffness;
	static float m_sfAPstiffness;
	static float m_sfAPdamping;
};

#endif // !INC_ANIMPOSEHELPER_H_
