//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Responsible for drawing the scene
//
//===========================================================================//

#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "model_types.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "viewrender.h"
#include "iclientmode.h"
#include "voice_status.h"
#include "glow_overlay.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/imaterialsystem.h"
#include "texture_group_names.h"
#include "bspflags.h"
#include "DetailObjectSystem.h"
#include "tier0/vprof.h"
#include "tier1/mempool.h"
#include "vstdlib/jobthread.h"
#include "datacache/imdlcache.h"
#include "engine/IEngineTrace.h"
#include "icliententity.h"
#include "cliententitylist.h"
#include "iclientrenderable.h"
#include "mathlib/mathlib.h"
#include "engine/ivmodelinfo.h"
#include "tier0/icommandline.h"
#include "view_scene.h"
#include "particles_ez.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "c_pixel_visibility.h"
#include "precache_register.h"
#include "c_rope.h"
#include "c_effects.h"
#include "smoke_fog_overlay.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "ScreenSpaceEffects.h"
#include "toolframework_client.h"
#include "c_func_reflective_glass.h"
#include "keyvalues.h"
#include "renderparm.h"
#include "modelrendersystem.h"
#include "vgui/ISurface.h"
#include "tier1/callqueue.h"
#include <string.h>


#include "rendertexture.h"
#include "viewpostprocess.h"
#include "viewdebug.h"

#ifdef SHADEREDITOR
#include "shadereditor/shadereditorsystem.h"
#endif
#include "deferred/deferred_shared_common.h"
#include "deferred/daylight_gi_math.h"
#include "../../../materialsystem/deferredshaders/radiance_hints_config.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar r_visocclusion;
extern ConVar vcollide_wireframe;
extern ConVar mat_motion_blur_enabled;
extern ConVar r_depthoverlay;

//-----------------------------------------------------------------------------
// Convars related to controlling rendering
//-----------------------------------------------------------------------------
extern ConVar cl_maxrenderable_dist;

extern ConVar r_entityclips; //FIXME: Nvidia drivers before 81.94 on cards that support user clip planes will have problems with this, require driver update? Detect and disable?

// Matches the version in the engine
extern ConVar r_drawopaqueworld;
extern ConVar r_drawtranslucentworld;
extern ConVar r_3dsky;
extern ConVar r_skybox;
extern ConVar r_drawviewmodel;
extern ConVar r_drawtranslucentrenderables;
extern ConVar r_drawopaquerenderables;

extern ConVar r_flashlightdepth_drawtranslucents;

// FIXME: This is not static because we needed to turn it off for TF2 playtests
extern ConVar r_DrawDetailProps;

extern ConVar r_worldlistcache;

//-----------------------------------------------------------------------------
// Convars related to fog color
//-----------------------------------------------------------------------------
extern void GetFogColor( fogparams_t *pFogParams, float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
extern float GetFogMaxDensity( fogparams_t *pFogParams, bool ignoreOverride = false );
extern bool GetFogEnable( fogparams_t *pFogParams, bool ignoreOverride = false );
extern float GetFogStart( fogparams_t *pFogParams, bool ignoreOverride = false );
extern float GetFogEnd( fogparams_t *pFogParams, bool ignoreOverride = false );
extern float GetSkyboxFogStart( bool ignoreOverride = false );
extern float GetSkyboxFogEnd( bool ignoreOverride = false );
extern float GetSkyboxFogMaxDensity( bool ignoreOverride = false );
extern void GetSkyboxFogColor( float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
// set any of these to use the maps fog
extern ConVar fog_enableskybox;

extern void PositionHudPanels( CUtlVector< vgui::VPANEL > &list, const CViewSetup &view );

//-----------------------------------------------------------------------------
// Water-related convars
//-----------------------------------------------------------------------------
extern ConVar r_debugcheapwater;
#ifndef _X360
extern ConVar r_waterforceexpensive;
#endif
extern ConVar r_waterforcereflectentities;
extern ConVar r_WaterDrawRefraction;
extern ConVar r_WaterDrawReflection;
extern ConVar r_ForceWaterLeaf;
extern ConVar mat_drawwater;
extern ConVar mat_clipz;


//-----------------------------------------------------------------------------
// Other convars
//-----------------------------------------------------------------------------
extern ConVar r_eyewaterepsilon;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
extern int g_CurrentViewID;
extern bool g_bRenderingScreenshot;

//static FrustumCache_t s_FrustumCache;
extern FrustumCache_t *FrustumCache( void );


//-----------------------------------------------------------------------------
// Describes a pruned set of leaves to be rendered this view. Reference counted
// because potentially shared by a number of views
//-----------------------------------------------------------------------------

extern void FlushWorldLists();

void DrawCube( CMeshBuilder &meshBuilder, Vector vecPos, float flRadius, float *pfl2Texcoords )
{
	const float flOffsets[24][3] = {
		-flRadius, -flRadius, -flRadius,
		flRadius, -flRadius, -flRadius,
		flRadius, flRadius, -flRadius,
		-flRadius, flRadius, -flRadius,

		flRadius, flRadius, flRadius,
		flRadius, flRadius, -flRadius,
		flRadius, -flRadius, -flRadius,
		flRadius, -flRadius, flRadius,

		-flRadius, flRadius, flRadius,
		-flRadius, -flRadius, flRadius,
		-flRadius, -flRadius, -flRadius,
		-flRadius, flRadius, -flRadius,

		flRadius, flRadius, flRadius,
		-flRadius, flRadius, flRadius,
		-flRadius, flRadius, -flRadius,
		flRadius, flRadius, -flRadius,

		flRadius, -flRadius, flRadius,
		flRadius, -flRadius, -flRadius,
		-flRadius, -flRadius, -flRadius,
		-flRadius, -flRadius, flRadius,

		flRadius, flRadius, flRadius,
		flRadius, -flRadius, flRadius,
		-flRadius, -flRadius, flRadius,
		-flRadius, flRadius, flRadius,
	};

	for ( int i = 0; i < 24; i++ )
	{
		meshBuilder.Position3f( vecPos.x + flOffsets[i][0],
			vecPos.y + flOffsets[i][1],
			vecPos.z + flOffsets[i][2] );
		meshBuilder.TexCoord2fv( 0, pfl2Texcoords );
		meshBuilder.AdvanceVertex();
	}
}

IMesh *CDeferredViewRender::GetRadianceHintsVolumeMesh()
{
	if ( m_pMesh_RadianceHintsVolume == NULL )
		m_pMesh_RadianceHintsVolume = CreateRadianceHintsVolumeMesh();

	Assert( m_pMesh_RadianceHintsVolume != NULL );
	return m_pMesh_RadianceHintsVolume;
}

IMesh *CDeferredViewRender::CreateRadianceHintsVolumeMesh()
{
    return CreateRadianceHintsVolumeMeshSize( RH_VOLUME_SIZE );
}

IMesh *CDeferredViewRender::CreateRadianceHintsVolumeMeshSize( int volumeSize )
{
	const VertexFormat_t format = VERTEX_POSITION
		| VERTEX_TANGENT_S
		| VERTEX_TEXCOORD_SIZE( 0, 2 );

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->CreateStaticMesh( format, TEXTURE_GROUP_OTHER );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, volumeSize );

	const float sliceWidth = 1.0f / MAX( volumeSize, 1 );
	for ( int z = 0; z < volumeSize; ++z )
	{
		const float u0 = z * sliceWidth;
		const float u1 = ( z + 1 ) * sliceWidth;
		const float x0 = u0 * 2.0f - 1.0f;
		const float x1 = u1 * 2.0f - 1.0f;
		const float zCenter = ( z + 0.5f ) * sliceWidth;

		meshBuilder.Position3f( x0,  1.0f, 0.0f );
		meshBuilder.TangentS3f( 0.0f, 0.0f, zCenter );
		meshBuilder.TexCoord2f( 0, u0, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x1,  1.0f, 0.0f );
		meshBuilder.TangentS3f( 1.0f, 0.0f, zCenter );
		meshBuilder.TexCoord2f( 0, u1, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x1, -1.0f, 0.0f );
		meshBuilder.TangentS3f( 1.0f, 1.0f, zCenter );
		meshBuilder.TexCoord2f( 0, u1, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x0, -1.0f, 0.0f );
		meshBuilder.TangentS3f( 0.0f, 1.0f, zCenter );
		meshBuilder.TexCoord2f( 0, u0, 1.0f );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	return pMesh;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CBaseWorldViewDeferred : public CRendering3dView
{
	DECLARE_CLASS( CBaseWorldViewDeferred, CRendering3dView );
protected:
	CBaseWorldViewDeferred(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	virtual bool	AdjustView( float waterHeight );

	void			DrawSetup( float waterHeight, int flags, float waterZAdjust, int iForceViewLeaf = -1, bool bShadowDepth = false );
	void			DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust, bool bShadowDepth = false );

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	// BUGBUG this causes all sorts of problems
	virtual bool	ShouldCacheLists(){ return false; };

	virtual void	DrawWorldDeferred( float waterZAdjust );
	virtual void	DrawOpaqueRenderablesDeferred( bool bNoDecals );

protected:

	void PushComposite();
	void PopComposite();
};


//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
class CSimpleWorldViewDeferred : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CSimpleWorldViewDeferred, CBaseWorldViewDeferred );
public:
	CSimpleWorldViewDeferred(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

	void			Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void			Draw();

	virtual bool	ShouldCacheLists(){ return true; };

private: 
	VisibleFogVolumeInfo_t m_fogInfo;

};

class CGBufferView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CGBufferView, CBaseWorldViewDeferred );
public:
	CGBufferView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
	}

	void			Setup( const CViewSetup &view, bool bDrewSkybox );
	void			Draw();

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	static void PushGBuffer( bool bInitial, float zScale = 1.0f, bool bClearDepth = true );
	static void PopGBuffer();

private: 
	VisibleFogVolumeInfo_t m_fogInfo;
	bool m_bDrewSkybox;
};

class CSkyboxViewDeferred : public CGBufferView
{
	DECLARE_CLASS( CSkyboxViewDeferred, CRendering3dView );
public:
	CSkyboxViewDeferred(CViewRender *pMainView) : 
		CGBufferView( pMainView ),
		m_pSky3dParams( NULL )
	  {
	  }

	bool			Setup( const CViewSetup &view, bool bGBuffer, SkyboxVisibility_t *pSkyboxVisible );
	void			Draw();

protected:

	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();
	bool			GetSkyboxFogEnable();

	void			Enable3dSkyboxFog( void );
	void			DrawInternal( view_id_t iSkyBoxViewID = VIEW_3DSKY, bool bInvokePreAndPostRender = true, ITexture *pRenderTarget = NULL );

	sky3dparams_t *	PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible );
	sky3dparams_t *m_pSky3dParams;

	bool		m_bGBufferPass;
};

class CPostLightingView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CPostLightingView, CBaseWorldViewDeferred );
public:
	CPostLightingView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
	}

	void			Setup( const CViewSetup &view );
	void			Draw();

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	DrawWorldDeferred( float waterZAdjust );
	virtual void	DrawOpaqueRenderablesDeferred( bool bNoDecals );

	static void		PushDeferredShadingFrameBuffer();
	static void		PopDeferredShadingFrameBuffer();

private: 
	VisibleFogVolumeInfo_t m_fogInfo;
};

abstract_class CBaseShadowView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CBaseShadowView, CBaseWorldViewDeferred );
public:
	CBaseShadowView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
		m_pDepthTexture = NULL;
		m_pDummyTexture = NULL;
		m_pRadAlbedoTexture = NULL;
		m_pRadNormalTexture = NULL;
		m_pRadRawAlbedoTexture = NULL;
		m_bOutputRadiosity = false;
	};

	void			Setup( const CViewSetup &view,
						ITexture *pDepthTexture,
						ITexture *pDummyTexture );
	void			SetupRadiosityTargets(
						ITexture *pFluxTexture,
						ITexture *pNormalTexture,
						ITexture *pRawAlbedoTexture );

	void SetRadiosityOutputEnabled( bool bEnabled );
	void AddVisibilityOrigin( const Vector &visibilityOrigin );

	void			Draw();
	virtual bool	AdjustView( float waterHeight );
	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	CalcShadowView() = 0;
	virtual void	CommitData(){};

	virtual int		GetShadowMode() = 0;

private:

	ITexture *m_pDepthTexture;
	ITexture *m_pDummyTexture;
	ITexture *m_pRadAlbedoTexture;
	ITexture *m_pRadNormalTexture;
	ITexture *m_pRadRawAlbedoTexture;
	ViewCustomVisibility_t shadowVis;

	bool m_bOutputRadiosity;
};

class COrthoShadowView : public CBaseShadowView
{
	DECLARE_CLASS( COrthoShadowView, CBaseShadowView );
public:
	COrthoShadowView(CViewRender *pMainView, const int &index)
		: CBaseShadowView( pMainView )
	{
			iCascadeIndex = index;
	}

	virtual void	CalcShadowView();
	virtual void	CommitData();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_ORTHO;
	};

private:
	int iCascadeIndex;
};

class CRadianceHintsRSMView : public CBaseShadowView
{
	DECLARE_CLASS( CRadianceHintsRSMView, CBaseShadowView );
public:
	CRadianceHintsRSMView( CViewRender *pMainView, const Vector &rhOrigin, float extent, float cellSize )
		: CBaseShadowView( pMainView ), m_vecRHOrigin( rhOrigin ),
		  m_flExtent( extent ), m_flCellSize( cellSize ), m_flRSMWorldSide( extent ) {}

	virtual void CalcShadowView();
	virtual void CommitData();
	virtual int GetShadowMode() { return DEFERRED_SHADOW_MODE_ORTHO; }

private:
	Vector m_vecRHOrigin;
	float m_flExtent;
	float m_flCellSize;
	float m_flRSMWorldSide;
};

class CDualParaboloidShadowView : public CBaseShadowView
{
	DECLARE_CLASS( CDualParaboloidShadowView, CBaseShadowView );
public:
	CDualParaboloidShadowView(CViewRender *pMainView,
		def_light_t *pLight,
		const bool &bSecondary)
		: CBaseShadowView( pMainView )
	{
			m_pLight = pLight;
			m_bSecondary = bSecondary;
	}
	virtual bool	AdjustView( float waterHeight );
	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	CalcShadowView();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_DPSM;
	};

private:
	bool m_bSecondary;
	def_light_t *m_pLight;
};

class CSpotLightShadowView : public CBaseShadowView
{
	DECLARE_CLASS( CSpotLightShadowView, CBaseShadowView );
public:
	CSpotLightShadowView(CViewRender *pMainView,
		def_light_t *pLight, int index )
		: CBaseShadowView( pMainView )
	{
			m_pLight = pLight;
			m_iIndex = index;
	}

	virtual void	CalcShadowView();
	virtual void	CommitData();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_PROJECTED;
	};

private:
	def_light_t *m_pLight;
	int m_iIndex;
};

//-----------------------------------------------------------------------------
// Computes draw flags for the engine to build its world surface lists
//-----------------------------------------------------------------------------
static inline unsigned long BuildEngineDrawWorldListFlags( unsigned nDrawFlags )
{
	unsigned long nEngineFlags = 0;

	if ( ( nDrawFlags & DF_SKIP_WORLD ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY;
	}

	if ( ( nDrawFlags & DF_SKIP_WORLD_DECALS_AND_OVERLAYS ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if ( nDrawFlags & DF_DRAWSKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SKYBOX;
	}

	if ( nDrawFlags & DF_RENDER_ABOVEWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYABOVEWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_UNDERWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYUNDERWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_WATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WATERSURFACE;
	}

	if( nDrawFlags & DF_CLIP_SKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_CLIPSKYBOX;
	}

	if( nDrawFlags & DF_SHADOW_DEPTH_MAP )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SHADOWDEPTH;
		nEngineFlags &= ~DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if( nDrawFlags & DF_RENDER_REFRACTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFRACTION;
	}

	if( nDrawFlags & DF_RENDER_REFLECTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFLECTION;
	}

	return nEngineFlags;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void SetClearColorToFogColor()
{
	unsigned char ucFogColor[3];
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetFogColor( ucFogColor );
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
	{
		// @MULTICORE (toml 8/16/2006): Find a way to not do this twice in eye above water case
		float scale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );
		ucFogColor[0] *= scale;
		ucFogColor[1] *= scale;
		ucFogColor[2] *= scale;
	}
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
}

//-----------------------------------------------------------------------------
// Precache of necessary materials
//-----------------------------------------------------------------------------

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheDeferredPostProcessingEffects )
	//PRECACHE( MATERIAL, "dev/blurfiltery_and_add_nohdr" )
PRECACHE_REGISTER_END( )


//-----------------------------------------------------------------------------
// Methods to set the current view/guard access to view parameters
//-----------------------------------------------------------------------------
extern void AllowCurrentViewAccess( bool allow );
extern bool IsCurrentViewAccessAllowed();

extern void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID, bool bDrawWorldNormal = false, bool bCullFrontFaces = false );

extern view_id_t CurrentViewID();

//-----------------------------------------------------------------------------
// Purpose: Portal views are considered 'Main' views. This function tests a view id 
//			against all view ids used by portal renderables, as well as the main view.
//-----------------------------------------------------------------------------
extern bool IsMainView ( view_id_t id );

extern void FinishCurrentView();


//#if !defined( INFESTED_DLL )
//static CViewRender g_ViewRender;
//IViewRender *GetViewRenderInstance()
//{
//	return &g_ViewRender;
//}
//#endif


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDeferredViewRender::CDeferredViewRender()
{
	m_pMesh_RadianceHintsVolume = NULL;
	m_bRadianceHintsInjected = false;
	m_flGICacheUpdateMilliseconds = 0.0f;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		m_vecRadiosityOrigin[clip].Init();
		m_bRadianceHintsOriginValid[clip] = false;
		m_flRadianceHintsCellSize[clip] = 0.0f;
		m_vecRHGeometryOrigin[clip].Init();
		m_flRHGeometryCellSize[clip] = 0.0f;
		m_bRHGeometryValid[clip] = false;
		m_vecRHShadowGeometryOrigin[clip].Init();
		m_flRHShadowGeometryCellSize[clip] = 0.0f;
		m_bRHShadowGeometryValid[clip] = false;
		m_vecRHOpenSkyOrigin[clip].Init();
		m_flRHOpenSkyCellSize[clip] = 0.0f;
		m_bRHOpenSkyValid[clip] = false;
		m_nGICoarseCellsRebuilt[clip] = 0;
		m_nGIFineCellsRebuilt[clip] = 0;
		m_nGIFineCellsTraced[clip] = 0;
		m_nGISkyCellsRebuilt[clip] = 0;
		m_nGIDynamicBlockers[clip] = 0;
	}
}

void CDeferredViewRender::Init()
{
	BaseClass::Init();
}

void CDeferredViewRender::Shutdown()
{
	if ( m_pMesh_RadianceHintsVolume != NULL )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->DestroyStaticMesh( m_pMesh_RadianceHintsVolume );
		m_pMesh_RadianceHintsVolume = NULL;
	}
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		m_RHStaticGeometry[clip].Purge();
		m_RHCombinedGeometry[clip].Purge();
		m_RHStaticSurfaceCache[clip].Purge();
		m_RHSurfaceCacheScratch[clip].Purge();
		m_RHStaticSurfaceGuide[clip].Purge();
		m_RHSurfaceGuideScratch[clip].Purge();
		m_RHShadowStaticGeometry[clip].Purge();
		m_RHShadowCombinedGeometry[clip].Purge();
		m_RHShadowGeometryDistance[clip].Purge();
		m_RHShadowCombinedDistance[clip].Purge();
		m_RHShadowStaticTraceNormal[clip].Purge();
		m_RHShadowTraceNormalScratch[clip].Purge();
		m_RHShadowStaticBlockerField[clip].Purge();
		m_RHShadowCombinedBlockerField[clip].Purge();
		m_RHSurfaceGuide[clip].Purge();
		m_RHOpenSky[clip].Purge();
		m_RHOpenSkyScratch[clip].Purge();
		m_bRHGeometryValid[clip] = false;
		m_bRHShadowGeometryValid[clip] = false;
		m_bRHOpenSkyValid[clip] = false;
	}

	BaseClass::Shutdown();
}

void CDeferredViewRender::LevelInit()
{
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		m_bRadianceHintsOriginValid[clip] = false;
		m_flRadianceHintsCellSize[clip] = 0.0f;
		m_bRHGeometryValid[clip] = false;
		m_flRHGeometryCellSize[clip] = 0.0f;
		m_bRHShadowGeometryValid[clip] = false;
		m_flRHShadowGeometryCellSize[clip] = 0.0f;
		m_bRHOpenSkyValid[clip] = false;
		m_flRHOpenSkyCellSize[clip] = 0.0f;
		m_RHStaticGeometry[clip].Purge();
		m_RHCombinedGeometry[clip].Purge();
		m_RHStaticSurfaceCache[clip].Purge();
		m_RHSurfaceCacheScratch[clip].Purge();
		m_RHStaticSurfaceGuide[clip].Purge();
		m_RHSurfaceGuideScratch[clip].Purge();
		m_RHShadowStaticGeometry[clip].Purge();
		m_RHShadowCombinedGeometry[clip].Purge();
		m_RHShadowGeometryDistance[clip].Purge();
		m_RHShadowCombinedDistance[clip].Purge();
		m_RHShadowStaticTraceNormal[clip].Purge();
		m_RHShadowTraceNormalScratch[clip].Purge();
		m_RHShadowStaticBlockerField[clip].Purge();
		m_RHShadowCombinedBlockerField[clip].Purge();
		m_RHSurfaceGuide[clip].Purge();
		m_RHOpenSky[clip].Purge();
		m_RHOpenSkyScratch[clip].Purge();
	}
	BaseClass::LevelInit();
}

