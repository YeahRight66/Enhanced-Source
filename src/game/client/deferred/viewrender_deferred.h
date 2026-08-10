#ifndef VIEWRENDER_DEFERRED_H
#define VIEWRENDER_DEFERRED_H

#include "viewrender.h"

#include "deferred/deferred_shared_common.h"
#include "../../../materialsystem/deferredshaders/radiance_hints_config.h"
#include "tier1/utlvector.h"


class CDeferredViewRender : public CViewRender
{
	DECLARE_CLASS( CDeferredViewRender, CViewRender );

public:
					CDeferredViewRender();
	virtual			~CDeferredViewRender( void ) {}

	virtual void	Init( void );
	virtual void	Shutdown( void );

	virtual void	RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw );

public:

	void			LevelInit( void );
	void			LevelShutdown( void );

	void			ViewDrawSceneDeferred( const CViewSetup &view, int nClearFlags, view_id_t viewID,
		bool bDrawViewModel );

	void			ViewDrawGBuffer( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
		bool bDrawViewModel );
	void			ViewDrawComposite( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
		int nClearFlags, view_id_t viewID, bool bDrawViewModel );

	void			ViewCombineDeferredShading( const CViewSetup &view, view_id_t viewID );
	void			ViewOutputDeferredShading( const CViewSetup &view );

	void			DrawSkyboxComposite( const CViewSetup &view, const bool &bDrew3dSkybox );
	void			DrawWorldComposite( const CViewSetup &view, int nClearFlags, bool bDrawSkybox );

	void			DrawLightShadowView( const CViewSetup &view, int iDesiredShadowmap, def_light_t *l );

protected:

	void			DrawViewModels( const CViewSetup &view, bool drawViewmodel, bool bGBuffer );


private:

	void ProcessDeferredGlobals( const CViewSetup &view );

	void PerformLighting( const CViewSetup &view );

	void BeginRadiosity( const CViewSetup &view );
	void UpdateRadiosityPosition();
	void RenderRadianceHintsRSM( const CViewSetup &view );
	void PerformRadiosityGlobal( int clip );
	void PerformRadiositySky( int clip );
	void PerformRadiositySurface( int clip );
	void PerformRadiosityFilter( int clip );
	void UpdateRadiosityGeometry( int clip );
	unsigned char BuildRadiosityStaticCell( const Vector &cellCenter, float cellSize,
		unsigned char *pSurfaceRGBA, unsigned char *pSurfaceGuideRGBA ) const;
	void StampRadiosityDynamicModels( int clip, const Vector &origin, float cellSize );
	void UpdateRadiosityShadowGeometry( int clip );
	void BuildRadiosityShadowDistanceField( int clip );
	void BuildRadiosityStaticBlockerField( int clip );
	void BuildRadiositySurfaceGuide( int clip );
	void UpdateRadiosityOpenSky( int clip );
	unsigned char BuildRadiosityOpenSkyCell( const Vector &cellCenter ) const;
	unsigned char BuildRadiosityShadowStaticCell( const Vector &cellCenter, float cellSize,
		unsigned char *pTraceNormal ) const;
	void StampRadiosityShadowDynamicModels( int clip, const Vector &origin, float cellSize );
	void EndRadiosity( const CViewSetup &view );
	void DebugRadiosity( const CViewSetup &view );

	void RenderCascadedShadows( const CViewSetup &view );

	IMesh *GetRadianceHintsVolumeMesh();
	IMesh *CreateRadianceHintsVolumeMesh();
	IMesh *CreateRadianceHintsVolumeMeshSize( int volumeSize );

	Vector m_vecRadiosityOrigin[2];
	IMesh *m_pMesh_RadianceHintsVolume;
	bool m_bRadianceHintsInjected;
	bool m_bRadianceHintsOriginValid[ RH_CLIP_LEVEL_COUNT ];
	float m_flRadianceHintsCellSize[ RH_CLIP_LEVEL_COUNT ];
	float m_flGICacheUpdateMilliseconds;
	int m_nGICoarseCellsRebuilt[ RH_CLIP_LEVEL_COUNT ];
	int m_nGIFineCellsRebuilt[ RH_CLIP_LEVEL_COUNT ];
	int m_nGIFineCellsTraced[ RH_CLIP_LEVEL_COUNT ];
	int m_nGISkyCellsRebuilt[ RH_CLIP_LEVEL_COUNT ];
	int m_nGIDynamicBlockers[ RH_CLIP_LEVEL_COUNT ];

	Vector m_vecRHGeometryOrigin[ RH_CLIP_LEVEL_COUNT ];
	float m_flRHGeometryCellSize[ RH_CLIP_LEVEL_COUNT ];
	bool m_bRHGeometryValid[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHStaticGeometry[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHCombinedGeometry[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHStaticSurfaceCache[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHSurfaceCacheScratch[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHStaticSurfaceGuide[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHSurfaceGuideScratch[ RH_CLIP_LEVEL_COUNT ];

	Vector m_vecRHShadowGeometryOrigin[ RH_CLIP_LEVEL_COUNT ];
	float m_flRHShadowGeometryCellSize[ RH_CLIP_LEVEL_COUNT ];
	bool m_bRHShadowGeometryValid[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowStaticGeometry[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowCombinedGeometry[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowGeometryDistance[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowCombinedDistance[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowStaticTraceNormal[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowTraceNormalScratch[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowStaticBlockerField[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHShadowCombinedBlockerField[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHSurfaceGuide[ RH_CLIP_LEVEL_COUNT ];

	Vector m_vecRHOpenSkyOrigin[ RH_CLIP_LEVEL_COUNT ];
	float m_flRHOpenSkyCellSize[ RH_CLIP_LEVEL_COUNT ];
	bool m_bRHOpenSkyValid[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHOpenSky[ RH_CLIP_LEVEL_COUNT ];
	CUtlVector< unsigned char > m_RHOpenSkyScratch[ RH_CLIP_LEVEL_COUNT ];
};




#endif
