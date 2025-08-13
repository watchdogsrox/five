//
// Filename:	CustomShaderEffectProp.h
// Description:	Class for controlling prop shaders variables
// Written by:	Andrzej
//
//	12/08/2010 -	Andrzej:	- initial;
//
//
//
//

// Rage headers
#include "grmodel/ShaderFx.h"
#include "grmodel/Geometry.h"
#include "grcore/texturereference.h"
#include "pheffects/wind.h"

#include "creature/creature.h"
#include "creature/componentshadervars.h"
#include "rmcore/drawable.h"

// Game headers
#include "debug/debug.h"
#include "peds/Ped.h"
#include "scene/Entity.h"
#include "CustomShaderEffectProp.h"


#define PROP_DEFAULT_WIND_MOTION_FREQ_X		(0.001f)
#define PROP_DEFAULT_WIND_MOTION_FREQ_Y		(0.001f)
#define PROP_DEFAULT_WIND_MOTION_AMP_X		(1.5f)
#define PROP_DEFAULT_WIND_MOTION_AMP_Y		(0.1f)
#define PROP_DEFAULT_WIND_RESPONSE_VEL_MIN	(0.0f)
#define PROP_DEFAULT_WIND_RESPONSE_VEL_MAX	(50.0f)
#define PROP_DEFAULT_WIND_RESPONSE_VEL_EXP	(0.0f)
#define PROP_DEFAULT_WIND_RESPONSE_AMOUNT	(25.0f)

#if CSE_PROP_EDITABLEVALUES
	bool	CCustomShaderEffectProp::ms_bEVEnabled					 = false;
	Vector4 CCustomShaderEffectProp::ms_vEVWrinkleMaskStrengths[4]	 = {0};
	Vector4	CCustomShaderEffectProp::ms_fEVUmGlobalParams			 = Vector4(0.025f, 0.025f, 6.000f, 6.000f);
	bool	CCustomShaderEffectProp::ms_bEVUseUmGlobalOverrideValues = false;
	Vector4	CCustomShaderEffectProp::ms_fEVUmGlobalOverrideParams	 = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	u32		CCustomShaderEffectProp::ms_nEVDiffuseTexId				 = 0;

	bool	CCustomShaderEffectProp::ms_bEVWindMotionEnabled		= true;
	float	CCustomShaderEffectProp::ms_fEVWindMotionFreqScale		= 1.0f;
	float	CCustomShaderEffectProp::ms_fEVWindMotionFreqX			= PROP_DEFAULT_WIND_MOTION_FREQ_X;
	float	CCustomShaderEffectProp::ms_fEVWindMotionFreqY			= PROP_DEFAULT_WIND_MOTION_FREQ_Y;
	float	CCustomShaderEffectProp::ms_fEVWindMotionAmpScale		= 1.0f;
	float	CCustomShaderEffectProp::ms_fEVWindMotionAmpX			= PROP_DEFAULT_WIND_MOTION_AMP_X;
	float	CCustomShaderEffectProp::ms_fEVWindMotionAmpY			= PROP_DEFAULT_WIND_MOTION_AMP_Y;
	bool	CCustomShaderEffectProp::ms_bEVWindResponseEnabled		= true;
	float	CCustomShaderEffectProp::ms_fEVWindResponseAmount		= PROP_DEFAULT_WIND_RESPONSE_AMOUNT;
	float	CCustomShaderEffectProp::ms_fEVWindResponseVelMin		= PROP_DEFAULT_WIND_RESPONSE_VEL_MIN;
	float	CCustomShaderEffectProp::ms_fEVWindResponseVelMax		= PROP_DEFAULT_WIND_RESPONSE_VEL_MAX;
	float	CCustomShaderEffectProp::ms_fEVWindResponseVelExp		= PROP_DEFAULT_WIND_RESPONSE_VEL_EXP;
#endif

