
#include "LinearPiecewiseFog.h"

#if LINEAR_PIECEWISE_FOG

#include "parser/manager.h"
#include "LinearPiecewiseFog_parser.h"

///////////////////////////////////////////////////////////////////////////////
// Global data.										
///////////////////////////////////////////////////////////////////////////////

#if __BANK
#include "rmptfx/ptxkeyframe.h"
// RAG key frames etc.
LinearPiecewiseFog::FogKeyframe LinearPiecewiseFog::sm_Keyframes[2];
float LinearPiecewiseFog::sm_Interp = 0.0f;

bool LinearPiecewiseFog::sm_DoLinearPiecewiseFog = false;
bool LinearPiecewiseFog::sm_UseOverrideHeightsAndVisibilites = false;

ptxKeyframe LinearPiecewiseFog::sm_ProfileVisualise;

#endif //__BANK

///////////////////////////////////////////////////////////////////////////////
// LinearPiecewiseFogShape.
///////////////////////////////////////////////////////////////////////////////

#define LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY 5.0f // 10^5 = 100000 mtrs.
#define LINEAR_PIECEWISE_FOG_WIDGET_MIN_VISIBILITY 0.0f // 10^0 = 1mtr.

#define LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS	0

#if LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
// Defined over 10^LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT, normalized into [0, 1] range then scaled over shape range.
#define LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT	5.0f // 10^5 = 100000 mtrs.
#define LINEAR_PIECEWISE_FOG_WIDGET_MIN_HEIGHT	0.0f // 10^0 = 1mtr.
#define LINEAR_PIECEWISE_FOG_MAX_HEIGHT			powf(10, LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT)

#else // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
#define LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT	1.0f // Just scaled over shape range.
#define LINEAR_PIECEWISE_FOG_WIDGET_MIN_HEIGHT	0.0f
#define LINEAR_PIECEWISE_FOG_MAX_HEIGHT			10000.0f
#endif // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS


LinearPiecewiseFogShape::LinearPiecewiseFogShape()
{
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		float k = (float)i/(float)(LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS - 1);
		float t = k*LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT;

		// Add some alpha values which the key-frame optimizer cannot reduce.
		m_ControlPoints[i].heightValue = t;
		m_ControlPoints[i].visibilityScale = expf(-k);
	}
}


LinearPiecewiseFogShape::~LinearPiecewiseFogShape()
{

}


float LinearPiecewiseFogShape::GetHeightAtControlPoint(int i, float rangeMin, float rangeMax)
{
#if LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
	// Height is on a logarithmic scale of base 10.
	return powf(10, m_ControlPoints[i].heightValue - LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT)*(rangeMax - rangeMin) + rangeMin;
#else // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
	return m_ControlPoints[i].heightValue*(rangeMax - rangeMin) + rangeMin;
#endif // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
}


float LinearPiecewiseFogShape::GetVisibilityAtControlPoint(int i, float log10MinVisibilty)
{
	float v = m_ControlPoints[i].visibilityScale;
	v = (log10MinVisibilty - LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY)*v + LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY;

	// Make the RAG widget act like a logarithmic scale, base 10.
	float visibility = powf(10, v);

	return visibility;
}

#if __BANK
void LinearPiecewiseFogShape::AddWidgets(bkBank &rBank, int i)
{
	char groupName[256];
	sprintf(groupName, "Fog shape %d", i);
	rBank.PushGroup(groupName, false);

	rBank.AddTitle("------------------------ Fog thickness ------------------------");
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		char str[256];
		sprintf(str, "%d", i);
		rBank.AddSlider(str, &m_ControlPoints[i].visibilityScale, 0.0f, 1.0f, 0.001f);
	}

	rBank.AddTitle("------------------------ Heights ------------------------");
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		char str[256];
	#if LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
		sprintf(str, "%d (log10)", i);
	#else // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
		sprintf(str, "%d", i);
	#endif // LINEAR_PIECEWISE_FOG_USE_LOG10_HEIGHTS
		rBank.AddSlider(str, &m_ControlPoints[i].heightValue, 0.0f, LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT, 0.001f);
	}

	rBank.AddButton("Visualize", datCallback(CFA2(LinearPiecewiseFogShape::OnVisualise), CallbackData((void *)this)));

	rBank.PopGroup();
}

