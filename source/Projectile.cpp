#include "Projectile.hpp"
#include "Map.hpp"
#include "Interactives.hpp"

using namespace bryte;

const Real32 Projectile::c_arrow_speed = 20.0f;
const Real32 Projectile::c_goo_speed = 5.0f;
const Real32 Projectile::c_stuck_time = 1.5f;

Vector Projectile::collision_points [ Direction::count ];

static Vector vector_from_direction ( Direction dir )
{
     switch ( dir )
     {
     default:
          ASSERT ( 0 );
          break;
     case Direction::left:
          return Vector { -1.0f, 0.0f };
     case Direction::up:
          return Vector { 0.0f, 1.0f };
     case Direction::right:
          return Vector { 1.0f, 0.0f };
     case Direction::down:
          return Vector { 0.0f, -1.0f };
     }

     // should not hit
     return Vector { 0.0f, 0.0f };
}

Void Projectile::hit_character ( Character& character )
{
     character.damage ( 1, facing );

     if ( on_fire ) {
          character.light_on_fire ( );
     }

     switch ( type ) {
     default:
          break;
     case Type::arrow:
          track_entity.entity = &character;
          track_entity.offset = position - character.position;
          stuck_watch.reset ( Projectile::c_stuck_time );
          break;
     case Type::goo:
          life_state = LifeState::dead;
          clear ( );
          break;
     }
}

Bool Projectile::check_for_solids ( const Map& map, Interactives& interactives )
{
     Vector arrow_center = position + Projectile::collision_points [ facing ];
     Map::Coordinates tile = Map::vector_to_coordinates ( arrow_center );

     if ( !map.coordinates_valid ( tile ) ) {
          return false;
     }

     if ( map.get_coordinate_solid ( tile.x, tile.y ) ) {
          return true;
     }

     Auto& interactive = interactives.get_from_tile ( tile.x, tile.y );

     if ( interactive.is_solid ( ) ) {
          // do not activate exits!
          if ( interactive.type == Interactive::Type::torch ) {

               if ( on_fire && !interactive.interactive_torch.on ) {
                   interactive.interactive_torch.on = true;
               }

               if ( interactive.interactive_torch.on ) {
                    on_fire = true;
               }

               return false;
          } else if ( interactive.type == Interactive::Type::pushable_torch ) {
               if ( on_fire && !interactive.interactive_pushable_torch.torch.on ) {
                   interactive.interactive_pushable_torch.torch.on = true;
               }

               if ( interactive.interactive_pushable_torch.torch.on ) {
                    on_fire = true;
               }

               return false;
          } else if ( interactive.type != Interactive::Type::exit ) {
               interactive.activate ( interactives );
          }

          return true;
     } else {
          if ( interactive.type == Interactive::Type::exit ) {
               // otherwise arrows can escape when doors are open
               // TODO: is this ok?
               return true;
          }
     }

     return false;
}

Void Projectile::update ( float time_delta, const Map& map, Interactives& interactives  )
{
     switch ( type ) {
     default:
          ASSERT ( 0 );
          break;
     case Type::arrow:
          update_arrow ( time_delta, map, interactives );
          break;
     case Type::goo:
          update_goo ( time_delta, map, interactives );
          break;
     }
}

Void Projectile::update_arrow ( float time_delta, const Map& map, Interactives& interactives )
{
     switch ( life_state ) {
     default:
          ASSERT ( 0 );
          break;
     case LifeState::dead:
          break;
     case LifeState::spawning:
          // nop for now ** pre-mature planning wooo **
          life_state = LifeState::alive;
          break;
     case LifeState::alive:
          if ( stuck_watch.expired ( ) ) {
               position += vector_from_direction ( facing ) * c_arrow_speed * time_delta;

               if ( check_for_solids ( map, interactives ) ) {
                    stuck_watch.reset ( c_stuck_time );
               }
          } else {
               stuck_watch.tick ( time_delta );

               if ( track_entity.entity ) {
                    position = track_entity.entity->position + track_entity.offset;
               }

               if ( stuck_watch.expired ( ) ) {
                    life_state = dead;
                    clear ( );
               }
          }
          break;
     }
}

Void Projectile::update_goo ( float time_delta, const Map& map, Interactives& interactives )
{
     switch ( life_state ) {
     default:
          ASSERT ( 0 );
          break;
     case LifeState::dead:
          break;
     case LifeState::spawning:
          // nop for now ** pre-mature planning wooo **
          life_state = LifeState::alive;
          break;
     case LifeState::alive:
          position += vector_from_direction ( facing ) * c_goo_speed * time_delta;

          if ( check_for_solids ( map, interactives ) ) {
               stuck_watch.reset ( c_stuck_time );
               life_state = dead;
               clear ( );
          }
          break;
     }
}

Void Projectile::clear ( )
{
     type = Type::arrow;
     facing = Direction::left;
     stuck_watch.reset ( 0.0f );
     track_entity.entity = nullptr;
     track_entity.offset.zero ( );
     position.zero ( );
}
