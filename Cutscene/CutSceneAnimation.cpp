/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsSceneLightObject.cpp
// PURPOSE : Describes an in game light.
// AUTHOR  : Thomas French
// STARTED : 1 / 10 / 2009
//
/////////////////////////////////////////////////////////////////////////////////

//Rage includes
#include "cranimation/frame.h"
#include "fwanimation/directorcomponentcreature.h"

//Game includes
#include "CutScene/CutSceneAnimation.h"
#include "Cutscene/cutscene_channel.h"
#include "vectormath/legacyconvert.h"
/////////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS ()

/////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////

CCutsceneAnimation::CCutsceneAnimation()
{
	m_pClip = NULL;
	m_fStartTime = 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////////

CCutsceneAnimation::~CCutsceneAnimation()
{
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
		m_pClip = NULL; 
	}

}

/////////////////////////////////////////////////////////////////////////////////////
 
void CCutsceneAnimation::Init(const crClip* pClip, float StartTime)
{
	if(pClip != NULL)
	{
		pClip->AddRef(); 
	}

	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
	}
	
	m_pClip = pClip; 

	m_fStartTime = StartTime;
}

void CCutsceneAnimation::Remove()
{
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
	}
	m_pClip = NULL; 
}
/////////////////////////////////////////////////////////////////////////////////////

float CCutsceneAnimation::GetPhase(float fCurrentTime) const
{
	float fPhase = 0.0f;

	if (m_pClip)	
	{
		fPhase = Clamp(m_pClip->ConvertTimeToPhase((fCurrentTime - m_fStartTime)), 0.0f, 1.0f);
	}
	
	return fPhase;
}

/////////////////////////////////////////////////////////////////////////////////////

float CCutsceneAnimation::GetTime(float fPhase) const
{
	float fTime = m_fStartTime;

	if (m_pClip)	
	{
		fTime += m_pClip->ConvertPhaseToTime(fPhase);
	}

	return fTime;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CCutsceneAnimation::GetQuaternionValue(const crClip* pClip, s32 iTrack, const float fPhase, Quaternion &QValue, u16 Id)
{
	if (pClip)
	{
		//Note: The validity of m_Animation is checked higher in the call stack.
		float time = pClip->ConvertPhaseToTime(fPhase);
		time = pClip->CalcBlockedTime(time); 
		crFrameDataSingleDof frameData((u8)iTrack, Id, kFormatTypeQuaternion);
		crFrameSingleDof frame(frameData);
		frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());
		pClip->Composite(frame, time);
		if(frame.GetValue<QuatV>(RC_QUATV(QValue)))
		{
			return true;
		}
		else
		{
			return false; 
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CCutsceneAnimation::GetVector3Value(const crClip* pClip, s32 iTrack, const float fPhase, Vector3 &vVecValue, u16 Id)
{
	if (pClip)
	{
		float time = pClip->ConvertPhaseToTime(fPhase);
		time = pClip->CalcBlockedTime(time); 
		crFrameDataSingleDof frameData((u8)iTrack, Id, kFormatTypeVector3);
		crFrameSingleDof frame(frameData);
		frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());
		pClip->Composite(frame, time);
		if(frame.GetValue<Vec3V>(RC_VEC3V(vVecValue)))
		{
			return true;
		}
		else
		{
			return false; 
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CCutsceneAnimation::GetFloatValue(const crClip* pClip, s32 iTrack, const float fPhase, float &fFloatVal, u16 Id)
{	
	if (pClip)
	{
		float time = pClip->ConvertPhaseToTime(fPhase);
		time = pClip->CalcBlockedTime(time); 
		crFrameDataSingleDof frameData((u8)iTrack, Id, kFormatTypeFloat);
		crFrameSingleDof frame(frameData);
		frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());
		pClip->Composite(frame, time);
		if(frame.GetValue<float>(fFloatVal))
		{
			return true;
		}
		else
		{
			return false; 
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
