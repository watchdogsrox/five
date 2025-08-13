#include "clearinghouse.h"

#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "grcore/viewport.h"
#include "vector/color32.h"
#include "vector/colors.h"

#if ENABLE_CLEARINGHOUSE
perfClearingHouse::System perfClearingHouse::sm_Systems[NUM_SYSTEMS] = {
	// Order must match enumeration, see clearinghouse.h!
	// Name										Parent					Allowance	Weight
	System("Game"),
		System("Time Step (ms)",				GAME,					33,			1.0f),
		System("Wait for Update (ms)",			GAME,					1,			1.0f),
		System("Wait for GPU (ms)",				GAME,					1,			1.0f),
		System("Streaming Requests",			GAME,					1,			1.0f),
			System("Normal Priority",			STREAMING_REQUESTS,		0,			1.0f),
			System("High Priority",				STREAMING_REQUESTS,		0,			1.0f),
		System("Physics Instances",				GAME,					5,			1.0f),
			System("Ragdolls",					PHYSICS_INSTANCES,		0,			1.0f),
				System("High",					RAGDOLLS,				0,			1.0f),
				System("Medium",				RAGDOLLS,				0,			1.0f),
				System("Low",					RAGDOLLS,				0,			1.0f),
		System("Entities",						GAME,					40,			1.0f),
			System("Peds",						ENTITIES,				0,			1.0f),
				System("High",					PEDS,					0,			1.0f),
				System("Low",					PEDS,					0,			1.0f),
			System("Vehicles",					ENTITIES,				0,			1.0f),
				System("Normal",				VEHICLES,				0,			1.0f),
				System("Dummies",				VEHICLES,				0,			1.0f),
				System("Super Dummies",			VEHICLES,				0,			1.0f),
			System("Objects",					ENTITIES,				0,			1.0f),
		System("Population Streaming",			GAME,					0,			1.0f),
			System("Ped Overage",				POPULATION_STREAMING,	0,			1.0f),
			System("Vehicle Overage",			POPULATION_STREAMING,	0,			1.0f),
		System("Combat Behaviours",				GAME,					10,			1.0f),
		System("PTFX",							GAME,					50,			1.0f),
			System("Particles",					PTFX,					1000,		0.0f),
		System("Cloths",						GAME,					0,			1.0f),
			System("Character",					CLOTHS,					1,			1.0f),
				System("Vertices",				CHARACTER_CLOTHS,		200,		0.0f),
			System("Environment",				CLOTHS,					2,			1.0f),
				System("Vertices",				ENVIRONMENT_CLOTHS,		400,		0.0f),
#if __BANK
		System("Debug",							GAME,					0,			0.0f),
			System("0",							DEBUG_REGISTERS,		0,			1.0f),
			System("1",							DEBUG_REGISTERS,		0,			1.0f),
			System("2",							DEBUG_REGISTERS,		0,			1.0f),
			System("3",							DEBUG_REGISTERS,		0,			1.0f)
#endif
};

void perfClearingHouse::System::Update()
{
	if(fwTimer::IsGamePaused())
	{
		m_Value = 0;
		return;
	}

	m_Total = m_Value;

	for(u32 i = 0; i < m_Children.GetCount(); ++i)
	{
		perfClearingHouse::sm_Systems[m_Children[i]].Update();
		m_Total += static_cast<u32>(perfClearingHouse::sm_Systems[m_Children[i]].m_Total * perfClearingHouse::sm_Systems[m_Children[i]].m_Weight);
	}

	if(m_Total > m_Peak)
	{
		m_Peak = m_Total;
	}

	m_Value = 0;
}

void perfClearingHouse::Reset()
{
	for(u32 i = 0; i < NUM_SYSTEMS; ++i)
	{
		sm_Systems[i].m_Value = 0;
		sm_Systems[i].m_Total = 0;
		sm_Systems[i].m_Peak = 0;
	}
}

void perfClearingHouse::Initialize(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		for(u32 i = 1; i < NUM_SYSTEMS; ++i)
		{
			sm_Systems[sm_Systems[i].m_Parent].m_Children.PushAndGrow(i, 1);
		}
	}
}

