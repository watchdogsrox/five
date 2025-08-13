//=====================================================================================================
// name:		ParticleMiscFxPacket.cpp
//=====================================================================================================

#include "ParticleMiscFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/replay.h"
#include "control/replay/replay_channel.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "control/trafficlights.h"
#include "Objects/object.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/VfxHelper.h"
#include "scene/Physical.h"
#include "scene/FocusEntity.h"
#include "script/Handlers/GameScriptResources.h"

#include "vfx/ptfx/ptfxscript.h"
#include "vfx/systems/vfxscript.h"
#include "vfx/ptfx/ptfxreg.h"
#include "Vfx/Systems/VfxPed.h"
#include "vfx/ptfx/ptfxwrapper.h"

atArray<u32>	CPacketRequestNamedPtfxAsset::m_requestedNamedHashes;

//========================================================================================================================
void CPacketMiscGlassGroundFx::StorePos(const Vector3& rPos)
{
	m_Position[0] = rPos.x;
	m_Position[1] = rPos.y;
	m_Position[2] = rPos.z;
}

//========================================================================================================================
void CPacketMiscGlassGroundFx::LoadPos(Vector3& rPos) const
{
	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
CPacketMiscGlassGroundFx::CPacketMiscGlassGroundFx(u32 uPfxHash, const Vector3& rPos)
	: CPacketEvent(PACKETID_GLASSGROUNDFX, sizeof(CPacketMiscGlassGroundFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePos(rPos);
}

//========================================================================================================================
void CPacketMiscGlassGroundFx::Extract(const CEventInfo<void>&) const
{
	Vector3 oVecPos;
	LoadPos(oVecPos);

	if(ptfxHistory::Check(m_pfxHash, 0, NULL, VECTOR3_TO_VEC3V(oVecPos), 0.5f))
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		pFxInst->SetBasePos(VECTOR3_TO_VEC3V(oVecPos));
		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: MISC GLASS GROUND FX: \n");
#endif

		ptfxHistory::Add(m_pfxHash, 0, NULL, VECTOR3_TO_VEC3V(oVecPos), 0.5f);
	}
}


//========================================================================================================================
void CPacketMiscGlassSmashFx::StorePos(const Vector3& rPos)
{
	m_Position[0] = rPos.x;
	m_Position[1] = rPos.y;
	m_Position[2] = rPos.z;
}

//========================================================================================================================
void CPacketMiscGlassSmashFx::LoadPos(Vector3& rPos) const
{
	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
void CPacketMiscGlassSmashFx::StoreNormal(const Vector3& rNormal)
{
	const float SCALAR = 127.0f;

	m_Normal[0] = (s8)(rNormal.x * SCALAR);
	m_Normal[1] = (s8)(rNormal.y * SCALAR);
	m_Normal[2] = (s8)(rNormal.z * SCALAR);
}

//========================================================================================================================
void CPacketMiscGlassSmashFx::LoadNormal(Vector3& rNormal) const
{
	const float SCALAR = 1.0f / 127.0f;

	rNormal.x = m_Normal[0] * SCALAR;
	rNormal.y = m_Normal[1] * SCALAR;
	rNormal.z = m_Normal[2] * SCALAR;
}

//========================================================================================================================
CPacketMiscGlassSmashFx::CPacketMiscGlassSmashFx(u32 uPfxHash, const Vector3& rPos, const Vector3& rNormal, float fForce)
	: CPacketEvent(PACKETID_GLASSSMASHFX, sizeof(CPacketMiscGlassSmashFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePos(rPos);
	StoreNormal(rNormal);

	m_fForce = fForce;
}

//========================================================================================================================
void CPacketMiscGlassSmashFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	Vector3 oVecPos, oVecNormal;
	LoadPos(oVecPos);
	LoadNormal(oVecNormal);

	// glass smash
	if (m_pfxHash == ATSTRINGHASH("imp_glass_smash", 0x575C3D4C))
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
		if (pFxInst)
		{
			Mat34V fxMat;
			CVfxHelper::CreateMatFromVecZ(fxMat, VECTOR3_TO_VEC3V(oVecPos), VECTOR3_TO_VEC3V(oVecNormal));

			// make the fx matrix relative to the entity
			Mat34V invEntityMat = pEntity->GetMatrix();
			Invert3x3Full(invEntityMat, invEntityMat);

			Mat34V relFxMat = fxMat;
			Transform(relFxMat, invEntityMat);

			pFxInst->SetOffsetMtx(relFxMat);

			ptfxAttach::Attach(pFxInst, pEntity, -1);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force",0x2b56b5f7), m_fForce);

			pFxInst->Start();
		}
	}
	// window smash
	else if (m_pfxHash == ATSTRINGHASH("imp_side_window_smash", 0x67683EBC))
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
		if (pFxInst)
		{
			Mat34V fxMat;
			CVfxHelper::CreateMatFromVecZ(fxMat, VECTOR3_TO_VEC3V(oVecPos), VECTOR3_TO_VEC3V(oVecNormal));

			// make the fx matrix relative to the entity
			Mat34V invEntityMat = pEntity->GetMatrix();
			Invert3x3Full(invEntityMat, invEntityMat);

			Mat34V relFxMat = fxMat;
			Transform(relFxMat, invEntityMat);

			pFxInst->SetOffsetMtx(relFxMat);

			ptfxAttach::Attach(pFxInst, pEntity, -1);

			// add collision set
			pFxInst->ClearCollisionSet();
			/*TODO4FIVE g_ptFxManager.GetColnInterface().Register(pFxInst, pEntity);*/

			pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(((CPhysical*)pEntity)->GetVelocity()));
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force",0x2b56b5f7), m_fForce);

			pFxInst->Start();
		}
	}
	// windscreen smash
	else if (m_pfxHash == ATSTRINGHASH("imp_windscreen_smash", 0x9BDC06BA))
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
		if (pFxInst)
		{
			Mat34V fxMat;
			CVfxHelper::CreateMatFromVecZ(fxMat, VECTOR3_TO_VEC3V(oVecPos), VECTOR3_TO_VEC3V(oVecNormal));

			// make the fx matrix relative to the entity
			Mat34V invEntityMat = pEntity->GetMatrix();
			Invert3x3Full(invEntityMat, invEntityMat);

			Mat34V relFxMat = fxMat;
			Transform(relFxMat, invEntityMat);

			pFxInst->SetOffsetMtx(relFxMat);

			ptfxAttach::Attach(pFxInst, pEntity, -1);

			// add collision set
			pFxInst->ClearCollisionSet();
			/*TODO4FIVE g_ptFxManager.GetColnInterface().Register(pFxInst, pEntity);*/

			pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(((CPhysical*)pEntity)->GetVelocity()));
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force",0x2b56b5f7), m_fForce);

			pFxInst->Start();
		}
	}
	else
	{
		replayAssertf(false, "Invalid Hash in CPacketMiscGlassSmashFx::Extract");
	}

