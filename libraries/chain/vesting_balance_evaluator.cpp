

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/vesting_balance_evaluator.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result vesting_balance_create_evaluator::do_evaluate( const vesting_balance_create_operation& op )
{ try {
   const database& d = db();

   const account_object& creator_account = *fee_paying_account;
   const account_object& owner_account = op.owner( d );

   const asset_object& asset_obj = op.amount.asset_id( d );

   FC_ASSERT( !asset_obj.is_transfer_restricted(), "Asset has transfer_restricted flag enabled" );

   // Since hard fork core-973, check asset authorization limitations
   if( HARDFORK_CORE_973_PASSED(d.head_block_time()) )
   {
      FC_ASSERT( is_authorized_asset( d, creator_account, asset_obj ),
                 "The creator account is not allowed to transact the asset" );
      FC_ASSERT( is_authorized_asset( d, owner_account, asset_obj ),
                 "The owner account is not allowed to transact the asset" );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

struct init_policy_visitor
{
   typedef void result_type;

   init_policy_visitor( vesting_policy& po,
                        const share_type& begin_balance,
                        const fc::time_point_sec& n ):p(po),init_balance(begin_balance),now(n){}

   vesting_policy&    p;
   share_type         init_balance;
   fc::time_point_sec now;

   void operator()( const linear_vesting_policy_initializer& i )const
   {
      linear_vesting_policy policy;
      policy.begin_timestamp = i.begin_timestamp;
      policy.vesting_cliff_seconds = i.vesting_cliff_seconds;
      policy.vesting_duration_seconds = i.vesting_duration_seconds;
      policy.begin_balance = init_balance;
      p = policy;
   }

   void operator()( const cdd_vesting_policy_initializer& i )const
   {
      cdd_vesting_policy policy;
      policy.vesting_seconds = i.vesting_seconds;
      policy.start_claim = i.start_claim;
      policy.coin_seconds_earned = 0;
      policy.coin_seconds_earned_last_update = now;
      p = policy;
   }

   void operator()( const instant_vesting_policy_initializer& i )const
   {
      p = instant_vesting_policy{};
   }

};

object_id_type vesting_balance_create_evaluator::do_apply( const vesting_balance_create_operation& op )
{ try {
   database& d = db();
   const time_point_sec now = d.head_block_time();

   d.adjust_balance( op.creator, -op.amount );

   const vesting_balance_object& vbo = d.create< vesting_balance_object >( [&]( vesting_balance_object& obj )
   {
      //WARNING: The logic to create a vesting balance object is replicated in vesting_balance_worker_type::initializer::init.
      // If making changes to this logic, check if those changes should also be made there as well.
      obj.owner = op.owner;
      obj.balance = op.amount;
      op.policy.visit( init_policy_visitor( obj.policy, op.amount.amount, now ) );
   } );


   return vbo.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result vesting_balance_withdraw_evaluator::do_evaluate( const vesting_balance_withdraw_operation& op )
{ try {
   const database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );
   FC_ASSERT( op.owner == vbo.owner, "", ("op.owner", op.owner)("vbo.owner", vbo.owner) );
   FC_ASSERT( vbo.is_withdraw_allowed( now, op.amount ), "", ("now", now)("op", op)("vbo", vbo) );
   assert( op.amount <= vbo.balance );      // is_withdraw_allowed should fail before this check is reached

   // Note: asset authorization check is skipped here,
   //       because it is fine to allow the funds to be moved to account balance
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result vesting_balance_withdraw_evaluator::do_apply( const vesting_balance_withdraw_operation& op )
{ try {
   database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );

   // Allow zero balance objects to stick around, (1) to comply
   // with the chain's "objects live forever" design principle, (2)
   // if it's cashback or worker, it'll be filled up again.

   d.modify( vbo, [&]( vesting_balance_object& vbo )
   {
      vbo.withdraw( now, op.amount );
   } );

   d.adjust_balance( op.owner, op.amount );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
