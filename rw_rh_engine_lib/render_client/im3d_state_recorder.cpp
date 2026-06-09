//
// Created by peter on 17.02.2021.
//

#include "im3d_state_recorder.h"
#include "data_desc/immediate_mode/im_state.h"
#include <ipc/MemoryReader.h>
#include <ipc/MemoryWriter.h>

namespace rh::rw::engine
{

constexpr auto Im3DVertexCountLimit = 100000;
constexpr auto Im3DIndexCountLimit  = 100000;

namespace
{
void EnsureVertexCapacity( std::vector<RwIm3DVertex> &buffer, uint32_t required )
{
    if ( buffer.size() < required )
        buffer.resize( required );
}

void EnsureIndexCapacity( std::vector<uint16_t> &buffer, uint32_t required )
{
    if ( buffer.size() < required )
        buffer.resize( required );
}

void EnsureDrawCallCapacity( std::vector<Im3DDrawCall> &buffer,
                             uint32_t                   required )
{
    if ( buffer.size() < required )
        buffer.resize( required );
}

uint32_t ConvertedIndexCount( RwPrimitiveType prim_type, uint32_t source_count )
{
    switch ( prim_type )
    {
    case rwPRIMTYPETRILIST: return source_count;
    case rwPRIMTYPETRISTRIP:
    case rwPRIMTYPETRIFAN:
        return source_count < 3 ? 0 : ( source_count - 2 ) * 3;
    case rwPRIMTYPELINELIST: return source_count;
    case rwPRIMTYPEPOLYLINE:
        return source_count < 2 ? 0 : ( source_count - 1 ) * 2;
    case rwPRIMTYPEPOINTLIST: return source_count;
    default: break;
    }
    return 0;
}

RwPrimitiveType NormalizePrimitiveType( RwPrimitiveType prim_type )
{
    switch ( prim_type )
    {
    case rwPRIMTYPETRISTRIP:
    case rwPRIMTYPETRIFAN: return rwPRIMTYPETRILIST;
    case rwPRIMTYPEPOLYLINE: return rwPRIMTYPELINELIST;
    default: return prim_type;
    }
}

template <typename IndexReader>
uint32_t WriteConvertedIndices( RwPrimitiveType prim_type, uint32_t source_count,
                                uint16_t *dst, IndexReader read_index )
{
    uint32_t out_count = 0;
    switch ( prim_type )
    {
    case rwPRIMTYPETRILIST:
    case rwPRIMTYPELINELIST:
    case rwPRIMTYPEPOINTLIST:
        for ( uint32_t i = 0; i < source_count; ++i )
            dst[out_count++] = read_index( i );
        break;
    case rwPRIMTYPETRISTRIP:
        for ( uint32_t i = 0; i + 2 < source_count; ++i )
        {
            const auto i0 = read_index( i );
            const auto i1 = read_index( i + 1 );
            const auto i2 = read_index( i + 2 );
            if ( i & 1 )
            {
                dst[out_count++] = i1;
                dst[out_count++] = i0;
                dst[out_count++] = i2;
            }
            else
            {
                dst[out_count++] = i0;
                dst[out_count++] = i1;
                dst[out_count++] = i2;
            }
        }
        break;
    case rwPRIMTYPETRIFAN:
        for ( uint32_t i = 1; i + 1 < source_count; ++i )
        {
            dst[out_count++] = read_index( 0 );
            dst[out_count++] = read_index( i );
            dst[out_count++] = read_index( i + 1 );
        }
        break;
    case rwPRIMTYPEPOLYLINE:
        for ( uint32_t i = 0; i + 1 < source_count; ++i )
        {
            dst[out_count++] = read_index( i );
            dst[out_count++] = read_index( i + 1 );
        }
        break;
    default: break;
    }
    return out_count;
}
} // namespace

Im3DStateRecorder::Im3DStateRecorder( ImmediateState &im_state ) noexcept
    : ImState( im_state )
{
    VertexBuffer.resize( Im3DVertexCountLimit );
    IndexBuffer.resize( Im3DIndexCountLimit );
    DrawCalls.resize( 4000 );
}

Im3DStateRecorder::~Im3DStateRecorder() noexcept = default;

void Im3DStateRecorder::Transform( RwIm3DVertex *vertices, uint32_t count,
                                   RwMatrix                 *ltm,
                                   [[maybe_unused]] uint32_t flags )
{
    StashedVertices      = vertices;
    StashedVerticesCount = count;
    if ( ltm )
    {
        StashedWorldTransform = DirectX::XMFLOAT4X3{
            ltm->right.x, ltm->up.x, ltm->at.x, ltm->pos.x,
            ltm->right.y, ltm->up.y, ltm->at.y, ltm->pos.y,
            ltm->right.z, ltm->up.z, ltm->at.z, ltm->pos.z,
        };
    }
    else
    {
        DirectX::XMStoreFloat4x3(
            &StashedWorldTransform,
            DirectX::XMMatrixTranspose( DirectX::XMMatrixIdentity() ) );
    }
}

void Im3DStateRecorder::RenderPrimitive( RwPrimitiveType prim_type )
{
    assert( StashedVertices );
    if ( StashedVerticesCount == 0 )
        return;

    const auto converted_index_count =
        ConvertedIndexCount( prim_type, StashedVerticesCount );
    if ( converted_index_count == 0 )
        return;

    EnsureDrawCallCapacity( DrawCalls, DrawCallCount + 1 );
    EnsureVertexCapacity( VertexBuffer, VertexCount + StashedVerticesCount );
    EnsureIndexCapacity( IndexBuffer, IndexCount + converted_index_count );

    auto &result_dc = DrawCalls[DrawCallCount];

    result_dc.IndexBufferOffset  = IndexCount;
    result_dc.VertexBufferOffset = VertexCount;

    CopyMemory( ( VertexBuffer.data() + VertexCount ), StashedVertices,
                StashedVerticesCount * sizeof( RwIm3DVertex ) );
    VertexCount += StashedVerticesCount;

    result_dc.IndexCount = WriteConvertedIndices(
        prim_type, StashedVerticesCount, IndexBuffer.data() + IndexCount,
        []( uint32_t i ) { return static_cast<uint16_t>( i ); } );
    IndexCount += result_dc.IndexCount;

    result_dc.VertexCount     = StashedVerticesCount;
    result_dc.RasterId        = ImState.Raster;
    result_dc.WorldTransform  = StashedWorldTransform;
    const auto normalizedType = NormalizePrimitiveType( prim_type );
    result_dc.State           = {
        ImState.ColorBlendSrc, ImState.ColorBlendDst,
        ImState.ColorBlendOp,  ImState.BlendEnable,
        ImState.ZTestEnable,   ImState.ZWriteEnable,
        ImState.StencilEnable, static_cast<uint8_t>( normalizedType ) };

    DrawCallCount++;
}

void Im3DStateRecorder::RenderIndexedPrimitive( RwPrimitiveType prim_type,
                                                uint16_t       *indices,
                                                int32_t         num_indices )
{
    assert( indices );
    assert( StashedVertices );
    if ( num_indices <= 0 || StashedVerticesCount == 0 )
        return;

    const auto source_index_count    = static_cast<uint32_t>( num_indices );
    const auto converted_index_count =
        ConvertedIndexCount( prim_type, source_index_count );
    if ( converted_index_count == 0 )
        return;

    EnsureDrawCallCapacity( DrawCalls, DrawCallCount + 1 );
    EnsureVertexCapacity( VertexBuffer, VertexCount + StashedVerticesCount );
    EnsureIndexCapacity( IndexBuffer, IndexCount + converted_index_count );

    auto &result_dc = DrawCalls[DrawCallCount];

    result_dc.IndexBufferOffset  = IndexCount;
    result_dc.VertexBufferOffset = VertexCount;

    CopyMemory( ( VertexBuffer.data() + VertexCount ), StashedVertices,
                StashedVerticesCount * sizeof( RwIm3DVertex ) );
    VertexCount += StashedVerticesCount;

    result_dc.IndexCount = WriteConvertedIndices(
        prim_type, source_index_count, IndexBuffer.data() + IndexCount,
        [indices]( uint32_t i ) { return indices[i]; } );
    IndexCount += result_dc.IndexCount;

    result_dc.VertexCount     = StashedVerticesCount;
    result_dc.RasterId        = ImState.Raster;
    result_dc.WorldTransform  = StashedWorldTransform;
    const auto normalizedType = NormalizePrimitiveType( prim_type );
    result_dc.State           = {
        ImState.ColorBlendSrc, ImState.ColorBlendDst,
        ImState.ColorBlendOp,  ImState.BlendEnable,
        ImState.ZTestEnable,   ImState.ZWriteEnable,
        ImState.StencilEnable, static_cast<uint8_t>( normalizedType ) };

    DrawCallCount++;
}

uint64_t Im3DStateRecorder::Serialize( MemoryWriter &writer )
{
    // serialize index buffer
    uint64_t index_count = IndexCount;
    writer.Write( &index_count );
    if ( IndexCount > 0 )
        writer.Write( IndexBuffer.data(), IndexCount );

    // serialize vertex buffer
    uint64_t vertex_count = VertexCount;
    writer.Write( &vertex_count );
    if ( VertexCount <= 0 )
        return writer.Pos();
    writer.Write( VertexBuffer.data(), VertexCount );

    // serialize drawcalls
    uint64_t dc_count = DrawCallCount;
    writer.Write( &dc_count );
    if ( DrawCallCount <= 0 )
        return writer.Pos();
    writer.Write( DrawCalls.data(), DrawCallCount );

    return writer.Pos();
}

Im3DRenderState Im3DRenderState::Deserialize( MemoryReader &reader )
{
    Im3DRenderState result{};
    auto            idx_count = *reader.Read<uint64_t>();
    if ( idx_count > 0 )
    {
        result.IndexBuffer = { reader.Read<uint16_t>( idx_count ),
                               static_cast<size_t>( idx_count ) };
    }

    auto vtx_count = *reader.Read<uint64_t>();
    if ( vtx_count <= 0 )
        return result;
    result.VertexBuffer = { reader.Read<RwIm3DVertex>( vtx_count ),
                            static_cast<size_t>( vtx_count ) };

    auto dc_count = *reader.Read<uint64_t>();
    if ( dc_count <= 0 )
        return result;
    result.DrawCalls = { reader.Read<Im3DDrawCall>( dc_count ),
                         static_cast<size_t>( dc_count ) };
    return result;
}

void Im3DStateRecorder::Flush()
{
    DrawCallCount = 0;
    VertexCount   = 0;
    IndexCount    = 0;
}
} // namespace rh::rw::engine