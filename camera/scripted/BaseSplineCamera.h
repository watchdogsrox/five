//
// camera/scripted/BaseSplineCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef SPLINE_CAMERA_H
#define SPLINE_CAMERA_H

#include "camera/scripted/ScriptedCamera.h"
#include "camera/helpers/SplineNode.h"
#include "script/script.h"

class camBaseSplineCameraMetadata;
class CEntity;

const u32 g_MaxSplineSpeedGraphMarkers = 20;
#if __BANK
const u32 g_NumSplineTestNodes = 5;
#endif // __BANK

#define SUPPORT_ZERO_DURATION_NODES		__DEV && 0
#if SUPPORT_ZERO_DURATION_NODES
#define SUPPORT_ZERO_DURATION_NODES_ONLY(x)	x
#else
#define SUPPORT_ZERO_DURATION_NODES_ONLY(x)
#endif

const camSplineNode::eFlags g_DefaultSplineNodeFlags	= (camSplineNode::eFlags)(camSplineNode::SHOULD_SMOOTH_ROTATION |
															  camSplineNode::SHOULD_SMOOTH_LENS_PARAMS);

//A camera that varies its position and orientation according to a spline function.
//NOTE: The spline camera uses a timed non-uniform spline, as this gives continuous acceleration in both translation and rotation, whilst allowing
//the transition time to each node to be specified.
//NOTE: Splining of orientation is performed by transforming the quaternion rotations into 4D space for conventional splining and then
//transforming back to the unit quaternion sphere.
class camBaseSplineCamera : public camScriptedCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseSplineCamera, camScriptedCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseSplineCamera(const camBaseObjectMetadata& metadata);

public:
	enum eSmoothingStyle
	{
		NO_SMOOTHING = 0,
		SLOW_IN,
		SLOW_OUT,
		SLOW_IN_OUT,
		VERY_SLOW_IN, 
		VERY_SLOW_OUT, 
		VERY_SLOW_IN_SLOW_OUT, 
		SLOW_IN_VERY_SLOW_OUT, 
		VERY_SLOW_IN_VERY_SLOW_OUT, 
		EASE_IN, 
		EASE_OUT, 
		QUADRATIC_EASE_IN, 
		QUADRATIC_EASE_OUT, 
		QUADRATIC_EASE_IN_OUT, 
		CUBIC_EASE_IN, 
		CUBIC_EASE_OUT, 
		CUBIC_EASE_IN_OUT, 
		QUARTIC_EASE_IN, 
		QUARTIC_EASE_OUT, 
		QUARTIC_EASE_IN_OUT, 
		QUINTIC_EASE_IN, 
		QUINTIC_EASE_OUT, 
		QUINTIC_EASE_IN_OUT, 
		CIRCULAR_EASE_IN, 
		CIRCULAR_EASE_OUT, 
		CIRCULAR_EASE_IN_OUT, 
		NUM_SMOOTHING_STYLES
	};

	~camBaseSplineCamera();

	bool		HasReachedEnd() const;

	void		SetShouldSelfDestructState(bool state)		{ m_ShouldSelfDestruct = state; }
	void		SetDuration(u32 duration)					{ m_Duration = duration; }
	virtual void  SetPhase(float progress);
	virtual float GetPhase(s32* time = NULL) const;
	u32			GetNodeIndex() const						{ return m_NodeIndex; }
	float		GetNodePhase() const						{ return m_NodePhase; }
	void		SetSmoothingStyle(eSmoothingStyle style)	{ m_SmoothingStyle = style; }

	void		AddNode(camBaseCamera* camera, u32 transitionTime = 1000, camSplineNode::eFlags flags = g_DefaultSplineNodeFlags);
	void		AddNode(const camBaseDirector& director, u32 transitionTime = 1000, camSplineNode::eFlags flags = g_DefaultSplineNodeFlags);
	void		AddNode(const camFrame& frame, u32 transitionTime = 1000, camSplineNode::eFlags flags = g_DefaultSplineNodeFlags);
	void		AddNode(const Vector3& position, const Vector3& lookAtPosition, u32 transitionTime = 1000,
						camSplineNode::eFlags flags = g_DefaultSplineNodeFlags);
	void		AddNodeWithOrientation(const Vector3& position, const Vector3& orientationEulers, u32 transitionTime = 1000,
						camSplineNode::eFlags flags = g_DefaultSplineNodeFlags, int rotationOrder = EULER_YXZ);

	s32			GetNumNodes() const							{ return m_Nodes.GetCount(); }
	void		ClearAllNodes();

	void		ForceLinearBlend(u32 nodeIndex, bool enable);
	void		ForceCut(u32 nodeIndex, bool enable);
	void		ForcePause(u32 nodeIndex, bool enable);
	void		ForceLevel(u32 nodeIndex, bool enable);
	void		SetEase(u32 nodeIndex, u32 easeFlag, float easeScale);
	void		SetVelocityScale(u32 nodeIndex, float scale);

	bool		IsPaused() const							{ return m_Paused; }

