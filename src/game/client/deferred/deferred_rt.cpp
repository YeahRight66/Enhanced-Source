
#include "cbase.h"
#include "deferred/deferred_shared_common.h"
#include "../../../materialsystem/deferredshaders/radiance_hints_config.h"

#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "vtf/vtf.h"
#include "texture_group_names.h"
#include <string.h>

static CTextureReference g_tex_Normals;
static CTextureReference g_tex_Depth;
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
static CTextureReference g_tex_LightCtrl;
#endif
static CTextureReference g_tex_Lightaccum;
static CTextureReference g_tex_Albedo;
static CTextureReference g_tex_Specular;

static CTextureReference g_tex_VolumePrepass;
static CTextureReference g_tex_VolumetricsBuffer[ 2 ];

static CTextureReference g_tex_ShadowColor_Ortho[ MAX_SHADOW_ORTHO ];
static CTextureReference g_tex_ShadowDepth_Ortho[ MAX_SHADOW_ORTHO ];

static CTextureReference g_tex_RHRSMDepth;
static CTextureReference g_tex_RHRSMColor;
static CTextureReference g_tex_RHRSMFlux;
static CTextureReference g_tex_RHRSMNormal;
static CTextureReference g_tex_RHRSMAlbedo;
static CTextureReference g_tex_ShadowColor_Proj[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowDepth_Proj[ MAX_SHADOW_PROJ ];
#if DEFCFG_ADAPTIVE_SHADOWMAP_LOD
static CTextureReference g_tex_ShadowColor_Proj_LOD1[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowDepth_Proj_LOD1[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowColor_Proj_LOD2[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowDepth_Proj_LOD2[ MAX_SHADOW_PROJ ];
#endif
static CTextureReference g_tex_ShadowColor_DP[ MAX_SHADOW_DP ];
static CTextureReference g_tex_ShadowDepth_DP[ MAX_SHADOW_DP ];

static CTextureReference g_tex_RadianceHints[ RH_CLIP_LEVEL_COUNT ][ RH_SET_COUNT ][ RH_CHANNEL_COUNT ];
static CTextureReference g_tex_RHIndirectHalf;
static CTextureReference g_tex_RHGeometry[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_RHSurfaceAlbedo[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_RHSurfaceNormal[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_GIBlockerField[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_RHSurfaceGuide[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_RHSurfaceCache[ RH_CLIP_LEVEL_COUNT ];
static CTextureReference g_tex_RHOpenSky[ RH_CLIP_LEVEL_COUNT ];

static unsigned char g_RHGeometryData[ RH_CLIP_LEVEL_COUNT ][ RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT ];
static unsigned char g_GIBlockerFieldData[ RH_CLIP_LEVEL_COUNT ][ RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT * 4 ];
static unsigned char g_RHSurfaceGuideData[ RH_CLIP_LEVEL_COUNT ][ RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT * 4 ];
static unsigned char g_RHSurfaceCacheData[ RH_CLIP_LEVEL_COUNT ][ RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT * 4 ];
static unsigned char g_RHOpenSkyData[ RH_CLIP_LEVEL_COUNT ][ RH_SKY_CACHE_ATLAS_WIDTH * RH_SKY_CACHE_ATLAS_HEIGHT ];

class CRHByteTextureRegenerator : public ITextureRegenerator
{
public:
    CRHByteTextureRegenerator( const unsigned char *pData, int width, int height )
        : m_pData( pData ), m_nWidth( width ), m_nHeight( height ) {}

    virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
    {
        (void)pTexture;
        (void)pRect;
        Assert( pTexture != NULL );
        Assert( pVTFTexture != NULL );
        Assert( pVTFTexture->Width() == m_nWidth );
        Assert( pVTFTexture->Height() == m_nHeight );
        Assert( pVTFTexture->Format() == IMAGE_FORMAT_RGBA8888 );

        unsigned char *pDst = pVTFTexture->ImageData( 0, 0, 0 );
        const int rowStride = pVTFTexture->RowSizeInBytes( 0 );
        for ( int y = 0; y < m_nHeight; ++y )
        {
            unsigned char *pRow = pDst + y * rowStride;
            const unsigned char *pSrc = m_pData + y * m_nWidth;
            for ( int x = 0; x < m_nWidth; ++x )
            {
                const unsigned char value = pSrc[x];
                pRow[x * 4 + 0] = value;
                pRow[x * 4 + 1] = value;
                pRow[x * 4 + 2] = value;
                pRow[x * 4 + 3] = 255;
            }
        }
    }

    virtual void Release() {}

private:
    const unsigned char *m_pData;
    int m_nWidth;
    int m_nHeight;
};

class CRHRGBAByteTextureRegenerator : public ITextureRegenerator
{
public:
    CRHRGBAByteTextureRegenerator( const unsigned char *pData, int width, int height )
        : m_pData( pData ), m_nWidth( width ), m_nHeight( height ) {}

    virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
    {
        (void)pTexture;
        (void)pRect;
        Assert( pVTFTexture != NULL );
        Assert( pVTFTexture->Width() == m_nWidth );
        Assert( pVTFTexture->Height() == m_nHeight );
        Assert( pVTFTexture->Format() == IMAGE_FORMAT_RGBA8888 );

        unsigned char *pDst = pVTFTexture->ImageData( 0, 0, 0 );
        const int rowStride = pVTFTexture->RowSizeInBytes( 0 );
        for ( int y = 0; y < m_nHeight; ++y )
            memcpy( pDst + y * rowStride, m_pData + y * m_nWidth * 4, m_nWidth * 4 );
    }

    virtual void Release() {}

private:
    const unsigned char *m_pData;
    int m_nWidth;
    int m_nHeight;
};

static CRHByteTextureRegenerator g_RHGeometryTextureRegenerator[ RH_CLIP_LEVEL_COUNT ] =
{
    CRHByteTextureRegenerator( g_RHGeometryData[0], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT ),
    CRHByteTextureRegenerator( g_RHGeometryData[1], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT )
};
static CRHRGBAByteTextureRegenerator g_GIBlockerFieldTextureRegenerator[ RH_CLIP_LEVEL_COUNT ] =
{
    CRHRGBAByteTextureRegenerator( g_GIBlockerFieldData[0], RH_SHADOW_ATLAS_WIDTH, RH_SHADOW_ATLAS_HEIGHT ),
    CRHRGBAByteTextureRegenerator( g_GIBlockerFieldData[1], RH_SHADOW_ATLAS_WIDTH, RH_SHADOW_ATLAS_HEIGHT )
};
static CRHRGBAByteTextureRegenerator g_RHSurfaceGuideTextureRegenerator[ RH_CLIP_LEVEL_COUNT ] =
{
    CRHRGBAByteTextureRegenerator( g_RHSurfaceGuideData[0], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT ),
    CRHRGBAByteTextureRegenerator( g_RHSurfaceGuideData[1], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT )
};
static CRHRGBAByteTextureRegenerator g_RHSurfaceCacheTextureRegenerator[ RH_CLIP_LEVEL_COUNT ] =
{
    CRHRGBAByteTextureRegenerator( g_RHSurfaceCacheData[0], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT ),
    CRHRGBAByteTextureRegenerator( g_RHSurfaceCacheData[1], RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT )
};
static CRHByteTextureRegenerator g_RHOpenSkyTextureRegenerator[ RH_CLIP_LEVEL_COUNT ] =
{
    CRHByteTextureRegenerator( g_RHOpenSkyData[0], RH_SKY_CACHE_ATLAS_WIDTH, RH_SKY_CACHE_ATLAS_HEIGHT ),
    CRHByteTextureRegenerator( g_RHOpenSkyData[1], RH_SKY_CACHE_ATLAS_WIDTH, RH_SKY_CACHE_ATLAS_HEIGHT )
};

static CTextureReference g_tex_ProjectableVGUI[ NUM_PROJECTABLE_VGUI ];

static float g_flDepthScalar = 65536.0f;

float GetDepthMapDepthResolution( float zDelta )
{
	return zDelta / g_flDepthScalar;
}

void DefRTsOnModeChanged()
{
	// Causes a crash ingame, so only allow in main menu
	if ( !engine->IsInGame() )
		InitDeferredRTs();
}

void InitDeferredRTs( bool bInitial )
{
	if ( !bInitial )
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly(); // HAHAHAHA. No.

	int screen_w = 128;
    int screen_h = 128;
    int dummy = 128;

    materials->GetBackBufferDimensions( screen_w, screen_h );
    const int rhHalfWidth = MAX( screen_w / 2, 1 );
    const int rhHalfHeight = MAX( screen_h / 2, 1 );

const ImageFormat fmt_gbuffer0 =
#if DEFCFG_LIGHTCTRL_PACKING
		IMAGE_FORMAT_RGBA8888;
#else
		IMAGE_FORMAT_RGB888;
#endif

#if !DEFCFG_LIGHTCTRL_PACKING
	const ImageFormat fmt_gbuffer2 = IMAGE_FORMAT_RGBA8888;
#endif

#if DEFCFG_DEFERRED_SHADING == 1
	const ImageFormat fmt_gbuffer2 = IMAGE_FORMAT_RGBA8888;
	const ImageFormat fmt_gbuffer3 = IMAGE_FORMAT_RGB888;
#endif
	const ImageFormat fmt_gbuffer1 = IMAGE_FORMAT_R32F;
	const ImageFormat fmt_lightAccum =
#if DEFCFG_LIGHTACCUM_COMPRESSED
		IMAGE_FORMAT_RGBA8888;
#else
		IMAGE_FORMAT_RGBA16161616F;
#endif
	const ImageFormat fmt_volumAccum = IMAGE_FORMAT_RGB888;
	const ImageFormat fmt_projVGUI = IMAGE_FORMAT_RGB888;

	const bool bShadowUseColor = 
#ifdef SHADOWMAPPING_USE_COLOR
		true;
#else
		false;
#endif

	const ImageFormat fmt_depth = GetDeferredManager()->GetShadowDepthFormat();
	const ImageFormat fmt_depthColor = bShadowUseColor ? IMAGE_FORMAT_R32F
		: g_pMaterialSystemHardwareConfig->GetNullTextureFormat();
    const ImageFormat fmt_radNormal = IMAGE_FORMAT_RGBA8888;
    const ImageFormat fmt_radBuffer = IMAGE_FORMAT_RGBA16161616F;
    const ImageFormat fmt_rhRSMFlux = IMAGE_FORMAT_RGBA16161616F;
    const ImageFormat fmt_rhRSMAlbedo = IMAGE_FORMAT_RGBA8888;
    const ImageFormat fmt_rhHalf = IMAGE_FORMAT_RGBA16161616F;

	if ( fmt_depth == IMAGE_FORMAT_D16_SHADOW )
		g_flDepthScalar = pow( 2.0, 16 );
	else if ( fmt_depth == IMAGE_FORMAT_D24X8_SHADOW )
		g_flDepthScalar = pow( 2.0, 24 );

	AssertMsg( fmt_depth == IMAGE_FORMAT_D16_SHADOW || fmt_depth == IMAGE_FORMAT_D24X8_SHADOW, "Unexpected depth format" );

	unsigned int gbufferFlags =			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int lightAccumFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int volumAccumFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int depthFlags =			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int shadowColorFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int projVGUIFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int radBufferFlags =       TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
    // RSM attributes must describe the same texel/surfel. Bilinear flux with
    // point-sampled depth/normal blends unrelated emitters at geometry edges and
    // then reconstructs that mixed energy at one surface. Daylight GI already
    // performs stratified injection and volume reconstruction, so keep every
    // RSM attribute texel-coherent.
    unsigned int rhRSMFluxFlags =      TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
    unsigned int rhRSMNormalFlags =    TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
    unsigned int rhRSMAlbedoFlags =    TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
    unsigned int rhHalfFlags =         TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;

	materials->BeginRenderTargetAllocation();

	shadowData_general_t generalShadowData;

	if ( bInitial )
	{
		g_tex_Normals.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER0,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_gbuffer0,
#if DEFCFG_DEFERRED_SHADING == 1
			MATERIAL_RT_DEPTH_NONE,
#else
			MATERIAL_RT_DEPTH_SHARED,
#endif
			gbufferFlags, 0 ) );

		g_tex_Depth.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER1,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_gbuffer1,
			MATERIAL_RT_DEPTH_NONE,
			gbufferFlags, 0 ) );

#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
	g_tex_LightCtrl.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER2,
		dummy, dummy,
		RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
		fmt_gbuffer2,
		MATERIAL_RT_DEPTH_NONE,
		gbufferFlags, 0 ) );

#elif DEFCFG_DEFERRED_SHADING == 1
	g_tex_Albedo.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER2,
		dummy, dummy,
		RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
		fmt_gbuffer2,
		MATERIAL_RT_DEPTH_SHARED,
		gbufferFlags, 0 ) );

	g_tex_Specular.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER3,
		dummy, dummy,
		RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
		fmt_gbuffer3,
		MATERIAL_RT_DEPTH_NONE,
		gbufferFlags, 0 ) );
#endif

		g_tex_Lightaccum.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_LIGHTACCUM,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_lightAccum,
			MATERIAL_RT_DEPTH_NONE,
			lightAccumFlags, 0 ) );

		for ( int i = 0; i < 2; i++ )
			g_tex_VolumetricsBuffer[ i ].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_VOLUMACCUM, i ),
				dummy, dummy,
				RT_SIZE_HDR,
				fmt_volumAccum,
				MATERIAL_RT_DEPTH_NONE,
				volumAccumFlags, 0 ) );

		g_tex_VolumePrepass.Init( materials->CreateNamedRenderTargetTextureEx2(
			DEFRTNAME_VOLUMPREPASS,
			dummy, dummy,
			RT_SIZE_HDR,
			fmt_gbuffer1,
			MATERIAL_RT_DEPTH_NONE,
			gbufferFlags, 0 ) );


