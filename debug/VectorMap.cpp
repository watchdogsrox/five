/////////////////////////////////////////////////////////////////////////////////
// FILE :    vectormap.cpmovementPortionOfCollision
// PURPOSE : A debug map that can be viewed on top of the game
// AUTHOR(s) :  Obbe, Adam Croston
// CREATED : 31/08/06
/////////////////////////////////////////////////////////////////////////////////
#include "debug/vectormap.h"

// Rage headers
#include "math/vecMath.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"

// Game headers
#include "scene/scene.h"
#include "scene/world/gameWorld.h"
#include "debug/debugscene.h"
#include "peds/ped.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/Viewports/Viewport.h"  
#include "vehicles/train.h" 
#include "vehicles/cargen.h" 
#include "Vfx/Misc/DistantLights.h" 

AI_OPTIMISATIONS()

#if __BANK

//////////////////////////////////////////////////////////////////////////////////
// Event Ripples
//////////////////////////////////////////////////////////////////////////////////
static bool s_displayEventRipples = true;		// Whether or not to display event ripples.

// A helper class to hold the data of a single temporal marker.
struct EventRipple
{
	EventRipple	();
	EventRipple	(	const Vector3&	worldPos, 
		const float		endWorldRadius, 
		const u32	lifeTimeMs, 
		const Color32	baseColour);

	void Update	(	const u32	currentTimeMs);
	void Draw	(	const u32	currentTimeMs);

	bool	m_inUse;
	Vector3	m_worldPos;
	float	m_endWorldRadius;
	u32	m_lifeTimeMs;
	Color32	m_baseColour;
	u32	m_startTimeMs;
};
EventRipple::EventRipple()
: // initializer list.
m_inUse			(false), 
m_worldPos		(0.0f, 0.0f, 0.0f), 
m_endWorldRadius(0.0f), 
m_lifeTimeMs	(0), 
m_baseColour	(0, 0, 0, 0), 
m_startTimeMs	(0)
{
	;
}
EventRipple::EventRipple(	const Vector3&	worldPos, 
						 const float		endWorldRadius, 
						 const u32	lifeTimeMs, 
						 const Color32	baseColour)
						 : // Initializer list.
m_inUse			(true), 
m_worldPos		(worldPos), 
m_endWorldRadius(endWorldRadius), 
m_lifeTimeMs	(lifeTimeMs), 
m_baseColour	(baseColour), 
m_startTimeMs	(fwTimer::GetTimeInMilliseconds())
{
	;
}
void EventRipple::Update(const u32 currentTimeMs)
{
	const u32 endTimeMs = m_startTimeMs + m_lifeTimeMs;
	if(currentTimeMs > endTimeMs)
	{
		m_inUse = false;
	}
}
void EventRipple::Draw(const u32 currentTimeMs)
{
	if(!m_inUse)
	{
		return;
	}

	Assert(currentTimeMs <= (m_startTimeMs + m_lifeTimeMs));

	const u32		timeAliveMs				= currentTimeMs - m_startTimeMs;
	const float		lifePortion				= static_cast<float>(timeAliveMs) / static_cast<float>(m_lifeTimeMs);
	const s32		currentAlphaUnclamped	= static_cast<s32>((1.0f-lifePortion) * m_baseColour.GetAlpha());
	const s32		currentAlpha			= rage::Clamp(currentAlphaUnclamped, 0, 255);
	const Color32	currentColour			(	m_baseColour.GetRed(), 
		m_baseColour.GetGreen(), 
		m_baseColour.GetBlue(), 
		currentAlpha);

	Vector2 mapPos;
	fwVectorMap::ConvertPointWorldToMap(m_worldPos, mapPos);
	float currentMapRadius;
	fwVectorMap::ConvertSizeWorldToMap(m_endWorldRadius * lifePortion, currentMapRadius);

	grcDebugDraw::Circle(	mapPos, 
		currentMapRadius, 
		currentColour, 
		false);
}

