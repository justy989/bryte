#include "Player.hpp"
#include "Utils.hpp"
#include "Log.hpp"

#include <fstream>

using namespace bryte;

const Real32 Player::c_item_cooldown = 1.0f;

Void Player::clear ( )
{
     state = State::idle;

     facing = Direction::up;
     state_watch.reset ( 0.0f );

     health           = 6;
     max_health       = 6;

     velocity.zero ( );
     acceleration.zero ( );

     dimension.set ( pixels_to_meters ( 16 ), pixels_to_meters ( 16 ) );

     collision_offset.set ( pixels_to_meters ( 5 ), pixels_to_meters ( 2 ) );
     collision_dimension.set ( pixels_to_meters ( 6 ), pixels_to_meters ( 7 ) );

     walk_acceleration = 18.0f;
     deceleration_scale = 6.0f;

     walk_tracker = 0.0f;
     walk_frame = 0;
     walk_frame_count = 3;
     walk_frame_rate = 30.0f;
     walk_frame_change = 1;
     constant_animation = false;
     draw_facing = true;

     damage_pushed = Direction::count;

     damage_watch.reset ( 0.0f );
     cooldown_watch.reset ( 0.0f );
     element_watch.reset ( 0.0f );

     effected_by_element = Element::none;
     on_ice = false;
     on_moving_walkway = Direction::count;

     position.zero ( );

     flies = false;
     knockbackable = true;
     collides_with_exits = false;

     item_mode = ItemMode::shield;
     item_cooldown.reset ( 0.0f );

     key_count   = 0;

     arrow_count = 0;
     max_arrows = 10;

     bomb_count = 0;
     max_bombs = 4;

     // TODO: init to false
     has_bow = true;

     save_slot = 0;
}

Bool Player::save ( )
{
     char filepath [ 128 ];

     sprintf ( filepath, "save/slot_%d.brs", save_slot );

     std::ofstream file ( filepath, std::ios::binary );

     LOG_INFO ( "Saving player info: %s\n", filepath );

     if ( !file.is_open ( ) ) {
          LOG_ERROR ( "Failed to open file.\n" );
          return false;
     }

     file.write ( reinterpret_cast<const Char8*>( &health ), sizeof ( health ) );
     file.write ( reinterpret_cast<const Char8*>( &max_health ), sizeof ( max_health ) );

     file.write ( reinterpret_cast<const Char8*>( &key_count ), sizeof ( key_count ) );

     file.write ( reinterpret_cast<const Char8*>( &arrow_count ), sizeof ( arrow_count ) );
     file.write ( reinterpret_cast<const Char8*>( &max_arrows ), sizeof ( max_arrows ) );

     file.write ( reinterpret_cast<const Char8*>( &bomb_count ), sizeof ( bomb_count ) );
     file.write ( reinterpret_cast<const Char8*>( &max_bombs ), sizeof ( max_bombs ) );

     file.write ( reinterpret_cast<const Char8*>( &has_bow ), sizeof ( has_bow ) );

     file.close ( );

     return true;
}

Bool Player::load ( )
{
     char filepath [ 128 ];

     sprintf ( filepath, "save/slot_%d.brs", save_slot );

     LOG_INFO ( "Loading player info: %s\n", filepath );

     std::ifstream file ( filepath, std::ios::binary );

     if ( !file.is_open ( ) ) {
          LOG_ERROR ( "Failed to open file.\n" );
          return false;
     }

     file.read ( reinterpret_cast<Char8*>( &health ), sizeof ( health ) );
     file.read ( reinterpret_cast<Char8*>( &max_health ), sizeof ( max_health ) );

     file.read ( reinterpret_cast<Char8*>( &arrow_count ), sizeof ( arrow_count ) );
     file.read ( reinterpret_cast<Char8*>( &max_arrows ), sizeof ( max_arrows ) );

     file.read ( reinterpret_cast<Char8*>( &bomb_count ), sizeof ( bomb_count ) );
     file.read ( reinterpret_cast<Char8*>( &max_bombs ), sizeof ( max_bombs ) );

     file.read ( reinterpret_cast<Char8*>( &has_bow ), sizeof ( has_bow ) );

     file.close ( );

     return true;
}

Void Player::give_arrow ( )
{
     arrow_count += c_arrow_increment;

     if ( arrow_count >= max_arrows ) {
          arrow_count = max_arrows;
     }
}

Void Player::give_bomb ( )
{
     bomb_count += c_bomb_increment;

     if ( bomb_count >= max_bombs ) {
          bomb_count = max_bombs;
     }
}

Void Player::give_upgrade ( Upgrade upgrade )
{
     switch ( upgrade ) {
     default:
          break;
     case Upgrade::heart:
          max_health += c_max_health_increment;
          health = max_health; // NOTE: do we want to do this?
          break;
     case Upgrade::quiver:
          max_arrows += c_max_arrow_increment;
          break;
     case Upgrade::bomb_bag:
          max_bombs += c_max_bomb_increment;
          break;
     case Upgrade::bow:
          has_bow = true;
          break;
     }
}

Bool Player::use_bow ( )
{
     if ( item_cooldown.expired ( ) && arrow_count > 0 ) {
          arrow_count--;
          item_cooldown.reset ( Player::c_item_cooldown );
          return true;
     }

     return false;
}

Bool Player::use_bomb ( )
{
     if ( item_cooldown.expired ( ) && bomb_count > 0 ) {
          bomb_count--;
          return true;
     }

     return false;
}