#if DEFCFG_ENABLE_RADIOSITY
        // Dedicated, camera-rotation-independent Reflective Shadow Map used only by RH.
        g_tex_RHRSMDepth.Init( materials->CreateNamedRenderTargetTextureEx2(
            DEFRTNAME_RH_RSM_DEPTH,
            RH_RSM_RESOLUTION, RH_RSM_RESOLUTION,
            RT_SIZE_NO_CHANGE,
            fmt_depth,
            MATERIAL_RT_DEPTH_NONE,
            depthFlags, 0 ) );

        g_tex_RHRSMColor.Init( materials->CreateNamedRenderTargetTextureEx2(
            DEFRTNAME_RH_RSM_COLOR,
            RH_RSM_RESOLUTION, RH_RSM_RESOLUTION,
            RT_SIZE_NO_CHANGE,
            fmt_depthColor,
            MATERIAL_RT_DEPTH_NONE,
            shadowColorFlags, 0 ) );

        g_tex_RHRSMFlux.Init( materials->CreateNamedRenderTargetTextureEx2(
            DEFRTNAME_RH_RSM_FLUX,
            RH_RSM_RESOLUTION, RH_RSM_RESOLUTION,
            RT_SIZE_NO_CHANGE,
            fmt_rhRSMFlux,
            MATERIAL_RT_DEPTH_NONE,
            rhRSMFluxFlags, 0 ) );

        g_tex_RHRSMNormal.Init( materials->CreateNamedRenderTargetTextureEx2(
            DEFRTNAME_RH_RSM_NORMAL,
            RH_RSM_RESOLUTION, RH_RSM_RESOLUTION,
            RT_SIZE_NO_CHANGE,
            fmt_radNormal,
            MATERIAL_RT_DEPTH_NONE,
            rhRSMNormalFlags, 0 ) );

        g_tex_RHRSMAlbedo.Init( materials->CreateNamedRenderTargetTextureEx2(
            DEFRTNAME_RH_RSM_ALBEDO,
            RH_RSM_RESOLUTION, RH_RSM_RESOLUTION,
            RT_SIZE_NO_CHANGE,
            fmt_rhRSMAlbedo,
            MATERIAL_RT_DEPTH_NONE,
            rhRSMAlbedoFlags, 0 ) );

