// File header
#include "Task/Movement/Climbing/ClimbDebug.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Scene/World/GameWorld.h"
#include "Task/Movement/Climbing/ClimbDetector.h"
#include "Task/Movement/Climbing/TaskVault.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

#if __BANK

//////////////////////////////////////////////////////////////////////////
// CClimbDebug
//////////////////////////////////////////////////////////////////////////

void CClimbDebug::SetupWidgets(bkBank& bank)
{
	bank.PushGroup("Climbing", false);

	CTaskVault::AddWidgets(bank);

	bank.PushGroup("Detector", false);
	bank.AddToggle("Display Search",                    &CClimbDetector::ms_bDebugDrawSearch);
	bank.AddToggle("Display Intersections",             &CClimbDetector::ms_bDebugDrawIntersections);
	bank.AddToggle("Display Compute",                   &CClimbDetector::ms_bDebugDrawCompute);
	bank.AddToggle("Display Validate",                  &CClimbDetector::ms_bDebugDrawValidate);
	bank.AddSlider("Heading Mod",                       &CClimbDetector::ms_fHeadingModifier, -PI, PI, 0.01f);
	bank.PopGroup(); // "Detector"

	bank.PopGroup(); // "Climbing"
}

void CClimbDebug::RenderDebug()
{
	// Render any detector tests
	CClimbDetector::Debug();
}

#endif // __BANK
