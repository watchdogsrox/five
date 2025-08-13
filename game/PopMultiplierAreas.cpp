/****************************************
 * cargen.cpp                           *
 * ------------                         *
 *                                      *
 * car generator control for GTA3		*
 *                                      *
 * GSW 07/06/00                         *
 *                                      *
 * (c)2000 Rockstar North               *
 ****************************************/

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths\random.h"
#include "fwsys/timer.h"

// Game headers
#include "game/popmultiplierareas.h"
#include "camera/CamInterface.h"
#include "debug\vectormap.h"

// network includes 
#include "network/NetworkInterface.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetworkPlayerMgr.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

const float PopMultiplierArea::MINIMUM_AREA_WIDTH	= 1.0f;
const float PopMultiplierArea::MINIMUM_AREA_HEIGHT	= 1.0f;
const float PopMultiplierArea::MINIMUM_AREA_DEPTH	= 1.0f;
const float PopMultiplierArea::MINIMUM_AREA_RADIUS	= 1.0f;

PopMultiplierArea CThePopMultiplierAreas::PopMultiplierAreaArray[MAX_POP_MULTIPLIER_AREAS];
s32 CThePopMultiplierAreas::ms_iNumAreas = 0;
s32 CThePopMultiplierAreas::ms_iNumPedAreas = 0;
s32 CThePopMultiplierAreas::ms_iNumTrafficAreas = 0;

#if !__FINAL
bool	CThePopMultiplierAreas::gs_bDisplayPopMultiplierAreas=false;
#endif

//--------------------------------------------
//
//
//--------------------------------------------

const s32 CThePopMultiplierAreas::INVALID_AREA_INDEX = -1;

//--------------------------------------------
//
//--------------------------------------------

