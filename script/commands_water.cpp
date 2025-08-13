
// Rage headers
#include "script\wrapper.h"
#include "script\commands_shapetest.h"
#include "script\commands_water.h"
#include "physics\physics.h"
#include "physics\gtaArchetype.h"
// Game headers
#include "script\script_channel.h"
#include "script\script.h"
#include "renderer\water.h"

namespace water_commands
{

bool CommandGetWaterHeight(const scrVector & scrVecPos, float &fHeight)
{
	Vector3	normal;
	Vector3 vPos = Vector3(scrVecPos);

	if (Water::GetWaterLevel(vPos, &fHeight, true, POOL_DEPTH, 999999.9f, NULL))
	{
		phSegment		seg;
		phIntersection	intersection;
		seg.Set(vPos, Vector3(vPos.x, vPos.y, -999999.9f));
		intersection.Reset();

		if (CPhysics::GetLevel()->TestProbe(seg, &intersection, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
		{
			if (fHeight > intersection.GetPosition().GetZf())
			{
				return true;		// Water is higher than land. Wet.
			}
			else
			{
				return false;		// Land is higher than water. Dry.
			}
		}
		else
		{
			return true;
		}
	}

	return false;

}

bool CommandGetWaterHeightNoWaves(const scrVector & scrVecPos, float &fHeight)
{
	Vector3 vPos = Vector3(scrVecPos);
	if (Water::GetWaterLevelNoWaves(vPos, &fHeight, POOL_DEPTH, 999999.9f, NULL))
	{
		phSegment		seg;
		phIntersection	intersection;
		seg.Set(vPos, Vector3(vPos.x, vPos.y, -999999.9f));
		intersection.Reset();

		if (CPhysics::GetLevel()->TestProbe(seg, &intersection, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
		{
			if (fHeight > intersection.GetPosition().GetZf())
			{
				return true;		// Water is higher than land. Wet.
			}
			else
			{
				return false;		// Land is higher than water. Dry.
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool CommandTestProbeAgainstWater(const scrVector & scrVecStartPos, const scrVector & scrVecEndPos, Vector3& intersectionPosition)
{
	Vec3V startPos = VECTOR3_TO_VEC3V(Vector3(scrVecStartPos));
	Vec3V endPos = VECTOR3_TO_VEC3V(Vector3(scrVecEndPos));
	Vec3V positionOnWater;
	if(Water::TestLineAgainstWater(RCC_VECTOR3(startPos),RCC_VECTOR3(endPos),&RC_VECTOR3(positionOnWater)))
	{
		phSegment seg(RCC_VECTOR3(startPos), RCC_VECTOR3(endPos));
		phIntersection intersection;
		if (CPhysics::GetLevel()->TestProbe(seg, &intersection, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
		{
			// if the land position is further along the probe than the water position, we hit the water first
			if(IsGreaterThanAll(Dot(Subtract(intersection.GetPosition(),positionOnWater),Subtract(endPos,startPos)),ScalarV(V_ZERO)))
			{
				RC_VEC3V(intersectionPosition) = positionOnWater;
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			RC_VEC3V(intersectionPosition) = positionOnWater;
			return true;
		}
	}

	return false;
}


s32 WaterProbeTestHelper(bool hitOcean, Vec3V_In positionOnOcean, Vec3V_In startPos, Vec3V_In endPos, s32 blockingFlags, Vec3V_InOut intersectionPosition)
{
	// Test against all blocking objects and rivers in the level
	phIntersection levelIntersection;
	if(CPhysics::GetLevel()->TestProbe(phSegment(RCC_VECTOR3(startPos),RCC_VECTOR3(endPos)), &levelIntersection, NULL, shapetest_commands::GetPhysicsFlags(blockingFlags) | ArchetypeFlags::GTA_RIVER_TYPE))
	{
		// If the probe hit the ocean and a bound in the level, use whichever was first
		Vec3V positionInLevel = levelIntersection.GetPosition();
		if(hitOcean && IsGreaterThanAll(Dot(Subtract(positionInLevel,positionOnOcean),Subtract(endPos,startPos)),ScalarV(V_ZERO)))
		{
			// The probe hit the ocean first
			intersectionPosition = positionOnOcean;
			return SCRIPT_WATER_TEST_RESULT_WATER;
		}
		else
		{
			// We hit either a river or a blocking object first, find out which
			if(Verifyf(levelIntersection.GetInstance(),"No instance on valid intersection returned by shapetest."))
			{
				intersectionPosition = positionInLevel;
				bool hitRiver = (levelIntersection.GetInstance()->GetArchetype()->GetTypeFlag(ArchetypeFlags::GTA_RIVER_TYPE) != 0);
				return hitRiver ? SCRIPT_WATER_TEST_RESULT_WATER : SCRIPT_WATER_TEST_RESULT_BLOCKED;
			}
		}
	}
	else if(hitOcean)
	{
		// The probe only hit the ocean
		intersectionPosition = positionOnOcean;
		return SCRIPT_WATER_TEST_RESULT_WATER;
	}

	return SCRIPT_WATER_TEST_RESULT_NONE;
}

s32 CommandTestProbeAgainstAllWater(const scrVector & scrVecStartPos, const scrVector & scrVecEndPos, s32 blockingFlags, Vector3& intersectionPosition)
{
	Vec3V startPos = VECTOR3_TO_VEC3V(Vector3(scrVecStartPos));
	Vec3V endPos = VECTOR3_TO_VEC3V(Vector3(scrVecEndPos));

	// Test against the ocean
	Vec3V positionOnOcean;
	bool hitOcean = Water::TestLineAgainstWater(RCC_VECTOR3(startPos),RCC_VECTOR3(endPos),&RC_VECTOR3(positionOnOcean));
	return WaterProbeTestHelper(hitOcean,positionOnOcean,startPos,endPos,blockingFlags,RC_VEC3V(intersectionPosition));
}

s32 CommandTestVerticalProbeAgainstAllWater(const scrVector & scrVecStartPos, s32 blockingFlags, float& intersectionHeight)
{
	// Create a downward vertical segment from the given position. 
	Vec3V startPos = VECTOR3_TO_VEC3V(Vector3(scrVecStartPos));
	Vec3V endPos = startPos;
	endPos.SetZf(-1000.0f);

	// Test against the ocean
	Vec3V positionOnOcean = startPos;
	bool hitOcean = Water::GetWaterLevel(RCC_VECTOR3(startPos),&positionOnOcean[2],true, POOL_DEPTH, 999999.9f, NULL);

	Vec3V finalIntersectionPosition;
	s32 result = WaterProbeTestHelper(hitOcean,positionOnOcean,startPos,endPos,blockingFlags,finalIntersectionPosition);
	if(result != SCRIPT_WATER_TEST_RESULT_NONE)
	{
		intersectionHeight = finalIntersectionPosition.GetZf();
	}
	return result;
}

void CommandModifyWater(float worldX, float worldY, float newSpeed, float changePercentage)
{
	Water::ModifyDynamicWaterSpeed(worldX, worldY, newSpeed, changePercentage);
}

int CommandAddExtraCalmingQuad(float minX, float minY, float maxX, float maxY, float dampening)
{
	scriptAssertf(dampening >=0.0f && dampening <= 1.0f, "%s: ADD_EXTRA_CALMING_QUAD - Dampening should be between 0.0 and 1.0, it is %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), dampening);
	scriptAssertf(maxX - minX > 1.0f && maxY - minY > 1.0f, "%s: ADD_EXTRA_CALMING_QUAD - Invalid quad dimensions, X: %.2f - %.2f\t Y: %.2f - %.2f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxX, minX, maxY, minY);

	int idx = Water::AddExtraCalmingQuad((int)minX,(int)minY,(int)maxX,(int)maxY,dampening);
	scriptAssertf(idx != -1, "%s: ADD_EXTRA_CALMING_QUAD - Failed to add a new calming quad, are we out?", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	return idx;
}

void CommandSetCalmedWaveHeightScaler( float scale )
{
	scriptAssertf(scale >=0.0f && scale <= 1.0f, "%s: SET_CALMED_WAVE_HEIGHT_SCALER - Calmed wave height scaler should be between 0.0 and 1.0, it is %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scale);
	Water::SetScriptCalmedWaveHeightScaler(scale);
}

void CommandRemoveExtraCalmingQuad(int idx)
{
	Water::RemoveExtraCalmingQuad(idx);
}

void CommandRemoveAllExtraCalmingQuads()
{
	Water::RemoveAllExtraCalmingQuads();
}

void CommandSetDeepOceanScaler(float s)
{
	scriptAssertf(s >=0.0f && s <= 1.0f, "%s: SET_DEEP_OCEAN_SCALER - Deep Ocean scaler should be between 0.0 and 1.0, it is %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s);
	Water::SetScriptDeepOceanScaler(s);
}

float CommandGetDeepOceanScaler()
{
	return Water::GetScriptDeepOceanScaler();
}

void CommandResetDeepOceanScaler()
{
	Water::SetScriptDeepOceanScaler(1.0f);
}


bool CommandSynchRecordingWithWater()
{
	return Water::SynchRecordingWithWater();
}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(GET_WATER_HEIGHT,0x182029c7e52a1e4f, CommandGetWaterHeight);
		SCR_REGISTER_SECURE(GET_WATER_HEIGHT_NO_WAVES,0x7f38b18228435c61, CommandGetWaterHeightNoWaves);
		SCR_REGISTER_SECURE(TEST_PROBE_AGAINST_WATER,0x7cbf98360c4b22e4, CommandTestProbeAgainstWater);
		SCR_REGISTER_SECURE(TEST_PROBE_AGAINST_ALL_WATER,0xd3c9aed49c894ccf, CommandTestProbeAgainstAllWater);
		SCR_REGISTER_SECURE(TEST_VERTICAL_PROBE_AGAINST_ALL_WATER,0xe77dda8d6639cb5e, CommandTestVerticalProbeAgainstAllWater);
		SCR_REGISTER_UNUSED(SYNCH_RECORDING_WITH_WATER,0x3ede3f11c4cd62a9, CommandSynchRecordingWithWater);
		SCR_REGISTER_SECURE(MODIFY_WATER,0x93c91349de947dae, CommandModifyWater);
		SCR_REGISTER_SECURE(ADD_EXTRA_CALMING_QUAD,0xcf4901abd5b28f62, CommandAddExtraCalmingQuad);
		SCR_REGISTER_SECURE(REMOVE_EXTRA_CALMING_QUAD,0xf672e351681b36f7, CommandRemoveExtraCalmingQuad);
		SCR_REGISTER_UNUSED(REMOVE_ALL_EXTRA_CALMING_QUAD,0x0a0c7680ead49fa3, CommandRemoveAllExtraCalmingQuads);
		SCR_REGISTER_SECURE(SET_DEEP_OCEAN_SCALER,0x3999d3aa01285aaf,CommandSetDeepOceanScaler);
		SCR_REGISTER_SECURE(GET_DEEP_OCEAN_SCALER,0x241ee5a1d050510d,CommandGetDeepOceanScaler);
		SCR_REGISTER_SECURE(SET_CALMED_WAVE_HEIGHT_SCALER,0xae0b891009dee273,CommandSetCalmedWaveHeightScaler);
		
		SCR_REGISTER_SECURE(RESET_DEEP_OCEAN_SCALER,0x1a7781189253be0f,CommandResetDeepOceanScaler);
	}
}