//
//
//
//
static inline u8 PropShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectPropType::Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bIsParachute, bool bSwapableDiffTex, bool bTint)
{
	Assert(pDrawable);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	const u32 uModelNameHash = pMI? pMI->GetModelNameHash() : 0;

	// create cascaded tint type:
	if(bTint)
	{
		m_pTintType = CCustomShaderEffectTintType::Create(pDrawable, pMI);
		if(!m_pTintType->Initialise(pDrawable))
		{
			m_pTintType->RemoveRef();
			m_pTintType = NULL;
			Assertf(0, "Error initialising CustomShaderEffectTint!");
			return(FALSE);
		}
	}
	else
	{
		m_pTintType = NULL;
	}

	if (pShaderGroup->LookupVar("WrinkleMaskStrengths0", FALSE))
	{
		m_idVarWrinkleMaskStrengths0 = PropShaderGroupLookupVar(pShaderGroup, "WrinkleMaskStrengths0", FALSE);
		m_idVarWrinkleMaskStrengths1 = PropShaderGroupLookupVar(pShaderGroup, "WrinkleMaskStrengths1", FALSE);
		m_idVarWrinkleMaskStrengths2 = PropShaderGroupLookupVar(pShaderGroup, "WrinkleMaskStrengths2", FALSE);
		m_idVarWrinkleMaskStrengths3 = PropShaderGroupLookupVar(pShaderGroup, "WrinkleMaskStrengths3", FALSE);
	}
	else
	{
		m_idVarWrinkleMaskStrengths0 = grmsgvNONE;
		m_idVarWrinkleMaskStrengths1 = grmsgvNONE;
		m_idVarWrinkleMaskStrengths2 = grmsgvNONE;
		m_idVarWrinkleMaskStrengths3 = grmsgvNONE;
	}

	m_idVarUmGlobalOverrideParams	= PropShaderGroupLookupVar(pShaderGroup, "umGlobalOverrideParams0", FALSE);

	m_bIsParachute					= bIsParachute;


	m_bSwapableDiffTex				= false;
	m_pDiffTexIdxTab				= NULL;

	if(bSwapableDiffTex)
	{
		m_bSwapableDiffTex = true;
		m_idVarDiffuseTex = PropShaderGroupLookupVar(pShaderGroup, "DiffuseTex", TRUE);
		
		pgDictionary<grcTexture> *pTexDict = GetTexDict(pDrawable);
		Assertf(pTexDict, "Texture dictionary not found in %s!", pMI->GetModelName());

		if(pTexDict)
		{
			m_pDiffTexIdxTab = rage_new atArray<s8>(CSE_PROP_MAX_DIFF_TEXTURES, CSE_PROP_MAX_DIFF_TEXTURES);
			atArray<s8>& diffTexIdxTab = *m_pDiffTexIdxTab;

			// choose base diff name (every bag uses different one):
			char const *pParaBagDiffBaseName = "p_para_bag_pilot_s_diff_";
			switch(uModelNameHash)
			{
				case(MID_PIL_P_PARA_BAG_PILOT_S):
					pParaBagDiffBaseName = "p_para_bag_pilot_s_diff_";
					break;
				case(MID_LTS_P_PARA_BAG_LTS_S):
					pParaBagDiffBaseName = "p_para_bag_lts_s_diff_";
					break;
				case(MID_LTS_P_PARA_BAG_PILOT2_S):
					pParaBagDiffBaseName = "p_para_bag_pilot2_s_diff_";
					break;
				case(MID_P_PARA_BAG_XMAS_S):
					pParaBagDiffBaseName = "p_para_bag_xmas_s_diff_";
					break;
				case(MID_VW_P_PARA_BAG_VINE_S):
					pParaBagDiffBaseName = "p_para_bag_vine_s_diff_";
					break;
				case(MID_P_PARA_BAG_TR_S_01A):
				case(MID_TR_P_PARA_BAG_TR_S_01A):
					pParaBagDiffBaseName = "p_para_bag_tr_s_01a_diff_";
					break;
				case(MID_REH_P_PARA_BAG_REH_S_01A):
					pParaBagDiffBaseName = "p_para_bag_reh_s_01a_diff_";
					break;
				default:
					Assertf(false, "Unrecognized parachute bag!");
					break;
			}

			// inititialize DiffuseTex remap table with available textures in texture dictionary:
			for(u32 i=0; i<CSE_PROP_MAX_DIFF_TEXTURES; i++)
			{
				char texName[256];
				char suffix[2] = "a";

				// p_para_bag_pilot_s_diff_a/b/c/d/...
				suffix[0] += (char)i;	// a,b,c,d,...
				sprintf(texName, "%s%s", pParaBagDiffBaseName, suffix);

				int idxDiffTex = pTexDict->LookupLocalIndex( pgDictionary<grcTexture>::ComputeHash(texName) );
				Assert((idxDiffTex==-1) || (idxDiffTex < 128));	// must fit into s8 and may be -1

				diffTexIdxTab[i] = (s8)idxDiffTex;
			}

			pTexDict->AddRef();
		}//if(pTextDict)...
	}// if(bSwapableDiffTex)...

	return(TRUE);
}// end of CCustomShaderEffectProp::Initialise()...


