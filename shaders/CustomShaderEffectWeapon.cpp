//
// Filename:	CustomShaderEffectWeapon.cpp
// Description:	Class for controlling weapon shader variables
// Written by:	Andrzej
//
//	16/02/2012 -	Andrzej:	- initial;
//
//
//
//

// Rage headers
#include "grmodel\ShaderFx.h"
#include "grmodel\Geometry.h"
#include "grcore\texturereference.h"

#include "rmcore/drawable.h"

// Game headers
#include "debug\debug.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "scene\Entity.h"
#include "scene\world\GameWorld.h"
#include "CustomShaderEffectWeapon.h"

//#include "system/findsize.h"
//FindSize(CCustomShaderEffectWeapon);

#if CSE_WEAPON_EDITABLEVALUES
	bool	CCustomShaderEffectWeapon::ms_bEVEnabled				= false;
	float	CCustomShaderEffectWeapon::ms_fEVSpecFalloff			= 100.0f;
	float	CCustomShaderEffectWeapon::ms_fEVSpecIntensity			= 0.125f;
	float	CCustomShaderEffectWeapon::ms_fEVSpecFresnel			= 0.97f;
	float	CCustomShaderEffectWeapon::ms_fEVSpec2Factor			= 40.0f;
	float	CCustomShaderEffectWeapon::ms_fEVSpec2ColorIntensity	= 1.0f;
	Color32	CCustomShaderEffectWeapon::ms_fEVSpec2Color				= Color32(255,255,255,255);
	u32		CCustomShaderEffectWeapon::ms_fEVDiffuseTexPalSelector	= 0;
	u32		CCustomShaderEffectWeapon::ms_fEVCamoDiffuseTexIdx		= 0;
#endif


//
//
//
//
inline u8 WeaponShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