#if GTA_REPLAY
	void		SetCreatedForReplay()						{ m_CreatedForReplay = true; }
#endif // GTA_REPLAY


#if __BANK
	camSplineNode* DebugGetNode(u32 nodeIndex)				{ return m_Nodes[nodeIndex]; }
	void		DebugSetTotalDelta(float time)				{ m_TotalDelta = time; }
	void		DebugForceSplineRebuild()					{ m_ShouldBuildSpline = true; }

	static void	AddWidgets(bkBank &bank);

	virtual void DebugRender();

	virtual void DebugAppendDetailedText(char* string, u32 maxLength) const;
#endif // __BANK

	static void	ResetCustomSpeedGraph()						{ ms_NumSpeedGraphMarkers = 0; }
	static void	AddCustomSpeedGraphMarker(float marker);

	u32			GetCurrentNode() const;

	void SetOrientationBasedOnStartEndNodes(u32 nodeIndex, float percent);

	static float EaseIn(float phase, float scale = 1.0f);
	static float EaseOut(float phase, float scale = 1.0f);
	static float EaseInOut(float phase, float scale = 1.0f);

protected:
	virtual void ValidateSplineDuration() = 0; 
	virtual void UpdateTransitionDeltas();
	virtual void BuildTranslationSpline() = 0; 
	void		AdjustTangents();

	void ComputeNodeIndexAndNodePhase(float time, u32& nodeIndex, float& nodeTime) const;
	Vector3 ComputePosition(u32 nodeIndex, float nodeTime);
	virtual bool Update();

	Vector3		ComputeStartTranslationVelocity(int nodeIndex);
	Vector3		ComputeEndTranslationVelocity(int nodeIndex);

	virtual void AddNodeInternal(camSplineNode* node, u32 transitionTime); 

	s32			GetCurrentTimeMSec() const;

	void		UpdateSplines();

	void		ComputePhaseNodeIndexAndNodePhase(s32 time, float& phase, u32& nodeIndex, float& nodePhase) const;
	float		ComputeCustomSpeedGraphFunction(float x) const;
	float		ApplySpeedGraph(float x) const;

	virtual Quaternion	ComputeOrientation(u32 nodeIndex, float nodeTime);
	Vector4		ComputePositionOnCubic(const Vector4 &startPos, const Vector4 &startVel, const Vector4 &endPos, const Vector4 &endVel, float time);

	void		LerpLensParameters(u32 nodeIndex, float nodeTime);

	float		CalculateNodeLength(const camSplineNode* node) const;

	void		TestForPause(u32 previousNodeIndex, float previousNodePhase);
#if __BANK
	static bool  ms_ShouldDebugRenderSplines;
	static bool  ms_ShouldDebugRenderTangents;
	static bool  ms_PauseGameForNewNode;
#endif // __BANK

private:
#if __BANK
	static void OnRenderSplineChange();
	static void OnSplineTogglePause();
	static void OnSplineAdvance();
#endif // __BANK

private:
	static float	ms_SpeedGraphMarkers[g_MaxSplineSpeedGraphMarkers];	//Used to create a custom speed control.
	static u32		ms_NumSpeedGraphMarkers; 

	const camBaseSplineCameraMetadata& m_Metadata;
	eSmoothingStyle	m_SmoothingStyle;
	
protected:
	u32				m_NodeIndex;
	u32				m_Duration;
	s32				m_StartTime;
	float			m_TotalDelta;					//Named "Delta" as this value could represent a distance or time, depending on the spline type.
	float			m_NodePhase;					//Allow script to query.
	bool			m_ShouldSelfDestruct	: 1;	//Should this camera delete itself once the spline has completed?
	bool			m_ShouldBuildSpline		: 1;	//Should (re)build the spline before use on the next update.
	bool			m_ShouldLerp			: 1;	//Should simply LERP the positions, rather than splining them.
	bool			m_Paused				: 1;	//Camera is paused before next node waiting for node flag to be cleared.
#if GTA_REPLAY
	bool			m_CreatedForReplay		: 1;
#endif // GTA_REPLAY
#if __BANK
	bool			m_WasScriptedCamInterpolating : 1;
#endif // __BANK
	atArray<camSplineNode*> m_Nodes;

private:
	//Forbid copy construction and assignment.
	camBaseSplineCamera(const camBaseSplineCamera& other);
	camBaseSplineCamera& operator=(const camBaseSplineCamera& other);
};

#endif // SPLINE_CAMERA_H
