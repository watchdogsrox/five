#ifndef _INC_PROCESSLIGHTOCCLUSIONASYNC_H_
#define _INC_PROCESSLIGHTOCCLUSIONASYNC_H_

#include "vectormath/vec3v.h"
#include "system/dependency.h"

class CLightSource;

struct ProcessLightOcclusionData
{
	Vec3V			m_vCameraPos;
	CLightSource*	m_LightSourceArrayEA;
	u32*			m_DependencyRunningEA;

#if __DEV
	u32*		m_pNumOcclusionTests;
	u32*		m_pNumObjectsOccluded;
	u32*		m_pTrivialAcceptActiveFrustum;
	u32*		m_pTrivialAcceptMinZ;
	u32*		m_pTrivialAcceptVisiblePixel;
#endif
};


class ProcessLightOcclusionAsync
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};


#endif // _INC_PROCESSLIGHTOCCLUSIONASYNC_H_