void LinearPiecewiseFogShape::OnVisualise(CallbackData cbData)
{
	LinearPiecewiseFogShape *pThis = (LinearPiecewiseFogShape *)cbData;
	
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		Vec4V v = Vec4V(1.0f, 1.0f, 1.0f, pThis->m_ControlPoints[i].visibilityScale);
		ScalarV t = ScalarV(pThis->m_ControlPoints[i].heightValue);
		if(i==0)
			LinearPiecewiseFog::sm_ProfileVisualise.Init(t, v);
		else
			LinearPiecewiseFog::sm_ProfileVisualise.AddEntry(t, v);
	}
	LinearPiecewiseFog::sm_ProfileVisualise.UpdateWidget();
}

#endif //__BANK

/*---------------------------------------------------------------------------------------------------------------------*/

///////////////////////////////////////////////////////////////////////////////
// LinearPiecewiseFog
///////////////////////////////////////////////////////////////////////////////

LinearPiecewiseFog::FogKeyframe::FogKeyframe()
{
	m_minHeight = 0.0f;
	m_maxHeight = LINEAR_PIECEWISE_FOG_MAX_HEIGHT;
	m_log10MinVisibility = LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY;
	m_shapeWeights = Vec4V(1.0f,  0.0f, 0.0f, 0.0f);
}

// Palette of fog shapes.
LinearPiecewise_FogShapePalette LinearPiecewiseFog::sm_ShapePalette;

void LinearPiecewiseFog::Init()
{
	LoadShapes();
}


void LinearPiecewiseFog::CleanUp()
{

}