//
//
//
//
CCustomShaderEffectPropType::~CCustomShaderEffectPropType()
{
	if(m_pTintType)
	{
		m_pTintType->RemoveRef();
		m_pTintType = NULL;
	}

	if(m_pDiffTexIdxTab)
	{
		delete m_pDiffTexIdxTab;
		m_pDiffTexIdxTab = NULL;
	}
}

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectPropType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectProp *pNewShaderEffect = rage_new CCustomShaderEffectProp;

	if(this->m_pTintType)
	{
		pNewShaderEffect->m_pTint = (CCustomShaderEffectTint*)( m_pTintType->CreateInstance(pEntity) );	// cascaded CSE tint instance
	}
	else
	{
		pNewShaderEffect->m_pTint = NULL;
	}

	pNewShaderEffect->m_pType = this;

	return(pNewShaderEffect);
}

//
//
//
//
bool CCustomShaderEffectPropType::RestoreModelInfoDrawable(rmcDrawable *pDrawable)
{
	if(m_bSwapableDiffTex)
	{
		pgDictionary<grcTexture> *pTexDict = GetTexDict(pDrawable);
		if(pTexDict)
		{
			pTexDict->Release();
		}
	}

	if(this->m_pTintType)
	{
		return m_pTintType->RestoreModelInfoDrawable(pDrawable);
	}
	else
	{
		return(true);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////

//
//
//
//
CCustomShaderEffectProp::CCustomShaderEffectProp()	: CCustomShaderEffectBase(sizeof(*this), CSE_PROP)
{
	sysMemSet(m_wrinkleMaskStrengths, 0, sizeof(m_wrinkleMaskStrengths));

	SetUmGlobalOverrideParams(1.0f, 1.0f, 1.0f, 1.0f);
	m_nDiffuseTexIdx = 0;
}

//
//
//
//
CCustomShaderEffectProp::~CCustomShaderEffectProp()
{
	if(m_pTint)
	{
		delete m_pTint;
		m_pTint = NULL;
	}
}


void CCustomShaderEffectProp::AddToDrawList(u32 modelIndex, bool bExecute)
{
	CCustomShaderEffectTint *tmpTint = m_pTint; // save cascaded tint ptr
	m_pTint = NULL;		// remove cascaded link for DLC
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
	m_pTint = tmpTint;	// restore ptr

	if(m_pTint)
	{
		m_pTint->AddToDrawList(modelIndex, bExecute);
	}
}

void CCustomShaderEffectProp::AddToDrawListAfterDraw()
{
	if(m_pTint)
	{
		m_pTint->AddToDrawListAfterDraw();
	}
}



//
//
//
//
void CCustomShaderEffectProp::Update(fwEntity* pEntityBase)
{
	CEntity* pEntity = static_cast<CEntity*>(pEntityBase);
	Assert (pEntity);

	// TODO: Animation update for m_wrinkleMaskStrengths[]....

	if(m_pTint)
	{
		m_pTint->Update(pEntity);
	}

	if(m_pType->m_bIsParachute)
	{
		CPed *pParachutingPed = NULL;

		CEntity* pParentEntity = static_cast<CEntity*>(pEntity->GetAttachParent());
		if(pParentEntity)
		{	// parachute is still folded: ped is parent to parachute:
			// Remove the assert since we now have vehicles that can parachute
			//Assertf(pParentEntity->GetIsTypePed()==true, "Parachute has parent which is NOT a ped?");
			if(pParentEntity->GetIsTypePed())
			{
				pParachutingPed = NULL;	//static_cast<CPed*>(pParentEntity);
			}
		}

		if(!pParachutingPed)
		{	// parachute is unfolded: parachute is parent to ped:
			CEntity* pChildEntity = static_cast<CEntity*>(pEntity->GetChildAttachment());
			if(pChildEntity)
			{
				// Remove the assert since we now have vehicles that can parachute
				//Assertf(pChildEntity->GetIsTypePed()==true, "Parachute has child which is NOT a ped?");
				if(pChildEntity->GetIsTypePed())
				{
					pParachutingPed = static_cast<CPed*>(pChildEntity);
				}
			}
		}
		
		if(pParachutingPed)
		{
			const float fWindyClothingScale = pParachutingPed->GetWindyClothingScale();
			if(fWindyClothingScale > 0.0f && pParachutingPed->GetIsParachuting())
			{
				const float umOverride = Lerp(fWindyClothingScale, 1.0f, 3.0f);
				this->SetUmGlobalOverrideParams(umOverride, umOverride, 1.0f, 1.0f);
			}
			else
			{
				//if nothing is affecting micromovement then allow wind to do it

				//initial wind amount is zero as we dont have an ambient micromovement like plants and trees
				float windAmount = 0.0f;
			#if CSE_PROP_EDITABLEVALUES
				if (ms_bEVWindResponseEnabled && (ms_fEVWindResponseAmount > 0.0f))
			#endif
				{
					Vec3V wind;
					//Using global velocity as ped causes disturbances in local velocity
					//const Vec3V entityPos = pEntity->GetTransform().GetPosition();
					//WIND.GetLocalVelocity(entityPos, wind);
					WIND.GetGlobalVelocity(WIND_TYPE_AIR, wind);
					float windVel = Mag(wind).Getf();
					const float windVelMin = BANK_SWITCH(ms_fEVWindResponseVelMin, PROP_DEFAULT_WIND_RESPONSE_VEL_MIN);
					const float windVelMax = BANK_SWITCH(ms_fEVWindResponseVelMax, PROP_DEFAULT_WIND_RESPONSE_VEL_MAX);
					const float windVelExp = BANK_SWITCH(ms_fEVWindResponseVelExp, PROP_DEFAULT_WIND_RESPONSE_VEL_EXP);

					//Calculate wind response in [0.0, 1.0f]
					float windResponse = Clamp<float>((windVel - windVelMin)/(windVelMax - windVelMin), 0.0f, 1.0f);

					if (windVelExp != 0.0f)
					{
						windResponse = powf(windResponse, powf(2.0f, windVelExp));
					}

					//Scale wind response
					windAmount += windResponse*BANK_SWITCH(ms_fEVWindResponseAmount, PROP_DEFAULT_WIND_RESPONSE_AMOUNT);
				}

				//set override params based on some parameters
				// Higher winds causes higher amplitude and higher frequency (looks like cloth flutter)
				this->SetUmGlobalOverrideParams(
						BANK_SWITCH(ms_fEVWindMotionAmpX*ms_fEVWindMotionAmpScale,	PROP_DEFAULT_WIND_MOTION_AMP_X)		BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * windAmount,
						BANK_SWITCH(ms_fEVWindMotionAmpY*ms_fEVWindMotionAmpScale,	PROP_DEFAULT_WIND_MOTION_AMP_Y)		BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * windAmount,
						BANK_SWITCH(ms_fEVWindMotionFreqX*ms_fEVWindMotionFreqScale,PROP_DEFAULT_WIND_MOTION_FREQ_X)	BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * windAmount,
						BANK_SWITCH(ms_fEVWindMotionFreqY*ms_fEVWindMotionFreqScale,PROP_DEFAULT_WIND_MOTION_FREQ_Y)	BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * windAmount
						);
			}
		}
	}//	if(m_pType->m_bIsParachute)...

}// end of Update()...


//
//
// called from CPedVariationMgr::RenderCompositePed():
//
void CCustomShaderEffectProp::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
	Assert(shaderGroup.GetShaderGroupVarCount() > 0);

	CCustomShaderEffectPropType *pType = m_pType;
	if (!pType)
		return;

	if (pType->m_idVarWrinkleMaskStrengths0)
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarWrinkleMaskStrengths0, m_wrinkleMaskStrengths[0]);
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarWrinkleMaskStrengths1, m_wrinkleMaskStrengths[1]);
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarWrinkleMaskStrengths2, m_wrinkleMaskStrengths[2]);
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarWrinkleMaskStrengths3, m_wrinkleMaskStrengths[3]);
	}

	if(pType->m_bIsParachute && pType->m_idVarUmGlobalOverrideParams)
	{
		Vector4 umOverrideParamsV4;
		umOverrideParamsV4.x = m_umGlobalOverrideParams.x.GetFloat32_FromFloat16();
		umOverrideParamsV4.y = m_umGlobalOverrideParams.y.GetFloat32_FromFloat16();
		umOverrideParamsV4.z = m_umGlobalOverrideParams.z.GetFloat32_FromFloat16();
		umOverrideParamsV4.w = m_umGlobalOverrideParams.w.GetFloat32_FromFloat16();
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarUmGlobalOverrideParams, umOverrideParamsV4);
	}

	if(pType->m_bSwapableDiffTex && pType->m_idVarDiffuseTex)
	{
		pgDictionary<grcTexture> *pTexDict = pType->GetTexDict(pDrawable);
		if(pTexDict)
		{
			atArray<s8>& diffTexIdxTab = *(pType->m_pDiffTexIdxTab);
			const int entry = diffTexIdxTab[m_nDiffuseTexIdx];
			if(entry != -1)
			{
				grcTexture *pDiffuseTex = pTexDict->GetEntry(entry);
				Assert(pDiffuseTex);
				shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex, pDiffuseTex);
			}
		}
	}

