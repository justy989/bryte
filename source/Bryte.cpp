#include "Bryte.hpp"
#include "Utils.hpp"
#include "Bitmap.hpp"
#include "Camera.hpp"
#include "MapDisplay.hpp"

#ifdef WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdio>
#include <cmath>
#include <cstdlib>

using namespace bryte;

static const Real32 c_lever_width             = 0.5f;
static const Real32 c_lever_height            = 0.5f;

static const Char8* c_test_tilesheet_path        = "castle_tilesheet.bmp";
static const Char8* c_test_decorsheet_path       = "castle_decorsheet.bmp";
static const Char8* c_test_lampsheet_path        = "castle_lampsheet.bmp";
static const Char8* c_test_player_path           = "test_hero.bmp";
static const Char8* c_test_rat_path              = "test_rat.bmp";
static const Char8* c_test_bat_path              = "test_bat.bmp";
static const Char8* c_test_pickups_path          = "test_pickups.bmp";

Vector Arrow::collision_points [ Direction::count ];

static State* get_state ( GameMemory& game_memory )
{
     return reinterpret_cast<MemoryLocations*>( game_memory.location ( ) )->state;
}

Vector vector_from_direction ( Direction dir )
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

const Real32 Arrow::c_speed = 20.0f;
const Real32 Arrow::c_stuck_time = 1.5f;

