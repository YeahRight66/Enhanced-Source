#ifndef VIEWRENDER_DEFERRED_H
#define VIEWRENDER_DEFERRED_H

#include "viewrender.h"

#include "deferred/deferred_shared_common.h"
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
	void PerformRadiosityGlobal();
	void PerformRadiositySky();
	void PerformRadiositySurface();
	void PerformRadiosityVisibility();
	void PerformRadiosityFilter();
	void PerformRadiosityHierarchy( bool bCombined );
	void UpdateRadiosityGeometry();
	void BuildRadiosityDistanceField();
	unsigned char BuildRadiosityStaticCell( const Vector &cellCenter, float cellSize, unsigned char *pSurfaceRGBA ) const;
	void StampRadiosityDynamicModels( const Vector &origin, float cellSize );
	void UpdateRadiosityShadowGeometry();
	void BuildRadiosityShadowDistanceField();
	void BuildRadiositySurfaceGuide();
	void BuildRadiosityShadowHierarchy();
	void UpdateRadiosityOpenSky();
	unsigned char BuildRadiosityOpenSkyCell( const Vector &cellCenter ) const;
	unsigned char BuildRadiosityShadowStaticCell( const Vector &cellCenter, float cellSize ) const;
	void StampRadiosityShadowDynamicModels( const Vector &origin, float cellSize );
	void EndRadiosity( const CViewSetup &view );
	void DebugRadiosity( const CViewSetup &view );

	void RenderCascadedShadows( const CViewSetup &view );

	IMesh *GetRadianceHintsVolumeMesh();
	IMesh *GetRadianceHintsHierarchyMesh( int level );
	IMesh *CreateRadianceHintsVolumeMesh();
	IMesh *CreateRadianceHintsVolumeMeshSize( int volumeSize );

	Vector m_vecRadiosityOrigin[2];
	IMesh *m_pMesh_RadianceHintsVolume;
	IMesh *m_pMesh_RadianceHintsHierarchy[3];
	bool m_bRadianceHintsInjected;
	bool m_bRadianceHintsOriginValid;
	float m_flRadianceHintsCellSize;

	Vector m_vecRHGeometryOrigin;
	float m_flRHGeometryCellSize;
	bool m_bRHGeometryValid;
	CUtlVector< unsigned char > m_RHStaticGeometry;
	CUtlVector< unsigned char > m_RHCombinedGeometry;
	CUtlVector< unsigned char > m_RHGeometryDistance;
	CUtlVector< unsigned char > m_RHStaticSurfaceCache;
	CUtlVector< unsigned char > m_RHSurfaceCacheScratch;

	Vector m_vecRHShadowGeometryOrigin;
	float m_flRHShadowGeometryCellSize;
	bool m_bRHShadowGeometryValid;
	CUtlVector< unsigned char > m_RHShadowStaticGeometry;
	CUtlVector< unsigned char > m_RHShadowCombinedGeometry;
	CUtlVector< unsigned char > m_RHShadowGeometryDistance;   // static Euclidean SDF
	CUtlVector< unsigned char > m_RHShadowCombinedDistance;   // static SDF + dynamic OBB proximity
	CUtlVector< unsigned char > m_RHSurfaceGuide;
	CUtlVector< unsigned char > m_RHShadowGeometry32;
	CUtlVector< unsigned char > m_RHShadowDistance32;
	CUtlVector< unsigned char > m_RHShadowGeometry16;
	CUtlVector< unsigned char > m_RHShadowDistance16;

	Vector m_vecRHOpenSkyOrigin;
	float m_flRHOpenSkyCellSize;
	bool m_bRHOpenSkyValid;
	CUtlVector< unsigned char > m_RHOpenSky;
	CUtlVector< unsigned char > m_RHOpenSkyScratch;
};




#endif