#if CSE_PROP_EDITABLEVALUES
	CCustomShaderEffectProp::SetEditableShaderValues(&shaderGroup, pDrawable, pType);
#endif

}// end of PedVariation_SetShaderVariables()...


// um global override params:
inline void	 CCustomShaderEffectProp::SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV)
{
	m_umGlobalOverrideParams.x.SetFloat16_FromFloat32(scaleH);
	m_umGlobalOverrideParams.y.SetFloat16_FromFloat32(scaleV);
	m_umGlobalOverrideParams.z.SetFloat16_FromFloat32(argFreqH);
	m_umGlobalOverrideParams.w.SetFloat16_FromFloat32(argFreqV);
}

// um global override params:
inline void	 CCustomShaderEffectProp::SetUmGlobalOverrideParams(float s)
{
Float16 s16(s);
	m_umGlobalOverrideParams.x =
	m_umGlobalOverrideParams.y =
	m_umGlobalOverrideParams.z =
	m_umGlobalOverrideParams.w = s16;
}

void CCustomShaderEffectProp::GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV)
{
	if(pScaleH)
	{
		*pScaleH = m_umGlobalOverrideParams.x.GetFloat32_FromFloat16(); 
	}

	if(pScaleV)
	{
		*pScaleV = m_umGlobalOverrideParams.y.GetFloat32_FromFloat16();
	}

	if(pArgFreqH)
	{
		*pArgFreqH = m_umGlobalOverrideParams.z.GetFloat32_FromFloat16();
	}

	if(pArgFreqV)
	{
		*pArgFreqV = m_umGlobalOverrideParams.w.GetFloat32_FromFloat16();
	}
}