void CDeferredViewRender::LevelShutdown()
{
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		m_bRadianceHintsOriginValid[clip] = false;
		m_flRadianceHintsCellSize[clip] = 0.0f;
		m_bRHGeometryValid[clip] = false;
		m_flRHGeometryCellSize[clip] = 0.0f;
		m_bRHShadowGeometryValid[clip] = false;
		m_flRHShadowGeometryCellSize[clip] = 0.0f;
		m_bRHOpenSkyValid[clip] = false;
		m_flRHOpenSkyCellSize[clip] = 0.0f;
		m_RHStaticGeometry[clip].Purge();
		m_RHCombinedGeometry[clip].Purge();
		m_RHStaticSurfaceCache[clip].Purge();
		m_RHSurfaceCacheScratch[clip].Purge();
		m_RHStaticSurfaceGuide[clip].Purge();
		m_RHSurfaceGuideScratch[clip].Purge();
		m_RHShadowStaticGeometry[clip].Purge();
		m_RHShadowCombinedGeometry[clip].Purge();
		m_RHShadowGeometryDistance[clip].Purge();
		m_RHShadowCombinedDistance[clip].Purge();
		m_RHShadowStaticTraceNormal[clip].Purge();
		m_RHShadowTraceNormalScratch[clip].Purge();
		m_RHShadowStaticBlockerField[clip].Purge();
		m_RHShadowCombinedBlockerField[clip].Purge();
		m_RHSurfaceGuide[clip].Purge();
		m_RHOpenSky[clip].Purge();
		m_RHOpenSkyScratch[clip].Purge();
	}
	BaseClass::LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Renders world and all entities, etc.
//-----------------------------------------------------------------------------
void CDeferredViewRender::ViewDrawSceneDeferred( const CViewSetup &view, int nClearFlags, view_id_t viewID, bool bDrawViewModel )
{
	VPROF( "CViewRender::ViewDrawScene" );

	bool bDrew3dSkybox = false;
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

	ViewDrawGBuffer( view, bDrew3dSkybox, nSkyboxVisible, bDrawViewModel );

	PerformLighting( view );

#if DEFCFG_DEFERRED_SHADING
	ViewCombineDeferredShading( view, viewID );
#else
	ViewDrawComposite( view, bDrew3dSkybox, nSkyboxVisible, nClearFlags, viewID, bDrawViewModel );
#endif

#if DEFCFG_ENABLE_RADIOSITY
	if ( deferred_gi_debug.GetInt() > 0 || deferred_gi_stats.GetBool() || deferred_radiosity_debug.GetBool() )
		DebugRadiosity( view );
#endif

#if DEFCFG_DEFERRED_SHADING == 1
	CPostLightingView::PushDeferredShadingFrameBuffer();
#endif

#ifdef SHADEREDITOR
	g_ShaderEditorSystem->UpdateSkymask( bDrew3dSkybox, view.x, view.y, view.width, view.height );
#endif

	GetLightingManager()->RenderVolumetrics( view );

	// Disable fog for the rest of the stuff
	DisableFog();

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()

	// Here are the overlays...
	CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );

	// issue the pixel visibility tests
	PixelVisibility_EndCurrentView();

	// Draw rain..
	DrawPrecipitation();

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	clienteffects->DrawEffects( gpGlobals->frametime );	

	// Mark the frame as locked down for client fx additions
	SetFXCreationAllowed( false );

	// Invoke post-render methods
	IGameSystem::PostRenderAllSystems();

#if DEFCFG_DEFERRED_SHADING == 1
	CPostLightingView::PopDeferredShadingFrameBuffer();

	ViewOutputDeferredShading( view );
#endif

	FinishCurrentView();

	// Set int rendering parameters back to defaults
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, 0 );

	if ( view.m_bCullFrontFaces )
	{
		pRenderContext->FlipCulling( false );
	}
}

void CDeferredViewRender::ViewDrawGBuffer( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
	bool bDrawViewModel )
{
	MDLCACHE_CRITICAL_SECTION();

	int oldViewID = g_CurrentViewID;
	g_CurrentViewID = VIEW_DEFERRED_GBUFFER;

	CSkyboxViewDeferred *pSkyView = new CSkyboxViewDeferred( this );
	if ( ( bDrew3dSkybox = pSkyView->Setup( view, true, &nSkyboxVisible ) ) != false )
		AddViewToScene( pSkyView );

	SafeRelease( pSkyView );

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, NULL );

	CRefPtr<CGBufferView> pGBufferView = new CGBufferView( this );
	pGBufferView->Setup( view, bDrew3dSkybox );
	AddViewToScene( pGBufferView );

	DrawViewModels( view, bDrawViewModel, true );

	g_CurrentViewID = oldViewID;
}

void CDeferredViewRender::ViewDrawComposite( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
		int nClearFlags, view_id_t viewID, bool bDrawViewModel )
{
	DrawSkyboxComposite( view, bDrew3dSkybox );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	m_BaseDrawFlags = 0;

	SetupCurrentView( view.origin, view.angles, viewID, view.m_bDrawWorldNormal, view.m_bCullFrontFaces );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, NULL );

	if ( !bDrew3dSkybox && 
		( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) ) //&& ( visFlags & IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS ) )
	{
		// This covers the case where we don't see a 3dskybox, yet radial vis is clipping
		// the far plane.  Need to clear to fog color in this case.
		nClearFlags |= VIEW_CLEAR_COLOR;
		SetClearColorToFogColor( );
	}
	else
		nClearFlags |= VIEW_CLEAR_DEPTH;

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
		drawSkybox = false;

	ParticleMgr()->IncrementFrameCode();

	DrawWorldComposite( view, nClearFlags, drawSkybox );

#ifdef SHADEREDITOR
	VisibleFogVolumeInfo_t fogVolumeInfo;
	render->GetVisibleFogVolume( view.origin, &fogVolumeInfo );
	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );
	g_ShaderEditorSystem->CustomViewRender( &g_CurrentViewID, fogVolumeInfo, info );
#endif

	DrawViewModels( view, bDrawViewModel, false );
}

void CDeferredViewRender::ViewCombineDeferredShading( const CViewSetup &view, view_id_t viewID )
{
#if DEFCFG_DEFERRED_SHADING == 1

	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_SCREENSPACE_SHADING ),
		view.width, view.height );

	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	m_BaseDrawFlags = 0;

	SetupCurrentView( view.origin, view.angles, viewID, view.m_bDrawWorldNormal, view.m_bCullFrontFaces );

	IGameSystem::PreRenderAllSystems();

	ParticleMgr()->IncrementFrameCode();

	MDLCACHE_CRITICAL_SECTION();

	CRefPtr<CPostLightingView> pPostLightingView = new CPostLightingView( this );
	pPostLightingView->Setup( view );
	AddViewToScene( pPostLightingView );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearBuffers( false, true );

#else

#endif
}

void CDeferredViewRender::ViewOutputDeferredShading( const CViewSetup &view )
{
#if DEFCFG_DEFERRED_SHADING

	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_SCREENSPACE_COMBINE ),
		view.width, view.height );

#endif
}

void CDeferredViewRender::DrawSkyboxComposite( const CViewSetup &view, const bool &bDrew3dSkybox )
{
	if ( !bDrew3dSkybox )
		return;

	CSkyboxViewDeferred *pSkyView = new CSkyboxViewDeferred( this );
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;
	if ( pSkyView->Setup( view, false, &nSkyboxVisible ) )
	{
		AddViewToScene( pSkyView );
#ifdef SHADEREDITOR
		g_ShaderEditorSystem->UpdateSkymask( bDrew3dSkybox, view.x, view.y, view.width, view.height );
#endif
	}

	SafeRelease( pSkyView );
	Assert( nSkyboxVisible == SKYBOX_3DSKYBOX_VISIBLE );
}

void CDeferredViewRender::DrawWorldComposite( const CViewSetup &view, int nClearFlags, bool bDrawSkybox )
{
	MDLCACHE_CRITICAL_SECTION();

	VisibleFogVolumeInfo_t fogVolumeInfo;

	render->GetVisibleFogVolume( view.origin, &fogVolumeInfo );

	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );

	CRefPtr<CSimpleWorldViewDeferred> pNoWaterView = new CSimpleWorldViewDeferred( this );
	pNoWaterView->Setup( view, nClearFlags, bDrawSkybox, fogVolumeInfo, info );
	AddViewToScene( pNoWaterView );
}

static lightData_Global_t GetActiveGlobalLightState()
{
	lightData_Global_t data;
	CLightingEditor *pEditor = GetLightingEditor();

	if ( pEditor->IsEditorLightingActive() && pEditor->GetKVGlobalLight() != NULL )
	{
		data = pEditor->GetGlobalState();
	}
	else if ( GetGlobalLight() != NULL )
	{
		data = GetGlobalLight()->GetState();
	}

	return data;
}

void CDeferredViewRender::PerformLighting( const CViewSetup &view )
{
	bool bResetLightAccum = false;
	const bool bRHEnabled = DEFCFG_ENABLE_RADIOSITY != 0 &&
		deferred_gi_enable.GetBool() && deferred_radiosity_enable.GetBool();

	if ( bRHEnabled )
		BeginRadiosity( view );

	if ( GetGlobalLight() != NULL )
	{
		struct defData_setGlobalLightState
		{
			lightData_Global_t state;
			static void Fire( defData_setGlobalLightState d ) { GetDeferredExt()->CommitLightData_Global( d.state ); }
		};

		defData_setGlobalLightState lightDataState;
		lightDataState.state = GetActiveGlobalLightState();

		if ( !GetLightingEditor()->IsEditorLightingActive() && deferred_override_globalLight_enable.GetBool() )
		{
			lightDataState.state.bShadow = deferred_override_globalLight_shadow_enable.GetBool();
			UTIL_StringToVector( lightDataState.state.diff.AsVector3D().Base(), deferred_override_globalLight_diffuse.GetString() );
			UTIL_StringToVector( lightDataState.state.ambh.AsVector3D().Base(), deferred_override_globalLight_ambient_high.GetString() );
			UTIL_StringToVector( lightDataState.state.ambl.AsVector3D().Base(), deferred_override_globalLight_ambient_low.GetString() );
			lightDataState.state.bEnabled = lightDataState.state.diff.LengthSqr() > 0.01f ||
				lightDataState.state.ambh.LengthSqr() > 0.01f || lightDataState.state.ambl.LengthSqr() > 0.01f;
		}

		QUEUE_FIRE( defData_setGlobalLightState, Fire, lightDataState );

		if ( lightDataState.state.bEnabled )
		{
			// RH owns a stable sun-space RSM. It is deliberately rendered before CSM,
			// because both paths temporarily use orthographic shadow constant slot zero.
			if ( bRHEnabled )
				RenderRadianceHintsRSM( view );

			if ( lightDataState.state.bShadow )
			{
				Vector origins[2] = { view.origin, view.origin + lightDataState.state.vecLight.AsVector3D() * 1024.0f };
				render->ViewSetupVis( false, 2, origins );
				RenderCascadedShadows( view );
			}

			// The stock global-light pass adds ambient-high/ambient-low directly and
			// without GI geometry visibility. The sky injection consumed the original
			// colours during PerformRadiositySky(), so commit a direct-light copy with
			// zero ambient before the fullscreen global pass. This avoids double
			// lighting and allows RH sky occlusion to remain visible.
			if ( bRHEnabled && m_bRadianceHintsInjected )
			{
				defData_setGlobalLightState directLightState = lightDataState;
				directLightState.state.ambh.Init();
				directLightState.state.ambl.Init();
				QUEUE_FIRE( defData_setGlobalLightState, Fire, directLightState );
			}
		}
		else
		{
			bResetLightAccum = true;
		}
	}
	else
	{
		bResetLightAccum = true;
	}

	CViewSetup lightingView = view;
	if ( building_cubemaps.GetBool() )
		engine->GetScreenSize( lightingView.width, lightingView.height );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Lightaccum() );

	if ( bResetLightAccum )
	{
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
	}
	else
	{
		DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_GLOBAL ),
			lightingView.width, lightingView.height );
	}

	pRenderContext.SafeRelease();
	GetLightingManager()->RenderLights( lightingView, this );

	if ( bRHEnabled && m_bRadianceHintsInjected )
		EndRadiosity( view );

	pRenderContext.GetFrom( materials );
	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::BeginRadiosity( const CViewSetup &view )
{
	const double cacheUpdateStart = Plat_FloatTime();
	m_bRadianceHintsInjected = false;
	const float clipCellSize[ RH_CLIP_LEVEL_COUNT ] =
	{
		RH_CLIP_NEAR_CELL_SIZE,
		RH_CLIP_FAR_CELL_SIZE
	};

	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		m_nGICoarseCellsRebuilt[clip] = 0;
		m_nGIFineCellsRebuilt[clip] = 0;
		m_nGIFineCellsTraced[clip] = 0;
		m_nGISkyCellsRebuilt[clip] = 0;
		m_nGIDynamicBlockers[clip] = 0;
		const float cellSize = clipCellSize[clip];
		const float extent = cellSize * RH_VOLUME_SIZE;
		Vector desiredOrigin = view.origin - Vector( extent, extent, extent ) * 0.5f;
		for ( int axis = 0; axis < 3; ++axis )
			desiredOrigin[axis] = floor( desiredOrigin[axis] / cellSize ) * cellSize;

		if ( !deferred_gi_freeze.GetBool() || !m_bRadianceHintsOriginValid[clip] )
			m_vecRadiosityOrigin[clip] = desiredOrigin;
		m_bRadianceHintsOriginValid[clip] = true;
		m_flRadianceHintsCellSize[clip] = cellSize;

		UpdateRadiosityGeometry( clip );
		UpdateRadiosityShadowGeometry( clip );
		UpdateRadiosityOpenSky( clip );
	}

	CMatRenderContextPtr pRenderContext( materials );
	const bool bSecondBounce = deferred_gi_quality.GetInt() > 0 &&
		deferred_gi_bounce_intensity.GetFloat() > 0.0f;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		// Injection is additive across stable RSM phases, so only its destination
		// needs a clear. Filter targets are completely overwritten and transport
		// is cleared immediately before its additive XYZ passes.
		for ( int channel = 0; channel < RH_CHANNEL_COUNT; ++channel )
		{
			pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, RH_SET_INJECTION, channel ), NULL,
				0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
			pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
			pRenderContext->PopRenderTargetAndViewport();
		}

		if ( bSecondBounce )
		{
			pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGISurfaceAlbedo( clip ), NULL,
				0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
			pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGISurfaceNormal( clip ) );
			pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
			pRenderContext->PopRenderTargetAndViewport();
		}
	}

	UpdateRadiosityPosition();
	m_flGICacheUpdateMilliseconds = (float)( ( Plat_FloatTime() - cacheUpdateStart ) * 1000.0 );
}


namespace
{
    inline int RHGeometryAtlasIndex( int x, int y, int z )
    {
        return DaylightGIAtlasIndex( x, y, z, RH_VOLUME_SIZE );
    }

    inline int RHClampCellIndex( int value )
    {
        return clamp( value, 0, RH_VOLUME_SIZE - 1 );
    }

    inline int RHShadowAtlasIndex( int x, int y, int z )
    {
        return DaylightGIAtlasIndex( x, y, z, RH_SHADOW_VOLUME_SIZE );
    }

    inline int RHClampShadowCellIndex( int value )
    {
        return clamp( value, 0, RH_SHADOW_VOLUME_SIZE - 1 );
    }

    inline int RHAtlasIndexN( int x, int y, int z, int size )
    {
        return y * size * size + z * size + x;
    }

    void RHEncodeOctNormal( const Vector &inputNormal, unsigned char *pEncoded )
    {
        Vector normal = inputNormal;
        if ( VectorNormalize( normal ) < 0.001f )
            normal.Init( 0.0f, 0.0f, 1.0f );

        const float inverseL1 = 1.0f / MAX(
            fabs( normal.x ) + fabs( normal.y ) + fabs( normal.z ), 1.0e-5f );
        float octX = normal.x * inverseL1;
        float octY = normal.y * inverseL1;
        if ( normal.z < 0.0f )
        {
            const float oldX = octX;
            octX = ( 1.0f - fabs( octY ) ) * ( oldX >= 0.0f ? 1.0f : -1.0f );
            octY = ( 1.0f - fabs( oldX ) ) * ( octY >= 0.0f ? 1.0f : -1.0f );
        }

        pEncoded[0] = (unsigned char)clamp(
            (int)floor( ( octX * 0.5f + 0.5f ) * 255.0f + 0.5f ), 0, 255 );
        pEncoded[1] = (unsigned char)clamp(
            (int)floor( ( octY * 0.5f + 0.5f ) * 255.0f + 0.5f ), 0, 255 );
    }

}

unsigned char CDeferredViewRender::BuildRadiosityStaticCell( const Vector &cellCenter, float cellSize,
    unsigned char *pSurfaceRGBA, unsigned char *pSurfaceGuideRGBA ) const
{
    if ( pSurfaceRGBA != NULL )
        memset( pSurfaceRGBA, 0, 4 );
    if ( pSurfaceGuideRGBA != NULL )
    {
        pSurfaceGuideRGBA[0] = 128;
        pSurfaceGuideRGBA[1] = 128;
        pSurfaceGuideRGBA[2] = 255;
        pSurfaceGuideRGBA[3] = 0;
    }

    // Full-voxel coarse traces form a conservative broad phase for the 64^3
    // cache. Gapless coverage is required or a thin wall could suppress every
    // fine trace in the cell that actually contains it.
    const float halfExtent = cellSize * 0.499f;
    const Vector hullMins( -halfExtent, -halfExtent, -halfExtent );
    const Vector hullMaxs(  halfExtent,  halfExtent,  halfExtent );
    const Vector epsilon( 0.0f, 0.0f, MAX( cellSize * 0.0025f, 0.01f ) );

    Ray_t ray;
    ray.Init( cellCenter - epsilon, cellCenter + epsilon, hullMins, hullMaxs );

    CTraceFilterWorldAndPropsOnly filter;
    trace_t trace;
    enginetrace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    const bool occupied = trace.startsolid || trace.allsolid || trace.fraction < 1.0f;
    if ( occupied && pSurfaceGuideRGBA != NULL )
    {
        Vector traceNormal = trace.plane.normal;
        if ( VectorNormalize( traceNormal ) > 0.001f )
        {
            for ( int axis = 0; axis < 3; ++axis )
            {
                const int encoded = (int)floor( ( traceNormal[axis] * 0.5f + 0.5f ) * 255.0f + 0.5f );
                pSurfaceGuideRGBA[axis] = (unsigned char)clamp( encoded, 0, 255 );
            }
            pSurfaceGuideRGBA[3] = 255;
        }
    }

    if ( occupied && pSurfaceRGBA != NULL &&
         trace.surface.name != NULL && trace.surface.name[0] != '\0' )
    {
        IMaterial *pMaterial = materials->FindMaterial( trace.surface.name, TEXTURE_GROUP_WORLD, false );
        if ( pMaterial == NULL || pMaterial->IsErrorMaterial() )
            pMaterial = materials->FindMaterial( trace.surface.name, TEXTURE_GROUP_MODEL, false );

        if ( pMaterial != NULL && !pMaterial->IsErrorMaterial() && !pMaterial->IsTranslucent() )
        {
            float color[3] = { 0.52f, 0.52f, 0.52f };
            float modulation[3] = { 1.0f, 1.0f, 1.0f };
            // Engine traces do not expose brush texture coordinates here. A
            // stable world-space sample still preserves coarse material colour
            // variation and avoids every cached cell reading the texture centre.
            float sampleU = fabs( cellCenter.x * ( 1.0f / 256.0f ) );
            float sampleV = fabs( cellCenter.y * ( 1.0f / 256.0f ) );
            sampleU -= floor( sampleU );
            sampleV -= floor( sampleV );
            pMaterial->GetLowResColorSample( sampleU, sampleV, color );
            pMaterial->GetColorModulation( &modulation[0], &modulation[1], &modulation[2] );
            for ( int c = 0; c < 3; ++c )
            {
                const float value = clamp( color[c] * modulation[c], 0.0f, 1.0f );
                pSurfaceRGBA[c] = (unsigned char)clamp( (int)floor( value * 255.0f + 0.5f ), 0, 255 );
            }
            pSurfaceRGBA[3] = 255;
        }
    }

    return occupied ? 255 : 0;
}

