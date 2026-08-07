#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_filter_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_FILTER, "RH 6.0 symmetric confidence/visibility-aware spatial reconstruction" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR, SHADER_PARAM_TYPE_TEXTURE, "", "Raw red SH volume" )
        SHADER_PARAM( SHG, SHADER_PARAM_TYPE_TEXTURE, "", "Raw green SH volume" )
        SHADER_PARAM( SHB, SHADER_PARAM_TYPE_TEXTURE, "", "Raw blue SH volume" )
        SHADER_PARAM( META, SHADER_PARAM_TYPE_TEXTURE, "", "Raw injection metadata" )
        SHADER_PARAM( VIS, SHADER_PARAM_TYPE_TEXTURE, "", "Directional visibility volume" )
        SHADER_PARAM( GEOMETRY, SHADER_PARAM_TYPE_TEXTURE, "", "Conservative geometry occupancy volume" )
        SHADER_PARAM( FILTERPHASE, SHADER_PARAM_TYPE_INTEGER, "0", "0=X axis, 1=Y axis, 2=Z axis" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[FILTERPHASE]->IsDefined() )
            params[FILTERPHASE]->SetIntValue( 0 );
    }

    SHADER_INIT
    {
        LoadTexture( SHR );
        LoadTexture( SHG );
        LoadTexture( SHB );
        LoadTexture( META );
        LoadTexture( VIS );
        LoadTexture( GEOMETRY );
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
            pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            DECLARE_STATIC_PIXEL_SHADER( radiosity_filter_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( FILTER_PHASE, clamp( params[FILTERPHASE]->GetIntValue(), 0, 2 ) );
            SET_STATIC_PIXEL_SHADER( radiosity_filter_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_filter_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_filter_ps30 );

            BindTexture( SHADER_SAMPLER0, SHR );
            BindTexture( SHADER_SAMPLER1, SHG );
            BindTexture( SHADER_SAMPLER2, SHB );
            BindTexture( SHADER_SAMPLER3, META );
            BindTexture( SHADER_SAMPLER4, VIS );
            BindTexture( SHADER_SAMPLER5, GEOMETRY );

            ConVarRef filterStrength( "deferred_rh_filter_strength" );
            ConVarRef fillBoost( "deferred_rh_filter_fill_boost" );
            ConVarRef energyScale( "deferred_rh_filter_energy_scale" );
            ConVarRef directionPreserve( "deferred_rh_filter_direction_preserve" );
            ConVarRef geometryStrength( "deferred_rh_geometry_strength" );
            ConVarRef geometryBias( "deferred_rh_geometry_bias" );
            ConVarRef minTransmittance( "deferred_rh_geometry_min_transmittance" );
            ConVarRef filterRadius( "deferred_rh_filter_radius" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );

            float c0[4] = {
                clamp( filterStrength.GetFloat(), 0.0f, 1.0f ),
                clamp( fillBoost.GetFloat(), 1.0f, 4.0f ),
                MAX( energyScale.GetFloat(), 0.0f ),
                clamp( directionPreserve.GetFloat(), 0.0f, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 0, c0 );

            float c1[4] = {
                clamp( geometryStrength.GetFloat(), 0.0f, 4.0f ),
                clamp( geometryBias.GetFloat(), 0.0f, 0.95f ),
                clamp( minTransmittance.GetFloat(), 0.0f, 1.0f ),
                clamp( filterRadius.GetFloat(), 0.5f, 1.5f )
            };
            pShaderAPI->SetPixelShaderConstant( 1, c1 );

            float c2[4] = { MAX( maxRadiance.GetFloat(), 0.25f ), 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 2, c2 );

        }
        Draw();
    }
END_SHADER
