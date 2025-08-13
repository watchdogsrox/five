#ifndef PERFORMANCE_CLEARINGHOUSE_H
#define PERFORMANCE_CLEARINGHOUSE_H

#include "atl/array.h"
#include "fwdebug/debugdraw.h"
#include "fwmaths/Rect.h"

#define ENABLE_CLEARINGHOUSE __BANK

class perfClearingHouse
{
public:
	enum 
	{
		GAME = 0,
		TIME_STEP,
		WAIT_FOR_UPDATE,
		WAIT_FOR_GPU,
		STREAMING_REQUESTS,
		NORMAL_PRIORITY_STREAMING_REQUESTS,
		HIGH_PRIORITY_STREAMING_REQUESTS,
		PHYSICS_INSTANCES,
		RAGDOLLS,
		HIGH_LOD_RAGDOLLS,
		MEDIUM_LOD_RAGDOLLS,
		LOW_LOD_RAGDOLLS,
		ENTITIES,
		PEDS,
		HIGH_LOD_PEDS,
		LOW_LOD_PEDS,
		VEHICLES,
		NORMAL_VEHICLES,
		DUMMY_VEHICLES,
		SUPER_DUMMY_VEHICLES,
		OBJECTS,
		POPULATION_STREAMING,
		PED_STREAMING_OVERAGE,
		VEHICLE_STREAMING_OVERAGE,
		COMBAT_BEHAVIOURS,
		PTFX,
		PARTICLES,
		CLOTHS,
		CHARACTER_CLOTHS,
		CHARACTER_CLOTH_VERTICES,
		ENVIRONMENT_CLOTHS,
		ENVIRONMENT_CLOTH_VERTICES,
#if __BANK
		DEBUG_REGISTERS,
		DEBUG_0,
		DEBUG_1,
		DEBUG_2,
		DEBUG_3,
#endif
		NUM_SYSTEMS
	};

#if ENABLE_CLEARINGHOUSE

	class System
	{
	public:
		System(const char* name, u32 parent = 0, u32 allowance = 0, float weight = 1.0f) : m_Name(name), m_Parent(parent), m_Allowance(allowance), m_Weight(weight), m_Value(0), m_Total(0), m_Peak(0) {}
		~System() {}

		bool IsBusy() { return m_Allowance != 0 && m_Total > m_Allowance; }
		void Update();

#if DEBUG_DRAW
		void Render(float x, float& y, s32 depth = 0, float scaleX = 1.0f, float scaleY = 1.0f);
#endif // DEBUG_DRAW

	private:
		const char* m_Name;
		atArray<u32> m_Children;
		u32 m_Parent;
		u32 m_Allowance;
		u32 m_Value;
		u32 m_Total;
		u32 m_Peak;
		float m_Weight;

		friend class perfClearingHouse;
	};

	static bool IsBusy(u32 system) { Assert(system < NUM_SYSTEMS); return sm_Systems[system].IsBusy(); }
	static void Increment(u32 system) { Assert(system < NUM_SYSTEMS); ++sm_Systems[system].m_Value; }
	static void SetValue(u32 system, u32 value) { Assert(system < NUM_SYSTEMS); sm_Systems[system].m_Value = value; }
	static void Reset();
	static void Initialize(unsigned initMode);
	static void Update();

#if DEBUG_DRAW
	static void Render(fwRect& rect, float scaleX = 1.0f, float scaleY = 1.0f);
#endif // DEBUG_DRAW

private:
	static System sm_Systems[NUM_SYSTEMS];

#else // ENABLE_CLEARINGHOUSE

	static bool IsBusy(u32 UNUSED_PARAM(system)) { return false; }
	static void Increment(u32 UNUSED_PARAM(system)) {}
	static void SetValue(u32 UNUSED_PARAM(system), u32 UNUSED_PARAM(value)) {}
	static void Reset() {}
	static void Initialize(unsigned UNUSED_PARAM(initMode)) {}
	static void Update() {}

#if DEBUG_DRAW
	static void Render(fwRect& UNUSED_PARAM(rect)) {}
#endif // DEBUG_DRAW

#endif // !ENABLE_CLEARINGHOUSE
};

#endif // PERFORMANCE_CLEARINGHOUSE_H