/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/FocusEntity.h
// PURPOSE : manages access to the current focus entity or position, for streaming collision etc
// AUTHOR :  Ian Kiigan
// CREATED : 06/07/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_FOCUSENTITY_H_
#define _SCENE_FOCUSENTITY_H_

#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "vector/vector3.h"

#if __BANK
#include "atl/array.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#endif

#define FOCUSENTITY_USE_FOLLOWPLAYER	(1)

class CFocusEntityMgr
{
public:
	enum eFocusState
	{
		FOCUS_DEFAULT_PLAYERPED,
		FOCUS_OVERRIDE_ENTITY,
		FOCUS_OVERRIDE_POS
	};

	CFocusEntityMgr() : m_eFocusState(FOCUS_DEFAULT_PLAYERPED) {}

	static void Init(u32 UNUSED_PARAM(initMode)) { GetMgr().SetDefault(); }
	void SetDefault(bool bCheckForRestore=false) { m_eFocusState=FOCUS_DEFAULT_PLAYERPED; if (bCheckForRestore) { RestoreSavedOverrideEntity(); } }
	void SetEntity(const CEntity& entity) { m_eFocusState=FOCUS_OVERRIDE_ENTITY; m_overrideEntity=&entity; }
	void SetPosAndVel(const Vector3& pos, const Vector3& vel) { m_eFocusState=FOCUS_OVERRIDE_POS; m_overridePos=pos; m_overrideVel=vel; }
	void SetPopPosAndVel(const Vector3& pos, const Vector3& vel) { m_overridenPop = true; m_overridePopPos = pos; m_overridePopVel = vel; }
	void ClearPopPosAndVel() { m_overridenPop = false; }
	bool HasOverridenPop() const { return m_overridenPop; }
	eFocusState GetFocusState() const { return m_eFocusState; }

	bool IsPlayerPed() const { return m_eFocusState==FOCUS_DEFAULT_PLAYERPED; }
	Vector3 GetPos();
	Vector3 GetVel();
	Vector3 GetPopPos() { return m_overridenPop ? m_overridePopPos : GetPos(); }
	Vector3 GetPopVel() { return m_overridenPop ? m_overridePopVel : GetVel(); }
	const CEntity* GetEntity() const;

	void RestoreSavedOverrideEntity()
	{
		if (m_savedOverrideEntity)
		{
			SetEntity(*m_savedOverrideEntity);
			m_savedOverrideEntity = NULL;
		}
	}
	void SetSavedOverrideEntity(const CEntity& entity)
	{
		m_savedOverrideEntity = &entity;
	}

	static inline CFocusEntityMgr& GetMgr() { return ms_focusMgr; }

private:
	eFocusState m_eFocusState;
	Vector3 m_overridePos;
	Vector3 m_overrideVel;
	Vector3 m_overridePopPos;
	Vector3 m_overridePopVel;

	RegdConstEnt m_overrideEntity;
	RegdConstEnt m_savedOverrideEntity;

    bool    m_overridenPop;
	static CFocusEntityMgr ms_focusMgr;

#if __BANK
public:
	void			InitWidgets(bkBank& bank);
	void			UpdateDebug();

private:
	static void		SetPickerEntityAsFocusCB();
	static void		SetCameraPosAsFocusCB();
	static void		ResetFocusCB();
	static bool		ms_bDebugDisplayCurrentPos;
	static bool		ms_bSetCameraFocusEveryFrame;
#endif	//__BANK
};

#endif	//_SCENE_FOCUSENTITY_H_
