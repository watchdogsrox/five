//
// CCustomShaderEffectAnimUV - class for managing UV animation for entities and their shaders;
//
//	20/06/2006	-	Andrzej:	- intial;
//	19/11/2007	-	Andrzej:	- Update(): anim time synchronization between entities and their lods added;
//
// The gta5 'testbed' level has an example of an AnimUV.
#include "shaders\CustomShaderEffectAnimUV.h"

//Rage includes
#include "cranimation/frame.h"
#include "cranimation/framedata.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentcreature.h"
#include "fwgeovis/geovis.h"
#include "fwscene/stores/clipdictionarystore.h"
#include "fwsys/timer.h"			// fwTimer()...
#include "fwdrawlist/drawlistmgr.h"

// Game headers
#include "scene/Entity.h"
#include "shaders/ShaderLib.h"
#include "streaming/defragmentation.h"
#include "Control/Replay/Replay.h"

ANIM_OPTIMISATIONS()

// Defines
#define VARNAME_GLOBAL_ANIM_UV0			("globalAnimUV0")
#define VARNAME_GLOBAL_ANIM_UV1			("globalAnimUV1")

#if CSE_ANIM_EDITABLEVALUES
	static bool bDbgUpdateAnimUVEnabled = TRUE;
#endif


//
//
//
//
CCustomShaderEffectAnimUV::CCustomShaderEffectAnimUV(u32 size)
	: CCustomShaderEffectBase(size, CSE_ANIMUV)
{
}

//
//
//
//
CCustomShaderEffectAnimUV::~CCustomShaderEffectAnimUV()
{
}

//
//
// creates extra shader variables in drawable, which will hold UV animation variables;
//
bool CCustomShaderEffectAnimUV::CreateExtraAnimationUVvariables(rmcDrawable *pDrawable)
{
	Assert(pDrawable);
	if(!pDrawable)
		return(FALSE);

bool bVarsCreated = FALSE;

		grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
		Assert(pShaderGroup);

		if(!pShaderGroup)
			return(FALSE);

		const s32 shaderCount	= pShaderGroup->GetCount();
		Assert(shaderCount >= 1);
		for(s32 j=0; j<shaderCount; j++)
		{
			grmShader *pShader = &pShaderGroup->GetShader(j);
			Assert(pShader);

			// check if shader contains UV anim variables:
			grcEffectVar shaderEffectVarUV0	= pShader->LookupVar(VARNAME_GLOBAL_ANIM_UV0, FALSE);
			grcEffectVar shaderEffectVarUV1	= pShader->LookupVar(VARNAME_GLOBAL_ANIM_UV1, FALSE);
			if(shaderEffectVarUV0 && shaderEffectVarUV1)
			{
				// this is simpler version of it, we don't create ShaderVars (=save memory)
				// and assume that all instances of the building will reset their uv anims otherwise
				// (by applying zero anim for instance):
				bVarsCreated = TRUE;
			}// if(shaderEffectVarUV0 && shaderEffectVarUV1)...
		}//for(s32 j=0; j<shaderCount; j++)...

	return(bVarsCreated);
}


