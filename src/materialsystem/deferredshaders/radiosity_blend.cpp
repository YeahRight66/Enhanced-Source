#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "defconstruct_vs30.inc"
#include "radiosity_blend_ps30.inc"

BEGIN_VS_SHADER( RADIOSITY_BLEND, "Two-level visibility-aware tetrahedral daylight GI receiver" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( CLIPLEVEL, SHADER_PARAM_TYPE_INTEGER, "0", "0=near overwrite/depth, 1=far additive RGB" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[ CLIPLEVEL ]->IsDefined() ) params[ CLIPLEVEL ]->SetIntValue( 0 );
    }
    SHADER_INIT {}
    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nClipLevel = clamp( params[ CLIPLEVEL ]->GetIntValue(), 0, 1 );
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false ); pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( nClipLevel == 0 );
            if ( nClipLevel == 1 )
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
            pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, NULL, 0 );
            DECLARE_STATIC_VERTEX_SHADER( defconstruct_vs30 );
            SET_STATIC_VERTEX_SHADER_COMBO( USEWORLDTRANSFORM, 0 );
            SET_STATIC_VERTEX_SHADER_COMBO( SENDWORLDPOS, 0 );
            SET_STATIC_VERTEX_SHADER( defconstruct_vs30 );
            DECLARE_STATIC_PIXEL_SHADER( radiosity_blend_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( PASS_MODE, 0 );
            SET_STATIC_PIXEL_SHADER( radiosity_blend_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            DECLARE_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 ); SET_DYNAMIC_VERTEX_SHADER( defconstruct_vs30 );
            ConVarRef bounceIntensity( "deferred_gi_bounce_intensity" );
            ConVarRef giQuality( "deferred_gi_quality" );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( SECOND_BOUNCE,
                bounceIntensity.GetFloat() > 0.0f && giQuality.GetInt() > 0 ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_blend_ps30 );

            const daylightGIData_t &giData = GetDeferredExt()->GetDaylightGIData();
            const int activeClip = clamp( giData.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 );
            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_Depth() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_Normals() );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 0, 0 ) );
            BindTexture( SHADER_SAMPLER3, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 0, 1 ) );
            BindTexture( SHADER_SAMPLER4, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 0, 2 ) );
            BindTexture( SHADER_SAMPLER5, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 2, 0 ) );
            BindTexture( SHADER_SAMPLER6, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 2, 1 ) );
            BindTexture( SHADER_SAMPLER7, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 2, 2 ) );
            BindTexture( SHADER_SAMPLER8, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 0, 3 ) );
            BindTexture( SHADER_SAMPLER9, GetDeferredExt()->GetTexture_DaylightGIBlockerField( activeClip ) );
            BindTexture( SHADER_SAMPLER10, GetDeferredExt()->GetTexture_DaylightGISurfaceCache( activeClip ) );

            CommitBaseDeferredConstants_Frustum( pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
            CommitBaseDeferredConstants_Origin( pShaderAPI, 0 );

            ConVarRef intensity( "deferred_gi_intensity" );
            ConVarRef debugMode( "deferred_gi_debug" );
            ConVarRef legacyDebugMode( "deferred_radiosity_debug" );

            const daylightGIClipDesc_t &clip = giData.clips[ clamp( giData.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 ) ];
            const Vector &origin = clip.vecRadianceOrigin;
            float c1[4] = { origin.x, origin.y, origin.z, 0.0f }; pShaderAPI->SetPixelShaderConstant( 1, c1 );
            float c2[4] = { clip.flExtent, 5.0f,
                clamp( intensity.GetFloat(), 0.0f, 2.0f ), clamp( bounceIntensity.GetFloat(), 0.0f, 1.0f ) };
            pShaderAPI->SetPixelShaderConstant( 2, c2 );
            float c3[4] = { 1.0f, 20.0f, 0.55f, 0.015f };
            pShaderAPI->SetPixelShaderConstant( 3, c3 );
            const float shadowCellRatio = RH_SHADOW_VOLUME_SIZE_F / RH_VOLUME_SIZE_F;
            float c4[4] = { 1.0f, 0.72f, 4.5f * shadowCellRatio, 0.07f };
            pShaderAPI->SetPixelShaderConstant( 4, c4 );
            float c7[4] = { 1.0f, 0.05f, 3.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 7, c7 );

            const Vector &shadowOrigin = clip.vecBlockerOrigin;
            const float invExtent = 1.0f / MAX( clip.flExtent, 1.0f );
            float c9[4] = {
                ( origin.x - shadowOrigin.x ) * invExtent,
                ( origin.y - shadowOrigin.y ) * invExtent,
                ( origin.z - shadowOrigin.z ) * invExtent,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 9, c9 );
            const int receiverDebugMode = debugMode.GetInt() > 0
                ? clamp( debugMode.GetInt(), 0, 10 )
                : ( legacyDebugMode.GetBool() ? 1 : 0 );
            float c10[4] = { 0.85f,
                0.55f,
                (float)receiverDebugMode,
                0.32f };
            pShaderAPI->SetPixelShaderConstant( 10, c10 );
            const daylightGIClipDesc_t &nearClip = giData.clips[DAYLIGHT_GI_CLIP_NEAR];
            float c11[4] = { nearClip.vecRadianceOrigin.x, nearClip.vecRadianceOrigin.y,
                nearClip.vecRadianceOrigin.z, nearClip.flExtent };
            pShaderAPI->SetPixelShaderConstant( 11, c11 );
            float c12[4] = { (float)activeClip, RH_CLIP_BLEND_CELLS,
                RH_CLIP_FAR_FADE_CELLS, RH_VOLUME_SIZE_F };
            pShaderAPI->SetPixelShaderConstant( 12, c12 );
        }
        Draw();
    }
END_SHADER
