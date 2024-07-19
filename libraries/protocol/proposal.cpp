// AcloudBank
#include <graphene/protocol/operations.hpp>
#include <graphene/protocol/fee_schedule.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

proposal_create_operation proposal_create_operation::committee_proposal(const chain_parameters& global_params, fc::time_point_sec head_block_time )
{
   // TODO move this method to unit tests as it is not useful
   proposal_create_operation op;
   op.expiration_time = head_block_time + global_params.maximum_proposal_lifetime;
   op.review_period_seconds = global_params.committee_proposal_review_period;
   return op;
}

void proposal_create_operation::validate() const
{
   FC_ASSERT( !proposed_ops.empty() );
   for( const auto& op : proposed_ops ) operation_validate( op.op );
}

share_type proposal_create_operation::calculate_fee(const fee_params_t& k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

void proposal_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   FC_ASSERT(!(active_approvals_to_add.empty() && active_approvals_to_remove.empty() &&
               owner_approvals_to_add.empty() && owner_approvals_to_remove.empty() &&
               key_approvals_to_add.empty() && key_approvals_to_remove.empty()));
   for( auto a : active_approvals_to_add )
   {
      FC_ASSERT(active_approvals_to_remove.find(a) == active_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
   }
   for( auto a : owner_approvals_to_add )
   {
      FC_ASSERT(owner_approvals_to_remove.find(a) == owner_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
   }
   for( auto a : key_approvals_to_add )
   {
      FC_ASSERT(key_approvals_to_remove.find(a) == key_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
   }
}

void proposal_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type proposal_update_operation::calculate_fee(const fee_params_t& k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

void proposal_update_operation::get_required_authorities( vector<authority>& o )const
{
   authority auth;
   for( const auto& k : key_approvals_to_add )
      auth.key_auths[k] = 1;
   for( const auto& k : key_approvals_to_remove )
      auth.key_auths[k] = 1;
   auth.weight_threshold = auth.key_auths.size();

   if( auth.weight_threshold > 0 )
      o.emplace_back( std::move(auth) );
}

void proposal_update_operation::get_required_active_authorities( flat_set<account_id_type>& a )const
{
   for( const auto& i : active_approvals_to_add )    a.insert(i);
   for( const auto& i : active_approvals_to_remove ) a.insert(i);
}

void proposal_update_operation::get_required_owner_authorities( flat_set<account_id_type>& a )const
{
   for( const auto& i : owner_approvals_to_add )    a.insert(i);
   for( const auto& i : owner_approvals_to_remove ) a.insert(i);
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_create_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_update_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_delete_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::proposal_delete_operation )