void CDeferredViewRender::StampRadiosityDynamicModels( int clip, const Vector &origin, float cellSize )
{
    CClientEntityList *pEntityList = cl_entitylist;
    if ( !deferred_gi_dynamic_blockers.GetBool() || pEntityList == NULL )
        return;

    const float conservativeRadius = cellSize * 0.8660254f;
    const int highestEntity = pEntityList->GetHighestEntityIndex();

    for ( int entityIndex = 1; entityIndex <= highestEntity; ++entityIndex )
    {
        IClientEntity *pEntity = pEntityList->GetClientEntity( entityIndex );
        IClientRenderable *pRenderable = pEntity != NULL ? pEntity->GetClientRenderable() : NULL;
        if ( pRenderable == NULL || !pRenderable->ShouldDraw() )
            continue;

        const model_t *pModel = pRenderable->GetModel();
        if ( pModel == NULL )
            continue;
        const modtype_t modelType = modelinfo->GetModelType( pModel );
        if ( modelType != mod_studio && modelType != mod_brush )
            continue;

        Vector worldMins, worldMaxs;
        pRenderable->GetRenderBoundsWorldspace( worldMins, worldMaxs );

        int minCell[3];
        int maxCell[3];
        bool overlapsVolume = true;
        for ( int axis = 0; axis < 3; ++axis )
        {
            minCell[axis] = (int)floor( ( worldMins[axis] - origin[axis] ) / cellSize ) - 1;
            maxCell[axis] = (int)floor( ( worldMaxs[axis] - origin[axis] ) / cellSize ) + 1;
            if ( maxCell[axis] < 0 || minCell[axis] >= RH_VOLUME_SIZE )
                overlapsVolume = false;
            minCell[axis] = RHClampCellIndex( minCell[axis] );
            maxCell[axis] = RHClampCellIndex( maxCell[axis] );
        }
        if ( !overlapsVolume )
            continue;

        Vector localMins, localMaxs;
        pRenderable->GetRenderBounds( localMins, localMaxs );
        localMins -= Vector( conservativeRadius, conservativeRadius, conservativeRadius );
        localMaxs += Vector( conservativeRadius, conservativeRadius, conservativeRadius );
        const matrix3x4_t &renderToWorld = pRenderable->RenderableToWorldTransform();

        for ( int z = minCell[2]; z <= maxCell[2]; ++z )
        for ( int y = minCell[1]; y <= maxCell[1]; ++y )
        for ( int x = minCell[0]; x <= maxCell[0]; ++x )
        {
            const Vector cellCenter = origin + Vector(
                ( x + 0.5f ) * cellSize,
                ( y + 0.5f ) * cellSize,
                ( z + 0.5f ) * cellSize );

            Vector localCenter;
            VectorITransform( cellCenter, renderToWorld, localCenter );
            const bool inside =
                localCenter.x >= localMins.x && localCenter.x <= localMaxs.x &&
                localCenter.y >= localMins.y && localCenter.y <= localMaxs.y &&
                localCenter.z >= localMins.z && localCenter.z <= localMaxs.z;

            if ( inside )
                m_RHCombinedGeometry[clip][ RHGeometryAtlasIndex( x, y, z ) ] = 255;
        }
    }
}

void CDeferredViewRender::UpdateRadiosityGeometry( int clip )
{
    const int cellCount = RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT;
    const float cellSize = MAX( m_flRadianceHintsCellSize[clip], 1.0f );
    const Vector origin = m_vecRadiosityOrigin[clip];

    if ( m_RHStaticGeometry[clip].Count() != cellCount )
        m_RHStaticGeometry[clip].SetCount( cellCount );
    if ( m_RHCombinedGeometry[clip].Count() != cellCount )
        m_RHCombinedGeometry[clip].SetCount( cellCount );
    const int surfaceBytes = cellCount * 4;
    if ( m_RHStaticSurfaceCache[clip].Count() != surfaceBytes ) m_RHStaticSurfaceCache[clip].SetCount( surfaceBytes );
    if ( m_RHSurfaceCacheScratch[clip].Count() != surfaceBytes ) m_RHSurfaceCacheScratch[clip].SetCount( surfaceBytes );
    if ( m_RHStaticSurfaceGuide[clip].Count() != surfaceBytes ) m_RHStaticSurfaceGuide[clip].SetCount( surfaceBytes );
    if ( m_RHSurfaceGuideScratch[clip].Count() != surfaceBytes ) m_RHSurfaceGuideScratch[clip].SetCount( surfaceBytes );

    const bool sameCellSize = m_bRHGeometryValid[clip] && fabs( m_flRHGeometryCellSize[clip] - cellSize ) <= 0.01f;
    int shift[3] = { RH_VOLUME_SIZE, RH_VOLUME_SIZE, RH_VOLUME_SIZE };
    bool integerShift = sameCellSize;
    if ( sameCellSize )
    {
        for ( int axis = 0; axis < 3; ++axis )
        {
            const float deltaCells = ( origin[axis] - m_vecRHGeometryOrigin[clip][axis] ) / cellSize;
            shift[axis] = (int)( deltaCells >= 0.0f ? floor( deltaCells + 0.5f ) : ceil( deltaCells - 0.5f ) );
            if ( fabs( deltaCells - shift[axis] ) > 0.01f )
                integerShift = false;
        }
    }

    const bool originChanged = !m_bRHGeometryValid[clip] || !sameCellSize ||
        origin.DistToSqr( m_vecRHGeometryOrigin[clip] ) > 0.01f;

    if ( originChanged )
    {
        // Preserve the previous static field as a read-only source while the
        // new camera-aligned atlas is rebuilt. Cells that remain at the same
        // world location are copied; only newly exposed slabs are traced.
        memcpy( m_RHCombinedGeometry[clip].Base(), m_RHStaticGeometry[clip].Base(), cellCount );
        memcpy( m_RHSurfaceCacheScratch[clip].Base(), m_RHStaticSurfaceCache[clip].Base(), surfaceBytes );
        memcpy( m_RHSurfaceGuideScratch[clip].Base(), m_RHStaticSurfaceGuide[clip].Base(), surfaceBytes );

        for ( int z = 0; z < RH_VOLUME_SIZE; ++z )
        for ( int y = 0; y < RH_VOLUME_SIZE; ++y )
        for ( int x = 0; x < RH_VOLUME_SIZE; ++x )
        {
            const int newIndex = RHGeometryAtlasIndex( x, y, z );
            const int oldX = x + shift[0];
            const int oldY = y + shift[1];
            const int oldZ = z + shift[2];
            const bool reusable = integerShift &&
                oldX >= 0 && oldX < RH_VOLUME_SIZE &&
                oldY >= 0 && oldY < RH_VOLUME_SIZE &&
                oldZ >= 0 && oldZ < RH_VOLUME_SIZE;

            if ( reusable )
            {
                const int oldIndex = RHGeometryAtlasIndex( oldX, oldY, oldZ );
                m_RHStaticGeometry[clip][newIndex] = m_RHCombinedGeometry[clip][ oldIndex ];
                memcpy( m_RHStaticSurfaceCache[clip].Base() + newIndex * 4,
                    m_RHSurfaceCacheScratch[clip].Base() + oldIndex * 4, 4 );
                memcpy( m_RHStaticSurfaceGuide[clip].Base() + newIndex * 4,
                    m_RHSurfaceGuideScratch[clip].Base() + oldIndex * 4, 4 );
            }
            else
            {
				++m_nGICoarseCellsRebuilt[clip];
                const Vector cellCenter = origin + Vector(
                    ( x + 0.5f ) * cellSize,
                    ( y + 0.5f ) * cellSize,
                    ( z + 0.5f ) * cellSize );
                m_RHStaticGeometry[clip][newIndex] = BuildRadiosityStaticCell( cellCenter, cellSize,
                    m_RHStaticSurfaceCache[clip].Base() + newIndex * 4,
                    m_RHStaticSurfaceGuide[clip].Base() + newIndex * 4 );
            }
        }

        m_vecRHGeometryOrigin[clip] = origin;
        m_flRHGeometryCellSize[clip] = cellSize;
        m_bRHGeometryValid[clip] = true;
    }

    memcpy( m_RHCombinedGeometry[clip].Base(), m_RHStaticGeometry[clip].Base(), cellCount );
    StampRadiosityDynamicModels( clip, origin, cellSize );
    UpdateDefRT_DaylightGIGeometry( clip, m_RHCombinedGeometry[clip].Base(), cellCount );
    UpdateDefRT_DaylightGISurfaceCache( clip, m_RHStaticSurfaceCache[clip].Base(), surfaceBytes );
}

unsigned char CDeferredViewRender::BuildRadiosityShadowStaticCell( const Vector &cellCenter, float cellSize,
    unsigned char *pTraceNormal ) const
{
    if ( pTraceNormal != NULL )
    {
        pTraceNormal[0] = 128; pTraceNormal[1] = 128;
        pTraceNormal[2] = 0; pTraceNormal[3] = 0;
    }
    // The blocker grid must cover the complete voxel. Leaving the legacy 16%
    // gap between neighbouring trace hulls allowed thin BSP walls to fall
    // between far-clip samples. A tiny epsilon avoids ambiguous shared-face
    // contacts while still providing effectively gapless conservative coverage.
    const float halfExtent = cellSize * 0.499f;
    const Vector hullMins( -halfExtent, -halfExtent, -halfExtent );
    const Vector hullMaxs(  halfExtent,  halfExtent,  halfExtent );
    const Vector epsilon( 0.0f, 0.0f, MAX( cellSize * 0.0025f, 0.01f ) );
    Ray_t ray;
    ray.Init( cellCenter - epsilon, cellCenter + epsilon, hullMins, hullMaxs );
    CTraceFilterWorldAndPropsOnly filter;
    trace_t trace;
    enginetrace->TraceRay( ray, MASK_SOLID, &filter, &trace );
    if ( !( trace.startsolid || trace.allsolid || trace.fraction < 1.0f ) )
        return 0;

    Vector traceNormal = trace.plane.normal;
    if ( pTraceNormal != NULL && VectorNormalize( traceNormal ) > 0.001f )
    {
        RHEncodeOctNormal( traceNormal, pTraceNormal );
        pTraceNormal[2] = 255;
    }

    // Alpha-tested leaves and fences block a fraction of the transport ray.
    // Opaque BSP/prop surfaces remain binary, which keeps the EDT topology
    // robust while the packed field carries material transmittance separately.
    if ( trace.surface.name != NULL && trace.surface.name[0] != '\0' )
    {
        IMaterial *pMaterial = materials->FindMaterial( trace.surface.name, TEXTURE_GROUP_WORLD, false );
        if ( pMaterial == NULL || pMaterial->IsErrorMaterial() )
            pMaterial = materials->FindMaterial( trace.surface.name, TEXTURE_GROUP_MODEL, false );
        if ( pMaterial != NULL && !pMaterial->IsErrorMaterial() && pMaterial->IsAlphaTested() )
            return 160;
    }
    return 255;
}

void CDeferredViewRender::StampRadiosityShadowDynamicModels( int clip, const Vector &origin, float cellSize )
{
    CClientEntityList *pEntityList = cl_entitylist;
    if ( !deferred_gi_dynamic_blockers.GetBool() || pEntityList == NULL )
        return;

    // The static Euclidean SDF does not contain moving studio models. Sampling
    // dynamic occupancy alone can sphere-trace straight over a model
    // when the static SDF returned a long step.  Build a cheap OBB-distance
    // overlay while we already visit dynamic model bounds; this avoids a full
    // 64^3 EDT every frame and keeps dynamic blockers visible to SDF stepping.
    const float conservativeRadius = cellSize * 0.8660254f;
    const float dynamicDistanceCells = 3.0f;
    const float dynamicInfluenceWorld = dynamicDistanceCells * cellSize;

    const int highestEntity = pEntityList->GetHighestEntityIndex();
    for ( int entityIndex = 1; entityIndex <= highestEntity; ++entityIndex )
    {
        IClientEntity *pEntity = pEntityList->GetClientEntity( entityIndex );
        IClientRenderable *pRenderable = pEntity != NULL ? pEntity->GetClientRenderable() : NULL;
        if ( pRenderable == NULL || !pRenderable->ShouldDraw() )
            continue;
        const model_t *pModel = pRenderable->GetModel();
        if ( pModel == NULL )
            continue;
        const modtype_t modelType = modelinfo->GetModelType( pModel );
        if ( modelType != mod_studio && modelType != mod_brush )
            continue;

        Vector worldMins, worldMaxs;
        pRenderable->GetRenderBoundsWorldspace( worldMins, worldMaxs );
        const float worldExpansion = conservativeRadius + dynamicInfluenceWorld;
        worldMins -= Vector( worldExpansion, worldExpansion, worldExpansion );
        worldMaxs += Vector( worldExpansion, worldExpansion, worldExpansion );

        int minCell[3], maxCell[3];
        bool overlaps = true;
        for ( int axis = 0; axis < 3; ++axis )
        {
            minCell[axis] = (int)floor( ( worldMins[axis] - origin[axis] ) / cellSize ) - 1;
            maxCell[axis] = (int)floor( ( worldMaxs[axis] - origin[axis] ) / cellSize ) + 1;
            if ( maxCell[axis] < 0 || minCell[axis] >= RH_SHADOW_VOLUME_SIZE ) overlaps = false;
            minCell[axis] = RHClampShadowCellIndex( minCell[axis] );
            maxCell[axis] = RHClampShadowCellIndex( maxCell[axis] );
        }
        if ( !overlaps ) continue;
		++m_nGIDynamicBlockers[clip];

        Vector localMins, localMaxs;
        pRenderable->GetRenderBounds( localMins, localMaxs );
        localMins -= Vector( conservativeRadius, conservativeRadius, conservativeRadius );
        localMaxs += Vector( conservativeRadius, conservativeRadius, conservativeRadius );
        const matrix3x4_t &renderToWorld = pRenderable->RenderableToWorldTransform();

        for ( int z = minCell[2]; z <= maxCell[2]; ++z )
        for ( int y = minCell[1]; y <= maxCell[1]; ++y )
        for ( int x = minCell[0]; x <= maxCell[0]; ++x )
        {
            const int atlasIndex = RHShadowAtlasIndex( x, y, z );
            const Vector center = origin + Vector(
                ( x + 0.5f ) * cellSize, ( y + 0.5f ) * cellSize, ( z + 0.5f ) * cellSize );

            Vector localCenter;
            VectorITransform( center, renderToWorld, localCenter );

            Vector closestLocal(
                clamp( localCenter.x, localMins.x, localMaxs.x ),
                clamp( localCenter.y, localMins.y, localMaxs.y ),
                clamp( localCenter.z, localMins.z, localMaxs.z ) );
            Vector localNormal = localCenter - closestLocal;
            const float distanceWorld = localNormal.Length();
            const bool inside = distanceWorld <= 1.0e-4f;
            if ( inside )
            {
                m_RHShadowCombinedGeometry[clip][ atlasIndex ] = 255;

                // Pick the nearest OBB face when the sample lies inside. This
                // supplies stable relocation directions for moving blockers.
                const float faceDistance[6] =
                {
                    localCenter.x - localMins.x, localMaxs.x - localCenter.x,
                    localCenter.y - localMins.y, localMaxs.y - localCenter.y,
                    localCenter.z - localMins.z, localMaxs.z - localCenter.z
                };
                int nearestFace = 0;
                for ( int face = 1; face < 6; ++face )
                    if ( faceDistance[face] < faceDistance[nearestFace] ) nearestFace = face;
                localNormal.Init( 0.0f, 0.0f, 0.0f );
                localNormal[nearestFace / 2] = ( nearestFace & 1 ) ? 1.0f : -1.0f;
            }
            else
            {
                VectorNormalize( localNormal );
            }

            if ( distanceWorld <= dynamicInfluenceWorld )
            {
                const float distanceCells = MIN(
                    distanceWorld / MAX( cellSize, 1.0f ),
                    RH_SHADOW_DISTANCE_MAX_CELLS );
                const unsigned char encoded = DaylightGIEncodeDistance(
                    distanceCells, RH_SHADOW_DISTANCE_MAX_CELLS );
                m_RHShadowCombinedDistance[clip][ atlasIndex ] =
                    MIN( m_RHShadowCombinedDistance[clip][ atlasIndex ], encoded );

                unsigned char *pPacked = m_RHShadowCombinedBlockerField[clip].Base() + atlasIndex * 4;
                if ( encoded <= pPacked[0] )
                {
                    Vector worldNormal;
                    VectorRotate( localNormal, renderToWorld, worldNormal );
                    pPacked[0] = encoded;
                    pPacked[1] = 255;
                    RHEncodeOctNormal( worldNormal, pPacked + 2 );
                }
                if ( inside )
                    pPacked[1] = 255;
            }
        }
    }
}

void CDeferredViewRender::BuildRadiosityShadowDistanceField( int clip )
{
    const int N = RH_SHADOW_VOLUME_SIZE;
    const int count = RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT;

    // Keep the EDT scratch storage alive between grid shifts. Allocating
    // ~2 MiB of float scratch every time the snapped GI origin moves would
    // amplified the visible movement hitch on older Source allocators.
    static CUtlVector<float> s_fieldA;
    static CUtlVector<float> s_fieldB;
    if ( s_fieldA.Count() != count ) s_fieldA.SetCount( count );
    if ( s_fieldB.Count() != count ) s_fieldB.SetCount( count );
    CUtlVector<float> &fieldA = s_fieldA;
    CUtlVector<float> &fieldB = s_fieldB;

    const float farValue = 1.0e7f;
    for ( int i = 0; i < count; ++i )
        fieldA[i] = m_RHShadowStaticGeometry[clip][i] > 0 ? 0.0f : farValue;

    float lineIn[ RH_SHADOW_VOLUME_SIZE ];
    float lineOut[ RH_SHADOW_VOLUME_SIZE ];

    // X transform.
    for ( int z = 0; z < N; ++z )
    for ( int y = 0; y < N; ++y )
    {
        for ( int x = 0; x < N; ++x ) lineIn[x] = fieldA[ RHShadowAtlasIndex( x, y, z ) ];
        DaylightGIDistanceTransform1D<RH_SHADOW_VOLUME_SIZE>( lineIn, lineOut );
        for ( int x = 0; x < N; ++x ) fieldB[ RHShadowAtlasIndex( x, y, z ) ] = lineOut[x];
    }
    // Y transform.
    for ( int z = 0; z < N; ++z )
    for ( int x = 0; x < N; ++x )
    {
        for ( int y = 0; y < N; ++y ) lineIn[y] = fieldB[ RHShadowAtlasIndex( x, y, z ) ];
        DaylightGIDistanceTransform1D<RH_SHADOW_VOLUME_SIZE>( lineIn, lineOut );
        for ( int y = 0; y < N; ++y ) fieldA[ RHShadowAtlasIndex( x, y, z ) ] = lineOut[y];
    }
    // Z transform.
    for ( int y = 0; y < N; ++y )
    for ( int x = 0; x < N; ++x )
    {
        for ( int z = 0; z < N; ++z ) lineIn[z] = fieldA[ RHShadowAtlasIndex( x, y, z ) ];
        DaylightGIDistanceTransform1D<RH_SHADOW_VOLUME_SIZE>( lineIn, lineOut );
        for ( int z = 0; z < N; ++z ) fieldB[ RHShadowAtlasIndex( x, y, z ) ] = lineOut[z];
    }

    if ( m_RHShadowGeometryDistance[clip].Count() != count ) m_RHShadowGeometryDistance[clip].SetCount( count );
    for ( int i = 0; i < count; ++i )
    {
        // The EDT measures centre-to-centre distance. Subtract the voxel
        // half-diagonal to obtain a conservative lower bound to the occupied
        // cube itself, then round down so 8-bit quantisation cannot lengthen a
        // sphere-tracing step through thin geometry.
        const float distanceCells = MIN( DaylightGIConservativeDistanceCells( fieldB[i] ),
            RH_SHADOW_DISTANCE_MAX_CELLS );
        m_RHShadowGeometryDistance[clip][i] = DaylightGIEncodeDistance(
            distanceCells, RH_SHADOW_DISTANCE_MAX_CELLS );
    }
}

