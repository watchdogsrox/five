/////////////////////////////////////////////////////////////////////////////////
// FILE :    vectormap.h
// PURPOSE : A debug map that can be viewed on top of the game
// AUTHOR :  Obbe, Adam Croston
// CREATED : 31/08/06
/////////////////////////////////////////////////////////////////////////////////
#ifndef VECTORMAP_H
#define VECTORMAP_H

// rage headers
#include "fwdebug/vectormap.h"
#include "vector/color32.h"
#include "vector/vector3.h"
namespace rage
{
	class bkBank;
	class Vector2;
};

#if __BANK
class CVectorMap : public rage::fwVectorMap
{
public:
	CVectorMap();
	~CVectorMap();

	virtual void AddGameSpecificWidgets(bkBank &bank);

	virtual void CleanSettingsCore();
	virtual void GoodSettingsCore();

	static void DrawVehicle(const class CVehicle *pVehicle, Color32 col2, bool bForceMap = true);
	static void DrawVehicleLodInfo(const class CVehicle *pVehicle, Color32 col2, float fScale, bool bForceMap = true);
	static void DrawPed(const class CEntity* pPed, Color32 col2, bool bForceMap = true);
	static void DrawLocalPlayerCamera(Color32 col, bool bDrawDof = false, bool bForceMap = true);

	static void MakeEventRipple(const Vector3& pos, float finalRadius, unsigned int lifetimeMs, const Color32 baseColour, bool bForceMap = true);

	static void InitClass();
	static void ShutdownClass();


protected:
	virtual void UpdateCore();
	virtual void DrawCore();

	virtual void UpdateFocus();
	virtual void UpdateRotation();

public:

	static bool m_bFocusOnPlayer;					// The vector map tracks the player
	static bool m_bFocusOnCamera;					// The vector map tracks the camera.
	static bool m_bRotateWithPlayer;				// The vector map rotates with the heading of the player (up is the same as the players heading)
	static bool m_bRotateWithCamera;				// The vector map rotates with the heading of the camera (up is the same as the cameras heading)
	static bool m_bDisplayDefaultPlayerIndicator;	// Whether or not to display an indicator for the player on the map (turns off for ped and vehicle display).
	static bool m_bDisplayLocalPlayerCamera;		// Whether or not to draw a representation of the local camera on the map.
	static float m_vehicleVectorMapScale;			// The scaling of vehs on the vector map.
	static float m_pedVectorMapScale;				// The scaling of peds on the vector map.
	static bool m_bDisplayDebugNameWithPed;			// Whether or not to draw the ped's debug name with the ped.
	static float m_waterLevel;						// The water level height to use on the contour map.
	static float m_fLastTanFOV;
};
#else //  __DEV
class CVectorMap : public rage::fwVectorMap
{
public:
	static __forceinline void DrawVehicle(class CVehicle *UNUSED_PARAM(pVehicle), Color32 UNUSED_PARAM(col2), bool UNUSED_PARAM(bForceMap) = true ) {};
	static __forceinline void DrawPed(class CEntity *UNUSED_PARAM(pPed), Color32 UNUSED_PARAM(col2), bool UNUSED_PARAM(bForceMap) = true ) {};
	static __forceinline void DrawLocalPlayerCamera(Color32 UNUSED_PARAM(col), bool UNUSED_PARAM(bDrawDof), bool UNUSED_PARAM(bForceMap) = true) {};

	static __forceinline void InitClass() {}
	static __forceinline void ShutdownClass() {}
};
#endif //  __DEV

#endif // VECTORMAP_H
