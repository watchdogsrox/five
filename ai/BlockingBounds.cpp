// File header
#include "AI/BlockingBounds.h"

// Rage headers
#include "ai/aichannel.h"
#include "fwdebug/debugdraw.h"
#include "system/nelem.h"

#if __BANK
#include "peds/ped.h"
#include "scene/world/gameworld.h"
#endif

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CScenarioBlockingArea
////////////////////////////////////////////////////////////////////////////////

void CScenarioBlockingArea::InitFromMinMax(Vector3& vMin, Vector3& vMax )
{
	m_vBoundingMin.Min(vMin, vMax);
	m_vBoundingMax.Max(vMin, vMax);
	m_bValid = true;

	m_bBlocksPeds = m_bBlocksVehicles = true;
}

////////////////////////////////////////////////////////////////////////////////

bool CScenarioBlockingArea::ContainsPoint(const Vector3& vPoint) const
{
	if( ( vPoint.x < m_vBoundingMin.x ) ||
		( vPoint.y < m_vBoundingMin.y ) ||
		( vPoint.z < m_vBoundingMin.z ) ||
		( vPoint.x > m_vBoundingMax.x ) ||
		( vPoint.y > m_vBoundingMax.y ) ||
		( vPoint.z > m_vBoundingMax.z ) )
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioBlockingAreas
////////////////////////////////////////////////////////////////////////////////

#if __BANK

static const char* s_Names[CScenarioBlockingAreas::BlockingAreaUserType::kNumUserTypes] =
{
	"None",
	"Cutscene",
	"Script",
	"NetScript",
	"Debug"
};

bool CScenarioBlockingAreas:: ms_bRenderScenarioBlockingAreas = false;
CScenarioBlockingAreas::DebugInfo CScenarioBlockingAreas::ms_aBlockingAreaDebugInfo[CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS];
#endif // __BANK

CScenarioBlockingArea CScenarioBlockingAreas::ms_aBlockingAreas[CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS];

int CScenarioBlockingAreas::AddScenarioBlockingArea(Vector3& vMin, Vector3& vMax , bool bShouldCancelActiveScenarios,
		bool bBlocksPeds, bool bBlocksVehicles,
		BlockingAreaUserType BANK_ONLY(debugUserType), const char* BANK_ONLY(debugUserName))
{

#if __DEV
	if(NetworkInterface::IsGameInProgress())
	{
		// In network games we make sure the scenario blocking area that is being added doesn't already exist.
		if(IsScenarioBlockingAreaExists(vMin, vMax))
		{
			Assertf(false, "CScenarioBlockingAreas::AddScenarioBlockingArea : game trying to add the same scenario blocking area twice! Not Adding!");

			gnetDebug2( "Attempted creating scenario blocking area - vMin %.2f, %.2f, %.2f.  vMax %.2f, %.2f, %.2f",
				vMin.x,vMin.y,vMin.z,
				vMax.x,vMax.y,vMax.z);

			return kScenarioBlockingAreaIdInvalid;
		}
	}
#endif
	for ( int i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; ++i )
	{
		if ( !ms_aBlockingAreas[i].IsValid() )
		{
			ms_aBlockingAreas[i].InitFromMinMax(vMin, vMax );
			ms_aBlockingAreas[i].SetShouldCancelActiveScenarios(bShouldCancelActiveScenarios);
			ms_aBlockingAreas[i].SetBlocksPeds(bBlocksPeds);
			ms_aBlockingAreas[i].SetBlocksVehicles(bBlocksVehicles);

#if __BANK
			aiAssert(debugUserType != kUserTypeNone);
			if(debugUserName)
			{
				ms_aBlockingAreaDebugInfo[i].m_UserName = debugUserName;
			}
			ms_aBlockingAreaDebugInfo[i].m_UserType = debugUserType;
#endif	// __BANK
			aiDebugf1("CScenarioBlockingAreas::AddScenarioBlockingArea : Creating! ID %i vMin %.2f, %.2f, %.2f vMax %.2f, %.2f, %.2f", IndexToId(i), 
				vMin.x,vMin.y,vMin.z,
				vMax.x,vMax.y,vMax.z);
			return IndexToId(i);
		}
	}

#if __ASSERT
	const char* extraInfo = "";
#if __BANK
	// If it's the first time, dump debug info to TTY. If we did this every time,
	// the spew may get annoying for somebody that ignores the assert.
	static bool s_FirstTime = true;
	if(s_FirstTime)
	{
		DumpDebugInfo();
		extraInfo = " See TTY spew for more info.";
		s_FirstTime = false;
	}
#endif	// __BANK

	aiAssertf(false, "Ran out of scenario blocking areas [Max %d]!%s", CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS, extraInfo);
#endif	// __ASSERT

	return kScenarioBlockingAreaIdInvalid;
}

////////////////////////////////////////////////////////////////////////////////

void CScenarioBlockingAreas::RemoveScenarioBlockingArea( int id )
{
	const int index = IdToIndex(id);

	aiAssertf( NetworkInterface::IsGameInProgress() || ms_aBlockingAreas[index].IsValid(), "Scenario blocking area %d should have been valid but wasn't", id ); //in MP the blocking area might be removed through the network, or might tried again to be removed locally - so might be invalid entering this area.

#if __ASSERT
	if (ms_aBlockingAreas[index].IsValid())
	{
		Vector3 vMin;
		Vector3 vMax;
		ms_aBlockingAreas[index].GetMin(vMin);
		ms_aBlockingAreas[index].GetMax(vMax);
		aiDebugf1("CScenarioBlockingAreas::RemoveScenarioBlockingArea : Invalidating! ID %i vMin %.2f, %.2f, %.2f vMax %.2f, %.2f, %.2f", id, 
			vMin.x,vMin.y,vMin.z,
			vMax.x,vMax.y,vMax.z);
	}
#endif // __ASSERT

	ms_aBlockingAreas[index].Invalidate();
#if __BANK
	aiAssert(index >= 0 && index < NELEM(ms_aBlockingAreaDebugInfo));
	ms_aBlockingAreaDebugInfo[index].m_UserName = NULL;
	ms_aBlockingAreaDebugInfo[index].m_UserType = kUserTypeNone;
#endif	// __BANK
}

////////////////////////////////////////////////////////////////////////////////

bool CScenarioBlockingAreas::IsScenarioBlockingAreaValid( int id )
{
	const int index = IdToIndex(id);
    return ms_aBlockingAreas[index].IsValid();
}

////////////////////////////////////////////////////////////////////////////////

bool CScenarioBlockingAreas::IsPointInsideBlockingAreas(const Vector3& vPoint, bool forPed, bool forVehicle)
{
	for(s32 i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; ++i)
	{
		const CScenarioBlockingArea& area =  ms_aBlockingAreas[i];
		if( area.IsValid() && area.AffectsEntities(forPed, forVehicle) && area.ContainsPoint(vPoint) )
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CScenarioBlockingAreas::ShouldScenarioQuitFromBlockingAreas(const Vector3& vPoint, bool forPed, bool forVehicle)
{
	for(s32 i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; ++i)
	{
		const CScenarioBlockingArea& area =  ms_aBlockingAreas[i];
		if( area.IsValid() && area.ShouldCancelActiveScenarios() && area.AffectsEntities(forPed, forVehicle) && area.ContainsPoint(vPoint) )
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CScenarioBlockingAreas::IsScenarioBlockingAreaExists(Vector3& vMin, Vector3& vMax)
{
	// In network games we make sure the scenario blocking area that is being added doesn't already exist.
	for ( int i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; ++i )
	{
		if(ms_aBlockingAreas[i].IsValid())
		{
			float fEpsilon = 0.1f;// a relatively high epsilon value to take into account quantization error serializing world position at 19 bit

			Vector3 vExistingMin;
			Vector3 vExistingMax;

			ms_aBlockingAreas[i].GetMax(vExistingMax);
			ms_aBlockingAreas[i].GetMin(vExistingMin);

			if(vExistingMax.IsClose(vMax, fEpsilon) && vExistingMin.IsClose(vMin, fEpsilon))
			{						
#if __BANK
				const CScenarioBlockingArea& a = ms_aBlockingAreas[i];
				if(a.IsValid())
				{
					Displayf("CScenarioBlockingAreas::IsScenarioBlockingAreaExists returns TRUE. Debug info on duplicate area:");
					Displayf("#  Type         Name                                MinX    MinY    MinZ     MaxX    MaxY    MaxZ C P V");					

					const DebugInfo& d = ms_aBlockingAreaDebugInfo[i];

					aiAssert(d.m_UserType >= 0 && d.m_UserType < kNumUserTypes);
					const char* userTypeName = s_Names[d.m_UserType];

					Vec3V minV, maxV;
					a.GetMin(RC_VECTOR3(minV));
					a.GetMax(RC_VECTOR3(maxV));

					Displayf("%-2d %-12s %-32s %7.1f %7.1f %7.1f  %7.1f %7.1f %7.1f %d %d %d",
						IndexToId(i), userTypeName, d.m_UserName.c_str() ? d.m_UserName.c_str() : "",
						minV.GetXf(), minV.GetYf(), minV.GetZf(),
						maxV.GetXf(), maxV.GetYf(), maxV.GetZf(),
						(int)a.ShouldCancelActiveScenarios(), (int)a.GetBlocksPeds(), (int)a.GetBlocksVehicles());
				}
#endif // __BANK
				return true;
			}
		}
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioBlockingAreas::RenderScenarioBlockingAreas()
{
	if(!CScenarioBlockingAreas::ms_bRenderScenarioBlockingAreas)
	{
		return;
	}

#if DEBUG_DRAW
	for( s32 i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; i++ )
	{
		if ( ms_aBlockingAreas[i].IsValid() )
		{
			Vector3 vMin, vMax;
			Color32 color( 1.0f, 0.0f, 0.0f, 0.3f );
			ms_aBlockingAreas[i].GetMin(vMin);
			ms_aBlockingAreas[i].GetMax(vMax);

			// top
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMin.z ), Vec3V( vMax.x, vMax.y, vMin.z), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMax.y, vMin.z), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMax.y, vMin.z), color );
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMin.z ), Vec3V( vMax.x, vMax.y, vMin.z), color );
			// bottom
			grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z), Vec3V( vMin.x, vMax.y, vMax.z ), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMax.z ), Vec3V( vMin.x, vMin.y, vMax.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMax.z ), Vec3V( vMin.x, vMin.y, vMax.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z), Vec3V( vMax.x, vMin.y, vMax.z ), RCC_VEC3V(vMax), color );

			//left
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMax.z), color );
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMin.y, vMax.z ), Vec3V( vMax.x, vMin.y, vMax.z), color );
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMin.z ), Vec3V( vMax.x, vMin.y, vMax.z), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMax.z), color );
			//right
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z), Vec3V( vMax.x, vMax.y, vMin.z ), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMax.z ), Vec3V( vMin.x, vMax.y, vMin.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMax.y, vMin.z ), Vec3V( vMin.x, vMax.y, vMin.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z), Vec3V( vMin.x, vMax.y, vMax.z ), RCC_VEC3V(vMax), color );

			//front
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMin.z ), Vec3V( vMin.x, vMax.y, vMax.z), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z ), RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMax.z), color );
			grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMax.z), color );
			grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMin.y, vMax.z ), Vec3V( vMin.x, vMax.y, vMax.z), color );
			//back
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z), Vec3V( vMax.x, vMax.y, vMin.z ), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMax.z ), Vec3V( vMax.x, vMin.y, vMin.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMax.y, vMin.z ), Vec3V( vMax.x, vMin.y, vMin.z), RCC_VEC3V(vMax), color );
			grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z), Vec3V( vMax.x, vMin.y, vMax.z ), RCC_VEC3V(vMax), color );
		}
	}