#if DEBUG_REPLAY_PFX
	replayDebugf1("REPLAY: MISC GLASS SMASH FX: %0X: ID: %0X\n", (u32)pEntity, pEntity->GetReplayID());
#endif

}


//////////////////////////////////////////////////////////////////////////
CPacketTrafficLight::CPacketTrafficLight(const char* trafficLightPrev, const char* trafficLightNext)
	: CPacketEvent(PACKETID_TRAFFICLIGHTOVERRIDE, sizeof(CPacketTrafficLight))
	, CPacketEntityInfo()
{
	for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
	{
		m_trafficLightPrev[i] = trafficLightPrev[i];
		m_trafficLightNext[i] = trafficLightNext[i];
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketTrafficLight::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	if(!pObject)
		return;
	replayAssert(CTrafficLights::IsMITrafficLight(pObject->GetBaseModelInfo()));

	ReplayTrafficLightExtension* pExt = ReplayTrafficLightExtension::GetExtension(pObject);
	REPLAY_CHECK(pExt, NO_RETURN_VALUE, "Failed to get Traffic Light Extension");

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		pExt->SetTrafficLightCommands(m_trafficLightPrev);
	}
	else
	{
		pExt->SetTrafficLightCommands(m_trafficLightNext);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketTrafficLight::PrintXML(FileHandle handle) const
{
	CPacketEvent::PrintXML(handle);

	char str[1024];
	for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
	{
		snprintf(str, 1024, "\t\t<TrafficLight%d>%u->%u</TrafficLight%d>\n", i, m_trafficLightPrev[i], m_trafficLightNext[i], i);
		CFileMgr::Write(handle, str, istrlen(str));
	}
}

tPacketVersion gPacketStartScriptPtFxPacketVersion_v1 = 1;
tPacketVersion gPacketStartScriptPtFxPacketVersion_v2 = 2;
CPacketStartScriptPtFx::CPacketStartScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes, bool hasColour, u8 r, u8 g, u8 b)
	: CPacketEventTracked(PACKETID_STARTSCRIPTFX, sizeof(CPacketStartScriptPtFx), gPacketStartScriptPtFxPacketVersion_v2)
	, CPacketEntityInfoStaticAware()
{
	m_PFxNameHash = pFxNameHash;
	m_PFxAssetHash = pFxAssetHash;
	m_BoneTag = boneTag;
	m_Position.Store(fxPos);
	m_Rotation.Store(fxRot);
	m_Scale = scale;
	m_InvertAxes = invertAxes;
	
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(pFxAssetHash);
	if (replayVerifyf(slot.IsValid(), "CPacketStartScriptPtFx - cannot find particle asset slot"))
	{
		m_Slot = slot.Get();
	}

	m_hasColour = hasColour;
	m_r = r;
	m_g = g;
	m_b = b;
}


//////////////////////////////////////////////////////////////////////////
void CPacketStartScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( eventInfo.m_pEffect[0] != 0 && ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
		{
			ptfxScript::Stop((int)eventInfo.m_pEffect[0]);
		}

		eventInfo.m_pEffect[0] = 0;
		return;
	}

	CEntity* pEntity = eventInfo.pEntity[0];

	Vector3 fxPos;
	m_Position.Load(fxPos);

	Vector3 fxRot;
	m_Rotation.Load(fxRot);

	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(atHashWithStringNotFinal(m_PFxAssetHash));
	eventInfo.m_pEffect[0] = (ptxEffectRef)ptfxScript::Start(atHashWithStringNotFinal(m_PFxNameHash), ptfxAssetName, pEntity, m_BoneTag, RC_VEC3V(const_cast<Vector3&>(fxPos)), RC_VEC3V(const_cast<Vector3&>(fxRot)), m_Scale, m_InvertAxes);

	if(GetPacketVersion() >= gPacketStartScriptPtFxPacketVersion_v2 && m_hasColour)
	{
		ptfxScript::UpdateColourTint((int)eventInfo.m_pEffect[0], Vec3V( (float) (m_r/255.0), (float) (m_g/255.0), (float) (m_b/255.0) ) );
	}
}

