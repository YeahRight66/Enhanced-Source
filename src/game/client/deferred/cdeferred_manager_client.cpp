
#include "cbase.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "filesystem.h"
#include "deferred/deferred_shared_common.h"
#include "../../../materialsystem/deferredshaders/radiance_hints_config.h"

#include "vgui_controls/messagebox.h"

static CDeferredManagerClient __g_defmanager;
CDeferredManagerClient *GetDeferredManager()
{
	return &__g_defmanager;
}

static IViewRender *g_pCurrentViewRender = NULL;

IViewRender *GetViewRenderInstance()
{
	AssertMsg( g_pCurrentViewRender != NULL, "viewrender creation failed!" );

	return g_pCurrentViewRender;
}

static CDeferredMaterialSystem g_DeferredMaterialSystem;
static IMaterialSystem *g_pOldMatSystem;


CDeferredManagerClient::CDeferredManagerClient() : BaseClass( "DeferredManagerClient" )
{
	m_bDefRenderingEnabled = false;

	Q_memset( m_pMat_Def, 0, sizeof(IMaterial*) * DEF_MAT_COUNT );
	Q_memset( m_pKV_Def, 0, sizeof(KeyValues*) * DEF_MAT_COUNT );
}

CDeferredManagerClient::~CDeferredManagerClient()
{
}

void CopyDev()
{
	FileFindHandle_t handle;
	char steamappsPath[MAX_PATH*4];
	const char *pszGameDir = engine->GetGameDirectory();

	Q_strcpy( steamappsPath, pszGameDir );
	Q_StripLastDir( steamappsPath, sizeof(steamappsPath) );
	Q_StripLastDir( steamappsPath, sizeof(steamappsPath) );

	char searchPath[MAX_PATH*4];
	Q_snprintf( searchPath, sizeof(searchPath), "%s\\shaders\\fxc\\*", pszGameDir );
	Q_FixSlashes( searchPath );
	Msg( "searching for shaders in: %s\n", searchPath );

	const char *pszName = g_pFullFileSystem->FindFirst( searchPath, &handle );

	while ( pszName != NULL )
	{
		if ( Q_strlen( pszName ) > 4 )
		{
			char filename[MAX_PATH];
			Q_FileBase( pszName, filename, sizeof( filename ) );

			char filepath_src[MAX_PATH];
			char filepath_dst[MAX_PATH];
			Q_snprintf( filepath_src, sizeof( filepath_src ), "%s\\shaders\\fxc\\%s.vcs\0", pszGameDir, filename );
			Q_snprintf( filepath_dst, sizeof( filepath_dst ), "%s\\..\\platform\\shaders\\fxc\\%s.vcs\0", pszGameDir, filename );
			Q_FixSlashes( filepath_src );
			Q_FixSlashes( filepath_dst );

			Msg( "%s --> %s\n", filepath_src, filepath_dst );
			engine->CopyFile( filepath_src, filepath_dst );
		}

		pszName = g_pFullFileSystem->FindNext( handle );
	}

	g_pFullFileSystem->FindClose( handle );
}