//
//
// master initialize in CBaseModelInfo::SetDrawable()
//
CCustomShaderEffectAnimUVType* CCustomShaderEffectAnimUVType::Create(rmcDrawable *pDrawable, u32 nObjectHashKey, s32 animDictFileIndex)
{
	Assert(pDrawable);
	if(!pDrawable)
		return NULL;

	if(!nObjectHashKey)
		return NULL;

	// Is the anim dictionary invalid?
	if(animDictFileIndex < 0)	
	{
		return NULL;
	}

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	const s32 shaderCount	= pShaderGroup->GetCount();
	Assert(shaderCount >= 1);

	CCustomShaderEffectAnimUVType *pType = rage_new CCustomShaderEffectAnimUVType;
	pType->m_tabAnimUVInfo.Reserve(shaderCount);

	for(s32 i=0; i<shaderCount; i++)
	{
		CCustomShaderEffectAnimUVType::structAnimUVinfo& animInfo  = pType->m_tabAnimUVInfo.Append();
		animInfo.m_pClip			= NULL;
		animInfo.m_shaderIdx		= 0;
		animInfo.m_varUV0			= grcevNONE;
		animInfo.m_varUV1			= grcevNONE;
	}

	pType->m_wadAnimIndex = animDictFileIndex;

	bool bAnimsFound = FALSE;
	for(s32 j=0; j<shaderCount; j++)
	{
		// load animations for shaders (if they exist):

		// UV anim names are of the form <objectNameString>"_uv_"<shaderIndexString>
		// Normally during resourcing (in ragebuilder) the hashKey for an anim is created using :
		// animHash = atStringHash(<animNameString>)
		// But the hashkey for anims that contain the string "_uv_" are created using :
		// animHash = atStringHash(<objectNameString>) + <shaderIndex> + 1
		// ragebuilder.cpp:
		// - JimmyClipDictionary::CalcKey(): code = atStringHash(rootName) + atoi(p_Found) + 1;
		// - (no longer used) SaveDictionaryOfAnims(): code = atStringHash(rootName) + atoi(p_Found) + 1;

		// create animation hashkey (objHash+shaderNo+1)
		const u32 nAnimHashKey = nObjectHashKey+j+1;	
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(animDictFileIndex, nAnimHashKey);
		if(pClip)
		{
			grmShader		*pShader		= &pShaderGroup->GetShader(j);
			Assert(pShader);
			grcEffectVar	effectVarUV0	= pShader->LookupVar(VARNAME_GLOBAL_ANIM_UV0, FALSE);
			grcEffectVar	effectVarUV1	= pShader->LookupVar(VARNAME_GLOBAL_ANIM_UV1, FALSE);

			// Andrzej's temp fix:
			// this is wrong, proper animations should be returned for given shaders' indices.
			if(effectVarUV0 != grcevNONE && effectVarUV1 != grcevNONE)
			{
				bAnimsFound = TRUE;
				CCustomShaderEffectAnimUVType::structAnimUVinfo *pAnimInfo = &pType->m_tabAnimUVInfo[j];
				pAnimInfo->m_pClip = pClip;
				Assign(pAnimInfo->m_shaderIdx,j);
				Assign(pAnimInfo->m_varUV0,effectVarUV0);
				Assign(pAnimInfo->m_varUV1,effectVarUV1);
				
				//u32 frameCount = rage::sysTimeManager::GetGlobalManager().GetFrameCount();
				//extern char* CCustomShaderEffectBase_EntityName;			
				//Warningf("frameCount = %u : ObjectName (%s) has been assigned UV anim (animDictName %s, animDictHash %d, animName %d, animHash %d) to shader (%d)", frameCount, CCustomShaderEffectBase_EntityName, g_ClipDictionaryStore.GetName(animDictFileIndex), animDictFileIndex, pClip->GetName().c_str(), nAnimHashKey, j);
			}
#if !__FINAL && !HEIGHTMAP_GENERATOR_TOOL
			else
			{
				// this is wrong: animation is present (as taken from hashkey), 
				// but shader doesn't support it (probably wrong hash was generated?):
				u32 frameCount = rage::sysTimeManager::GetGlobalManager().GetFrameCount();
				extern char* CCustomShaderEffectBase_EntityName;
				Warningf("frameCount = %u : ObjectName (%s) has been wrongly assigned UV anim (animDictName %s, animDictIndex %d, animName %s, animHash %d) to shader (%d) hashkey mismatch?", frameCount, CCustomShaderEffectBase_EntityName, g_ClipDictionaryStore.GetName(strLocalIndex(animDictFileIndex)), animDictFileIndex, pClip->GetName(), nAnimHashKey, j);
			}
#endif //!__FINAL && !HEIGHTMAP_GENERATOR_TOOL
		}// if(pClip)...
		else
		{
			//u32 frameCount = rage::sysTimeManager::GetGlobalManager().GetFrameCount();
			//extern char* CCustomShaderEffectBase_EntityName;		
			//char uvAnimName[100];
			//sprintf(uvAnimName, "%d", CCustomShaderEffectBase_EntityName, "_uv_0", j);
			//Warningf("frameCount = %u : Object '%s' Can't find UV animation (animDictName %s, animDictHash %d, animName %d, animHash %d) to shader (%d)", frameCount, CCustomShaderEffectBase_EntityName, g_ClipDictionaryStore.GetName(animDictFileIndex), animDictFileIndex, uvAnimName, nAnimHashKey, j);
		}
	}//for(s32 j=0; j<numShaders; j++)...


	// The anim dictionary is streamed in as a dependency of the drawable
	// Add reference so the anim dictionary is not deleted
	if(animDictFileIndex > -1)	
	{
		fwAnimManager::AddRef(strLocalIndex(animDictFileIndex));
	}

	// No anims found return nothing
	if(!bAnimsFound)
	{
		delete pType;
		return NULL;
	}

#if __DEV
	//extern char* CCustomShaderEffectBase_EntityName;
	//AreClipsValid(pNewAnimUV, CCustomShaderEffectBase_EntityName);
#endif //__DEV

	return pType;
}// end of Create()...


