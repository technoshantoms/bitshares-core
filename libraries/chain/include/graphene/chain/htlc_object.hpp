/*
 * Acloudbank
 */

#pragma once

#include <graphene/protocol/htlc.hpp>
#include <graphene/db/generic_index.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
   using namespace protocol;

   /**
    * @brief database object to store HTLCs
    *
    * This object is stored in the database while an HTLC is active. The HTLC will
    * become inactive at expiration or when unlocked via the preimage.
    */
   class htlc_object : public graphene::db::abstract_object<htlc_object, protocol_ids, htlc_object_type>
   {
   public:
      struct transfer_info
      {
         account_id_type from;
         account_id_type to;
         share_type amount;
         asset_id_type asset_id;
      };
      struct condition_info
      {
         struct hash_lock_info
         {
            htlc_hash preimage_hash;
            uint16_t preimage_size;
         };
         struct time_lock_info
         {
            fc::time_point_sec expiration;
         };
         hash_lock_info hash_lock;
         time_lock_info time_lock;
      };

      transfer_info transfer;
      condition_info conditions;
      fc::optional<memo_data> memo;

      /****
       * Index helper for timelock
       */
      struct timelock_extractor {
         using result_type = fc::time_point_sec;
         const result_type& operator()(const htlc_object& o)const { return o.conditions.time_lock.expiration; }
      };

      /*****
       * Index helper for from
       */
      struct from_extractor {
         using result_type = account_id_type;
         const result_type& operator()(const htlc_object& o)const { return o.transfer.from; }
      };

      /*****
       * Index helper for to
       */
      struct to_extractor {
         using result_type = account_id_type;
         const result_type& operator()(const htlc_object& o)const { return o.transfer.to; }
      };
   };

   struct by_from_id;
   struct by_expiration;
   struct by_to_id;
   using htlc_object_multi_index_type = multi_index_container<
         htlc_object,
         indexed_by<
            ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
            ordered_unique< tag< by_expiration >,
               composite_key< htlc_object,
                  htlc_object::timelock_extractor,
                  member< object, object_id_type, &object::id > > >,
            ordered_unique< tag< by_from_id >,
               composite_key< htlc_object,
                  htlc_object::from_extractor,
                  member< object, object_id_type, &object::id > > >,
            ordered_unique< tag< by_to_id >,
               composite_key< htlc_object,
                  htlc_object::to_extractor,
                  member< object, object_id_type, &object::id > > >
         >
   >;

   using htlc_index = generic_index< htlc_object, htlc_object_multi_index_type >;

} } // namespace graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::htlc_object)

FC_REFLECT_TYPENAME( graphene::chain::htlc_object::condition_info::hash_lock_info )
FC_REFLECT_TYPENAME( graphene::chain::htlc_object::condition_info::time_lock_info )
FC_REFLECT_TYPENAME( graphene::chain::htlc_object::condition_info )
FC_REFLECT_TYPENAME( graphene::chain::htlc_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::htlc_object )
