//
// CustomDrawableShaderVar - small useful class for setting group of variables
//	in drawable's shaders (with use of grmShaderVars); really useful when CLONED_EFFECTS=0;
//

// rage headers:
#include "grcore\texture.h"
#include "rmcore\drawable.h"

// game headers:
#include "shaders\CustomDrawableShaderVar.h"
#include "debug\Debug.h"		// Assert()...



//
//
//
//
CCustomDrawableShaderVar::CCustomDrawableShaderVar()
{
	// do nothing
}

//
//
//
//
CCustomDrawableShaderVar::~CCustomDrawableShaderVar()
{
	this->Shutdown();
}


//
//
//
//
bool CCustomDrawableShaderVar::LookupForShaderVar(rmcDrawable *pDrawable, const char *pVarName)
{
	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	const s32 shaderCount	= pShaderGroup->GetCount();
	Assert(shaderCount >= 1);


atArray<structCustomShaderVar>	 tempVarTab(0,shaderCount);	// temp table to gather all variable info's

	for(s32 i=0; i<shaderCount; i++)
	{
		grmShader *pShader = pShaderGroup->GetShaderPtr(i);
		Assert(pShader);

		grcEffectVar nShaderVar = pShader->LookupVar(pVarName, FALSE);
		if(nShaderVar)
		{
			structCustomShaderVar	csVar;
			csVar.pShader			= pShader;
			csVar.nShaderVar		= nShaderVar;
			tempVarTab.Append()		= csVar;
		}
	}// for(s32 i=0; i<shaderCount; i++)...


	// copy found variables to ms_tabDiffColEffect table (if necessary):
	m_tabVar.Reset();
	const s32 varShadCount = tempVarTab.GetCount();
	if(varShadCount > 0)
	{
		m_tabVar.Reserve(varShadCount);
		for(s32 i=0; i<varShadCount; i++)
		{
			m_tabVar.Append() = tempVarTab[i];
		}
		tempVarTab.Reset();	// destroy temp data
	}

	return( this->IsValid() );

}// end of LookupForShaderVar()...


//
//
//
//
#define SET_SHADER_VAR(TYPE, VAL)											\
{																			\
	const s32 count = m_tabVar.GetCount();								\
	if(count)																\
	for(s32 i=0; i<count; i++)											\
	{																		\
		structCustomShaderVar *ptr = &m_tabVar[i];							\
		ptr->pShader->SetVar(ptr->nShaderVar, (TYPE)VAL);				\
	}																		\
}



//
//
//
//
void CCustomDrawableShaderVar::SetShaderVarTex(grcTexture *pTexture)
{
	Assert(pTexture);
	SET_SHADER_VAR( grcTexture*, pTexture);
}

void CCustomDrawableShaderVar::SetShaderVarB(bool val)
{
	SET_SHADER_VAR(bool, val);
}

void CCustomDrawableShaderVar::SetShaderVarF(float val)
{
	SET_SHADER_VAR(float, val);
}


void CCustomDrawableShaderVar::SetShaderVarV2(const Vector2& val)
{
	SET_SHADER_VAR(Vector2&, val);
}

void CCustomDrawableShaderVar::SetShaderVarV3(const Vector3& val)
{
	SET_SHADER_VAR(Vector3&, val);
}

void CCustomDrawableShaderVar::SetShaderVarV4(const Vector4& val)
{
	SET_SHADER_VAR(Vector4&, val);
}

void CCustomDrawableShaderVar::SetShaderVarM34(const Matrix34& val)
{
	SET_SHADER_VAR(Matrix34&, val);
}

void CCustomDrawableShaderVar::SetShaderVarM44(const Matrix44& val)
{
	SET_SHADER_VAR(Matrix44&, val);
}


//
//
//
//
void CCustomDrawableShaderVar::Shutdown(bool UNUSED_PARAM(bKillData))
{
	// remove references to any used textures;
	// trigger internal KillData() (which dereferences textures), when setting None texture:
	const s32 count = m_tabVar.GetCount();
	if(count)
	{
		grcEffect &e = m_tabVar[0].pShader->GetInstanceData().GetBasis();

		u32 nameHash;
		grcEffect::VarType type;
		e.GetVarDesc(m_tabVar[0].nShaderVar,nameHash,type);

		if(type==grcEffect::VT_TEXTURE)
		for(s32 i=0; i<count; i++)
		{
			structCustomShaderVar *ptr = &m_tabVar[i];
			ptr->pShader->SetVar(ptr->nShaderVar, (grcTexture*)grcTexture::None);
		}
	}

#if 0		// Shouldn't be necessary any longer DCE
	// kill any referenced textures & matrices:
	if(bKillData)
	{
		const s32 count = m_tabVar.GetCount();
		for(s32 i=0; i<count; i++)
		{
			structCustomShaderVar *ptr = &m_tabVar[i];
			ptr->pShaderVar->KillData();
		}
	}
#endif

	this->m_tabVar.Reset();
}