//
//
// master shutdown in CBaseModelInfo::DeleteDrawable()
//
CCustomShaderEffectAnimUVType::~CCustomShaderEffectAnimUVType()
{
	if(m_wadAnimIndex > -1)	
	{
		// remove a reference to the anim dictionary
		fwAnimManager::RemoveRef(strLocalIndex(m_wadAnimIndex));
	}
	for (int i=0; i<m_tabAnimUVInfo.GetCount(); i++)
		m_tabAnimUVInfo[i].m_pClip = 0;
#if __DEV
	//AreClipsValid(this);
#endif //__DEV

}// end of Shutdown()...


//
//
// returns AnimUVFX of given entity's LOD (if one exists):
//
// static
// CCustomShaderEffectAnimUV* GetLODAnimUVFX(CEntity *pEntity)
// {
// 	if(!pEntity->__GetLod())
// 		return(NULL);
// 
// 	CEntity *pLodEntity = (CEntity*)pEntity->__GetLod();
// 	Assert(pLodEntity);
// 
// 	if(pLodEntity->GetAlphaFade()==0)
// 		return(NULL);
// 
// 	CGtaEntityDrawData* lodGtaDrawable = pLodEntity->GetDrawData();
// 	if(!lodGtaDrawable)
// 		return(NULL);
// 
// 
// 	CCustomShaderEffectAnimUV *pLodAnimUVFX = static_cast<CCustomShaderEffectAnimUV*>(lodGtaDrawable->GetShaderEffect());
// 	return(pLodAnimUVFX);
// }

