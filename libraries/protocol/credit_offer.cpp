/*
 * Acloudbank
 */

#include <graphene/protocol/credit_offer.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

static void validate_acceptable_collateral( const flat_map<asset_id_type, price>& acceptable_collateral,
                                            const asset_id_type* p_asset_type = nullptr )
{
   FC_ASSERT( !acceptable_collateral.empty(), "Acceptable collateral list should not be empty" );

   asset_id_type asset_type = ( p_asset_type != nullptr ) ? *p_asset_type
                              : acceptable_collateral.begin()->second.base.asset_id;

   for( const auto& collateral : acceptable_collateral )
   {
      const auto& collateral_asset_type = collateral.first;
      const auto& collateral_price = collateral.second;
      FC_ASSERT( collateral_price.base.asset_id == asset_type,
                 "Base asset ID in price of acceptable collateral should be same as offer asset type" );
      FC_ASSERT( collateral_price.quote.asset_id == collateral_asset_type,
                 "Quote asset ID in price of acceptable collateral should be same as collateral asset type" );
      collateral_price.validate( true );
   }
}

static void validate_acceptable_borrowers( const flat_map<account_id_type, share_type>& acceptable_borrowers )
{
   for( const auto& borrower : acceptable_borrowers )
   {
      const auto& max_borrow_amount = borrower.second.value;
      FC_ASSERT( max_borrow_amount >= 0,
                 "Maximum amount to borrow for acceptable borrowers should not be negative" );
      FC_ASSERT( max_borrow_amount <= GRAPHENE_MAX_SHARE_SUPPLY,
                 "Maximum amount to borrow for acceptable borrowers should not be greater than ${max}",
                 ("max", GRAPHENE_MAX_SHARE_SUPPLY) );
   }
}

void credit_offer_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );
   FC_ASSERT( balance > 0, "Balance should be positive" );
   FC_ASSERT( max_duration_seconds <= GRAPHENE_MAX_CREDIT_DEAL_SECS,
              "Maximum duration should not be greater than ${d} days",
              ("d", GRAPHENE_MAX_CREDIT_DEAL_DAYS) );
   FC_ASSERT( min_deal_amount >= 0, "Minimum deal amount should not be negative" );
   FC_ASSERT( min_deal_amount <= GRAPHENE_MAX_SHARE_SUPPLY,
              "Minimum deal amount should not be greater than ${max}",
              ("max", GRAPHENE_MAX_SHARE_SUPPLY) );

   validate_acceptable_collateral( acceptable_collateral, &asset_type );
   validate_acceptable_borrowers( acceptable_borrowers );
}

share_type credit_offer_create_operation::calculate_fee( const fee_params_t& schedule )const
{
   share_type core_fee_required = schedule.fee;
   core_fee_required += calculate_data_fee( fc::raw::pack_size(*this), schedule.price_per_kbyte );
   return core_fee_required;
}

void credit_offer_delete_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );
}

void credit_offer_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );
   bool updating_something = false;

   if( delta_amount.valid() )
   {
      updating_something = true;
      FC_ASSERT( delta_amount->amount != 0, "Delta amount should not be zero" );
   }
   if( fee_rate.valid() )
      updating_something = true;
   if( max_duration_seconds.valid() )
   {
      updating_something = true;
      FC_ASSERT( *max_duration_seconds <= GRAPHENE_MAX_CREDIT_DEAL_SECS,
                 "Maximum duration should not be greater than ${d} days",
                 ("d", GRAPHENE_MAX_CREDIT_DEAL_DAYS) );
   }
   if( min_deal_amount.valid() )
   {
      updating_something = true;
      FC_ASSERT( *min_deal_amount >= 0, "Minimum deal amount should not be negative" );
      FC_ASSERT( *min_deal_amount <= GRAPHENE_MAX_SHARE_SUPPLY,
                 "Minimum deal amount should not be greater than ${max}",
                 ("max", GRAPHENE_MAX_SHARE_SUPPLY) );
   }
   if( enabled.valid() )
      updating_something = true;
   if( auto_disable_time.valid() )
      updating_something = true;
   if( acceptable_collateral.valid() )
   {
      updating_something = true;
      validate_acceptable_collateral( *acceptable_collateral ); // Note: check base asset ID in evaluator
   }
   if( acceptable_borrowers.valid() )
   {
      updating_something = true;
      validate_acceptable_borrowers( *acceptable_borrowers );
   }

   FC_ASSERT( updating_something,
              "Should change something - at least one of the optional data fields should be present" );
}

share_type credit_offer_update_operation::calculate_fee( const fee_params_t& schedule )const
{
   share_type core_fee_required = schedule.fee;
   core_fee_required += calculate_data_fee( fc::raw::pack_size(*this), schedule.price_per_kbyte );
   return core_fee_required;
}

void credit_offer_accept_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );
   FC_ASSERT( borrow_amount.amount > 0, "Amount to borrow should be positive" );
   FC_ASSERT( collateral.amount > 0, "Collateral amount should be positive" );
   if( extensions.value.auto_repay.valid() )
   {
      constexpr auto cdar_count = static_cast<uint8_t>( credit_deal_auto_repayment_type::CDAR_TYPE_COUNT );
      FC_ASSERT( *extensions.value.auto_repay < cdar_count,
                 "auto_repay should be less than ${c}", ("c",cdar_count) );
   }
}

void credit_deal_repay_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );
   FC_ASSERT( repay_amount.amount > 0, "Amount to repay should be positive" );
   FC_ASSERT( credit_fee.amount >= 0, "Credit fee should not be negative" );
   FC_ASSERT( repay_amount.asset_id == credit_fee.asset_id,
             "Asset type of repay amount and credit fee should be the same" );
}

void credit_deal_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0, "Fee should not be negative" );

   constexpr auto cdar_count = static_cast<uint8_t>( credit_deal_auto_repayment_type::CDAR_TYPE_COUNT );
   FC_ASSERT( auto_repay < cdar_count,
              "auto_repay should be less than ${c}", ("c",cdar_count) );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_create_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_delete_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_update_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_repay_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_update_operation::fee_params_t )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation::ext )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_delete_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_repay_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_expired_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_update_operation )