void CDeferredViewRender::BuildRadiosityStaticBlockerField( int clip )
{
    const int count = RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT;
    const int fieldBytes = count * 4;
    if ( m_RHShadowStaticBlockerField[clip].Count() != fieldBytes )
        m_RHShadowStaticBlockerField[clip].SetCount( fieldBytes );

    for ( int z = 0; z < RH_SHADOW_VOLUME_SIZE; ++z )
    for ( int y = 0; y < RH_SHADOW_VOLUME_SIZE; ++y )
    for ( int x = 0; x < RH_SHADOW_VOLUME_SIZE; ++x )
    {
        const int x0 = RHClampShadowCellIndex( x - 1 );
        const int x1 = RHClampShadowCellIndex( x + 1 );
        const int y0 = RHClampShadowCellIndex( y - 1 );
        const int y1 = RHClampShadowCellIndex( y + 1 );
        const int z0 = RHClampShadowCellIndex( z - 1 );
        const int z1 = RHClampShadowCellIndex( z + 1 );
        const Vector gradient(
            (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x1, y, z ) ] -
                (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x0, y, z ) ],
            (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x, y1, z ) ] -
                (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x, y0, z ) ],
            (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x, y, z1 ) ] -
                (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( x, y, z0 ) ] );

        const int atlasIndex = RHShadowAtlasIndex( x, y, z );
        int nearestBlockerIndex = atlasIndex;
        unsigned char *pField = m_RHShadowStaticBlockerField[clip].Base() + atlasIndex * 4;
        pField[0] = m_RHShadowGeometryDistance[clip][atlasIndex];
        pField[1] = m_RHShadowStaticGeometry[clip][atlasIndex];

        // Carry the nearest blocker's material opacity through the narrow SDF
        // band. Fixed-count SM3 traces can then use conservative distance
        // without turning alpha-tested surfaces into either holes or slabs.
        const float distanceCells = pField[0] * ( RH_SHADOW_DISTANCE_MAX_CELLS / 255.0f );
        if ( pField[1] == 0 && distanceCells <= 1.5f )
        {
            Vector outward = gradient;
            if ( VectorNormalize( outward ) > 0.001f )
            {
                const float centerDistance = distanceCells + 0.8660254038f +
                    RH_SHADOW_DISTANCE_MAX_CELLS / 255.0f;
                const int nearestX = RHClampShadowCellIndex( (int)floor( x - outward.x * centerDistance + 0.5f ) );
                const int nearestY = RHClampShadowCellIndex( (int)floor( y - outward.y * centerDistance + 0.5f ) );
                const int nearestZ = RHClampShadowCellIndex( (int)floor( z - outward.z * centerDistance + 0.5f ) );
                nearestBlockerIndex = RHShadowAtlasIndex( nearestX, nearestY, nearestZ );
                pField[1] = m_RHShadowStaticGeometry[clip][nearestBlockerIndex];

                if ( pField[1] == 0 )
                {
                    float nearestSquared = 1.0e20f;
                    for ( int oz = -1; oz <= 1; ++oz )
                    for ( int oy = -1; oy <= 1; ++oy )
                    for ( int ox = -1; ox <= 1; ++ox )
                    {
                        const int candidateX = RHClampShadowCellIndex( nearestX + ox );
                        const int candidateY = RHClampShadowCellIndex( nearestY + oy );
                        const int candidateZ = RHClampShadowCellIndex( nearestZ + oz );
                        const int candidateIndex = RHShadowAtlasIndex(
                            candidateX, candidateY, candidateZ );
                        const unsigned char opacity = m_RHShadowStaticGeometry[clip][candidateIndex];
                        const float squared = (float)( ox * ox + oy * oy + oz * oz );
                        if ( opacity != 0 && squared < nearestSquared )
                        {
                            nearestSquared = squared;
                            pField[1] = opacity;
                            nearestBlockerIndex = candidateIndex;
                        }
                    }
                }
            }
        }
        // Reuse the traced plane normal from the nearest occupied cell across
        // the narrow SDF band. The distance gradient is only a fallback when
        // the trace did not provide a stable plane (for example start-solid).
        const unsigned char *pTraceNormal = m_RHShadowStaticTraceNormal[clip].Base() +
            nearestBlockerIndex * 4;
        if ( pTraceNormal[2] != 0 )
        {
            pField[2] = pTraceNormal[0];
            pField[3] = pTraceNormal[1];
        }
        else
        {
            RHEncodeOctNormal( gradient, pField + 2 );
        }
    }
}

void CDeferredViewRender::BuildRadiositySurfaceGuide( int clip )
{
    const int count = RH_ATLAS_WIDTH * RH_ATLAS_HEIGHT;
    if ( m_RHSurfaceGuide[clip].Count() != count * 4 ) m_RHSurfaceGuide[clip].SetCount( count * 4 );

    const float radianceCell = MAX( m_flRadianceHintsCellSize[clip], 1.0f );
    const float shadowCell = MAX( m_flRHShadowGeometryCellSize[clip], 1.0f );
    const Vector radianceOrigin = m_vecRadiosityOrigin[clip];
    const Vector shadowOrigin = m_vecRHShadowGeometryOrigin[clip];

    for ( int z = 0; z < RH_VOLUME_SIZE; ++z )
    for ( int y = 0; y < RH_VOLUME_SIZE; ++y )
    for ( int x = 0; x < RH_VOLUME_SIZE; ++x )
    {
        const Vector worldCenter = radianceOrigin + Vector(
            ( x + 0.5f ) * radianceCell,
            ( y + 0.5f ) * radianceCell,
            ( z + 0.5f ) * radianceCell );

        const int sxUnclamped = (int)floor( ( worldCenter.x - shadowOrigin.x ) / shadowCell );
        const int syUnclamped = (int)floor( ( worldCenter.y - shadowOrigin.y ) / shadowCell );
        const int szUnclamped = (int)floor( ( worldCenter.z - shadowOrigin.z ) / shadowCell );
        const bool insideShadow =
            sxUnclamped >= 0 && sxUnclamped < RH_SHADOW_VOLUME_SIZE &&
            syUnclamped >= 0 && syUnclamped < RH_SHADOW_VOLUME_SIZE &&
            szUnclamped >= 0 && szUnclamped < RH_SHADOW_VOLUME_SIZE;

        const int index = RHGeometryAtlasIndex( x, y, z ) * 4;
        // Prefer the actual collision plane normal captured while tracing this
        // radiance cell. The SDF gradient below is only a fallback for nearby
        // cells or traces that began inside solid and had no reliable plane.
        if ( m_RHStaticSurfaceGuide[clip].Count() == count * 4 &&
             m_RHStaticSurfaceGuide[clip][index + 3] > 0 )
        {
            memcpy( m_RHSurfaceGuide[clip].Base() + index,
                m_RHStaticSurfaceGuide[clip].Base() + index, 4 );
            continue;
        }

        if ( !insideShadow )
        {
            m_RHSurfaceGuide[clip][index + 0] = 128;
            m_RHSurfaceGuide[clip][index + 1] = 128;
            m_RHSurfaceGuide[clip][index + 2] = 255;
            m_RHSurfaceGuide[clip][index + 3] = 0;
            continue;
        }

        const int sx = sxUnclamped;
        const int sy = syUnclamped;
        const int sz = szUnclamped;
        const int sx0 = RHClampShadowCellIndex( sx - 1 ), sx1 = RHClampShadowCellIndex( sx + 1 );
        const int sy0 = RHClampShadowCellIndex( sy - 1 ), sy1 = RHClampShadowCellIndex( sy + 1 );
        const int sz0 = RHClampShadowCellIndex( sz - 1 ), sz1 = RHClampShadowCellIndex( sz + 1 );

        const float dx = (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx1, sy, sz ) ] -
                         (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx0, sy, sz ) ];
        const float dy = (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx, sy1, sz ) ] -
                         (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx, sy0, sz ) ];
        const float dz = (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx, sy, sz1 ) ] -
                         (float)m_RHShadowGeometryDistance[clip][ RHShadowAtlasIndex( sx, sy, sz0 ) ];

        Vector normal( dx, dy, dz );
        const float gradientLength = VectorNormalize( normal );
        if ( gradientLength < 0.001f ) normal.Init( 0.0f, 0.0f, 1.0f );

        const int shadowIndex = RHShadowAtlasIndex( sx, sy, sz );
        const float distanceCells = m_RHShadowGeometryDistance[clip][ shadowIndex ] *
            ( RH_SHADOW_DISTANCE_MAX_CELLS / 255.0f );
        // A zero/ambiguous SDF gradient has no reliable surface orientation.
        // The old fallback normal (+Z) still received coverage in free cells,
        // creating fake horizontal bounce surfaces in narrow/equidistant spaces.
        const float gradientValid = gradientLength > 0.001f ? 1.0f : 0.0f;
        // Keep the fallback cache to a single fine-voxel shell. A thick 2.5-cell
        // shell creates several parallel emitters around one wall and is the
        // main source of over-bright corners after the physical second bounce.
        const float coverage = clamp( ( 1.25f - distanceCells ) / 1.25f, 0.0f, 1.0f ) * gradientValid;

        m_RHSurfaceGuide[clip][index + 0] = (unsigned char)clamp( (int)floor( ( normal.x * 0.5f + 0.5f ) * 255.0f + 0.5f ), 0, 255 );
        m_RHSurfaceGuide[clip][index + 1] = (unsigned char)clamp( (int)floor( ( normal.y * 0.5f + 0.5f ) * 255.0f + 0.5f ), 0, 255 );
        m_RHSurfaceGuide[clip][index + 2] = (unsigned char)clamp( (int)floor( ( normal.z * 0.5f + 0.5f ) * 255.0f + 0.5f ), 0, 255 );
        m_RHSurfaceGuide[clip][index + 3] = (unsigned char)clamp( (int)floor( coverage * 255.0f + 0.5f ), 0, 255 );
    }
}

void CDeferredViewRender::UpdateRadiosityShadowGeometry( int clip )
{
    const int count = RH_SHADOW_ATLAS_WIDTH * RH_SHADOW_ATLAS_HEIGHT;
    const float radianceCell = MAX( m_flRadianceHintsCellSize[clip], 1.0f );
    const float extent = radianceCell * RH_VOLUME_SIZE;
    const float shadowCell = extent / RH_SHADOW_VOLUME_SIZE_F;
    const Vector radianceOrigin = m_vecRadiosityOrigin[clip];

    // The 64^3 cache has an explicit origin and snaps independently to its
    // blocker-cell lattice. This guarantees exact integer slab shifts and keeps
    // the world-to-blocker transform correct for both clip levels.
    Vector desiredShadowOrigin = radianceOrigin;
    for ( int axis = 0; axis < 3; ++axis )
        desiredShadowOrigin[axis] =
            floor( desiredShadowOrigin[axis] / shadowCell + 0.5f ) * shadowCell;

    if ( m_RHShadowStaticGeometry[clip].Count() != count ) m_RHShadowStaticGeometry[clip].SetCount( count );
    if ( m_RHShadowCombinedGeometry[clip].Count() != count ) m_RHShadowCombinedGeometry[clip].SetCount( count );
    if ( m_RHShadowGeometryDistance[clip].Count() != count ) m_RHShadowGeometryDistance[clip].SetCount( count );
    if ( m_RHShadowCombinedDistance[clip].Count() != count ) m_RHShadowCombinedDistance[clip].SetCount( count );
    const int traceNormalBytes = count * 4;
    if ( m_RHShadowStaticTraceNormal[clip].Count() != traceNormalBytes ) m_RHShadowStaticTraceNormal[clip].SetCount( traceNormalBytes );
    if ( m_RHShadowTraceNormalScratch[clip].Count() != traceNormalBytes ) m_RHShadowTraceNormalScratch[clip].SetCount( traceNormalBytes );
    const int blockerBytes = count * 4;
    if ( m_RHShadowStaticBlockerField[clip].Count() != blockerBytes ) m_RHShadowStaticBlockerField[clip].SetCount( blockerBytes );
    if ( m_RHShadowCombinedBlockerField[clip].Count() != blockerBytes ) m_RHShadowCombinedBlockerField[clip].SetCount( blockerBytes );

    const bool sameCellSize = m_bRHShadowGeometryValid[clip] &&
        fabs( m_flRHShadowGeometryCellSize[clip] - shadowCell ) <= 0.01f;
    int shift[3] = { RH_SHADOW_VOLUME_SIZE, RH_SHADOW_VOLUME_SIZE, RH_SHADOW_VOLUME_SIZE };
    bool integerShift = sameCellSize;

    if ( sameCellSize )
    {
        for ( int axis = 0; axis < 3; ++axis )
        {
            const float deltaCells =
                ( desiredShadowOrigin[axis] - m_vecRHShadowGeometryOrigin[clip][axis] ) / shadowCell;
            shift[axis] = (int)( deltaCells >= 0.0f
                ? floor( deltaCells + 0.5f )
                : ceil( deltaCells - 0.5f ) );
            if ( fabs( deltaCells - (float)shift[axis] ) > 0.01f )
                integerShift = false;
        }
    }

    const bool originChanged = !m_bRHShadowGeometryValid[clip] || !sameCellSize ||
        desiredShadowOrigin.DistToSqr( m_vecRHShadowGeometryOrigin[clip] ) > 0.01f;

    if ( originChanged )
    {
        // m_RHShadowCombinedGeometry is scratch here. Copying the previous
        // static cache lets us remap surviving cells without touching the
        // trace system; only newly exposed slabs are queried.
        memcpy( m_RHShadowCombinedGeometry[clip].Base(), m_RHShadowStaticGeometry[clip].Base(), count );
        memcpy( m_RHShadowTraceNormalScratch[clip].Base(),
            m_RHShadowStaticTraceNormal[clip].Base(), traceNormalBytes );

        const bool canReuse = integerShift &&
            shift[0] > -RH_SHADOW_VOLUME_SIZE && shift[0] < RH_SHADOW_VOLUME_SIZE &&
            shift[1] > -RH_SHADOW_VOLUME_SIZE && shift[1] < RH_SHADOW_VOLUME_SIZE &&
            shift[2] > -RH_SHADOW_VOLUME_SIZE && shift[2] < RH_SHADOW_VOLUME_SIZE;

        for ( int z = 0; z < RH_SHADOW_VOLUME_SIZE; ++z )
        for ( int y = 0; y < RH_SHADOW_VOLUME_SIZE; ++y )
        for ( int x = 0; x < RH_SHADOW_VOLUME_SIZE; ++x )
        {
            const int newIndex = RHShadowAtlasIndex( x, y, z );
            const int oldX = x + shift[0];
            const int oldY = y + shift[1];
            const int oldZ = z + shift[2];
            const bool reusable = canReuse &&
                oldX >= 0 && oldX < RH_SHADOW_VOLUME_SIZE &&
                oldY >= 0 && oldY < RH_SHADOW_VOLUME_SIZE &&
                oldZ >= 0 && oldZ < RH_SHADOW_VOLUME_SIZE;

            if ( reusable )
            {
                m_RHShadowStaticGeometry[clip][newIndex] =
                    m_RHShadowCombinedGeometry[clip][ RHShadowAtlasIndex( oldX, oldY, oldZ ) ];
                const int oldIndex = RHShadowAtlasIndex( oldX, oldY, oldZ );
                memcpy( m_RHShadowStaticTraceNormal[clip].Base() + newIndex * 4,
                    m_RHShadowTraceNormalScratch[clip].Base() + oldIndex * 4, 4 );
                continue;
            }
			++m_nGIFineCellsRebuilt[clip];

            const Vector center = desiredShadowOrigin + Vector(
                ( x + 0.5f ) * shadowCell,
                ( y + 0.5f ) * shadowCell,
                ( z + 0.5f ) * shadowCell );

            // The 32^3 cache uses gapless full-voxel hull traces and is therefore
            // a conservative broad phase. It removes the prohibitive 64^3 cold
            // trace cost without reintroducing the old thin-wall holes caused by
            // a sparse/undersized coarse gate.
            const Vector coarseVoxel = DaylightGIWorldToVoxel( center,
                m_vecRHGeometryOrigin[clip], MAX( m_flRHGeometryCellSize[clip], 1.0f ) );
            const int coarseX = (int)floor( coarseVoxel.x );
            const int coarseY = (int)floor( coarseVoxel.y );
            const int coarseZ = (int)floor( coarseVoxel.z );
            const bool insideCoarse = coarseX >= 0 && coarseX < RH_VOLUME_SIZE &&
                coarseY >= 0 && coarseY < RH_VOLUME_SIZE &&
                coarseZ >= 0 && coarseZ < RH_VOLUME_SIZE;
            const bool coarseOccupied = insideCoarse && m_RHStaticGeometry[clip][
                RHGeometryAtlasIndex( coarseX, coarseY, coarseZ ) ] != 0;
            if ( coarseOccupied )
            {
				++m_nGIFineCellsTraced[clip];
                m_RHShadowStaticGeometry[clip][newIndex] =
                    BuildRadiosityShadowStaticCell( center, shadowCell,
                        m_RHShadowStaticTraceNormal[clip].Base() + newIndex * 4 );
            }
            else
            {
                m_RHShadowStaticGeometry[clip][newIndex] = 0;
                unsigned char *pTraceNormal = m_RHShadowStaticTraceNormal[clip].Base() + newIndex * 4;
                pTraceNormal[0] = 128; pTraceNormal[1] = 128;
                pTraceNormal[2] = 0; pTraceNormal[3] = 0;
            }
        }

        m_vecRHShadowGeometryOrigin[clip] = desiredShadowOrigin;
        m_flRHShadowGeometryCellSize[clip] = shadowCell;
        m_bRHShadowGeometryValid[clip] = true;

        // Exact EDT + surface guide are rebuilt only after an actual integer
        // blocker-grid shift, never merely because the 32^3 origin moved.
        BuildRadiosityShadowDistanceField( clip );
        BuildRadiosityStaticBlockerField( clip );
        BuildRadiositySurfaceGuide( clip );

        if ( m_RHSurfaceGuide[clip].Count() > 0 )
            UpdateDefRT_DaylightGISurfaceGuide( clip, m_RHSurfaceGuide[clip].Base(), m_RHSurfaceGuide[clip].Count() );
    }

    memcpy( m_RHShadowCombinedGeometry[clip].Base(), m_RHShadowStaticGeometry[clip].Base(), count );
    memcpy( m_RHShadowCombinedDistance[clip].Base(), m_RHShadowGeometryDistance[clip].Base(), count );
    memcpy( m_RHShadowCombinedBlockerField[clip].Base(),
        m_RHShadowStaticBlockerField[clip].Base(), blockerBytes );
    StampRadiosityShadowDynamicModels( clip, m_vecRHShadowGeometryOrigin[clip], shadowCell );
    UpdateDefRT_DaylightGIBlockerField( clip, m_RHShadowCombinedBlockerField[clip].Base(), blockerBytes );
}

unsigned char CDeferredViewRender::BuildRadiosityOpenSkyCell( const Vector &cellCenter ) const
{
    const float traceDistance = 4096.0f;
    const float h = 0.5f;
    const float z = 0.8660254038f;
    const Vector dirs[5] =
    {
        Vector( 0.0f,  0.0f, 1.0f ),
        Vector( h,     0.0f, z ), Vector( -h, 0.0f, z ),
        Vector( 0.0f,  h,    z ), Vector( 0.0f,-h,    z )
    };

    CTraceFilterWorldAndPropsOnly filter;
    float open = 0.0f;
    for ( int i = 0; i < 5; ++i )
    {
        Ray_t ray;
        ray.Init( cellCenter, cellCenter + dirs[i] * traceDistance );
        trace_t trace;
        enginetrace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        const bool reachesSky = !trace.startsolid && !trace.allsolid &&
            ( trace.fraction >= 0.9999f || ( trace.surface.flags & SURF_SKY ) != 0 );
        open += reachesSky ? 1.0f : 0.0f;
    }
    return (unsigned char)clamp( (int)floor( open * ( 255.0f / 5.0f ) + 0.5f ), 0, 255 );
}

void CDeferredViewRender::UpdateRadiosityOpenSky( int clip )
{
    const int size = RH_SKY_CACHE_SIZE;
    const int count = RH_SKY_CACHE_ATLAS_WIDTH * RH_SKY_CACHE_ATLAS_HEIGHT;
    const float extent = MAX( m_flRadianceHintsCellSize[clip], 1.0f ) * RH_VOLUME_SIZE;
    const float skyCell = extent / RH_SKY_CACHE_SIZE_F;
    Vector desiredOrigin = m_vecRadiosityOrigin[clip];
    for ( int axis = 0; axis < 3; ++axis )
        desiredOrigin[axis] = floor( desiredOrigin[axis] / skyCell + 0.5f ) * skyCell;

    if ( m_RHOpenSky[clip].Count() != count ) m_RHOpenSky[clip].SetCount( count );
    if ( m_RHOpenSkyScratch[clip].Count() != count ) m_RHOpenSkyScratch[clip].SetCount( count );

    const bool sameCell = m_bRHOpenSkyValid[clip] && fabs( m_flRHOpenSkyCellSize[clip] - skyCell ) <= 0.01f;
    int shift[3] = { size, size, size };
    bool integerShift = sameCell;
    if ( sameCell )
    {
        for ( int axis = 0; axis < 3; ++axis )
        {
            const float delta = ( desiredOrigin[axis] - m_vecRHOpenSkyOrigin[clip][axis] ) / skyCell;
            shift[axis] = (int)( delta >= 0.0f ? floor( delta + 0.5f ) : ceil( delta - 0.5f ) );
            if ( fabs( delta - shift[axis] ) > 0.01f ) integerShift = false;
        }
    }

    const bool changed = !m_bRHOpenSkyValid[clip] || !sameCell || desiredOrigin.DistToSqr( m_vecRHOpenSkyOrigin[clip] ) > 0.01f;
    if ( changed )
    {
        memcpy( m_RHOpenSkyScratch[clip].Base(), m_RHOpenSky[clip].Base(), count );
        const bool canReuse = integerShift &&
            shift[0] > -size && shift[0] < size &&
            shift[1] > -size && shift[1] < size &&
            shift[2] > -size && shift[2] < size;

        for ( int z = 0; z < size; ++z )
        for ( int y = 0; y < size; ++y )
        for ( int x = 0; x < size; ++x )
        {
            const int target = RHAtlasIndexN( x, y, z, size );
            const int ox = x + shift[0], oy = y + shift[1], oz = z + shift[2];
            const bool reuse = canReuse && ox >= 0 && ox < size && oy >= 0 && oy < size && oz >= 0 && oz < size;
            if ( reuse )
            {
                m_RHOpenSky[clip][target] = m_RHOpenSkyScratch[clip][ RHAtlasIndexN( ox, oy, oz, size ) ];
            }
            else
            {
				++m_nGISkyCellsRebuilt[clip];
                const Vector center = desiredOrigin + Vector(
                    ( x + 0.5f ) * skyCell, ( y + 0.5f ) * skyCell, ( z + 0.5f ) * skyCell );
                m_RHOpenSky[clip][target] = BuildRadiosityOpenSkyCell( center );
            }
        }

        m_vecRHOpenSkyOrigin[clip] = desiredOrigin;
        m_flRHOpenSkyCellSize[clip] = skyCell;
        m_bRHOpenSkyValid[clip] = true;
    }

    UpdateDefRT_DaylightGIOpenSky( clip, m_RHOpenSky[clip].Base(), count );
}

