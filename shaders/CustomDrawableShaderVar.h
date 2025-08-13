//
// CustomDrawableShaderVar - small useful class for setting group of variables
//	in drawable's shaders (with use of grmShaderVars); really useful when CLONED_EFFECTS=0;
//
//	05/04/2006	-	Andrzej:	- initial;
//
//
//
#ifndef __CUSTOMDRAWABLESHADERVAR_H__
#define	__CUSTOMDRAWABLESHADERVAR_H__

#include "CustomShaderEffectBase.h"

namespace rage {
class rmcDrawable;
}


//
//
//
//
class CCustomDrawableShaderVar
{
public:
	CCustomDrawableShaderVar();
	~CCustomDrawableShaderVar();

public:
	bool		LookupForShaderVar(rmcDrawable *pDrawable, const char *pVarName);
	inline bool	IsValid();
	void		Shutdown(bool bKillData=TRUE);


	// note: you have to directly cast to given type when using SetShaderVar(),
	// otherwise grcTextures can be cast to bool, 
	// causing very hard to detect texture referencing errors, etc.
	void		SetShaderVarTex(grcTexture		*pTexture);
	void		SetShaderVarB(bool				b);
	void		SetShaderVarI(int				val);
	void		SetShaderVarF(float				f);
	void		SetShaderVarV2(const Vector2&	v2);
	void		SetShaderVarV3(const Vector3&	v3);
	void		SetShaderVarV4(const Vector4&	v4);
	void		SetShaderVarM34(const Matrix34&	m34);
	void		SetShaderVarM44(const Matrix44&	m44);


private:
	struct structCustomShaderVar
	{
		grmShader		*pShader;
		grcEffectVar	nShaderVar;
	};

	atArray<structCustomShaderVar>	m_tabVar;
};



//
//
//
//
bool CCustomDrawableShaderVar::IsValid()
{
	return(this->m_tabVar.GetCount()>0);
}


#endif//__CUSTOMDRAWABLESHADERVAR_H__...
