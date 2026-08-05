#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_gen_global_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_GLOBAL, "RH 4.2 deterministic radiance injection" )
    BEGIN_SHADER_PARAMS
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() {}
    SHADER_INIT
    {
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

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
        }
        DYNAMIC_STATE
        {
            const radiosityData_t &data = GetDeferredExt()->GetRadiosityData();
            pShaderAPI->SetDefaultState();

            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_RadianceHintsRSMFlux() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_RadianceHintsRSMNormal() );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_RadianceHintsRSMDepth() );

            pShaderAPI->SetPixelShaderConstant( 0, data.matWorldToRSM.Base(), 4 );
            pShaderAPI->SetPixelShaderConstant( 4, data.matRSMToWorld.Base(), 4 );

            const Vector &origin = data.vecOrigin[0];
            float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 8, originConstant );

            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef worldSpread( "deferred_rh_world_spread" );
            ConVarRef injectionGain( "deferred_rh_injection_gain" );
            ConVarRef edgeFade( "deferred_rh_rsm_edge_fade" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );

            const float cell = MAX( cellSize.GetFloat(), 1.0f );
            float settings[4] = {
                cell * RH_VOLUME_SIZE,
                cell,
                MAX( worldSpread.GetFloat(), cell * 1.5f ),
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
        }

        Draw();
    }
END_SHADER
