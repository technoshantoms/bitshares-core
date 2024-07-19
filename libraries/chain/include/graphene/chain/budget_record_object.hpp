/*
 * Acloudbank
 */

#pragma once
#include <graphene/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

struct budget_record
{
   uint64_t time_since_last_budget = 0;

   /// Sources of budget
   ///@{
   share_type from_initial_reserve = 0;
   share_type from_accumulated_fees = 0;
   share_type from_unused_witness_budget = 0;
   ///@}

   /// Witness budget requested by the committee
   share_type requested_witness_budget = 0;

   /// Funds that can be released from reserve at maximum rate
   share_type total_budget = 0;

   /// Sinks of budget, should sum up to total_budget
   ///@{
   share_type witness_budget = 0;
   share_type worker_budget = 0;
   ///@}

   /// Unused budget
   share_type leftover_worker_funds = 0;

   /// Change in supply due to budget operations
   share_type supply_delta = 0;

   /// Maximum supply
   share_type max_supply;

   /// Current supply
   share_type current_supply;
};

class budget_record_object : public graphene::db::abstract_object<budget_record_object,
                                       implementation_ids, impl_budget_record_object_type>
{
   public:
      fc::time_point_sec time;
      budget_record record;
};

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::budget_record_object)

FC_REFLECT_TYPENAME( graphene::chain::budget_record )
FC_REFLECT_TYPENAME( graphene::chain::budget_record_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::budget_record )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::budget_record_object )
