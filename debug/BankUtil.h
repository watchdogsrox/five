// ======================
// debug/BankUtil.h
// (c) 2011 RockstarNorth
// ======================

#ifndef _DEBUG_BANKUTIL_H_
#define _DEBUG_BANKUTIL_H_

#if __BANK

#include "atl/array.h"

namespace rage { class bkBank; }

class CMultiSlider
{
public:
	CMultiSlider() { Assert(0); } // shouldn't ever be constructed like this
	CMultiSlider(float minValue, float maxValue, float step, float spacing = 0.0f);

	void AddSlider(bkBank& bk, const char* name,        float* var);
	void AddSlider(bkBank& bk, const char* name, int i, float* var); // name will be sprintf(..,name,i)

protected:
	atArray<float*> m_vars;
	atArray<float>  m_last;
	float m_min;
	float m_max;
	float m_step;

public:
	float m_spacing;
	bool  m_negRelative;
	bool  m_posRelative;
};

class CScalarVAngle : private datBase
{
public:
	CScalarVAngle(bkBank& bk, const char* name, ScalarV* var, bkAngleType::Enum angleType, bool bAbsTangent, float minValue, float maxValue);
	CScalarVAngle(bkBank& bk, const char* name, ScalarV* var, bkAngleType::Enum angleType, bool bAbsTangent);
	~CScalarVAngle();

private:
	void Update();

	bkAngle* m_widget;
	ScalarV& m_var;
	float    m_value;
	bool     m_bAbsTangent;
};

// make this nasty hack look less awful ..
#define bk_AddAngle(...)  rage_new CScalarVAngle (bk,##__VA_ARGS__)

#endif // __BANK
#endif // _DEBUG_BANKUTIL_H_