//
//
//
//
u32 CCustomShaderEffectProp::GetNumTintPalettes()
{
	if(m_pTint)
	{
		return m_pTint->GetNumTintPalettes();
	}

	return(0);
}


#if CSE_PROP_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectProp::SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectPropType *pType)
{
	Assert(pShaderGroup);
	Assert(pType);

	if(!ms_bEVEnabled)
		return;

	SetVector4ShaderGroupVar(pShaderGroup,	"WrinkleMaskStrengths0", ms_vEVWrinkleMaskStrengths[0]);
	SetVector4ShaderGroupVar(pShaderGroup,	"WrinkleMaskStrengths1", ms_vEVWrinkleMaskStrengths[1]);
	SetVector4ShaderGroupVar(pShaderGroup,	"WrinkleMaskStrengths2", ms_vEVWrinkleMaskStrengths[2]);
	SetVector4ShaderGroupVar(pShaderGroup,	"WrinkleMaskStrengths3", ms_vEVWrinkleMaskStrengths[3]);

	SetVector4ShaderGroupVar(pShaderGroup,	"umGlobalParams0",			ms_fEVUmGlobalParams);

	if(ms_bEVUseUmGlobalOverrideValues)
	{
		SetVector4ShaderGroupVar(pShaderGroup,	"umGlobalOverrideParams0",	ms_fEVUmGlobalOverrideParams);
	}
	
	if(pType->m_bSwapableDiffTex && pType->m_idVarDiffuseTex)
	{
		pgDictionary<grcTexture> *pTexDict = pType->GetTexDict(pDrawable);
		if(pTexDict)
		{
			atArray<s8>& diffTexIdxTab = *(pType->m_pDiffTexIdxTab);
			const int entry = diffTexIdxTab[ms_nEVDiffuseTexId];
			if(entry != -1)
			{
				grcTexture *pDiffuseTex = pTexDict->GetEntry(entry);
				Assert(pDiffuseTex);
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex, pDiffuseTex);
			}
		}
	}

}// end of SetEditableShaderValues()...


