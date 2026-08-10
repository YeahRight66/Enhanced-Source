
#include "deferred_includes.h"

CDeferredExtension __g_defExt;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDeferredExtension, IDeferredExtension, DEFERRED_EXTENSION_VERSION, __g_defExt );

CDeferredExtension::CDeferredExtension()
{
	m_bDefLightingEnabled = false;

	m_vecOrigin.Init();
	m_vecForward.Init();
	m_flZDists[0] = m_flZDists[1] = m_flZDists[2] = 0;
	m_matTFrustumD.Identity();

	m_pTexNormals = NULL;
	m_pTexDepth = NULL;
	m_pTexLightAccum = NULL;
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
	m_pTexLightCtrl = NULL;
#elif DEFCFG_DEFERRED_SHADING == 1
	m_pTexAlbedo = NULL;
	m_pTexSpecular = NULL;
#endif

	Q_memset( m_pTexShadowDepth_Ortho, 0, sizeof( ITexture* ) * MAX_SHADOW_ORTHO );
	Q_memset( m_pTexShadowDepth_DP, 0, sizeof( ITexture* ) * MAX_SHADOW_DP );
	Q_memset( m_pTexShadowDepth_Proj, 0, sizeof( ITexture* ) * MAX_SHADOW_PROJ );
	Q_memset( m_pTexCookie, 0, sizeof( ITexture* ) * NUM_COOKIE_SLOTS );
	m_pTexVolumePrePass = NULL;
	Q_memset( m_pTexRadianceHintsRSM, 0, sizeof( ITexture* ) * 3 );
	Q_memset( m_pTexDaylightGIRadiance, 0, sizeof( m_pTexDaylightGIRadiance ) );
	Q_memset( m_pTexDaylightGISurfaceAlbedo, 0, sizeof( m_pTexDaylightGISurfaceAlbedo ) );
	Q_memset( m_pTexDaylightGISurfaceNormal, 0, sizeof( m_pTexDaylightGISurfaceNormal ) );
	Q_memset( m_pTexDaylightGIGeometry, 0, sizeof( m_pTexDaylightGIGeometry ) );
	Q_memset( m_pTexDaylightGIBlockerField, 0, sizeof( m_pTexDaylightGIBlockerField ) );
	Q_memset( m_pTexDaylightGISurfaceGuide, 0, sizeof( m_pTexDaylightGISurfaceGuide ) );
	Q_memset( m_pTexDaylightGISurfaceCache, 0, sizeof( m_pTexDaylightGISurfaceCache ) );
	Q_memset( m_pTexDaylightGIOpenSky, 0, sizeof( m_pTexDaylightGIOpenSky ) );

	m_pflCommonLightData = NULL;
	m_iCommon_NumRows = 0;
	m_iNumCommon_ShadowedCookied = 0;
	m_iNumCommon_Shadowed = 0;
	m_iNumCommon_Cookied = 0;
	m_iNumCommon_Simple = 0;
}

CDeferredExtension::~CDeferredExtension()
{
}


void CDeferredExtension::EnableDeferredLighting()
{
	m_bDefLightingEnabled = true;
}

bool CDeferredExtension::IsDeferredLightingEnabled()
{
	return m_bDefLightingEnabled;
}

bool CDeferredExtension::IsRadiosityEnabled()
{
	static ConVarRef refGI( "deferred_gi_enable" );
	static ConVarRef refRadiosityAlias( "deferred_radiosity_enable" );

	Assert( refGI.IsValid() );
	Assert( refRadiosityAlias.IsValid() );

	return refGI.GetBool() && refRadiosityAlias.GetBool();
}

void CDeferredExtension::CommitOrigin( const Vector &origin )
{
	VectorCopy( origin.Base(), m_vecOrigin.Base() );
}
void CDeferredExtension::CommitViewForward( const Vector &fwd )
{
	VectorCopy( fwd.Base(), m_vecForward.Base() );
}
void CDeferredExtension::CommitZDists( const float &zNear, const float &zFar )
{
	m_flZDists[0] = zNear;
	m_flZDists[1] = zFar;
}
void CDeferredExtension::CommitZScale( const float &zFar )
{
	m_flZDists[2] = zFar;
}
void CDeferredExtension::CommitFrustumDeltas( const VMatrix &matTFrustum )
{
	m_matTFrustumD = matTFrustum;
}
#if DEFCFG_BILATERAL_DEPTH_TEST
void CDeferredExtension::CommitWorldToCameraDepthTex( const VMatrix &matWorldCameraDepthTex )
{
	m_matWorldCameraDepthTex = matWorldCameraDepthTex;
}
#endif
void CDeferredExtension::CommitShadowData_Ortho( const int &index, const shadowData_ortho_t &data )
{
	Assert( index >= 0 && index < SHADOW_NUM_CASCADES );
	m_dataOrtho[ index ] = data;
}
void CDeferredExtension::CommitShadowData_Proj( const int &index, const shadowData_proj_t &data )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	m_dataProj[ index ] = data;
}
void CDeferredExtension::CommitShadowData_General( const shadowData_general_t &data )
{
	m_dataGeneral = data;
}

void CDeferredExtension::CommitVolumeData( const volumeData_t &data )
{
	m_dataVolume = data;
}

void CDeferredExtension::CommitDaylightGIData( const daylightGIData_t &data )
{
	m_dataDaylightGI = data;
}

