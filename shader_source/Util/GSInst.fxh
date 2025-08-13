/*
 * GSInst.fxh
 *
 * geometry shader pass-through function for geometry shader instancing
 * 
 * If there is a need to gs-instance something, shader that needs instancing has to include this fxh
 * 
 */
//#define NO_PS_NEEDED	!CASCADE_SHADOWS_ENTITY_ID_TARGET	// cascade shadow doesn't need PS

#if (GS_INSTANCED_CUBEMAP || GS_INSTANCED_SHADOWS)
// ----------------------------------------------------------------------------------------------- //
//	GS instancing passthrough function
#define GEN_GSINST_TYPE(TYPE,VS_OUT_TYPE,SV_OUTPUT_TYPE) \
\
struct JOIN3(VS_OUT_TYPE,TYPE,GSInst) \
{ \
	VS_OUT_TYPE	 OutputGS; \
	int InstID : TEXCOORD15; \
}; \
struct JOIN3(GS_,TYPE,GSInstOUT) \
{ \
	VS_OUT_TYPE OutputGS; \
	uint s_index : SV_OUTPUT_TYPE; \
}; \
\

#define GEN_GSINST_FUNC_NOPS(TYPE, FUNCNAME, VS_IN_TYPE, VS_OUT_TYPE, PS_IN_TYPE, VS_COMMON_FUNC) \
\
JOIN3(VS_OUT_TYPE,TYPE,GSInst) JOIN3(VS_,FUNCNAME,TransformGSInst)(VS_IN_TYPE IN) \
{ \
	JOIN3(VS_OUT_TYPE,TYPE,GSInst) OUT; \
	OUT.OutputGS = VS_COMMON_FUNC; \
	OUT.InstID = INSTOPT_INDEX(IN.InstID); \
	return OUT; \
} \
[maxvertexcount(3)] \
void JOIN3(GS_,FUNCNAME,GSInstPassThrough)(triangle JOIN3(VS_OUT_TYPE,TYPE,GSInst) input[3], inout TriangleStream<JOIN3(GS_,TYPE,GSInstOUT)> OutputStream) \
{ \
	JOIN3(GS_,TYPE,GSInstOUT) output; \
	for(int i = 0; i < 3; i++) \
	{ \
		output.OutputGS = input[i].OutputGS; \
		output.s_index = input[i].InstID.x; \
		OutputStream.Append(output); \
	} \
	OutputStream.RestartStrip(); \
} \
\


#define GEN_GSINST_FUNC(TYPE, FUNCNAME, VS_IN_TYPE, VS_OUT_TYPE, PS_IN_TYPE, VS_COMMON_FUNC, PS_COMMON_FUNC) \
\
JOIN3(VS_OUT_TYPE,TYPE,GSInst) JOIN3(VS_,FUNCNAME,TransformGSInst)(VS_IN_TYPE IN) \
{ \
	JOIN3(VS_OUT_TYPE,TYPE,GSInst) OUT; \
	OUT.OutputGS = VS_COMMON_FUNC; \
	OUT.InstID = INSTOPT_INDEX(IN.InstID); \
	return OUT; \
} \
[maxvertexcount(3)] \
void JOIN3(GS_,FUNCNAME,GSInstPassThrough)(triangle JOIN3(VS_OUT_TYPE,TYPE,GSInst) input[3], inout TriangleStream<JOIN3(GS_,TYPE,GSInstOUT)> OutputStream) \
{ \
	JOIN3(GS_,TYPE,GSInstOUT) output; \
	for(int i = 0; i < 3; i++) \
	{ \
		output.OutputGS = input[i].OutputGS; \
		output.s_index = input[i].InstID.x; \
		OutputStream.Append(output); \
	} \
	OutputStream.RestartStrip(); \
} \
OutHalf4Color JOIN3(PS_,FUNCNAME,TexturedGSInst)(PS_IN_TYPE IN) \
{ \
	return PS_COMMON_FUNC; \
} \
\

#define GEN_GSINST_FUNC_INSTANCEDPARAM(TYPE, FUNCNAME, VS_IN_TYPE, VS_OUT_TYPE, PS_IN_TYPE, VS_COMMON_FUNC, PS_COMMON_FUNC) \
\
JOIN3(VS_OUT_TYPE,TYPE,GSInst) JOIN3(VS_,FUNCNAME,TransformGSInst)(VS_IN_TYPE IN) \
{ \
	JOIN3(VS_OUT_TYPE,TYPE,GSInst) OUT; \
	OUT.OutputGS = VS_COMMON_FUNC; \
	OUT.InstID = INSTOPT_INDEX(IN.baseVertIn.InstID); \
	return OUT; \
} \
[maxvertexcount(3)] \
void JOIN3(GS_,FUNCNAME,GSInstPassThrough)(triangle JOIN3(VS_OUT_TYPE,TYPE,GSInst) input[3], inout TriangleStream<JOIN3(GS_,TYPE,GSInstOUT)> OutputStream) \
{ \
	JOIN3(GS_,TYPE,GSInstOUT) output; \
	for(int i = 0; i < 3; i++) \
	{ \
		output.OutputGS = input[i].OutputGS; \
		output.s_index = input[i].InstID.x; \
		OutputStream.Append(output); \
	} \
	OutputStream.RestartStrip(); \
} \
OutHalf4Color JOIN3(PS_,FUNCNAME,TexturedGSInst)(PS_IN_TYPE IN) \
{ \
	return PS_COMMON_FUNC; \
} \
\


#define GEN_GSINST_TECHPASS_NOPS(FUNCNAME, PASSNAME) \
\
pass JOIN(p_, PASSNAME) \
{ \
	VertexShader = compile VSGS_SHADER	JOIN3(VS_,FUNCNAME,TransformGSInst)(); \
	SetGeometryShader(compileshader(gs_5_0, JOIN3(GS_,FUNCNAME,GSInstPassThrough)())); \
} \
\

#define GEN_GSINST_TECHPASS(FUNCNAME, PASSNAME) \
\
pass JOIN(p_, PASSNAME) \
{ \
	VertexShader = compile VSGS_SHADER	JOIN3(VS_,FUNCNAME,TransformGSInst)(); \
	SetGeometryShader(compileshader(gs_5_0, JOIN3(GS_,FUNCNAME,GSInstPassThrough)())); \
	PixelShader  = compile PIXELSHADER	JOIN3(PS_,FUNCNAME,TexturedGSInst)()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
\

#define GEN_GSINST_NOTECHPASS(FUNCNAME) \
\
VertexShader = compile VSGS_SHADER	JOIN3(VS_,FUNCNAME,TransformGSInst)(); \
SetGeometryShader(compileshader(gs_5_0, JOIN3(GS_,FUNCNAME,GSInstPassThrough)())); \
PixelShader  = compile PIXELSHADER	JOIN3(PS_,FUNCNAME,TexturedGSInst)()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
\

#else
#define GEN_GSINST_TYPE(TYPE,VS_OUT_TYPE,SV_OUTPUT_TYPE)
#define GEN_GSINST_FUNC(TYPE, FUNCNAME, VS_IN_TYPE, VS_OUT_TYPE, PS_IN_TYPE, VS_COMMON_FUNC, PS_COMMON_FUNC) 
#define GEN_GSINST_TECHPASS(FUNCNAME, PASSNAME) 
#endif
