#include "deferred_includes.h"

#include "radiosity_propagate_ps30.inc"
#include "radiosity_propagate_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_PROPAGATE, "Radiance Hints stochastic secondary bounce" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SHR, SHADER_PARAM_TYPE_TEXTURE, "", "Red SH volume" )
		SHADER_PARAM( SHG, SHADER_PARAM_TYPE_TEXTURE, "", "Green SH volume" )
		SHADER_PARAM( SHB, SHADER_PARAM_TYPE_TEXTURE, "", "Blue SH volume" )
		SHADER_PARAM( AUX, SHADER_PARAM_TYPE_TEXTURE, "", "Distance/validity volume" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_INIT
	{
		Assert( params[ SHR ]->IsDefined() );
		Assert( params[ SHG ]->IsDefined() );
		Assert( params[ SHB ]->IsDefined() );
		Assert( params[ AUX ]->IsDefined() );
		LoadTexture( SHR );
		LoadTexture( SHG );
		LoadTexture( SHB );
		LoadTexture( AUX );
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
			pShaderShadow->EnableAlphaWrites( true );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

			int texCoordDimensions[] = { 2 };
			pShaderShadow->VertexShaderVertexFormat(
				VERTEX_POSITION | VERTEX_TANGENT_S,
				1,
				texCoordDimensions,
				0 );

			DECLARE_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );
			SET_STATIC_VERTEX_SHADER( radiosity_propagate_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( radiosity_propagate_ps30 );
			SET_STATIC_PIXEL_SHADER( radiosity_propagate_ps30 );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( radiosity_propagate_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_propagate_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( radiosity_propagate_ps30 );

			BindTexture( SHADER_SAMPLER0, SHR );
			BindTexture( SHADER_SAMPLER1, SHG );
			BindTexture( SHADER_SAMPLER2, SHB );
			BindTexture( SHADER_SAMPLER3, AUX );

			ConVarRef cellSize( "deferred_rh_cell_size" );
			ConVarRef worldSpread( "deferred_rh_world_spread" );
			ConVarRef bounceGain( "deferred_rh_bounce_gain" );
			const float extent = MAX( cellSize.GetFloat(), 1.0f ) * RH_VOLUME_SIZE;
			float settings[4] = {
				clamp( worldSpread.GetFloat() / extent, 1.5f / RH_VOLUME_SIZE, 0.45f ),
				MAX( bounceGain.GetFloat(), 0.0f ),
				0.05f,
				0.0f
			};
			pShaderAPI->SetPixelShaderConstant( 0, settings );
		}

		Draw();
	}
END_SHADER
