#include "conductorintensitymap.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"

AUDIO_OPTIMISATIONS()

#if __BANK
//-------------------------------------------------------------------------------------------------------------------
void audConductorMap::DrawMap()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	for(f32 i = 0; i <= 360; i += DeltaAngle )
	{
		grcDebugDraw::Line(playerPosition,playerPosition + MaxGridDistance * Vector3(rage::Sinf(DtoR * i),rage::Cosf(DtoR * i),0.f), Color_blue,1);
	}
	u32 area = 0;
	for(f32 i = WidthOfZones; i <= MaxGridDistance; i += WidthOfZones )
	{
		area++;
		if(area <= NumZonesInClosestArea)
			grcDebugDraw::Circle(playerPosition,i,Color_blue,Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f));
		else if (area <= NumZonesInClosestArea + NumZonesInMediumArea)
			grcDebugDraw::Circle(playerPosition,i,Color_red,Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f));
		else
			grcDebugDraw::Circle(playerPosition,i,Color_green,Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f));
	}
}
#endif