//
//
// instance update in CEntity::PreRender()
//
void CCustomShaderEffectAnimUV::Update(fwEntity*)
{

#if CSE_ANIM_EDITABLEVALUES
	// debug widgets:
	if(!bDbgUpdateAnimUVEnabled)
		return;
#endif	//CSE_ANIM_EDITABLEVALUES...

	// Is the anim dictionary valid?
	if (m_pType->m_wadAnimIndex > -1)
	{
	// synchronize with this entity LOD's UV anim (if available) (BS#156358):
		const s32 count = m_pType->m_tabAnimUVInfo.GetCount();

		// JW : shouldn't need to sync anims between LODs because they should all get created synced to LodTree local clock
// 		if(count)
// 		{
// 			CCustomShaderEffectAnimUV *pLodAnimUVFX = GetLODAnimUVFX(pEntity);
// 			if(pLodAnimUVFX)
// 			{
// 				const s32 maxCountLod = pLodAnimUVFX->m_tabAnimUVInfoCount;
// 				for(s32 i=0; i<count; i++)
// 				{
// 						const crClip *pClip = m_tabAnimUVInfo[i].m_pClip;
// 					if(pClip)
// 					{
// 						const float animDuration	= pClip->GetDuration();
// 						const float animNumFrames	= pClip->GetNum30Frames();
// 						for(s32 j=0; j<maxCountLod; j++)
// 						{
// 								const crClip *pLodClip = pLodAnimUVFX->m_tabAnimUVInfo[j].m_pClip;
// 								if(pLodClip)
// 								{
// 									// find matching anim (by length and num frames):
// 									// JA - This seems like an error prone way of matching the anim why not use the anim hash?
// 									if( (pLodClip->GetDuration() == animDuration)
// 									 && (pLodClip->GetNum30Frames() == animNumFrames) )
// 									{
// 										m_tabAnimUVInfo[i].m_timeAnimStart = pLodAnimUVFX->m_tabAnimUVInfo[j].m_timeAnimStart;
// 								}
// 							}
// 						}//for(s32 j=0; j<maxCountLod; j++)...
// 					}//if(pAnim)...
// 				}//for(s32 i=0; i<count; i++)...
// 			}//if(pLodAnimUVFX)...
// 		}//if(count)...

	crFrameDataFixedDofs<2> frameData;
	frameData.AddDof(kTrackShaderSlideU, 0, kFormatTypeVector3);
	frameData.AddDof(kTrackShaderSlideV, 0, kFormatTypeVector3);
	crFrameFixedDofs<2> frame(frameData);
	frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());

	CCustomShaderEffectAnimUVType *pType = m_pType;

		// update animation
	for(s32 i=0; i<count; i++)
	{
		CCustomShaderEffectAnimUVType::structAnimUVinfo *pAnimInfo = &pType->m_tabAnimUVInfo[i];

		const crClip *pClip = pAnimInfo->m_pClip;
		if(pClip)
		{
			const s32 nAnimDurationInMs	=	  s32(pClip->GetDuration() * 1000.0f);
			const s32 nCurrTimeInMs			= fwTimer::GetTimeInMilliseconds();
			s32 nCurrAnimTimeMs				= nCurrTimeInMs - m_uvs[i].m_timeAnimStart;

			//Assertf(nCurrAnimTime >= 0, "Animation time is not in positive domain - something wrong with current gametime.");
			if(nCurrAnimTimeMs < 0)		// temp fix for bug #22526 & #54280
			{
#if GTA_REPLAY
				//On replay when we rewind or jump backwards on the timeline we need to wrap the anim times correctly.
				if( CReplayMgr::IsEditModeActive() )
				{
					nCurrAnimTimeMs = nAnimDurationInMs + nCurrAnimTimeMs;
					m_uvs[i].m_timeAnimStart = nCurrTimeInMs - nCurrAnimTimeMs;
				}
				else
#endif //GTA_REPLAY
				{
					nCurrAnimTimeMs = 0;
				}
			}

			// How many frames have we looped by
			if ( nCurrTimeInMs > (m_uvs[i].m_timeAnimStart + nAnimDurationInMs) )
			{
				nCurrAnimTimeMs = nCurrAnimTimeMs % nAnimDurationInMs;
				m_uvs[i].m_timeAnimStart = nCurrTimeInMs - nCurrAnimTimeMs;
			}

			float fCurrAnimPhase = float(nCurrAnimTimeMs)/nAnimDurationInMs;

			float fCurrAnim30Frame = pClip->ConvertPhaseTo30Frame(fCurrAnimPhase);
			fCurrAnim30Frame = float(s32(fCurrAnim30Frame));
			pClip->Composite(frame, pClip->Convert30FrameToTime(fCurrAnim30Frame));

			// Save and restore timeAnimStart since it may get stomped by vector operations
			unsigned temp = m_uvs[i].m_timeAnimStart;
			frame.GetValue<Vec3V>(kTrackShaderSlideU, 0, (Vec3V&)(m_uvs[i].m_uv0));
			frame.GetValue<Vec3V>(kTrackShaderSlideV, 0, (Vec3V&)(m_uvs[i].m_uv1));
			m_uvs[i].m_timeAnimStart = temp;

			// invert v1 offset
			// (Greg's TODO #16006: to be done during export)
//			pAnimInfo->m_uv1.z = 1.0f - pAnimInfo->m_uv1.z;
		}// if(pAnim)...
	} // end of for(s32 i=0; i<count; i++)...
	}//m_wadAnimIndex > -1)

} // end of Update()...

//
//
// instance setting variables in CEntity/DynamicEntity::Render()
//
//void CCustomShaderEffectAnimUV::SetShaderVariables(CEntity* ASSERT_ONLY(pEntity))
void CCustomShaderEffectAnimUV::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	const s32 count = m_pType->m_tabAnimUVInfo.GetCount();
	Assert(count > 0);

	for(s32 i=0; i<count; i++)
	{
		CCustomShaderEffectAnimUVType::structAnimUVinfo *pAnimInfo = &m_pType->m_tabAnimUVInfo[i];
		if(pAnimInfo->m_pClip)
		{
			pDrawable->GetShaderGroup().GetShader(pAnimInfo->m_shaderIdx).SetVar((grcEffectVar)pAnimInfo->m_varUV0, (Vector3&)m_uvs[i].m_uv0);
			pDrawable->GetShaderGroup().GetShader(pAnimInfo->m_shaderIdx).SetVar((grcEffectVar)pAnimInfo->m_varUV1, (Vector3&)m_uvs[i].m_uv1);
		}
	}

}// end of SetShaderVariables()...

