#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "deferred_particlesphere_vs30.inc"
#include "deferred_particlesphere_ps30.inc"

BEGIN_VS_SHADER( DEFERREDPARTICLESPHERE,
    "Forward translucent particles lit by daylight GI and composited CSM" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( DEPTHBLEND, SHADER_PARAM_TYPE_INTEGER, "1", "Fade particle intersections" )
        SHADER_PARAM( DEPTHBLENDSCALE, SHADER_PARAM_TYPE_FLOAT, "50.0", "Soft-particle depth scale" )
        SHADER_PARAM( USINGPIXELSHADER, SHADER_PARAM_TYPE_BOOL, "1", "Particle renderer contract" )
        SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "particle/SmokeStack", "Sphere normal and opacity" )
        SHADER_PARAM( PARTICLEALBEDO, SHADER_PARAM_TYPE_VEC3, "[0.68 0.68 0.68]", "Scattering albedo" )
        SHADER_PARAM( LIGHT_POSITION, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "Legacy proxy light position" )
        SHADER_PARAM( LIGHT_COLOR, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "Legacy proxy light color" )
        SHADER_PARAM( LIGHT_POSITION_WORLD, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "World-space compatibility light" )
        SHADER_PARAM( LIGHT_COLOR_WORLD, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "World-space compatibility light color" )
        SHADER_PARAM( CAMERA_RIGHT, SHADER_PARAM_TYPE_VEC3, "[1 0 0]", "World-space camera-right vector" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[ DEPTHBLEND ]->IsDefined() ) params[ DEPTHBLEND ]->SetIntValue( 1 );
        if ( !params[ DEPTHBLENDSCALE ]->IsDefined() ) params[ DEPTHBLENDSCALE ]->SetFloatValue( 50.0f );
        if ( !params[ USINGPIXELSHADER ]->IsDefined() ) params[ USINGPIXELSHADER ]->SetIntValue( 1 );
        if ( !params[ PARTICLEALBEDO ]->IsDefined() ) params[ PARTICLEALBEDO ]->SetVecValue( 0.68f, 0.68f, 0.68f );
    }

    SHADER_INIT
    {
        if ( params[ BUMPMAP ]->IsDefined() ) LoadTexture( BUMPMAP );
    }

    SHADER_FALLBACK
    {
        if ( g_pHardwareConfig->GetDXSupportLevel() < 90 ) return "ParticleSphere";
        return 0;
    }

    SHADER_DRAW
    {
        const bool bDepthBlend = params[ DEPTHBLEND ]->GetIntValue() != 0;

        SHADOW_STATE
        {
            pShaderShadow->SetDefaultState();
            pShaderShadow->EnableDepthWrites( false );
            pShaderShadow->EnableAlphaWrites( false );
            pShaderShadow->EnableBlending( true );
            pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

            for ( int sampler = 0; sampler <= 14; ++sampler )
                pShaderShadow->EnableTexture( (Sampler_t)sampler, true );

            int texCoordDimensions[] = { 2, 4 };
            pShaderShadow->VertexShaderVertexFormat(
                VERTEX_POSITION | VERTEX_COLOR, 2, texCoordDimensions, 0 );

            DECLARE_STATIC_VERTEX_SHADER( deferred_particlesphere_vs30 );
            SET_STATIC_VERTEX_SHADER( deferred_particlesphere_vs30 );

            DECLARE_STATIC_PIXEL_SHADER( deferred_particlesphere_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( DEPTHBLEND, bDepthBlend ? 1 : 0 );
            SET_STATIC_PIXEL_SHADER( deferred_particlesphere_ps30 );
            FogToFogColor();
        }

        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            BindTexture( SHADER_SAMPLER0, BUMPMAP );
            if ( bDepthBlend )
                pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_FRAME_BUFFER_FULL_DEPTH );

            ConVarRef particleLighting( "deferred_particle_lighting" );
            ConVarRef particleGIIntensity( "deferred_particle_gi_intensity" );
            ConVarRef particleBounceIntensity( "deferred_particle_bounce_intensity" );
            ConVarRef particleSunIntensity( "deferred_particle_sun_intensity" );
            ConVarRef particleNormalStrength( "deferred_particle_normal_strength" );
            ConVarRef particleLegacyIntensity( "deferred_particle_legacy_intensity" );
            ConVarRef particleShadowSoftness( "deferred_particle_shadow_softness" );
            ConVarRef particleDebug( "deferred_particle_lighting_debug" );
            ConVarRef giQuality( "deferred_gi_quality" );
            ConVarRef giIntensity( "deferred_gi_intensity" );
            ConVarRef giBounceIntensity( "deferred_gi_bounce_intensity" );

            const bool bLighting = !particleLighting.IsValid() || particleLighting.GetBool();
            const daylightGIData_t &giData = GetDeferredExt()->GetDaylightGIData();
            const unsigned int validClipMask = ( 1u << DAYLIGHT_GI_CLIP_COUNT ) - 1u;
            const bool bGI = bLighting && GetDeferredExt()->IsRadiosityEnabled() &&
                ( giData.nValidClipMask & validClipMask ) == validClipMask;
            const float bounceScale = clamp(
                ( giBounceIntensity.IsValid() ? giBounceIntensity.GetFloat() : 0.0f ) *
                ( particleBounceIntensity.IsValid() ? particleBounceIntensity.GetFloat() : 1.0f ),
                0.0f, 1.0f );
            const bool bSecondBounce = bGI && bounceScale > 0.0f &&
                ( !giQuality.IsValid() || giQuality.GetInt() > 0 );

            const lightData_Global_t &globalLight = GetDeferredExt()->GetLightData_Global();
            const bool bShadow = bLighting && globalLight.bEnabled && globalLight.bShadow;

            DECLARE_DYNAMIC_VERTEX_SHADER( deferred_particlesphere_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( deferred_particlesphere_vs30 );

            DECLARE_DYNAMIC_PIXEL_SHADER( deferred_particlesphere_ps30 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( GI_ENABLED, bGI ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( SECOND_BOUNCE, bSecondBounce ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER_COMBO( HAS_SHADOW, bShadow ? 1 : 0 );
            SET_DYNAMIC_PIXEL_SHADER( deferred_particlesphere_ps30 );

            if ( bGI )
            {
                BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 0, 0 ) );
                BindTexture( SHADER_SAMPLER3, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 0, 1 ) );
                BindTexture( SHADER_SAMPLER4, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 0, 2 ) );

                if ( bSecondBounce )
                {
                    BindTexture( SHADER_SAMPLER5, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 2, 0 ) );
                    BindTexture( SHADER_SAMPLER6, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 2, 1 ) );
                    BindTexture( SHADER_SAMPLER7, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 2, 2 ) );
                    BindTexture( SHADER_SAMPLER8, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 0 ) );
                    BindTexture( SHADER_SAMPLER9, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 1 ) );
                    BindTexture( SHADER_SAMPLER10, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 2 ) );
                    BindTexture( SHADER_SAMPLER11, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 2, 0 ) );
                    BindTexture( SHADER_SAMPLER12, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 2, 1 ) );
                    BindTexture( SHADER_SAMPLER13, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 2, 2 ) );
                }
                else
                {
                    BindTexture( SHADER_SAMPLER5, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_NEAR, 0, 3 ) );
                    BindTexture( SHADER_SAMPLER6, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 0 ) );
                    BindTexture( SHADER_SAMPLER7, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 1 ) );
                    BindTexture( SHADER_SAMPLER8, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 2 ) );
                    BindTexture( SHADER_SAMPLER9, GetDeferredExt()->GetTexture_DaylightGIRadiance( DAYLIGHT_GI_CLIP_FAR, 0, 3 ) );
                }
            }

            if ( bShadow )
            {
                BindTexture( bSecondBounce ? SHADER_SAMPLER14 : SHADER_SAMPLER10,
                    GetDeferredExt()->GetTexture_ShadowDepth_Ortho( 0 ) );
                COMPILE_TIME_ASSERT( CSM_USE_COMPOSITED_TARGET == 1 );
                COMPILE_TIME_ASSERT( SHADOW_NUM_CASCADES == 2 );
                CommitShadowProjectionConstants_Ortho_Composite( pShaderAPI, 2, 2 );
            }

            pShaderAPI->SetDepthFeatheringPixelShaderConstant(
                0, params[ DEPTHBLENDSCALE ]->GetFloatValue() );

            int viewportX, viewportY, viewportWidth, viewportHeight;
            pShaderAPI->GetCurrentViewport( viewportX, viewportY, viewportWidth, viewportHeight );
            int targetWidth, targetHeight;
            pShaderAPI->GetCurrentRenderTargetDimensions( targetWidth, targetHeight );
            float viewportMad[4] = {
                (float)viewportWidth / MAX( targetWidth, 1 ),
                (float)viewportHeight / MAX( targetHeight, 1 ),
                (float)viewportX / MAX( targetWidth, 1 ),
                (float)viewportY / MAX( targetHeight, 1 )
            };
            pShaderAPI->SetVertexShaderConstant(
                VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, viewportMad, 1 );

            float cameraPosition[4];
            pShaderAPI->GetWorldSpaceCameraPosition( cameraPosition );
            cameraPosition[3] = 0.0f;
            pShaderAPI->SetPixelShaderConstant( 1, cameraPosition );

            CommitGlobalLightForward( pShaderAPI, 16 );
            pShaderAPI->SetPixelShaderConstant( 17, globalLight.diff.Base() );
            pShaderAPI->SetPixelShaderConstant( 18, globalLight.ambh.Base() );
            Vector4D ambientLowHalf = MakeHalfAmbient( globalLight.ambl, globalLight.ambh );
            pShaderAPI->SetPixelShaderConstant( 19, ambientLowHalf.Base() );

            const daylightGIClipDesc_t &nearClip = giData.clips[ DAYLIGHT_GI_CLIP_NEAR ];
            const daylightGIClipDesc_t &farClip = giData.clips[ DAYLIGHT_GI_CLIP_FAR ];
            float nearData[4] = { nearClip.vecRadianceOrigin.x, nearClip.vecRadianceOrigin.y,
                nearClip.vecRadianceOrigin.z, 1.0f / MAX( nearClip.flExtent, 1.0f ) };
            float farData[4] = { farClip.vecRadianceOrigin.x, farClip.vecRadianceOrigin.y,
                farClip.vecRadianceOrigin.z, 1.0f / MAX( farClip.flExtent, 1.0f ) };
            pShaderAPI->SetPixelShaderConstant( 20, nearData );
            pShaderAPI->SetPixelShaderConstant( 21, farData );

            const float finalGIIntensity = clamp(
                ( giIntensity.IsValid() ? giIntensity.GetFloat() : 1.0f ) *
                ( particleGIIntensity.IsValid() ? particleGIIntensity.GetFloat() : 1.0f ),
                0.0f, 2.0f );
            float lightingSettings[4] = {
                finalGIIntensity,
                bounceScale,
                clamp( particleNormalStrength.IsValid() ? particleNormalStrength.GetFloat() : 0.65f, 0.0f, 1.0f ),
                bLighting && globalLight.bEnabled
                    ? clamp( particleSunIntensity.IsValid() ? particleSunIntensity.GetFloat() : 1.0f, 0.0f, 2.0f )
                    : 0.0f
            };
            pShaderAPI->SetPixelShaderConstant( 22, lightingSettings );

            float clipSettings[4] = { RH_CLIP_BLEND_CELLS, RH_CLIP_FAR_FADE_CELLS,
                RH_VOLUME_SIZE_F,
                clamp( particleLegacyIntensity.IsValid() ? particleLegacyIntensity.GetFloat() : 1.0f, 0.0f, 2.0f ) };
            pShaderAPI->SetPixelShaderConstant( 23, clipSettings );

            const float *cameraRightValue = params[ CAMERA_RIGHT ]->GetVecValue();
            float cameraRight[4] = { cameraRightValue[0], cameraRightValue[1],
                cameraRightValue[2], bLighting ? 1.0f : 0.0f };
            pShaderAPI->SetPixelShaderConstant( 24, cameraRight );

            const float *particleAlbedoValue = params[ PARTICLEALBEDO ]->GetVecValue();
            float particleSettings[4] = { particleAlbedoValue[0], particleAlbedoValue[1],
                particleAlbedoValue[2],
                (float)clamp( particleDebug.IsValid() ? particleDebug.GetInt() : 0, 0, 4 ) };
            pShaderAPI->SetPixelShaderConstant( 25, particleSettings );
            float shadowSettings[4] = {
                clamp( particleShadowSoftness.IsValid() ? particleShadowSoftness.GetFloat() : 1.5f,
                    0.25f, 3.0f ), 0.0f, 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 26, shadowSettings );
            pShaderAPI->SetPixelShaderConstant( 27, params[ LIGHT_POSITION_WORLD ]->GetVecValue() );
            pShaderAPI->SetPixelShaderConstant( 28, params[ LIGHT_COLOR_WORLD ]->GetVecValue() );
            pShaderAPI->SetPixelShaderFogParams( 40 );
        }

        Draw();
    }
END_SHADER
