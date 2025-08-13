//
// vfx/sky/ShaderVariable.cpp
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// INCLUDES
#include "Vfx/Sky/ShaderVariable.h"

// rage
#include "Shaders/ShaderLib.h"

// ------------------------------------------------------------------------------------------------------------------//
//												CShaderVariable														 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
CShaderVariable<valueType>::CShaderVariable() 
: m_id(grcevNONE)
, m_value()
{}

// ------------------------------------------------------------------------------------------------------------------//
//												CShaderVariable														 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
CShaderVariable<valueType>::CShaderVariable(const grmShader* shader, const char* nameInShader, const valueType &value)
: m_id(grcevNONE)
, m_value(value)
{
	m_id = shader->LookupVar(nameInShader, false);
}

// ------------------------------------------------------------------------------------------------------------------//
//													sendToShader													 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::sendToShader(grmShader *shader)
{
	// send to shader
	shader->SetVar(m_id, m_value);
}

// ------------------------------------------------------------------------------------------------------------------//
//												sendToShaderLinear													 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::sendToShaderLinear(grmShader*)
{
	Assertf(false, "Trying to use sendToShaderLinear with an unsupported type");
}

template<>
void CShaderVariable<Vec3V>::sendToShaderLinear(grmShader *shader)
{
	// send to shader
	shader->SetVar(m_id, m_value * m_value);
}

// ------------------------------------------------------------------------------------------------------------------//
//													sendToShaderMult												 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::sendToShaderMult(grmShader *shader, const float mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * mult);
}

template<>
void CShaderVariable<Vec3V>::sendToShaderMult(grmShader *shader, const float mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * ScalarV(mult));
}

template<>
void CShaderVariable<Vec2V>::sendToShaderMult(grmShader *shader, const float mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * ScalarV(mult));
}

template<>
void CShaderVariable<Vec4V>::sendToShaderMult(grmShader *shader, const float mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * ScalarV(mult));
}

template<>
void CShaderVariable<grcTextureHandle>::sendToShaderMult(grmShader *, const float)
{
	Assertf(false, "Trying to use sendToShaderMult with a texture");
}

template<>
void CShaderVariable<grcRenderTarget*>::sendToShaderMult(grmShader *, const float)
{
	Assertf(false, "Trying to use sendToShaderMult with a texture");
}

// ------------------------------------------------------------------------------------------------------------------//
//													sendToShaderMultV												 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::sendToShaderMultV(grmShader *shader, Vec4V_In mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * mult.GetXf());
}

template<>
void CShaderVariable<Vec3V>::sendToShaderMultV(grmShader *shader, Vec4V_In mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * mult.GetXYZ());
}

template<>
void CShaderVariable<Vec2V>::sendToShaderMultV(grmShader *shader, Vec4V_In mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * mult.GetXY());
}

template<>
void CShaderVariable<Vec4V>::sendToShaderMultV(grmShader *shader, Vec4V_In mult)
{
	// send to shader
	shader->SetVar(m_id, m_value * mult);
}

template<>
void CShaderVariable<grcTextureHandle>::sendToShaderMultV(grmShader *, Vec4V_In)
{
	Assertf(false, "Trying to use sendToShaderMult with a texture");
}

template<>
void CShaderVariable<grcRenderTarget*>::sendToShaderMultV(grmShader *, Vec4V_In)
{
	Assertf(false, "Trying to use sendToShaderMult with a texture");
}


// ------------------------------------------------------------------------------------------------------------------//
//												sendToShaderLinear													 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::sendToShaderLinearMult(grmShader*, const float)
{
	Assertf(false, "Trying to use sendToShaderLinear with an unsupported type");
}

template<>
void CShaderVariable<Vec3V>::sendToShaderLinearMult(grmShader *shader, const float mult)
{
	// send to shader
	shader->SetVar(m_id, (m_value * m_value) * ScalarV(mult));
}


// ------------------------------------------------------------------------------------------------------------------//
//													setValue														 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::setValue(const valueType &value)
{
	m_value = value;
}

// ------------------------------------------------------------------------------------------------------------------//
//													setName														 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::setName(const grmShader* shader, const char* nameInShader)
{
	m_id = shader->LookupVar(nameInShader, false);
}

// ------------------------------------------------------------------------------------------------------------------//
//												setNameAndValue														 //
// ------------------------------------------------------------------------------------------------------------------//
template<class valueType>
void CShaderVariable<valueType>::setNameAndValue(const grmShader* shader, const char* nameInShader, const valueType &value)
{
	m_id = shader->LookupVar(nameInShader, false);
	m_value = value;
}

// Required due to MSVC++ linking issues
template class CShaderVariable<float>;
template class CShaderVariable<bool>;
template class CShaderVariable<Vec2V>;
template class CShaderVariable<Vec3V>;
template class CShaderVariable<Vec4V>;
template class CShaderVariable<grcRenderTarget*>;
template class CShaderVariable<grcTextureHandle>;
