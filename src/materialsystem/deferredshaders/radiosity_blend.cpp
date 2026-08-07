#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "defconstruct_vs30.inc"
#include "radiosity_blend_ps30.inc"

BEGIN_VS_SHADER( RADIOSITY_BLEND, "RH 9.1 receiver / high-resolution SDF shadow pass" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce red SH" )
        SHADER_PARAM( SHG0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce green SH" )
        SHADER_PARAM( SHB0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce blue SH" )
        SHADER_PARAM( SHR1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce red SH" )
        SHADER_PARAM( SHG1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce green SH" )
        SHADER_PARAM( SHB1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce blue SH" )
        SHADER_PARAM( META0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce metadata" )
        SHADER_PARAM( AUXTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Half-resolution shadow visibility or 64^3 shadow occupancy" )
        SHADER_PARAM( SHADOWDISTANCE, SHADER_PARAM_TYPE_TEXTURE, "", "64^3 Euclidean shadow distance field" )
        SHADER_PARAM( PASSMODE, SHADER_PARAM_TYPE_INTEGER, "0", "0=receiver, 1=half-res SDF visibility" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() { if ( !params[ PASSMODE ]->IsDefined() ) params[ PASSMODE ]->SetIntValue( 0 ); }
    SHADER_INIT
    {
        LoadTexture( SHR0 ); LoadTexture( SHG0 ); LoadTexture( SHB0 );
        LoadTexture( SHR1 ); LoadTexture( SHG1 ); LoadTexture( SHB1 );
        LoadTexture( META0 ); LoadTexture( AUXTEXTURE ); LoadTexture( SHADOWDISTANCE );
    }
    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nPassMode = clamp( params[ PASSMODE ]->GetIntValue(), 0, 1 );
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false ); pShaderShadow->EnableDepthWrites( false );
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
            SET_STATIC_PIXEL_SHADER_COMBO( PASS_MODE, nPassMode );
            SET_STATIC_PIXEL_SHADER( radiosity_blend_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            DECLARE_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 ); SET_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 );
            ConVarRef bounceCount( "deferred_rh_bounce_count" );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( SECOND_BOUNCE, bounceCount.GetInt() > 0 ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_Depth() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_Normals() );
            BindTexture( SHADER_SAMPLER2, SHR0 ); BindTexture( SHADER_SAMPLER3, SHG0 ); BindTexture( SHADER_SAMPLER4, SHB0 );
            BindTexture( SHADER_SAMPLER5, SHR1 ); BindTexture( SHADER_SAMPLER6, SHG1 ); BindTexture( SHADER_SAMPLER7, SHB1 );
            BindTexture( SHADER_SAMPLER8, META0 ); BindTexture( SHADER_SAMPLER9, AUXTEXTURE ); BindTexture( SHADER_SAMPLER10, SHADOWDISTANCE );

            CommitBaseDeferredConstants_Frustum( pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
            CommitBaseDeferredConstants_Origin( pShaderAPI, 0 );

            ConVarRef cellSize( "deferred_rh_cell_size" );
            ConVarRef receiverOffset( "deferred_rh_receiver_offset" );
            ConVarRef intensity( "deferred_rh_intensity" );
            ConVarRef legacyMultiplier( "deferred_radiosity_multiplier" );
            ConVarRef saturation( "deferred_rh_saturation" );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            ConVarRef receiverRadius( "deferred_rh_receiver_radius" );
            ConVarRef confidenceFloor( "deferred_rh_reconstruction_confidence_floor" );
            ConVarRef shadowStrength( "deferred_rh_soft_shadow_strength" );
            ConVarRef shadowSecond( "deferred_rh_shadow_second_strength" );
            ConVarRef shadowDistance( "deferred_rh_soft_shadow_distance" );
            ConVarRef shadowMinVisibility( "deferred_rh_soft_shadow_min_visibility" );
            ConVarRef sdfSteps( "deferred_rh_shadow_sdf_steps" );
            ConVarRef sdfSecondSteps( "deferred_rh_shadow_sdf_second_steps" );
            ConVarRef sdfStepScale( "deferred_rh_shadow_sdf_step_scale" );
            ConVarRef sdfMinStep( "deferred_rh_shadow_sdf_min_step" );
            ConVarRef sdfMaxStep( "deferred_rh_shadow_sdf_max_step" );
            ConVarRef sdfSoftness( "deferred_rh_shadow_sdf_softness" );
            ConVarRef shadowClearance( "deferred_rh_shadow_receiver_clearance" );
            ConVarRef shadowContact( "deferred_rh_shadow_contact_strength" );
            ConVarRef shadowFar( "deferred_rh_shadow_far_strength" );
            ConVarRef sdfEnable( "deferred_rh_shadow_sdf_enable" );
            ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
            ConVarRef geometryBias( "deferred_rh_geometry_bias" );
            ConVarRef legacySoftness( "deferred_rh_soft_shadow_softness" );
            ConVarRef legacyStart( "deferred_rh_shadow_start_cells" );
            ConVarRef shadowExtinction( "deferred_rh_shadow_extinction" );
            ConVarRef isotropic( "deferred_rh_shadow_isotropic_blend" );

            const Vector &origin = GetDeferredExt()->GetRadiosityData().vecOrigin[0];
            float c1[4] = { origin.x, origin.y, origin.z, 0.0f }; pShaderAPI->SetPixelShaderConstant( 1, c1 );
            const float cell = MAX( cellSize.GetFloat(), 1.0f );
            float c2[4] = { cell * RH_VOLUME_SIZE, clamp( receiverOffset.GetFloat(), 0.0f, cell * 0.45f ),
                MAX( intensity.GetFloat(), 0.0f ) * MAX( legacyMultiplier.GetFloat(), 0.0f ), 1.0f };
            pShaderAPI->SetPixelShaderConstant( 2, c2 );
            float c3[4] = { clamp( saturation.GetFloat(), 0.0f, 2.0f ), MAX( maxRadiance.GetFloat(), 0.25f ),
                clamp( receiverRadius.GetFloat(), 0.0f, 1.5f ), clamp( confidenceFloor.GetFloat(), 0.0f, 0.25f ) };
            pShaderAPI->SetPixelShaderConstant( 3, c3 );
            const float shadowCellRatio = RH_SHADOW_VOLUME_SIZE_F / RH_VOLUME_SIZE_F;
            float c4[4] = { clamp( shadowStrength.GetFloat(), 0.0f, 2.0f ), clamp( shadowSecond.GetFloat(), 0.0f, 2.0f ),
                clamp( shadowDistance.GetFloat() * shadowCellRatio, 0.5f, 20.0f ), clamp( shadowMinVisibility.GetFloat(), 0.0f, 1.0f ) };
            pShaderAPI->SetPixelShaderConstant( 4, c4 );
            float c5[4] = { clamp( sdfStepScale.GetFloat(), 0.1f, 1.5f ), clamp( sdfMinStep.GetFloat(), 0.15f, 2.0f ),
                clamp( sdfMaxStep.GetFloat(), 0.25f, 5.0f ), (float)clamp( sdfSteps.GetInt(), 3, RH_SHADOW_TRACE_STEPS ) };
            pShaderAPI->SetPixelShaderConstant( 5, c5 );
            float c6[4] = { clamp( sdfSoftness.GetFloat(), 0.1f, 4.0f ),
                clamp( shadowClearance.GetFloat() * shadowCellRatio, 0.25f, 3.5f ), MAX( shadowContact.GetFloat(), 0.0f ), MAX( shadowFar.GetFloat(), 0.0f ) };
            pShaderAPI->SetPixelShaderConstant( 6, c6 );
            float c7[4] = { ( sdfEnable.GetBool() && geometryEnable.GetBool() ) ? 1.0f : 0.0f,
                clamp( geometryBias.GetFloat(), 0.0f, 0.95f ), (float)clamp( sdfSecondSteps.GetInt(), 1, 3 ), 0.0f };
            pShaderAPI->SetPixelShaderConstant( 7, c7 );
            float c8[4] = { clamp( legacySoftness.GetFloat(), 0.05f, 2.0f ),
                clamp( legacyStart.GetFloat() * shadowCellRatio, 0.0f, 4.0f ),
                clamp( shadowExtinction.GetFloat(), 0.0f, 3.0f ),
                clamp( isotropic.GetFloat(), 0.0f, 1.0f ) };
            pShaderAPI->SetPixelShaderConstant( 8, c8 );
        }
        Draw();
    }
END_SHADER
