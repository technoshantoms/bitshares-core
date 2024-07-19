/*
 * Acloudbank
 */
#pragma once

#include <graphene/chain/vesting_balance_object.hpp>

namespace graphene { namespace chain {

   class balance_object : public abstract_object<balance_object, protocol_ids, balance_object_type>
   {
      public:
         bool is_vesting_balance()const
         { return vesting_policy.valid(); }
         asset available(fc::time_point_sec now)const
         {
            return is_vesting_balance()? vesting_policy->get_allowed_withdraw({balance, now, {}})
                                       : balance;
         }

         address owner;
         asset   balance;
         optional<linear_vesting_policy> vesting_policy;
         time_point_sec last_claim_date;
         asset_id_type asset_type()const { return balance.asset_id; }
   };

   struct by_owner;

   /**
    * @ingroup object_index
    */
   using balance_multi_index_type = multi_index_container<
      balance_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_owner>, composite_key<
            balance_object,
            member<balance_object, address, &balance_object::owner>,
            const_mem_fun<balance_object, asset_id_type, &balance_object::asset_type>
         > >
      >
   >;

   /**
    * @ingroup object_index
    */
   using balance_index = generic_index<balance_object, balance_multi_index_type>;
} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::balance_object)

FC_REFLECT_TYPENAME( graphene::chain::balance_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::balance_object )