// A helper class to manage and update many temporal markers.
#define MAX_NUM_RIPPLES	128
class EventRippleManager
{
public:
	static void Init			();
	static void Shutdown		();
	static void Update			();
	static void Draw			();

	static bool MakeEventRipple	(	const Vector3&	worldPos, 
		const float		endWorldRadius, 
		const u32	lifeTimeMs, 
		const Color32	baseColour);
protected:
	static EventRipple	m_eventRipples[MAX_NUM_RIPPLES];
	static u32		m_currentTimeMs;
};
EventRipple	EventRippleManager::m_eventRipples[MAX_NUM_RIPPLES];
u32		EventRippleManager::m_currentTimeMs		= 0;
void EventRippleManager::Init()
{
	for(u32 i  = 0; i < MAX_NUM_RIPPLES; ++i)
	{
		m_eventRipples[i].m_inUse = false;
	}

	m_currentTimeMs = 0;
}
void EventRippleManager::Shutdown()
{
}
void EventRippleManager::Update()
{
	m_currentTimeMs = fwTimer::GetTimeInMilliseconds();

	for(u32 i  = 0; i < MAX_NUM_RIPPLES; ++i)
	{
		if(m_eventRipples[i].m_inUse)
		{
			m_eventRipples[i].Update(m_currentTimeMs);
		}
	}
}
void EventRippleManager::Draw()
{
	for(u32 i  = 0; i < MAX_NUM_RIPPLES; ++i)
	{
		if(m_eventRipples[i].m_inUse)
		{
			m_eventRipples[i].Draw(m_currentTimeMs);
		}
	}
}
bool EventRippleManager::MakeEventRipple(	const Vector3&	worldPos, 
										 const float		endWorldRadius, 
										 const u32	lifeTimeMs, 
										 const Color32	baseColour)
{
	// Find an empty slot.
	for(u32 i  = 0; i < MAX_NUM_RIPPLES; ++i)
	{
		if(!m_eventRipples[i].m_inUse)
		{
			// Set the values in the slot.
			EventRipple tempMarker(	worldPos, 
				endWorldRadius, 
				lifeTimeMs, 
				baseColour);
			m_eventRipples[i] = tempMarker;

			// Let the caller know we made a marker.
			return true;
		}
	}

	// Let the caller know we could not make a marker.
	return false;
}


// CVectorMap
//////////////////////////////////////////////////////////////////////////////////
bool	CVectorMap::m_bFocusOnPlayer					= false;
bool	CVectorMap::m_bFocusOnCamera					= true;
bool	CVectorMap::m_bRotateWithPlayer					= false;
bool	CVectorMap::m_bRotateWithCamera					= true;
bool	CVectorMap::m_bDisplayDefaultPlayerIndicator	= false;
bool	CVectorMap::m_bDisplayLocalPlayerCamera			= false;
float	CVectorMap::m_vehicleVectorMapScale				= 3.0f;
float	CVectorMap::m_pedVectorMapScale					= 5.0f;
bool	CVectorMap::m_bDisplayDebugNameWithPed			= false;
float	CVectorMap::m_waterLevel						= 0.0f;
float	CVectorMap::m_fLastTanFOV						= 1.0f;

CVectorMap::CVectorMap()
{
	EventRippleManager::Init();
}

CVectorMap::~CVectorMap()
{
	EventRippleManager::Shutdown();
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: UpdateFocus
//			 Update the focus of the map for this frame.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::UpdateFocus()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();

	// Determine the focus and rotation of the map.
	// it may use the normal pan values or it might use the player or camera as a source.
	if (m_bFocusOnPlayer)
	{
		m_FocusWorldX = CGameWorld::FindLocalPlayerCoors().x;
		m_FocusWorldY = CGameWorld::FindLocalPlayerCoors().y;
	}
	else if (m_bFocusOnCamera)
	{
		// Get the camera to use.
		bool isUsingFreeCamera = debugDirector.IsFreeCamActive();
		const camFrame& frame = isUsingFreeCamera ? debugDirector.GetFreeCamFrame() : camInterface::GetGameplayDirector().GetFrame();

		// Get the cameras position.
		m_FocusWorldX = frame.GetPosition().x;
		m_FocusWorldY = frame.GetPosition().y;
	}
	else
	{
		// Just use the pan values already set.
		fwVectorMap::UpdateFocus();
	}
}