inline u8 WeaponEffectLookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar  varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectWeaponType::Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bTint, bool bPalette)
{
	Assert(pDrawable);
	Assert( u32(bTint)+u32(bPalette) >= 1);	// can't have both tint and palette enabled!

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();


	m_bHasTint  = false;

	// create cascaded tint type:
	if(bTint)
	{
		if (!m_pTintType)
		{
			m_pTintType = CCustomShaderEffectTintType::Create(pDrawable, pMI);
		}

		if(!m_pTintType->Initialise(pDrawable))
		{
			m_pTintType->RemoveRef();
			m_pTintType = NULL;
			Assertf(0, "Error initialising CustomShaderEffectTint!");
			return(FALSE);
		}

		m_bHasTint  = true;
	}
	else
	{
		m_pTintType = NULL;
	}

	
	m_bHasPalette				= false;
	m_idVarDiffuseTexPalSelector= 0;

	m_bSwappableCamoTex			= false;
	m_pCamoDiffTexIdxTab		= NULL;
	m_pTexDict					= NULL;
	m_pCamoShaderDiffTexRemap	= NULL;

	if(bPalette)
	{
		m_bHasPalette = true;
		m_idVarDiffuseTexPalSelector = WeaponShaderGroupLookupVar(pShaderGroup, "DiffuseTexPaletteSelector", TRUE);


		// swappable textures for camo:
		if(	pShaderGroup->LookupShader(CSE_WEAPON_CAMO_SHADER1_NAME)	||
			pShaderGroup->LookupShader(CSE_WEAPON_CAMO_SHADER2_NAME)	||
			false)
		{
			m_bSwappableCamoTex = true;

			m_pTexDict = g_TxdStore.Get(strLocalIndex(pMI->GetAssetParentTxdIndex()));
			Assertf(m_pTexDict, "Missing txd for weapon %s", pMI->GetModelName());
			if(m_pTexDict)
			{
				m_pCamoDiffTexIdxTab = rage_new atArray<s8>(CSE_WEAPON_MAX_CAMO_TEXTURES, CSE_WEAPON_MAX_CAMO_TEXTURES);
				atArray<s8>& camoDiffTexIdxTab = *m_pCamoDiffTexIdxTab;

				// inititialize DiffuseTex remap table with available textures in texture dictionary:
				for(u32 i=0; i<CSE_WEAPON_MAX_CAMO_TEXTURES; i++)
				{
					char texName[256];
					// w_camo_1/2/3...
					sprintf(texName, "w_camo_%d", i+1);

					int idxDiffTex = m_pTexDict->LookupLocalIndex( pgDictionary<grcTexture>::ComputeHash(texName) );
					Assert((idxDiffTex==-1) || (idxDiffTex < 128));	// must fit into s8 and may be -1

					camoDiffTexIdxTab[i] = (s8)idxDiffTex;
				}

				m_pTexDict->AddRef();
			}// if(m_pTexDict)...

			const u32 numShaders = pShaderGroup->GetCount();
			u32 numCamoShaders=0;
			for(u32 i=0; i<numShaders; i++)
			{
				grmShader *pShader = pShaderGroup->GetShaderPtr(i);
				const u32 nShaderHashCode = pShader->GetHashCode();
				if( (nShaderHashCode==CSE_WEAPON_CAMO_SHADER1_NAMEHASH) ||
					(nShaderHashCode==CSE_WEAPON_CAMO_SHADER2_NAMEHASH)	||
					false
				)
				{
					numCamoShaders++;
				}
			}

			Assert(numCamoShaders > 0);
			
			m_pCamoShaderDiffTexRemap = rage_new atArray<shaderDiffuseTex>(numCamoShaders, numCamoShaders);
			atArray<shaderDiffuseTex>& camoShaderDiffTexRemap = *m_pCamoShaderDiffTexRemap;

			const u32 diffuseTexHash = atStringHash("DiffuseTex");
			u32 camoShaderIdx=0;
			for(u32 i=0; i<numShaders; i++)
			{
				grmShader *pShader = pShaderGroup->GetShaderPtr(i);
				const u32 nShaderHashCode = pShader->GetHashCode();
				if(	(nShaderHashCode==CSE_WEAPON_CAMO_SHADER1_NAMEHASH)	||
					(nShaderHashCode==CSE_WEAPON_CAMO_SHADER2_NAMEHASH)	||
					false
				)
				{
					Assert(i < 256);	// must fit into u8
					camoShaderDiffTexRemap[camoShaderIdx].m_ShaderIdx = (u8)i;
					
					u32 diffuseTexId = (u32)pShader->LookupVarByHash(diffuseTexHash);
					Assert((diffuseTexId > 0) && (diffuseTexId < 256));	// must fit into u8
					camoShaderDiffTexRemap[camoShaderIdx].m_idVarDiffuseTex = (u8)diffuseTexId; 
					
					camoShaderIdx++;
				}
			}
		}//if(pShaderGroup->LookupShader(CSE_WEAPON_CAMO_SHADER_NAME))...
	}//if(bPalette)...


	m_idVarSpecFalloffMult	= WeaponShaderGroupLookupVar(pShaderGroup, "Specular", TRUE);
	m_idVarSpecIntMult		= WeaponShaderGroupLookupVar(pShaderGroup, "SpecularColor", TRUE);
	m_idVarSpecFresnel		= WeaponShaderGroupLookupVar(pShaderGroup, "Fresnel", TRUE);
	m_idVarSpec2Factor		= WeaponShaderGroupLookupVar(pShaderGroup, "Specular2Factor", TRUE);
	m_idVarSpec2ColorInt	= WeaponShaderGroupLookupVar(pShaderGroup, "Specular2ColorIntensity", TRUE);
	m_idVarSpec2Color		= WeaponShaderGroupLookupVar(pShaderGroup, "Specular2Color", TRUE);

	SetIsHighDetail(false);

	return(TRUE);
}// end of CCustomShaderEffectWeapon::Initialise()...

