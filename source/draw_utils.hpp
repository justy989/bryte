#ifndef BRYTE_DRAW_UTILS_HPP
#define BRYTE_DRAW_UTILS_HPP

#include "types.hpp"

#include <SDL2/SDL.h>

namespace bryte
{
     class draw_utils {
     public:

          static void draw_border ( const rectangle& outline, Uint32 color, SDL_Surface* back_buffer );

     private:

          // not constructable
          draw_utils ( );
     };
}

#endif