bool PopMultiplierArea::operator==(PopMultiplierArea const& rhs) const
{
	if((m_init) && (rhs.m_init))
	{
		float fEpsilon = FLT_EPSILON;

		if(NetworkInterface::IsNetworkOpen())
		{
			fEpsilon = 0.1f; // a relatively high epsilon value to take into account quantization error serializing world position at 19 bit
		}

		if(m_minWS.IsClose(rhs.m_minWS, fEpsilon))
		{
			if(m_maxWS.IsClose(rhs.m_maxWS, fEpsilon))
			{
				if(m_isSphere == rhs.m_isSphere)
				{
					if(fabs(m_pedDensityMultiplier - rhs.m_pedDensityMultiplier) < 0.05f)
					{
						if(fabs(m_trafficDensityMultiplier - rhs.m_trafficDensityMultiplier) < 0.05f)
						{
							if(m_bCameraGlobalMultiplier == rhs.m_bCameraGlobalMultiplier)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

//--------------------------------------------
// <CThePopMultiplierAreas::Init>
// RSGLDS ADW
//--------------------------------------------

void PopMultiplierArea::Init
(
	Vector3 const& _minWS, 
	Vector3 const& _maxWS, 
	float const _pedMultiplier, 
	float const _trafficMultiplier,
	const bool bCameraGlobalMultiplier
)
{
	Assertf(!m_init, "PopMultiplierArea::Init : Trying to initialise an already initialised area! - RSGLDS ADW");

	m_minWS						= _minWS;
	m_maxWS						= _maxWS;
	m_pedDensityMultiplier		= _pedMultiplier;
	m_trafficDensityMultiplier	= _trafficMultiplier;

	m_init						= true;
	m_isSphere					= false;
	m_bCameraGlobalMultiplier	= bCameraGlobalMultiplier;
}

void PopMultiplierArea::Init(const Vector3& center, float radius, float const pedMultiplier, float const trafficMultiplier, const bool bCameraGlobalMultiplier)
{
	Assertf(!m_init, "PopMultiplierArea::Init : Trying to initialise an already initialised area! - RSGLDS ADW");

	m_minWS						= center;
	m_maxWS.x					= radius;
	m_maxWS.y					= 0.0f;
	m_maxWS.z					= 0.0f;
	m_pedDensityMultiplier		= pedMultiplier;
	m_trafficDensityMultiplier	= trafficMultiplier;

	m_init						= true;
	m_isSphere					= true;
	m_bCameraGlobalMultiplier	= bCameraGlobalMultiplier;
}

//--------------------------------------------
// <CThePopMultiplierAreas::Reset>
// RSGLDS ADW
//--------------------------------------------

void PopMultiplierArea::Reset()
{
	m_init						= false;

	m_pedDensityMultiplier		= 1.0f;
	m_trafficDensityMultiplier	= 1.0f;

	m_minWS						= Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_maxWS						= Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

//--------------------------------------------
// <CThePopMultiplierAreas::GetCentreWS>
// RSGLDS ADW
//--------------------------------------------

Vector3 PopMultiplierArea::GetCentreWS(void) const
{
	if (m_isSphere)
		return m_minWS;
	else
		return m_minWS + ((m_maxWS - m_minWS) * 0.5f);
}

//--------------------------------------------
// <CThePopMultiplierAreas::DebugRender>
// RSGLDS ADW
//--------------------------------------------

#if !__FINAL

void PopMultiplierArea::DebugRender()
{
	
#if DEBUG_DRAW

	char buffer[256];

	// Here we make sure the pop area that is being added doesn't already exist.
	for (u32 i = 0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(m_init)
		{
			sprintf(buffer, "Ped Density %f : Traffic Density %f", m_pedDensityMultiplier, m_trafficDensityMultiplier);
			grcDebugDraw::Text(GetCentreWS(), Color_white, buffer);

			Color32 color = Color_blue;
			if (m_isSphere)
				grcDebugDraw::Sphere(RCC_VEC3V(m_minWS), GetRadius(), color, false);
			else
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(m_minWS), RCC_VEC3V(m_maxWS), color, false);
		}
	}

#endif // DEBUG_DRAW
}

#endif // !F_FINAL

//--------------------------------------------
// <CThePopMultiplierAreas::Update>
// RSGLDS ADW
//--------------------------------------------

void CThePopMultiplierAreas::Update(void)
{
#if !__FINAL
	if(gs_bDisplayPopMultiplierAreas)
	{
		PopMultiplierArea* pArea;
		for (u32 i = 0; i < MAX_POP_MULTIPLIER_AREAS; i++)
		{
			pArea = &PopMultiplierAreaArray[i];
			if(pArea->IsInit())
			{
				pArea->DebugRender();
			}
		}
	}
#endif // !__FINAL
}

//--------------------------------------------
// <CThePopMultiplierAreas::CreatePopMultiplierArea>
// RSGLDS ADW
//--------------------------------------------

s32 CThePopMultiplierAreas::CreatePopMultiplierArea
(
	Vector3 const & _minWS, 
	Vector3 const & _maxWS, 
	float const _pedDensityMultiplier, 
	float const _trafficDensityMultiplier,
	const bool bCameraGlobalMultiplier
)
{
	Assertf(NetworkInterface::IsNetworkOpen(), "CThePopMultiplierAreas is intended only for network game usage.");
	if(!NetworkInterface::IsNetworkOpen())
		return INVALID_AREA_INDEX;

	s32 i;

#if __DEV
	
	// Here we make sure the pop area that is being added doesn't already exist.
	for (i = 0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init && !PopMultiplierAreaArray[i].m_isSphere)
		{
			float fEpsilon = FLT_EPSILON;
			if(NetworkInterface::IsNetworkOpen())
			{
				fEpsilon = 0.1f; // a relatively high epsilon value to take into account quantization error serializing world position at 19 bit
			}
			if(_minWS.IsClose(PopMultiplierAreaArray[i].m_minWS, fEpsilon) && _maxWS.IsClose(PopMultiplierAreaArray[i].m_maxWS, fEpsilon))
			{
				Assertf(false, "CThePopMultiplierAreas::CreatePopMultiplierArea : trying to add the same area twice! Not Adding! - RSGLDS ADW");

				gnetDebug2( " Attempted creating pop area - m_minWS %.2f, %.2f, %.2f.  m_maxWS %.2f, %.2f, %.2f",
					_minWS.x,_minWS.y,_minWS.z,
					_maxWS.x,_maxWS.y,_maxWS.z);

				gnetDebug2( " Existing pop area Idx %d - m_minWS %.2f, %.2f, %.2f.  m_maxWS %.2f, %.2f, %.2f",
					i,
					PopMultiplierAreaArray[i].m_minWS.x,PopMultiplierAreaArray[i].m_minWS.y,PopMultiplierAreaArray[i].m_minWS.z,
					PopMultiplierAreaArray[i].m_maxWS.x,PopMultiplierAreaArray[i].m_maxWS.y,PopMultiplierAreaArray[i].m_maxWS.z);

				return INVALID_AREA_INDEX;
			}
		}
	}

#endif

	//--------------

	if((_minWS.x > _maxWS.x) || (_minWS.y > _maxWS.y) || (_minWS.z > _maxWS.z))
	{
		Assertf(false, "CThePopMultiplierAreas::CreatePopMultiplierArea : MIN (%.1f, %.1f, %.1f) MAX (%.1f, %.1f, %.1f) Min values are greater than Max values?! : Not Adding! - RSGLDS ADW", _minWS.x, _minWS.y, _minWS.z, _maxWS.x, _maxWS.y, _maxWS.z);
		return INVALID_AREA_INDEX;
	}

	Vector3 dimensions = _maxWS - _minWS;
	if((dimensions.x < PopMultiplierArea::MINIMUM_AREA_WIDTH) || (dimensions.y < PopMultiplierArea::MINIMUM_AREA_HEIGHT) || (dimensions.z < PopMultiplierArea::MINIMUM_AREA_DEPTH))
	{
		Assertf(false, "CThePopMultiplierAreas::CreatePopMultiplierArea : this area is very small along at least one axis?! Not Adding! - RSGLDS ADW");
		return INVALID_AREA_INDEX;
	}

	//--------------

	for(i=0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init == false)
		{
			PopMultiplierAreaArray[i].Init(_minWS, _maxWS, _pedDensityMultiplier, _trafficDensityMultiplier, bCameraGlobalMultiplier);
			ms_iNumAreas = Min(ms_iNumAreas+1, MAX_POP_MULTIPLIER_AREAS);
			if(_pedDensityMultiplier != 1.0f)
				ms_iNumPedAreas = Min(ms_iNumPedAreas+1, MAX_POP_MULTIPLIER_AREAS);
			if(_trafficDensityMultiplier != 1.0f)
				ms_iNumTrafficAreas = Min(ms_iNumTrafficAreas+1, MAX_POP_MULTIPLIER_AREAS);
			return i;
		}	
	}

	return INVALID_AREA_INDEX;
}

s32 CThePopMultiplierAreas::CreatePopMultiplierArea(Vector3 const& center, float radius, float pedDensityMultiplier, float trafficDensityMultiplier, const bool bCameraGlobalMultiplier)
{
	Assertf(NetworkInterface::IsNetworkOpen(), "CThePopMultiplierAreas is intended only for network game usage.");
	if(!NetworkInterface::IsNetworkOpen())
		return INVALID_AREA_INDEX;

#if __DEV
	// Here we make sure the pop area that is being added doesn't already exist.
	for (s32 i = 0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init && PopMultiplierAreaArray[i].m_isSphere)
		{
			float fEpsilon = FLT_EPSILON;
			if(NetworkInterface::IsNetworkOpen())
			{
				fEpsilon = 0.1f; // a relatively high epsilon value to take into account quantization error serializing world position at 19 bit
			}
			if(center.IsClose(PopMultiplierAreaArray[i].GetCentreWS(), fEpsilon) && IsClose(radius, PopMultiplierAreaArray[i].GetRadius(), fEpsilon))
			{
				Assertf(false, "CThePopMultiplierAreas::CreatePopMultiplierArea : trying to add the same sphere twice! Not Adding! - RSGLDS ADW");
				return INVALID_AREA_INDEX;
			}
		}
	}

#endif

	if(radius < PopMultiplierArea::MINIMUM_AREA_RADIUS)
	{
		Assertf(false, "CThePopMultiplierAreas::CreatePopMultiplierArea : this area's radius is too small! Not Adding! - RSGLDS ADW");
		return INVALID_AREA_INDEX;
	}

	for (s32 i=0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init == false)
		{
			PopMultiplierAreaArray[i].Init(center, radius, pedDensityMultiplier, trafficDensityMultiplier, bCameraGlobalMultiplier);
			ms_iNumAreas = Min(ms_iNumAreas+1, MAX_POP_MULTIPLIER_AREAS);
			if(pedDensityMultiplier != 1.0f)
				ms_iNumPedAreas = Min(ms_iNumPedAreas+1, MAX_POP_MULTIPLIER_AREAS);
			if(trafficDensityMultiplier != 1.0f)
				ms_iNumTrafficAreas = Min(ms_iNumTrafficAreas+1, MAX_POP_MULTIPLIER_AREAS);
			return i;
		}	
	}

	return INVALID_AREA_INDEX;
}

//--------------------------------------------
// <CThePopMultiplierAreas::RemovePopMultiplierArea>
// RSGLDS ADW
//--------------------------------------------

void CThePopMultiplierAreas::RemovePopMultiplierArea(const Vector3 & min, const Vector3 & max, bool const isSphere, float const pedDensityMultipier, float const trafficDensityMultiplier, const bool bCameraGlobalMultiplier)
{
	PopMultiplierArea temp;
	temp.m_minWS					= min;
	temp.m_maxWS					= max;
	temp.m_isSphere					= isSphere;
	temp.m_pedDensityMultiplier		= pedDensityMultipier;
	temp.m_trafficDensityMultiplier = trafficDensityMultiplier;
	temp.m_init						= true;
	temp.m_bCameraGlobalMultiplier	= bCameraGlobalMultiplier;

	for(int i = 0; i < MAX_POP_MULTIPLIER_AREAS; ++i)
	{
		if(temp == PopMultiplierAreaArray[i])
		{
			ms_iNumAreas = Max(0, ms_iNumAreas-1);
			if(temp.m_pedDensityMultiplier != 1.0f)
				ms_iNumPedAreas = Min(ms_iNumPedAreas-1, MAX_POP_MULTIPLIER_AREAS);
			if(temp.m_trafficDensityMultiplier != 1.0f)
				ms_iNumTrafficAreas = Min(ms_iNumTrafficAreas-1, MAX_POP_MULTIPLIER_AREAS);

			PopMultiplierAreaArray[i].Reset();

			//! Just quit out.
			break;
		}
	}
}

//--------------------------------------------
// <CThePopMultiplierAreas::InitLevelWithMapLoaded>
// RSGLDS ADW
//--------------------------------------------

void CThePopMultiplierAreas::InitLevelWithMapLoaded(void)
{
	for(s32 i=0; i<MAX_POP_MULTIPLIER_AREAS; i++)
	{
		PopMultiplierAreaArray[i].Reset();
	}
	ms_iNumAreas = 0;
	ms_iNumPedAreas = 0;
	ms_iNumTrafficAreas = 0;
}

//--------------------------------------------
// <CThePopMultiplierAreas::Init>
// RSGLDS ADW
//--------------------------------------------

void CThePopMultiplierAreas::Init(unsigned int UNUSED_PARAM(InitMode))
{
	Reset();
}

void CThePopMultiplierAreas::Reset()
{
	for(s32 i=0; i<MAX_POP_MULTIPLIER_AREAS; i++)
	{
		PopMultiplierAreaArray[i].Reset();
	}
	ms_iNumAreas = 0;
	ms_iNumPedAreas = 0;
	ms_iNumTrafficAreas = 0;
}

//--------------------------------------------------------
// <CThePopMultiplierAreas::GetLastActiveAreaIndex_DebugOnly>
// RSGLDS ADW
//--------------------------------------------------------

int CThePopMultiplierAreas::GetLastActiveAreaIndex_DebugOnly()
{
	for(s32 i=MAX_POP_MULTIPLIER_AREAS-1; i>=0; --i)
	{
		if(PopMultiplierAreaArray[i].m_init)
		{
			return i;
		}
	}	

	return INVALID_AREA_INDEX;
}

//--------------------------------------------
// <CThePopMultiplierAreas::GetActiveArea>
// RSGLDS ADW
//--------------------------------------------

const PopMultiplierArea * CThePopMultiplierAreas::GetActiveArea(u32 const _index)
{
	if (!Verifyf(_index < MAX_POP_MULTIPLIER_AREAS, "Invalid area index %d passed to GetActiveArea", _index))
		return NULL;

	if(PopMultiplierAreaArray[_index].m_init)
	{
		return &PopMultiplierAreaArray[_index];
	}

	return NULL;
}

//--------------------------------------------
// <CThePopMultiplierAreas::GetTrafficDensityMultiplier>
// RSGLDS ADW
//--------------------------------------------

float CThePopMultiplierAreas::GetTrafficDensityMultiplier(Vector3 const & vPos, const bool bCameraGlobalMultiplier)
{
	float trafficDensityMultiplier = 1.0f;
	int iNumVisited = 0;

	for(s32 i=0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init && PopMultiplierAreaArray[i].m_bCameraGlobalMultiplier == bCameraGlobalMultiplier)
		{
			if (PopMultiplierAreaArray[i].m_isSphere)
			{
				spdSphere sphere(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), ScalarV(PopMultiplierAreaArray[i].GetRadius()));
				if(sphere.ContainsPoint(RCC_VEC3V(vPos)))
				{
					trafficDensityMultiplier *= PopMultiplierAreaArray[i].m_trafficDensityMultiplier;
				}
			}
			else
			{
				spdAABB box(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), RCC_VEC3V(PopMultiplierAreaArray[i].m_maxWS));
				if(box.ContainsPoint(RCC_VEC3V(vPos)))
				{
					trafficDensityMultiplier *= PopMultiplierAreaArray[i].m_trafficDensityMultiplier;
				}
			}
		}	

		if( PopMultiplierAreaArray[i].m_trafficDensityMultiplier != 1.0f )
		{
			iNumVisited++;
			if(iNumVisited == ms_iNumTrafficAreas)
				break;
		}
	}	

	return trafficDensityMultiplier;
}