void LinearPiecewiseFog::ComputeShaderParams(LinearPiecewiseFog_ShaderParams &shaderParams, float cameraHeight, FogKeyframe timeCycleKeyframe)
{
	static float s_MaxVisibility = powf(10, LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY);

	FogKeyframe keyframeToUse = timeCycleKeyframe;
	HeightAndVisibility sortedByHeight[LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS];

#if __BANK
	if(sm_UseOverrideHeightsAndVisibilites)
	{
		// Interpolate between key-frames to simulate time-cycle.
		keyframeToUse.m_minHeight = (1.0f - sm_Interp)*sm_Keyframes[0].m_minHeight + sm_Interp*sm_Keyframes[1].m_minHeight;
		keyframeToUse.m_maxHeight = (1.0f - sm_Interp)*sm_Keyframes[0].m_maxHeight + sm_Interp*sm_Keyframes[1].m_maxHeight;
		keyframeToUse.m_log10MinVisibility = (1.0f - sm_Interp)*sm_Keyframes[0].m_log10MinVisibility + sm_Interp*sm_Keyframes[1].m_log10MinVisibility;
		keyframeToUse.m_shapeWeights = ScalarV(1.0f - sm_Interp)*sm_Keyframes[0].m_shapeWeights + ScalarV(sm_Interp)*sm_Keyframes[1].m_shapeWeights;
	}
#endif //__BANK

	HeightAndVisibility entry;
	entry.height = 0.0f;
	entry.visibility = 0.0f;
			 
	// Reset to a zero profile.
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
		sortedByHeight[i] = entry;

	// Create the fog shape from the keyframe weights.
	LinearPiecewiseFogShape finalShape;



	float modWeightsSqr = Dot(keyframeToUse.m_shapeWeights, keyframeToUse.m_shapeWeights).Getf(); 

	// Create a valid weight if neds be.
	if(modWeightsSqr < 0.001f)
	{
		Assertf(1, "LinearPiecewiseFog::ComputeShaderParams()...Shape weight is zeros");
		keyframeToUse.m_shapeWeights = Vec4V(V_ZERO_WONE);
	}

	float weights[4];
	weights[0] = keyframeToUse.m_shapeWeights.GetXf();
	weights[1] = keyframeToUse.m_shapeWeights.GetYf();
	weights[2] = keyframeToUse.m_shapeWeights.GetZf();
	weights[3] = keyframeToUse.m_shapeWeights.GetWf();

	float denominator = 1.0f/(weights[0] + weights[1] + weights[2] + weights[3]);

	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		finalShape.m_ControlPoints[i].heightValue = denominator*(weights[0]*sm_ShapePalette.m_Shapes[0].m_ControlPoints[i].heightValue +
			weights[1]*sm_ShapePalette.m_Shapes[1].m_ControlPoints[i].heightValue +
			weights[2]*sm_ShapePalette.m_Shapes[2].m_ControlPoints[i].heightValue +
			weights[3]*sm_ShapePalette.m_Shapes[3].m_ControlPoints[i].heightValue);

		finalShape.m_ControlPoints[i].visibilityScale = denominator*(weights[0]*sm_ShapePalette.m_Shapes[0].m_ControlPoints[i].visibilityScale +
			weights[1]*sm_ShapePalette.m_Shapes[1].m_ControlPoints[i].visibilityScale +
			weights[2]*sm_ShapePalette.m_Shapes[2].m_ControlPoints[i].visibilityScale +
			weights[3]*sm_ShapePalette.m_Shapes[3].m_ControlPoints[i].visibilityScale);
	}

	// Calculate each control point from the final fog shape.
	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS; i++)
	{
		// Compute the two shapes of the fog stretched over the height range provided by the time cycle.
		float height = finalShape.GetHeightAtControlPoint(i, keyframeToUse.m_minHeight, keyframeToUse.m_maxHeight);
		float visibility = finalShape.GetVisibilityAtControlPoint(i, keyframeToUse.m_log10MinVisibility);

		// Make max. visibility fully transparent.
		if(s_MaxVisibility - visibility < 5.0f)
			visibility = 0.0f;

		// Interpolate between the shapes to form a final height and visibility.
		entry.height = height;
		entry.visibility = visibility;
		sortedByHeight[i] = entry;
	}

	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES; i++)
		shaderParams.shaderParams[i] = Vec4V(V_ZERO);

	bool doFogging = false;

#if __BANK
	doFogging = sm_DoLinearPiecewiseFog;
#endif // __BANK

	if(doFogging)
	{
		int outIdx =0;
		float densityAtViewer = 0.0f;

		for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS-1; i++)
		{
			// Does i and i + 1 form a valid range AND has fog defined (visibility = 0 means no fog).
			if((sortedByHeight[i].height < sortedByHeight[i+1].height) && ((sortedByHeight[i].visibility != 0.0f) || (sortedByHeight[i+1].visibility != 0.0f)))
			{
				float d1 = (sortedByHeight[i].visibility != 0.0f) ? (1.0f/sortedByHeight[i].visibility) : 0.0f;
				float d2 = (sortedByHeight[i+1].visibility != 0.0f) ? (1.0f/sortedByHeight[i+1].visibility) : 0.0f;
				float h1 = sortedByHeight[i].height;
				float h2 = sortedByHeight[i+1].height;

				// Density is a linear function of height : Ax + B.
				float A = (d2 - d1)/(h2 - h1);
				float B = (h2*d1 - h1*d2)/(h2 - h1);

				// Store heights and A, B.
				shaderParams.shaderParams[1 + outIdx++] = Vec4V(sortedByHeight[i].height, sortedByHeight[i+1].height, A, B);

				// Is the camera inside this band?
				if((cameraHeight >= h1) && (cameraHeight <= h2))
				{
					// Sum the densities in case we have over-lapping bands.
					densityAtViewer += A*cameraHeight + B;
				}
			}
		}
		// Fill out num of pieces, density at viewer, camera height and Debug on/off switch.
		shaderParams.shaderParams[0] = Vec4V((float)outIdx, densityAtViewer, cameraHeight, 1.0f);
	}
}


