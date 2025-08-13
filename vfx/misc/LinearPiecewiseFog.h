#ifndef _LINEAR_PIECEWISE_FOG_H
#define _LINEAR_PIECEWISE_FOG_H

#include "vfx/vfx_shared.h"
#include "bank/bank.h"
#include "vectormath/vec4v.h"
#include "parser/macros.h"

#if LINEAR_PIECEWISE_FOG
namespace rage { class ptxKeyframe; }

class LinearPiecewiseFog_ShaderParams
{
public:
	LinearPiecewiseFog_ShaderParams()
	{
		memset(shaderParams, 0, sizeof(shaderParams));
	}
	~LinearPiecewiseFog_ShaderParams()
	{

	}
private:
	Vec4V shaderParams[LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES];

	friend class CShaderLib;
	friend class LinearPiecewiseFog;
};


class LinearPiecewiseFogShape_ControlPoint
{
public:
	LinearPiecewiseFogShape_ControlPoint() { heightValue = 0.0f; visibilityScale = 0.0f; }
	~LinearPiecewiseFogShape_ControlPoint() {}
	float heightValue;
	float visibilityScale;
	PAR_SIMPLE_PARSABLE;
};


class LinearPiecewiseFogShape
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public:
	LinearPiecewiseFogShape();
	~LinearPiecewiseFogShape();

public:
	float GetHeightAtControlPoint(int i, float rangeMin, float rangeMax);
	float GetVisibilityAtControlPoint(int i, float log10MinVisibilty);
#if __BANK
	void AddWidgets(bkBank &rBank, int i);
	static void OnVisualise(CallbackData cbData);
#endif //__BANK

private:
	LinearPiecewiseFogShape_ControlPoint m_ControlPoints[LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS];
	PAR_SIMPLE_PARSABLE;

	friend class LinearPiecewiseFog;
};


class LinearPiecewise_FogShapePalette
{
public:
	LinearPiecewise_FogShapePalette() {}
	~LinearPiecewise_FogShapePalette() {}
public:
	LinearPiecewiseFogShape m_Shapes[LINEAR_PIECEWISE_FOG_NUM_FOG_SHAPES];
	PAR_SIMPLE_PARSABLE;
};


class LinearPiecewiseFog
{
	///////////////////////////////////
	// INTERNAL STUFF
	///////////////////////////////////

public:
	struct FogKeyframe
	{
	public:
		FogKeyframe();
		// In metres.
		float m_minHeight;
		float m_maxHeight;
		// Log10(visibility).
		float m_log10MinVisibility;
		// Shape weighting.
		Vec4V m_shapeWeights;
	};

private:  //////////////////////////
	struct HeightAndVisibility
	{
		float height;
		float visibility;
	};

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public: //////////////////////////
	static void Init();
	static void CleanUp();
public:
	static void ComputeShaderParams(LinearPiecewiseFog_ShaderParams &shaderParams, float cameraHeight, FogKeyframe timeCycleKeyframe);
#if __BANK
	static void AddWidgets(bkBank &rBank);
	static void SaveShapes();
#endif //__BANK
	static void LoadShapes();

	///////////////////////////////////
	// DATA MEMBERS
	///////////////////////////////////

private: //////////////////////////
	// Palette of fog shapes.
	static LinearPiecewise_FogShapePalette sm_ShapePalette;

#if __BANK
	static bool sm_DoLinearPiecewiseFog;
	static bool sm_UseOverrideHeightsAndVisibilites;

	// RAG key frames etc.
	static FogKeyframe sm_Keyframes[2];
	static float sm_Interp;

	static ptxKeyframe sm_ProfileVisualise;
#endif // __BANK

	friend class LinearPiecewiseFogShape;
};

#endif // LINEAR_PIECEWISE_FOG

#endif //_LINEAR_PIECEWISE_FOG_H
