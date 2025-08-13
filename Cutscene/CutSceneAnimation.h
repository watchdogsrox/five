
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneAnimation.h
// PURPOSE : Class for handling cut scene anims that are not handled by the 
//			 in Anim blender/ anim player system. 
// AUTHOR  : Thomas French
// STARTED : 03/11 /2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_ANIMATION_H
#define CUTSCENE_ANIMATION_H

//Rage includes
#include "crclip/clip.h"
#include "paging/ref.h"

//Game includes
#include "vector/quaternion.h"

/////////////////////////////////////////////////////////////////////////////////

class CCutsceneAnimation
{
public:
	CCutsceneAnimation();

	~CCutsceneAnimation();

	void Init(const crClip* pClip, float fStartTime);

	static bool GetQuaternionValue(const crClip* pClip, s32 iTrack, const float fPhase, Quaternion &Qval, u16 Id = 0);  

	static bool GetVector3Value(const crClip* pClip, s32 iTrack, const float fPhase, Vector3 &vVec, u16 Id = 0);

	static bool GetFloatValue(const crClip* pClip, s32 iTrack, const float fPhase, float &fFloat, u16 Id = 0);

	float GetPhase(float fCurrentTime) const;

	float GetTime(float fPhase) const;

	void SetStartTime(float fStartTime) { m_fStartTime = fStartTime; }

	float GetStartTime() const { return m_fStartTime; }

	void Remove(); 

	//Gets the pointer to our anim
	const crClip* GetClip() const { return m_pClip; } 
	
	const crClip* GetClip() { return m_pClip; }
private:

	float m_fStartTime; 

	pgRef<const crClip> m_pClip;
};

#endif