void perfClearingHouse::Update()
{
	SetValue(TIME_STEP, fwTimer::GetSystemTimeStepInMilliseconds());

	sm_Systems[GAME].Update();
}

////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
#define LINE_INDENT 2
#define COLUMN_OFFSET 15

void perfClearingHouse::System::Render(float x, float& y, s32 depth, float scaleX, float scaleY)
{
	static bool oddLine = true;

	if(depth == 0)
	{
		oddLine = true;
	}

	float debugCharHeight = static_cast<float>(grcFont::GetCurrent().GetHeight()) / grcViewport::GetDefaultScreen()->GetHeight();

	char buffer[2][256];

	Color32 color = oddLine ? Color32(1.0f, 0.5f, 0.3f, 1.0f) : Color32(1.0f, 0.5f, 0.7f, 1.0f);
	
	if(depth != 0)
	{
		if(m_Allowance != 0)
		{
			Color32 busyColor =  Color32(1.0f, 0.0f, 0.0f, 1.0f);

			sprintf(buffer[0], "%*s%s", depth * LINE_INDENT, "", m_Name);
			sprintf(buffer[1], "%*s%*i%*i", -COLUMN_OFFSET * 2, buffer[0], -COLUMN_OFFSET, m_Allowance, -COLUMN_OFFSET * 2, m_Total);

			if(IsBusy())
			{
				color = busyColor;
				safecat(buffer[1], "Busy");
			}
			else
			{
				safecat(buffer[1], "Not Busy");
			}

			grcDebugDraw::Text(Vector2(x, y), color, buffer[1], false, scaleX, scaleY);

			if(m_Peak > m_Allowance)
			{
				color = busyColor;
			}

			sprintf(buffer[0], "%*s(%i)", COLUMN_OFFSET * 4, "", m_Peak);
			grcDebugDraw::Text(Vector2(x, y), color, buffer[0], false, scaleX, scaleY);
		}
		else
		{
			sprintf(buffer[0], "%*s%s", depth * LINE_INDENT, "", m_Name);
			sprintf(buffer[1], "%*s%*i(%i)", -COLUMN_OFFSET * 3, buffer[0], -COLUMN_OFFSET, m_Total, m_Peak);

			grcDebugDraw::Text(Vector2(x, y), color, buffer[1], false, scaleX, scaleY);
		}

		oddLine = !oddLine;
	}
	else
	{
		grcDebugDraw::Text(Vector2(x, y), color, m_Name, false, scaleX, scaleY);

		oddLine = !oddLine;
	}

	y += debugCharHeight*scaleY;
	depth += 1;

	for(u32 i = 0; i < m_Children.GetCount(); ++i)
	{
		perfClearingHouse::sm_Systems[m_Children[i]].Render(x, y, depth, scaleX, scaleY);
	}
}

void perfClearingHouse::Render(fwRect& rect, float scaleX, float scaleY)
{
	float debugCharHeight = static_cast<float>(grcFont::GetCurrent().GetHeight()) / grcViewport::GetDefaultScreen()->GetHeight();

	float x = rect.left / grcViewport::GetDefaultScreen()->GetWidth();
	float y = (rect.top / grcViewport::GetDefaultScreen()->GetHeight()) + debugCharHeight;

	grcDebugDraw::Text(Vector2(x, y), Color_white, "SYSTEM LOAD", false, scaleX, scaleY);

	y += debugCharHeight * 2 * scaleY;

	char buffer[256];

	sprintf(buffer, "%*s%*s%*s%*s%s", COLUMN_OFFSET * 2, "", -COLUMN_OFFSET, "Allowance", -COLUMN_OFFSET, "Actual", -COLUMN_OFFSET, "Peak", "Status");

	grcDebugDraw::Text(Vector2(x, y), Color_white, buffer, false, scaleX, scaleY);

	y += debugCharHeight * scaleY;

	sm_Systems[GAME].Render(x, y, 0, scaleX, scaleY);

	rect.top = (y + debugCharHeight) * grcViewport::GetDefaultScreen()->GetHeight();
}
#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_CLEARINGHOUSE