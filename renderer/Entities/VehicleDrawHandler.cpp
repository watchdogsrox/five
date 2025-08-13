/////////////////////////////////////////////////////////////////////////////////
// Title	:	VehicleDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/VehicleDrawHandler.h"

// Rage Headers

// Framework Headers
//#include "fwdrawlist/drawlistmgr.h"

// Game Headers
#include "debug/DebugScene.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Vehicles/vehicle.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/render_channel.h"
#include "renderer/renderer.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/shadows/shadows.h"
#include "renderer/OcclusionQueries.h"

RENDER_OPTIMISATIONS()

//
//
//
//
//static void CSetVehEnvMapDC(void *ptr)
//{
//	Assert(ptr);
//	CVehicleModelInfo *vmi	= (CVehicleModelInfo*)ptr;
//	rmcDrawable *pDrawable	= vmi->GetDrawable();
//	if(pDrawable)
//	{
//		grmShaderGroup* pShaderGroup	= &pDrawable->GetShaderGroup();
//		Assert(pShaderGroup && (u32(pShaderGroup)!=0xEEEEEEEE));
//		const grcRenderTarget *refMap	= CRenderPhaseReflection::GetRenderTarget();
//		const grmShaderGroupVar var		= pShaderGroup->LookupVar("EnvironmentTex", FALSE);
//		if(var && refMap)
//		{
//			pShaderGroup->SetVar(var, refMap);
//		}
//	}
//}
//
////
////
////
////
//static void CSetVehEnvMapHDDC(void *ptr)
//{
//	Assert(ptr);
//	CVehicleModelInfo *vmi	= (CVehicleModelInfo*)ptr;
//	if(vmi->GetAreHDFilesLoaded())
//	{
//		const fragType* type = vmi->GetHDFragType();
//		fragDrawable* pDrawable = type->GetCommonDrawable();
//		if(pDrawable)
//		{
//			grmShaderGroup* pShaderGroup	= &pDrawable->GetShaderGroup();
//			Assert(pShaderGroup && (u32(pShaderGroup)!=0xEEEEEEEE));
//			const grcRenderTarget *refMap	= CRenderPhaseReflection::GetRenderTarget();
//			const grmShaderGroupVar var		= pShaderGroup->LookupVar("EnvironmentTex", FALSE);
//			if(var && refMap)
//			{
//				pShaderGroup->SetVar(var, refMap);
//			}
//		}
//	}
//}


//
//
//
//

CVehicleDrawHandler::CVehicleDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable)
: CDynamicEntityDrawHandler(pEntity, pDrawable), m_pStandardDetailShaderEffect(NULL)
{
	CVehicleModelInfo* mi = static_cast<CVehicle*>(pEntity)->GetVehicleModelInfo();
	Assert(mi);

	u8 col1, col2, col3, col4, col5, col6;
	mi->ChooseVehicleColour(col1, col2, col3, col4, col5, col6);
	m_variation.SetColor1(col1);
	m_variation.SetColor2(col2);
	m_variation.SetColor3(col3);
	m_variation.SetColor4(col4);
	m_variation.SetColor5(col5);
	m_variation.SetColor6(col6);
}

CVehicleDrawHandler::~CVehicleDrawHandler()
{
	if (m_pStandardDetailShaderEffect)
	{
		m_pStandardDetailShaderEffect->DeleteInstance();
	}
}

dlCmdBase* CVehicleDrawHandler::AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( VehicleAddToDrawList );

	CDrawListPrototypeManager::Flush();

	CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

#if __BANK
	if (pVehicle && pVehicle->GetBaseModelInfo() && !pVehicle->GetBaseModelInfo()->GetIsProp() && CEntity::ms_renderOnlyProps)
		return NULL;
		
	if(g_DrawVehicles == false)
		return NULL;