void CDeferredExtension::CommitDaylightGIActiveClip( int clip )
{
	Assert( clip >= 0 && clip < DAYLIGHT_GI_CLIP_COUNT );
	m_dataDaylightGI.iActiveClip = clamp( clip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 );
	m_dataDaylightGI.vecRSMParams.w =
		m_dataDaylightGI.clips[m_dataDaylightGI.iActiveClip].flRadianceCellSize;
}

void CDeferredExtension::CommitDaylightGIRSMData( const VMatrix &worldToRSM,
	const VMatrix &rsmToWorld, const Vector4D &rsmParams )
{
	m_dataDaylightGI.matWorldToRSM = worldToRSM;
	m_dataDaylightGI.matRSMToWorld = rsmToWorld;
	m_dataDaylightGI.vecRSMParams = rsmParams;
	const int clip = clamp( m_dataDaylightGI.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 );
	m_dataDaylightGI.vecRSMParams.w = m_dataDaylightGI.clips[clip].flRadianceCellSize;
}

void CDeferredExtension::CommitLightData_Global( const lightData_Global_t &data )
{
	m_globalLight = data;
}

float *CDeferredExtension::CommitLightData_Common( float *pFlData, int numRows,
		int numShadowedCookied, int numShadowed,
		int numCookied, int numSimple )
{
	float *pReturn = m_pflCommonLightData;

	m_pflCommonLightData = pFlData;
	m_iCommon_NumRows = numRows;
	m_iNumCommon_ShadowedCookied = numShadowedCookied;
	m_iNumCommon_Shadowed = numShadowed;
	m_iNumCommon_Cookied = numCookied;
	m_iNumCommon_Simple = numSimple;

	return pReturn;
}

void CDeferredExtension::CommitTexture_General( ITexture *pTexNormals, ITexture *pTexDepth,
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
		ITexture *pTexLightingCtrl,
#elif DEFCFG_DEFERRED_SHADING == 1
		ITexture *pTexAlbedo,
		ITexture *pTexSpecular,
#endif
		ITexture *pTexLightAccum )
{
	m_pTexNormals = pTexNormals;
	m_pTexDepth = pTexDepth;
	m_pTexLightAccum = pTexLightAccum;
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
	m_pTexLightCtrl = pTexLightingCtrl;
#elif DEFCFG_DEFERRED_SHADING == 1
	m_pTexAlbedo = pTexAlbedo;
	m_pTexSpecular = pTexSpecular;
#endif
}
void CDeferredExtension::CommitTexture_CascadedDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	m_pTexShadowDepth_Ortho[ index ] = pTexShadowDepth;
}
void CDeferredExtension::CommitTexture_DualParaboloidDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	m_pTexShadowDepth_DP[ index ] = pTexShadowDepth;
}
void CDeferredExtension::CommitTexture_ProjectedDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	m_pTexShadowDepth_Proj[ index ] = pTexShadowDepth;
}
void CDeferredExtension::CommitTexture_Cookie( const int &index, ITexture *pTexCookie )
{
	Assert( index >= 0 && index < NUM_COOKIE_SLOTS );
	m_pTexCookie[ index ] = pTexCookie;
}
void CDeferredExtension::CommitTexture_VolumePrePass( ITexture *pTexVolumePrePass )
{
	m_pTexVolumePrePass = pTexVolumePrePass;
}
void CDeferredExtension::CommitTexture_RadianceHintsRSM( ITexture *pFlux, ITexture *pNormal, ITexture *pDepth )
{
    m_pTexRadianceHintsRSM[0] = pFlux;
    m_pTexRadianceHintsRSM[1] = pNormal;
    m_pTexRadianceHintsRSM[2] = pDepth;
}
void CDeferredExtension::CommitTexture_DaylightGIRadiance( int clip, int setIndex,
    ITexture *pSHR, ITexture *pSHG, ITexture *pSHB, ITexture *pMeta )
{
    Assert( clip >= 0 && clip < DAYLIGHT_GI_CLIP_COUNT );
    Assert( setIndex >= 0 && setIndex < DAYLIGHT_GI_WORK_SET_COUNT );
    if ( clip < 0 || clip >= DAYLIGHT_GI_CLIP_COUNT ||
         setIndex < 0 || setIndex >= DAYLIGHT_GI_WORK_SET_COUNT ) return;
    m_pTexDaylightGIRadiance[clip][setIndex][0] = pSHR;
    m_pTexDaylightGIRadiance[clip][setIndex][1] = pSHG;
    m_pTexDaylightGIRadiance[clip][setIndex][2] = pSHB;
    m_pTexDaylightGIRadiance[clip][setIndex][3] = pMeta;
}
void CDeferredExtension::CommitTexture_DaylightGICache( int clip,
    ITexture *pSurfaceAlbedo, ITexture *pSurfaceNormal, ITexture *pGeometry,
    ITexture *pBlockerField,
    ITexture *pSurfaceGuide, ITexture *pSurfaceCache, ITexture *pOpenSky )
{
    Assert( clip >= 0 && clip < DAYLIGHT_GI_CLIP_COUNT );
    if ( clip < 0 || clip >= DAYLIGHT_GI_CLIP_COUNT ) return;
    m_pTexDaylightGISurfaceAlbedo[clip] = pSurfaceAlbedo;
    m_pTexDaylightGISurfaceNormal[clip] = pSurfaceNormal;
    m_pTexDaylightGIGeometry[clip] = pGeometry;
    m_pTexDaylightGIBlockerField[clip] = pBlockerField;
    m_pTexDaylightGISurfaceGuide[clip] = pSurfaceGuide;
    m_pTexDaylightGISurfaceCache[clip] = pSurfaceCache;
    m_pTexDaylightGIOpenSky[clip] = pOpenSky;
}