//
//
// extra access to light's animation:
//
bool CCustomShaderEffectAnimUV::GetLightAnimUV(Vector3 *uv0, Vector3 *uv1)
{
	if((!uv0) || (!uv1))
		return(FALSE);

	const s32 count = m_pType->m_tabAnimUVInfo.GetCount();
	Assert(count > 0);

	Vector3& out0 = *uv0;
	Vector3& out1 = *uv1;

	// find 1st anim data and return it at it is:
	for(s32 i=0; i<count; i++)
	{
		CCustomShaderEffectAnimUVType::structAnimUVinfo *pAnimInfo = &m_pType->m_tabAnimUVInfo[i];
		if(pAnimInfo->m_pClip)
		{
			out0.Set(m_uvs[i].m_uv0[0],m_uvs[i].m_uv0[1],m_uvs[i].m_uv0[2]);
			out1.Set(m_uvs[i].m_uv1[0],m_uvs[i].m_uv1[1],m_uvs[i].m_uv1[2]);
			break;
		}
	}

	return(TRUE);
}

//
//
//
//
void CCustomShaderEffectAnimUV::AddToDrawList(u32 modelIndex, bool bExecute)
{
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

//
//
// instance create in CEntity::CreateDrawable()
//
CCustomShaderEffectBase*	CCustomShaderEffectAnimUVType::CreateInstance(CEntity* pEntity)
{
	const u32 seedVal = (((u32)(size_t) pEntity->GetRootLod()) >> 3) & 0xff;	// for syncing across LODs
	const s32 count = m_tabAnimUVInfo.GetCount();

	u32 size = sizeof(CCustomShaderEffectAnimUV) + count * sizeof(CCustomShaderEffectAnimUV::structUVs);
	CCustomShaderEffectAnimUV* pNewAnimUV = new(rage_aligned_new(16) u8[size]) CCustomShaderEffectAnimUV(size);

	pNewAnimUV->m_pType = this;
	for(s32 i=0; i<count; i++)
	{
		CCustomShaderEffectAnimUV::structUVs &uv = pNewAnimUV->m_uvs[i];
		uv.m_uv0[0] = 1.0f; uv.m_uv0[1] = 0.0f; uv.m_uv0[2] = 0.0f; uv.m_timeAnimStart = seedVal;
		uv.m_uv1[0] = 0.0f; uv.m_uv1[1] = 1.0f; uv.m_uv1[2] = 0.0f; uv.pad = 0;
	}

#if __DEV
	//AreClipsValid(pNewAnimUV);
#endif //__DEV

	return(pNewAnimUV);
}// end of CreateInstance()...


//
//
//
//

#if __DEV
// Added to help track down defrag bugs
void CCustomShaderEffectAnimUV::AreClipsValid(CCustomShaderEffectAnimUV* pShader, const char *pName)
{
	if (pShader)
	{
		Assertf(g_ClipDictionaryStore.IsValidSlot(pShader->GetClipDictIndex()),			"Clip dict index is invalid");
		if (pShader->GetClipDictIndex().IsValid())
		{
			Assertf(g_ClipDictionaryStore.IsValidSlot(pShader->GetClipDictIndex()),		"Clip dict has no slot");
			Assertf(g_ClipDictionaryStore.IsObjectInImage(pShader->GetClipDictIndex()),	"Clip dict is not in image");
			Assertf(g_ClipDictionaryStore.HasObjectLoaded(pShader->GetClipDictIndex()),	"Clip dict is not loaded");
			Assertf(g_ClipDictionaryStore.GetNumRefs(strLocalIndex(pShader->GetClipDictIndex())),		"Clip dict has no refs");
		}

		const s32 count = pShader->m_pType->m_tabAnimUVInfo.GetCount();
		for(s32 i=0; i<count; i++)
		{
			CCustomShaderEffectAnimUVType::structAnimUVinfo* pAnimInfo  = &pShader->m_pType->m_tabAnimUVInfo[i];
			if ( (pAnimInfo->m_pClip == NULL) || (pAnimInfo->m_pClip->GetType() == crClip::kClipTypeAnimation) )
			{
			}
			else
			{
				u32 frameCount = rage::sysTimeManager::GetGlobalManager().GetFrameCount();
				Errorf("Frame %u = CCustomShaderEffectAnimUV::IsClipValid : Clip is not type kClipTypeAnimation on Object '%s' : UV animation dict (%d) : shader %d\n", frameCount, pName ? pName : "unknown", pShader->GetClipDictIndex().Get(), i);
			}
		}
	}
}
#endif //__DEV

#if CSE_ANIM_EDITABLEVALUES
//
//
//
//
bool CCustomShaderEffectAnimUV::InitWidgets(bkBank& bank)
{
	bank.AddToggle("AnimUV update enabled:", &bDbgUpdateAnimUVEnabled);

	return(TRUE);
}
#endif //CSE_ANIM_EDITABLEVALUES...