eShouldExtract CPacketStartScriptPtFx::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	// Quick check here as this packet has a special 'ShouldExtract' which is causing it to play multiple times at the start of a clip (url:bugstar:2657111)
	if(IsPlayed() == true && (flags & REPLAY_DIRECTION_FWD))
		return REPLAY_EXTRACT_FAIL;

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

const CReplayID& CPacketStartScriptPtFx::GetReplayID(u8 value) const
{	
	if( GetPacketVersion() >= gPacketStartScriptPtFxPacketVersion_v1 )
	{
		return CPacketEntityInfoStaticAware::GetReplayID(value);
	}

	return NoEntityID;
}

bool CPacketStartScriptPtFx::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}


CPacketStartNetworkScriptPtFx::CPacketStartNetworkScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes,
															 bool hasColour,	u8 r, u8 g, u8 b,
															 u32 evoIDA, float evoValA, u32 evoIDB, float evoValB)
	: CPacketEventTracked(PACKETID_STARTNETWORKSCRIPTFX, sizeof(CPacketStartNetworkScriptPtFx))
	, CPacketEntityInfoStaticAware()
{
	m_PFxNameHash = pFxNameHash;
	m_PFxAssetHash = pFxAssetHash;
	m_BoneTag = boneTag;
	m_Position.Store(fxPos);
	m_Rotation.Store(fxRot);
	m_Scale = scale;
	m_InvertAxes = invertAxes;

	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(pFxAssetHash);
	if (replayVerifyf(slot.IsValid(), "CPacketStartScriptPtFx - cannot find particle asset slot"))
	{
		m_Slot = slot.Get();
	}

	m_hasColour = hasColour;
	m_r = r;
	m_g = g;
	m_b = b;

	m_evoValA = evoValA;
	m_evoValB = evoValB;
	m_evoIDA = evoIDA;
	m_evoIDB = evoIDB;
}

