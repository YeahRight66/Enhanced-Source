#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_propagate_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_PROPAGATE, "RH 4.2 confidence/visibility-aware secondary bounce" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered red SH volume" )
        SHADER_PARAM( SHG, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered green SH volume" )
        SHADER_PARAM( SHB, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered blue SH volume" )
        SHADER_PARAM( VIS, SHADER_PARAM_TYPE_TEXTURE, "", "Directional visibility volume" )
        SHADER_PARAM( META, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered injection metadata" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() {}

    SHADER_INIT
    {
        LoadTexture( SHR );
        LoadTexture( SHG );
        LoadTexture( SHB );
        LoadTexture( VIS );
        LoadTexture( META );
    }

    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false );
            pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( true );

            pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            DECLARE_STATIC_PIXEL_SHADER( radiosity_propagate_ps30 );
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

            ConVarRef bounceGain( "deferred_rh_bounce_gain" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
            ConVarRef geometryStrength( "deferred_rh_geometry_strength" );
            ConVarRef minTransmittance( "deferred_rh_geometry_min_transmittance" );
            ConVarRef geometryBias( "deferred_rh_geometry_bias" );
            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef traceWidth( "deferred_rh_bounce_trace_width" );
            ConVarRef minConfidence( "deferred_rh_bounce_min_confidence" );

            float settings[4] = {
                2.25f / RH_VOLUME_SIZE_F,
                MAX( bounceGain.GetFloat(), 0.0f ),
                MAX( maxRadiance.GetFloat(), 0.25f ),
                2.25f
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
                0.0f,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 3, trace );
        }

        Draw();
    }
END_SHADER
