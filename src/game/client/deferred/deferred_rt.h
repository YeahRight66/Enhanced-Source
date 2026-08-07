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
ITexture *GetDefRT_RHSurfaceAlbedo();
ITexture *GetDefRT_RHSurfaceNormal();
ITexture *GetDefRT_RHVisibility();
ITexture *GetDefRT_RHIndirectHalf();
ITexture *GetDefRT_RHGeometry();
ITexture *GetDefRT_RHGeometryDistance();
ITexture *GetDefRT_RHShadowGeometry();
ITexture *GetDefRT_RHShadowDistance();
ITexture *GetDefRT_RHSurfaceGuide();
ITexture *GetDefRT_RHShadowHalf();
void UpdateDefRT_RHGeometry( const unsigned char *pData, int nDataSize );
void UpdateDefRT_RHGeometryDistance( const unsigned char *pData, int nDataSize );
void UpdateDefRT_RHShadowGeometry( const unsigned char *pData, int nDataSize );
void UpdateDefRT_RHShadowDistance( const unsigned char *pData, int nDataSize );
void UpdateDefRT_RHSurfaceGuide( const unsigned char *pData, int nDataSize );

ITexture *GetDefRT_RadianceHints( int setIndex, int channelIndex );
// Compatibility aliases used by the existing deferred extension/debug code.
ITexture *GetDefRT_RadiosityBuffer( int index );
ITexture *GetDefRT_RadiosityNormal( int index );

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

ITexture *GetRadiosityAlbedoRT_Ortho( int index );
ITexture *GetRadiosityNormalRT_Ortho( int index );

#endif