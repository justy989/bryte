#include "room.hpp"

#include <cassert>

using namespace bryte;

room::room ( coordinate_base_type width, coordinate_base_type height )
{
     reset ( width, height );
}

void room::reset ( coordinate_base_type width, coordinate_base_type height )
{
     m_width = width;
     m_height = height;

     auto size = width * height;

     // clear everything
     for ( int i = 0; i < size; ++i ) {
          m_tiles [ i ] = 0;
          m_solids [ i ] = false;
     }
}

coordinate_base_type room::location_to_index ( const location& loc )
{
     // validate input location
     assert ( loc.x ( ) >= 0 );
     assert ( loc.y ( ) >= 0 );
     assert ( loc.x ( ) < m_width );
     assert ( loc.y ( ) < m_height );

     return ( loc.y ( ) * m_width ) + loc.x ( );
}