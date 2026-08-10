#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_propagate_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_PROPAGATE, "Packed-SDF physical secondary diffuse bounce" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( BOUNCEMODE, SHADER_PARAM_TYPE_INTEGER, "0", "0=surface; 1..3=physical XYZ transport" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS() {}

    SHADER_INIT {}

    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nBounceMode = clamp( params[ BOUNCEMODE ]->GetIntValue(), 0, 3 );
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

            const daylightGIData_t &giData = GetDeferredExt()->GetDaylightGIData();
            const int activeClip = clamp( giData.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 );
            const int sourceSet = nBounceMode >= 1 && nBounceMode <= 3 ? 1 : 0;
            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, sourceSet, 0 ) );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, sourceSet, 1 ) );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, sourceSet, 2 ) );
            BindTexture( SHADER_SAMPLER3, GetDeferredExt()->GetTexture_DaylightGIRadiance( activeClip, 0, 3 ) );
            BindTexture( SHADER_SAMPLER4, GetDeferredExt()->GetTexture_DaylightGIGeometry( activeClip ) );
            BindTexture( SHADER_SAMPLER5, GetDeferredExt()->GetTexture_DaylightGISurfaceAlbedo( activeClip ) );
            BindTexture( SHADER_SAMPLER6, GetDeferredExt()->GetTexture_DaylightGISurfaceNormal( activeClip ) );
            BindTexture( SHADER_SAMPLER7, GetDeferredExt()->GetTexture_DaylightGIBlockerField( activeClip ) );
            BindTexture( SHADER_SAMPLER8, GetDeferredExt()->GetTexture_DaylightGISurfaceGuide( activeClip ) );
            BindTexture( SHADER_SAMPLER9, GetDeferredExt()->GetTexture_DaylightGISurfaceCache( activeClip ) );

            ConVarRef bounceIntensity( "deferred_gi_bounce_intensity" );

            const float radiusCells = 2.25f;
            float settings[4] = {
                radiusCells / RH_VOLUME_SIZE_F,
                clamp( bounceIntensity.GetFloat(), 0.0f, 1.0f ),
                20.0f,
                radiusCells
            };
            pShaderAPI->SetPixelShaderConstant( 0, settings );

            float geometry[4] = { 1.0f, 1.20f, 0.08f, 0.05f };
            pShaderAPI->SetPixelShaderConstant( 1, geometry );

            const daylightGIClipDesc_t &clip = giData.clips[ clamp( giData.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 ) ];
            const Vector &origin = clip.vecRadianceOrigin;
            float worldGrid[4] = {
                origin.x, origin.y, origin.z,
                MAX( clip.flRadianceCellSize, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 2, worldGrid );

            float trace[4] = {
                0.35f,
                0.015f,
                (float)nBounceMode,
                1.0f
            };
            pShaderAPI->SetPixelShaderConstant( 3, trace );

            float surface[4] = {
                0.04f,
                0.0f, 0.0f, 0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 4, surface );

            float surfaceGuide[4] = { 1.0f, 0.70f, 0.52f, 1.0f };
            pShaderAPI->SetPixelShaderConstant( 6, surfaceGuide );

            const Vector &shadowOrigin = clip.vecBlockerOrigin;
            const float extent = clip.flExtent;
            const float invExtent = 1.0f / MAX( extent, 1.0f );
            float shadowOffset[4] = {
                ( origin.x - shadowOrigin.x ) * invExtent,
                ( origin.y - shadowOrigin.y ) * invExtent,
                ( origin.z - shadowOrigin.z ) * invExtent,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 8, shadowOffset );
            float cacheSettings[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 9, cacheSettings );
        }

        Draw();
    }
END_SHADER