CCustomShaderEffectWeaponType::~CCustomShaderEffectWeaponType()
{
	if(m_pTintType)
	{
		m_pTintType->RemoveRef();
		m_pTintType = NULL;
	}

	if(m_pCamoDiffTexIdxTab)
	{
		delete m_pCamoDiffTexIdxTab;
		m_pCamoDiffTexIdxTab = NULL;
	}

	if(m_pTexDict)
	{
		m_pTexDict->Release();
		m_pTexDict = NULL;
	}

	if(m_pCamoShaderDiffTexRemap)
	{
		delete m_pCamoShaderDiffTexRemap;
		m_pCamoShaderDiffTexRemap = NULL;
	}
}

void CCustomShaderEffectWeaponType::SetIsHighDetail(bool flag)
{
	m_bIsHighDetail = flag;
	if(m_pTintType)
	{
		m_pTintType->SetIsHighDetail(flag);
	}
}

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectWeaponType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectWeapon *pNewShaderEffect = rage_new CCustomShaderEffectWeapon;

	pNewShaderEffect->m_pType = this;

	if(this->m_pTintType)
	{
		pNewShaderEffect->m_pTint = (CCustomShaderEffectTint*)(m_pTintType->CreateInstance(pEntity));	// cascaded CSE tint instance
	}
	else
	{
		pNewShaderEffect->m_pTint = NULL;
	}

	pNewShaderEffect->m_varDiffuseTexPalSelector = 0;
	pNewShaderEffect->m_varCamoDiffuseTexIdx = 0;

	// default values:
	pNewShaderEffect->m_fSpecFalloffMult	= 100.0f;
	pNewShaderEffect->m_fSpecIntMult		= 0.125f;
	pNewShaderEffect->m_fSpecFresnel		= 0.97f;
	pNewShaderEffect->m_fSpec2Factor		= 40.0f;
	pNewShaderEffect->m_fSpec2ColorInt		= 1.0f;
	pNewShaderEffect->m_fSpec2Color.Set(255,255,255,255);

	return(pNewShaderEffect);
}

//
//
//
//
bool CCustomShaderEffectWeaponType::RestoreModelInfoDrawable(rmcDrawable *pDrawable)
{
	if(this->m_pTintType)
	{
		return m_pTintType->RestoreModelInfoDrawable(pDrawable);
	}
	else
	{
		return(true);
	}
}


//
//
//
//
CCustomShaderEffectWeapon::CCustomShaderEffectWeapon() : CCustomShaderEffectBase(sizeof(*this), CSE_WEAPON)
{
	// do nothing
}

//
//
//
//
CCustomShaderEffectWeapon::~CCustomShaderEffectWeapon()
{
	if(m_pTint)
	{
		delete m_pTint;
		m_pTint = NULL;
	}
}


void CCustomShaderEffectWeapon::AddToDrawList(u32 modelIndex, bool bExecute)
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

void CCustomShaderEffectWeapon::AddToDrawListAfterDraw()
{
	if(m_pTint)
	{
		m_pTint->AddToDrawListAfterDraw();
	}
}

u32 CCustomShaderEffectWeapon::AddDataForPrototype(void * address)
{
	u32 size = 0;
	DrawListAddress drawListOffset;
	CCustomShaderEffectTint *tmpTint = m_pTint; // save cascaded tint ptr
	m_pTint = NULL;		// remove cascaded link for DLC
	CopyOffShader(this, this->Size(), drawListOffset, false, m_pType);
	drawListOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_SHADER, GetSharedDataOffset());
	m_pTint = tmpTint;	// restore ptr
	*static_cast<DrawListAddress*>(address) = drawListOffset;
	size += sizeof(DrawListAddress);
	if(m_pTint)
	{
		size += m_pTint->AddDataForPrototype((char*)address + size);
	}
	else
	{
		DrawListAddress nullAddress;
		*reinterpret_cast<DrawListAddress*>((char*)address + size) = nullAddress;
		size += sizeof(DrawListAddress);
	}
	return size;
}

//
//
//
//
void CCustomShaderEffectWeapon::Update(fwEntity* pEntity)
{
	Assert(pEntity);

	if(m_pTint)
	{
		m_pTint->Update(pEntity);
	}

}// end of Update()...