namespace
{
	struct defData_setDaylightGIClip
	{
		int clip;
		static void Fire( defData_setDaylightGIClip d )
		{
			GetDeferredExt()->CommitDaylightGIActiveClip( d.clip );
		}
	};

	void SetDaylightGIActiveClip( int clip )
	{
		defData_setDaylightGIClip data;
		data.clip = clip;
		QUEUE_FIRE( defData_setDaylightGIClip, Fire, data );
	}
}

void CDeferredViewRender::UpdateRadiosityPosition()
{
	struct defData_setupRadiosity
	{
		daylightGIData_t data;
		static void Fire( defData_setupRadiosity d ) { GetDeferredExt()->CommitDaylightGIData( d.data ); }
	};

	defData_setupRadiosity setup;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		daylightGIClipDesc_t &desc = setup.data.clips[clip];
		desc.vecRadianceOrigin = m_vecRadiosityOrigin[clip];
		desc.vecBlockerOrigin = m_vecRHShadowGeometryOrigin[clip];
		desc.flRadianceCellSize = m_flRadianceHintsCellSize[clip];
		desc.flBlockerCellSize = m_flRHShadowGeometryCellSize[clip];
		desc.flExtent = m_flRadianceHintsCellSize[clip] * RH_VOLUME_SIZE;
		desc.flBlockerExtent = m_flRHShadowGeometryCellSize[clip] * RH_SHADOW_VOLUME_SIZE;
		if ( m_bRHGeometryValid[clip] && m_bRHShadowGeometryValid[clip] )
			setup.data.nValidClipMask |= 1u << clip;
	}
	setup.data.iActiveClip = DAYLIGHT_GI_CLIP_NEAR;
	setup.data.vecRSMParams.w = m_flRadianceHintsCellSize[RH_CLIP_NEAR];
	QUEUE_FIRE( defData_setupRadiosity, Fire, setup );
}

void CDeferredViewRender::RenderRadianceHintsRSM( const CViewSetup &view )
{
	const float cellSize = m_flRadianceHintsCellSize[RH_CLIP_FAR];
	const float extent = cellSize * RH_VOLUME_SIZE;
	const Vector volumeCenter = m_vecRadiosityOrigin[RH_CLIP_FAR] + Vector( extent, extent, extent ) * 0.5f;
	const float visOffset = extent * 0.25f;

	// One stable sun-facing RSM covers the far clip and is reused by both levels.
	CRefPtr<CRadianceHintsRSMView> pRSM = new CRadianceHintsRSMView(
		this, m_vecRadiosityOrigin[RH_CLIP_FAR], extent, cellSize );
	pRSM->Setup( view, GetDefRT_RHRSMDepth(), GetDefRT_RHRSMColor() );
	pRSM->SetupRadiosityTargets( GetDefRT_RHRSMFlux(), GetDefRT_RHRSMNormal(), GetDefRT_RHRSMAlbedo() );
	pRSM->SetRadiosityOutputEnabled( true );
	pRSM->AddVisibilityOrigin( view.origin );
	for ( int sx = -1; sx <= 1; sx += 2 )
	for ( int sy = -1; sy <= 1; sy += 2 )
	for ( int sz = -1; sz <= 1; sz += 2 )
		pRSM->AddVisibilityOrigin( volumeCenter + Vector( sx * visOffset, sy * visOffset, sz * visOffset ) );
	AddViewToScene( pRSM );

	const bool bSecondBounce = deferred_gi_quality.GetInt() > 0 &&
		deferred_gi_bounce_intensity.GetFloat() > 0.0f;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		SetDaylightGIActiveClip( clip );
		PerformRadiosityGlobal( clip );
		PerformRadiositySky( clip );
		if ( bSecondBounce )
			PerformRadiositySurface( clip );
		PerformRadiosityFilter( clip );
	}
	m_bRadianceHintsInjected = true;
}

void CDeferredViewRender::PerformRadiosityGlobal( int clip )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RADIOSITY_CASCADE, 0 );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_R ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_G ) );
	pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_B ) );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_META ) );
	// High quality uses sixteen stable stratified RSM samples per cell, split into
	// four four-sample additive draws so legacy FXC never sees a wide sampler kernel.
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_GLOBAL ) );
	GetRadianceHintsVolumeMesh()->Draw();
#if RH_RADIANCE_SAMPLE_COUNT >= 8
	if ( deferred_gi_quality.GetInt() >= 1 )
	{
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ) );
		GetRadianceHintsVolumeMesh()->Draw();
	}
#endif
#if RH_RADIANCE_SAMPLE_COUNT >= 16
	if ( deferred_gi_quality.GetInt() >= 2 )
	{
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ) );
		GetRadianceHintsVolumeMesh()->Draw();
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ) );
		GetRadianceHintsVolumeMesh()->Draw();
	}
#endif
	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::PerformRadiositySky( int clip )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_R ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_G ) );
	pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_B ) );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_META ) );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_SKY ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_SKY_1 ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::PerformRadiositySurface( int clip )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGISurfaceAlbedo( clip ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGISurfaceNormal( clip ) );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_SURFACE ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::PerformRadiosityFilter( int clip )
{
	CMatRenderContextPtr pRenderContext( materials );

	// Separable X/Y/Z reconstruction is exactly reflection invariant and keeps
	// each ps_3_0 permutation small enough for the legacy Source FXC compiler.
	// Pass X: raw set 0 -> set 1.
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_R ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_G ) );
	pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_B ) );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_META ) );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_FILTER ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->PopRenderTargetAndViewport();

	// Pass Y: set 1 -> set 2.
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_R ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_G ) );
	pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_B ) );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_META ) );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->PopRenderTargetAndViewport();

	// Pass Z: set 2 -> set 0. Set 0 is the final first-bounce field.
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_R ), NULL,
		0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_G ) );
	pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_SH_B ) );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_DaylightGIRadiance( clip, 0, RH_CHANNEL_META ) );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ) );
	GetRadianceHintsVolumeMesh()->Draw();
	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::EndRadiosity( const CViewSetup &view )
{
	const bool bSecondBounce = deferred_gi_quality.GetInt() > 0 &&
		deferred_gi_bounce_intensity.GetFloat() > 0.0f;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT && bSecondBounce; ++clip )
	{
		SetDaylightGIActiveClip( clip );
		CMatRenderContextPtr pRenderContext( materials );

		// Stage 1: convert incoming first-bounce/hemisphere irradiance at RSM
		// surfaces into an outward Lambertian radiance hemisphere.
		pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_R ), NULL,
			0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
		pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_G ) );
		pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 1, RH_CHANNEL_SH_B ) );
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ) );
		GetRadianceHintsVolumeMesh()->Draw();
		pRenderContext->PopRenderTargetAndViewport();

		// Stage 2: X/Y/Z transport is split into three small additive passes.
		// This avoids the legacy FXC crash from one six-direction sampler-heavy shader.
		pRenderContext->PushRenderTargetAndViewport( GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_R ), NULL,
			0, 0, RH_ATLAS_WIDTH, RH_ATLAS_HEIGHT );
		pRenderContext->SetRenderTargetEx( 1, GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_G ) );
		pRenderContext->SetRenderTargetEx( 2, GetDefRT_DaylightGIRadiance( clip, 2, RH_CHANNEL_SH_B ) );
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ) );
		GetRadianceHintsVolumeMesh()->Draw();
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ) );
		GetRadianceHintsVolumeMesh()->Draw();
		pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ) );
		GetRadianceHintsVolumeMesh()->Draw();

		pRenderContext->PopRenderTargetAndViewport();
	}

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pHalf = GetDefRT_DaylightGIIndirectHalf();
	const int halfWidth = MAX( pHalf->GetActualWidth(), 1 );
	const int halfHeight = MAX( pHalf->GetActualHeight(), 1 );
	pRenderContext->PushRenderTargetAndViewport( pHalf, NULL, 0, 0, halfWidth, halfHeight );
	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false );
	pRenderContext->PopRenderTargetAndViewport();

	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		SetDaylightGIActiveClip( clip );
		pRenderContext->PushRenderTargetAndViewport( pHalf, NULL, 0, 0, halfWidth, halfHeight );
		const DEF_MATERIALS receiverMaterial = clip == RH_CLIP_NEAR
			? DEF_MAT_LIGHT_RADIOSITY_BLEND : DEF_MAT_LIGHT_RADIOSITY_BLEND_FAR;
		// Four compact additive permutations reconstruct the same tetrahedral
		// receiver while remaining reliable with Source's d3dx9_33 compiler.
		for ( int probe = 0; probe < 4; ++probe )
		{
			pRenderContext->SetIntRenderingParameter(
				INT_RENDERPARM_DEFERRED_RADIOSITY_CASCADE, probe );
			DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( receiverMaterial ),
				halfWidth, halfHeight );
		}
		pRenderContext->PopRenderTargetAndViewport();
	}
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RADIOSITY_CASCADE, 0 );

	// The caller's _rt_LightAccum target is restored by the pop above.
	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ),
		view.width, view.height );
}

void CDeferredViewRender::DebugRadiosity( const CViewSetup &view )
{
	(void)view;
	uint64 giVideoMemoryBytes = 0;
	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		for ( int setIndex = 0; setIndex < RH_SET_COUNT; ++setIndex )
		for ( int channel = 0; channel < RH_CHANNEL_COUNT; ++channel )
			giVideoMemoryBytes += GetDefRT_DaylightGIRadiance( clip, setIndex, channel )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGISurfaceAlbedo( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGISurfaceNormal( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGIGeometry( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGIBlockerField( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGISurfaceGuide( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGISurfaceCache( clip )->GetApproximateVidMemBytes();
		giVideoMemoryBytes += GetDefRT_DaylightGIOpenSky( clip )->GetApproximateVidMemBytes();
	}
	giVideoMemoryBytes += GetDefRT_RHRSMDepth()->GetApproximateVidMemBytes();
	giVideoMemoryBytes += GetDefRT_RHRSMColor()->GetApproximateVidMemBytes();
	giVideoMemoryBytes += GetDefRT_RHRSMFlux()->GetApproximateVidMemBytes();
	giVideoMemoryBytes += GetDefRT_RHRSMNormal()->GetApproximateVidMemBytes();
	giVideoMemoryBytes += GetDefRT_RHRSMAlbedo()->GetApproximateVidMemBytes();
	giVideoMemoryBytes += GetDefRT_DaylightGIIndirectHalf()->GetApproximateVidMemBytes();

	for ( int clip = 0; clip < RH_CLIP_LEVEL_COUNT; ++clip )
	{
		const float cellSize = m_flRadianceHintsCellSize[clip];
		const float extent = cellSize * RH_VOLUME_SIZE;
		const Vector &origin = m_vecRadiosityOrigin[clip];
		const Vector maximum = origin + Vector( extent, extent, extent );
		DebugDrawCross( origin, cellSize * 0.4f, -1.0f );
		DebugDrawCross( maximum, cellSize * 0.4f, -1.0f );
	}
	engine->Con_NPrintf( 20,
		"Daylight GI | near %g/%g | far %g/%g | SH %i^3 | SDF %i^3 | RSM %i | valid %i",
		m_flRadianceHintsCellSize[RH_CLIP_NEAR], RH_CLIP_NEAR_EXTENT,
		m_flRadianceHintsCellSize[RH_CLIP_FAR], RH_CLIP_FAR_EXTENT,
		RH_VOLUME_SIZE, RH_SHADOW_VOLUME_SIZE, RH_RSM_RESOLUTION,
		m_bRadianceHintsInjected ? 1 : 0 );
	engine->Con_NPrintf( 21,
		"GI cache %.2f ms | rebuilt C/F/T/S near %i/%i/%i/%i far %i/%i/%i/%i | dynamic %i/%i",
		m_flGICacheUpdateMilliseconds,
		m_nGICoarseCellsRebuilt[RH_CLIP_NEAR], m_nGIFineCellsRebuilt[RH_CLIP_NEAR],
		m_nGIFineCellsTraced[RH_CLIP_NEAR], m_nGISkyCellsRebuilt[RH_CLIP_NEAR],
		m_nGICoarseCellsRebuilt[RH_CLIP_FAR], m_nGIFineCellsRebuilt[RH_CLIP_FAR],
		m_nGIFineCellsTraced[RH_CLIP_FAR], m_nGISkyCellsRebuilt[RH_CLIP_FAR],
		m_nGIDynamicBlockers[RH_CLIP_NEAR], m_nGIDynamicBlockers[RH_CLIP_FAR] );
	engine->Con_NPrintf( 22, "GI render-target memory %.2f MiB | quality %i | second bounce %i",
		(double)giVideoMemoryBytes / ( 1024.0 * 1024.0 ),
		deferred_gi_quality.GetInt(),
		deferred_gi_quality.GetInt() > 0 && deferred_gi_bounce_intensity.GetFloat() > 0.0f ? 1 : 0 );
}

void CDeferredViewRender::RenderCascadedShadows( const CViewSetup &view )
{
	for ( int i = 0; i < SHADOW_NUM_CASCADES; ++i )
	{
#if CSM_USE_COMPOSITED_TARGET == 0
		const int textureIndex = i;
#else
		const int textureIndex = 0;
#endif
		CRefPtr<COrthoShadowView> pOrthoDepth = new COrthoShadowView( this, i );
		pOrthoDepth->Setup( view, GetShadowDepthRT_Ortho( textureIndex ), GetShadowColorRT_Ortho( textureIndex ) );
		AddViewToScene( pOrthoDepth );
	}
}


void CDeferredViewRender::DrawLightShadowView( const CViewSetup &view, int iDesiredShadowmap, def_light_t *l )
{
	CViewSetup setup;
	setup.origin = l->pos;
	setup.angles = l->ang;
	setup.m_bOrtho = false;
	setup.m_flAspectRatio = 1;
	setup.x = setup.y = 0;

	Vector origins[2] = { view.origin, l->pos };
	render->ViewSetupVis( false, 2, origins );

	switch ( l->iLighttype )
	{
	default:
		Assert( 0 );
		break;
	case DEFLIGHTTYPE_POINT:
		{
			CRefPtr<CDualParaboloidShadowView> pDPView0 = new CDualParaboloidShadowView( this, l, false );
			pDPView0->Setup( setup, GetShadowDepthRT_DP( iDesiredShadowmap ), GetShadowColorRT_DP( iDesiredShadowmap ) );
			AddViewToScene( pDPView0 );

			CRefPtr<CDualParaboloidShadowView> pDPView1 = new CDualParaboloidShadowView( this, l, true );
			pDPView1->Setup( setup, GetShadowDepthRT_DP( iDesiredShadowmap ), GetShadowColorRT_DP( iDesiredShadowmap ) );
			AddViewToScene( pDPView1 );
		}
		break;
	case DEFLIGHTTYPE_SPOT:
		{
			CRefPtr<CSpotLightShadowView> pProjView = new CSpotLightShadowView( this, l, iDesiredShadowmap );
			
			pProjView->Setup( setup, GetShadowDepthRT_Proj( iDesiredShadowmap ), GetShadowColorRT_Proj( iDesiredShadowmap ) );
			AddViewToScene( pProjView );
		}
		break;
	}
}

void CDeferredViewRender::DrawViewModels( const CViewSetup &view, bool drawViewmodel, bool bGBuffer )
{
	VPROF( "CViewRender::DrawViewModel" );

	bool bShouldDrawPlayerViewModel = ShouldDrawViewModel( drawViewmodel );
	bool bShouldDrawToolViewModels = ToolsEnabled();

	if ( !bShouldDrawPlayerViewModel && !bShouldDrawToolViewModels )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	MDLCACHE_CRITICAL_SECTION();


	PIXEVENT( pRenderContext, "DrawViewModels()" );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	CViewSetup viewModelSetup( view );
	viewModelSetup.zNear = view.zNearViewmodel;
	viewModelSetup.zFar = view.zFarViewmodel;
	viewModelSetup.fov = view.fovViewmodel;
	viewModelSetup.m_flAspectRatio = engine->GetScreenAspectRatio( view.width, view.height );

	render->Push3DView( viewModelSetup, 0, NULL, GetFrustum() );

	if ( bGBuffer )
	{
		const float flViewmodelScale = view.zFarViewmodel / view.zFar;
		CGBufferView::PushGBuffer( false, flViewmodelScale, false );
	}
	else
	{
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
			DEFERRED_RENDER_STAGE_COMPOSITION );
	}


	const bool bUseDepthHack = true;

	// FIXME: Add code to read the current depth range
	float depthmin = 0.0f;
	float depthmax = 1.0f;

	// HACK HACK:  Munge the depth range to prevent view model from poking into walls, etc.
	// Force clipped down range
	if( bUseDepthHack )
		pRenderContext->DepthRange( 0.0f, 0.1f );
	
	CViewModelRenderablesList list;
	ClientLeafSystem()->CollateViewModelRenderables( &list );
	CViewModelRenderablesList::RenderGroups_t &opaqueList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	if ( ToolsEnabled() && ( !bShouldDrawPlayerViewModel || !bShouldDrawToolViewModels ) )
	{
		int nOpaque = opaqueList.Count();
		for ( int i = nOpaque-1; i >= 0; --i )
		{
			IClientRenderable *pRenderable = opaqueList[ i ].m_pRenderable;
			bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				opaqueList.FastRemove( i );
			}
		}

		int nTranslucent = translucentList.Count();
		for ( int i = nTranslucent-1; i >= 0; --i )
		{
			IClientRenderable *pRenderable = translucentList[ i ].m_pRenderable;
			bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				translucentList.FastRemove( i );
			}
		}
	}

	// Update refract for opaque models & draw
	bool bUpdatedRefractForOpaque = UpdateRefractIfNeededByList( opaqueList );
	DrawRenderablesInList( opaqueList );

	if ( !bGBuffer )
	{
		// Update refract for translucent models (if we didn't already update it above) & draw
		if ( !bUpdatedRefractForOpaque ) // Only do this once for better perf
		{
			UpdateRefractIfNeededByList( translucentList );
		}
		DrawRenderablesInList( translucentList, STUDIO_TRANSPARENCY );
	}
	else
	{
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
			DEFERRED_RENDER_STAGE_INVALID );
	}

	// Reset the depth range to the original values
	if( bUseDepthHack )
		pRenderContext->DepthRange( depthmin, depthmax );

	if ( bGBuffer )
	{
		CGBufferView::PopGBuffer();
	}

	render->PopView( GetFrustum() );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

//-----------------------------------------------------------------------------
// Purpose: This renders the entire 3D view and the in-game hud/viewmodel
// Input  : &view - 
//			whatToDraw - 
//-----------------------------------------------------------------------------
// This renders the entire 3D view.
void CDeferredViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CViewSetup worldView = view;

	CLightingEditor *pLightEditor = GetLightingEditor();

	if ( pLightEditor->IsEditorActive() && !building_cubemaps.GetBool() )
		pLightEditor->GetEditorView( &worldView.origin, &worldView.angles );
	else
		pLightEditor->SetEditorView( &worldView.origin, &worldView.angles );

	m_CurrentView = worldView;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CViewRender::RenderView" );

	{
		// HACK: server-side weapons use the viewmodel model, and client-side weapons swap that out for
		// the world model in DrawModel.  This is too late for some bone setup work that happens before
		// DrawModel, so here we just iterate all weapons we know of and fix them up ahead of time.
		MDLCACHE_CRITICAL_SECTION();
		CUtlLinkedList< CBaseCombatWeapon * > &weaponList = C_BaseCombatWeapon::GetWeaponList();
		FOR_EACH_LL( weaponList, it )
		{
			C_BaseCombatWeapon *weapon = weaponList[it];
			if ( !weapon->IsDormant() )
			{
				weapon->EnsureCorrectRenderingModel();
			}
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *saveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	{
		RenderPreScene( worldView );

		// Must be first 
		render->SceneBegin();

		g_pColorCorrectionMgr->UpdateColorCorrection();

		// Send the current tonemap scalar to the material system
		UpdateMaterialSystemTonemapScalar();

		// clear happens here probably
		SetupMain3DView( slot, worldView, hudViewSetup, nClearFlags, saveRenderTarget );

		g_pClientShadowMgr->UpdateSplitscreenLocalPlayerShadowSkip();

		ProcessDeferredGlobals( worldView );
		GetLightingManager()->LightSetup( worldView );

		PreViewDrawScene( worldView );

		// Force it to clear the framebuffer if they're in solid space.
		if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( enginetrace->GetPointContents( worldView.origin ) == CONTENTS_SOLID )
			{
				nClearFlags |= VIEW_CLEAR_COLOR;
			}
		}

		ViewDrawSceneDeferred( worldView, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

		// We can still use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( true );

		// must happen before teardown
		pLightEditor->OnRender();

		GetLightingManager()->LightTearDown();

		PostViewDrawScene( worldView );

		engine->DrawPortals();

		DisableFog();

		// Finish scene
		render->SceneEnd();

		// Draw lightsources if enabled
		//render->DrawLights();

		//RenderPlayerSprites();

		// Image-space motion blur and depth of field
		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif

		if ( !building_cubemaps.GetBool() )
		{
			if ( IsDepthOfFieldEnabled() )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoDepthOfField()" );
					DoDepthOfField( worldView );
				}
				pRenderContext.SafeRelease();
			}

			if ( ( worldView.m_nMotionBlurMode != MOTION_BLUR_DISABLE ) && ( mat_motion_blur_enabled.GetInt() ) )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur()" );
					DoImageSpaceMotionBlur( worldView );
				}
				pRenderContext.SafeRelease();
			}
		}

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif

		// Now actually draw the viewmodel
		//DrawViewModels( view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

		DrawUnderwaterOverlay();

		PixelVisibility_EndScene();

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif

		// Draw fade over entire screen if needed
		byte color[4];
		bool blend;
		GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

		// Store off color fade params to be applied in fullscreen postprocess pass
		SetViewFadeParams( color[0], color[1], color[2], color[3], blend );

		// Draw an overlay to make it even harder to see inside smoke particle systems.
		DrawSmokeFogOverlay();

		// Overlay screen fade on entire screen
		PerformScreenOverlay( worldView.x, worldView.y, worldView.width, worldView.height );

		// Prevent sound stutter if going slow
		engine->Sound_ExtraUpdate();	

		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
		{
			pRenderContext.GetFrom( materials );
			pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
			pRenderContext.SafeRelease();
		}

		if ( !building_cubemaps.GetBool() && worldView.m_bDoBloomAndToneMapping )
		{
			pRenderContext.GetFrom( materials );
			{
				static bool bAlreadyShowedLoadTime = false;
				
				if ( ! bAlreadyShowedLoadTime )
				{
					bAlreadyShowedLoadTime = true;
					if ( CommandLine()->CheckParm( "-timeload" ) )
					{
						Warning( "time to initial render = %f\n", Plat_FloatTime() );
					}
				}

				PIXEVENT( pRenderContext, "DoEnginePostProcessing()" );

				bool bFlashlightIsOn = false;
				C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
				if ( pLocal )
				{
					bFlashlightIsOn = pLocal->IsEffectActive( EF_DIMLIGHT );
				}
				DoEnginePostProcessing( worldView.x, worldView.y, worldView.width, worldView.height, bFlashlightIsOn );
			}
			pRenderContext.SafeRelease();
		}