#if __BANK
void LinearPiecewiseFog::AddWidgets(bkBank &rBank)
{
	rBank.PushGroup("Linear Piecewise Fog.");

	rBank.AddToggle("On/Off", &sm_DoLinearPiecewiseFog);
	rBank.AddToggle("Override time cycle.", &sm_UseOverrideHeightsAndVisibilites);
	rBank.AddButton("Save shapes", LinearPiecewiseFog::SaveShapes);
	rBank.AddButton("Load shapes", LinearPiecewiseFog::LoadShapes);

	rBank.AddTitle("Simulate TC interpolation");
	rBank.AddSlider("interpolation", &sm_Interp, 0.0f, 1.0f, 0.0010f);

	rBank.AddTitle("Previous TC frame");
	rBank.AddSlider("Previous TC min height",		&sm_Keyframes[0].m_minHeight, 0.0f, LINEAR_PIECEWISE_FOG_MAX_HEIGHT, 0.5f);
	rBank.AddSlider("Previous TC max height",		&sm_Keyframes[0].m_maxHeight, 0.0f, LINEAR_PIECEWISE_FOG_MAX_HEIGHT, 0.5f);
	rBank.AddSlider("Previous visibility (log10)",	&sm_Keyframes[0].m_log10MinVisibility, LINEAR_PIECEWISE_FOG_WIDGET_MIN_VISIBILITY, LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY, 0.01f);
	rBank.AddVector("shape weights",				&sm_Keyframes[0].m_shapeWeights, 0.0f, 1.0f, 0.001f);

	rBank.AddTitle("Next TC frame");
	rBank.AddSlider("Next TC min height",			&sm_Keyframes[1].m_minHeight, 0.0f, LINEAR_PIECEWISE_FOG_MAX_HEIGHT, 0.5f);
	rBank.AddSlider("Next TC max height",			&sm_Keyframes[1].m_maxHeight, 0.0f, LINEAR_PIECEWISE_FOG_MAX_HEIGHT, 0.5f);
	rBank.AddSlider("Next visibility (log10)",		&sm_Keyframes[1].m_log10MinVisibility, LINEAR_PIECEWISE_FOG_WIDGET_MIN_VISIBILITY, LINEAR_PIECEWISE_FOG_WIDGET_MAX_VISIBILITY, 0.01f);
	rBank.AddVector("shape weights",				&sm_Keyframes[1].m_shapeWeights, 0.0f, 1.0f, 0.001f);

	/*----------------------------------------------------------------------------------------------------------*/

	static const float sMin = 0.0f;
	static const float sMax = LINEAR_PIECEWISE_FOG_WIDGET_MAX_HEIGHT;
	static const float sDelta = 1.0f;

	Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
	Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);
	Vec4V vDefaultValue = Vec4V(V_ZERO);

	static ptxKeyframeDefn s_WidgetProfile("Visualise", 0, PTXKEYFRAMETYPE_FLOAT4, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", "Alpha", true);

	sm_ProfileVisualise.SetDefn(&s_WidgetProfile);
	ScalarV t = ScalarV(0.0f);
	Vec4V v = Vec4V(1.0f, 1.0f, 1.0f, 0.0f);
	sm_ProfileVisualise.AddEntry(t, v);
	sm_ProfileVisualise.AddWidgets(rBank);

	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_FOG_SHAPES; i++)
		sm_ShapePalette.m_Shapes[i].AddWidgets(rBank, i);

	rBank.PopGroup();
}


void LinearPiecewiseFog::SaveShapes()
{
	PARSER.SaveObject("common:/data/FogShapes", "xml", &LinearPiecewiseFog::sm_ShapePalette, parManager::XML);
}

#endif //__BANK

void LinearPiecewiseFog::LoadShapes()
{
	PARSER.LoadObject("common:/data/FogShapes", "xml", LinearPiecewiseFog::sm_ShapePalette);
}


#endif // LINEAR_PIECEWISE_FOG
