#include "deferred_includes.h"

#include "screenspace_vs30.inc"
#include "radiosity_upsample_ps30.inc"

BEGIN_VS_SHADER( RADIOSITY_UPSAMPLE, "RH 4.2 depth/normal bilateral upsample" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Half-resolution RH indirect buffer" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS() {}
	SHADER_INIT { LoadTexture( BASETEXTURE ); }
	SHADER_FALLBACK { return 0; }

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->SetDefaultState();
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, NULL, 0 );

			DECLARE_STATIC_VERTEX_SHADER( screenspace_vs30 );
			SET_STATIC_VERTEX_SHADER( screenspace_vs30 );
			DECLARE_STATIC_PIXEL_SHADER( radiosity_upsample_ps30 );
			SET_STATIC_PIXEL_SHADER( radiosity_upsample_ps30 );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();
			DECLARE_DYNAMIC_VERTEX_SHADER( screenspace_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( screenspace_vs30 );
			DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_upsample_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( radiosity_upsample_ps30 );

			BindTexture( SHADER_SAMPLER0, BASETEXTURE );
			BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_Depth() );
			BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_Normals() );

			ConVarRef depthScale( "deferred_rh_upsample_depth_scale" );
			ConVarRef normalPower( "deferred_rh_upsample_normal_power" );
			ITexture *pHalf = params[ BASETEXTURE ]->GetTextureValue();
			const float halfW = (float)MAX( pHalf ? pHalf->GetActualWidth() : 1, 1 );
			const float halfH = (float)MAX( pHalf ? pHalf->GetActualHeight() : 1, 1 );
			float c0[4] = { halfW, halfH, 1.0f / halfW, 1.0f / halfH };
			pShaderAPI->SetPixelShaderConstant( 0, c0 );

            float c1[4] = {
                MAX( depthScale.GetFloat(), 0.0f ),
                MAX( normalPower.GetFloat(), 1.0f ),
                1.0f,
                0.0f
            };
			pShaderAPI->SetPixelShaderConstant( 1, c1 );
		}
		Draw();
	}
END_SHADER
