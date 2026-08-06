#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "defconstruct_vs30.inc"
#include "radiosity_blend_ps30.inc"

BEGIN_VS_SHADER( RADIOSITY_BLEND, "RH 5.0 half-resolution production reconstruction" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR0, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered first-bounce red SH" )
        SHADER_PARAM( SHG0, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered first-bounce green SH" )
        SHADER_PARAM( SHB0, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered first-bounce blue SH" )
        SHADER_PARAM( VIS0, SHADER_PARAM_TYPE_TEXTURE, "", "Directional visibility SH" )
        SHADER_PARAM( SHR1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce red SH" )
        SHADER_PARAM( SHG1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce green SH" )
        SHADER_PARAM( SHB1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce blue SH" )
        SHADER_PARAM( META0, SHADER_PARAM_TYPE_TEXTURE, "", "Filtered first-bounce metadata" )
        SHADER_PARAM( GEOMETRY, SHADER_PARAM_TYPE_TEXTURE, "", "Conservative geometry occupancy volume" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() {}

    SHADER_INIT
    {
        LoadTexture( SHR0 ); LoadTexture( SHG0 ); LoadTexture( SHB0 ); LoadTexture( VIS0 );
        LoadTexture( SHR1 ); LoadTexture( SHG1 ); LoadTexture( SHB1 ); LoadTexture( META0 );
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
            pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );

            pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, NULL, 0 );

            DECLARE_STATIC_VERTEX_SHADER( defconstruct_vs30 );
            SET_STATIC_VERTEX_SHADER_COMBO( USEWORLDTRANSFORM, 0 );
            SET_STATIC_VERTEX_SHADER_COMBO( SENDWORLDPOS, 0 );
            SET_STATIC_VERTEX_SHADER( defconstruct_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( radiosity_blend_ps30 );
            SET_STATIC_PIXEL_SHADER( radiosity_blend_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();

            DECLARE_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 );

            ConVarRef bounceCount( "deferred_rh_bounce_count" );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( SECOND_BOUNCE, bounceCount.GetInt() > 0 ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_Depth() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_Normals() );
            BindTexture( SHADER_SAMPLER2, SHR0 ); BindTexture( SHADER_SAMPLER3, SHG0 );
            BindTexture( SHADER_SAMPLER4, SHB0 ); BindTexture( SHADER_SAMPLER5, VIS0 );
            BindTexture( SHADER_SAMPLER6, SHR1 ); BindTexture( SHADER_SAMPLER7, SHG1 );
            BindTexture( SHADER_SAMPLER8, SHB1 ); BindTexture( SHADER_SAMPLER9, META0 );
            BindTexture( SHADER_SAMPLER10, GEOMETRY );

            CommitBaseDeferredConstants_Frustum( pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
            CommitBaseDeferredConstants_Origin( pShaderAPI, 0 );

            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef receiverOffset( "deferred_rh_receiver_offset" );
            ConVarRef intensity( "deferred_rh_intensity" );
            ConVarRef legacyMultiplier( "deferred_radiosity_multiplier" );
            ConVarRef saturation( "deferred_rh_saturation" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
            ConVarRef visibilityStrength( "deferred_rh_visibility_strength" );
            ConVarRef visibilityDecay( "deferred_rh_visibility_decay" );
            ConVarRef shadowStrength( "deferred_rh_soft_shadow_strength" );
            ConVarRef shadowDistance( "deferred_rh_soft_shadow_distance" );
            ConVarRef shadowSoftness( "deferred_rh_soft_shadow_softness" );
            ConVarRef shadowMinVisibility( "deferred_rh_soft_shadow_min_visibility" );
            ConVarRef geometryBias( "deferred_rh_geometry_bias" );
            ConVarRef receiverRadius( "deferred_rh_receiver_radius" );
            ConVarRef confidenceFloor( "deferred_rh_reconstruction_confidence_floor" );
            ConVarRef isotropicShadow( "deferred_rh_shadow_isotropic_blend" );

            const Vector &origin = GetDeferredExt()->GetRadiosityData().vecOrigin[0];
            float c1[4] = { origin.x, origin.y, origin.z, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 1, c1 );

            const float cell = MAX( cellSize.GetFloat(), 1.0f );
            float c2[4] = {
                cell * RH_VOLUME_SIZE,
                clamp( receiverOffset.GetFloat(), 0.0f, cell * 0.45f ),
                MAX( intensity.GetFloat(), 0.0f ) * MAX( legacyMultiplier.GetFloat(), 0.0f ),
                1.0f
            };
            pShaderAPI->SetPixelShaderConstant( 2, c2 );

            const float softness = clamp( shadowSoftness.GetFloat(), 0.0f, 1.0f );
            float c3[4] = {
                clamp( saturation.GetFloat(), 0.0f, 2.0f ),
                MAX( maxRadiance.GetFloat(), 0.25f ),
                geometryEnable.GetBool() ? MAX( visibilityStrength.GetFloat(), 0.0f ) : 0.0f,
                MAX( visibilityDecay.GetFloat(), 0.0f ) * ( 1.20f - 0.55f * softness )
            };
            pShaderAPI->SetPixelShaderConstant( 3, c3 );

            float c4[4] = {
                geometryEnable.GetBool() ? clamp( shadowStrength.GetFloat(), 0.0f, 2.0f ) : 0.0f,
                clamp( shadowDistance.GetFloat(), 0.5f, 12.0f ),
                clamp( shadowMinVisibility.GetFloat(), 0.0f, 1.0f ),
                clamp( geometryBias.GetFloat(), 0.0f, 0.95f )
            };
            pShaderAPI->SetPixelShaderConstant( 4, c4 );

            float c5[4] = {
                clamp( receiverRadius.GetFloat(), 0.0f, 1.5f ),
                clamp( confidenceFloor.GetFloat(), 0.0f, 0.25f ),
                0.08f + softness * 0.22f,
                clamp( isotropicShadow.GetFloat(), 0.0f, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 5, c5 );
        }

        Draw();
    }
END_SHADER
