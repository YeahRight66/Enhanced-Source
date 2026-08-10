#ifndef DEFERRED_RT_H
#define DEFERRED_RT_H

class ITexture;

float GetDepthMapDepthResolution( float zDelta );
void DefRTsOnModeChanged();
void InitDeferredRTs( bool bInitial = false );

ITexture *GetDefRT_Normals();
ITexture *GetDefRT_Depth();
ITexture *GetDefRT_Albedo();
ITexture *GetDefRT_Specular();
ITexture *GetDefRT_LightCtrl();
ITexture *GetDefRT_Lightaccum();

ITexture *GetDefRT_VolumePrepass();
ITexture *GetDefRT_VolumetricsBuffer( int index );

ITexture *GetDefRT_RHRSMDepth();
ITexture *GetDefRT_RHRSMColor();
ITexture *GetDefRT_RHRSMFlux();
ITexture *GetDefRT_RHRSMNormal();
ITexture *GetDefRT_RHRSMAlbedo();

// Two-level daylight GI resources.
ITexture *GetDefRT_DaylightGIRadiance( int clip, int setIndex, int channelIndex );
ITexture *GetDefRT_DaylightGISurfaceAlbedo( int clip );
ITexture *GetDefRT_DaylightGISurfaceNormal( int clip );
ITexture *GetDefRT_DaylightGIGeometry( int clip );
ITexture *GetDefRT_DaylightGIBlockerField( int clip );
ITexture *GetDefRT_DaylightGISurfaceGuide( int clip );
ITexture *GetDefRT_DaylightGISurfaceCache( int clip );
ITexture *GetDefRT_DaylightGIOpenSky( int clip );
void UpdateDefRT_DaylightGIGeometry( int clip, const unsigned char *pData, int nDataSize );
void UpdateDefRT_DaylightGIBlockerField( int clip, const unsigned char *pData, int nDataSize );
void UpdateDefRT_DaylightGISurfaceGuide( int clip, const unsigned char *pData, int nDataSize );
void UpdateDefRT_DaylightGISurfaceCache( int clip, const unsigned char *pData, int nDataSize );
void UpdateDefRT_DaylightGIOpenSky( int clip, const unsigned char *pData, int nDataSize );

ITexture *GetDefRT_DaylightGIIndirectHalf();

int GetShadowResolution_Spot();
int GetShadowResolution_Point();

#if DEFCFG_ADAPTIVE_SHADOWMAP_LOD
int GetShadowResolution_Spot_LOD1();
int GetShadowResolution_Spot_LOD2();


int GetShadowResolution_Point_LOD1();
int GetShadowResolution_Point_LOD2();
#endif

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

ITexture *GetShadowColorRT_Ortho( int index );
ITexture *GetShadowDepthRT_Ortho( int index );

ITexture *GetShadowColorRT_Proj( int index );
ITexture *GetShadowDepthRT_Proj( int index );

ITexture *GetShadowColorRT_DP( int index );
ITexture *GetShadowDepthRT_DP( int index );

ITexture *GetProjectableVguiRT( int index );

#endif