Bool Arrow::check_for_solids ( const Map& map, Interactives& interactives )
{
     Vector arrow_center = position + Arrow::collision_points [ facing ];

     Int32 tile_x = static_cast<Int32>( arrow_center.x ( ) / Map::c_tile_dimension_in_meters );
     Int32 tile_y = static_cast<Int32>( arrow_center.y ( ) / Map::c_tile_dimension_in_meters );

     if ( tile_x < 0 || tile_x >= map.width ( ) ||
          tile_y < 0 || tile_y >= map.height ( ) ) {
          return false;
     }

     if ( map.get_coordinate_solid ( tile_x, tile_y ) ) {
          return true;
     }

     auto& interactive = interactives.get_from_tile ( tile_x, tile_y );

     if ( interactive.is_solid ( ) ) {
          // do not activate exits!
          if ( interactive.type != Interactive::Type::exit ) {
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

Void Arrow::update ( float time_delta, const Map& map, Interactives& interactives )
{
     switch ( state ) {
     default:
          ASSERT ( 0 );
          break;
     case State::dead:
          break;
     case State::spawning:
          // nop for now ** pre-mature planning wooo **
          state = flying;
          break;
     case State::flying:
          position += vector_from_direction ( facing ) * c_speed * time_delta;
          if ( check_for_solids ( map, interactives ) ) {
               state = State::stuck;
               stuck_watch.reset ( c_stuck_time );
          }
          break;
     case State::stuck:
          stuck_watch.tick ( time_delta );

          if ( stuck_watch.expired ( ) ) {
               state = dead;
          }
          break;
     }
}

// assuming A attacks B
static Direction determine_damage_direction ( const Character& a, const Character& b, Random& random )
{
     Real32 a_center_x = a.position.x ( ) + a.width ( ) * 0.5f;
     Real32 a_center_y = a.position.y ( ) + a.collision_dimension.y ( ) * 0.5f;

     Real32 b_center_x = b.position.x ( ) + b.width ( ) * 0.5f;
     Real32 b_center_y = b.position.y ( ) + b.collision_dimension.x ( ) * 0.5f;

     Real32 diff_x = b_center_x - a_center_x;
     Real32 diff_y = b_center_y - a_center_y;

     Real32 abs_x = fabs ( diff_x );
     Real32 abs_y = fabs ( diff_y );

     if ( abs_x > abs_y ) {
          if ( diff_x > 0.0f ) {
               return Direction::right;
          }

          return Direction::left;
     } else if ( abs_y > abs_x ) {
          if ( diff_y > 0.0f ) {
               return Direction::up;
          }

          return Direction::down;
     } else {
          Direction valid_dirs [ 2 ];

          if ( diff_x > 0.0f ) {
               valid_dirs [ 0 ] = Direction::right;
          } else {
               valid_dirs [ 0 ] = Direction::left;
          }

          if ( diff_y > 0.0f ) {
               valid_dirs [ 1 ] = Direction::up;
          } else {
               valid_dirs [ 1 ] = Direction::down;
          }

          // coin flip between using the x or y direction
          return valid_dirs [ random.generate ( 0, 2 ) ];
     }

     // the above cases should catch all
     ASSERT ( 0 );
     return Direction::left;
}

Bool State::initialize ( GameMemory& game_memory, Settings* settings )
{
     random.seed ( 13371 );

     player_spawn_tile_x = settings->player_spawn_tile_x;
     player_spawn_tile_y = settings->player_spawn_tile_y;

     player.position.set ( pixels_to_meters ( player_spawn_tile_x * Map::c_tile_dimension_in_pixels ),
                           pixels_to_meters ( player_spawn_tile_y * Map::c_tile_dimension_in_pixels ) );

     player.state  = Character::State::alive;
     player.facing = Direction::left;

     player.health           = 25;
     player.max_health       = 25;

     player.velocity.zero ( );
     player.acceleration.zero ( );

     player.dimension.set ( pixels_to_meters ( 16 ), pixels_to_meters ( 16 ) );
     player.collision_offset.set ( pixels_to_meters ( 5 ), pixels_to_meters ( 2 ) );
     player.collision_dimension.set ( pixels_to_meters ( 6 ), pixels_to_meters ( 7 ) );

     player.damage_pushed = Direction::left;
     player.state_watch.reset ( 0.0f );
     player.damage_watch.reset ( 0.0f );
     player.cooldown_watch.reset ( 0.0f );

     player.collides_with_exits  = false;
     player.collides_with_solids = true;

     player.walk_acceleration.set ( 8.5f, 8.5f );

     // ensure all enemies start dead
     for ( Uint32 i = 0; i < c_max_enemies; ++i ) {
         enemies [ i ].init ( Enemy::Type::count, 0.0f, 0.0f, Direction::left, Pickup::Type::none );
         enemies [ i ].state = Character::State::dead;
     }

     enemy_count = 0;

     for ( Uint32 i = 0; i < c_max_pickups; ++i ) {
          pickups [ i ].type = Pickup::Type::none;
     }

     for ( int i = 0; i < Enemy::Type::count; ++i ) {
          character_display.enemy_sheets [ i ] = nullptr;
     }

     for ( int i = 0; i < Interactive::Type::count; ++i ) {
          interactives_display.interactive_sheets [ i ] = nullptr;
     }

     FileContents text_contents = load_entire_file ( "text.bmp", &game_memory );

     text.fontsheet         = load_bitmap ( &text_contents );
     text.character_width   = 5;
     text.character_height  = 8;
     text.character_spacing = 1;

     if ( !text.fontsheet ) {
          return false;
     }

     // load test graphics
     if ( !load_bitmap_with_game_memory ( map_display.tilesheet, game_memory,
                                          c_test_tilesheet_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( map_display.decorsheet, game_memory,
                                          c_test_decorsheet_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( map_display.lampsheet, game_memory,
                                          c_test_lampsheet_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( character_display.enemy_sheets [ Enemy::Type::rat ], game_memory,
                                          c_test_rat_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( character_display.enemy_sheets [ Enemy::Type::bat ], game_memory,
                                          c_test_bat_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( character_display.player_sheet, game_memory,
                                          c_test_player_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( character_display.vertical_sword_sheet, game_memory,
                                          "test_vertical_sword.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( character_display.horizontal_sword_sheet, game_memory,
                                          "test_horizontal_sword.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::exit ],
                                          game_memory,
                                          "castle_exitsheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::lever ],
                                          game_memory,
                                          "castle_leversheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::pushable_block ],
                                          game_memory,
                                          "castle_pushableblocksheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::torch ],
                                          game_memory,
                                          "castle_torchsheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::pushable_torch ],
                                          game_memory,
                                          "castle_pushabletorchsheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( interactives_display.interactive_sheets [ Interactive::Type::light_detector ],
                                          game_memory,
                                          "castle_lightdetectorsheet.bmp" ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( pickup_sheet, game_memory, c_test_pickups_path ) ) {
          return false;
     }

     if ( !load_bitmap_with_game_memory ( arrow_sheet, game_memory, "test_arrow.bmp" ) ) {
          return false;
     }

     for ( Int32 i = 0; i < 4; ++i ) {
          direction_keys [ i ] = false;
     }

     map.load_master_list ( settings->map_master_list_filename );
     map.load_from_master_list ( settings->map_index, interactives );
     spawn_map_enemies ( );

     player_key_count = 0;

     attack_key = false;

     Arrow::collision_points [ Direction::left ].set ( pixels_to_meters ( 1 ), pixels_to_meters ( 7 ) );
     Arrow::collision_points [ Direction::up ].set ( pixels_to_meters ( 7 ), pixels_to_meters ( 14 ) );
     Arrow::collision_points [ Direction::right ].set ( pixels_to_meters ( 14 ), pixels_to_meters ( 7 ) );
     Arrow::collision_points [ Direction::down ].set ( pixels_to_meters ( 7 ), pixels_to_meters ( 1 ) );

#ifdef DEBUG
     enemy_think = true;
#endif

     return true;
}

Void State::destroy ( )
{
     SDL_FreeSurface ( map_display.tilesheet );
     SDL_FreeSurface ( map_display.decorsheet );
     SDL_FreeSurface ( map_display.lampsheet );

     SDL_FreeSurface ( character_display.player_sheet );

     for ( int i = 0; i < Enemy::Type::count; ++i ) {
          SDL_FreeSurface ( character_display.enemy_sheets [ i ] );
     }

     for ( int i = 0; i < Interactive::Type::count; ++i ) {
          if ( interactives_display.interactive_sheets [ i ] ) {
               SDL_FreeSurface ( interactives_display.interactive_sheets [ i ] );
          }
     }

     SDL_FreeSurface ( pickup_sheet );
}

Bool State::spawn_enemy ( Real32 x, Real32 y, Uint8 id, Direction facing, Pickup::Type drop )
{
     Enemy* enemy = nullptr;

     for ( Uint32 i = 0; i < c_max_enemies; ++i ) {
          if ( enemies [ i ].state == Character::State::dead ) {
               enemy = enemies + i;
               break;
          }
     }

     if ( !enemy ) {
          LOG_WARNING ( "Tried to spawn enemy when %d enemies already exist.\n", c_max_enemies );
          return false;
     }

#ifdef DEBUG
     static const char* enemy_id_names [ ] = { "rat", "bat" };
#endif

     LOG_DEBUG ( "Spawning enemy %s at: %f, %f\n", enemy_id_names [ id ], x, y );

     enemy->init ( static_cast<Enemy::Type>( id ), x, y, facing, drop );

     enemy_count++;

     return true;
}

Bool State::spawn_pickup ( Real32 x, Real32 y, Pickup::Type type )
{
     for ( Uint32 i = 0; i < State::c_max_pickups; ++i ) {
          Pickup& pickup = pickups [ i ];

          if ( pickup.type == Pickup::Type::none ) {
               pickup.type = type;
               pickup.position.set ( x, y );

               LOG_DEBUG ( "Spawn pickup %s at %f, %f\n", Pickup::c_names [ type ], x, y );

               return true;
          }
     }

     return false;
}

Bool State::spawn_arrow ( Real32 x, Real32 y, Direction facing )
{
     for ( Uint32 i = 0; i < c_max_arrows; ++i ) {
          Arrow& arrow = arrows [ i ];

          if ( arrow.state == Arrow::State::dead ) {
               LOG_DEBUG ( "Spawn Arrow %f, %f -> %d\n", x, y, facing );
               arrow.state    = Arrow::State::spawning;
               arrow.position.set ( x, y );
               arrow.facing   = facing;
               return true;
          }
     }

     return false;
}

Void State::spawn_map_enemies ( )
{
     for ( int i = 0; i < map.enemy_spawn_count ( ); ++i ) {
          auto& enemy_spawn = map.enemy_spawn ( i );

          spawn_enemy ( pixels_to_meters ( enemy_spawn.location.x * Map::c_tile_dimension_in_pixels ),
                        pixels_to_meters ( enemy_spawn.location.y * Map::c_tile_dimension_in_pixels ),
                        enemy_spawn.id, enemy_spawn.facing, enemy_spawn.drop );
     }
}

Void State::clear_enemies ( )
{
     for ( Uint32 i = 0; i < enemy_count; ++i ) {
          enemies [ i ].state = Character::State::dead;
     }

     enemy_count = 0;
}

Void State::clear_pickups ( )
{
     for ( Uint32 i = 0; i < c_max_pickups; ++i ) {
          pickups [ i ].type = Pickup::Type::none;
     }
}

Void State::clear_arrows ( )
{
     for ( Uint32 i = 0; i < c_max_arrows; ++i ) {
          arrows [ i ].position.set ( 0.0f, 0.0f );
          arrows [ i ].facing = Direction::left;
          arrows [ i ].state = Arrow::State::dead;
          arrows [ i ].stuck_watch.reset ( 0.0f );
     }
}

Void State::player_death ( )
{
     player.state  = Character::State::alive;
     player.facing = Direction::left;

     player.health           = 25;
     player.max_health       = 25;

     player.position.set ( pixels_to_meters ( player_spawn_tile_x * Map::c_tile_dimension_in_pixels ),
                           pixels_to_meters ( player_spawn_tile_y * Map::c_tile_dimension_in_pixels ) );

     player.velocity.zero ( );

     player.damage_pushed = Direction::left;
     player.state_watch.reset ( 0.0f );
     player.damage_watch.reset ( 0.0f );
     player.cooldown_watch.reset ( 0.0f );

     // load the first map
     map.load_from_master_list ( 0, interactives );

     clear_enemies ( );
     spawn_map_enemies ( );
}

extern "C" Bool game_init ( GameMemory& game_memory, void* settings )
{
     MemoryLocations* memory_locations = GAME_PUSH_MEMORY ( game_memory, MemoryLocations );
     State* state = GAME_PUSH_MEMORY ( game_memory, State );

     memory_locations->state = state;

     if ( !state->initialize ( game_memory, reinterpret_cast<Settings*>( settings ) ) ) {
          return false;
     }

     return true;
}

extern "C" Void game_destroy ( GameMemory& game_memory )
{
     auto* state = get_state ( game_memory );

     state->destroy ( );
}

extern "C" Void game_user_input ( GameMemory& game_memory, const GameInput& game_input )
{
     auto* state = get_state ( game_memory );

     for ( Uint32 i = 0; i < game_input.key_change_count; ++i ) {
          const GameInput::KeyChange& key_change = game_input.key_changes [ i ];

          switch ( key_change.scan_code ) {
          default:
               break;
          case SDL_SCANCODE_W:
               state->direction_keys [ Direction::up ]    = key_change.down;
               break;
          case SDL_SCANCODE_S:
               state->direction_keys [ Direction::down ]  = key_change.down;
               break;
          case SDL_SCANCODE_A:
               state->direction_keys [ Direction::left ]  = key_change.down;
               break;
          case SDL_SCANCODE_D:
               state->direction_keys [ Direction::right ] = key_change.down;
               break;
          case SDL_SCANCODE_SPACE:
               state->attack_key = key_change.down;
               break;
          case SDL_SCANCODE_E:
               state->activate_key = key_change.down;
               break;
          case SDL_SCANCODE_8:
               if ( key_change.down ) {
                    state->spawn_enemy ( state->player.position.x ( ) - state->player.width ( ),
                                         state->player.position.y ( ), 0, Direction::left,
                                         Pickup::Type::health );
               }
               break;
#ifdef DEBUG
          case SDL_SCANCODE_I:
               if ( key_change.down ) {
                    state->enemy_think = !state->enemy_think;
               }
               break;
          case SDL_SCANCODE_K:
               if ( key_change.down ) {
                    state->player_key_count++;
               }
               break;
#endif
          }
     }
}

Void character_adjacent_tile ( const Character& character, Int32* adjacent_tile_x, Int32* adjacent_tile_y )
{
     Vector character_center { character.collision_center_x ( ),
                               character.collision_center_y ( ) };

     Int32 character_center_tile_x = meters_to_pixels ( character_center.x ( ) ) / Map::c_tile_dimension_in_pixels;
     Int32 character_center_tile_y = meters_to_pixels ( character_center.y ( ) ) / Map::c_tile_dimension_in_pixels;

     switch ( character.facing ) {
          default:
               break;
          case Direction::left:
               character_center_tile_x--;
               break;
          case Direction::right:
               character_center_tile_x++;
               break;
          case Direction::up:
               character_center_tile_y++;
               break;
          case Direction::down:
               character_center_tile_y--;
               break;
     }

     *adjacent_tile_x = character_center_tile_x;
     *adjacent_tile_y = character_center_tile_y;
}

extern "C" Void game_update ( GameMemory& game_memory, Real32 time_delta )
{
     auto* state = get_state ( game_memory );

     if ( state->direction_keys [ Direction::up ] ) {
          state->player.walk ( Direction::up );
     }

     if ( state->direction_keys [ Direction::down ] ) {
          state->player.walk ( Direction::down );
     }

     if ( state->direction_keys [ Direction::right ] ) {
          state->player.walk ( Direction::right );
     }

     if ( state->direction_keys [ Direction::left ] ) {
          state->player.walk ( Direction::left );
     }

     if ( state->attack_key ) {
          state->attack_key = false;

          state->player.attack ( );

          state->spawn_arrow ( state->player.position.x ( ),
                               state->player.position.y ( ),
                               state->player.facing );
     }

     state->player.update ( time_delta, state->map, state->interactives );

     if ( state->player.state == Character::State::pushing ) {
          Int32 push_tile_x = 0;
          Int32 push_tile_y = 0;

          character_adjacent_tile ( state->player, &push_tile_x, &push_tile_y );

          if ( push_tile_x >= 0 && push_tile_x < state->interactives.width ( ) &&
               push_tile_y >= 0 && push_tile_y < state->interactives.height ( ) ) {
               state->interactives.push ( push_tile_x, push_tile_y, state->player.facing, state->map, state->enemies, state->enemy_count, state->player );
          }
     }

     for ( Uint32 i = 0; i < State::c_max_enemies; ++i ) {
          auto& enemy = state->enemies [ i ];

          if ( enemy.state == Character::State::dead ) {
               continue;
          }

#ifdef DEBUG
          if ( state->enemy_think ) {
               enemy.think ( state->player.position, state->random, time_delta );
          }
#else
          enemy.think ( state->player.position, state->random, time_delta );
#endif

          enemy.update ( time_delta, state->map, state->interactives );

#if 0
          // not sure I want enemies messing with your puzzles
          if ( enemy.state == Character::State::pushing ) {
               Int32 push_tile_x = 0;
               Int32 push_tile_y = 0;

               character_adjacent_tile ( enemy, &push_tile_x, &push_tile_y );

               if ( push_tile_x >= 0 && push_tile_x < state->interactives.width ( ) &&
                    push_tile_y >= 0 && push_tile_y < state->interactives.height ( ) ) {
                    state->interactives.push ( push_tile_x, push_tile_y, enemy.facing, state->map, state->enemies, state->enemy_count, state->player );
               }
          }
#endif

          // check collision between player and enemy
          if ( state->player.state != Character::State::blinking &&
               state->player.collides_with ( enemy ) ) {
               Direction damage_dir = determine_damage_direction ( enemy, state->player, state->random );
               state->player.damage ( 1, damage_dir );

               if ( state->player.state == Character::State::dead ) {
                    state->player_death ( );
               }
          }

          // attacking enemy
          if ( state->player.state == Character::State::attacking &&
               enemy.state != Character::State::blinking &&
               state->player.attack_collides_with ( enemy ) ) {
               Direction damage_dir = determine_damage_direction ( state->player, enemy, state->random );
               enemy.damage ( 1, damage_dir );

               if ( enemy.state == Character::State::dead ) {
                    if ( enemy.drop != Pickup::Type::none ) {
                         state->spawn_pickup ( enemy.position.x ( ), enemy.position.y ( ), enemy.drop );
                    }
#if 0
                    // generate an item to drop
                    auto roll = state->random.generate ( 1, 11 );

                    if ( roll > 5 && roll < 8 ) {
                         state->spawn_pickup ( enemy.position.x ( ), enemy.position.y ( ), Pickup::Type::health );
                    } else if ( roll >= 8 ) {
                         state->spawn_pickup ( enemy.position.x ( ), enemy.position.y ( ), Pickup::Type::key );
                    }
#endif
               }
          }
     }

     // update interactives
     state->interactives.update ( time_delta );

     if ( state->activate_key ) {
          state->activate_key = false;

          Int32 player_activate_tile_x = 0;
          Int32 player_activate_tile_y = 0;

          character_adjacent_tile ( state->player, &player_activate_tile_x, &player_activate_tile_y );

          if ( player_activate_tile_x >= 0 && player_activate_tile_x < state->interactives.width ( ) &&
               player_activate_tile_y >= 0 && player_activate_tile_y < state->interactives.height ( ) ) {

               auto& interactive = state->interactives.get_from_tile ( player_activate_tile_x, player_activate_tile_y );

               if ( interactive.type == Interactive::Type::exit ) {
                    if ( interactive.interactive_exit.state == Exit::State::locked &&
                         state->player_key_count > 0 ) {
                         LOG_DEBUG ( "Unlock Door: %d, %d\n", player_activate_tile_x, player_activate_tile_y );
                         state->interactives.activate ( player_activate_tile_x, player_activate_tile_y );
                         state->player_key_count--;
                    }
               } else {
                    LOG_DEBUG ( "Activate: %d, %d\n", player_activate_tile_x, player_activate_tile_y );
                    state->interactives.activate ( player_activate_tile_x, player_activate_tile_y );
               }
          }

     }

     for ( Uint32 i = 0; i < State::c_max_arrows; ++i ) {
          state->arrows [ i ].update ( time_delta, state->map, state->interactives );
     }

     for ( Uint32 i = 0; i < State::c_max_pickups; ++i ) {
          Pickup& pickup = state->pickups [ i ];

          if ( pickup.type == Pickup::Type::none ) {
               continue;
          }

          if ( rect_collides_with_rect ( state->player.collision_x ( ), state->player.collision_y ( ),
                                         state->player.collision_width ( ), state->player.collision_height ( ),
                                         pickup.position.x ( ), pickup.position.y ( ),
                                         Pickup::c_dimension_in_meters, Pickup::c_dimension_in_meters ) ) {

               LOG_DEBUG ( "Player got pickup %s\n", Pickup::c_names [ pickup.type ] );

               switch ( pickup.type ) {
               default:
                    break;
               case Pickup::Type::health:
                    state->player.health += 5;

                    if ( state->player.health > state->player.max_health ) {
                         state->player.health = state->player.max_health;
                    }
                    break;
               case Pickup::Type::key:
                    state->player_key_count++;
                    break;
               }

               pickup.type = Pickup::Type::none;
          }
     }

     auto& map         = state->map;

     Vector player_center { state->player.collision_center_x ( ),
                            state->player.collision_center_y ( ) };

     Int32 player_center_tile_x = meters_to_pixels ( player_center.x ( ) ) / Map::c_tile_dimension_in_pixels;
     Int32 player_center_tile_y = meters_to_pixels ( player_center.y ( ) ) / Map::c_tile_dimension_in_pixels;

     if ( player_center_tile_x >= 0 && player_center_tile_x < state->interactives.width ( ) &&
          player_center_tile_y >= 0 && player_center_tile_y < state->interactives.height ( ) ) {

          auto& interactive = state->interactives.get_from_tile ( player_center_tile_x, player_center_tile_y );

          if ( interactive.type == Interactive::Type::exit &&
               interactive.interactive_exit.state == Exit::State::open &&
               interactive.interactive_exit.direction == opposite_direction ( state->player.facing ) ) {
               Vector new_position ( pixels_to_meters ( interactive.interactive_exit.exit_index_x *
                                                        Map::c_tile_dimension_in_pixels ),
                                     pixels_to_meters ( interactive.interactive_exit.exit_index_y *
                                                        Map::c_tile_dimension_in_pixels ) );

               new_position += Vector ( Map::c_tile_dimension_in_meters * 0.5f,
                                        Map::c_tile_dimension_in_meters * 0.5f );

               map.load_from_master_list ( interactive.interactive_exit.map_index, state->interactives );

               state->clear_pickups ( );
               state->clear_arrows ( );
               state->clear_enemies ( );
               state->spawn_map_enemies ( );

               state->player.set_collision_center ( new_position.x ( ), new_position.y ( ) );

               LOG_DEBUG ( "Teleporting player to %f %f on new map\n",
                           state->player.position.x ( ),
                           state->player.position.y ( ) );
          }
     }

     state->map.reset_light ( );
     state->interactives.contribute_light ( map );

     // give interactives their light values
     for ( Int32 y = 0; y < state->interactives.height ( ); ++y ) {
          for ( Int32 x = 0; x < state->interactives.width ( ); ++x ) {
               state->interactives.light ( x, y, map.get_coordinate_light ( x, y ) );
          }
     }
}

static Void render_pickups ( SDL_Surface* back_buffer, SDL_Surface* pickup_sheet, Pickup* pickups,
                             Real32 camera_x, Real32 camera_y )
{
     for ( Uint32 i = 0; i < State::c_max_pickups; ++i ) {
          Pickup& pickup = pickups [ i ];

          if ( pickup.type == Pickup::Type::none ||
               pickup.type == Pickup::Type::ingredient ) {
               continue;
          }

          SDL_Rect dest_rect = build_world_sdl_rect ( pickup.position.x ( ), pickup.position.y ( ),
                                                      Pickup::c_dimension_in_meters,
                                                      Pickup::c_dimension_in_meters );

          SDL_Rect clip_rect { ( static_cast<Int32>( pickup.type ) - 1) * Pickup::c_dimension_in_pixels, 0,
                               Pickup::c_dimension_in_pixels, Pickup::c_dimension_in_pixels };

          world_to_sdl ( dest_rect, back_buffer, camera_x, camera_y );

          SDL_BlitSurface ( pickup_sheet, &clip_rect, back_buffer, &dest_rect );
     }
}

static Void render_arrow ( SDL_Surface* back_buffer, SDL_Surface* arrow_sheet, const Arrow& arrow,
                           Real32 camera_x, Real32 camera_y )
{

     SDL_Rect dest_rect = build_world_sdl_rect ( arrow.position.x ( ), arrow.position.y ( ),
                                                 Map::c_tile_dimension_in_meters,
                                                 Map::c_tile_dimension_in_meters );

     SDL_Rect clip_rect { 0, static_cast<Int32>( arrow.facing ) * Map::c_tile_dimension_in_pixels,
                          Map::c_tile_dimension_in_pixels, Map::c_tile_dimension_in_pixels };

     world_to_sdl ( dest_rect, back_buffer, camera_x, camera_y );

     SDL_BlitSurface ( arrow_sheet, &clip_rect, back_buffer, &dest_rect );
}


extern "C" Void game_render ( GameMemory& game_memory, SDL_Surface* back_buffer )
{
     auto* state = get_state ( game_memory );
     Uint32 red    = SDL_MapRGB ( back_buffer->format, 255, 0, 0 );
     Uint32 white  = SDL_MapRGB ( back_buffer->format, 255, 255, 255 );
     Uint32 black  = SDL_MapRGB ( back_buffer->format, 0, 0, 0 );

     // calculate camera
     state->camera.set_x ( calculate_camera_position ( back_buffer->w, state->map.width ( ),
                                                       state->player.position.x ( ), state->player.width ( ) ) );
     state->camera.set_y ( calculate_camera_position ( back_buffer->h - 16, state->map.height ( ),
                                                       state->player.position.y ( ), state->player.height ( ) ) );

     // map
     state->map_display.render ( back_buffer, state->map, state->camera.x ( ), state->camera.y ( ) );

     // interactives
     state->interactives_display.render ( back_buffer, state->interactives,
                                          state->camera.x ( ), state->camera.y ( ) );

     // enemies
     for ( Uint32 i = 0; i < state->enemy_count; ++i ) {
          if ( state->enemies [ i ].state == Character::State::dead ) {
               continue;
          }

          state->character_display.render_enemy ( back_buffer, state->enemies [ i ],
                                                  state->camera.x ( ), state->camera.y ( ) );
     }

     // player
     state->character_display.render_player ( back_buffer, state->player,
                                              state->camera.x ( ), state->camera.y ( ) );

     // pickups
     render_pickups ( back_buffer, state->pickup_sheet, state->pickups, state->camera.x ( ), state->camera.y ( ) );

     // arrows
     for ( Uint32 i = 0; i < State::c_max_arrows; ++i ) {
          auto& arrow = state->arrows [ i ];

          if ( arrow.state == Arrow::State::dead ) {
               continue;
          }

          render_arrow ( back_buffer, state->arrow_sheet, arrow, state->camera.x ( ), state->camera.y ( ) );
     }

     // light
     render_light ( back_buffer, state->map, state->camera.x ( ), state->camera.y ( ) );

     // ui
     SDL_Rect hud_rect { 0, 0, back_buffer->w, 16 };
     SDL_FillRect ( back_buffer, &hud_rect, black );

     // draw player health bar
     Real32 pct = static_cast<Real32>( state->player.health ) /
                  static_cast<Real32>( state->player.max_health );

     Int32 bar_len = static_cast<Int32>( 50.0f * pct );

     SDL_Rect health_bar_rect { 3, 3, bar_len, 10 };
     SDL_Rect health_bar_border_rect { 2, 2, 52, 12 };

     SDL_FillRect ( back_buffer, &health_bar_border_rect, white );
     SDL_FillRect ( back_buffer, &health_bar_rect, red );

     char buffer [ 64 ];

#ifdef LINUX
     sprintf ( buffer, "%d", state->player_key_count );
#endif

#ifdef WIN32
     sprintf_s ( buffer, "%d", state->player_key_count );
#endif
     state->text.render ( back_buffer, buffer, 235, 4 );

     SDL_Rect pickup_dest_rect { 225, 3, Pickup::c_dimension_in_pixels, Pickup::c_dimension_in_pixels };
     SDL_Rect pickup_clip_rect { Pickup::c_dimension_in_pixels, 0, Pickup::c_dimension_in_pixels, Pickup::c_dimension_in_pixels };

     SDL_BlitSurface ( state->pickup_sheet, &pickup_clip_rect, back_buffer, &pickup_dest_rect );
}