//////////////////////////////////////////////////////////////////////////
void CPacketStartNetworkScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( eventInfo.m_pEffect[0] != 0 && ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
		{
			ptfxScript::Stop((int)eventInfo.m_pEffect[0]);
		}

		eventInfo.m_pEffect[0] = 0;
		return;
	}

	CEntity* pEntity = eventInfo.pEntity[0];

	Vector3 fxPos;
	m_Position.Load(fxPos);

	Vector3 fxRot;
	m_Rotation.Load(fxRot);

	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(atHashWithStringNotFinal(m_PFxAssetHash));
	eventInfo.m_pEffect[0] = (ptxEffectRef)ptfxScript::Start(atHashWithStringNotFinal(m_PFxNameHash), ptfxAssetName, pEntity, m_BoneTag, RC_VEC3V(const_cast<Vector3&>(fxPos)), RC_VEC3V(const_cast<Vector3&>(fxRot)), m_Scale, m_InvertAxes);

	if (m_hasColour)
	{
		ptfxScript::UpdateColourTint((int)eventInfo.m_pEffect[0], Vec3V( (float) (m_r/255.0), (float) (m_g/255.0), (float) (m_b/255.0) ) );
	}

	if (m_evoIDA)
	{
		ptfxScript::UpdateEvo((int)eventInfo.m_pEffect[0], m_evoIDA, m_evoValA);
	}

	if (m_evoIDB)
	{
		ptfxScript::UpdateEvo((int)eventInfo.m_pEffect[0], m_evoIDB, m_evoValB);
	}
}


ePreloadResult CPacketStartNetworkScriptPtFx::Preload(CTrackedEventInfo<ptxEffectRef>&) const
{
	s32 slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(m_PFxAssetHash).Get();

	PTFX_ReplayStreamingRequest* request = CReplayMgr::GetStreamingRequests().Access(slot);
	if(!request)
	{
		request = &CReplayMgr::GetStreamingRequests().Insert(slot).data;
		request->SetIndex(slot);
		request->SetStreamingModule(ptfxManager::GetAssetStore());
		request->Load();
	}

	return ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(slot)) ? PRELOAD_SUCCESSFUL : PRELOAD_FAIL;
}


eShouldExtract CPacketStartNetworkScriptPtFx::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

bool CPacketStartNetworkScriptPtFx::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}

//////////////////////////////////////////////////////////////////////////

CPacketForceVehInteriorScriptPtFx::CPacketForceVehInteriorScriptPtFx(bool force)
	: CPacketEventTracked(PACKETID_FORCEVEHINTERIORSCRIPTFX, sizeof(CPacketForceVehInteriorScriptPtFx))
	, CPacketEntityInfo()
	, m_forceVehicleInterior(force)
{
}

void CPacketForceVehInteriorScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{	
	if( eventInfo.m_pEffect[0] != 0 && ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
	{
		ptfxScript::UpdateForceVehicleInteriorFX((int)eventInfo.m_pEffect[0], m_forceVehicleInterior);
	}
}


//////////////////////////////////////////////////////////////////////////

CPacketStopScriptPtFx::CPacketStopScriptPtFx()
	: CPacketEventTracked(PACKETID_STOPSCRIPTFX, sizeof(CPacketStopScriptPtFx))
	, CPacketEntityInfo()
{
}

void CPacketStopScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{	
	//only stop the effect if it actually exists - B*2123608
	if( eventInfo.m_pEffect[0] != 0 && ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
	{
		if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
		{
			// If we're jumping then just remove the effect as the 'Stop' will allow it do die off naturally which may not be
			// desirable depending on how far we've jumped.
			ptfxScript::Remove((int)eventInfo.m_pEffect[0]);
		}
		else
		{
			ptfxScript::Stop((int)eventInfo.m_pEffect[0]);
		}
		
		eventInfo.m_pEffect[0] = 0;
	}
}


//////////////////////////////////////////////////////////////////////////

CPacketDestroyScriptPtFx::CPacketDestroyScriptPtFx()
	: CPacketEventTracked(PACKETID_DESTROYSCRIPTFX, sizeof(CPacketDestroyScriptPtFx))
	, CPacketEntityInfo()
{
}

void CPacketDestroyScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{	
	//only stop the effect if it actually exists - B*2123608
	if( eventInfo.m_pEffect[0] != 0 && ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
	{
		ptfxScript::Destroy((int)eventInfo.m_pEffect[0]);
		eventInfo.m_pEffect[0] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

CPacketUpdateOffsetScriptPtFx::CPacketUpdateOffsetScriptPtFx(const Matrix34& mat)
	: CPacketEventTracked(PACKETID_UPDATEOFFSETSCRIPTFX, sizeof(CPacketUpdateOffsetScriptPtFx))
	, CPacketEntityInfo()
{
	m_Matrix.StoreMatrix(mat);
}


void CPacketUpdateOffsetScriptPtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{	
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		return;
	}

	Matrix34 mat;
	m_Matrix.LoadMatrix(mat);
	ptfxScript::UpdateOffset((int)eventInfo.m_pEffect[0], MATRIX34_TO_MAT34V(mat));
}

bool CPacketUpdateOffsetScriptPtFx::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}

eShouldExtract CPacketUpdateOffsetScriptPtFx::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

//////////////////////////////////////////////////////////////////////////

CPacketEvolvePtFx::CPacketEvolvePtFx(u32 evoHash, float evoVal)
	: CPacketEventTracked(PACKETID_EVOLVEPTFX, sizeof(CPacketEvolvePtFx))
	, CPacketEntityInfo()
{
	m_EvoHash = evoHash;
	m_Value = evoVal;
}


void CPacketEvolvePtFx::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		return;
	}

	if( !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]) )
	{
		return;
	}

	ptfxScript::UpdateEvo((int)eventInfo.m_pEffect[0], m_EvoHash, m_Value);
}

bool CPacketEvolvePtFx::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}

eShouldExtract CPacketEvolvePtFx::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

//////////////////////////////////////////////////////////////////////////

CPacketUpdatePtFxFarClipDist::CPacketUpdatePtFxFarClipDist(float dist)
	: CPacketEventTracked(PACKETID_UPDATESCRIPTFXFARCLIPDIST, sizeof(CPacketUpdatePtFxFarClipDist))
	, CPacketEntityInfo()
{
	m_dist = dist;
}

void CPacketUpdatePtFxFarClipDist::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{	
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		return;
	}

	ptfxScript::UpdateFarClipDist((int)eventInfo.m_pEffect[0], m_dist);
}

bool CPacketUpdatePtFxFarClipDist::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}

eShouldExtract CPacketUpdatePtFxFarClipDist::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

//////////////////////////////////////////////////////////////////////////
CPacketTriggeredScriptPtFx::CPacketTriggeredScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes)
	: CPacketEvent(PACKETID_TRIGGEREDSCRIPTFX, sizeof(CPacketTriggeredScriptPtFx))
	, CPacketEntityInfoStaticAware()
{
	m_PFxNameHash = pFxNameHash;
	m_PFxAssetHash = pFxAssetHash;
	m_BoneTag = boneTag;
	m_Position.Store(fxPos);
	m_Rotation.Store(fxRot);
	m_Scale = scale;
	m_InvertAxes = invertAxes;
	m_HasMatrix = false;

	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(pFxAssetHash);
	if (replayVerifyf(slot.IsValid(), "CPacketTriggeredScriptPtFx - cannot find particle asset slot"))
	{
		m_Slot = slot.Get();
	}
}