bool CDeferredManagerClient::Init()
{
	// CopyDev();

	const bool bDeferredEnabled = CommandLine() && CommandLine()->FindParm( "-nodeferred" ) == 0;

	if (!bDeferredEnabled) {
		Msg( "Using stdshaders, because of -nodeferred param.\n" );
		g_pCurrentViewRender = new CViewRender();

		return true;
	}

	AssertMsg( g_pCurrentViewRender == NULL, "viewrender already allocated?!" );

	const bool bForceDeferred = CommandLine() && CommandLine()->FindParm("-forcedeferred") != 0;
	bool bSM30 = g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 95;

	if ( !bSM30 )
	{
		Warning( "The engine doesn't recognize your GPU to support SM3.0, running deferred anyway...\n" );
		bSM30 = true;
	}

	if ( bSM30 || bForceDeferred )
	{
		bool bGotDefShaderDll = ConnectDeferredExt();

		if ( bGotDefShaderDll )
		{
			g_pOldMatSystem = materials;

			g_DeferredMaterialSystem.InitPassThru( materials );
			materials = &g_DeferredMaterialSystem;
			engine->Mat_Stub( &g_DeferredMaterialSystem );

			m_bDefRenderingEnabled = true;
			GetDeferredExt()->EnableDeferredLighting();

			g_pCurrentViewRender = new CDeferredViewRender();

			ConVarRef r_shadows( "r_shadows" );
			r_shadows.SetValue( "0" );

			InitDeferredRTs( true );

			materials->AddModeChangeCallBack( &DefRTsOnModeChanged );

			InitializeDeferredMaterials();
		}
	}

	if ( !m_bDefRenderingEnabled )
	{
		Assert( g_pCurrentViewRender == NULL );

		Warning( "Your hardware does not seem to support shader model 3.0. If you think that this is an error (hybrid GPUs), add -forcedeferred as start parameter.\n" );
		g_pCurrentViewRender = new CViewRender();
	}
	else
	{
#define VENDOR_NVIDIA 0x10DE
#define VENDOR_INTEL 0x8086
#define VENDOR_ATI 0x1002
#define VENDOR_AMD 0x1022

#ifndef SHADOWMAPPING_USE_COLOR
		MaterialAdapterInfo_t info;
		materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );

		if ( info.m_VendorID == VENDOR_ATI ||
			info.m_VendorID == VENDOR_AMD )
		{
			vgui::MessageBox *pATIWarning = new vgui::MessageBox("UNSUPPORTED HARDWARE", VarArgs( "AMD/ATI IS NOT YET SUPPORTED IN HARDWARE FILTERING MODE\n"
				"(cdeferred_manager_client.cpp #%i).", __LINE__ ) );

			pATIWarning->InvalidateLayout();
			pATIWarning->DoModal();
		}
#endif
	}

	return true;
}

void CDeferredManagerClient::Shutdown()
{
	def_light_t::ShutdownSharedMeshes();

	ShutdownDeferredMaterials();
	ShutdownDeferredExt();

	if ( IsDeferredRenderingEnabled() )
	{
		materials->RemoveModeChangeCallBack( &DefRTsOnModeChanged );

		materials = g_pOldMatSystem;
		engine->Mat_Stub( g_pOldMatSystem );
	}

	delete g_pCurrentViewRender;
	g_pCurrentViewRender = NULL;
	view = NULL;
}

ImageFormat CDeferredManagerClient::GetShadowDepthFormat()
{
	ImageFormat f = g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat();

	// hack for hybrid stuff
	if ( f == IMAGE_FORMAT_UNKNOWN )
		f = IMAGE_FORMAT_D16_SHADOW;

	return f;
}

ImageFormat CDeferredManagerClient::GetNullFormat()
{
	return g_pMaterialSystemHardwareConfig->GetNullTextureFormat();
}

#define DEF_WRITE_VMT

void CDeferredManagerClient::InitializeDeferredMaterials()
{
	// BUG!!! Creating the materials directly is causing some weird performance bug at map start (high cpu load)
	// Instead write each material to file and then find them.
#ifdef DEF_WRITE_VMT

	// Make sure the directory exists
	if( filesystem->FileExists("materials/deferred", "MOD") == false )
	{
		filesystem->CreateDirHierarchy("materials/deferred", "MOD");
	}

#if DEBUG
	m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] = new KeyValues( "wireframe" );
	if ( m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SetString( "$color", "[1 0.5 0.1]" );
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SaveToFile( filesystem, "materials/deferred/lightworld_wirefram.vmt", "MOD" );
	}
	m_pMat_Def[ DEF_MAT_WIREFRAME_DEBUG ] = materials->FindMaterial( "deferred/lightworld_wirefram", NULL );
