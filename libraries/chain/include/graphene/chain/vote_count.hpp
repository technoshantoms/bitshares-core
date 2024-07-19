

#pragma once

#include <graphene/protocol/authority.hpp>

namespace graphene { namespace chain {

/**
 * Keep track of vote totals in internal authority object.  See #533.
 */
struct vote_counter
{
   template< typename Component >
   void add( Component who, uint64_t votes )
   {
      if( votes == 0 )
         return;
      assert( votes <= last_votes );
      last_votes = votes;
      if( bitshift == -1 )
         bitshift = std::max(int8_t(boost::multiprecision::detail::find_msb( votes )) - 15, 0);
      uint64_t scaled_votes = std::max( votes >> (uint8_t)bitshift, uint64_t(1) );
      assert( scaled_votes <= std::numeric_limits<weight_type>::max() );
      total_votes += scaled_votes;
      assert( total_votes <= std::numeric_limits<uint32_t>::max() );
      auth.add_authority( who, weight_type( scaled_votes ) );
   }

   /**
    * Write into out_auth, but only if we have at least one member.
    */
   void finish( authority& out_auth )
   {
      if( total_votes == 0 )
         return;
      assert( total_votes <= std::numeric_limits<uint32_t>::max() );
      uint32_t weight = uint32_t( total_votes );
      weight = (weight >> 1)+1;
      auth.weight_threshold = weight;
      out_auth = auth;
   }

   bool is_empty()const
   {
      return (total_votes == 0);
   }

   uint64_t last_votes = std::numeric_limits<uint64_t>::max();
   uint64_t total_votes = 0;
   int8_t bitshift = -1;
   authority auth;
};

} } // graphene::chain