CPacketTriggeredScriptPtFx::CPacketTriggeredScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Matrix34& mat)
	: CPacketEvent(PACKETID_TRIGGEREDSCRIPTFX, sizeof(CPacketTriggeredScriptPtFx))
	, CPacketEntityInfoStaticAware()
{
	m_PFxNameHash = pFxNameHash;
	m_PFxAssetHash = pFxAssetHash;
	m_BoneTag = boneTag;

	m_HasMatrix = true;
	m_Matrix.StoreMatrix(mat);

	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(pFxAssetHash);
	if (replayVerifyf(slot.IsValid(), "CPacketTriggeredScriptPtFx - cannot find particle asset slot"))
	{
		m_Slot = slot.Get();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketTriggeredScriptPtFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity(0);

	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(atHashWithStringNotFinal(m_PFxAssetHash));

	if( m_HasMatrix )
	{
		Matrix34 mat;
		m_Matrix.LoadMatrix(mat);
		ptfxScript::Trigger(atHashWithStringNotFinal(m_PFxNameHash), ptfxAssetName, pEntity, m_BoneTag, MATRIX34_TO_MAT34V(mat));
	}
	else
	{
		Vector3 fxPos;
		m_Position.Load(fxPos);

		Vector3 fxRot;
		m_Rotation.Load(fxRot);

		ptfxScript::Trigger(atHashWithStringNotFinal(m_PFxNameHash), ptfxAssetName, pEntity, m_BoneTag, RC_VEC3V(const_cast<Vector3&>(fxPos)), RC_VEC3V(const_cast<Vector3&>(fxRot)), m_Scale, m_InvertAxes);
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketTriggeredScriptPtFxColour::CPacketTriggeredScriptPtFxColour(float r, float g, float b)
	: CPacketEvent(PACKETID_TRIGGEREDSCRIPTFXCOLOUR, sizeof(CPacketTriggeredScriptPtFxColour))
	, CPacketEntityInfo()
{
	m_Red = r;
	m_Green = g;
	m_Blue = b;
}

//////////////////////////////////////////////////////////////////////////
void CPacketTriggeredScriptPtFxColour::Extract(const CEventInfo<void>&) const
{
	ptfxScript::SetTriggeredColourTint(m_Red, m_Green, m_Blue);
}

//////////////////////////////////////////////////////////////////////////
CPacketUpdateScriptPtFxColour::CPacketUpdateScriptPtFxColour(float r, float g, float b)
	: CPacketEventTracked(PACKETID_UPDATESCRIPTFXCOLOUR, sizeof(CPacketUpdateScriptPtFxColour))
	, CPacketEntityInfo()
{
	m_Red = r;
	m_Green = g;
	m_Blue = b;
}

//////////////////////////////////////////////////////////////////////////
void CPacketUpdateScriptPtFxColour::Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		return;
	}

	ptfxScript::UpdateColourTint((int)eventInfo.m_pEffect[0], Vec3V(m_Red, m_Green, m_Blue));
}

bool CPacketUpdateScriptPtFxColour::HasExpired(const CTrackedEventInfo<ptxEffectRef>& eventInfo) const
{
	return !ptfxScript::DoesExist((int)eventInfo.m_pEffect[0]);
}

eShouldExtract CPacketUpdateScriptPtFxColour::ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
{
	if (fwTimer::IsGamePaused())
	{
		return REPLAY_EXTRACT_FAIL_PAUSED;
	}

	if( flags & REPLAY_DIRECTION_BACK )
		return REPLAY_EXTRACT_SUCCESS;
	else
		return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
}

//////////////////////////////////////////////////////////////////////////
CPacketParacuteSmokeFx::CPacketParacuteSmokeFx(u32 hashName, s32 boneIndex, const Vector3& color)
	: CPacketEvent(PACKETID_PARACUTESMOKE, sizeof(CPacketParacuteSmokeFx))
	, m_HashName(hashName)
	, m_BoneIndex(boneIndex)
{
	m_Color.Store(color);
}

//////////////////////////////////////////////////////////////////////////
void CPacketParacuteSmokeFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{	
	CEntity* pEntity = eventInfo.GetEntity(0);

	if( pEntity )
	{		
		// register the fx system
		bool justCreated = false;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, PTFXOFFSET_PED_PARACHUTE_SMOKE, true, m_HashName, justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_BoneIndex);

			if (justCreated)
			{
				Vector3 color;
				m_Color.Load(color);
				ptfxWrapper::SetColourTint(pFxInst, VECTOR3_TO_VEC3V(color));

				pFxInst->Start();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketRequestPtfxAsset::CPacketRequestPtfxAsset(s32 slot)
	: CPacketEvent(PACKETID_REQUESTPTFXASSET, sizeof(CPacketRequestPtfxAsset))
	, CPacketEntityInfo()
	, m_Slot(slot)
{
}

void CPacketRequestPtfxAsset::Extract(const CEventInfo<void>&) const
{	
}


//////////////////////////////////////////////////////////////////////////
CPacketRequestNamedPtfxAsset::CPacketRequestNamedPtfxAsset(u32 hash)
	: CPacketEvent(PACKETID_REQUESTNAMEDPTFXASSET, sizeof(CPacketRequestNamedPtfxAsset))
	, CPacketEntityInfo()
	, m_hash(hash)
{
}

void CPacketRequestNamedPtfxAsset::Extract(const CEventInfo<void>&) const
{

}


ePreloadResult CPacketRequestNamedPtfxAsset::Preload(const CEventInfo<void>&) const
{
	s32 slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(m_hash).Get();

	PTFX_ReplayStreamingRequest* request = CReplayMgr::GetStreamingRequests().Access(slot);
	if(!request)
	{
		request = &CReplayMgr::GetStreamingRequests().Insert(slot).data;
		request->SetIndex(slot);
		request->SetStreamingModule(ptfxManager::GetAssetStore());
		request->Load();
	}

	return ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(slot)) ? PRELOAD_SUCCESSFUL : PRELOAD_FAIL;
}



//////////////////////////////////////////////////////////////////////////
CPacketFocusEntity::CPacketFocusEntity()
	: CPacketEvent(PACKETID_FOCUSENTITY, sizeof(CPacketFocusEntity))
{}

//////////////////////////////////////////////////////////////////////////
void CPacketFocusEntity::Extract(const CEventInfo<CEntity>& eventInfo) const
{	
	CEntity* pEntity = eventInfo.GetEntity(0);

	if(pEntity)
	{		
		CFocusEntityMgr::GetMgr().SetEntity(*pEntity);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CPacketFocusEntity::HasExpired(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetEntity() == nullptr || CFocusEntityMgr::GetMgr().GetEntity() != eventInfo.GetEntity())
	{
		return true;
	}

	return false;
}

int CPacketFocusPosAndVelCount = 0;
//////////////////////////////////////////////////////////////////////////
CPacketFocusPosAndVel::CPacketFocusPosAndVel(const Vector3& pos, const Vector3& vel)
	: CPacketEvent(PACKETID_FOCUSPOSANDVEL, sizeof(CPacketFocusPosAndVel))
{
	m_focusPos.Store(pos);
	m_focusVel.Store(vel);
	CPacketFocusPosAndVelCount++;
	replayDebugf1("CPacketFocusPosAndVelCount %d, pos %f, %f, %f, vel %f, %f, %f", CPacketFocusPosAndVelCount, pos.GetX(), pos.GetY(), pos.GetZ(), vel.GetX(), vel.GetY(), vel.GetZ());
}

//////////////////////////////////////////////////////////////////////////
void CPacketFocusPosAndVel::Extract(const CEventInfo<void>& /*eventInfo*/) const
{	
	Vector3 pos, vel;
	m_focusPos.Load(pos);
	m_focusVel.Load(vel);
	CFocusEntityMgr::GetMgr().SetPosAndVel(pos, vel);
}


//////////////////////////////////////////////////////////////////////////
bool CPacketFocusPosAndVel::HasExpired(const CEventInfo<void>& /*eventInfo*/) const
{
	if(CFocusEntityMgr::GetMgr().GetFocusState() != CFocusEntityMgr::FOCUS_OVERRIDE_POS)
	{
		CPacketFocusPosAndVelCount--;
		return true;
	}
	
	Vector3 pos, vel;
	m_focusPos.Load(pos);
	m_focusVel.Load(vel);
	if(CFocusEntityMgr::GetMgr().GetPos() != pos || CFocusEntityMgr::GetMgr().GetVel() != vel)
	{
		CPacketFocusPosAndVelCount--;
		return true;
	}

	return false;
}

#endif // GTA_REPLAY
