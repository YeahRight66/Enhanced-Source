#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_filter_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_FILTER, "Daylight-GI confidence and blocker-aware spatial reconstruction" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( FILTERPHASE, SHADER_PARAM_TYPE_INTEGER, "0", "0=X axis, 1=Y axis, 2=Z axis" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[FILTERPHASE]->IsDefined() )
            params[FILTERPHASE]->SetIntValue( 0 );
    }

    SHADER_INIT {}

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

            const daylightGIData_t &giData = GetDeferredExt()->GetDaylightGIData();
            const int clip = clamp( giData.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 );
            const int phase = clamp( params[FILTERPHASE]->GetIntValue(), 0, 2 );
            const int sourceSet = phase == 0 ? 0 : ( phase == 1 ? 1 : 2 );
            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_DaylightGIRadiance( clip, sourceSet, 0 ) );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_DaylightGIRadiance( clip, sourceSet, 1 ) );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_DaylightGIRadiance( clip, sourceSet, 2 ) );
            BindTexture( SHADER_SAMPLER3, GetDeferredExt()->GetTexture_DaylightGIRadiance( clip, sourceSet, 3 ) );
            BindTexture( SHADER_SAMPLER4, GetDeferredExt()->GetTexture_DaylightGIBlockerField( clip ) );

            float c0[4] = { 0.70f, 1.75f, 1.20f, 0.65f };
            pShaderAPI->SetPixelShaderConstant( 0, c0 );

            float c1[4] = { 1.20f, 0.05f, 0.08f, 1.0f };
            pShaderAPI->SetPixelShaderConstant( 1, c1 );

            float c2[4] = { 20.0f, 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 2, c2 );

            const daylightGIClipDesc_t &clipDesc = giData.clips[clip];
            const float inverseExtent = 1.0f / MAX( clipDesc.flExtent, 1.0f );
            float c3[4] = {
                ( clipDesc.vecRadianceOrigin.x - clipDesc.vecBlockerOrigin.x ) * inverseExtent,
                ( clipDesc.vecRadianceOrigin.y - clipDesc.vecBlockerOrigin.y ) * inverseExtent,
                ( clipDesc.vecRadianceOrigin.z - clipDesc.vecBlockerOrigin.z ) * inverseExtent,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 3, c3 );

        }
        Draw();
    }
END_SHADER
