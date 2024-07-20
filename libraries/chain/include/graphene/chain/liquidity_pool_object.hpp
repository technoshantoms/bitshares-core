/*
 * Acloudbank
 */

#pragma once

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>

#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/liquidity_pool.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

using namespace graphene::db;

/**
 *  @brief A liquidity pool
 *  @ingroup object
 *  @ingroup protocol
 *
 */
class liquidity_pool_object : public abstract_object<liquidity_pool_object, protocol_ids, liquidity_pool_object_type>
{
   public:
      asset_id_type   asset_a;                     ///< Type of the first asset in the pool
      asset_id_type   asset_b;                     ///< Type of the second asset in the pool
      share_type      balance_a;                   ///< The balance of the first asset in the pool
      share_type      balance_b;                   ///< The balance of the second asset in the pool
      asset_id_type   share_asset;                 ///< Type of the share asset aka the LP token
      uint16_t        taker_fee_percent = 0;       ///< Taker fee percent
      uint16_t        withdrawal_fee_percent = 0;  ///< Withdrawal fee percent
      fc::uint128_t   virtual_value = 0;           ///< Virtual value of the pool

      void update_virtual_value()
      {
         virtual_value = fc::uint128_t( balance_a.value ) * balance_b.value;
      }
};

struct by_share_asset;
struct by_asset_a;
struct by_asset_b;
struct by_asset_ab;

/**
* @ingroup object_index
*/
typedef multi_index_container<
   liquidity_pool_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_share_asset>,
                      member< liquidity_pool_object, asset_id_type, &liquidity_pool_object::share_asset > >,
      ordered_unique< tag<by_asset_a>,
         composite_key< liquidity_pool_object,
            member< liquidity_pool_object, asset_id_type, &liquidity_pool_object::asset_a >,
            member< object, object_id_type, &object::id>
         >
      >,
      ordered_unique< tag<by_asset_b>,
         composite_key< liquidity_pool_object,
            member< liquidity_pool_object, asset_id_type, &liquidity_pool_object::asset_b >,
            member< object, object_id_type, &object::id>
         >
      >,
      ordered_unique< tag<by_asset_ab>,
         composite_key< liquidity_pool_object,
            member< liquidity_pool_object, asset_id_type, &liquidity_pool_object::asset_a >,
            member< liquidity_pool_object, asset_id_type, &liquidity_pool_object::asset_b >,
            member< object, object_id_type, &object::id>
         >
      >
   >
> liquidity_pool_multi_index_type;

/**
* @ingroup object_index
*/
typedef generic_index<liquidity_pool_object, liquidity_pool_multi_index_type> liquidity_pool_index;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE( graphene::chain::liquidity_pool_object )

// Note: this is left here but not moved to a cpp file due to the extended_liquidity_pool_object struct in API.
FC_REFLECT_DERIVED( graphene::chain::liquidity_pool_object, (graphene::db::object),
                    (asset_a)
                    (asset_b)
                    (balance_a)
                    (balance_b)
                    (share_asset)
                    (taker_fee_percent)
                    (withdrawal_fee_percent)
                    (virtual_value)
                  )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::liquidity_pool_object )