//
//
//
//
void CCustomShaderEffectWeapon::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	CCustomShaderEffectWeaponType* pType = m_pType;
	grmShaderGroup *shaderGroup = &pDrawable->GetShaderGroup();

	if(pType->m_idVarSpecFresnel)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpecFresnel, m_fSpecFresnel);
	}

	if(pType->m_idVarSpecFalloffMult)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpecFalloffMult, m_fSpecFalloffMult);
	}

	if(pType->m_idVarSpecIntMult)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpecIntMult, m_fSpecIntMult);
	}

	if(pType->m_idVarSpec2Factor)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpec2Factor, m_fSpec2Factor);
	}

	if(pType->m_idVarSpec2ColorInt)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpec2ColorInt, m_fSpec2ColorInt);
	}

	if(pType->m_idVarSpec2Color)
	{
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarSpec2Color, m_fSpec2Color);
	}

	if(pType->m_idVarDiffuseTexPalSelector && m_pType->m_bHasPalette)
	{
		// convert palette selector to UV coordinate:
		// uv = (paletteSelector + 0.5f) / 8.0f;	// palette row select: 0-7
		const float divider = 1.0f / (CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM-1);
	#if RSG_ORBIS || RSG_DURANGO || RSG_PC
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarDiffuseTexPalSelector, float(m_varDiffuseTexPalSelector) * divider);
	#else
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarDiffuseTexPalSelector, (float(m_varDiffuseTexPalSelector)+0.5f) * divider);
	#endif
	}

	if(pType->m_bSwappableCamoTex)
	{
		if(pType->m_pTexDict && pType->m_pCamoDiffTexIdxTab && pType->m_pCamoShaderDiffTexRemap)
		{
			atArray<s8>& camoDiffTexIdxTab = *(pType->m_pCamoDiffTexIdxTab);

			const int entry = camoDiffTexIdxTab[m_varCamoDiffuseTexIdx];
			if(entry != -1)
			{
				grcTexture *pDiffuseTex = pType->m_pTexDict->GetEntry(entry);
				Assert(pDiffuseTex);
				
				atArray<CCustomShaderEffectWeaponType::shaderDiffuseTex>& camoShaderDiffTexRemap = *(pType->m_pCamoShaderDiffTexRemap);
					
				const u32 count = camoShaderDiffTexRemap.GetCount();
				for(u32 i=0; i<count; i++)
				{
					grmShader *pShader = shaderGroup->GetShaderPtr( camoShaderDiffTexRemap[i].m_ShaderIdx );
					Assert(pShader);
					pShader->SetVar((grcEffectVar)camoShaderDiffTexRemap[i].m_idVarDiffuseTex, pDiffuseTex);
				}
			}
		}
	}


#if CSE_WEAPON_EDITABLEVALUES
	CCustomShaderEffectWeapon::SetEditableShaderValues(shaderGroup, pDrawable, pType);
#endif

}// end of SetShaderVariables()...