#endif // DEBUG_DRAW
}

void CScenarioBlockingAreas::DumpDebugInfo()
{
	CompileTimeAssert(NELEM(s_Names) == kNumUserTypes);

	Displayf("#  Type         Name                                MinX    MinY    MinZ     MaxX    MaxY    MaxZ C P V");
	Displayf("-------------------------------------------------------------------------------------------------------");

	for(int i = 0; i < CScenarioBlockingAreas::MAX_SCENARIO_BLOCKING_AREAS; i++)
	{
		const CScenarioBlockingArea& a = ms_aBlockingAreas[i];
		if(!a.IsValid())
		{
			continue;
		}

		const DebugInfo& d = ms_aBlockingAreaDebugInfo[i];

		aiAssert(d.m_UserType >= 0 && d.m_UserType < kNumUserTypes);
		const char* userTypeName = s_Names[d.m_UserType];

		Vec3V minV, maxV;
		a.GetMin(RC_VECTOR3(minV));
		a.GetMax(RC_VECTOR3(maxV));

		Displayf("%-2d %-12s %-32s %7.1f %7.1f %7.1f  %7.1f %7.1f %7.1f %d %d %d",
				IndexToId(i), userTypeName, d.m_UserName.c_str() ? d.m_UserName.c_str() : "",
				minV.GetXf(), minV.GetYf(), minV.GetZf(),
				maxV.GetXf(), maxV.GetYf(), maxV.GetZf(),
				(int)a.ShouldCancelActiveScenarios(), (int)a.GetBlocksPeds(), (int)a.GetBlocksVehicles());
	}
}


void CScenarioBlockingAreas::AddAroundPlayer()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return;
	}

	static float s_Size = 400.0f;
	Vec3V centerPosV = pPlayer->GetMatrixRef().GetCol3();
	Vec3V minV = centerPosV - Vec3V(s_Size, s_Size, s_Size);
	Vec3V maxV = centerPosV + Vec3V(s_Size, s_Size, s_Size);
	AddScenarioBlockingArea(RC_VECTOR3(minV), RC_VECTOR3(maxV), true, true, true, kUserTypeDebug);
}


void CScenarioBlockingAreas::ClearAll()
{
	for(int i = 0; i < MAX_SCENARIO_BLOCKING_AREAS; i++)
	{
		const CScenarioBlockingArea& a = ms_aBlockingAreas[i];
		if(!a.IsValid())
		{
			continue;
		}

		RemoveScenarioBlockingArea(IndexToId(i));
	}
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
