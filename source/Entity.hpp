#ifndef BRYTE_ENTITY_HPP
#define BRYTE_ENTITY_HPP

#include "Vector.hpp"

namespace bryte
{
     struct Entity {
          enum LifeState {
               dead,
               dying,
               spawning,
               alive
          };

          inline Bool is_dead ( ) const;
          inline Bool is_alive ( ) const;

          Vector    position;
          LifeState life_state;
          Bool      on_fire;
     };

     inline Bool Entity::is_dead ( ) const
     {
          return life_state == LifeState::dead ||
                 life_state == LifeState::dying;
     }

     inline Bool Entity::is_alive ( ) const
     {
          return life_state != LifeState::dead &&
                 life_state != LifeState::dying;
     }

     struct TrackEntity {
          Entity* entity;
          Vector offset;
     };
}

#endif
