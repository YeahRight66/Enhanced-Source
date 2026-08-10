#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_gen_global_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_GLOBAL, "Two-level daylight-GI sun/sky injection and surface cache" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( RSMALBEDO, SHADER_PARAM_TYPE_TEXTURE, "", "Raw RSM material albedo" )
        SHADER_PARAM( SURFACEMODE, SHADER_PARAM_TYPE_INTEGER, "0", "0=sun radiance, 1=surface attributes, 2=hemisphere sky" )
        SHADER_PARAM( SAMPLEPHASE, SHADER_PARAM_TYPE_INTEGER, "0", "0..3=four-sample stratified sun phases; sky/surface use 0..1" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[ SURFACEMODE ]->IsDefined() )
            params[ SURFACEMODE ]->SetIntValue( 0 );
        if ( !params[ SAMPLEPHASE ]->IsDefined() )
            params[ SAMPLEPHASE ]->SetIntValue( 0 );
    }
    SHADER_INIT
    {
        LoadTexture( RSMALBEDO );
    }
    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int nSurfaceMode = clamp( params[ SURFACEMODE ]->GetIntValue(), 0, 2 );
        const int nSamplePhase = clamp( params[ SAMPLEPHASE ]->GetIntValue(), 0, 3 );
        const bool bAdditiveMode = nSurfaceMode != 0 || nSamplePhase != 0;
        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthTest( false );
            pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( true );
            if ( bAdditiveMode )
                EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

            pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
            pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );

            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( SURFACE_MODE, nSurfaceMode );
            SET_STATIC_PIXEL_SHADER_COMBO( SAMPLE_PHASE, nSamplePhase );
            SET_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
        }
        DYNAMIC_STATE
        {
            const daylightGIData_t &data = GetDeferredExt()->GetDaylightGIData();
            const daylightGIClipDesc_t &clip = data.clips[ clamp( data.iActiveClip, 0, DAYLIGHT_GI_CLIP_COUNT - 1 ) ];
            const lightData_Global_t &globalLight = GetDeferredExt()->GetLightData_Global();
            pShaderAPI->SetDefaultState();

            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );

            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );

            BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_RadianceHintsRSMFlux() );
            BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_RadianceHintsRSMNormal() );
            BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_RadianceHintsRSMDepth() );
            BindTexture( SHADER_SAMPLER3, RSMALBEDO );
            BindTexture( SHADER_SAMPLER4, GetDeferredExt()->GetTexture_DaylightGIBlockerField( data.iActiveClip ) );
            BindTexture( SHADER_SAMPLER5, GetDeferredExt()->GetTexture_DaylightGIOpenSky( data.iActiveClip ) );

            pShaderAPI->SetPixelShaderConstant( 0, data.matWorldToRSM.Base(), 4 );
            pShaderAPI->SetPixelShaderConstant( 4, data.matRSMToWorld.Base(), 4 );

            const Vector &origin = clip.vecRadianceOrigin;
            float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 8, originConstant );

            ConVarRef giQuality( "deferred_gi_quality" );
            const int qualityLevel = clamp( giQuality.GetInt(), 0, 2 );
            const float cell = MAX( clip.flRadianceCellSize, 1.0f );
            const float gatherWorldRadius = cell * ( qualityLevel == 0 ? 4.0f :
                ( qualityLevel == 1 ? 5.0f : 5.5f ) );
            float settings[4] = {
                clip.flExtent,
                cell,
                gatherWorldRadius,
                1.0f
            };
            pShaderAPI->SetPixelShaderConstant( 9, settings );

            float quality[4] = {
                0.02f,
                20.0f,
                data.vecRSMParams.y,
                MAX( data.vecRSMParams.z, 1.0f )
            };
            pShaderAPI->SetPixelShaderConstant( 10, quality );

            float upper[4] = {
                globalLight.ambh.x,
                globalLight.ambh.y,
                globalLight.ambh.z,
                1.0f
            };
            float lower[4] = {
                globalLight.ambl.x * 0.30f,
                globalLight.ambl.y * 0.30f,
                globalLight.ambl.z * 0.30f,
                0.55f
            };
            float skySurface[4] = {
                1.25f,
                8.0f,
                1.35f,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 11, upper );
            pShaderAPI->SetPixelShaderConstant( 12, lower );
            pShaderAPI->SetPixelShaderConstant( 13, skySurface );

            float adaptive[4] = { 1.0f,
                qualityLevel == 0 ? 3.0f : 3.5f,
                qualityLevel == 0 ? 4.0f : 5.5f,
                0.32f };
            pShaderAPI->SetPixelShaderConstant( 14, adaptive );
            float surfaceGuide[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 15, surfaceGuide );

            const Vector &shadowOrigin = clip.vecBlockerOrigin;
            const float invExtent = 1.0f / MAX( clip.flExtent, 1.0f );
            float shadowOffset[4] = {
                ( origin.x - shadowOrigin.x ) * invExtent,
                ( origin.y - shadowOrigin.y ) * invExtent,
                ( origin.z - shadowOrigin.z ) * invExtent,
                0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 16, shadowOffset );
            float injectionSettings[4] = { 0.55f, 1.0f, 1.0f, 0.85f };
            pShaderAPI->SetPixelShaderConstant( 17, injectionSettings );

            const float activeSampleCount = (float)MIN( RH_RADIANCE_SAMPLE_COUNT,
                qualityLevel == 0 ? 4 : ( qualityLevel == 1 ? 8 : 16 ) );
            float runtimeQuality[4] = {
                activeSampleCount,
                (float)qualityLevel, 0.0f, 0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 18, runtimeQuality );
        }

        Draw();
    }
END_SHADER