#ifdef SHADEREDITOR
		g_ShaderEditorSystem->CustomPostRender();
#endif

		// And here are the screen-space effects

		if ( IsPC() )
		{
			// Grab the pre-color corrected frame for editing purposes
			engine->GrabPreColorCorrectedFrame( worldView.x, worldView.y, worldView.width, worldView.height );
		}

		PerformScreenSpaceEffects( worldView.x, worldView.y, worldView.width, worldView.height );


		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif

		GetClientMode()->DoPostScreenSpaceEffects( &worldView );

		CleanupMain3DView( worldView );

		if ( m_FreezeParams[ slot ].m_bTakeFreezeFrame )
		{
			pRenderContext = materials->GetRenderContext();
			if ( IsX360() )
			{
				// 360 doesn't create the Fullscreen texture
				pRenderContext->CopyRenderTargetToTextureEx( GetFullFrameFrameBufferTexture( 1 ), 0, NULL, NULL );
			}
			else
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetFullscreenTexture(), 0, NULL, NULL );
			}
			pRenderContext.SafeRelease();
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
		}

		pRenderContext = materials->GetRenderContext();
		pRenderContext->SetRenderTarget( saveRenderTarget );
		pRenderContext.SafeRelease();

		// Draw the overlay
		if ( m_bDrawOverlay )
		{	   
			// This allows us to be ok if there are nested overlay views
			CViewSetup currentView = m_CurrentView;
			CViewSetup tempView = m_OverlayViewSetup;
			tempView.fov = ScaleFOVByWidthRatio( tempView.fov, tempView.m_flAspectRatio / ( 4.0f / 3.0f ) );
			tempView.m_bDoBloomAndToneMapping = false;				// FIXME: Hack to get Mark up and running
			tempView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;		// FIXME: Hack to get Mark up and running
			m_bDrawOverlay = false;
			RenderView( tempView, hudViewSetup, m_OverlayClearFlags, m_OverlayDrawFlags );
			m_CurrentView = currentView;
		}
	}

	// Clear a row of pixels at the edge of the viewport if it isn't at the edge of the screen
	if ( VGui_IsSplitScreen() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport();

		int nScreenWidth, nScreenHeight;
		g_pMaterialSystem->GetBackBufferDimensions( nScreenWidth, nScreenHeight );

		// NOTE: view.height is off by 1 on the PC in a release build, but debug is correct! I'm leaving this here to help track this down later.
		// engine->Con_NPrintf( 25 + hh, "view( %d, %d, %d, %d ) GetBackBufferDimensions( %d, %d )\n", view.x, view.y, view.width, view.height, nScreenWidth, nScreenHeight );

		if ( worldView.x != 0 ) // if left of viewport isn't at 0
		{
			pRenderContext->Viewport( worldView.x, worldView.y, 1, worldView.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( worldView.x + worldView.width ) != nScreenWidth ) // if right of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( worldView.x + worldView.width - 1, worldView.y, 1, worldView.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( worldView.y != 0 ) // if top of viewport isn't at 0
		{
			pRenderContext->Viewport( worldView.x, worldView.y, worldView.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( worldView.y + worldView.height ) != nScreenHeight ) // if bottom of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( worldView.x, worldView.y + worldView.height - 1, worldView.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		pRenderContext->PopRenderTargetAndViewport();
		pRenderContext->Release();
	}

	// Draw the 2D graphics
	m_CurrentView = hudViewSetup;
	pRenderContext = materials->GetRenderContext();
	if ( true )
	{
		PIXEVENT( pRenderContext, "2D Client Rendering" );

		render->Push2DView( hudViewSetup, 0, saveRenderTarget, GetFrustum() );

		Render2DEffectsPreHUD( hudViewSetup );

		if ( whatToDraw & RENDERVIEW_DRAWHUD )
		{
			VPROF_BUDGET( "VGui_DrawHud", VPROF_BUDGETGROUP_OTHER_VGUI );
			// paint the vgui screen
			VGui_PreRender();

			CUtlVector< vgui::VPANEL > vecHudPanels;

			vecHudPanels.AddToTail( VGui_GetClientDLLRootPanel() );

			// This block is suspect - why are we resizing fullscreen panels to be the size of the hudViewSetup
			// which is potentially only half the screen
			if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
			{
				vecHudPanels.AddToTail( VGui_GetFullscreenRootVPANEL() );

#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
#endif
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS ) );
			}

			PositionHudPanels( vecHudPanels, hudViewSetup );

			// The crosshair, etc. needs to get at the current setup stuff
			AllowCurrentViewAccess( true );

			// Draw the in-game stuff based on the actual viewport being used
			render->VGui_Paint( PAINT_INGAMEPANELS );

			AllowCurrentViewAccess( false );

			VGui_PostRender();

			GetClientMode()->PostRenderVGui();
			pRenderContext->Flush();
		}

		CDebugViewRender::Draw2DDebuggingInfo( hudViewSetup );

		Render2DEffectsPostHUD( hudViewSetup );

		// We can no longer use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( false );

		if ( IsPC() )
		{
			CDebugViewRender::GenerateOverdrawForTesting();
		}

		render->PopView( GetFrustum() );
	}
	pRenderContext.SafeRelease();

	FlushWorldLists();

	m_CurrentView = worldView;

#ifdef PARTICLE_USAGE_DEMO
	ParticleUsageDemo();
#endif
}

struct defData_setGlobals
{
public:
	Vector orig, fwd;
	float zDists[2];
	VMatrix frustumDeltas;
#if DEFCFG_BILATERAL_DEPTH_TEST
	VMatrix worldCameraDepthTex;
#endif

	static void Fire( defData_setGlobals d )
	{
		IDeferredExtension *pDef = GetDeferredExt();
		pDef->CommitOrigin( d.orig );
		pDef->CommitViewForward( d.fwd );
		pDef->CommitZDists( d.zDists[0], d.zDists[1] );
		pDef->CommitFrustumDeltas( d.frustumDeltas );
#if DEFCFG_BILATERAL_DEPTH_TEST
		pDef->CommitWorldToCameraDepthTex( d.worldCameraDepthTex );
#endif
	};
};

void CDeferredViewRender::ProcessDeferredGlobals( const CViewSetup &view )
{
	VMatrix matPerspective, matView, matViewProj, screen2world;
	matView.Identity();
	matView.SetupMatrixOrgAngles( vec3_origin, view.angles );

	MatrixSourceToDeviceSpace( matView );
#ifdef SHADEREDITOR
	//g_ShaderEditorSystem->SetMainViewMatrix( matView );
#endif

	matView = matView.Transpose3x3();
	Vector viewPosition;

	Vector3DMultiply( matView, view.origin, viewPosition );
	matView.SetTranslation( -viewPosition );
	MatrixBuildPerspectiveX( matPerspective, view.fov, view.m_flAspectRatio,
		view.zNear, view.zFar );
	MatrixMultiply( matPerspective, matView, matViewProj );

	MatrixInverseGeneral( matViewProj, screen2world );

	GetLightingManager()->SetRenderConstants( screen2world, view );

	Vector frustum_c0, frustum_cc, frustum_1c;
	float projDistance = 1.0f;
	Vector3DMultiplyPositionProjective( screen2world, Vector(0,projDistance,projDistance), frustum_c0 );
	Vector3DMultiplyPositionProjective( screen2world, Vector(0,0,projDistance), frustum_cc );
	Vector3DMultiplyPositionProjective( screen2world, Vector(projDistance,0,projDistance), frustum_1c );

	frustum_c0 -= view.origin;
	frustum_cc -= view.origin;
	frustum_1c -= view.origin;

	Vector frustum_up = frustum_c0 - frustum_cc;
	Vector frustum_right = frustum_1c - frustum_cc;

	frustum_cc /= view.zFar;
	frustum_right /= view.zFar;
	frustum_up /= view.zFar;

	defData_setGlobals data;
	data.orig = view.origin;
	AngleVectors( view.angles, &data.fwd );
	data.zDists[0] = view.zNear;
	data.zDists[1] = view.zFar;

	data.frustumDeltas.Identity();
	data.frustumDeltas.SetBasisVectors( frustum_cc, frustum_right, frustum_up );
	data.frustumDeltas = data.frustumDeltas.Transpose3x3();

#if DEFCFG_BILATERAL_DEPTH_TEST
	VMatrix matWorldToCameraDepthTex;
	MatrixBuildScale( matWorldToCameraDepthTex, 0.5f, -0.5f, 1.0f );
	matWorldToCameraDepthTex[0][3] = matWorldToCameraDepthTex[1][3] = 0.5f;
	MatrixMultiply( matWorldToCameraDepthTex, matViewProj, matWorldToCameraDepthTex );

	data.worldCameraDepthTex = matWorldToCameraDepthTex.Transpose();
#endif

	QUEUE_FIRE( defData_setGlobals, Fire, data );
}

//-----------------------------------------------------------------------------
// Returns true if the view plane intersects the water
//-----------------------------------------------------------------------------
extern bool DoesViewPlaneIntersectWater( float waterZ, int leafWaterDataID );



//-----------------------------------------------------------------------------
// Fakes per-entity clip planes on cards that don't support user clip planes.
//  Achieves the effect by drawing an invisible box that writes to the depth buffer
//  around the clipped area. It's not perfect, but better than nothing.
//-----------------------------------------------------------------------------
static void DrawClippedDepthBox( IClientRenderable *pEnt, float *pClipPlane )
{
//#define DEBUG_DRAWCLIPPEDDEPTHBOX //uncomment to draw the depth box as a colorful box

	static const int iQuads[6][5] = {	{ 0, 4, 6, 2, 0 }, //always an extra copy of first index at end to make some algorithms simpler
										{ 3, 7, 5, 1, 3 },
										{ 1, 5, 4, 0, 1 },
										{ 2, 6, 7, 3, 2 },
										{ 0, 2, 3, 1, 0 },
										{ 5, 7, 6, 4, 5 } };

	static const int iLines[12][2] = {	{ 0, 1 },
										{ 0, 2 },
										{ 0, 4 },
										{ 1, 3 },
										{ 1, 5 },
										{ 2, 3 },
										{ 2, 6 },
										{ 3, 7 },
										{ 4, 6 },
										{ 4, 5 },
										{ 5, 7 },
										{ 6, 7 } };


#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	static const float fColors[6][3] = {	{ 1.0f, 0.0f, 0.0f },
											{ 0.0f, 1.0f, 1.0f },
											{ 0.0f, 1.0f, 0.0f },
											{ 1.0f, 0.0f, 1.0f },
											{ 0.0f, 0.0f, 1.0f },
											{ 1.0f, 1.0f, 0.0f } };
#endif

	
	

	Vector vNormal = *(Vector *)pClipPlane;
	float fPlaneDist = pClipPlane[3];

	Vector vMins, vMaxs;
	pEnt->GetRenderBounds( vMins, vMaxs );

	Vector vOrigin = pEnt->GetRenderOrigin();
	QAngle qAngles = pEnt->GetRenderAngles();
	
	Vector vForward, vUp, vRight;
	AngleVectors( qAngles, &vForward, &vRight, &vUp );

	Vector vPoints[8];
	vPoints[0] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[1] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[2] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[3] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[4] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[5] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[6] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);
	vPoints[7] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);

	int iClipped[8];
	float fDists[8];
	for( int i = 0; i != 8; ++i )
	{
		fDists[i] = vPoints[i].Dot( vNormal ) - fPlaneDist;
		iClipped[i] = (fDists[i] > 0.0f) ? 1 : 0;
	}

	Vector vSplitPoints[8][8]; //obviously there are only 12 lines, not 64 lines or 64 split points, but the indexing is way easier like this
	int iLineStates[8][8]; //0 = unclipped, 2 = wholly clipped, 3 = first point clipped, 4 = second point clipped

	//categorize lines and generate split points where needed
	for( int i = 0; i != 12; ++i )
	{
		const int *pPoints = iLines[i];
		int iLineState = (iClipped[pPoints[0]] + iClipped[pPoints[1]]);
		if( iLineState != 1 ) //either both points are clipped, or neither are clipped
		{
			iLineStates[pPoints[0]][pPoints[1]] = 
				iLineStates[pPoints[1]][pPoints[0]] = 
					iLineState;
		}
		else
		{
			//one point is clipped, the other is not
			if( iClipped[pPoints[0]] == 1 )
			{
				//first point was clipped, index 1 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[0]] - fDists[pPoints[1]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist)) - (vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist));
				
				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 3;
				iLineStates[pPoints[1]][pPoints[0]] = 4;
			}
			else
			{
				//second point was clipped, index 0 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[1]] - fDists[pPoints[0]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist)) - (vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist));

				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 4;
				iLineStates[pPoints[1]][pPoints[0]] = 3;
			}
		}
	}

	extern CMaterialReference g_material_WriteZ;
	CMatRenderContextPtr pRenderContext( materials );
	
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	pRenderContext->Bind( materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER ), NULL );
#else
	pRenderContext->Bind( g_material_WriteZ, NULL );
#endif

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 18 ); //6 sides, possible one cut per side. Any side is capable of having 3 tri's. Lots of padding for things that aren't possible

	//going to draw as a collection of triangles, arranged as a triangle fan on each side
	for( int i = 0; i != 6; ++i )
	{
		const int *pPoints = iQuads[i];

		//can't start the fan on a wholly clipped line, so seek to one that isn't
		int j = 0;
		do
		{
			if( iLineStates[pPoints[j]][pPoints[j+1]] != 2 ) //at least part of this line will be drawn
				break;

			++j;
		} while( j != 3 );

		if( j == 3 ) //not enough lines to even form a triangle
			continue;

		float *pStartPoint = static_cast<float *>(0);
		float *pTriangleFanPoints[4]; //at most, one of our fans will have 5 points total, with the first point being stored separately as pStartPoint
		int iTriangleFanPointCount = 1; //the switch below creates the first for sure
		
		//figure out how to start the fan
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 0: //uncut
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j+1]].x;
			break;

		case 4: //second index was clipped
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			break;

		case 3: //first index was clipped
			pStartPoint = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j + 1]].x;
			break;

		default:
			Assert( false );
			break;
		};

		for( ++j; j != 3; ++j ) //add end points for the rest of the indices, we're assembling a triangle fan
		{
			switch( iLineStates[pPoints[j]][pPoints[j+1]] )
			{
			case 0: //uncut line, normal endpoint
				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 2: //wholly cut line, no endpoint
				break;

			case 3: //first point is clipped, normal endpoint
				//special case, adds start and end point
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;

				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 4: //second point is clipped
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			default:
				Assert( false );
				break;
			};
		}
		
		//special case endpoints, half-clipped lines have a connecting line between them and the next line (first line in this case)
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 3:
		case 4:
			pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			++iTriangleFanPointCount;
			break;
		};

		Assert( iTriangleFanPointCount <= 4 );

		//add the fan to the mesh
		int iLoopStop = iTriangleFanPointCount - 1;
		for( int k = 0; k != iLoopStop; ++k )
		{
			meshBuilder.Position3fv( pStartPoint );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			float fHalfColors[3] = { fColors[i][0] * 0.5f, fColors[i][1] * 0.5f, fColors[i][2] * 0.5f };
			meshBuilder.Color3fv( fHalfColors );
#endif
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Position3fv( pTriangleFanPoints[k] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pTriangleFanPoints[k+1] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
	pRenderContext->Flush( false );
}

//-----------------------------------------------------------------------------
// Unified bit of draw code for opaque and translucent renderables
//-----------------------------------------------------------------------------
static inline void DrawRenderable( IClientRenderable *pEnt, int flags, const RenderableInstance_t &instance )
{
	float *pRenderClipPlane = NULL;
	if( r_entityclips.GetBool() )
		pRenderClipPlane = pEnt->GetRenderClipPlane();

	if( pRenderClipPlane )	
	{
		CMatRenderContextPtr pRenderContext( materials );
		if( !materials->UsingFastClipping() ) //do NOT change the fast clip plane mid-scene, depth problems result. Regular user clip planes are fine though
			pRenderContext->PushCustomClipPlane( pRenderClipPlane );
		else
			DrawClippedDepthBox( pEnt, pRenderClipPlane );
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		bool bBlockNormalDraw = false;
		if( !bBlockNormalDraw )
			pEnt->DrawModel( flags, instance );
		view->SetCurrentlyDrawingEntity( NULL );

		if( !materials->UsingFastClipping() )	
			pRenderContext->PopCustomClipPlane();
	}
	else
	{
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		bool bBlockNormalDraw = false;
		if( !bBlockNormalDraw )
			pEnt->DrawModel( flags, instance );
		view->SetCurrentlyDrawingEntity( NULL );
	}
}

//-----------------------------------------------------------------------------
// Draws all opaque renderables in leaves that were rendered
//-----------------------------------------------------------------------------
static inline void DrawOpaqueRenderable( IClientRenderable *pEnt, bool bTwoPass, bool bNoDecals )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	float color[3];

	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
	Assert( (pEnt->GetIClientUnknown() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity()->IsBlurred() == false) );
	pEnt->GetColorModulation( color );
	render->SetColorModulation(	color );

	int flags = STUDIO_RENDER;
	if ( bTwoPass )
	{
		flags |= STUDIO_TWOPASS;
	}

	if ( bNoDecals )
	{
		flags |= STUDIO_SKIP_DECALS;
	}

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	DrawRenderable( pEnt, flags, instance );
}

//-------------------------------------


static void SetupBonesOnBaseAnimating( C_BaseAnimating *&pBaseAnimating )
{
	pBaseAnimating->SetupBones( NULL, -1, -1, gpGlobals->curtime );
}


static void DrawOpaqueRenderables_DrawBrushModels( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bNoDecals )
{
	for( int i = 0; i < nCount; ++i )
	{
		Assert( !ppEntities[i]->m_TwoPass );
		DrawOpaqueRenderable( ppEntities[i]->m_pRenderable, false, bNoDecals );
	}
}

static void DrawOpaqueRenderables_DrawStaticProps( int nCount, CClientRenderablesList::CEntry **ppEntities )
{
	if ( nCount == 0 )
		return;

	float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	render->SetColorModulation(	one );
	render->SetBlend( 1.0f );
	
	const int MAX_STATICS_PER_BATCH = 512;
	IClientRenderable *pStatics[ MAX_STATICS_PER_BATCH ];
	RenderableInstance_t pInstances[ MAX_STATICS_PER_BATCH ];
	
	int numScheduled = 0, numAvailable = MAX_STATICS_PER_BATCH;

	for( int i = 0; i < nCount; ++i )
	{
		CClientRenderablesList::CEntry *itEntity = ppEntities[i];
		if ( itEntity->m_pRenderable )
			NULL;
		else
			continue;

		pInstances[ numScheduled ] = itEntity->m_InstanceData;
		pStatics[ numScheduled ++ ] = itEntity->m_pRenderable;
		if ( -- numAvailable > 0 )
			continue; // place a hint for compiler to predict more common case in the loop
		
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, false, vcollide_wireframe.GetBool() );
		numScheduled = 0;
		numAvailable = MAX_STATICS_PER_BATCH;
	}
	
	if ( numScheduled )
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, false, vcollide_wireframe.GetBool() );
}

