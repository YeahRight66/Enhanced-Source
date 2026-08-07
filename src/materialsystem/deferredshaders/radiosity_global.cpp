#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_gen_global_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_GLOBAL, "RH 9.0 adaptive 16-sample sun/hemisphere and surface injection" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( RSMALBEDO, SHADER_PARAM_TYPE_TEXTURE, "", "Raw RSM material albedo" )
        SHADER_PARAM( GEOMETRY, SHADER_PARAM_TYPE_TEXTURE, "", "RH geometry occupancy" )
        SHADER_PARAM( DISTANCE, SHADER_PARAM_TYPE_TEXTURE, "", "RH geometry distance field" )
        SHADER_PARAM( SHADOWDISTANCE, SHADER_PARAM_TYPE_TEXTURE, "", "RH8 64^3 Euclidean distance field" )
        SHADER_PARAM( SURFACEGUIDE, SHADER_PARAM_TYPE_TEXTURE, "", "RH9 SDF-derived surface normal/coverage guide" )
        SHADER_PARAM( SURFACEMODE, SHADER_PARAM_TYPE_INTEGER, "0", "0=sun radiance, 1=surface attributes, 2=hemisphere sky" )
        SHADER_PARAM( SAMPLEPHASE, SHADER_PARAM_TYPE_INTEGER, "0", "0..3=four-sample stratified sun phases; sky/surface use 0..1" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[ SURFACEMODE ]->IsDefined() )
            params[ SURFACEMODE ]->SetIntValue( 0 );
        if ( !params[ SAMPLEPHASE ]->IsDefined() )
            params[ SAMPLEPHASE ]->SetIntValue( 0 );
    }
    SHADER_INIT
    {
        LoadTexture( RSMALBEDO );
        LoadTexture( GEOMETRY );
        LoadTexture( DISTANCE );
        LoadTexture( SHADOWDISTANCE );
        LoadTexture( SURFACEGUIDE );
    }
    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nSurfaceMode = clamp( params[ SURFACEMODE ]->GetIntValue(), 0, 2 );
        const int nSamplePhase = clamp( params[ SAMPLEPHASE ]->GetIntValue(), 0, 3 );
        const bool bAdditiveMode = nSurfaceMode != 0 || nSamplePhase != 0;
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false );
            pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( true );
            if ( bAdditiveMode )
                EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

            pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( SURFACE_MODE, nSurfaceMode );
            SET_STATIC_PIXEL_SHADER_COMBO( SAMPLE_PHASE, nSamplePhase );
            SET_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
        }
        DYNAMIC_STATE
        {
            const radiosityData_t &data = GetDeferredExt()->GetRadiosityData();
            const lightData_Global_t &globalLight = GetDeferredExt()->GetLightData_Global();
            pShaderAPI->SetDefaultState();

            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_RadianceHintsRSMFlux() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_RadianceHintsRSMNormal() );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_RadianceHintsRSMDepth() );
            BindTexture( SHADER_SAMPLER3, RSMALBEDO );
            BindTexture( SHADER_SAMPLER4, GEOMETRY );
            BindTexture( SHADER_SAMPLER5, DISTANCE );
            BindTexture( SHADER_SAMPLER6, SHADOWDISTANCE );
            BindTexture( SHADER_SAMPLER7, SURFACEGUIDE );

            pShaderAPI->SetPixelShaderConstant( 0, data.matWorldToRSM.Base(), 4 );
            pShaderAPI->SetPixelShaderConstant( 4, data.matRSMToWorld.Base(), 4 );

            const Vector &origin = data.vecOrigin[0];
            float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 8, originConstant );

            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef gatherRadiusCells( "deferred_rh_gather_radius_cells" );
            ConVarRef worldSpread( "deferred_rh_world_spread" );
            ConVarRef injectionGain( "deferred_rh_injection_gain" );
            ConVarRef edgeFade( "deferred_rh_rsm_edge_fade" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            ConVarRef skyEnable( "deferred_rh_sky_enable" );
            ConVarRef skyIntensity( "deferred_rh_sky_intensity" );
            ConVarRef skyUpperScale( "deferred_rh_sky_upper_scale" );
            ConVarRef skyLowerScale( "deferred_rh_sky_lower_scale" );
            ConVarRef skyOcclusion( "deferred_rh_sky_occlusion" );
            ConVarRef skyTraceDistance( "deferred_rh_sky_trace_distance" );
            ConVarRef surfaceRadius( "deferred_rh_surface_radius" );
            ConVarRef adaptiveGather( "deferred_rh_adaptive_gather" );
            ConVarRef adaptiveGatherNear( "deferred_rh_adaptive_gather_near" );
            ConVarRef adaptiveGatherFar( "deferred_rh_adaptive_gather_far" );
            ConVarRef relocation( "deferred_rh_cell_relocation" );
            ConVarRef surfaceCacheEnable( "deferred_rh_surface_cache_enable" );

            const float cell = MAX( cellSize.GetFloat(), 1.0f );
            const float legacyWorldRadius = worldSpread.GetFloat();
            const float gatherWorldRadius = legacyWorldRadius > 0.0f
                ? legacyWorldRadius
                : cell * clamp( gatherRadiusCells.GetFloat(), 1.5f, 10.0f );
            float settings[4] = {
                cell * RH_VOLUME_SIZE,
                cell,
                gatherWorldRadius,
                MAX( injectionGain.GetFloat(), 0.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 9, settings );

            float quality[4] = {
                clamp( edgeFade.GetFloat(), 0.001f, 0.20f ),
                MAX( maxRadiance.GetFloat(), 0.25f ),
                data.vecRSMParams.y,
                MAX( data.vecRSMParams.z, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 10, quality );

            const float upperScale = MAX( skyUpperScale.GetFloat(), 0.0f );
            const float lowerScale = MAX( skyLowerScale.GetFloat(), 0.0f );
            float upper[4] = {
                globalLight.ambh.x * upperScale,
                globalLight.ambh.y * upperScale,
                globalLight.ambh.z * upperScale,
                skyEnable.GetBool() ? 1.0f : 0.0f
            };
            float lower[4] = {
                globalLight.ambl.x * lowerScale,
                globalLight.ambl.y * lowerScale,
                globalLight.ambl.z * lowerScale,
                MAX( skyIntensity.GetFloat(), 0.0f )
            };
            float skySurface[4] = {
                MAX( skyOcclusion.GetFloat(), 0.0f ),
                clamp( skyTraceDistance.GetFloat(), 1.0f, 16.0f ),
                clamp( surfaceRadius.GetFloat(), 0.5f, 3.0f ),
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 11, upper );
            pShaderAPI->SetPixelShaderConstant( 12, lower );
            pShaderAPI->SetPixelShaderConstant( 13, skySurface );

            float adaptive[4] = { adaptiveGather.GetBool() ? 1.0f : 0.0f,
                clamp( adaptiveGatherNear.GetFloat(), 1.0f, 8.0f ),
                clamp( adaptiveGatherFar.GetFloat(), 1.0f, 10.0f ),
                clamp( relocation.GetFloat(), 0.0f, 0.45f ) };
            pShaderAPI->SetPixelShaderConstant( 14, adaptive );
            float surfaceGuide[4] = { surfaceCacheEnable.GetBool() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 15, surfaceGuide );
        }

        Draw();
    }
END_SHADER