#if CSE_WEAPON_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectWeapon::SetEditableShaderValues(grmShaderGroup* pShaderGroup, rmcDrawable* UNUSED_PARAM(pDrawable), CCustomShaderEffectWeaponType *pType)
{
	Assert(pShaderGroup);
	Assert(pType);

	if(!ms_bEVEnabled)
		return;

	SetFloatShaderGroupVar(pShaderGroup,	"Specular",					ms_fEVSpecFalloff);
	SetFloatShaderGroupVar(pShaderGroup,	"SpecularColor",			ms_fEVSpecIntensity);
	SetFloatShaderGroupVar(pShaderGroup,	"Fresnel",					ms_fEVSpecFresnel);
	SetFloatShaderGroupVar(pShaderGroup,	"Specular2Factor",			ms_fEVSpec2Factor);
	SetFloatShaderGroupVar(pShaderGroup,	"Specular2ColorIntensity",	ms_fEVSpec2ColorIntensity);
	SetVector3ShaderGroupVar(pShaderGroup,	"Specular2Color",			Vector3(ms_fEVSpec2Color.GetRedf(),ms_fEVSpec2Color.GetGreenf(),ms_fEVSpec2Color.GetBluef()) );
#if RSG_ORBIS || RSG_DURANGO || RSG_PC
	SetFloatShaderGroupVar(pShaderGroup,	"DiffuseTexPaletteSelector",float(ms_fEVDiffuseTexPalSelector)/float(CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM-1));
#else
	SetFloatShaderGroupVar(pShaderGroup,	"DiffuseTexPaletteSelector",(float(ms_fEVDiffuseTexPalSelector)+0.5f)/float(CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM-1));
#endif

	if(pType->m_bSwappableCamoTex)
	{
		if(pType->m_pTexDict && pType->m_pCamoDiffTexIdxTab && pType->m_pCamoShaderDiffTexRemap)
		{
			atArray<s8>& camoDiffTexIdxTab = *(pType->m_pCamoDiffTexIdxTab);

			const int entry = camoDiffTexIdxTab[ ms_fEVCamoDiffuseTexIdx ];
			if(entry != -1)
			{
				grcTexture *pDiffuseTex = pType->m_pTexDict->GetEntry(entry);
				Assert(pDiffuseTex);
				
				atArray<CCustomShaderEffectWeaponType::shaderDiffuseTex>& camoShaderDiffTexRemap = *(pType->m_pCamoShaderDiffTexRemap);
					
				const u32 count = camoShaderDiffTexRemap.GetCount();
				for(u32 i=0; i<count; i++)
				{
					grmShader *pShader = pShaderGroup->GetShaderPtr( camoShaderDiffTexRemap[i].m_ShaderIdx );
					Assert(pShader);
					pShader->SetVar((grcEffectVar)camoShaderDiffTexRemap[i].m_idVarDiffuseTex, pDiffuseTex);
				}
			}
		}
	}

}// end of SetEditableShaderValues()...


//
//
//
//
bool CCustomShaderEffectWeapon::InitWidgets(bkBank& bank)
{
	// debug widgets:
	bank.PushGroup("Weapon Editable Shaders Values", false);
		bank.AddToggle("Enable",				&ms_bEVEnabled);
		
		bank.AddSlider("Specular falloff",		&ms_fEVSpecFalloff,			0.0f, 500.0f, 0.1f		);
		bank.AddSlider("Specular intensity",	&ms_fEVSpecIntensity,		0.0f, 1.0f, 0.01f		);
		bank.AddSlider("Specular Fresnel",		&ms_fEVSpecFresnel,			0.0f, 1.0f, 0.01f		);

		bank.AddSlider("Specular2 Falloff",		&ms_fEVSpec2Factor,			0.0f, 512.0f, 0.1f	);
		bank.AddSlider("Specular2 Intensity",	&ms_fEVSpec2ColorIntensity,	0.0f,  10.0f, 0.1f	);
		bank.AddColor( "Specular2 Color",		&ms_fEVSpec2Color									);

		bank.AddSlider("Palette selector",		&ms_fEVDiffuseTexPalSelector, 0, CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM-1, 1);
		bank.AddSlider("Camo texture",			&ms_fEVCamoDiffuseTexIdx,	0, CSE_WEAPON_MAX_CAMO_TEXTURES-1, 1);
	bank.PopGroup();

	return(TRUE);
}// end of InitWidgets()...

#endif //CSE_WEAPON_EDITABLEVALUES...

void CCustomShaderEffectWeapon::CopySettings(CCustomShaderEffectWeapon* src)
{
	m_fSpecFresnel				= src->m_fSpecFresnel;
	m_fSpecFalloffMult			= src->m_fSpecFalloffMult;
	m_fSpecIntMult				= src->m_fSpecIntMult;

	m_fSpec2Factor				= src->m_fSpec2Factor;
	m_fSpec2ColorInt			= src->m_fSpec2ColorInt;
	m_fSpec2Color				= src->m_fSpec2Color;

	m_varDiffuseTexPalSelector	= src->m_varDiffuseTexPalSelector;
	m_varCamoDiffuseTexIdx		= src->m_varCamoDiffuseTexIdx;
}