//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: UpdateRotation
//			 Update the rotation of the map for this frame.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::UpdateRotation()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();

	if(m_bRotateWithPlayer)
	{
		m_MapRotationRads = -CGameWorld::FindLocalPlayerHeading();
	}
	else if(m_bRotateWithCamera)
	{
		// Get the camera to use.
		bool isUsingFreeCamera = debugDirector.IsFreeCamActive();
		const camFrame& frame = isUsingFreeCamera ? debugDirector.GetFreeCamFrame() : camInterface::GetGameplayDirector().GetFrame();
		// Use the cameras forward vector to get a heading.
		m_MapRotationRads = -frame.ComputeHeading();
	}
	else
	{
		// Don'seg0Portion_out rotate the map.
		fwVectorMap::UpdateRotation();
	}
}

//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: Update
//			 Should be called at the end of the update loop.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::UpdateCore()
{
	EventRippleManager::Update();
	fwVectorMap::UpdateCore();
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: Draw
//			 Draws the map.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::DrawCore()
{
	// Draw events and ripples.
	if(s_displayEventRipples)
	{
		EventRippleManager::Draw();
	}

	// Draw an indicator for the player so that we know where we are.
	if(m_bDisplayDefaultPlayerIndicator)
	{
		if (!CDebugScene::bDisplayPedsOnVMap && !CDebugScene::bDisplayPedsToBeStreamedOutOnVMap)
		{
			CPed *pPed;
			if ((pPed = FindPlayerPed()) != NULL)
			{
				Color32 Colour = Color32(255, 255, 255, 255);
				if (fwTimer::GetTimeInMilliseconds() & 512)
				{
					Colour = Color32(0, 0, 0, 255);
				}

				if (pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					DrawVehicle(pPed->GetMyVehicle(), Colour);
				}
				else
				{
					DrawPed(pPed, Colour);
				}
			}
		}
	}

	// Draw the local player camera.
	if(m_bDisplayLocalPlayerCamera)
	{
		DrawLocalPlayerCamera(Color32(255, 255, 255, 80));
	}

	fwVectorMap::DrawCore();
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: DrawVehicle
//			 Draws a vehicle on the map.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::DrawVehicle(const CVehicle* pVehicle, Color32 col, bool bForceMap)
{
	m_bMapShouldBeDrawnThisFrame = m_bMapShouldBeDrawnThisFrame || bForceMap;		// Force the map itself to be drawn
	if(!m_bAllowDisplayOfMap || !m_bMapShouldBeDrawnThisFrame){return;}

	Vector3	coors = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vector3	forward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
	Vector3	side = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());

	Vector3 sideLeft = side * (pVehicle->GetBoundingBoxMin().x * m_vehicleVectorMapScale);
	Vector3 sideRight = side * (pVehicle->GetBoundingBoxMax().x * m_vehicleVectorMapScale);
	forward *= (pVehicle->GetBoundingBoxMax().y * m_vehicleVectorMapScale);

	Vector3 backLeft(coors.x + sideLeft.x - forward.x, coors.y + sideLeft.y - forward.y, 0.0f);
	Vector3 backRight(coors.x + sideRight.x - forward.x, coors.y + sideRight.y - forward.y, 0.0f);
	Vector3 forwardLeft(coors.x + sideLeft.x + forward.x, coors.y + sideLeft.y + forward.y, 0.0f);
	Vector3 forwardRight(coors.x + sideRight.x + forward.x, coors.y + sideRight.y + forward.y, 0.0f);

	DrawLine(forwardLeft, forwardRight, col, col, bForceMap);		// forward bumper
	DrawLine(backLeft, backRight, col, col, bForceMap);		// rear bumper
	DrawLine(backLeft, forwardLeft, col, col, bForceMap);		// left side
	DrawLine(backRight, forwardRight, col, col, bForceMap);		// right side

	// line a bit before back from forward bumper (so that we can see the orientation of the car)
	DrawLine(Vector3(coors.x + sideRight.x + 0.4f * forward.x, coors.y + sideRight.y + 0.4f * forward.y, 0.0f), 
			 Vector3(coors.x + sideLeft.x + 0.4f * forward.x, coors.y + sideLeft.y + 0.4f * forward.y, 0.0f),
			 col, col, bForceMap);		

	// Draw a little x in the middle if the car is parked.
	if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
	{
		DrawLine(forwardRight, backLeft, col, col, bForceMap);
		DrawLine(backRight, forwardLeft, col, col, bForceMap);
	}
	else
	{
		forward *= 0.05f;
		DrawLine(Vector3(coors.x + forward.x, coors.y + forward.y, 0.0f), Vector3(coors.x - forward.x, coors.y - forward.y, 0.0f), col, col, bForceMap);
	}
}

void CVectorMap::DrawVehicleLodInfo(const CVehicle* pVehicle, Color32 col, float fScale, bool bForceMap)
{
	m_bMapShouldBeDrawnThisFrame = m_bMapShouldBeDrawnThisFrame || bForceMap;		// Force the map itself to be drawn
	if(!m_bAllowDisplayOfMap || !m_bMapShouldBeDrawnThisFrame)
		return;

	Vector3	coors = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vector3	forward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
	Vector3	side = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());

	u32 uTimeSinceCreation = fwTimer::GetTimeInMilliseconds() - pVehicle->m_TimeOfCreation;
	fScale *= RampValue((float)uTimeSinceCreation, 0.0f, 1000.0f, 3.0f, 1.0f);

	Vector3 sideLeft = side * (pVehicle->GetBoundingBoxMin().x * fScale);
	Vector3 sideRight = side * (pVehicle->GetBoundingBoxMax().x * fScale);
	forward *= (pVehicle->GetBoundingBoxMax().y * fScale);

	Vector3 backLeft(coors.x + sideLeft.x - forward.x, coors.y + sideLeft.y - forward.y, 0.0f);
	Vector3 backRight(coors.x + sideRight.x - forward.x, coors.y + sideRight.y - forward.y, 0.0f);
	Vector3 forwardLeft(coors.x + sideLeft.x + forward.x, coors.y + sideLeft.y + forward.y, 0.0f);
	Vector3 forwardRight(coors.x + sideRight.x + forward.x, coors.y + sideRight.y + forward.y, 0.0f);
	
	Color32 endCol = pVehicle->IsNetworkClone() ? Color_black : col;
	DrawLine(forwardLeft, forwardRight, endCol, endCol, bForceMap);		// forward bumper
	DrawLine(backLeft, backRight, endCol, endCol, bForceMap);		// rear bumper
	DrawLine(backLeft, forwardLeft, col, col, bForceMap);		// left side
	DrawLine(backRight, forwardRight, col, col, bForceMap);		// right side

	Color32 oriCol = pVehicle->GetIsVisibleInSomeViewportThisFrame() ? Color_white : col;
	// line a bit before back from forward bumper. White line means car is visible
	DrawLine(Vector3(coors.x + sideRight.x + 0.4f * forward.x, coors.y + sideRight.y + 0.4f * forward.y, 0.0f), 
			 Vector3(coors.x + sideLeft.x + 0.4f * forward.x, coors.y + sideLeft.y + 0.4f * forward.y, 0.0f),
			 oriCol, oriCol, bForceMap);			

	// Draw a little x in the middle if the car is parked.
	if(pVehicle->ConsiderParkedForLodPurposes())
	{
		DrawLine(forwardRight, backLeft, oriCol, oriCol, bForceMap);
		DrawLine(backRight, forwardLeft, oriCol, oriCol, bForceMap);
	}
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: DrawPed
//			 Draws a ped on the map.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::DrawPed(const CEntity* pPed, Color32 col, bool bForceMap)
{
	m_bMapShouldBeDrawnThisFrame = m_bMapShouldBeDrawnThisFrame || bForceMap;		// Force the map itself to be drawn
	if(!m_bAllowDisplayOfMap || !m_bMapShouldBeDrawnThisFrame){return;}

	const Vector3 originalCoors	= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 forward		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
	const Vector3 side			= VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());

	// Get the dimensions of our ped triangle to draw.
	const float pedSize = 0.7f;
	const float triangleSize = pedSize * m_pedVectorMapScale;
	const float triangleHalfWidth  = triangleSize / 3.0f;
	const float triangleHeight  = triangleSize;

	// Centre ped triangle a bit better
	Vector3 coors = originalCoors;
	coors.x -= (triangleHeight / 3.0f) * forward.x;
	coors.y -= (triangleHeight / 3.0f) * forward.y;

	DrawLine(Vector3(coors.x + triangleHalfWidth * side.x, coors.y + triangleHalfWidth * side.y, 0.0f), Vector3(coors.x - triangleHalfWidth * side.x, coors.y - triangleHalfWidth * side.y, 0.0f), col, col, bForceMap);
	DrawLine(Vector3(coors.x + triangleHalfWidth * side.x, coors.y + triangleHalfWidth * side.y, 0.0f), Vector3(coors.x + triangleHeight * forward.x, coors.y + triangleHeight * forward.y, 0.0f), col, col, bForceMap);
	DrawLine(Vector3(coors.x - triangleHalfWidth * side.x, coors.y - triangleHalfWidth * side.y, 0.0f), Vector3(coors.x + triangleHeight * forward.x, coors.y + triangleHeight * forward.y, 0.0f), col, col, bForceMap);

	if(m_bDisplayDebugNameWithPed && pPed->GetIsTypePed())
	{
		const CPed* pPedReal = static_cast<const CPed*>(pPed);
		DrawString(originalCoors, pPedReal->m_debugPedName, col, false);
	}
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: DrawLocalPlayerCamera
//			 Draws the local player camera on the map.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::DrawLocalPlayerCamera(Color32 col, bool bDrawDof, bool bForceMap)
{
	m_bMapShouldBeDrawnThisFrame = m_bMapShouldBeDrawnThisFrame || bForceMap;		// Force the map itself to be drawn
	if(!m_bAllowDisplayOfMap || !m_bMapShouldBeDrawnThisFrame){return;}

	const Matrix34&	camMat = camInterface::GetMat();

	// this camera matrix is in the form specified from the camera interface (where the b Vector is the forward vector)
	const Vector3	vRight		= camMat.a;
	const Vector3	vForward	= camMat.b;
	const Vector3	vUp			= camMat.c;
	const Vector3	vPos		= camMat.d;
	const float		farPlane	= camInterface::GetFar();

	float tanHFov = camInterface::GetViewportTanFovH();
	m_fLastTanFOV = tanHFov;
	float tanVFov = camInterface::GetViewportTanFovV();
	const Vector3	vFarPlaneUpRight	= vPos + (vForward + vUp * tanVFov + vRight * tanHFov) * farPlane;
	const Vector3	vFarPlaneUpLeft		= vPos + (vForward + vUp * tanVFov - vRight * tanHFov) * farPlane;
	const Vector3	vFarPlaneDownRight	= vPos + (vForward - vUp * tanVFov + vRight * tanHFov) * farPlane;
	const Vector3	vFarPlaneDownLeft	= vPos + (vForward - vUp * tanVFov - vRight * tanHFov) * farPlane;
	DrawLine(vPos, vFarPlaneUpRight, col, col, bForceMap);
	DrawLine(vPos, vFarPlaneUpLeft, col, col, bForceMap);
	DrawLine(vPos, vFarPlaneDownRight, col, col, bForceMap);
	DrawLine(vPos, vFarPlaneDownLeft, col, col, bForceMap);
	DrawLine(vFarPlaneUpRight, vFarPlaneUpLeft, col, col, bForceMap);
	DrawLine(vFarPlaneUpLeft, vFarPlaneDownLeft, col, col, bForceMap);
	DrawLine(vFarPlaneDownLeft, vFarPlaneDownRight, col, col, bForceMap);
	DrawLine(vFarPlaneDownRight, vFarPlaneUpRight, col, col, bForceMap);

	if(bDrawDof)
	{
		const Vector3	vFarPlaneRight		= vPos + (vForward + vRight * tanHFov) * farPlane;
		const Vector3	vFarPlaneLeft		= vPos + (vForward - vRight * tanHFov) * farPlane;

		const float		nearDOF				= camInterface::GetNearDof();
		const float		farDOF				= camInterface::GetFarDof();
		const Vector3	edgeRightDelta		= vFarPlaneRight - vPos;
		const Vector3	edgeLeftDelta		= vFarPlaneLeft - vPos;

		const Vector3	nearDOFEdgeRight	= vPos + ((nearDOF/farPlane) * edgeRightDelta);
		const Vector3	nearDOFEdgeLeft		= vPos + ((nearDOF/farPlane) * edgeLeftDelta);
		DrawLine(nearDOFEdgeRight, nearDOFEdgeLeft, col, col, bForceMap);

		const Vector3	farDOFEdgeRight		= vPos + ((farDOF/farPlane) * edgeRightDelta);
		const Vector3	farDOFEdgeLeft		= vPos + ((farDOF/farPlane) * edgeLeftDelta);
		DrawLine(farDOFEdgeRight, farDOFEdgeLeft, col, col, bForceMap);
	}
}

//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: MakeEventRipple
//			 causes an expanding and fading circle to draw on the map.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::MakeEventRipple(const Vector3& pos, float finalRadius, unsigned int lifetimeMs, const Color32 baseColour, bool bForceMap)
{
	m_bMapShouldBeDrawnThisFrame = m_bMapShouldBeDrawnThisFrame || bForceMap;		// Force the map itself to be drawn
	if(!m_bAllowDisplayOfMap || !m_bMapShouldBeDrawnThisFrame){return;}

	EventRippleManager::MakeEventRipple(pos, finalRadius, lifetimeMs, baseColour);
};

//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: DefaultSettings
//			 Sets some decent settings for debugging stuff.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::CleanSettingsCore()
{
	m_bFocusOnPlayer = false;
	m_bFocusOnCamera = false;
	m_bRotateWithCamera = false;
	m_bRotateWithPlayer = false;

	m_bDisplayDefaultPlayerIndicator = false;
	m_bDisplayLocalPlayerCamera = false;
	m_bDisplayDebugNameWithPed = false;

	m_waterLevel = 0.0f;
	ThePaths.bDisplayPathsOnVMapDontForceLoad = false;
	ThePaths.bDisplayPathsOnVMapFlashingForSwitchedOffNodes = false;
	CDebugScene::bDisplayBuildingsOnVMap = false;
	CDebugScene::bDisplayPedsOnVMap = false;
	CDebugScene::bDisplayVehiclesOnVMap = false;
	CDebugScene::bDisplayVehiclesUsesFadeOnVMap = true;

	fwVectorMap::CleanSettingsCore();
}


//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: GoodSettings
//			 Sets some decent settings for debugging stuff.
//////////////////////////////////////////////////////////////////////////////////
void CVectorMap::GoodSettingsCore()
{
	m_bFocusOnPlayer = true;
	m_bFocusOnCamera = false;
	m_bRotateWithCamera = true;
	m_bRotateWithPlayer = false;

	m_bDisplayDefaultPlayerIndicator = true;
	m_bDisplayLocalPlayerCamera = true;
	m_bDisplayDebugNameWithPed = true;

	m_waterLevel = 0.0f;
	ThePaths.bDisplayPathsOnVMapDontForceLoad = true;
	ThePaths.bDisplayPathsOnVMapFlashingForSwitchedOffNodes = false;
	CDebugScene::bDisplayBuildingsOnVMap = false;
	CDebugScene::bDisplayPedsOnVMap = true;
	CDebugScene::bDisplayVehiclesOnVMap = true;
	CDebugScene::bDisplayVehiclesUsesFadeOnVMap = true;

	fwVectorMap::GoodSettingsCore();
}

void CVectorMap::AddGameSpecificWidgets(bkBank &bank)
{
	bank.AddToggle("Focus on Player", &m_bFocusOnPlayer);
	bank.AddToggle("Focus on Camera", &m_bFocusOnCamera);
	bank.AddToggle("Rotate with Player", &m_bRotateWithPlayer);
	bank.AddToggle("Rotate with Camera", &m_bRotateWithCamera);

	bank.AddToggle("Display Default Player Indicator", &m_bDisplayDefaultPlayerIndicator);
	bank.AddToggle("Display Local Player Camera", &m_bDisplayLocalPlayerCamera);

	bank.AddSlider("Veh Scale", &m_vehicleVectorMapScale, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Ped Scale", &m_pedVectorMapScale, 0.0f, 100.0f, 0.1f);
	bank.AddToggle("Display Debug Name With Ped", &m_bDisplayDebugNameWithPed);

	bank.AddSlider("Water Level", &m_waterLevel, -1000.0f, 1000.0f, 10.0f);

	bank.AddToggle("Display path lines (dont force load)", &ThePaths.bDisplayPathsOnVMapDontForceLoad);
	bank.AddToggle("Display path lines with density (dont force load)", &ThePaths.bDisplayPathsWithDensityOnVMapDontForceLoad);
	bank.AddToggle("Display path lines", &ThePaths.bDisplayPathsOnVMap);
	bank.AddToggle("Display path lines with density", &ThePaths.bDisplayPathsWithDensityOnVMap);
	bank.AddToggle("Display only players road nodes (on VM)", &ThePaths.bDisplayOnlyPlayersRoadNodes);


	bank.AddToggle("Display buildings ",						&CDebugScene::bDisplayBuildingsOnVMap);
	bank.AddToggle("Display scene scored entities ",			&CDebugScene::bDisplaySceneScoredEntities);

	bank.AddToggle("Display vehicles ",							&CDebugScene::bDisplayVehiclesOnVMap);
	bank.AddToggle("Display lines to vehicles ",				&CDebugScene::bDisplayLinesToLocalDrivingCars);
	bank.AddToggle("Display vehicles active/inactive ",			&CDebugScene::bDisplayVehiclesOnVMapAsActiveInactive);
	bank.AddToggle("Display vehicles uses fade",				&CDebugScene::bDisplayVehiclesUsesFadeOnVMap);
	bank.AddToggle("Display veh pop failed create events",		&CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap);
	bank.AddToggle("Display veh pop create events",				&CDebugScene::bDisplayVehPopCreateEventsOnVMap);
	bank.AddToggle("Display veh pop destroy events",			&CDebugScene::bDisplayVehPopDestroyEventsOnVMap);
	bank.AddToggle("Display veh pop conversion events",			&CDebugScene::bDisplayVehPopConversionEventsOnVMap);
	bank.AddToggle("Display veh creation paths",				&CDebugScene::bDisplayVehicleCreationPathsOnVMap);
	bank.AddToggle("Display veh creation paths curr density",	&CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap);
	bank.AddToggle("Display vehs to stream out",				&CDebugScene::bDisplayVehiclesToBeStreamedOutOnVMap);
	bank.AddToggle("Display veh collisions",					&CDebugScene::bDisplayVehicleCollisionsOnVMap);

	bank.AddToggle("Display peds",								&CDebugScene::bDisplayPedsOnVMap);
	bank.AddToggle("Display peds as active/inactive",			&CDebugScene::bDisplayPedsOnVMapAsActiveInactive);
	bank.AddToggle("Display spawn point raw density",			&CDebugScene::bDisplaySpawnPointsRawDensityOnVMap);
	bank.AddToggle("Display ped population events",				&CDebugScene::bDisplayPedPopulationEventsOnVMap);
	bank.AddToggle("Display peds to stream out",				&CDebugScene::bDisplayPedsToBeStreamedOutOnVMap);
	bank.AddToggle("Display candidate scenario points",			&CDebugScene::bDisplayCandidateScenarioPointsOnVMap);
#if __DEV
	bank.AddToggle("Display scene update cost",					&CDebugScene::bDisplaySceneUpdateCostOnVMap);
	bank.AddToggle("Scene update cost for selected entity only",&CDebugScene::bDisplaySceneUpdateCostSelectedOnly);
	bank.AddSlider("Scene update operation to profile",			&CDebugScene::iDisplaySceneUpdateCostStep, 0, 31, 1, CDebugScene::UpdateSceneUpdateCostStep);
	CDebugScene::UpdateSceneUpdateCostStep();
	bank.AddText("Scene update step being profiled",			CDebugScene::DisplaySceneUpdateCostStepName, sizeof(CDebugScene::DisplaySceneUpdateCostStepName), true);
#endif // __DEV

	bank.AddToggle("Display Event Ripples", &s_displayEventRipples);

	bank.AddToggle("Display objects", &CDebugScene::bDisplayObjectsOnVMap);
	bank.AddToggle("Display pickups", &CDebugScene::bDisplayPickupsOnVMap);
	bank.AddToggle("Display network game", &CDebugScene::bDisplayNetworkGameOnVMap);
	bank.AddToggle("Display portal insts", &CDebugScene::bDisplayPortalInstancesOnVMap);
	bank.AddToggle("Display remote player cameras", &CDebugScene::bDisplayRemotePlayerCameras);
	bank.AddToggle("Display car creation (network only)", &CDebugScene::bDisplayCarCreation);
	bank.AddToggle("Display train tracks", &CTrain::sm_bDisplayTrainAndTrackDebugOnVMap);
	bank.AddToggle("Display car generators", &CTheCarGenerators::gs_bDisplayCarGeneratorsOnVMap);
	//bank.AddToggle("Display distant lights", &g_distantLights.GetDisplayOnVMap());
	bank.AddToggle("Display water", &CDebugScene::bDisplayWaterOnVMap);
	bank.AddToggle("Display audio shorelines", &CDebugScene::bDisplayShoreLinesOnVMap);
	bank.AddToggle("Display calming water", &CDebugScene::bDisplayCalmingWaterOnVMap);

	bank.AddToggle("Display targeting ranges", &CDebugScene::bDisplayTargetingRanges);
	bank.AddToggle("Display targeting cones", &CDebugScene::bDisplayTargetingCones);
	bank.AddToggle("Display targeting entities", &CDebugScene::bDisplayTargetingEntities);
}

void CVectorMap::InitClass()
{
	FastAssert(ms_Instance == NULL);
	ms_Instance = rage_new CVectorMap();
}

void CVectorMap::ShutdownClass()
{
	delete ms_Instance;
	ms_Instance = NULL;
}


#endif // __DEV
