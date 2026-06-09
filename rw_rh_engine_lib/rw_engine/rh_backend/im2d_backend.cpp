#include "im2d_backend.h"
#include "../system_funcs/rw_device_system_globals.h"
#include "common_headers.h"
#include "ipc/MemoryWriter.h"
#include "raster_backend.h"
#include <Engine/Common/IBuffer.h>
#include <Engine/Common/IDeviceState.h>
#include <array>
#include <render_client/render_client.h>
#include <vector>

namespace rh::rw::engine
{
using namespace rh::engine;

namespace
{
void AppendTriFan( RwIm2DVertex *vertices, int32_t vertex_count,
                   std::vector<RwIm2DVertex> &out )
{
    for ( int32_t i = 1; i + 1 < vertex_count; i++ )
    {
        out.push_back( vertices[0] );
        out.push_back( vertices[i] );
        out.push_back( vertices[i + 1] );
    }
}

void AppendTriStrip( RwIm2DVertex *vertices, int32_t vertex_count,
                     std::vector<RwIm2DVertex> &out )
{
    for ( int32_t i = 0; i + 2 < vertex_count; i++ )
    {
        if ( i & 1 )
        {
            out.push_back( vertices[i + 1] );
            out.push_back( vertices[i] );
            out.push_back( vertices[i + 2] );
        }
        else
        {
            out.push_back( vertices[i] );
            out.push_back( vertices[i + 1] );
            out.push_back( vertices[i + 2] );
        }
    }
}

std::vector<RwIm2DVertex> IndexedToTriList( int32_t prim_type,
                                            RwIm2DVertex *vertices,
                                            int16_t *indices,
                                            int32_t index_count )
{
    std::vector<RwIm2DVertex> out;
    if ( index_count <= 0 )
        return out;

    if ( prim_type == RwPrimitiveType::rwPRIMTYPETRIFAN && index_count >= 3 )
        out.reserve( ( index_count - 2 ) * 3 );
    else if ( prim_type == RwPrimitiveType::rwPRIMTYPETRISTRIP &&
              index_count >= 3 )
        out.reserve( ( index_count - 2 ) * 3 );
    else
        out.reserve( index_count );

    auto read = [vertices, indices]( int32_t i ) -> RwIm2DVertex & {
        return vertices[indices[i]];
    };

    switch ( prim_type )
    {
    case RwPrimitiveType::rwPRIMTYPETRILIST:
        for ( int32_t i = 0; i < index_count; i++ )
            out.push_back( read( i ) );
        break;
    case RwPrimitiveType::rwPRIMTYPETRIFAN:
        for ( int32_t i = 1; i + 1 < index_count; i++ )
        {
            out.push_back( read( 0 ) );
            out.push_back( read( i ) );
            out.push_back( read( i + 1 ) );
        }
        break;
    case RwPrimitiveType::rwPRIMTYPETRISTRIP:
        for ( int32_t i = 0; i + 2 < index_count; i++ )
        {
            if ( i & 1 )
            {
                out.push_back( read( i + 1 ) );
                out.push_back( read( i ) );
                out.push_back( read( i + 2 ) );
            }
            else
            {
                out.push_back( read( i ) );
                out.push_back( read( i + 1 ) );
                out.push_back( read( i + 2 ) );
            }
        }
        break;
    default: break;
    }
    return out;
}
} // namespace

int32_t Im2DRenderPrimitiveFunction( int32_t primType, RwIm2DVertex *vertices,
                                     int32_t numVertices )
{
    if ( gRwDeviceGlobals.DeviceGlobalsPtr->curCamera == nullptr )
        return 1;

    auto &im2d = gRenderClient->RenderState.Im2D;
    if ( primType == RwPrimitiveType::rwPRIMTYPETRILIST )
    {
        im2d.RecordDrawCall( vertices, numVertices );
        return 1;
    }

    std::vector<RwIm2DVertex> vertices_2;
    if ( primType == RwPrimitiveType::rwPRIMTYPETRIFAN )
        AppendTriFan( vertices, numVertices, vertices_2 );
    else if ( primType == RwPrimitiveType::rwPRIMTYPETRISTRIP )
        AppendTriStrip( vertices, numVertices, vertices_2 );

    if ( !vertices_2.empty() )
        im2d.RecordDrawCall( vertices_2.data(), vertices_2.size() );
    return 1;
}

int32_t Im2DRenderIndexedPrimitiveFunction( int32_t       primType,
                                            RwIm2DVertex *vertices,
                                            int32_t       numVertices,
                                            int16_t *     indices,
                                            int32_t       numIndices )
{
    if ( gRwDeviceGlobals.DeviceGlobalsPtr->curCamera == nullptr )
        return 1;

    auto &im2d = gRenderClient->RenderState.Im2D;
    if ( primType == RwPrimitiveType::rwPRIMTYPETRILIST )
    {
        im2d.RecordDrawCall( vertices, numVertices, indices, numIndices );
        return 1;
    }

    auto vertices_2 = IndexedToTriList( primType, vertices, indices, numIndices );
    if ( !vertices_2.empty() )
        im2d.RecordDrawCall( vertices_2.data(), vertices_2.size() );
    return 1;
}
} // namespace rh::rw::engine
