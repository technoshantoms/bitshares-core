/*
 * AcloudBank
 */
#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol {

   /**
    * Creates a take profit limit order
    */
   struct create_take_profit_order_action
   {
      /// Asset ID that will be used to pay operation fee for placing the take profit order
      asset_id_type   fee_asset_id;
      /// A percentage indicating how far the price of the take profit order differs from the original order
      uint16_t        spread_percent = 0;
      /// A percentage indicating how much amount to sell in the take profit order
      uint16_t        size_percent = GRAPHENE_100_PERCENT;
      /// How long the take profit order to be kept on the market
      uint32_t        expiration_seconds = 0;
      /// Whether to create another take profit order for this take profit order if this take profit order is matched
      bool            repeat = false;

      extensions_type extensions;  ///< Unused. Reserved for future use.

      void            validate()const;
   };

   /// Automatic actions for limit orders
   using limit_order_auto_action = static_variant< create_take_profit_order_action >;

   /**
    *  @class limit_order_create_operation
    *  @brief instructs the blockchain to attempt to sell one asset for another
    *  @ingroup operations
    *
    *  The blockchain will atempt to sell amount_to_sell.asset_id for as
    *  much min_to_receive.asset_id as possible.  The fee will be paid by
    *  the seller's account.  Market fees will apply as specified by the
    *  issuer of both the selling asset and the receiving asset as
    *  a percentage of the amount exchanged.
    *
    *  If either the selling asset or the receiving asset is white list
    *  restricted, the order will only be created if the seller is on
    *  the white list of the restricted asset type.
    *
    *  Market orders are matched in the order they are included
    *  in the block chain.
    */
   struct limit_order_create_operation : public base_operation
   {
      /**
       * Options to be used in @ref limit_order_create_operation
       *
       * @note this struct can be expanded by adding more options in the end.
       */
      struct options_type
      {
         /// Automatic actions when the limit order is filled or partially filled
         optional< vector< limit_order_auto_action > > on_fill;
      };

      struct fee_params_t { uint64_t fee = 5 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset           fee;
      account_id_type seller;
      asset           amount_to_sell;
      asset           min_to_receive;

      /// The order will be removed from the books if not filled by expiration
      /// Upon expiration, all unsold asset will be returned to seller
      time_point_sec expiration = time_point_sec::maximum();

      /// If this flag is set the entire order must be filled or the operation is rejected
      bool fill_or_kill = false;

      using extensions_type = extension<options_type>; // note: will be jsonified to {...} but not [...]
      extensions_type   extensions;

      pair<asset_id_type,asset_id_type> get_market()const
      {
         return amount_to_sell.asset_id < min_to_receive.asset_id ?
                std::make_pair(amount_to_sell.asset_id, min_to_receive.asset_id) :
                std::make_pair(min_to_receive.asset_id, amount_to_sell.asset_id);
      }
      account_id_type fee_payer()const { return seller; }
      void            validate()const;
      price           get_price()const { return amount_to_sell / min_to_receive; }
   };

   /**
    * @ingroup operations
    * Used to update an existing limit order.
    */
   struct limit_order_update_operation : public base_operation
   {
       struct fee_params_t {
           uint64_t fee = ( GRAPHENE_BLOCKCHAIN_PRECISION * 3 ) / 8;
       };

       asset fee;
       account_id_type seller;
       limit_order_id_type order;
       optional<price> new_price;
       optional<asset> delta_amount_to_sell;
       optional<time_point_sec> new_expiration;
       /// Automatic actions when the limit order is filled or partially filled
       optional< vector< limit_order_auto_action > > on_fill;

       extensions_type extensions;

       account_id_type fee_payer() const { return seller; }
       void validate() const override;
   };

   /**
    *  @ingroup operations
    *  Used to cancel an existing limit order. Both fee_pay_account and the
    *  account to receive the proceeds must be the same as order->seller.
    *
    *  @return the amount actually refunded
    */
   struct limit_order_cancel_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 0; };

      asset               fee;
      limit_order_id_type order;
      /** must be order->seller */
      account_id_type     fee_paying_account;
      extensions_type   extensions;

      account_id_type fee_payer()const { return fee_paying_account; }
      void            validate()const;
   };

   /**
    *  @ingroup operations
    *
    *  This operation can be used to add collateral, cover, and adjust the margin call price for a particular user.
    *
    *  For prediction markets the collateral and debt must always be equal.
    *
    *  This operation will fail if it would trigger a margin call that couldn't be filled.  If the margin call hits
    *  the call price limit then it will fail if the call price is above the settlement price.
    *
    *  @note this operation can be used to force a market order using the collateral without requiring outside funds.
    */
   struct call_order_update_operation : public base_operation
   {
      /**
       * Options to be used in @ref call_order_update_operation.
       *
       * @note this struct can be expanded by adding more options in the end.
       */
      struct options_type
      {
         /// Maximum CR to maintain when selling collateral on margin call
         optional<uint16_t> target_collateral_ratio;
      };

      /** this is slightly more expensive than limit orders, this pricing impacts prediction markets */
      struct fee_params_t { uint64_t fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset               fee;
      account_id_type     funding_account; ///< pays fee, collateral, and cover
      asset               delta_collateral; ///< the amount of collateral to add to the margin position
      asset               delta_debt; ///< the amount of the debt to be paid off, may be negative to issue new debt

      using extensions_type = extension<options_type>; // note: this will be jsonified to {...} but no longer [...]
      extensions_type     extensions;

      account_id_type fee_payer()const { return funding_account; }
      void            validate()const;
   };

   /**
    * @ingroup operations
    *
    * @note This is a virtual operation that is created while matching orders and
    * emitted for the purpose of accurately tracking account history, accelerating
    * a reindex.
    */
   struct fill_order_operation : public base_operation
   {
      struct fee_params_t {};

      fill_order_operation(){}
      fill_order_operation( object_id_type o, account_id_type a, asset p, asset r, asset f, price fp, bool m )
         :order_id(o),account_id(a),pays(p),receives(r),fee(f),fill_price(fp),is_maker(m) {}

      object_id_type      order_id;
      account_id_type     account_id;
      asset               pays;
      asset               receives;
      asset               fee; // paid by receiving account
      price               fill_price;
      bool                is_maker = true;

      pair<asset_id_type,asset_id_type> get_market()const
      {
         return pays.asset_id < receives.asset_id ?
                std::make_pair( pays.asset_id, receives.asset_id ) :
                std::make_pair( receives.asset_id, pays.asset_id );
      }
      account_id_type fee_payer()const { return account_id; }
      void            validate()const { FC_ASSERT( !"virtual operation" ); }

      /// This is a virtual operation; there is no fee
      share_type      calculate_fee(const fee_params_t&)const { return 0; }
   };

   /**
    *  @ingroup operations
    *
    *  This operation can be used after a global settlement to bid collateral for
    *  taking over part of the debt and the settlement_fund (see BSIP-0018).
    */
   struct bid_collateral_operation : public base_operation
   {
      /** should be equivalent to call_order_update fee */
      struct fee_params_t { uint64_t fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset               fee;
      account_id_type     bidder; ///< pays fee and additional collateral
      asset               additional_collateral; ///< the amount of collateral to bid for the debt
      asset               debt_covered; ///< the amount of debt to take over
      extensions_type     extensions;

      account_id_type fee_payer()const { return bidder; }
      void            validate()const;
   };

   /**
    * @ingroup operations
    *
    * @note This is a virtual operation that is created while reviving a
    * bitasset from collateral bids.
    */
   struct execute_bid_operation : public base_operation
   {
      struct fee_params_t {};

      execute_bid_operation(){}
      execute_bid_operation( account_id_type a, asset d, asset c )
         : bidder(a), debt(d), collateral(c) {}

      account_id_type     bidder;
      asset               debt;
      asset               collateral;
      asset               fee;

      account_id_type fee_payer()const { return bidder; }
      void            validate()const { FC_ASSERT( !"virtual operation" ); }

      /// This is a virtual operation; there is no fee
      share_type      calculate_fee(const fee_params_t&)const { return 0; }
   };
} } // graphene::protocol