#endif

		for ( int i = 0; i < MAX_SHADOW_ORTHO; i++ )
		{
#if CSM_USE_COMPOSITED_TARGET
			int iResolution_x = CSM_COMP_RES_X;
			int iResolution_y = CSM_COMP_RES_Y;
#else
			const cascade_t &c = GetCascadeInfo( i );
			int iResolution_x = c.iResolution;
			int iResolution_y = c.iResolution;
#endif

			g_tex_ShadowDepth_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );

			g_tex_ShadowColor_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );

			Assert( iResolution_y == g_tex_ShadowDepth_Ortho[i]->GetActualHeight() );
			Assert( iResolution_y == g_tex_ShadowColor_Ortho[i]->GetActualHeight() );
			Assert( iResolution_x == g_tex_ShadowDepth_Ortho[i]->GetActualWidth() );
			Assert( iResolution_x == g_tex_ShadowColor_Ortho[i]->GetActualWidth() );
		}

		for ( int i = 0; i < NUM_PROJECTABLE_VGUI; i++ )
		{
			g_tex_ProjectableVGUI[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_PROJECTABLE_VGUI, i ),
				PROJECTABLE_VGUI_RES, PROJECTABLE_VGUI_RES,
				RT_SIZE_NO_CHANGE,
				fmt_projVGUI,
				MATERIAL_RT_DEPTH_NONE,
				projVGUIFlags, 0 ) );
		}

