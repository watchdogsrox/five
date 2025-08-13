/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/FocusEntity.h
// PURPOSE : manages access to the current focus entity or position, for streaming collision etc
// AUTHOR :  Ian Kiigan
// CREATED : 06/07/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/FocusEntity.h"

#include "peds/Ped.h"
#include "scene/world/GameWorld.h"

#if __BANK
#include "camera/CamInterface.h"
#include "fwdebug/debugdraw.h"
#include "fwdebug/picker.h"

bool CFocusEntityMgr::ms_bDebugDisplayCurrentPos = false;
bool CFocusEntityMgr::ms_bSetCameraFocusEveryFrame = false;
#endif	//__BANK

CFocusEntityMgr CFocusEntityMgr::ms_focusMgr;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetPos
// PURPOSE:		returns the current focus position, based on current state
//////////////////////////////////////////////////////////////////////////
Vector3 CFocusEntityMgr::GetPos()
{
	// find focus entity
	const CEntity* pEntity = GetEntity();
	if (m_eFocusState==FOCUS_OVERRIDE_ENTITY && !pEntity)
	{
		Assertf(false, "FocusEntity: current focus entity no longer exists, reverting to player ped");
		SetDefault();
		pEntity = GetEntity();
	}
	
	// return position
	Vector3 vPos;
	if (pEntity)
	{
		vPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	}
	else
	{
		Assertf(m_eFocusState==FOCUS_OVERRIDE_POS, "FocusEntity: no valid entity or position!");
		vPos = m_overridePos;
	}
	return vPos;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetVel
// PURPOSE:		returns velocity of current focus entity if available, or the overridden velocity, or zero vector
//////////////////////////////////////////////////////////////////////////
Vector3 CFocusEntityMgr::GetVel()
{
	switch (m_eFocusState)
	{
	case FOCUS_DEFAULT_PLAYERPED:
#if FOCUSENTITY_USE_FOLLOWPLAYER
		return CGameWorld::FindFollowPlayerSpeed();
#else
		return CGameWorld::FindLocalPlayerSpeed();
#endif
	case FOCUS_OVERRIDE_ENTITY:
		{
			const CEntity* pEntity = GetEntity();
			if (pEntity)
			{
				const phCollider* pCollider = pEntity->GetCollider();
				if (pCollider)
				{
					return RCC_VECTOR3(pCollider->GetVelocity());
				}
			}
		}
		break;
	case FOCUS_OVERRIDE_POS:
		return m_overrideVel;
	}
	return VEC3_ZERO;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetEntity
// PURPOSE:		returns current focus entity, if there is one
//////////////////////////////////////////////////////////////////////////
const CEntity* CFocusEntityMgr::GetEntity() const
{
	switch (m_eFocusState)
	{
	case FOCUS_DEFAULT_PLAYERPED:
#if FOCUSENTITY_USE_FOLLOWPLAYER
		return CGameWorld::FindFollowPlayer();
#else
		return CGameWorld::FindLocalPlayer();
#endif
	case FOCUS_OVERRIDE_ENTITY:
		return m_overrideEntity;
	case FOCUS_OVERRIDE_POS:
	default:
		return NULL;
	}
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		add debug widgets
//////////////////////////////////////////////////////////////////////////
void CFocusEntityMgr::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Focus Entity");
	bank.AddToggle("Display current focus", &ms_bDebugDisplayCurrentPos);
	bank.AddButton("Reset focus to player ped", datCallback(ResetFocusCB));
	bank.AddButton("Set picker entity as focus", datCallback(SetPickerEntityAsFocusCB));
	bank.AddButton("Set camera pos and vel as focus", datCallback(SetCameraPosAsFocusCB));
	bank.AddToggle("Set camera focus every frame", &ms_bSetCameraFocusEveryFrame);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		prints debug info about current focus position
//////////////////////////////////////////////////////////////////////////
void CFocusEntityMgr::UpdateDebug()
{
	if (ms_bDebugDisplayCurrentPos)
	{
		static const char* apszFocusTypes[] =
		{
			"FOCUS_DEFAULT_PLAYERPED",
			"FOCUS_OVERRIDE_ENTITY",
			"FOCUS_OVERRIDE_POS"
		};
		Vector3 vPos = GetPos();
		Vector3 vVel = GetVel();
		const CEntity* pEntity = GetEntity();
		grcDebugDraw::AddDebugOutput(
			"FocusEntity Mode: %s: Name: %s Position: <%f, %f, %f> ",
			apszFocusTypes[m_eFocusState],
			(pEntity ? pEntity->GetModelName() : "n/a"),
			vPos.x, vPos.y, vPos.z
		);
		grcDebugDraw::AddDebugOutput("FocusEntity Velocity: <%f, %f, %f> ", vVel.x, vVel.y, vVel.z);
		grcDebugDraw::AddDebugOutput("Override Pop: %d Override Pop Pos: <%f, %f, %f>", m_overridenPop, m_overridePopPos.x, m_overridePopPos.y, m_overridePopPos.z) ;
	}

	if (ms_bSetCameraFocusEveryFrame)
	{
		SetCameraPosAsFocusCB();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetPickerEntityAsFocusCB
// PURPOSE:		takes an entity ptr from picker and uses that as focus entity
//////////////////////////////////////////////////////////////////////////
void CFocusEntityMgr::SetPickerEntityAsFocusCB()
{
	CEntity* pEntity = smart_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEntity)
	{
		GetMgr().SetEntity(*pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetCamerPosAsFocusCB
// PURPOSE:		takes the current camera position and uses that as focus pos
//////////////////////////////////////////////////////////////////////////
void CFocusEntityMgr::SetCameraPosAsFocusCB()
{
	GetMgr().SetPosAndVel(camInterface::GetPos(), camInterface::GetVelocity());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ResetFocusCB
// PURPOSE:		resets the focus to player ped
//////////////////////////////////////////////////////////////////////////
void CFocusEntityMgr::ResetFocusCB()
{
	GetMgr().SetDefault();
}
#endif	//__BANK