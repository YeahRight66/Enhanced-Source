#include "deferred_includes.h"
#include "radiance_hints_config.h"

#include "radiosity_hierarchy_ps30.inc"
#include "radiosity_gen_vs30.inc"

BEGIN_VS_SHADER( RADIOSITY_HIERARCHY, "RH11 energy-preserving radiance hierarchy" )
    BEGIN_SHADER_PARAMS
        SHADER_PARAM( SHR0, SHADER_PARAM_TYPE_TEXTURE, "", "Primary source red SH" )
        SHADER_PARAM( SHG0, SHADER_PARAM_TYPE_TEXTURE, "", "Primary source green SH" )
        SHADER_PARAM( SHB0, SHADER_PARAM_TYPE_TEXTURE, "", "Primary source blue SH" )
        SHADER_PARAM( SHR1, SHADER_PARAM_TYPE_TEXTURE, "", "Optional second-bounce red SH" )
        SHADER_PARAM( SHG1, SHADER_PARAM_TYPE_TEXTURE, "", "Optional second-bounce green SH" )
        SHADER_PARAM( SHB1, SHADER_PARAM_TYPE_TEXTURE, "", "Optional second-bounce blue SH" )
        SHADER_PARAM( SOURCELEVEL, SHADER_PARAM_TYPE_INTEGER, "0", "0=40, 1=20, 2=10" )
        SHADER_PARAM( COMBINED, SHADER_PARAM_TYPE_INTEGER, "0", "Include second bounce at source level 0" )
    END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
        if ( !params[SOURCELEVEL]->IsDefined() ) params[SOURCELEVEL]->SetIntValue( 0 );
        if ( !params[COMBINED]->IsDefined() ) params[COMBINED]->SetIntValue( 0 );
    }
    SHADER_INIT
    {
        LoadTexture( SHR0 ); LoadTexture( SHG0 ); LoadTexture( SHB0 );
        LoadTexture( SHR1 ); LoadTexture( SHG1 ); LoadTexture( SHB1 );
    }
    SHADER_FALLBACK { return 0; }

    SHADER_DRAW
    {
        const int sourceLevel = clamp( params[SOURCELEVEL]->GetIntValue(), 0, 2 );
        const int combined = params[COMBINED]->GetIntValue() != 0 ? 1 : 0;
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
            pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
            int texCoordDimensions[] = { 2 };
            pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION | VERTEX_TANGENT_S, 1, texCoordDimensions, 0 );
            DECLARE_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_STATIC_VERTEX_SHADER( radiosity_gen_vs30 );
            DECLARE_STATIC_PIXEL_SHADER( radiosity_hierarchy_ps30 );
            SET_STATIC_PIXEL_SHADER_COMBO( SOURCE_LEVEL, sourceLevel );
            SET_STATIC_PIXEL_SHADER_COMBO( COMBINED, combined );
            SET_STATIC_PIXEL_SHADER( radiosity_hierarchy_ps30 );
        }
        DYNAMIC_STATE
        {
            pShaderAPI->SetDefaultState();
            DECLARE_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            SET_DYNAMIC_VERTEX_SHADER( radiosity_gen_vs30 );
            DECLARE_DYNAMIC_PIXEL_SHADER( radiosity_hierarchy_ps30 );
            SET_DYNAMIC_PIXEL_SHADER( radiosity_hierarchy_ps30 );
            BindTexture( SHADER_SAMPLER0, SHR0 ); BindTexture( SHADER_SAMPLER1, SHG0 ); BindTexture( SHADER_SAMPLER2, SHB0 );
            BindTexture( SHADER_SAMPLER3, SHR1 ); BindTexture( SHADER_SAMPLER4, SHG1 ); BindTexture( SHADER_SAMPLER5, SHB1 );
            ConVarRef maxRadiance( "deferred_rh_max_radiance" );
            float c0[4] = { 1.0f, MAX( maxRadiance.GetFloat(), 0.25f ), 0.0f, 0.0f };
            pShaderAPI->SetPixelShaderConstant( 0, c0 );
        }
        Draw();
    }
END_SHADER