#if DEFCFG_ENABLE_RADIOSITY
        static const char *s_RHNames[ RH_CHANNEL_COUNT ] =
        {
            DEFRTNAME_RH_SH_R,
            DEFRTNAME_RH_SH_G,
            DEFRTNAME_RH_SH_B,
            DEFRTNAME_RH_META
        };

		for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
		{
			for ( int setIndex = 0; setIndex < RH_SET_COUNT; ++setIndex )
			{
				for ( int channelIndex = 0; channelIndex < RH_CHANNEL_COUNT; ++channelIndex )
				{
					g_tex_RadianceHints[ clip ][ setIndex ][ channelIndex ].Init(
						materials->CreateNamedRenderTargetTextureEx2(
							VarArgs( "%sL%i_%02i", s_RHNames[ channelIndex ], clip, setIndex ),
							RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT, RT_SIZE_NO_CHANGE, fmt_radBuffer,
							MATERIAL_RT_DEPTH_NONE, radBufferFlags, 0 ) );
				}
			}

			g_tex_RHSurfaceAlbedo[clip].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s_L%i", DEFRTNAME_RH_SURFACE_ALBEDO, clip ), RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT,
				RT_SIZE_NO_CHANGE, fmt_radBuffer, MATERIAL_RT_DEPTH_NONE, radBufferFlags, 0 ) );
			g_tex_RHSurfaceNormal[clip].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s_L%i", DEFRTNAME_RH_SURFACE_NORMAL, clip ), RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT,
				RT_SIZE_NO_CHANGE, fmt_radBuffer, MATERIAL_RT_DEPTH_NONE, radBufferFlags, 0 ) );
		}