static void DrawOpaqueRenderables_Range( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bNoDecals )
{
	for ( int i = 0; i < nCount; ++i )
	{
		CClientRenderablesList::CEntry *itEntity = ppEntities[i]; 
		if ( itEntity->m_pRenderable )
		{
			DrawOpaqueRenderable( itEntity->m_pRenderable, ( itEntity->m_TwoPass != 0 ), bNoDecals );
		}
	}
}

extern ConVar cl_modelfastpath;
extern ConVar cl_skipslowpath;
extern ConVar r_drawothermodels;
static void	DrawOpaqueRenderables_ModelRenderables( int nCount, ModelRenderSystemData_t* pModelRenderables )
{
	g_pModelRenderSystem->DrawModels( pModelRenderables, nCount, MODEL_RENDER_MODE_NORMAL );
}

static void	DrawOpaqueRenderables_NPCs( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bNoDecals )
{
	DrawOpaqueRenderables_Range( nCount, ppEntities, bNoDecals );
}

//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list
//-----------------------------------------------------------------------------
static inline void DrawTranslucentRenderable( IClientRenderable *pEnt, const RenderableInstance_t &instance, bool twoPass )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );

	// Renderable list building should already have caught this
	Assert( instance.m_nAlpha > 0 );

	// Determine blending amount and tell engine
	float blend = (float)( instance.m_nAlpha / 255.0f );

	// Tell engine
	render->SetBlend( blend );

	float color[3];
	pEnt->GetColorModulation( color );
	render->SetColorModulation(	color );

	int flags = STUDIO_RENDER | STUDIO_TRANSPARENCY;
	if ( twoPass )
		flags |= STUDIO_TWOPASS;

	DrawRenderable( pEnt, flags, instance );
}

void CBaseWorldViewDeferred::DrawWorldDeferred( float waterZAdjust )
{
	DrawWorld( waterZAdjust );
}

void CBaseWorldViewDeferred::DrawOpaqueRenderablesDeferred( bool bNoDecals )
{
	VPROF("CViewRender::DrawOpaqueRenderables" );

	if( !r_drawopaquerenderables.GetBool() )
		return;

	if( !m_pMainView->ShouldDrawEntities() )
		return;

	render->SetBlend( 1 );

	//
	// Prepare to iterate over all leaves that were visible, and draw opaque things in them.	
	//
	RopeManager()->ResetRenderCache();
	g_pParticleSystemMgr->ResetRenderCache();

	// Categorize models by type
	int nOpaqueRenderableCount = m_pRenderablesList->m_RenderGroupCounts[RENDER_GROUP_OPAQUE];
	CUtlVector< CClientRenderablesList::CEntry* > brushModels( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CUtlVector< CClientRenderablesList::CEntry* > staticProps( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CUtlVector< CClientRenderablesList::CEntry* > otherRenderables( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CClientRenderablesList::CEntry *pOpaqueList = m_pRenderablesList->m_RenderGroups[RENDER_GROUP_OPAQUE];
	for ( int i = 0; i < nOpaqueRenderableCount; ++i )
	{
		switch( pOpaqueList[i].m_nModelType )
		{
		case RENDERABLE_MODEL_BRUSH:		brushModels.AddToTail( &pOpaqueList[i] ); break; 
		case RENDERABLE_MODEL_STATIC_PROP:	staticProps.AddToTail( &pOpaqueList[i] ); break; 
		default:							otherRenderables.AddToTail( &pOpaqueList[i] ); break; 
		}
	}

	//
	// First do the brush models
	//
	DrawOpaqueRenderables_DrawBrushModels( brushModels.Count(), brushModels.Base(), bNoDecals );

	// Move all static props to modelrendersystem
	bool bUseFastPath = ( cl_modelfastpath.GetInt() != 0 );

	//
	// Sort everything that's not a static prop
	//
	int nStaticPropCount = staticProps.Count();
	int numOpaqueEnts = otherRenderables.Count();
	CUtlVector< CClientRenderablesList::CEntry* > arrRenderEntsNpcsFirst( (CClientRenderablesList::CEntry **)stackalloc( numOpaqueEnts * sizeof( CClientRenderablesList::CEntry ) ), numOpaqueEnts );
	CUtlVector< ModelRenderSystemData_t > arrModelRenderables( (ModelRenderSystemData_t *)stackalloc( ( numOpaqueEnts + nStaticPropCount ) * sizeof( ModelRenderSystemData_t ) ), numOpaqueEnts + nStaticPropCount );

	// Queue up RENDER_GROUP_OPAQUE_ENTITY entities to be rendered later.
	CClientRenderablesList::CEntry *itEntity;
	if( r_drawothermodels.GetBool() )
	{
		for ( int i = 0; i < numOpaqueEnts; ++i )
		{
			itEntity = otherRenderables[i];
			if ( !itEntity->m_pRenderable )
				continue;

			IClientUnknown *pUnknown = itEntity->m_pRenderable->GetIClientUnknown();
			IClientModelRenderable *pModelRenderable = pUnknown->GetClientModelRenderable();
			C_BaseEntity *pEntity = pUnknown->GetBaseEntity();

			// FIXME: Strangely, some static props are in the non-static prop bucket
			// which is what the last case in this if statement is for
			if ( bUseFastPath && pModelRenderable )
			{
				ModelRenderSystemData_t data;
				data.m_pRenderable = itEntity->m_pRenderable;
				data.m_pModelRenderable = pModelRenderable;
				data.m_InstanceData = itEntity->m_InstanceData;
				arrModelRenderables.AddToTail( data );
				otherRenderables.FastRemove( i );
				--i; --numOpaqueEnts;
				continue;
			}

			if ( !pEntity )
				continue;

			if ( pEntity->IsNPC() )
			{
				arrRenderEntsNpcsFirst.AddToTail( itEntity );
				otherRenderables.FastRemove( i );
				--i; --numOpaqueEnts;
				continue;
			}
		}
	}

	// Queue up the static props to be rendered later.
	for ( int i = 0; i < nStaticPropCount; ++i )
	{
		itEntity = staticProps[i];
		if ( !itEntity->m_pRenderable )
			continue;

		IClientUnknown *pUnknown = itEntity->m_pRenderable->GetIClientUnknown();
		IClientModelRenderable *pModelRenderable = pUnknown->GetClientModelRenderable();
		if ( !bUseFastPath || !pModelRenderable )
			continue;

		ModelRenderSystemData_t data;
		data.m_pRenderable = itEntity->m_pRenderable;
		data.m_pModelRenderable = pModelRenderable;
		data.m_InstanceData = itEntity->m_InstanceData;
		arrModelRenderables.AddToTail( data );

		staticProps.FastRemove( i );
		--i; --nStaticPropCount;
	}

	//
	// Draw model renderables now (ie. models that use the fast path)
	//					 
	DrawOpaqueRenderables_ModelRenderables( arrModelRenderables.Count(), arrModelRenderables.Base() );

	// Turn off z pass here. Don't want non-fastpath models with potentially large dynamic VB requirements overwrite
	// stuff in the dynamic VB ringbuffer. We're calling End360ZPass again in DrawExecute, but that's not a problem.
	// Begin360ZPass/End360ZPass don't have to be matched exactly.
	End360ZPass();

	//
	// Draw static props + opaque entities that aren't using the fast path.
	//
	DrawOpaqueRenderables_Range( otherRenderables.Count(), otherRenderables.Base(), bNoDecals );
	DrawOpaqueRenderables_DrawStaticProps( staticProps.Count(), staticProps.Base() );

	//
	// Draw NPCs now
	//
	DrawOpaqueRenderables_NPCs( arrRenderEntsNpcsFirst.Count(), arrRenderEntsNpcsFirst.Base(), bNoDecals );

	//
	// Ropes and particles
	//
	RopeManager()->DrawRenderCache( false );
	g_pParticleSystemMgr->DrawRenderCache( false );
}



static ConVar r_unlimitedrefract( "r_unlimitedrefract", "0" );
extern ConVar cl_tlucfastpath;
extern ConVar cl_colorfastpath;


//-----------------------------------------------------------------------------
// Standard 3d skybox view
//-----------------------------------------------------------------------------
SkyboxVisibility_t CSkyboxViewDeferred::ComputeSkyboxVisibility()
{
	if ( ( enginetrace->GetPointContents( origin ) & CONTENTS_SOLID ) != 0 )
		return SKYBOX_NOT_VISIBLE;

	return engine->IsSkyboxVisibleFromPoint( origin );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxViewDeferred::GetSkyboxFogEnable()
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return false;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	static ConVarRef fog_override( "fog_override" );
	if( fog_override.GetInt() )
	{
		if( fog_enableskybox.GetInt() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return !!local->m_skybox3d.fog.enable;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxViewDeferred::Enable3dSkyboxFog( void )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	CMatRenderContextPtr pRenderContext( materials );

	if( GetSkyboxFogEnable() )
	{
		float fogColor[3];
		GetSkyboxFogColor( fogColor );
		float scale = 1.0f;
		if ( local->m_skybox3d.scale > 0.0f )
		{
			scale = 1.0f / local->m_skybox3d.scale;
		}
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetSkyboxFogStart() * scale );
		pRenderContext->FogEnd( GetSkyboxFogEnd() * scale );
		pRenderContext->FogMaxDensity( GetSkyboxFogMaxDensity() );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
sky3dparams_t *CSkyboxViewDeferred::PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible )
{
	if ( ( nSkyboxVisible != SKYBOX_3DSKYBOX_VISIBLE ) && r_3dsky.GetInt() != 2 )
		return NULL;

	// render the 3D skybox
	if ( !r_3dsky.GetInt() )
		return NULL;

	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();

	// No local player object yet...
	if ( !pbp )
		return NULL;

	CPlayerLocalData* local = &pbp->m_Local;
	if ( local->m_skybox3d.area == 255 )
		return NULL;

	return &local->m_skybox3d;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxViewDeferred::DrawInternal( view_id_t iSkyBoxViewID, bool bInvokePreAndPostRender, ITexture *pRenderTarget )
{
	bInvokePreAndPostRender = !m_bGBufferPass;

	if ( m_bGBufferPass )
	{
		m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;

#if DEFCFG_DEFERRED_SHADING
		m_DrawFlags |= DF_DRAWSKYBOX;
#endif
	}

	unsigned char **areabits = render->GetAreaBits();
	unsigned char *savebits;
	unsigned char tmpbits[ 32 ];
	savebits = *areabits;
	memset( tmpbits, 0, sizeof(tmpbits) );

	// set the sky area bit
	tmpbits[m_pSky3dParams->area>>3] |= 1 << (m_pSky3dParams->area&7);

	*areabits = tmpbits;

	// if you can get really close to the skybox geometry it's possible that you'll be able to clip into it
	// with this near plane.  If so, move it in a bit.  It's at 2.0 to give us more precision.  That means you 
	// need to keep the eye position at least 2 * scale away from the geometry in the skybox
	zNear = 2.0;
	zFar = 10000.0f; //MAX_TRACE_LENGTH;

	float skyScale = 1.0f;
	// scale origin by sky scale and translate to sky origin
	{
		skyScale = (m_pSky3dParams->scale > 0) ? m_pSky3dParams->scale : 1.0f;
		float scale = 1.0f / skyScale;

		Vector vSkyOrigin = m_pSky3dParams->origin;
		VectorScale( origin, scale, origin );
		VectorAdd( origin, vSkyOrigin, origin );

		if( m_bCustomViewMatrix )
		{
			Vector vTransformedSkyOrigin;
			VectorRotate( vSkyOrigin, m_matCustomViewMatrix, vTransformedSkyOrigin ); //Rotate instead of transform because we haven't scale the existing offset yet

			//scale existing translation, and tack on the skybox offset (subtract because it's a view matrix)
			m_matCustomViewMatrix.m_flMatVal[0][3] = (m_matCustomViewMatrix.m_flMatVal[0][3] * scale) - vTransformedSkyOrigin.x;
			m_matCustomViewMatrix.m_flMatVal[1][3] = (m_matCustomViewMatrix.m_flMatVal[1][3] * scale) - vTransformedSkyOrigin.y;
			m_matCustomViewMatrix.m_flMatVal[2][3] = (m_matCustomViewMatrix.m_flMatVal[2][3] * scale) - vTransformedSkyOrigin.z;
		}
	}

	if ( !m_bGBufferPass )
		Enable3dSkyboxFog();

	// BUGBUG: Fix this!!!  We shouldn't need to call setup vis for the sky if we're connecting
	// the areas.  We'd have to mark all the clusters in the skybox area in the PVS of any 
	// cluster with sky.  Then we could just connect the areas to do our vis.
	//m_bOverrideVisOrigin could hose us here, so call direct
	render->ViewSetupVis( false, 1, &m_pSky3dParams->origin.Get() );
	render->Push3DView( (*this), m_ClearFlags, pRenderTarget, GetFrustum() );

	if ( m_bGBufferPass )
		PushGBuffer( true, skyScale );
	else
		PushComposite();

	// Store off view origin and angles
	SetupCurrentView( origin, angles, iSkyBoxViewID );

#if defined( _X360 )
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
	pRenderContext.SafeRelease();
#endif

	// Invoke pre-render methods
	if ( bInvokePreAndPostRender )
	{
		IGameSystem::PreRenderAllSystems();
	}

	BuildWorldRenderLists( true, -1, ShouldCacheLists() );

	BuildRenderableRenderLists( m_bGBufferPass ? VIEW_SHADOW_DEPTH_TEXTURE : iSkyBoxViewID );

	DrawWorld( 0.0f );

	// Iterate over all leaves and render objects in those leaves
	DrawOpaqueRenderables( false );

	if ( !m_bGBufferPass )
	{
		// Iterate over all leaves and render objects in those leaves
		DrawTranslucentRenderables( true, false );
		DrawNoZBufferTranslucentRenderables();
	}

	if ( !m_bGBufferPass )
	{
		m_pMainView->DisableFog();

		CGlowOverlay::UpdateSkyOverlays( zFar, m_bCacheFullSceneState );

		PixelVisibility_EndCurrentView();
	}

	// restore old area bits
	*areabits = savebits;

	// Invoke post-render methods
	if( bInvokePreAndPostRender )
	{
		IGameSystem::PostRenderAllSystems();
		FinishCurrentView();
	}

	if ( m_bGBufferPass )
		PopGBuffer();
	else
		PopComposite();

	render->PopView( GetFrustum() );

#if defined( _X360 )
	pRenderContext.GetFrom( materials );
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxViewDeferred::Setup( const CViewSetup &view, bool bGBuffer, SkyboxVisibility_t *pSkyboxVisible )
{
	BaseClass::Setup( view );

	// The skybox might not be visible from here
	*pSkyboxVisible = ComputeSkyboxVisibility();
	m_pSky3dParams = PreRender3dSkyboxWorld( *pSkyboxVisible );

	if ( !m_pSky3dParams )
	{
		return false;
	}

	m_bGBufferPass = bGBuffer;
	// At this point, we've cleared everything we need to clear
	// The next path will need to clear depth, though.
	m_ClearFlags = VIEW_CLEAR_DEPTH; //*pClearFlags;
	//*pClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL | VIEW_CLEAR_FULL_TARGET );
	//*pClearFlags |= VIEW_CLEAR_DEPTH; // Need to clear depth after rednering the skybox

	m_DrawFlags = DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_RENDER_WATER;
	if( !m_bGBufferPass && r_skybox.GetBool() )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxViewDeferred::Draw()
{
	VPROF_BUDGET( "CViewRender::Draw3dSkyboxworld", "3D Skybox" );

	DrawInternal();
}



void CGBufferView::Setup( const CViewSetup &view, bool bDrewSkybox )
{
	m_fogInfo.m_bEyeInFogVolume = false;
	m_bDrewSkybox = bDrewSkybox;

	BaseClass::Setup( view );
	m_bDrawWorldNormal = true;

	m_ClearFlags = 0;
	m_DrawFlags = DF_DRAW_ENTITITES;

	m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;

#if DEFCFG_DEFERRED_SHADING
	if ( !bDrewSkybox )
		m_DrawFlags |= DF_DRAWSKYBOX;
#endif
}

void CGBufferView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldViewDeferred::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	SetupCurrentView( origin, angles, VIEW_DEFERRED_GBUFFER );

	DrawSetup( 0, m_DrawFlags, 0 );

	const bool bOptimizedGbuffer = DEFCFG_DEFERRED_SHADING == 0;
	DrawExecute( 0, CurrentViewID(), 0, bOptimizedGbuffer );

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CGBufferView::PushView( float waterHeight )
{
	PushGBuffer( !m_bDrewSkybox );
}

void CGBufferView::PopView()
{
	PopGBuffer();
}

void CGBufferView::PushGBuffer( bool bInitial, float zScale, bool bClearDepth )
{
	ITexture *pNormals = GetDefRT_Normals();
	ITexture *pDepth = GetDefRT_Depth();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );

	if ( bInitial )
	{
		pRenderContext->PushRenderTargetAndViewport( pDepth );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();
	}

#if DEFCFG_DEFERRED_SHADING == 1
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Albedo() );
#else
	pRenderContext->PushRenderTargetAndViewport( pNormals );
#endif

	if ( bClearDepth )
		pRenderContext->ClearBuffers( false, true );

	pRenderContext->SetRenderTargetEx( 1, pDepth );

#if DEFCFG_DEFERRED_SHADING == 1
	pRenderContext->SetRenderTargetEx( 2, pNormals );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_Specular() );
#endif

	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_GBUFFER );

	struct defData_setZScale
	{
	public:
		float zScale;

		static void Fire( defData_setZScale d )
		{
			GetDeferredExt()->CommitZScale( d.zScale );
		};
	};

	defData_setZScale data;
	data.zScale = zScale;
	QUEUE_FIRE( defData_setZScale, Fire, data );
}

void CGBufferView::PopGBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );

	pRenderContext->PopRenderTargetAndViewport();
}




//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
bool CBaseWorldViewDeferred::AdjustView( float waterHeight )
{
	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		ITexture *pTexture = GetWaterRefractionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();

		return true;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();
		angles[0] = -angles[0];
		angles[2] = -angles[2];
		origin[2] -= 2.0f * ( origin[2] - (waterHeight));
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldViewDeferred::PushView( float waterHeight )
{
	float spread = 2.0f;
	if( m_DrawFlags & DF_FUDGE_UP )
	{
		waterHeight += spread;
	}
	else
	{
		waterHeight -= spread;
	}

	MaterialHeightClipMode_t clipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	if ( ( m_DrawFlags & DF_CLIP_Z ) && mat_clipz.GetBool() )
	{
		if( m_DrawFlags & DF_CLIP_BELOW )
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT;
		}
		else
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT;
		}
	}

	CMatRenderContextPtr pRenderContext( materials );

	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		pRenderContext->SetFogZ( waterHeight );
		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		// Have to re-set up the view since we reset the size
		render->Push3DView( *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

		return;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		pRenderContext->SetFogZ( waterHeight );

		bool bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();
		if( bSoftwareUserClipPlane && ( origin[2] > waterHeight - r_eyewaterepsilon.GetFloat() ) )
		{
			waterHeight = origin[2] + r_eyewaterepsilon.GetFloat();
		}

		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		render->Push3DView( *this, m_ClearFlags, pTexture, GetFrustum() );
		return;
	}

	if ( m_ClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_STENCIL ) )
	{
		if ( m_ClearFlags & VIEW_CLEAR_OBEY_STENCIL )
		{
			pRenderContext->ClearBuffersObeyStencil( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false );
		}
		else
		{
			pRenderContext->ClearBuffers( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_STENCIL ) ? true : false );
		}
	}

	pRenderContext->SetHeightClipMode( clipMode );
	if ( clipMode != MATERIAL_HEIGHTCLIPMODE_DISABLE )
	{   
		pRenderContext->SetHeightClipZ( waterHeight );
	}
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldViewDeferred::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	if( m_DrawFlags & (DF_RENDER_REFRACTION | DF_RENDER_REFLECTION) )
	{
		if ( IsX360() )
		{
			// these renders paths used their surfaces, so blit their results
			if ( m_DrawFlags & DF_RENDER_REFRACTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterRefractionTexture(), NULL, NULL );
			}
			if ( m_DrawFlags & DF_RENDER_REFLECTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterReflectionTexture(), NULL, NULL );
			}
		}

		render->PopView( GetFrustum() );
	}
}


//-----------------------------------------------------------------------------
// Draws the world + entities
//-----------------------------------------------------------------------------
void CBaseWorldViewDeferred::DrawSetup( float waterHeight, int nSetupFlags, float waterZAdjust, int iForceViewLeaf, bool bShadowDepth )
{
	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = bShadowDepth ? VIEW_DEFERRED_SHADOW : VIEW_MAIN;

	bool bViewChanged = AdjustView( waterHeight );

	if ( bViewChanged )
	{
		render->Push3DView( *this, 0, NULL, GetFrustum() );
	}

	bool bDrawEntities = ( nSetupFlags & DF_DRAW_ENTITITES ) != 0;
	bool bDrawReflection = ( nSetupFlags & DF_RENDER_REFLECTION ) != 0;
	BuildWorldRenderLists( bDrawEntities, iForceViewLeaf, ShouldCacheLists(), false, bDrawReflection ? &waterHeight : NULL );

	PruneWorldListInfo();

	if ( bDrawEntities )
	{
		bool bOptimized = bShadowDepth || savedViewID == VIEW_DEFERRED_GBUFFER;
		BuildRenderableRenderLists( bOptimized ? VIEW_SHADOW_DEPTH_TEXTURE : savedViewID );
	}

	if ( bViewChanged )
	{
		render->PopView( GetFrustum() );
	}

	g_CurrentViewID = savedViewID;
}


