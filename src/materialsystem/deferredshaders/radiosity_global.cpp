#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_gen_global_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_GLOBAL, "Radiance Hints RSM injection and geometry occupancy" )
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}
	SHADER_INIT
	{
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

			int texCoordDimensions[] = { 2 };
			pShaderShadow->VertexShaderVertexFormat(
				VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );

			DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
			SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
			SET_STATIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
		}
		DYNAMIC_STATE
		{
			const radiosityData_t &data = GetDeferredExt()->GetRadiosityData();
			const int shadowCascade = pShaderAPI->GetIntRenderingParameter(
				INT_RENDERPARM_DEFERRED_RADIOSITY_CASCADE );

			pShaderAPI->SetDefaultState();

			DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( radiosity_gen_global_ps30 );

			BindTexture( SHADER_SAMPLER0, GetDeferredExt()->GetTexture_ShadowRad_Ortho_Albedo() );
			BindTexture( SHADER_SAMPLER1, GetDeferredExt()->GetTexture_ShadowRad_Ortho_Normal() );
			BindTexture( SHADER_SAMPLER2, GetDeferredExt()->GetTexture_ShadowDepth_Ortho( 0 ) );

			const Vector &origin = data.vecOrigin[0];
			float originConstant[4] = { origin.x, origin.y, origin.z, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, originConstant );

			CommitShadowProjectionConstants_Ortho_Single( pShaderAPI, shadowCascade, 1 );

			ConVarRef cellSize( "deferred_rh_cell_size" );
			ConVarRef worldSpread( "deferred_rh_world_spread" );
			ConVarRef injectionGain( "deferred_rh_injection_gain" );
			ConVarRef edgeFade( "deferred_rh_rsm_edge_fade" );
			ConVarRef maxRadiance( "deferred_rh_max_radiance" );
			ConVarRef geometryEnable( "deferred_rh_geometry_enable" );
			ConVarRef geometryInner( "deferred_rh_geometry_inner" );
			ConVarRef geometryOuter( "deferred_rh_geometry_outer" );
			ConVarRef geometryOccupancy( "deferred_rh_geometry_occupancy" );

			const float cell = MAX( cellSize.GetFloat(), 1.0f );
			float settings[4] = {
				cell * RH_VOLUME_SIZE,
				cell,
				MAX( worldSpread.GetFloat(), cell ),
				MAX( injectionGain.GetFloat(), 0.0f )
			};
			pShaderAPI->SetPixelShaderConstant( 8, settings );

			float qualitySettings[4] = {
				clamp( edgeFade.GetFloat(), 0.001f, 0.25f ),
				MAX( maxRadiance.GetFloat(), 0.25f ),
				0.0f,
				0.0f
			};
			pShaderAPI->SetPixelShaderConstant( 9, qualitySettings );

			const float innerRadius = clamp( geometryInner.GetFloat(), 0.0f, 2.0f );
			float geometrySettings[4] = {
				geometryEnable.GetBool() ? 1.0f : 0.0f,
				innerRadius,
				clamp( geometryOuter.GetFloat(), innerRadius + 0.01f, 3.0f ),
				clamp( geometryOccupancy.GetFloat(), 0.0f, 2.0f )
			};
			pShaderAPI->SetPixelShaderConstant( 10, geometrySettings );
		}

		Draw();
	}
END_SHADER
