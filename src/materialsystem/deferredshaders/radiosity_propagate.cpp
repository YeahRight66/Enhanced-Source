#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_propagate_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_PROPAGATE, "RH 9.0 SDF-guided physical surface bounce plus adaptive RH diffusion" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR, SHADER_PARAM_TYPE_TEXTURE, "", "Source red SH volume" )
        SHADER_PARAM( SHG, SHADER_PARAM_TYPE_TEXTURE, "", "Source green SH volume" )
        SHADER_PARAM( SHB, SHADER_PARAM_TYPE_TEXTURE, "", "Source blue SH volume" )
        SHADER_PARAM( VIS, SHADER_PARAM_TYPE_TEXTURE, "", "Directional visibility volume" )
        SHADER_PARAM( META, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce metadata" )
        SHADER_PARAM( GEOMETRY, SHADER_PARAM_TYPE_TEXTURE, "", "Conservative geometry occupancy" )
        SHADER_PARAM( DISTANCE, SHADER_PARAM_TYPE_TEXTURE, "", "Geometry distance field" )
        SHADER_PARAM( SURFACEALBEDO, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated RSM surface albedo" )
        SHADER_PARAM( SURFACENORMAL, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated RSM surface normal" )
        SHADER_PARAM( SHADOWDISTANCE, SHADER_PARAM_TYPE_TEXTURE, "", "RH8 64^3 Euclidean distance field" )
        SHADER_PARAM( SURFACEGUIDE, SHADER_PARAM_TYPE_TEXTURE, "", "RH9 independent surface normal/coverage guide" )
        SHADER_PARAM( SHADOWGEOMETRY, SHADER_PARAM_TYPE_TEXTURE, "", "RH8 64^3 static+dynamic blocker occupancy" )
        SHADER_PARAM( SURFACECACHE, SHADER_PARAM_TYPE_TEXTURE, "", "RH10 static Source material albedo cache" )
        SHADER_PARAM( HIER20R, SHADER_PARAM_TYPE_TEXTURE, "", "RH11 20^3 red SH hierarchy" )
        SHADER_PARAM( HIER20G, SHADER_PARAM_TYPE_TEXTURE, "", "RH11 20^3 green SH hierarchy" )
        SHADER_PARAM( HIER20B, SHADER_PARAM_TYPE_TEXTURE, "", "RH11 20^3 blue SH hierarchy" )
        SHADER_PARAM( BOUNCEMODE, SHADER_PARAM_TYPE_INTEGER, "0", "0=surface; 1..3=physical XYZ; 4..6=diffusion XYZ" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() {}

    SHADER_INIT
    {
        LoadTexture( SHR ); LoadTexture( SHG ); LoadTexture( SHB );
        LoadTexture( VIS ); LoadTexture( META ); LoadTexture( GEOMETRY );
        LoadTexture( DISTANCE ); LoadTexture( SURFACEALBEDO ); LoadTexture( SURFACENORMAL );
        LoadTexture( SHADOWDISTANCE ); LoadTexture( SURFACEGUIDE ); LoadTexture( SHADOWGEOMETRY );
        LoadTexture( SURFACECACHE ); LoadTexture( HIER20R ); LoadTexture( HIER20G ); LoadTexture( HIER20B );
    }

    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nBounceMode = clamp( params[ BOUNCEMODE ]->GetIntValue(), 0, 6 );
        const bool bAdditiveTransport = nBounceMode != 0;
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false );
            pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( true );
            if ( bAdditiveTransport )
                EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
            pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER11, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER12, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER13, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER14, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER15, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            DECLARE_STATIC_PIXEL_SHADER( radiosity_propagate_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( BOUNCE_MODE, nBounceMode );
            SET_STATIC_PIXEL_SHADER( radiosity_propagate_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_propagate_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_propagate_ps30 );

            BindTexture( SHADER_SAMPLER0, SHR );
            BindTexture( SHADER_SAMPLER1, SHG );
            BindTexture( SHADER_SAMPLER2, SHB );
            BindTexture( SHADER_SAMPLER3, VIS );
            BindTexture( SHADER_SAMPLER4, META );
            BindTexture( SHADER_SAMPLER5, GEOMETRY );
            BindTexture( SHADER_SAMPLER6, DISTANCE );
            BindTexture( SHADER_SAMPLER7, SURFACEALBEDO );
            BindTexture( SHADER_SAMPLER8, SURFACENORMAL );
            BindTexture( SHADER_SAMPLER9, SHADOWDISTANCE );
            BindTexture( SHADER_SAMPLER10, SURFACEGUIDE );
            BindTexture( SHADER_SAMPLER11, SHADOWGEOMETRY );
            BindTexture( SHADER_SAMPLER12, SURFACECACHE );
            BindTexture( SHADER_SAMPLER13, HIER20R );
            BindTexture( SHADER_SAMPLER14, HIER20G );
            BindTexture( SHADER_SAMPLER15, HIER20B );

            ConVarRef bounceGain( "deferred_rh_bounce_gain" );
            ConVarRef surfaceBounceGain( "deferred_rh_surface_bounce_gain" );
            ConVarRef surfaceMinCoverage( "deferred_rh_surface_min_coverage" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
            ConVarRef geometryStrength( "deferred_rh_geometry_strength" );
            ConVarRef minTransmittance( "deferred_rh_geometry_min_transmittance" );
            ConVarRef geometryBias( "deferred_rh_geometry_bias" );
            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef traceWidth( "deferred_rh_bounce_trace_width" );
            ConVarRef minConfidence( "deferred_rh_bounce_min_confidence" );
            ConVarRef diffusionGain( "deferred_rh_diffusion_gain" );
            ConVarRef diffusionRadius( "deferred_rh_diffusion_radius" );
            ConVarRef diffusionMinConfidence( "deferred_rh_diffusion_min_confidence" );
            ConVarRef surfaceCacheEnable( "deferred_rh_surface_cache_enable" );
            ConVarRef surfaceCacheBlend( "deferred_rh_surface_cache_blend" );
            ConVarRef surfaceFallbackAlbedo( "deferred_rh_surface_fallback_albedo" );
            ConVarRef adaptiveDiffusion( "deferred_rh_adaptive_diffusion" );
            ConVarRef adaptiveDiffusionNear( "deferred_rh_adaptive_diffusion_near" );
            ConVarRef adaptiveDiffusionFar( "deferred_rh_adaptive_diffusion_far" );
            ConVarRef materialCache( "deferred_rh_surface_material_cache" );
            ConVarRef materialScale( "deferred_rh_surface_material_scale" );
            ConVarRef classification( "deferred_rh_cell_classification" );
            ConVarRef hierarchyEnable( "deferred_rh_hierarchy_enable" );
            ConVarRef hierarchyDiffusion( "deferred_rh_hierarchy_diffusion_blend" );

            const float radiusCells = 2.25f;
            float settings[4] = {
                radiusCells / RH_VOLUME_SIZE_F,
                MAX( bounceGain.GetFloat(), 0.0f ),
                MAX( maxRadiance.GetFloat(), 0.25f ),
                radiusCells
            };
            pShaderAPI->SetPixelShaderConstant( 0, settings );

            float geometry[4] = {
                geometryEnable.GetBool() ? 1.0f : 0.0f,
                clamp( geometryStrength.GetFloat(), 0.0f, 4.0f ),
                clamp( minTransmittance.GetFloat(), 0.0f, 1.0f ),
                clamp( geometryBias.GetFloat(), 0.0f, 0.95f )
            };
            pShaderAPI->SetPixelShaderConstant( 1, geometry );

            const Vector &origin = GetDeferredExt()->GetRadiosityData().vecOrigin[0];
            float worldGrid[4] = {
                origin.x, origin.y, origin.z,
                MAX( cellSize.GetFloat(), 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 2, worldGrid );

            float trace[4] = {
                clamp( traceWidth.GetFloat(), 0.0f, 1.5f ),
                clamp( minConfidence.GetFloat(), 0.0f, 1.0f ),
                (float)nBounceMode,
                MAX( surfaceBounceGain.GetFloat(), 0.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 3, trace );

            float surface[4] = {
                clamp( surfaceMinCoverage.GetFloat(), 0.0f, 4.0f ),
                0.0f, 0.0f, 0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 4, surface );

            const float diffusionRadiusCells = clamp( diffusionRadius.GetFloat(), 0.75f, 4.0f );
            float diffusion[4] = {
                clamp( diffusionGain.GetFloat(), 0.0f, 0.35f ),
                diffusionRadiusCells / RH_VOLUME_SIZE_F,
                diffusionRadiusCells,
                clamp( diffusionMinConfidence.GetFloat(), 0.0f, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 5, diffusion );

            float surfaceGuide[4] = { surfaceCacheEnable.GetBool() ? 1.0f : 0.0f,
                clamp( surfaceCacheBlend.GetFloat(), 0.0f, 1.0f ),
                clamp( surfaceFallbackAlbedo.GetFloat(), 0.0f, 1.0f ), clamp( materialScale.GetFloat(), 0.0f, 2.0f ) };
            pShaderAPI->SetPixelShaderConstant( 6, surfaceGuide );
            float adaptiveDiff[4] = { adaptiveDiffusion.GetBool() ? 1.0f : 0.0f,
                clamp( adaptiveDiffusionNear.GetFloat(), 0.5f, 4.0f ),
                clamp( adaptiveDiffusionFar.GetFloat(), 0.5f, 5.0f ), 0.0f };
            pShaderAPI->SetPixelShaderConstant( 7, adaptiveDiff );

            const radiosityData_t &rhData = GetDeferredExt()->GetRadiosityData();
            const Vector &shadowOrigin = rhData.vecOrigin[1];
            const float extent = MAX( cellSize.GetFloat(), 1.0f ) * RH_VOLUME_SIZE;
            const float invExtent = 1.0f / MAX( extent, 1.0f );
            float shadowOffset[4] = {
                ( origin.x - shadowOrigin.x ) * invExtent,
                ( origin.y - shadowOrigin.y ) * invExtent,
                ( origin.z - shadowOrigin.z ) * invExtent,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 8, shadowOffset );
            float stage10_11[4] = { materialCache.GetBool() ? 1.0f : 0.0f,
                classification.GetBool() ? 1.0f : 0.0f, hierarchyEnable.GetBool() ? 1.0f : 0.0f,
                clamp( hierarchyDiffusion.GetFloat(), 0.0f, 1.0f ) };
            pShaderAPI->SetPixelShaderConstant( 9, stage10_11 );
        }

        Draw();
    }
END_SHADER
