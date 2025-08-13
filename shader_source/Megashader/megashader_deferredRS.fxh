//
// deferred renderstate header - see megashader.fxh how it's used:
//
//



#ifdef DECAL_SHADER

	AlphaBlendEnable = true;
	
	#if EMISSIVE_ADDITIVE
		BlendOp   = ADD;
		SrcBlend  = ONE;
		DestBlend = ONE;
	#elif defined(DECAL_SHADOW_ONLY)
		BlendOp          = ADD;
		SrcBlend         = INVSRCALPHA;
		DestBlend        = SRCALPHA;
		StencilEnable	 = False;
		ZWriteEnable	 = False;
		zFunc = FixedupLESS;
	#else
		BlendOp   = ADD;
		SrcBlend  = SRCALPHA;
		DestBlend = INVSRCALPHA;

		#ifdef DECAL_WRITE_SLOPE
			BlendOpAlpha = ADD;
			SrcBlendAlpha  = ZERO;
			DestBlendAlpha = INVSRCALPHA;
			SeparateAlphaBlendEnable = true;
		#endif
	#endif

	#ifdef DECAL_WRITE_SLOPE
		#define NORMAL_COLOR_WRITE RED+GREEN+BLUE+ALPHA
	#else
		#define NORMAL_COLOR_WRITE RED+GREEN+BLUE
	#endif

	#if defined(DECAL_AMB_ONLY)
		SET_COLOR_WRITE_ENABLE(0, 0, 0, RED+GREEN)
	#elif defined(DECAL_DIRT)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, RED+GREEN+BLUE, 0)
	#elif defined(DECAL_DIFFUSE_ONLY)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, 0)
	#elif defined(DECAL_NORMAL_ONLY)
		SET_COLOR_WRITE_ENABLE(0, NORMAL_COLOR_WRITE, 0, 0)
	#elif defined(DECAL_DIFFUSE_NORMAL_ONLY)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, NORMAL_COLOR_WRITE, 0, 0)
	#elif defined(DECAL_SHADOW_ONLY)
		SET_COLOR_WRITE_ENABLE(0, 0, ALPHA, 0)
	#elif defined(USE_NORMAL_MAP)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, NORMAL_COLOR_WRITE, RED+GREEN+BLUE, BLUE)
	#elif defined(DECAL_GLUE)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, NORMAL_COLOR_WRITE, RED+GREEN+BLUE, RED+GREEN)
	#elif defined(DECAL_EMISSIVE_ONLY) || defined(DECAL_EMISSIVENIGHT_ONLY)
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, BLUE)
	#elif EMISSIVE_ADDITIVE
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, BLUE)
	#else
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, RED+GREEN, BLUE)
	#endif

	#if defined(DECAL_GLUE)
		zFunc = FixedupLESS;
		ZwriteEnable = true;
	#endif // defined(DECAL_GLUE)

#endif // not DECAL_SHADER

#ifdef USE_BACKLIGHTING_HACK
#ifdef USE_UMOVEMENTS_INV_BLUE
	SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
#endif
#endif
