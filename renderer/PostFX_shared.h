
#include "../../rage/base/src/grcore/config_switches.h"
#include "../../rage/base/src/grcore/effect_mrt_config.h"

#define DOF_TYPE_CHANGEABLE_IN_RAG			(1 && (__D3D11 || RSG_ORBIS))			//Allow all dof types selectable from RAG

#define AVG_LUMINANCE_COMPUTE		(1 && (RSG_PC && __D3D11))

#define DOF_COMPUTE_GAUSSIAN		((1 && __D3D11) || (1 && RSG_ORBIS) || DOF_TYPE_CHANGEABLE_IN_RAG)
#define DOF_DIFFUSION				((0 && __D3D11) || (1 && RSG_ORBIS) || DOF_TYPE_CHANGEABLE_IN_RAG)

#define BOKEH_SUPPORT				((1 && __D3D11) || (1 && RSG_ORBIS))
#define COC_SPREAD					(BOKEH_SUPPORT && (DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION))		//blur the near coc to give blurred near objects when against in focus background

#define DOF_MaxSampleRadius		15
#define DOF_ComputeGridSize		226		//this + 2*maxsampleradius wants to be less than 256 and multiple of 64

#define DOF_MaxSampleRadiusCOC	5
#define DOF_ComputeGridSizeCOC	246		//this + 2*maxsampleradius wants to be less than 256 and multiple of 64

#define ADAPTIVE_DOF (__D3D11 || RSG_ORBIS)

//Cpu version uses ray casts to calculate focal distance, doesnt work as well due to lack of collision geomtry in the distance + few other issues.
#define ADAPTIVE_DOF_CPU	 (RSG_PC && ADAPTIVE_DOF)
#define ADAPTIVE_DOF_GPU	 (1 && ADAPTIVE_DOF)

#define ADAPTIVE_DOF_GPU_HISTORY_SIZE		1
#define ADAPTIVE_DOF_OUTPUT_UAV				(1)
#define	ADAPTIVE_DOF_SMALL_FLOAT			(1.0e-6f)

#define FXAA_CUSTOM_TUNING (!__FINAL)

#define POSTFX_SEPARATE_AA_PASS					(1)
#define POSTFX_SEPARATE_LENS_DISTORTION_PASS	(1)

#define BOKEH_SORT_BITONIC_TRANSPOSE			1			//Allows more elements to be sorted, up to 1024*1024
#if BOKEH_SORT_BITONIC_TRANSPOSE
#define BOKEH_SORT_NUM_BUCKETS					1
#define BOKEH_SORT_BITONIC_BLOCK_SIZE			1024
#define BOKEH_SORT_TRANSPOSE_BLOCK_SIZE			16
#else
#define BOKEH_SORT_NUM_BUCKETS					8
#define BOKEH_SORT_BITONIC_BLOCK_SIZE			1024
#endif

#if defined(__SHADERMODEL)
struct AdaptiveDOFStruct
{
	float distanceToSubject;
	float maxBlurDiskRadiusNear;
	float maxBlurDiskRadiusFar;
	float springResult;
	float springVelocity;
};
#endif