#endif
	}

#if DEFCFG_ENABLE_RADIOSITY
	// Exact half-resolution target must be recreated after a video-mode change.
	g_tex_RHIndirectHalf.Init( materials->CreateNamedRenderTargetTextureEx2(
		DEFRTNAME_RH_INDIRECT_HALF,
		rhHalfWidth, rhHalfHeight,
		RT_SIZE_DEFAULT,
		fmt_rhHalf,
		MATERIAL_RT_DEPTH_NONE,
		rhHalfFlags, 0 ) );
#endif

	for ( int i = 0; i < MAX_SHADOW_PROJ; i++ )
	{
		int res = GetShadowResolution_Spot();
		generalShadowData.iPROJ_Res = res;
		bool bFirst = i == 0;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_Proj[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_PROJ, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_Proj[i].Init( g_tex_ShadowDepth_Proj[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_Proj[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_PROJ, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_Proj[i].Init( g_tex_ShadowColor_Proj[0] );

		Assert( res == g_tex_ShadowDepth_Proj[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowColor_Proj[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowDepth_Proj[i]->GetActualWidth() );
		Assert( res == g_tex_ShadowColor_Proj[i]->GetActualWidth() );

#if DEFCFG_ADAPTIVE_SHADOWMAP_LOD
		res = GetShadowResolution_Spot_LOD1();
		generalShadowData.iPROJ_Res_LOD1 = res;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_Proj_LOD1[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_PROJ_LOD1, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_Proj_LOD1[i].Init( g_tex_ShadowDepth_Proj_LOD1[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_Proj_LOD1[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_PROJ_LOD1, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_Proj_LOD1[i].Init( g_tex_ShadowColor_Proj_LOD1[0] );

		Assert( res == g_tex_ShadowDepth_Proj_LOD1[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowColor_Proj_LOD1[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowDepth_Proj_LOD1[i]->GetActualWidth() );
		Assert( res == g_tex_ShadowColor_Proj_LOD1[i]->GetActualWidth() );

		res = GetShadowResolution_Spot_LOD2();
		generalShadowData.iPROJ_Res_LOD2 = res;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_Proj_LOD2[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_PROJ_LOD2, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_Proj_LOD2[i].Init( g_tex_ShadowDepth_Proj_LOD2[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_Proj_LOD2[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_PROJ_LOD2, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_Proj_LOD2[i].Init( g_tex_ShadowColor_Proj_LOD2[0] );

		Assert( res == g_tex_ShadowDepth_Proj_LOD2[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowColor_Proj_LOD2[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowDepth_Proj_LOD2[i]->GetActualWidth() );
		Assert( res == g_tex_ShadowColor_Proj_LOD2[i]->GetActualWidth() );
#endif	
	}

	for ( int i = 0; i < MAX_SHADOW_DP; i++ )
	{
		int res_x = GetShadowResolution_Point();
		int res_y = res_x * 2;

		generalShadowData.iDPSM_Res_x = res_x;
		generalShadowData.iDPSM_Res_y = res_y;

		bool bFirst = i == 0;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_DP[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_DP, i ),
				res_x, res_y,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_DP[i].Init( g_tex_ShadowDepth_DP[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_DP[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_DP, i ),
				res_x, res_y,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_DP[i].Init( g_tex_ShadowColor_DP[0] );

		Assert( res_y == g_tex_ShadowDepth_DP[i]->GetActualHeight() );
		Assert( res_y == g_tex_ShadowColor_DP[i]->GetActualHeight() );
		Assert( res_x == g_tex_ShadowDepth_DP[i]->GetActualWidth() );
		Assert( res_x == g_tex_ShadowColor_DP[i]->GetActualWidth() );
	}

	

	materials->EndRenderTargetAllocation();


#if DEFCFG_ENABLE_RADIOSITY
    for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
    {
#define RH_CREATE_CLIP_BYTE_TEX( REF, NAME, DATA, REGEN, W, H, INITIAL, EXTRAFLAGS ) \
        if ( bInitial && !(REF)[clip].IsValid() ) \
        { \
            memset( (DATA)[clip], INITIAL, sizeof( (DATA)[clip] ) ); \
            (REF)[clip].Init( materials->CreateProceduralTexture( VarArgs( "%s_L%i", NAME, clip ), \
                TEXTURE_GROUP_OTHER, W, H, IMAGE_FORMAT_RGBA8888, TEXTUREFLAGS_PROCEDURAL | \
                TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | EXTRAFLAGS ) ); \
            if ( (REF)[clip].IsValid() ) \
            { \
                (REF)[clip]->SetTextureRegenerator( &(REGEN)[clip] ); \
                (REF)[clip]->Download(); \
            } \
        }

        RH_CREATE_CLIP_BYTE_TEX( g_tex_RHGeometry, DEFRTNAME_RH_GEOMETRY,
            g_RHGeometryData, g_RHGeometryTextureRegenerator,
            RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT, 0, TEXTUREFLAGS_POINTSAMPLE );
        const bool initializeBlockerField = bInitial && !g_tex_GIBlockerField[clip].IsValid();
        RH_CREATE_CLIP_BYTE_TEX( g_tex_GIBlockerField, DEFRTNAME_GI_BLOCKER_FIELD,
            g_GIBlockerFieldData, g_GIBlockerFieldTextureRegenerator,
            RH_SHADOW_ATLAS_WIDTH, RH_SHADOW_ATLAS_HEIGHT, 0, 0 );
        if ( initializeBlockerField && g_tex_GIBlockerField[clip].IsValid() )
        {
            const int blockerCellCount = RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT;
            for ( int blockerCell = 0; blockerCell < blockerCellCount; ++blockerCell )
            {
                unsigned char *pField = g_GIBlockerFieldData[clip] + blockerCell * 4;
                pField[0] = 255; pField[1] = 0; pField[2] = 128; pField[3] = 128;
            }
            g_tex_GIBlockerField[clip]->Download();
        }
        RH_CREATE_CLIP_BYTE_TEX( g_tex_RHSurfaceGuide, DEFRTNAME_RH_SURFACE_GUIDE,
            g_RHSurfaceGuideData, g_RHSurfaceGuideTextureRegenerator,
            RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT, 0, 0 );
        RH_CREATE_CLIP_BYTE_TEX( g_tex_RHSurfaceCache, DEFRTNAME_RH_SURFACE_CACHE,
            g_RHSurfaceCacheData, g_RHSurfaceCacheTextureRegenerator,
            RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT, 0, TEXTUREFLAGS_POINTSAMPLE );
        RH_CREATE_CLIP_BYTE_TEX( g_tex_RHOpenSky, DEFRTNAME_RH_OPEN_SKY,
            g_RHOpenSkyData, g_RHOpenSkyTextureRegenerator,
            RH_SKY_CACHE_ATLAS_WIDTH, RH_SKY_CACHE_ATLAS_HEIGHT, 255, 0 );
#undef RH_CREATE_CLIP_BYTE_TEX
    }

#endif
	
	if (!bInitial)
		materials->FinishRenderTargetAllocation();


	GetDeferredExt()->CommitTexture_General( g_tex_Normals, g_tex_Depth,
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
		g_tex_LightCtrl,
#elif DEFCFG_DEFERRED_SHADING == 1
		g_tex_Albedo,
		g_tex_Specular,
#endif
		g_tex_Lightaccum );

	for ( int i = 0; i < MAX_SHADOW_ORTHO; i++ )
		GetDeferredExt()->CommitTexture_CascadedDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_Ortho[i] : g_tex_ShadowDepth_Ortho[i]
		);

	for ( int i = 0; i < MAX_SHADOW_DP; i++ )
		GetDeferredExt()->CommitTexture_DualParaboloidDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_DP[i] : g_tex_ShadowDepth_DP[i]
		);

	for ( int i = 0; i < MAX_SHADOW_PROJ; i++ )
		GetDeferredExt()->CommitTexture_ProjectedDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_Proj[i] : g_tex_ShadowDepth_Proj[i]
		);

	GetDeferredExt()->CommitTexture_VolumePrePass( g_tex_VolumePrepass );

	GetDeferredExt()->CommitShadowData_General( generalShadowData );

#if DEFCFG_ENABLE_RADIOSITY
    GetDeferredExt()->CommitTexture_RadianceHintsRSM(
        g_tex_RHRSMFlux,
        g_tex_RHRSMNormal,
        bShadowUseColor ? g_tex_RHRSMColor : g_tex_RHRSMDepth );
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		for ( int setIndex = 0; setIndex < RH_SET_COUNT; ++setIndex )
		{
			GetDeferredExt()->CommitTexture_DaylightGIRadiance( clip, setIndex,
				g_tex_RadianceHints[clip][setIndex][RH_CHANNEL_SH_R],
				g_tex_RadianceHints[clip][setIndex][RH_CHANNEL_SH_G],
				g_tex_RadianceHints[clip][setIndex][RH_CHANNEL_SH_B],
				g_tex_RadianceHints[clip][setIndex][RH_CHANNEL_META] );
		}
		GetDeferredExt()->CommitTexture_DaylightGICache( clip,
			g_tex_RHSurfaceAlbedo[clip], g_tex_RHSurfaceNormal[clip],
			g_tex_RHGeometry[clip], g_tex_GIBlockerField[clip],
			g_tex_RHSurfaceGuide[clip], g_tex_RHSurfaceCache[clip], g_tex_RHOpenSky[clip] );
	}
#endif
}

int GetShadowResolution_Spot()
{
	return deferred_rt_shadowspot_res.GetInt();
}

#if DEFCFG_ADAPTIVE_SHADOWMAP_LOD
int GetShadowResolution_Spot_LOD1()
{
	return deferred_rt_shadowspot_lod1_res.GetInt();
}

int GetShadowResolution_Spot_LOD2()
{
	return deferred_rt_shadowspot_lod2_res.GetInt();
}
#endif

int GetShadowResolution_Point()
{
	return deferred_rt_shadowpoint_res.GetInt();
}

#if DEFCFG_ADAPTIVE_SHADOWMAP_LOD
int GetShadowResolution_Point_LOD1()
{
	return deferred_rt_shadowpoint_lod1_res.GetInt();
}

int GetShadowResolution_Point_LOD2()
{
	return deferred_rt_shadowpoint_lod2_res.GetInt();
}
#endif

ITexture *GetDefRT_Normals()
{
	Assert( g_tex_Normals.IsValid() );
	return g_tex_Normals;
}

ITexture *GetDefRT_Depth()
{
	Assert( g_tex_Depth.IsValid() );
	return g_tex_Depth;
}

ITexture *GetDefRT_Albedo()
{
	Assert( g_tex_Albedo.IsValid() );
	return g_tex_Albedo;
}

ITexture *GetDefRT_Specular()
{
	Assert( g_tex_Specular.IsValid() );
	return g_tex_Specular;
}

#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
ITexture *GetDefRT_LightCtrl()
{
	return g_tex_LightCtrl;
}
#endif

ITexture *GetDefRT_Lightaccum()
{
	Assert( g_tex_Lightaccum.IsValid() );
	return g_tex_Lightaccum;
}

ITexture *GetDefRT_VolumePrepass()
{
	Assert( g_tex_VolumePrepass.IsValid() );
	return g_tex_VolumePrepass;
}

ITexture *GetDefRT_VolumetricsBuffer( int index )
{
	Assert( g_tex_VolumetricsBuffer[ index ].IsValid() );
	return g_tex_VolumetricsBuffer[ index ];
}

ITexture *GetDefRT_RHRSMDepth()
{
    Assert( g_tex_RHRSMDepth.IsValid() );
    return g_tex_RHRSMDepth;
}

ITexture *GetDefRT_RHRSMColor()
{
    Assert( g_tex_RHRSMColor.IsValid() );
    return g_tex_RHRSMColor;
}

ITexture *GetDefRT_RHRSMFlux()
{
    Assert( g_tex_RHRSMFlux.IsValid() );
    return g_tex_RHRSMFlux;
}

ITexture *GetDefRT_RHRSMNormal()
{
    Assert( g_tex_RHRSMNormal.IsValid() );
    return g_tex_RHRSMNormal;
}

ITexture *GetDefRT_RHRSMAlbedo()
{
    Assert( g_tex_RHRSMAlbedo.IsValid() );
    return g_tex_RHRSMAlbedo;
}

ITexture *GetDefRT_DaylightGIIndirectHalf()
{
    Assert( g_tex_RHIndirectHalf.IsValid() );
    return g_tex_RHIndirectHalf;
}

ITexture *GetDefRT_DaylightGIRadiance( int clip, int setIndex, int channelIndex )
{
    Assert( clip >= 0 && clip < RH_CLIP_LEVEL_COUNT );
    Assert( setIndex >= 0 && setIndex < RH_SET_COUNT );
    Assert( channelIndex >= 0 && channelIndex < RH_CHANNEL_COUNT );
    Assert( g_tex_RadianceHints[clip][setIndex][channelIndex].IsValid() );
    return g_tex_RadianceHints[clip][setIndex][channelIndex];
}

#define RH_GI_CLIP_GETTER( FUNC, TEX ) \
ITexture *FUNC( int clip ) \
{ \
    Assert( clip >= 0 && clip < RH_CLIP_LEVEL_COUNT ); \
    Assert( (TEX)[clip].IsValid() ); \
    return (TEX)[clip]; \
}
RH_GI_CLIP_GETTER( GetDefRT_DaylightGISurfaceAlbedo, g_tex_RHSurfaceAlbedo )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGISurfaceNormal, g_tex_RHSurfaceNormal )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGIGeometry, g_tex_RHGeometry )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGIBlockerField, g_tex_GIBlockerField )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGISurfaceGuide, g_tex_RHSurfaceGuide )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGISurfaceCache, g_tex_RHSurfaceCache )
RH_GI_CLIP_GETTER( GetDefRT_DaylightGIOpenSky, g_tex_RHOpenSky )
#undef RH_GI_CLIP_GETTER

#define RH_GI_CLIP_UPDATE( FUNC, TEX, DATA, SIZEEXPR ) \
void FUNC( int clip, const unsigned char *pData, int nDataSize ) \
{ \
    const int expectedSize = (SIZEEXPR); \
    Assert( clip >= 0 && clip < RH_CLIP_LEVEL_COUNT ); \
    Assert( pData != NULL ); Assert( nDataSize == expectedSize ); \
    if ( clip < 0 || clip >= RH_CLIP_LEVEL_COUNT || pData == NULL || \
         nDataSize != expectedSize || !(TEX)[clip].IsValid() ) return; \
    if ( memcmp( (DATA)[clip], pData, expectedSize ) == 0 ) return; \
    memcpy( (DATA)[clip], pData, expectedSize ); \
    (TEX)[clip]->Download(); \
}
RH_GI_CLIP_UPDATE( UpdateDefRT_DaylightGIGeometry, g_tex_RHGeometry, g_RHGeometryData, RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT )
RH_GI_CLIP_UPDATE( UpdateDefRT_DaylightGIBlockerField, g_tex_GIBlockerField, g_GIBlockerFieldData, RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT * 4 )
RH_GI_CLIP_UPDATE( UpdateDefRT_DaylightGISurfaceGuide, g_tex_RHSurfaceGuide, g_RHSurfaceGuideData, RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT * 4 )
RH_GI_CLIP_UPDATE( UpdateDefRT_DaylightGISurfaceCache, g_tex_RHSurfaceCache, g_RHSurfaceCacheData, RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT * 4 )
RH_GI_CLIP_UPDATE( UpdateDefRT_DaylightGIOpenSky, g_tex_RHOpenSky, g_RHOpenSkyData, RH_SKY_CACHE_ATLAS_WIDTH * RH_SKY_CACHE_ATLAS_HEIGHT )
#undef RH_GI_CLIP_UPDATE

ITexture *GetShadowColorRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	Assert( g_tex_ShadowColor_Ortho[ index ].IsValid() );
	return g_tex_ShadowColor_Ortho[ index ];
}
ITexture *GetShadowDepthRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	Assert( g_tex_ShadowDepth_Ortho[ index ].IsValid() );
	return g_tex_ShadowDepth_Ortho[ index ];
}

ITexture *GetShadowColorRT_Proj( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	Assert( g_tex_ShadowColor_Proj[ index ].IsValid() );
	return g_tex_ShadowColor_Proj[ index ];
}
ITexture *GetShadowDepthRT_Proj( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	Assert( g_tex_ShadowDepth_Proj[ index ].IsValid() );
	return g_tex_ShadowDepth_Proj[ index ];
}

ITexture *GetShadowColorRT_DP( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	Assert( g_tex_ShadowColor_DP[ index ].IsValid() );
	return g_tex_ShadowColor_DP[ index ];
}
ITexture *GetShadowDepthRT_DP( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	Assert( g_tex_ShadowDepth_DP[ index ].IsValid() );
	return g_tex_ShadowDepth_DP[ index ];
}
ITexture *GetProjectableVguiRT( int index )
{
	Assert( index >= 0 && index < NUM_PROJECTABLE_VGUI );
	Assert( g_tex_ProjectableVGUI[ index ].IsValid() );
	return g_tex_ProjectableVGUI[ index ];
}
