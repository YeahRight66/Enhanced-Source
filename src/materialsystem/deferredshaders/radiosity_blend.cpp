#include "deferred_includes.h"

#include "defconstruct_vs30.inc"
#include "radiosity_blend_ps30.inc"

BEGIN_VS_SHADER( RADIOSITY_BLEND, "Radiance Hints deferred reconstruction" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SHR0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce red SH" )
		SHADER_PARAM( SHG0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce green SH" )
		SHADER_PARAM( SHB0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce blue SH" )
		SHADER_PARAM( AUX0, SHADER_PARAM_TYPE_TEXTURE, "", "First-bounce distance/validity" )
		SHADER_PARAM( SHR1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce red SH" )
		SHADER_PARAM( SHG1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce green SH" )
		SHADER_PARAM( SHB1, SHADER_PARAM_TYPE_TEXTURE, "", "Second-bounce blue SH" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_INIT
	{
		LoadTexture( SHR0 );
		LoadTexture( SHG0 );
		LoadTexture( SHB0 );
		LoadTexture( AUX0 );
		LoadTexture( SHR1 );
		LoadTexture( SHG1 );
		LoadTexture( SHB1 );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

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
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );

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
			BindTexture( SHADER_SAMPLER2, SHR0 );
			BindTexture( SHADER_SAMPLER3, SHG0 );
			BindTexture( SHADER_SAMPLER4, SHB0 );
			BindTexture( SHADER_SAMPLER5, AUX0 );
			BindTexture( SHADER_SAMPLER6, SHR1 );
			BindTexture( SHADER_SAMPLER7, SHG1 );
			BindTexture( SHADER_SAMPLER8, SHB1 );

			CommitBaseDeferredConstants_Frustum( pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
			CommitBaseDeferredConstants_Origin( pShaderAPI, 0 );

			const Vector &origin = GetDeferredExt()->GetRadiosityData().vecOrigin[0];
			float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 1, originConstant );

			ConVarRef cellSize( "deferred_rh_cell_size" );
			ConVarRef receiverOffset( "deferred_rh_receiver_offset" );
			ConVarRef intensity( "deferred_rh_intensity" );
			ConVarRef legacyMultiplier( "deferred_radiosity_multiplier" );
			float settings[4] = {
				MAX( cellSize.GetFloat(), 1.0f ) * RH_VOLUME_SIZE,
				MAX( receiverOffset.GetFloat(), 0.0f ),
				MAX( intensity.GetFloat(), 0.0f ) * MAX( legacyMultiplier.GetFloat(), 0.0f ),
				1.0f
			};
			pShaderAPI->SetPixelShaderConstant( 2, settings );
		}

		Draw();
	}
END_SHADER