//
//
//
//
bool CCustomShaderEffectProp::InitWidgets(bkBank& bank)
{
	// debug widgets:
	bank.PushGroup("Prop Editable Shaders Values", false);
		bank.AddToggle("Enable",	&ms_bEVEnabled);

		bank.AddSlider("Force Swapable Diffuse Tex",	&ms_nEVDiffuseTexId,				0, CSE_PROP_MAX_DIFF_TEXTURES-1, 1);

		bank.PushGroup("Prop Wrinkle Map", false);
			for(int i=0;i<4;i++)
			{
				char buffer[256];
				::sprintf(buffer, "wrinkle mask strengths %d", i);
				bank.AddSlider(buffer, &ms_vEVWrinkleMaskStrengths[i], -1.0f, 2.0f, 0.1f);
			}
		bank.PopGroup();

		bank.PushGroup("uMovements", true);
			bank.AddSlider("Global ScaleH",				(float*)&ms_fEVUmGlobalParams.x,	0.0f, 0.1f,  0.001f);
			bank.AddSlider("Global ScaleV",				(float*)&ms_fEVUmGlobalParams.y,	0.0f, 0.1f,  0.001f);
			bank.AddSlider("Global ArgFreqH",			(float*)&ms_fEVUmGlobalParams.z,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global ArgFreqV",			(float*)&ms_fEVUmGlobalParams.w,	0.0f, 10.0f, 0.001f);
			bank.AddSeparator();
			bank.AddToggle("Use Global Override Values", &ms_bEVUseUmGlobalOverrideValues);
			bank.AddSlider("Global Override ScaleH",	(float*)&ms_fEVUmGlobalOverrideParams.x,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ScaleV",	(float*)&ms_fEVUmGlobalOverrideParams.y,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ArgFreqH",	(float*)&ms_fEVUmGlobalOverrideParams.z,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ArgFreqV",	(float*)&ms_fEVUmGlobalOverrideParams.w,	0.0f, 10.0f, 0.001f);
		bank.PopGroup();

		bank.PushGroup("Wind Movement", true);
			bank.AddToggle("Motion Enabled",	&ms_bEVWindMotionEnabled);
			bank.AddSlider("Motion Freq Scale",	&ms_fEVWindMotionFreqScale,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Motion Freq X",		&ms_fEVWindMotionFreqX,			0.0f, 10.0f, 1.0f/256.0f);
			bank.AddSlider("Motion Freq Y",		&ms_fEVWindMotionFreqY,			0.0f, 10.0f, 1.0f/256.0f);
			bank.AddSlider("Motion Amp Scale",	&ms_fEVWindMotionAmpScale,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Motion Amp X",		&ms_fEVWindMotionAmpX,			0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Motion Amp Y",		&ms_fEVWindMotionAmpY,			0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSeparator();
			bank.AddToggle("Response Enabled",	&ms_bEVWindResponseEnabled);
			bank.AddSlider("Response Amount",	&ms_fEVWindResponseAmount,		0.0f, 50.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Min",	&ms_fEVWindResponseVelMin,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Max",	&ms_fEVWindResponseVelMax,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Exp",	&ms_fEVWindResponseVelExp,		-4.0f, 4.0f, 1.0f/256.0f);
		bank.PopGroup();
	bank.PopGroup();

	return(TRUE);
}// end of InitWidgets()...

#endif //CSE_PROP__EDITABLEVALUES...


