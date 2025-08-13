//
// vfx/sky/ShaderVariable.h
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SHADER_VARIABLE
#define INC_SHADER_VARIABLE

// INCLUDES
// rage
#include "atl/string.h"
#include "grcore/texture.h"

// Forward declaration
namespace rage
{
	class grmShader;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//													CShaderVariable													 //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class valueType>
class CShaderVariable
{
public:
	
	// CONSTRUCTORS / DESTRUCTORS
	CShaderVariable();
	CShaderVariable(const grmShader* shader, const char* nameInShader, const valueType &value);

	// FUNCTIONS
	void sendToShader(grmShader *shader);
	void sendToShaderLinear(grmShader *shader);
	void sendToShaderMult(grmShader *shader, const float mult);
	void sendToShaderMultV(grmShader *shader, Vec4V_In mult);
	void sendToShaderLinearMult(grmShader *shader, const float mult);

	void setValue(const valueType &value);
	void setNameAndValue(const grmShader* shader, const char* nameInShader, const valueType &value);
	void setNameAndValue(const grmShader* /*shader*/, const char* /*nameInShader*/, grcTexture * /*value*/)	{ Assert(false); }	// Not defined for anything other than valueType=grcTexture*
	void setName(const grmShader* shader, const char* nameInShader);

	valueType getValue() { return m_value; }

	// VARIABLES
	grcEffectVar m_id;
	valueType m_value;
};

template<>
inline void CShaderVariable<grcTextureHandle>::setNameAndValue(const grmShader* shader, const char* nameInShader, grcTexture *texture)
{
	grcTextureHandle handle;
	handle = texture;
	setNameAndValue(shader, nameInShader, handle);
}

#endif
