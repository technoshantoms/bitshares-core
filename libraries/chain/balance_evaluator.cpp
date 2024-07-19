/*
 * Acloudbank
 */
#include <graphene/chain/balance_evaluator.hpp>
#include <graphene/protocol/pts_address.hpp>

namespace graphene { namespace chain {

void_result balance_claim_evaluator::do_evaluate(const balance_claim_operation& op)
{
   database& d = db();
   balance = &op.balance_to_claim(d);

   bool is_balance_owner = (
             op.balance_owner_key == balance->owner ||
             pts_address(op.balance_owner_key, false) == balance->owner || // version = 56 (default)
             pts_address(op.balance_owner_key, true) == balance->owner ); // version = 56 (default)
   is_balance_owner = (
             is_balance_owner ||
             pts_address(op.balance_owner_key, false, 0) == balance->owner ||
             pts_address(op.balance_owner_key, true, 0) == balance->owner );
   GRAPHENE_ASSERT( is_balance_owner,
             balance_claim_owner_mismatch,
             "Balance owner key was specified as '${op}' but balance's actual owner is '${bal}'",
             ("op", op.balance_owner_key)
             ("bal", balance->owner)
             );

   FC_ASSERT(op.total_claimed.asset_id == balance->asset_type());

   if( balance->is_vesting_balance() )
   {
      GRAPHENE_ASSERT(
         balance->vesting_policy->is_withdraw_allowed(
            { balance->balance,
              d.head_block_time(),
              op.total_claimed } ),
         balance_claim_invalid_claim_amount,
         "Attempted to claim ${c} from a vesting balance with ${a} available",
         ("c", op.total_claimed)("a", balance->available(d.head_block_time()))
         );
      GRAPHENE_ASSERT(
         d.head_block_time() - balance->last_claim_date >= fc::days(1),
         balance_claim_claimed_too_often,
         "Genesis vesting balances may not be claimed more than once per day."
         );
      return {};
   }

   FC_ASSERT(op.total_claimed == balance->balance);
   return {};
}

/**
 * @note the fee is always 0 for this particular operation because once the
 * balance is claimed it frees up memory and it cannot be used to spam the network
 */
void_result balance_claim_evaluator::do_apply(const balance_claim_operation& op)
{
   database& d = db();

   if( balance->is_vesting_balance() && op.total_claimed < balance->balance )
      d.modify(*balance, [&](balance_object& b) {
         b.vesting_policy->on_withdraw({b.balance, d.head_block_time(), op.total_claimed});
         b.balance -= op.total_claimed;
         b.last_claim_date = d.head_block_time();
      });
   else
      d.remove(*balance);

   d.adjust_balance(op.deposit_to_account, op.total_claimed);
   return {};
}
} } // namespace graphene::chain
