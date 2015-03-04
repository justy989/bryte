#include "Text.hpp"
#include "Utils.hpp"
#include "GameMemory.hpp"
#include "Bitmap.hpp"

#include <ctype.h>

Bool Text::load_surfaces ( GameMemory& game_memory )
{
     if ( !load_bitmap_with_game_memory ( fontsheet, game_memory, "text.bmp" ) ) {
          return false;
     }

     character_width   = 5;
     character_height  = 8;
     character_spacing = 1;

     return true;
}

Void Text::unload ( )
{
     FREE_SURFACE ( fontsheet );
}

Void Text::render ( SDL_Surface* back_buffer, const Char8* message, Int32 position_x, Int32 position_y )
{
     Char8 c;
     SDL_Rect dest { position_x, position_y, character_width, character_height };
     SDL_Rect clip { 0, 0, character_width, character_height };

     while ( ( c = *message ) ) {

          if ( isalpha ( c ) ) {
               clip.x = ( c - 'A' ) * character_width;
          } else if ( isdigit ( c ) ) {
               clip.x = ( ( c - '0' ) * character_width ) + ( ( ( 'Z' - 'A' ) + 1 ) * character_width );
          } else if ( c == '.' ) {
               clip.x = ( ( 'Z' - 'A' ) + ( '9' - '0' ) + 3 ) * character_width;
          } else if ( c == '+' ) {
               clip.x = ( ( 'Z' - 'A' ) + ( '9' - '0' ) + 5 ) * character_width;
          } else if ( c == ' ' ) {
               message++;
               dest.x += ( character_width + character_spacing );
               continue;
          }

          SDL_BlitSurface ( fontsheet, &clip, back_buffer, &dest );

          message++;
          dest.x += ( character_width + character_spacing );
     }
}