#endif //__BANK

	if( pVehicle->IsBaseFlagSet(fwEntity::IS_VISIBLE) == false )
	{
		return NULL;	
	}

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnAddToDrawListOfFocusEntity(), pVehicle );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddToDrawListCallingEntity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) );

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = gDrawListMgr->GetUpdateBucket();

	if( pVehicle->IsBaseFlagSet(fwEntity::SUPPRESS_ALPHA) == true && bucket == CRenderer::RB_ALPHA && renderMode != rmSpecial )
	{
		return NULL;	
	}

	if( pVehicle->GetUseOcclusionQuery() || pVehicle->m_nVehicleFlags.bRefreshVisibility )
	{
		unsigned int query = GetOcclusionQueryId();
			
		if(query == 0)
		{
			query = OcclusionQueries::OQAllocate();
			SetOcclusionQueryId(query);
		}
			
		if( query )
		{
			Vec3V min = VECTOR3_TO_VEC3V(pVehicle->GetBoundingBoxMin());
			Vec3V max = VECTOR3_TO_VEC3V(pVehicle->GetBoundingBoxMax());
			
			OcclusionQueries::OQSetBoundingBox(query, min, max, pVehicle->GetMatrix());
		}
	}
	else
	{
		u32 query = GetOcclusionQueryId();
		if(query)
		{
			OcclusionQueries::OQFree(query);
			SetOcclusionQueryId(0);
		}
	}

CCustomShaderEffectVehicle* pOrigCseHD = NULL;
	if(((CDrawListMgr*)gDrawListMgr)->IsBuildingWaterReflectionDrawList() || ((CDrawListMgr*)gDrawListMgr)->IsBuildingMirrorReflectionDrawList())
	{
		// special case: water reflection RP directly requests certain lod levels for entities (incl. vehicles)
		// current vehicle may be in HD state, but its SD drawable will be rendered anyway, so it needs to store CSE SD instead:
		CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);
		if(pVehicle->GetIsCurrentlyHD())
		{
			bool bAllowHD = true;
			gDrawListMgr->AdjustVehicleHD(false, bAllowHD);
			if(!bAllowHD)
			{
				CCustomShaderEffectVehicle* pCseSD = (CCustomShaderEffectVehicle*)GetShaderEffectSD();
				Assert(pCseSD);	// must be valid
				if(pCseSD)
				{
					pOrigCseHD = (CCustomShaderEffectVehicle*)GetShaderEffect();
					pCseSD->CopySettings(pOrigCseHD, pVehicle);	// update CseSD (B*2712498: update livery mod state)
					SetShaderEffect(pCseSD);			// swap for CseSD
				}
			}
		}
	}// IsBuildingWaterReflectionDrawList()...

	CCustomShaderEffectVehicle *pShaderEffect = static_cast<CCustomShaderEffectVehicle*>(GetShaderEffect());
	Assert(pShaderEffect);

	dlCmdBase *pReturnDC = NULL;
	if( pVehicle->m_nFlags.bAddtoMotionBlurMask )
	{
		DLC_Add(CShaderLib::BeginMotionBlurMask);
		pReturnDC = CDynamicEntityDrawHandler::AddToDrawList(pVehicle, pParams);
		DLC_Add(CShaderLib::EndMotionBlurMask);
	}
	else
	{
		pReturnDC = CDynamicEntityDrawHandler::AddToDrawList(pVehicle, pParams);
	}

	if(pReturnDC)
	{
		Assert(pReturnDC->GetInstructionIdStatic() == DC_DrawFrag);
		CDrawFragDC* pFragDC = static_cast<CDrawFragDC*>(pReturnDC);

		bool bAllowHD = true;

		// NOTE: this doesn't work here, we have to intercept this in FragDraw and FragDrawCarWheels
		// adjust HD based on current renderphase settings etc.
		//gDrawListMgr->AdjustVehicleHD(false, bAllowHD);

		if (pVehicle->GetIsCurrentlyHD() && bAllowHD)
		{
			pFragDC->SetFlag(CDrawFragDC::HD_REQUIRED);
		}

		// make sure we can identify vehicle bits belonging to player's vehicle later on...
		if (pVehicle->ContainsLocalPlayer())
		{
			pFragDC->SetFlag(CDrawFragDC::IS_LOCAL_PLAYER);
		}
		//else if (NetworkInterface::IsOnLobbyScreen())
		//{
			// Gta4: If we are in the lobby then vehicles will be rendered with high level of detail. FIX FOR BUG: 139779
		//	pFragDC->SetFlag(CDrawFragDC::IS_LOCAL_PLAYER);
		//}

		// set up wheel burst ratios in draw command
		if(pVehicle->GetNumWheels() && (!pVehicle->IsModded() || (!pVehicle->GetVariationInstance().HasCustomWheels() && !pVehicle->GetVariationInstance().HasCustomRearWheels())))
		{
			// set flag in draw command to draw wheels along with veh
			pFragDC->SetFlag(CDrawFragDC::CAR_PLUS_WHEELS);

			// copy burst ratios into draw command if any tyres are burst
			if(pShaderEffect->GetIsBurstWheel())
			{
				pFragDC->SetupBurstWheels(pShaderEffect->GetBurstAndSideRatios(0));
			}
		}
	}

	if(pOrigCseHD)
	{	// restore original CseHD back (if required):
		SetShaderEffect(pOrigCseHD);
		pOrigCseHD = NULL;
	}

	return pReturnDC;
}

