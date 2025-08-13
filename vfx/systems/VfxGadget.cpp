///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxGadget.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 Nov 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxGadget.h"

// rage

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxAttach.h"
#include "vfx/vfxwidget.h"

// game
#include "Core/Game.h"
#include "Peds/Ped.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxGadget		g_vfxGadget;

// rag tweakable settings 
//dev_float		VFXGADGET_SKI_SPRAY_MAX					= 6.0f;
//dev_float		VFXGADGET_SKI_SPRAY_DEADZONE			= 0.3f;
//dev_float		VFXGADGET_SKI_SPRAY_INSIDE_MULT			= 0.5f;
//dev_float 		VFXGADGET_SKI_SPRAY_UPRIGHT_THRESH		= 0.7f;
//dev_float 		VFXGADGET_SKI_TRAIL_SPEED_THRESH		= 15.0f;
//dev_float 		VFXGADGET_SKI_TRAIL_ANGLE_THRESH		= 0.4f;
//dev_float		VFXGADGET_SKI_TRAIL_SPEED_THRESH_MIN		= 1.0f;
//dev_float		VFXGADGET_SKI_TRAIL_SPEED_THRESH_MAX		= 10.0f;
//dev_float		VFXGADGET_SKI_TRAIL_ALPHA_MAX			= 0.5f;
//dev_float		VFXGADGET_SKI_TRAIL_REJECT_ANGLE			= 0.8f;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxGadget
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxGadget::Init								(unsigned initMode)
{	
	if (initMode == INIT_CORE)
	{
#if __BANK
		// ski spray
		//m_skiSprayDisableLeft = false;
		//m_skiSprayDisableRight = false;
		//m_skiSprayDebugOverride = 0.0f;
		//m_skiSprayDebugRenderZOffset = 0.1f;
		//m_skiSprayDebugRenderScale = 1.5f;

		// ski trails
		//m_skiTrailDisableLeft = false;
		//m_skiTrailDisableRight = false;

		// ski trails
		//m_skiTrailDisableLeft = false;
		//m_skiTrailDisableRight = false;
		//m_skiTrailUseStaticWidth = false;
		//m_skiTrailForceSnowMtl = false;
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxGadget::Shutdown							(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxSkiing
///////////////////////////////////////////////////////////////////////////////
/*
void		 	CVfxGadget::ProcessVfxSkiing					(CPed* pPed, CEntity* pSkiProp, Vec3V_In vFootVel, bool isRightSki)
{
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// only process for the player ped at present
	if (!pPed->IsPlayer())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXRANGE_PED_SKI_SQR)
	{
		return;
	}

	// get the required effect for this material
	VfxPedPtFxInfo_s* pVfxPedPtFxInfo = g_vfxPed.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(pPed->GetPackedGroundMaterialId()));
	if (pVfxPedPtFxInfo->ptFxSkiSprayHashName==0)
	{
		return;
	}

	// find the dot product of the ski's side vector with the velocity of the foot
	// this gives us a side velocity of the ski that will increase as the angle between the foot velocity and the ski increases
	// this is also proportional to the foot velocity as the footVel vector is not normalised
	//float sprayVal = DotProduct(footVel, pSkiMtx->a);

	// separate the data this time
	Vec3V vMoveDir = vFootVel;
	float footSpeed = Mag(vMoveDir).Getf();
	vMoveDir = Normalize(vMoveDir);
	Mat34V_ConstRef skiMat = pSkiProp->GetTransform().GetMatrix();
	Vec3V vSkiRight = skiMat.GetCol0();
	float skiAngle = Dot(vMoveDir, vSkiRight).Getf();

	UpdatePtFxSkiSpray(pPed, pSkiProp, skiMat, pVfxPedPtFxInfo, footSpeed, skiAngle, isRightSki);
//	UpdatePtFxSkiTrail(pPed, pSkiProp, pVfxPedPtFxInfo, footSpeed, skiAngle, isRightSki);
//	UpdateSkiTrails(pPed, pSkiProp, skiAngle, isRightSki);

#if __BANK
	if (m_skiSprayDebugRender)
	{
		Vec3V vSkiDir = skiMat.GetCol1();
		Vec3V vSkiPos = skiMat.GetCol3();

		Vec3V vZOffset(0.0f, 0.0f, m_skiSprayDebugRenderZOffset);

		Vec3V vSkiOrigin = vSkiPos+vZOffset;
		Vec3V vSkiRed = vSkiPos+(vSkiDir*ScalarVFromF32(m_skiSprayDebugRenderScale))+vZOffset;
		Vec3V vSkiGreen = vSkiPos+(ScalarVFromF32(skiAngle*footSpeed)*vSkiRight*ScalarVFromF32(m_skiSprayDebugRenderScale))+vZOffset;
		Vec3V vSkiBlue = vSkiPos+(vFootVel*ScalarVFromF32(m_skiSprayDebugRenderScale))+vZOffset;

		grcDebugDraw::Line(RCC_VECTOR3(vSkiOrigin), RCC_VECTOR3(vSkiRed), Color_red);
		grcDebugDraw::Line(RCC_VECTOR3(vSkiOrigin), RCC_VECTOR3(vSkiGreen), Color_blue);
		grcDebugDraw::Line(RCC_VECTOR3(vSkiOrigin), RCC_VECTOR3(vSkiBlue), Color_green);
	}
#endif
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdateSkiTrails
///////////////////////////////////////////////////////////////////////////////
/*
void		 	CVfxGadget::UpdateSkiTrails						(CPed* pPed, CEntity* pSkiProp, float skiAngle, bool isRightSki)
{
#if __BANK
	if (m_skiTrailDisableLeft && !isRightSki)
	{
		return;
	}

	if (m_skiTrailDisableRight && isRightSki)
	{
		return;
	}
#endif

	// get the ski trail info 	
	VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(pPed->GetPackedGroundMaterialId());
#if __BANK
	if (m_skiTrailForceSnowMtl)
	{
		vfxGroup = VFXGROUP_SNOW;
	}
#endif

	VfxPedPtFxInfo_s* pVfxPedPtFxInfo = g_vfxPed.GetInfo(vfxGroup);
	if (pVfxPedPtFxInfo->projTexInfo[VFXPEDTEXTYPE_SKI].pTexDiffuse==NULL)
	{
		// no ski trail texture is set up - just return
		return;
	}

	// reject if the ped isn't moving quickly enough
	float pedMoveSpeed = pPed->GetVelocity().Mag();
	if (pedMoveSpeed<VFXGADGET_SKI_TRAIL_SPEED_THRESH_MIN)
	{
		return;
	}

	// calculate the speed ratio (for calculating the alpha)
	float speedRatio = CVfxHelper::GetInterpValue(pedMoveSpeed, VFXGADGET_SKI_TRAIL_SPEED_THRESH_MIN, VFXGADGET_SKI_TRAIL_SPEED_THRESH_MAX);

	// find out if this is the left or right side effect 
	bool isRightSideFx = false;
	if (skiAngle>0.0f)
	{
		isRightSideFx = true;
	}

	// if the skis too far away from being straight then stop doing trails on the inside ski
	if (Abs(skiAngle)>VFXGADGET_SKI_TRAIL_REJECT_ANGLE)
	{
		if (isRightSideFx!=isRightSki)
		{
			return;
		}
	}

	// calculate the length and width of the ski
	Vec3V_ConstRef vSkiBBMin = RCC_VEC3V(pSkiProp->GetBoundingBoxMin());
	Vec3V_ConstRef vSkiBBMax = RCC_VEC3V(pSkiProp->GetBoundingBoxMax());
	float skiLength = vSkiBBMax.GetYf() - vSkiBBMin.GetYf();
	float skiWidth = vSkiBBMax.GetXf() - vSkiBBMin.GetXf();

	// get the ski matrix
	Mat34V_ConstRef skiMtx = pSkiProp->GetTransform().GetMatrix();
	Vec3V vSkiPos = skiMtx.GetCol3();
	Vec3V vSkiRight = skiMtx.GetCol0();

	// dot the ski side vector with the forward movement vector to get the width multiplier
	Vec3V vMoveVec = pPed->GetTransform().GetMatrix().GetCol1();

	fwUniqueObjId trailId = fwIdKeyGenerator::Get(pPed, VFXTRAIL_PED_OFFSET_SKI+isRightSki);
	const VfxTrailInfo_t* pLastTrailInfo = g_vfxTrail.GetLastTrailInfo(trailId);
	if (pLastTrailInfo)
	{
		// the movement is from the last trail to the current position
		vMoveVec = vSkiPos - pLastTrailInfo->vWorldPos;
		vMoveVec = Normalize(vMoveVec);
	}

	float widthDot = Abs(Dot(vSkiRight, vMoveVec)).Getf();
	float width = Max(widthDot * skiLength, skiWidth);
#if __BANK
	if (m_skiTrailUseStaticWidth)
	{
		width = skiWidth;
	}
#endif

	// force static width for the moment
	width = skiWidth;

	// set up and register the ski trail
	Vec3V vForwardCheck = pPed->GetTransform().GetMatrix().GetCol1();

	VfxTrailInfo_t vfxTrailInfo;
	vfxTrailInfo.id						= trailId;
	vfxTrailInfo.type					= VFXTRAIL_TYPE_SKI;
	vfxTrailInfo.regdEnt				= pPed->GetGroundPhysical();
	vfxTrailInfo.componentId			= pPed->GetGroundPhysicalComponent();
	vfxTrailInfo.vWorldPos				= vSkiPos;
	vfxTrailInfo.vNormal				= RCC_VEC3V(pPed->GetGroundNormal());
	vfxTrailInfo.vDir					= Vec3V(V_ZERO);			// not required as we have an id
	vfxTrailInfo.vForwardCheck			= vForwardCheck;
	vfxTrailInfo.decalRenderSettingId	= 0;		// not used for scrapes at the moment
	vfxTrailInfo.width					= width;
	vfxTrailInfo.alphaMult				= speedRatio * VFXGADGET_SKI_TRAIL_ALPHA_MAX;
	vfxTrailInfo.colR					= 255;
	vfxTrailInfo.colG					= 255;
	vfxTrailInfo.colB					= 255;
	vfxTrailInfo.pVfxMaterialInfo		= NULL;
	vfxTrailInfo.pVfxPedPtFxInfo		= pVfxPedPtFxInfo;
	vfxTrailInfo.mtlId			 		= 0;
	vfxTrailInfo.liquidPoolStartSize	= 0.0f;
	vfxTrailInfo.liquidPoolEndSize		= 0.0f;
	vfxTrailInfo.liquidPoolGrowthRate	= 0.0f;
	vfxTrailInfo.createLiquidPools		= false;
	vfxTrailInfo.dontApplyDecal			= false;

	g_vfxTrail.RegisterPoint(vfxTrailInfo);
}
*/


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSkiSpray
///////////////////////////////////////////////////////////////////////////////
/*
void		 	CVfxGadget::UpdatePtFxSkiSpray					(CPed* pPed, CEntity* pSkiProp, Mat34V_ConstRef skiMat, VfxPedPtFxInfo_s* pVfxPedPtFxInfo, float footSpeed, float skiAngle, bool isRightSki)
{
#if __BANK
	if (m_skiSprayDisableLeft && !isRightSki)
	{
		return;
	}

	if (m_skiSprayDisableRight && isRightSki)
	{
		return;
	}
#endif

	float sprayVal = skiAngle*footSpeed;

#if __BANK
	if (m_skiSprayDebugOverride!=0.0f)
	{
		sprayVal = m_skiSprayDebugOverride;
	}
#endif

	Vec3V vSkiUp = skiMat.GetCol2();

	// calc how upright the ski is to the ground (0.0 - flat on the ground; 1.0 - at right angles to the ground)
//	float skiUprightFactor = 1.0f - (Dot(RCC_VEC3F(pPed->GetGroundNormal()), skiUp));

	// now scale so this value goes from VFXGADGET_SKI_SPRAY_UPRIGHT_THRESH to 1.0
//	skiUprightFactor = VFXGADGET_SKI_SPRAY_UPRIGHT_THRESH + (skiUprightFactor*(1.0f-VFXGADGET_SKI_SPRAY_UPRIGHT_THRESH));

	// multiply this into the spray value
//	sprayVal *= skiUprightFactor;

	// calculate an evolution value for the spray based on the set range
	float sprayEvo = Abs(sprayVal);
	if (sprayEvo<VFXGADGET_SKI_SPRAY_DEADZONE)
	{
		return;
	}
	sprayEvo /= VFXGADGET_SKI_SPRAY_MAX-VFXGADGET_SKI_SPRAY_DEADZONE;
	sprayEvo = Min(sprayEvo, 1.0f);

	// find out if this is the left or right side effect (each ski has a left and right effect)
	bool isRightSideFx = false;
	if (sprayVal>0.0f)
	{
		isRightSideFx = true;
	}

	// scale down the spray evo on the 'inside' ski
	if (isRightSki != isRightSideFx)
	{
		sprayEvo *= VFXGADGET_SKI_SPRAY_INSIDE_MULT;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_SKI_SPRAY_LEFT_L+(isRightSki*2)+isRightSideFx, true, pVfxPedPtFxInfo->ptFxSkiSprayHashName, justCreated);
	if (pFxInst)
	{
		// spray evo
		vfxAssertf(sprayEvo>=0.0f, "Spray evo out of range");
		vfxAssertf(sprayEvo<=1.0f, "Spray evo out of range");
		pFxInst->SetEvoValue("spray", Abs(sprayEvo));
		
		// speed evo
		float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), 1.0f, 20.0f);
		pFxInst->SetEvoValue("speed", speedEvo);

		ptfxAttach::Attach(pFxInst, pSkiProp, -1);

		if (justCreated)
		{
			// invert the x axis for an effect on the left side of the ski
			if (isRightSideFx==false)
			{
				pFxInst->SetInvertAxes(ptxEffectInst::X_Axis);
			}

			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSkiTrail
///////////////////////////////////////////////////////////////////////////////
/*
void		 	CVfxGadget::UpdatePtFxSkiTrail					(CPed* pPed, CEntity* pSkiProp, VfxPedPtFxInfo_s* pVfxPedPtFxInfo, float footSpeed, float skiAngle, bool isRightSki)
{
#if __BANK
	if (m_skiTrailDisableLeft && !isRightSki)
	{
		return;
	}

	if (m_skiTrailDisableRight && isRightSki)
	{
		return;
	}
#endif

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_SKI_TRAIL_L+isRightSki, true, pVfxPedPtFxInfo->ptFxSkiTrailHashName, justCreated);
	if (pFxInst)
	{
		// speed evo
		float speedEvo = CVfxHelper::GetInterpValue(footSpeed, 0.0f, VFXGADGET_SKI_TRAIL_SPEED_THRESH);
		pFxInst->SetEvoValue("speed", speedEvo);

		// angle evo
		float angleEvo = CVfxHelper::GetInterpValue(Abs(skiAngle), 0.0f, VFXGADGET_SKI_TRAIL_ANGLE_THRESH);
		pFxInst->SetEvoValue("angle", angleEvo);

		ptfxAttach::Attach(pFxInst, pSkiProp, -1);

		if (justCreated)
		{
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}
*/


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxJetPackNozzle
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxGadget::UpdatePtFxJetPackNozzle				(CPed* pPed, CObject* pJetPackObject, s32 nozzleBoneIndex, float thrustDown, float thrustUp, s32 nozzleId)
{
	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_JETPACK_THRUST+nozzleId, true, "gad_clifford_thrust", justCreated);
	if (pFxInst)
	{
		float thrustEvo = 0.5f;
		if (thrustUp)
		{
			thrustEvo += thrustUp*0.5f;
		}
		if (thrustDown)
		{
			thrustEvo -= thrustDown*0.5f;
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("thrust", 0x6d6f8f43), thrustEvo);

		ptfxAttach::Attach(pFxInst, pJetPackObject, nozzleBoneIndex);

		if (justCreated)
		{
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxGadget::InitWidgets					()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Vfx Gadget", false);
	{
#if __DEV
		//pVfxBank->AddTitle("");
		//pVfxBank->AddTitle("SETTINGS");
		//pVfxBank->AddSlider("Ski Spray Max", &VFXGADGET_SKI_SPRAY_MAX, 0.0f, 25.0f, 0.05f);
		//pVfxBank->AddSlider("Ski Spray Dead Zone", &VFXGADGET_SKI_SPRAY_DEADZONE, 0.0f, 25.0f, 0.05f);
		//pVfxBank->AddSlider("Ski Spray Inside Mult", &VFXGADGET_SKI_SPRAY_INSIDE_MULT, 0.0f, 1.0f, 0.01f);
		//pVfxBank->AddSlider("Ski Spray Upright Thresh", &VFXGADGET_SKI_SPRAY_UPRIGHT_THRESH, 0.0f, 1.0f, 0.01f);
		//pVfxBank->AddSlider("Ski Trail Speed Thresh", &VFXGADGET_SKI_TRAIL_SPEED_THRESH, 0.0f, 50.0f, 0.25f);
		//pVfxBank->AddSlider("Ski Trail Angle Thresh", &VFXGADGET_SKI_TRAIL_ANGLE_THRESH, 0.0f, 50.0f, 0.25f);
		//pVfxBank->AddSlider("Ski Trail Speed Thresh Min", &VFXGADGET_SKI_TRAIL_SPEED_THRESH_MIN, 0.0f, 20.0f, 0.1f);
		//pVfxBank->AddSlider("Ski Trail Speed Thresh Max", &VFXGADGET_SKI_TRAIL_SPEED_THRESH_MAX, 0.0f, 20.0f, 0.1f);
		//pVfxBank->AddSlider("Ski Trail Alpha Max", &VFXGADGET_SKI_TRAIL_ALPHA_MAX, 0.0f, 1.0f, 0.01f);
		//pVfxBank->AddSlider("Ski Trail Reject Angle", &VFXGADGET_SKI_TRAIL_REJECT_ANGLE, 0.0f, 1.0f, 0.01f);
#endif

		//pVfxBank->AddTitle("DEBUG SKI SPRAY");
		//pVfxBank->AddToggle("Disable Left", &m_skiSprayDisableLeft);
		//pVfxBank->AddToggle("Disable Right", &m_skiSprayDisableRight);
		//pVfxBank->AddToggle("Debug Render", &m_skiSprayDebugRender);
		//pVfxBank->AddSlider("Debug Z Offset", &m_skiSprayDebugRenderZOffset, 0.0f, 1.0f, 0.01f);
		//pVfxBank->AddSlider("Debug Scale", &m_skiSprayDebugRenderScale, 0.0f, 10.0f, 0.01f);
		//pVfxBank->AddSlider("Override", &m_skiSprayDebugOverride, -1.0f, 1.0f, 0.01f);

		//pVfxBank->AddTitle("DEBUG SKI TRAIL");
		//pVfxBank->AddToggle("Disable Left", &m_skiTrailDisableLeft);
		//pVfxBank->AddToggle("Disable Right", &m_skiTrailDisableRight);

		//pVfxBank->AddTitle("DEBUG SKI TRAILS");
		//pVfxBank->AddToggle("Disable Left", &m_skiTrailDisableLeft);
		//pVfxBank->AddToggle("Disable Right", &m_skiTrailDisableRight);
		//pVfxBank->AddToggle("Use Static Width", &m_skiTrailUseStaticWidth);
		//pVfxBank->AddToggle("Force Snow Material", &m_skiTrailForceSnowMtl);
	}
	pVfxBank->PopGroup();
}
#endif