#endif

	// Create Materials
	m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] = new KeyValues( "LIGHTING_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ]->SaveToFile( filesystem, "materials/deferred/lightpass_global.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_fs.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_w.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_fs.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_w.vmt", "MOD" );
	}

	// Find the materials
	m_pMat_Def[ DEF_MAT_LIGHT_GLOBAL ] = materials->FindMaterial( "deferred/lightpass_global", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_point_fs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = materials->FindMaterial( "deferred/lightpass_point_w", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_spot_fs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = materials->FindMaterial( "deferred/lightpass_spot_w", NULL );

	/*
	lighting volumes
	*/

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_vfs.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_v.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_vfs.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_v.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = new KeyValues( "VOLUME_PREPASS" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ]->SaveToFile( filesystem, "materials/deferred/volume_prepass.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = new KeyValues( "VOLUME_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SaveToFile( filesystem, "materials/deferred/volume_blend.vmt", "MOD" );
	}

	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_point_vfs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = materials->FindMaterial( "deferred/lightpass_point_v", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_spot_vfs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = materials->FindMaterial( "deferred/lightpass_spot_v", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = materials->FindMaterial( "deferred/volume_prepass", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = materials->FindMaterial( "deferred/volume_blend", NULL );

	/*
	Radiance Hints
	*/

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SaveToFile( filesystem, "materials/deferred/radpass_global.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_global_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetInt( "$SAMPLEPHASE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SaveToFile( filesystem, "materials/deferred/radpass_global_2.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetInt( "$SAMPLEPHASE", 3 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SaveToFile( filesystem, "materials/deferred/radpass_global_3.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetInt( "$SURFACEMODE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SaveToFile( filesystem, "materials/deferred/radpass_sky.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetInt( "$SURFACEMODE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_sky_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetInt( "$SURFACEMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SaveToFile( filesystem, "materials/deferred/radpass_surface.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetInt( "$SURFACEMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_surface_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] = new KeyValues( "RADIOSITY_VISIBILITY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ]->SetInt( "$VISIBILITYPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ]->SaveToFile( filesystem, "materials/deferred/radpass_visibility.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] = new KeyValues( "RADIOSITY_VISIBILITY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ]->SetInt( "$VISIBILITYPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_visibility_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetInt( "$FILTERPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SaveToFile( filesystem, "materials/deferred/radpass_filter.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$META", GetDefRT_RadianceHints( 1, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetInt( "$FILTERPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_filter_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHR", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHG", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHB", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$META", GetDefRT_RadianceHints( 2, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetInt( "$FILTERPHASE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SaveToFile( filesystem, "materials/deferred/radpass_filter_2.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetInt( "$BOUNCEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_0.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetInt( "$BOUNCEMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetInt( "$BOUNCEMODE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_2.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetInt( "$BOUNCEMODE", 3 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_3.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetInt( "$BOUNCEMODE", 4 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_4.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetInt( "$BOUNCEMODE", 5 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_5.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetInt( "$BOUNCEMODE", 6 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_6.vmt", "MOD" );
	}


	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetInt( "$SOURCELEVEL", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetInt( "$COMBINED", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SaveToFile( filesystem, "materials/deferred/radpass_hierarchy_first.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetInt( "$SOURCELEVEL", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetInt( "$COMBINED", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SaveToFile( filesystem, "materials/deferred/radpass_hierarchy_combined.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHR0", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHG0", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHB0", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHR1", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHG1", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHB1", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetInt( "$SOURCELEVEL", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetInt( "$COMBINED", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SaveToFile( filesystem, "materials/deferred/radpass_hierarchy_10.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHR0", GetDefRT_RHHierarchy( 1, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHG0", GetDefRT_RHHierarchy( 1, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHB0", GetDefRT_RHHierarchy( 1, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHR1", GetDefRT_RHHierarchy( 1, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHG1", GetDefRT_RHHierarchy( 1, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHB1", GetDefRT_RHHierarchy( 1, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetInt( "$SOURCELEVEL", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetInt( "$COMBINED", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SaveToFile( filesystem, "materials/deferred/radpass_hierarchy_5.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$META0", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$AUXTEXTURE", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA0", GetDefRT_RHShadowGeometry32()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA1", GetDefRT_RHShadowDistance32()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA2", GetDefRT_RHShadowGeometry16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA3", GetDefRT_RHShadowDistance16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA4", GetDefRT_RHShadowGeometry16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetInt( "$PASSMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SaveToFile( filesystem, "materials/deferred/radpass_shadow.vmt", "MOD" );
	}


	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$META0", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$AUXTEXTURE", GetDefRT_RHShadowHalf()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA0", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA1", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA2", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA3", GetDefRT_RHHierarchyEnergy( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA4", GetDefRT_RHHierarchyEnergy( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetInt( "$PASSMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SaveToFile( filesystem, "materials/deferred/radpass_blend.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] = new KeyValues( "RADIOSITY_UPSAMPLE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ]->SetString( "$BASETEXTURE", GetDefRT_RHIndirectHalf()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ]->SaveToFile( filesystem, "materials/deferred/radpass_upsample.vmt", "MOD" );
	}

	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = materials->FindMaterial( "deferred/radpass_global", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] = materials->FindMaterial( "deferred/radpass_global_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] = materials->FindMaterial( "deferred/radpass_global_2", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] = materials->FindMaterial( "deferred/radpass_global_3", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] = materials->FindMaterial( "deferred/radpass_sky", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] = materials->FindMaterial( "deferred/radpass_sky_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] = materials->FindMaterial( "deferred/radpass_surface", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] = materials->FindMaterial( "deferred/radpass_surface_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] = materials->FindMaterial( "deferred/radpass_visibility", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] = materials->FindMaterial( "deferred/radpass_visibility_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] = materials->FindMaterial( "deferred/radpass_filter", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] = materials->FindMaterial( "deferred/radpass_filter_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] = materials->FindMaterial( "deferred/radpass_filter_2", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = materials->FindMaterial( "deferred/radpass_prop_0", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = materials->FindMaterial( "deferred/radpass_prop_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] = materials->FindMaterial( "deferred/radpass_prop_2", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] = materials->FindMaterial( "deferred/radpass_prop_3", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] = materials->FindMaterial( "deferred/radpass_prop_4", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] = materials->FindMaterial( "deferred/radpass_prop_5", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] = materials->FindMaterial( "deferred/radpass_prop_6", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] = materials->FindMaterial( "deferred/radpass_hierarchy_first", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] = materials->FindMaterial( "deferred/radpass_hierarchy_combined", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] = materials->FindMaterial( "deferred/radpass_hierarchy_10", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] = materials->FindMaterial( "deferred/radpass_hierarchy_5", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] = materials->FindMaterial( "deferred/radpass_shadow", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = materials->FindMaterial( "deferred/radpass_blend", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] = materials->FindMaterial( "deferred/radpass_upsample", NULL );

#if DEFCFG_DEFERRED_SHADING == 1
	/*
	deferred shading
	*/

	m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] = new KeyValues( "SCREENSPACE_SHADING" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] != NULL )
		m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ]->SaveToFile( filesystem, "materials/deferred/screenspace_shading.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = new KeyValues( "SCREENSPACE_COMBINE" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] != NULL )
		m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ]->SaveToFile( filesystem, "materials/deferred/screenspace_combine.vmt", "MOD" );

	m_pMat_Def[ DEF_MAT_SCREENSPACE_SHADING ] = materials->FindMaterial( "deferred/screenspace_shading", NULL );
	m_pMat_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = materials->FindMaterial( "deferred/screenspace_combine", NULL );
#endif

	/*
	blur
	*/

	m_pKV_Def[ DEF_MAT_BLUR_G6_X ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_X ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SaveToFile( filesystem, "materials/deferred/blurpass_vbuf_x.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetInt( "$ISVERTICAL", 1 );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SaveToFile( filesystem, "materials/deferred/blurpass_vbuf_y.vmt", "MOD" );
	}

	m_pMat_Def[ DEF_MAT_BLUR_G6_X ] = materials->FindMaterial( "deferred/blurpass_vbuf_x", NULL );
	m_pMat_Def[ DEF_MAT_BLUR_G6_Y ] = materials->FindMaterial( "deferred/blurpass_vbuf_y", NULL );

#else // Create materials directly (caused performance bug)
#if DEBUG
	m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] = new KeyValues( "wireframe" );
	if ( m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SetString( "$color", "[1 0.5 0.1]" );
		m_pMat_Def[ DEF_MAT_WIREFRAME_DEBUG ] = materials->CreateMaterial( "__lightworld_wireframe", m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] );
	}
#endif

	m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] = new KeyValues( "LIGHTING_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_GLOBAL ] = materials->CreateMaterial( "__lightpass_global", m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_point_fs", m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = materials->CreateMaterial( "__lightpass_point_w", m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_spot_fs", m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = materials->CreateMaterial( "__lightpass_spot_w", m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] );
	}


	/*

	lighting volumes

	*/

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_point_vfs", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = materials->CreateMaterial( "__lightpass_point_v", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_spot_v", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = materials->CreateMaterial( "__lightpass_spot_v", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = new KeyValues( "VOLUME_PREPASS" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = materials->CreateMaterial( "__volume_prepass", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = new KeyValues( "VOLUME_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = materials->CreateMaterial( "__volume_blend", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] );
	}

	/*
	Radiance Hints (direct material creation fallback)
	*/

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = materials->CreateMaterial( "__radpass_global", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] = materials->CreateMaterial( "__radpass_global_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetInt( "$SAMPLEPHASE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] = materials->CreateMaterial( "__radpass_global_2", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_2 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetInt( "$SURFACEMODE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetInt( "$SAMPLEPHASE", 3 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] = materials->CreateMaterial( "__radpass_global_3", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL_3 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetInt( "$SURFACEMODE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] = materials->CreateMaterial( "__radpass_sky", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetInt( "$SURFACEMODE", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] = materials->CreateMaterial( "__radpass_sky_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SKY_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetInt( "$SURFACEMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetInt( "$SAMPLEPHASE", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] = materials->CreateMaterial( "__radpass_surface", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetInt( "$SURFACEMODE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetInt( "$SAMPLEPHASE", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$RSMALBEDO", GetDefRT_RHRSMAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ]->SetString( "$OPENSKY", GetDefRT_RHOpenSky()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] = materials->CreateMaterial( "__radpass_surface_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SURFACE_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] = new KeyValues( "RADIOSITY_VISIBILITY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ]->SetInt( "$VISIBILITYPHASE", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] = materials->CreateMaterial( "__radpass_visibility", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] = new KeyValues( "RADIOSITY_VISIBILITY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ]->SetInt( "$VISIBILITYPHASE", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] = materials->CreateMaterial( "__radpass_visibility_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_VISIBILITY_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ]->SetInt( "$FILTERPHASE", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] = materials->CreateMaterial( "__radpass_filter", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$META", GetDefRT_RadianceHints( 1, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ]->SetInt( "$FILTERPHASE", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] = materials->CreateMaterial( "__radpass_filter_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] = new KeyValues( "RADIOSITY_FILTER" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHR", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHG", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$SHB", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$META", GetDefRT_RadianceHints( 2, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ]->SetInt( "$FILTERPHASE", 2 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] = materials->CreateMaterial( "__radpass_filter_2", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_FILTER_2 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetInt( "$BOUNCEMODE", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = materials->CreateMaterial( "__radpass_prop_0", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetInt( "$BOUNCEMODE", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = materials->CreateMaterial( "__radpass_prop_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ]->SetInt( "$BOUNCEMODE", 2 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] = materials->CreateMaterial( "__radpass_prop_2", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_2 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHR", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHG", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHB", GetDefRT_RadianceHints( 1, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ]->SetInt( "$BOUNCEMODE", 3 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] = materials->CreateMaterial( "__radpass_prop_3", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_3 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ]->SetInt( "$BOUNCEMODE", 4 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] = materials->CreateMaterial( "__radpass_prop_4", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_4 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ]->SetInt( "$BOUNCEMODE", 5 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] = materials->CreateMaterial( "__radpass_prop_5", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_5 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHR", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHG", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHB", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$VIS", GetDefRT_RHVisibility()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$META", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$GEOMETRY", GetDefRT_RHGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$DISTANCE", GetDefRT_RHGeometryDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACEGUIDE", GetDefRT_RHSurfaceGuide()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SHADOWGEOMETRY", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACEALBEDO", GetDefRT_RHSurfaceAlbedo()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACENORMAL", GetDefRT_RHSurfaceNormal()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$SURFACECACHE", GetDefRT_RHSurfaceCache()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20R", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20G", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetString( "$HIER20B", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ]->SetInt( "$BOUNCEMODE", 6 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] = materials->CreateMaterial( "__radpass_prop_6", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_6 ] );
	}


	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetInt( "$SOURCELEVEL", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ]->SetInt( "$COMBINED", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] = materials->CreateMaterial( "__radpass_hierarchy_first", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_FIRST ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetInt( "$SOURCELEVEL", 0 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ]->SetInt( "$COMBINED", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] = materials->CreateMaterial( "__radpass_hierarchy_combined", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_COMBINED ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHR0", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHG0", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHB0", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHR1", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHG1", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetString( "$SHB1", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetInt( "$SOURCELEVEL", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ]->SetInt( "$COMBINED", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] = materials->CreateMaterial( "__radpass_hierarchy_10", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_10 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] = new KeyValues( "RADIOSITY_HIERARCHY" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHR0", GetDefRT_RHHierarchy( 1, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHG0", GetDefRT_RHHierarchy( 1, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHB0", GetDefRT_RHHierarchy( 1, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHR1", GetDefRT_RHHierarchy( 1, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHG1", GetDefRT_RHHierarchy( 1, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetString( "$SHB1", GetDefRT_RHHierarchy( 1, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetInt( "$SOURCELEVEL", 2 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ]->SetInt( "$COMBINED", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] = materials->CreateMaterial( "__radpass_hierarchy_5", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_HIERARCHY_5 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$META0", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$AUXTEXTURE", GetDefRT_RHShadowGeometry()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA0", GetDefRT_RHShadowGeometry32()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA1", GetDefRT_RHShadowDistance32()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA2", GetDefRT_RHShadowGeometry16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA3", GetDefRT_RHShadowDistance16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetString( "$EXTRA4", GetDefRT_RHShadowGeometry16()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ]->SetInt( "$PASSMODE", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] = materials->CreateMaterial( "__radpass_shadow", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_SHADOW ] );
	}


	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHR0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHG0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHB0", GetDefRT_RadianceHints( 0, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHR1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_R )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHG1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_G )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHB1", GetDefRT_RadianceHints( 2, RH_CHANNEL_SH_B )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$META0", GetDefRT_RadianceHints( 0, RH_CHANNEL_META )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$AUXTEXTURE", GetDefRT_RHShadowHalf()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$SHADOWDISTANCE", GetDefRT_RHShadowDistance()->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA0", GetDefRT_RHHierarchy( 0, 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA1", GetDefRT_RHHierarchy( 0, 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA2", GetDefRT_RHHierarchy( 0, 2 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA3", GetDefRT_RHHierarchyEnergy( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetString( "$EXTRA4", GetDefRT_RHHierarchyEnergy( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SetInt( "$PASSMODE", 0 );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = materials->CreateMaterial( "__radpass_blend", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] = new KeyValues( "RADIOSITY_UPSAMPLE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ]->SetString( "$BASETEXTURE", GetDefRT_RHIndirectHalf()->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] = materials->CreateMaterial( "__radpass_upsample", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_UPSAMPLE ] );
	}

#if DEFCFG_DEFERRED_SHADING == 1
	/*

	deferred shading

	*/

	m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] = new KeyValues( "SCREENSPACE_SHADING" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] != NULL )
		m_pMat_Def[ DEF_MAT_SCREENSPACE_SHADING ] = materials->CreateMaterial( "__screenspace_shading", m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] );

	m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = new KeyValues( "SCREENSPACE_COMBINE" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] != NULL )
		m_pMat_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = materials->CreateMaterial( "__screenspace_combine", m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] );
#endif

	/*

	blur

	*/

	m_pKV_Def[ DEF_MAT_BLUR_G6_X ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_X ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_BLUR_G6_X ] = materials->CreateMaterial( "__blurpass_vbuf_x", m_pKV_Def[ DEF_MAT_BLUR_G6_X ] );
	}

	m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetInt( "$ISVERTICAL", 1 );
		m_pMat_Def[ DEF_MAT_BLUR_G6_Y ] = materials->CreateMaterial( "__blurpass_vbuf_y", m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] );
	}

#if DEBUG
	for ( int i = 0; i < DEF_MAT_COUNT; i++ )
	{
		Assert( m_pKV_Def[ i ] != NULL );
		Assert( m_pMat_Def[ i ] != NULL );
	}
#endif
#endif
}

void CDeferredManagerClient::ShutdownDeferredMaterials()
{
	// not deleted on purpose!!!!!
	for ( int i = 0; i < DEF_MAT_COUNT; i++ )
	{
		if ( m_pKV_Def[ i ] != NULL )
			m_pKV_Def[ i ]->Clear();
		m_pKV_Def[ i ] = NULL;
	}
}
