#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_gen_visibility_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_VISIBILITY, "RH 5.0 directional blocker injection" )
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
            EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

            pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( radiosity_gen_visibility_ps30 );
            SET_STATIC_PIXEL_SHADER( radiosity_gen_visibility_ps30 );
        }
        DYNAMIC_STATE
        {
            const radiosityData_t &data = GetDeferredExt()->GetRadiosityData();
            pShaderAPI->SetDefaultState();

            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_gen_visibility_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_gen_visibility_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_RadianceHintsRSMDepth() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_RadianceHintsRSMNormal() );

            pShaderAPI->SetPixelShaderConstant( 0, data.matWorldToRSM.Base(), 4 );
            pShaderAPI->SetPixelShaderConstant( 4, data.matRSMToWorld.Base(), 4 );

            const Vector &origin = data.vecOrigin[0];
            float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 8, originConstant );

            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef edgeFade( "deferred_rh_rsm_edge_fade" );
            ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
            ConVarRef visibilityInner( "deferred_rh_visibility_inner" );
            ConVarRef visibilityOuter( "deferred_rh_visibility_outer" );
            ConVarRef visibilityStrength( "deferred_rh_visibility_strength" );
            ConVarRef visibilityDecay( "deferred_rh_visibility_decay" );
            ConVarRef visibilityDilation( "deferred_rh_visibility_dilation" );
            ConVarRef visibilityNormalWeight( "deferred_rh_visibility_normal_weight" );

            const float cell = MAX( cellSize.GetFloat(), 1.0f );
            float settings[4] = {
                cell * RH_VOLUME_SIZE,
                cell,
                clamp( edgeFade.GetFloat(), 0.001f, 0.20f ),
                MAX( data.vecRSMParams.z, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 9, settings );

            const float inner = clamp( visibilityInner.GetFloat(), 0.0f, 2.0f );
            float visibility[4] = {
                geometryEnable.GetBool() ? 1.0f : 0.0f,
                inner,
                clamp( visibilityOuter.GetFloat(), inner + 0.01f, 3.0f ),
                clamp( visibilityStrength.GetFloat(), 0.0f, 4.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 10, visibility );

            float decay[4] = {
                clamp( visibilityDecay.GetFloat(), 0.0f, 4.0f ),
                data.vecRSMParams.y,
                clamp( visibilityDilation.GetFloat(), 0.0f, 1.5f ),
                clamp( visibilityNormalWeight.GetFloat(), 0.0f, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 11, decay );

            ConVarRef backRSM( "deferred_rh_back_rsm_enable" );
            const float passScale = backRSM.GetBool() ? 0.5f : 1.0f;
            float pass[4] = { passScale, 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 12, pass );
        }

        Draw();
    }
END_SHADER
