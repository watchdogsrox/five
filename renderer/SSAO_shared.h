//
// renderer/SSAO_shared.h
//
// Copyright (C) 2013 Rockstar Games.  All Rights Reserved.
//

#ifndef RENDERER_SSAO_SHARED_H
#define RENDERER_SSAO_SHARED_H

//on Orbis R32F does not have enough precision, using 32bit depth instead
#define SSAO_OUTPUT_DEPTH			(RSG_ORBIS || __D3D11)

#define HDAO2_MULTISAMPLE_FILTER		(0)

// Group Defines
// 32 * 32 = 1024 threads
#define HDAO2_GROUP_THREAD_DIM			32
#define HDAO2_GROUP_TEXEL_DIM			56
#define HDAO2_GROUP_TEXEL_OVERLAP		12
//HDAO2_GROUP_TEXEL_DIM = HDAO2_GROUP_THREAD_DIM + 2*HDAO2_GROUP_TEXEL_OVERLAP

// Texture Op Defines
#define HDAO2_GATHER_THREADS			784
#define HDAO2_GATHER_THREADS_PER_ROW	28

// Filter Defines
#define HDAO2_NUM_THREADS               32
#define HDAO2_RUN_LINES                 2
#define HDAO2_RUN_SIZE                  128

// Packs normals into a float. Doesn`t appear to work on Orbis (maybe for the same reason cited in SSAO_OUTPUT_DEPTH).
#define MR_SSAO_USE_PACKED_NORMALS		0

#define MR_SSAO_FULL_KERNEL				0 // Usual full kernel.
#define MR_SSAO_STIPPLED_KERNEL			1 // Alternate pixels.
#define MR_SSAO_STAR_KERNEL				2 // Horizontal/vertical + 45 degree crosses.
#define MR_SSAO_KERNEL_MAX				3

// Uses full screen linear depth for CP/QS mix.
// (TODO:- PC doesn`t like this and banding/badness takes place. Might be MSAA related or 0.5 pixel offset - See PS_LinearizeDepthCommon() in SSAO.fx).
#define SSAO_USE_LINEAR_DEPTH_TARGETS	(1 && ((1 && RSG_ORBIS) || (0 && RSG_PC) || (1 && RSG_DURANGO)))
// Makes 1/4 linear buffer from the full screen linear one.
#define SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR (1 && SSAO_USE_LINEAR_DEPTH_TARGETS)

// Uses ESRAM for targets.
#define	SSAO_USE_ESRAM_TARGETS			(1)

#define SUPPORT_HBAO					(1 && ((1 && RSG_ORBIS) || (1 && RSG_PC) || (1 && RSG_DURANGO)))

#endif	//RENDERER_SSAO_SHARED_H