void CBaseWorldViewDeferred::DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust, bool bShadowDepth )
{
	// @MULTICORE (toml 8/16/2006): rethink how, where, and when this is done...
	//g_pClientShadowMgr->ComputeShadowTextures( *this, m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafDataList );

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = viewID;

	// Update our render view flags.
	int iDrawFlagsBackup = m_DrawFlags;
	m_DrawFlags |= m_pMainView->GetBaseDrawFlags();

	PushView( waterHeight );

	CMatRenderContextPtr pRenderContext( materials );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
#endif

	ITexture *pSaveFrameBufferCopyTexture = pRenderContext->GetFrameBufferCopyTexture( 0 );
	pRenderContext->SetFrameBufferCopyTexture( GetPowerOfTwoFrameBufferTexture() );
	pRenderContext.SafeRelease();


	Begin360ZPass();
	m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	DrawWorldDeferred( waterZAdjust );
	m_DrawFlags &= ~DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	if ( m_DrawFlags & DF_DRAW_ENTITITES )
	{
		DrawOpaqueRenderablesDeferred( m_bDrawWorldNormal );
	}
	End360ZPass();		// DrawOpaqueRenderables currently already calls End360ZPass. No harm in calling it again to make sure we're always ending it

	// Only draw decals on opaque surfaces after now. Benefit is two-fold: Early Z benefits on PC, and
	// we're pulling out stuff that uses the dynamic VB from the 360 Z pass
	// (which can lead to rendering corruption if we overflow the dyn. VB ring buffer).
	if ( !bShadowDepth )
	{
		m_DrawFlags |= DF_SKIP_WORLD;
		DrawWorldDeferred( waterZAdjust );
		m_DrawFlags &= ~DF_SKIP_WORLD;
	}
		
	if ( !m_bDrawWorldNormal )
	{
		if ( m_DrawFlags & DF_DRAW_ENTITITES )
		{
			DrawTranslucentRenderables( false, false );
			if ( !bShadowDepth )
				DrawNoZBufferTranslucentRenderables();
		}
		else
		{
			// Draw translucent world brushes only, no entities
			DrawTranslucentWorldInLeaves( false );
		}
	}

	pRenderContext.GetFrom( materials );
	pRenderContext->SetFrameBufferCopyTexture( pSaveFrameBufferCopyTexture );
	PopView();

	m_DrawFlags = iDrawFlagsBackup;

	g_CurrentViewID = savedViewID;

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CBaseWorldViewDeferred::PushComposite()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_COMPOSITION );
}

void CBaseWorldViewDeferred::PopComposite()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );
}

//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldViewDeferred::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox,
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_ClearFlags = nClearFlags;
	m_DrawFlags = DF_DRAW_ENTITITES;

	if ( !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	}
	else
	{
		bool bViewIntersectsWater = DoesViewPlaneIntersectWater( fogInfo.m_flWaterHeight, fogInfo.m_nVisibleFogVolume );
		if( bViewIntersectsWater )
		{
			// have to draw both sides if we can see both.
			m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
		}
		else if ( fogInfo.m_bEyeInFogVolume )
		{
			m_DrawFlags |= DF_RENDER_UNDERWATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_ABOVEWATER;
		}
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}

	if ( !fogInfo.m_bEyeInFogVolume && bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	m_pCustomVisibility = pCustomVisibility;
	m_fogInfo = fogInfo;
}

//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldViewDeferred::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldViewDeferred::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif
	pRenderContext.SafeRelease();

	PushComposite();

	DrawSetup( 0, m_DrawFlags, 0 );

	if ( !m_fogInfo.m_bEyeInFogVolume )
	{
		EnableWorldFog();
	}
	else
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;

		SetFogVolumeState( m_fogInfo, false );

		pRenderContext.GetFrom( materials );

		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	pRenderContext.SafeRelease();

	DrawExecute( 0, CurrentViewID(), 0 );

	PopComposite();

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}


void CPostLightingView::Setup( const CViewSetup &view )
{
	m_fogInfo.m_bEyeInFogVolume = false;

	BaseClass::Setup( view );

	m_ClearFlags = 0;

	m_DrawFlags = DF_DRAW_ENTITITES | DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
}

void CPostLightingView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldViewDeferred::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	ITexture *pTexAlbedo = GetDefRT_Albedo();
	pRenderContext->CopyRenderTargetToTexture( pTexAlbedo );
	pRenderContext->PushRenderTargetAndViewport( pTexAlbedo );
	pRenderContext.SafeRelease();
	PushComposite();

	SetupCurrentView( origin, angles, VIEW_MAIN );

	DrawSetup( 0, m_DrawFlags, 0 );

	DrawExecute( 0, CurrentViewID(), 0, false );

	PopComposite();

	pRenderContext.GetFrom( materials );
	pRenderContext->PopRenderTargetAndViewport();

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CPostLightingView::PushView( float waterHeight )
{
	//PushGBuffer( !m_bDrewSkybox );
	BaseClass::PushView( waterHeight );
}

void CPostLightingView::PopView()
{
	BaseClass::PopView();
}

void CPostLightingView::DrawWorldDeferred( float waterZAdjust )
{
#if 0
	int iOldDrawFlags = m_DrawFlags;

	m_DrawFlags &= ~DF_DRAW_ENTITITES;
	m_DrawFlags &= ~DF_RENDER_UNDERWATER;
	m_DrawFlags &= ~DF_RENDER_ABOVEWATER;
	m_DrawFlags &= ~DF_DRAW_ENTITITES;

	BaseClass::DrawWorldDeferred( waterZAdjust );

	m_DrawFlags = iOldDrawFlags;
#endif
}

void CPostLightingView::DrawOpaqueRenderablesDeferred( bool bNoDecals )
{
}

void CPostLightingView::PushDeferredShadingFrameBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Albedo() );
}

void CPostLightingView::PopDeferredShadingFrameBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopRenderTargetAndViewport();
}

void CBaseShadowView::Setup( const CViewSetup &view, ITexture *pDepthTexture, ITexture *pDummyTexture )
{
	m_pDepthTexture = pDepthTexture;
	m_pDummyTexture = pDummyTexture;

	BaseClass::Setup( view );

	m_bDrawWorldNormal = true;

	m_DrawFlags = DF_DRAW_ENTITITES | DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	m_ClearFlags = 0;

	CalcShadowView();

	m_pCustomVisibility = &shadowVis;
	shadowVis.AddVisOrigin( origin );
}

void CBaseShadowView::SetupRadiosityTargets( ITexture *pFluxTexture, ITexture *pNormalTexture, ITexture *pRawAlbedoTexture )
{
	m_pRadAlbedoTexture = pFluxTexture;
	m_pRadNormalTexture = pNormalTexture;
	m_pRadRawAlbedoTexture = pRawAlbedoTexture;
}

void CBaseShadowView::Draw()
{
	int oldViewID = g_CurrentViewID;
	SetupCurrentView( origin, angles, VIEW_DEFERRED_SHADOW );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_SHADOWPASS );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_MODE,
		GetShadowMode() );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_RADIOSITY,
		m_bOutputRadiosity ? 1 : 0 );
	pRenderContext.SafeRelease();
	
	DrawSetup( 0, m_DrawFlags, 0, -1, true );

	DrawExecute( 0, CurrentViewID(), 0, true );

	pRenderContext.GetFrom( materials );
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_RADIOSITY,
		0 );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );

	g_CurrentViewID = oldViewID;
}

bool CBaseShadowView::AdjustView( float waterHeight )
{
	CommitData();

	return true;
}

void CBaseShadowView::PushView( float waterHeight )
{
	CMatRenderContextPtr pRenderContext( materials );

	// Clear the persistent RSM attachments before binding them beside the shadow
	// colour target. Clearing after SetRenderTargetEx would also clear MRT0 and
	// corrupt the colour-depth shadow map's required white background.
	if ( m_bOutputRadiosity )
	{
		Assert( !IsErrorTexture( m_pRadAlbedoTexture ) );
		Assert( !IsErrorTexture( m_pRadNormalTexture ) );
		Assert( !IsErrorTexture( m_pRadRawAlbedoTexture ) );

		pRenderContext->PushRenderTargetAndViewport( m_pRadAlbedoTexture );
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();

		pRenderContext->PushRenderTargetAndViewport( m_pRadNormalTexture );
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();

		pRenderContext->PushRenderTargetAndViewport( m_pRadRawAlbedoTexture );
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();
	}

	render->Push3DView( *this, 0, m_pDummyTexture, GetFrustum(), m_pDepthTexture );
	pRenderContext->PushRenderTargetAndViewport( m_pDummyTexture, m_pDepthTexture, x, y, width, height );

#if defined( DEBUG ) || defined( SHADOWMAPPING_USE_COLOR )
	pRenderContext->ClearColor4ub( 255, 255, 255, 255 );
	pRenderContext->ClearBuffers( true, true );
#else
	pRenderContext->ClearBuffers( false, true );
#endif

	if ( m_bOutputRadiosity )
	{
		pRenderContext->SetRenderTargetEx( 1, m_pRadAlbedoTexture );
		pRenderContext->SetRenderTargetEx( 2, m_pRadNormalTexture );
		pRenderContext->SetRenderTargetEx( 3, m_pRadRawAlbedoTexture );
	}
}

void CBaseShadowView::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopRenderTargetAndViewport();

	render->PopView( GetFrustum() );
}

void CBaseShadowView::SetRadiosityOutputEnabled( bool bEnabled )
{
	m_bOutputRadiosity = bEnabled;
}

void CBaseShadowView::AddVisibilityOrigin( const Vector &visibilityOrigin )
{
	shadowVis.AddVisOrigin( visibilityOrigin );
}

void CRadianceHintsRSMView::CalcShadowView()
{
	lightData_Global_t state = GetActiveGlobalLightState();
	QAngle lightAngles;
	Vector rsmDirection = -state.vecLight.AsVector3D();
	VectorAngles( rsmDirection, lightAngles );

	Vector forward, right, up;
	AngleVectors( lightAngles, &forward, &right, &up );

	const Vector center = m_vecRHOrigin + Vector( m_flExtent, m_flExtent, m_flExtent ) * 0.5f;
	const float projectedHalfWidth = 0.5f * m_flExtent *
		( fabs( right.x ) + fabs( right.y ) + fabs( right.z ) );
	const float projectedHalfHeight = 0.5f * m_flExtent *
		( fabs( up.x ) + fabs( up.y ) + fabs( up.z ) );
	const float projectedHalfDepth = 0.5f * m_flExtent *
		( fabs( forward.x ) + fabs( forward.y ) + fabs( forward.z ) );
	const float padding = 2.0f * m_flCellSize;
	const float halfSide = MAX( projectedHalfWidth, projectedHalfHeight ) + padding;
	const float halfDepth = projectedHalfDepth + padding;

	m_flRSMWorldSide = MAX( halfSide * 2.0f, m_flCellSize );
	origin = center - forward * halfDepth + up * halfSide - right * halfSide;
	angles = lightAngles;

	x = y = 0;
	width = height = RH_RSM_RESOLUTION;
	m_bOrtho = true;
	m_OrthoLeft = 0.0f;
	m_OrthoTop = -m_flRSMWorldSide;
	m_OrthoRight = m_flRSMWorldSide;
	m_OrthoBottom = 0.0f;
	zNear = zNearViewmodel = 0.0f;
	zFar = zFarViewmodel = MAX( halfDepth * 2.0f, m_flCellSize * 4.0f );
	m_flAspectRatio = 0.0f;

	// Stable light-space texel snapping. Camera rotation never participates.
	const float worldPerTexel = m_flRSMWorldSide / RH_RSM_RESOLUTION;
	const float rightCoordinate = DotProduct( right, origin );
	const float upCoordinate = DotProduct( up, origin );
	const float snappedRight = floor( rightCoordinate / worldPerTexel + 0.5f ) * worldPerTexel;
	const float snappedUp = floor( upCoordinate / worldPerTexel + 0.5f ) * worldPerTexel;
	origin += right * ( snappedRight - rightCoordinate );
	origin += up * ( snappedUp - upCoordinate );

	const float depthQuantum = GetDepthMapDepthResolution( zFar - zNear );
	if ( depthQuantum > 0.0f )
	{
		const float depthCoordinate = DotProduct( forward, origin );
		const float snappedDepth = floor( depthCoordinate / depthQuantum + 0.5f ) * depthQuantum;
		origin += forward * ( snappedDepth - depthCoordinate );
	}
}

void CRadianceHintsRSMView::CommitData()
{
	struct sendRHData
	{
		shadowData_ortho_t shadow;
		VMatrix matRSMToWorld;
		Vector4D vecRSMParams;
		static void Fire( sendRHData d )
		{
			IDeferredExtension *pDef = GetDeferredExt();
			pDef->CommitShadowData_Ortho( 0, d.shadow );
			pDef->CommitDaylightGIRSMData( d.shadow.matWorldToTexture,
				d.matRSMToWorld, d.vecRSMParams );
		}
	};

	sendRHData packet;
	const cascade_t &referenceCascade = GetCascadeInfo( 0 );
	packet.shadow.iRes_x = RH_RSM_RESOLUTION;
	packet.shadow.iRes_y = RH_RSM_RESOLUTION;
	packet.shadow.vecSlopeSettings.Init(
		referenceCascade.flSlopeScaleMin,
		referenceCascade.flSlopeScaleMax,
		referenceCascade.flNormalScaleMax,
		1.0f / MAX( zFar, 1.0f ) );
	packet.shadow.vecOrigin.Init( origin );
#if CSM_USE_COMPOSITED_TARGET
	packet.shadow.vecUVTransform.Init( 0.0f, 0.0f, 1.0f, 1.0f );
#endif

	VMatrix viewMatrix, projectionMatrix, worldToProjection, worldToPixels;
	render->GetMatricesForView( *this, &viewMatrix, &projectionMatrix, &worldToProjection, &worldToPixels );
	VMatrix screenToTexture;
	MatrixBuildScale( screenToTexture, 0.5f, -0.5f, 1.0f );
	screenToTexture[0][3] = 0.5f;
	screenToTexture[1][3] = 0.5f;
	MatrixMultiply( screenToTexture, worldToProjection, packet.shadow.matWorldToTexture );

	VMatrix textureToWorld;
	MatrixInverseGeneral( packet.shadow.matWorldToTexture, textureToWorld );
	packet.matRSMToWorld = textureToWorld;
	packet.vecRSMParams.Init(
		(float)RH_RSM_RESOLUTION,
		1.0f / (float)RH_RSM_RESOLUTION,
		m_flRSMWorldSide,
		m_flCellSize );

	QUEUE_FIRE( sendRHData, Fire, packet );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX, 0 );
}


void COrthoShadowView::CalcShadowView()
{
	const cascade_t &m_data = GetCascadeInfo( iCascadeIndex );
	Vector mainFwd;
	AngleVectors( angles, &mainFwd );

	lightData_Global_t state = GetActiveGlobalLightState();
	QAngle lightAng;
	VectorAngles( -state.vecLight.AsVector3D(), lightAng );

	Vector viewFwd, viewRight, viewUp;
	AngleVectors( lightAng, &viewFwd, &viewRight, &viewUp );

	const float halfOrthoSize = m_data.flProjectionSize * 0.5f;

	origin += -viewFwd * m_data.flOriginOffset +
		viewUp * halfOrthoSize -
		viewRight * halfOrthoSize +
		mainFwd * halfOrthoSize;

	angles = lightAng;

	x = 0;
	y = 0;
	height = m_data.iResolution;
	width = m_data.iResolution;

	m_bOrtho = true;
	m_OrthoLeft = 0;
	m_OrthoTop = -m_data.flProjectionSize;
	m_OrthoRight = m_data.flProjectionSize;
	m_OrthoBottom = 0;

	zNear = zNearViewmodel = 0;
	zFar = zFarViewmodel = m_data.flFarZ;
	m_flAspectRatio = 0;

	float mapping_world = m_data.flProjectionSize / m_data.iResolution;
	origin -= fmod( DotProduct( viewRight, origin ), mapping_world ) * viewRight;
	origin -= fmod( DotProduct( viewUp, origin ), mapping_world ) * viewUp;

	origin -= fmod( DotProduct( viewFwd, origin ), GetDepthMapDepthResolution( zFar - zNear ) ) * viewFwd;

#if CSM_USE_COMPOSITED_TARGET
	x = m_data.iViewport_x;
	y = m_data.iViewport_y;
#endif
}

void COrthoShadowView::CommitData()
{
	struct sendShadowDataOrtho
	{
		shadowData_ortho_t data;
		int index;
		static void Fire( sendShadowDataOrtho d )
		{
			IDeferredExtension *pDef = GetDeferredExt();
			pDef->CommitShadowData_Ortho( d.index, d.data );
		};
	};

	const cascade_t &data = GetCascadeInfo( iCascadeIndex );

	Vector fwd, right, down;
	AngleVectors( angles, &fwd, &right, &down );
	down *= -1.0f;

	Vector vecScale( m_OrthoRight, abs( m_OrthoTop ), zFar - zNear );

	sendShadowDataOrtho shadowData;
	shadowData.index = iCascadeIndex;

#if CSM_USE_COMPOSITED_TARGET
	shadowData.data.iRes_x = CSM_COMP_RES_X;
	shadowData.data.iRes_y = CSM_COMP_RES_Y;
#else
	shadowData.data.iRes_x = width;
	shadowData.data.iRes_y = height;
#endif

	shadowData.data.vecSlopeSettings.Init(
		data.flSlopeScaleMin, data.flSlopeScaleMax, data.flNormalScaleMax, 1.0f / zFar
		);
	shadowData.data.vecOrigin.Init( origin );

	Vector4D matrix_scale_offset( 0.5f, -0.5f, 0.5f, 0.5f );

#if CSM_USE_COMPOSITED_TARGET
	shadowData.data.vecUVTransform.Init( x / (float)CSM_COMP_RES_X,
		y / (float)CSM_COMP_RES_Y,
		width / (float) CSM_COMP_RES_X,
		height / (float) CSM_COMP_RES_Y );
#endif

	VMatrix a,b,c,d,screenToTexture;
	render->GetMatricesForView( *this, &a, &b, &c, &d );
	MatrixBuildScale( screenToTexture, matrix_scale_offset.x,
		matrix_scale_offset.y,
		1.0f );

	screenToTexture[0][3] = matrix_scale_offset.z;
	screenToTexture[1][3] = matrix_scale_offset.w;

	MatrixMultiply( screenToTexture, c, shadowData.data.matWorldToTexture );

	QUEUE_FIRE( sendShadowDataOrtho, Fire, shadowData );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX, iCascadeIndex );
}


bool CDualParaboloidShadowView::AdjustView( float waterHeight )
{
	BaseClass::AdjustView( waterHeight );

	// HACK: when pushing our actual view the renderer fails building the worldlist right!
	// So we can't. Shit.
	return false;
}

void CDualParaboloidShadowView::PushView( float waterHeight )
{
	BaseClass::PushView( waterHeight );
}

void CDualParaboloidShadowView::PopView()
{
	BaseClass::PopView();
}

void CDualParaboloidShadowView::CalcShadowView()
{
	float flRadius = m_pLight->flRadius;

	m_bOrtho = true;
	m_OrthoTop = m_OrthoLeft = -flRadius;
	m_OrthoBottom = m_OrthoRight = flRadius;

	int dpsmRes = GetShadowResolution_Point();

	width = dpsmRes;
	height = dpsmRes;

	zNear = zNearViewmodel = 0;
	zFar = zFarViewmodel = flRadius;

	if ( m_bSecondary )
	{
		y = dpsmRes;

		Vector fwd, up;
		AngleVectors( angles, &fwd, NULL, &up );
		VectorAngles( -fwd, up, angles );
	}
}


void CSpotLightShadowView::CalcShadowView()
{
	float flRadius = m_pLight->flRadius;

	int spotRes = GetShadowResolution_Spot();

	width = spotRes;
	height = spotRes;

	zNear = zNearViewmodel = DEFLIGHT_SPOT_ZNEAR;
	zFar = zFarViewmodel = flRadius;

	fov = fovViewmodel = m_pLight->GetFOV();
}

void CSpotLightShadowView::CommitData()
{
	struct sendShadowDataProj
	{
		shadowData_proj_t data;
		int index;
		static void Fire( sendShadowDataProj d )
		{
			IDeferredExtension *pDef = GetDeferredExt();
			pDef->CommitShadowData_Proj( d.index, d.data );
		};
	};

	Vector fwd;
	AngleVectors( angles, &fwd );

	sendShadowDataProj data;
	data.index = m_iIndex;
	data.data.vecForward.Init( fwd );
	data.data.vecOrigin.Init( origin );
	// slope min, slope max, normal max, depth
	//data.data.vecSlopeSettings.Init( 0.005f, 0.02f, 3, zFar );
	data.data.vecSlopeSettings.Init( 0.001f, 0.005f, 3, 0 );

	QUEUE_FIRE( sendShadowDataProj, Fire, data );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX, m_iIndex );
}
