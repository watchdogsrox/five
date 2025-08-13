//
// game/renderer/HorizonObjects.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_HORIZON_OBJECTS
#define INC_HORIZON_OBJECTS

#include "fwscene/stores/dwdstore.h" // For loading model

namespace rage
{
	class grmShader;

#if __BANK
	class bkBank;
#endif // __BANK
}
class gtaDrawable;

class CHorizonObjects
{
public:
	CHorizonObjects();
	~CHorizonObjects();

	/**
	 * Initialize the material and horizon meshes.
	 */
	void Init(unsigned initMode);

	void Shutdown(unsigned shutdownMode);

	static void Render();
	static void RenderEx(float depthRange, bool renderWater, bool waterReflections, int scissorX, int scissorY, int scissorW, int scissorH);

	/**
	 * Used by script for culling all the horizon objects.
	 */
	static void CullEverything(bool bEnabled);

#if __BANK
	/**
	 * Initialize the BANK widgets.
	 */
	void InitWidgets(bkBank* pBank);

	/**
	 * Clamp the drawable selection to the range of m_arrDrawables.
	 */
	void ClampSelection();
#endif // __BANK

private:

	void RenderInternal(float depthRange, bool renderWater, bool waterReflections, int scissorX, int scissorY, int scissorW, int scissorH);

	static s32 Compare(const int* a, const int* b);

	/**
	 * System enable / disable.
	 */
	bool m_bEnable;

	/**
	 * Cull all horizon objects.
	 * Used by script.
	 */
	bool m_bCullEverything;

	/**
	 * Shader used for rendering the horizon objects.
	 */
	grmShader* m_pSilhoutteShader;

	/**
	 * Technique to render the horizon objects.
	 */
	grcEffectTechnique m_SilhoutteDrawTech;

	/**
	 * Technique to render the horizon objects into the water reflections.
	 */
	grcEffectTechnique m_SilhoutteDrawWaterReflectionTech;

	/**
	 * Model dictionary.
	 */
	Dwd* m_pHorizonObjectsDictionary;

	/**
	 * Array with all the horizon object drawables.
	 */
	atArray<gtaDrawable*> m_arrDrawables;

	/**
	 * Array with the indices to the visible models.
	 */
	atArray<int> m_arrVisible;

	/**
	 * Scale factor for the models.
	 */
	float m_fScaleFactor;

	/**
	 * Depth stencil state for the horizon objects.
	 */
	grcDepthStencilStateHandle m_DSS_Scene;
	grcDepthStencilStateHandle m_DSS_Water;

	/**
	 * Percentage of the depth range used for rendering the horizon objects.
	 * The lower the value, the closer the front plane is going to get pushed towards the far plane.
	 * This works as a clip plane to get rid of the geometry overlapping with the high LOD.
	 */
	bank_float m_fDepthRangePercentage;

	/**
	 * Max far plane distance.
	 */
	bank_float m_fFarPlane;

#if __BANK
	/**
	 * Draw the horizon objects bounding box.
	 */
	bool m_bDrawBounds;

	/**
	 * Skip the drawable AABB test.
	 */
	bool m_bSkipAABBTest;

	/**
	 * Draw the horizon objects with the debug shader.
	 */
	bool m_bDebugShader;

	/**
	 * Total horizon objects.
	 */
	int m_iTotalObjects;

	/**
	 * Total visible horizon objects.
	 */
	int m_iTotalVisibleObjects;

	/**
	 * Index of the selected horizon object (-1 for non-selected).
	 */
	int m_iObjectSelection;

	/**
	 * Total memory used by the models.
	 */
	u32 m_MemUsage;

	/**
	 * Skip searching for the closest far plane that contains all the horizon objects.
	 */
	bool m_bSkipFarDistanceSearch;

	/**
	 * sort solid FLODS
	 */
	bool m_bSortMapObjects;
	
	/**
	 * skip solid FLODS
	 */
	bool m_bSkipMapObjects;
	
	/**
	 * skip water FLODS
	 */
	bool m_bSkipWaterObjects;
	

#if __PS3
	/**
	 * Disable EDGE culling.
	 */
	bool m_bDisableEdgeCulling;
#endif // __PS3

#endif // __BANK
};

extern CHorizonObjects g_HorizonObjects;

#endif // INC_HORIZON_OBJECTS