//--------------------------------------------
// <CThePopMultiplierAreas::GetPedDensityMultiplier>
// RSGLDS ADW
//--------------------------------------------

float CThePopMultiplierAreas::GetPedDensityMultiplier(Vector3 const & vPos, const bool bCameraGlobalMultiplier)
{
	float pedDensityMultiplier = 1.0f;
	int iNumVisited = 0;

	for(s32 i=0; i < MAX_POP_MULTIPLIER_AREAS; i++)
	{
		if(PopMultiplierAreaArray[i].m_init && PopMultiplierAreaArray[i].m_bCameraGlobalMultiplier == bCameraGlobalMultiplier)
		{
			if (PopMultiplierAreaArray[i].m_isSphere)
			{
				spdSphere sphere(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), ScalarV(PopMultiplierAreaArray[i].GetRadius()));
				if(sphere.ContainsPoint(RCC_VEC3V(vPos)))
				{
					pedDensityMultiplier *= PopMultiplierAreaArray[i].m_pedDensityMultiplier;
				}
			}
			else
			{
				spdAABB box(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), RCC_VEC3V(PopMultiplierAreaArray[i].m_maxWS));
				if(box.ContainsPoint(RCC_VEC3V(vPos)))
				{
					pedDensityMultiplier *= PopMultiplierAreaArray[i].m_pedDensityMultiplier;
				}
			}

			if( PopMultiplierAreaArray[i].m_pedDensityMultiplier != 1.0f )
			{
				iNumVisited++;
				if(iNumVisited == ms_iNumPedAreas)
					break;
			}
		}	
	}	

	return pedDensityMultiplier;
}
