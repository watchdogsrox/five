// ======================
// debug/BankUtil.cpp
// (c) 2011 RockstarNorth
// ======================

#if __BANK

#include "bank/bank.h"
#include "bank/slider.h"
#include "vectormath/classes.h"
#include "system/memory.h"

#include "debug/BankUtil.h"

#include <stdio.h>

class CMultiSliderProxy : private datBase
{
private:
	CMultiSliderProxy(CMultiSlider* _this, int index) : m_this(_this), m_index(index) {} // private c'tor

	void Update();

	void* m_this;
	int   m_index;

	friend class CMultiSlider;
};

CMultiSlider::CMultiSlider(float minValue, float maxValue, float step, float spacing)
{
	m_min         = minValue;
	m_max         = maxValue;
	m_step        = step;
	m_spacing     = spacing;
	m_negRelative = true;
	m_posRelative = true;
}

void CMultiSlider::AddSlider(bkBank& bk, const char* name, float* var)
{
	USE_DEBUG_MEMORY();

	CMultiSliderProxy* proxy = rage_new CMultiSliderProxy(this, m_vars.size());
	bk.AddSlider(name, var, m_min, m_max, m_step, datCallback(MFA(CMultiSliderProxy::Update), proxy));
	m_vars.PushAndGrow(var);
	m_last.PushAndGrow(*var); // track previous values
}

void CMultiSlider::AddSlider(bkBank& bk, const char* name, int i, float* var)
{
	char temp[256] = "";
	sprintf(temp, name, i);
	AddSlider(bk, temp, var);
}

class CMultiSliderPrivate : public CMultiSlider
{
public:
	CMultiSliderPrivate() { Assert(0); } // shouldn't ever be constructed like this

	void Update(int index);

private:
	void SpreadForwards (int index);
	void SpreadBackwards(int index);
	void SweepForwards  (int index);
	void SweepBackwards (int index);

	friend class CMultiSliderProxy;
};

void CMultiSliderProxy::Update()
{
	((CMultiSliderPrivate*)m_this)->Update(m_index);
}

void CMultiSliderPrivate::Update(int index)
{
	if (m_posRelative)
	{
		SpreadForwards(index);
	}

	if (m_negRelative)
	{
		SpreadBackwards(index);
	}

	if (*m_vars[index] > m_last[index]) // moved forwards
	{
		SweepForwards(index);
		SweepBackwards(m_vars.size() - 1);
	}
	else if (*m_vars[index] < m_last[index]) // moved backwards
	{
		SweepBackwards(index);
		SweepForwards(0);
	}

	for (int i = 0; i < m_vars.size(); i++) // update all
	{
		m_last[i] = *m_vars[i];
	}
}

void CMultiSliderPrivate::SpreadForwards(int index)
{
	const float last0 = m_last[index];
	const float last1 = m_last.back();
	const float curr0 = *m_vars[index];
	const float curr1 = *m_vars.back();

	for (int i = index + 1; i < m_vars.size() - 1; i++)
	{
		*m_vars[i] = m_last[i] = curr0 + (curr1 - curr0)*(m_last[i] - last0)/(last1 - last0);
	}
}

void CMultiSliderPrivate::SpreadBackwards(int index)
{
	const float last0 = m_last[0];
	const float last1 = m_last[index];
	const float curr0 = *m_vars[0];
	const float curr1 = *m_vars[index];

	for (int i = 1; i < index; i++)
	{
		*m_vars[i] = m_last[i] = curr0 + (curr1 - curr0)*(m_last[i] - last0)/(last1 - last0);
	}
}

void CMultiSliderPrivate::SweepForwards(int index)
{
	for (int j = index + 1; j < m_vars.size(); j++) // sweep forwards
	{
		float  prevValue = *m_vars[j - 1];
		float& currValue = *m_vars[j - 0];

		currValue = Max<float>(prevValue + m_spacing, currValue); // enforce spacing (forwards)

		if (j == m_vars.size() - 1)
		{
			currValue = Min<float>(m_max, currValue); // enforce max
		}
	}
}

void CMultiSliderPrivate::SweepBackwards(int index)
{
	for (int j = index - 1; j >= 0; j--) // sweep backwards
	{
		float  prevValue = *m_vars[j + 1];
		float& currValue = *m_vars[j + 0];

		currValue = Min<float>(prevValue - m_spacing, currValue); // enforce spacing (backwards)

		if (j == 0)
		{
			currValue = Max<float>(m_min, currValue); // enforce min
		}
	}
}

CScalarVAngle::CScalarVAngle(bkBank& bk, const char* name, ScalarV* var, bkAngleType::Enum angleType, bool bAbsTangent, float minValue, float maxValue) : m_var(*var), m_value(var->Getf()), m_bAbsTangent(bAbsTangent)
{
	m_widget = bk.AddAngle(name, &m_value, angleType, minValue, maxValue, datCallback(MFA(CScalarVAngle::Update), this));
}

CScalarVAngle::CScalarVAngle(bkBank& bk, const char* name, ScalarV* var, bkAngleType::Enum angleType, bool bAbsTangent) : m_var(*var), m_value(var->Getf()), m_bAbsTangent(bAbsTangent)
{
	m_widget = bk.AddAngle(name, &m_value, angleType, datCallback(MFA(CScalarVAngle::Update), this));
}

CScalarVAngle::~CScalarVAngle()
{
	m_widget->Destroy();
}

void CScalarVAngle::Update()
{
	const float a = m_bAbsTangent ? Abs<float>(tanf(m_widget->GetRadians())) : m_value;

	m_var = LoadScalar32IntoScalarV(a);
}

#endif // __BANK