FC_REFLECT( graphene::protocol::create_take_profit_order_action,
            (fee_asset_id)(spread_percent)(size_percent)(expiration_seconds)(repeat)(extensions) )

FC_REFLECT( graphene::protocol::limit_order_create_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::limit_order_update_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::limit_order_cancel_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::call_order_update_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::bid_collateral_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::fill_order_operation::fee_params_t,  ) // VIRTUAL
FC_REFLECT( graphene::protocol::execute_bid_operation::fee_params_t,  ) // VIRTUAL

FC_REFLECT( graphene::protocol::limit_order_create_operation::options_type, (on_fill) )
FC_REFLECT( graphene::protocol::call_order_update_operation::options_type, (target_collateral_ratio) )

FC_REFLECT( graphene::protocol::limit_order_create_operation,
            (fee)(seller)(amount_to_sell)(min_to_receive)(expiration)(fill_or_kill)(extensions))
FC_REFLECT( graphene::protocol::limit_order_update_operation,
            (fee)(seller)(order)(new_price)(delta_amount_to_sell)(new_expiration)(on_fill)(extensions))
FC_REFLECT( graphene::protocol::limit_order_cancel_operation,
            (fee)(fee_paying_account)(order)(extensions) )
FC_REFLECT( graphene::protocol::call_order_update_operation,
            (fee)(funding_account)(delta_collateral)(delta_debt)(extensions) )
FC_REFLECT( graphene::protocol::fill_order_operation,
            (fee)(order_id)(account_id)(pays)(receives)(fill_price)(is_maker) )
FC_REFLECT( graphene::protocol::bid_collateral_operation,
            (fee)(bidder)(additional_collateral)(debt_covered)(extensions) )
FC_REFLECT( graphene::protocol::execute_bid_operation,
            (fee)(bidder)(debt)(collateral) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::create_take_profit_order_action)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_create_operation::options_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::call_order_update_operation::options_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_create_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_update_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_cancel_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::call_order_update_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::bid_collateral_operation::fee_params_t )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_order_cancel_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::call_order_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::bid_collateral_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fill_order_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::execute_bid_operation )
