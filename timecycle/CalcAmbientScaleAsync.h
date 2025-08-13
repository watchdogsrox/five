#ifndef CALC_AMBIENT_SCALE_ASYNC_H_
#define CALC_AMBIENT_SCALE_ASYNC_H_

#include "system/dependency.h"

class CPed;

struct CalcAmbientScalePedData
{
	CPed*		m_pPed;
	float		m_incaredNess;
};

class CalcAmbientScaleAsync
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};

#endif
