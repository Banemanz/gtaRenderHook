//
// Created by peter on 16.02.2021.
//
#include "im_state.h"
#include <common_headers.h>
#include <rw_engine/rh_backend/raster_backend.h>
#include <rw_engine/rw_rh_convert_funcs.h>
#include <Engine/Common/types/blend_op.h>

namespace rh::rw::engine
{

namespace
{
uintptr_t RHBlendOpToRwBlendFunction( uint8_t blend_op )
{
    switch ( static_cast<rh::engine::BlendOp>( blend_op ) )
    {
    case rh::engine::BlendOp::Zero: return rwBLENDZERO;
    case rh::engine::BlendOp::One: return rwBLENDONE;
    case rh::engine::BlendOp::SrcColor: return rwBLENDSRCCOLOR;
    case rh::engine::BlendOp::InvSrcColor: return rwBLENDINVSRCCOLOR;
    case rh::engine::BlendOp::SrcAlpha: return rwBLENDSRCALPHA;
    case rh::engine::BlendOp::InvSrcAlpha: return rwBLENDINVSRCALPHA;
    case rh::engine::BlendOp::DestAlpha: return rwBLENDDESTALPHA;
    case rh::engine::BlendOp::InvDestAlpha: return rwBLENDINVDESTALPHA;
    case rh::engine::BlendOp::DestColor: return rwBLENDDESTCOLOR;
    case rh::engine::BlendOp::InvDestColor: return rwBLENDINVDESTCOLOR;
    case rh::engine::BlendOp::SrcAlphaSat: return rwBLENDSRCALPHASAT;
    default: break;
    }
    return rwBLENDNABLEND;
}
} // namespace

ImmediateState::ImmediateState()
{
    Raster        = BackendRasterPlugin::NullRasterId;
    ColorBlendSrc = static_cast<uint8_t>( rh::engine::BlendOp::SrcAlpha );
    ColorBlendDst = static_cast<uint8_t>( rh::engine::BlendOp::InvSrcAlpha );
    ColorBlendOp  = 0;
    BlendEnable   = false;
    ZTestEnable   = true;
    ZWriteEnable  = true;
    StencilEnable = false;
}

void ImmediateState::Update( int32_t nState, void *pParam )
{
    switch ( nState )
    {
    case rwRENDERSTATENARENDERSTATE: break;
    case rwRENDERSTATETEXTURERASTER:
    {
        if ( pParam == nullptr )
        {
            Raster = BackendRasterPlugin::NullRasterId;
            return;
        }
        auto &backend_ext =
            BackendRasterPlugin::GetData( static_cast<RwRaster *>( pParam ) );
        Raster = backend_ext.mImageId;
        // weird hack to fix GTA 3 blend in menus
        // TODO: check for image alpha before doing this
        BlendEnable = true;
        break;
    }
    case rwRENDERSTATETEXTUREADDRESS: break;
    case rwRENDERSTATETEXTUREADDRESSU: break;
    case rwRENDERSTATETEXTUREADDRESSV: break;
    case rwRENDERSTATETEXTUREPERSPECTIVE: break;
    case rwRENDERSTATEZTESTENABLE:
    {
        ZTestEnable = pParam != nullptr;
        break;
    }
    case rwRENDERSTATESHADEMODE: break;
    case rwRENDERSTATEZWRITEENABLE:
    {
        ZWriteEnable = pParam != nullptr;
        break;
    }
    case rwRENDERSTATETEXTUREFILTER: break;
    case rwRENDERSTATESRCBLEND:
    {
        ColorBlendSrc = static_cast<uint8_t>(
            RwBlendFunctionToRHBlendOp( static_cast<RwBlendFunction>(
                reinterpret_cast<uintptr_t>( pParam ) ) ) );
        break;
    }
    case rwRENDERSTATEDESTBLEND:
    {
        ColorBlendDst = static_cast<uint8_t>(
            RwBlendFunctionToRHBlendOp( static_cast<RwBlendFunction>(
                reinterpret_cast<uintptr_t>( pParam ) ) ) );
        break;
    }
    case rwRENDERSTATEVERTEXALPHAENABLE:
    {
        BlendEnable = pParam != nullptr;
        break;
    }
    case rwRENDERSTATEBORDERCOLOR: break;
    case rwRENDERSTATEFOGENABLE: break;
    case rwRENDERSTATEFOGCOLOR: break;
    case rwRENDERSTATEFOGTYPE: break;
    case rwRENDERSTATEFOGDENSITY: break;
    case rwRENDERSTATECULLMODE: break;
    case rwRENDERSTATESTENCILENABLE: break;
    case rwRENDERSTATESTENCILFAIL: break;
    case rwRENDERSTATESTENCILZFAIL: break;
    case rwRENDERSTATESTENCILPASS: break;
    case rwRENDERSTATESTENCILFUNCTION: break;
    case rwRENDERSTATESTENCILFUNCTIONREF: break;
    case rwRENDERSTATESTENCILFUNCTIONMASK: break;
    case rwRENDERSTATESTENCILFUNCTIONWRITEMASK: break;
    case rwRENDERSTATEALPHATESTFUNCTION: break;
    case rwRENDERSTATEALPHATESTFUNCTIONREF: break;
    }
}

uintptr_t ImmediateState::Get( int32_t nState ) const
{
    switch ( nState )
    {
    case rwRENDERSTATETEXTURERASTER:
        return Raster == BackendRasterPlugin::NullRasterId ? 0 : Raster;
    case rwRENDERSTATEZTESTENABLE: return ZTestEnable;
    case rwRENDERSTATEZWRITEENABLE: return ZWriteEnable;
    case rwRENDERSTATESRCBLEND:
        return RHBlendOpToRwBlendFunction( ColorBlendSrc );
    case rwRENDERSTATEDESTBLEND:
        return RHBlendOpToRwBlendFunction( ColorBlendDst );
    case rwRENDERSTATEVERTEXALPHAENABLE: return BlendEnable;
    case rwRENDERSTATESTENCILENABLE: return StencilEnable;
    case rwRENDERSTATETEXTUREADDRESS:
    case rwRENDERSTATETEXTUREADDRESSU:
    case rwRENDERSTATETEXTUREADDRESSV:
        return rwTEXTUREADDRESSWRAP;
    case rwRENDERSTATETEXTUREFILTER: return rwFILTERLINEAR;
    case rwRENDERSTATECULLMODE: return rwCULLMODECULLBACK;
    case rwRENDERSTATEALPHATESTFUNCTIONREF: return 0;
    default: break;
    }
    return 0;
}

} // namespace rh::rw::engine
