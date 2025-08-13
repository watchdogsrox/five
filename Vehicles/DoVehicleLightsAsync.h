#ifndef DO_VEHICLE_LIGHTS_ASYNC_H_
#define DO_VEHICLE_LIGHTS_ASYNC_H_

#include "atl/array.h"
#include "system/dependency.h"
#include "vector/color32.h"

#include "fwscene/world/InteriorLocation.h"

class CVehicle;
class CLightSource;
struct Corona_t;

#define MAX_NUM_VEHICLE_LIGHT_CORONAS		928
#define MAX_NUM_VEHICLE_LIGHTS				400

#define MAX_NUM_CORONAS_PER_JOB				200
#define MAX_NUM_LIGHTS_PER_JOB				80

#define DO_VEHICLE_LIGHTS_ASYNC_SCRATCH_SIZE	(MAX_NUM_CORONAS_PER_JOB*sizeof(Corona_t) + MAX_NUM_LIGHTS_PER_JOB*(sizeof(CLightSource)))

struct DoVehicleLightsAsyncData
{
	CVehicle**						m_paVehicles;
	u32*							m_paLightFlags;
	Vec4V*							m_paAmbientVolumeParams;
	fwInteriorLocation*				m_paInteriorLocs;
	Corona_t* volatile*				m_pCoronasCurrPtr;
	CLightSource* volatile*			m_pLightSourcesCurrPtr;
	Corona_t*						m_CoronasStartingPtr;
	CLightSource*					m_LightSourcesStartingPtr;
	u32								m_MaxCoronas;
	u32								m_MaxLightSources;
	float							m_timeCycleSpriteSize;
	float							m_timeCycleSpriteBrightness;
	float							m_timecycleVehicleIntensityScale;
};

class CoronaAndLightList
{
public:
	void Init(Corona_t* pCoronas,
		CLightSource* pLightSources,
		u32 maxCoronas,
		u32 maxLights,
		float tcSpriteSize,
		float tcSpriteBrightness,
		float tcVehicleLightIntensity);

	void RegisterCorona(Vec3V_In vPos,
		const float size,
		const Color32 col,
		const float intensity,
		const float zBias,
		Vec3V_In   vDir,
		const float dirViewThreshold,
		const float dirLightConeAngleInner,
		const float dirLightConeAngleOuter,
		const u16 coronaFlags);

	void AddLight(CLightSource& light);

	u32 GetCoronaCount()		{ return m_coronaCount; }
	u32 GetLightCount()			{ return m_lightCount; }

	float GetTCSpriteSize()				{ return m_timeCycleSpriteSize; }
	float GetTCSpriteBrightness()		{ return m_timeCycleSpriteBrightness; }
	float GetTCVehicleIntensityScale()	{ return m_timecycleVehicleIntensityScale; }

private:
	Corona_t*		m_pCoronas;
	CLightSource*	m_pLightSources;
	u32				m_coronaCount;
	u32				m_lightCount;
	u32				m_maxCoronas;
	u32				m_maxLights;
	float			m_timeCycleSpriteSize;
	float			m_timeCycleSpriteBrightness;
	float			m_timecycleVehicleIntensityScale;
};

class DoVehicleLightsAsync
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};


#endif