// create the HD shader effect and init it with data from the normal shader effect
void CVehicleDrawHandler::ShaderEffect_HD_CreateInstance(CVehicleModelInfo *pModelInfo, CVehicle *pVehicle)
{
	static	sysSpinLockToken	sSpinlock; //has to be static or global to be shared by threads

	SYS_SPINLOCK_ENTER(sSpinlock);

	m_pStandardDetailShaderEffect = GetShaderEffect();		// store the old one

	CCustomShaderEffectVehicleType *pMasterShaderEffectType = pModelInfo->GetHDMasterCustomShaderEffect();
	if(pMasterShaderEffectType)
	{
		CCustomShaderEffectVehicle* pHDShaderEffect = NULL;
		renderAssert(pMasterShaderEffectType->GetIsHighDetail());
		pHDShaderEffect = static_cast<CCustomShaderEffectVehicle*>(pMasterShaderEffectType->CreateInstance(pVehicle));
		Assert(pHDShaderEffect);

		// copy CSE std->HD settings:
		pHDShaderEffect->CopySettings((CCustomShaderEffectVehicle*)GetShaderEffect(), pVehicle);
		// pHDShaderEffect->SetIsHighDetail(true);

		SetShaderEffect(pHDShaderEffect);
	}
	else
	{
		m_pStandardDetailShaderEffect = NULL;
	}
	SYS_SPINLOCK_EXIT(sSpinlock);
}

void CVehicleDrawHandler::ShaderEffect_HD_DestroyInstance(CVehicle *pVehicle)
{
	fwCustomShaderEffect* pHDShaderEffect = GetShaderEffect();
	renderAssert(pHDShaderEffect);
	renderAssert(pHDShaderEffect->GetType()==CSE_VEHICLE);
	renderAssert((static_cast<CCustomShaderEffectVehicle*>(pHDShaderEffect))->GetCseType()->GetIsHighDetail());

	if(m_pStandardDetailShaderEffect)
	{
		if(pHDShaderEffect)
		{
			// copy CSE HD->std settings back:
			((CCustomShaderEffectVehicle*)m_pStandardDetailShaderEffect)->CopySettings((CCustomShaderEffectVehicle*)pHDShaderEffect, pVehicle);
			pHDShaderEffect->DeleteInstance();
		}

		SetShaderEffect(m_pStandardDetailShaderEffect);
		m_pStandardDetailShaderEffect = NULL;
	}
}
