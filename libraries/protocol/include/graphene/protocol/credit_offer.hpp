/*
 * Acloudbank
 */

#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol {

   /**
    * @brief Create a new credit offer
    * @ingroup operations
    *
    * A credit offer is a fund that can be used by other accounts who provide certain collateral.
    */
   struct credit_offer_create_operation : public base_operation
   {
      struct fee_params_t {
         uint64_t fee             = 1 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 1 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset           fee;                   ///< Operation fee
      account_id_type owner_account;         ///< Owner of the credit offer
      asset_id_type   asset_type;            ///< Asset type in the credit offer
      share_type      balance;               ///< Usable amount in the credit offer
      uint32_t        fee_rate = 0;          ///< Fee rate, the demominator is GRAPHENE_FEE_RATE_DENOM
      uint32_t        max_duration_seconds = 0; ///< The time limit that borrowed funds should be repaid
      share_type      min_deal_amount;          ///< Minimum amount to borrow for each new deal
      bool            enabled = false;          ///< Whether this offer is available
      time_point_sec  auto_disable_time;        ///< The time when this offer will be disabled automatically

      /// Types and rates of acceptable collateral
      flat_map<asset_id_type, price>          acceptable_collateral;

      /// Allowed borrowers and their maximum amounts to borrow. No limitation if empty.
      flat_map<account_id_type, share_type>   acceptable_borrowers;

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return owner_account; }
      void            validate()const override;
      share_type      calculate_fee(const fee_params_t& k)const;
   };

   /**
    * @brief Delete a credit offer
    * @ingroup operations
    */
   struct credit_offer_delete_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 0; };

      asset                fee;                ///< Operation fee
      account_id_type      owner_account;      ///< The account who owns the credit offer
      credit_offer_id_type offer_id;           ///< ID of the credit offer

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return owner_account; }
      void            validate()const override;
   };

   /**
    * @brief Update a credit offer
    * @ingroup operations
    */
   struct credit_offer_update_operation : public base_operation
   {
      struct fee_params_t {
         uint64_t fee             = 1 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 1 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset                    fee;                   ///< Operation fee
      account_id_type          owner_account;         ///< Owner of the credit offer
      credit_offer_id_type     offer_id;              ///< ID of the credit offer
      optional<asset>          delta_amount;          ///< Delta amount, optional
      optional<uint32_t>       fee_rate;              ///< New fee rate, optional
      optional<uint32_t>       max_duration_seconds;  ///< New repayment time limit, optional
      optional<share_type>     min_deal_amount;       ///< Minimum amount to borrow for each new deal, optional
      optional<bool>           enabled;               ///< Whether this offer is available, optional
      optional<time_point_sec> auto_disable_time;     ///< New time to disable automatically, optional

      /// New types and rates of acceptable collateral, optional
      optional<flat_map<asset_id_type, price>>          acceptable_collateral;

      /// New allowed borrowers and their maximum amounts to borrow, optional
      optional<flat_map<account_id_type, share_type>>   acceptable_borrowers;

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return owner_account; }
      void            validate()const override;
      share_type      calculate_fee(const fee_params_t& k)const;
   };

   /// Defines automatic repayment types
   enum class credit_deal_auto_repayment_type
   {
      /// Do not repay automatically
      no_auto_repayment = 0,
      /// Automatically repay fully when and only when the account balance is sufficient
      only_full_repayment = 1,
      /// Automatically repay as much as possible using available account balance
      allow_partial_repayment = 2,
      /// Total number of available automatic repayment types
      CDAR_TYPE_COUNT = 3
   };

   /**
    * @brief Accept a credit offer, thereby creating a credit deal
    * @ingroup operations
    */
   struct credit_offer_accept_operation : public base_operation
   {
      struct ext
      {
         /// After the core-2595 hard fork, the account can specify whether and how to automatically repay
         fc::optional<uint8_t> auto_repay;
      };

      struct fee_params_t { uint64_t fee = 1 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          borrower;           ///< The account who accepts the offer
      credit_offer_id_type     offer_id;           ///< ID of the credit offer
      asset                    borrow_amount;      ///< The amount to borrow
      asset                    collateral;         ///< The collateral
      uint32_t                 max_fee_rate = 0;   ///< The maximum acceptable fee rate
      uint32_t                 min_duration_seconds = 0; ///< The minimum acceptable duration

      extension<ext> extensions; ///< Extensions

      account_id_type fee_payer()const { return borrower; }
      void            validate()const override;
   };

   /**
    * @brief Repay a credit deal
    * @ingroup operations
    */
   struct credit_deal_repay_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 1 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who repays to the credit offer
      credit_deal_id_type      deal_id;            ///< ID of the credit deal
      asset                    repay_amount;       ///< The amount to repay
      asset                    credit_fee;         ///< The credit fee relative to the amount to repay

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const override;
   };

   /**
    * @brief A credit deal expired without being fully repaid
    * @ingroup operations
    * @note This is a virtual operation.
    */
   struct credit_deal_expired_operation : public base_operation
   {
      struct fee_params_t {};

      credit_deal_expired_operation() = default;

      credit_deal_expired_operation( const credit_deal_id_type& did, const credit_offer_id_type& oid,
            const account_id_type& o, const account_id_type& b, const asset& u, const asset& c, const uint32_t fr)
      : deal_id(did), offer_id(oid), offer_owner(o), borrower(b), unpaid_amount(u), collateral(c), fee_rate(fr)
      { /* Nothing to do */ }

      asset                    fee;                ///< Only for compatibility, unused
      credit_deal_id_type      deal_id;            ///< ID of the credit deal
      credit_offer_id_type     offer_id;           ///< ID of the credit offer
      account_id_type          offer_owner;        ///< Owner of the credit offer
      account_id_type          borrower;           ///< The account who repays to the credit offer
      asset                    unpaid_amount;      ///< The amount that is unpaid
      asset                    collateral;         ///< The collateral liquidated
      uint32_t                 fee_rate = 0;       ///< Fee rate, the demominator is GRAPHENE_FEE_RATE_DENOM

      account_id_type fee_payer()const { return borrower; }
      void            validate()const override { FC_ASSERT( !"virtual operation" ); }

      /// This is a virtual operation; there is no fee
      share_type      calculate_fee(const fee_params_t&)const { return 0; }
   };

   /**
    * @brief Update a credit deal
    * @ingroup operations
    */
   struct credit_deal_update_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 1 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who owns the credit deal
      credit_deal_id_type      deal_id;            ///< ID of the credit deal
      uint8_t                  auto_repay;         ///< The specified automatic repayment type

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const override;
   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::credit_offer_create_operation::fee_params_t, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::credit_offer_delete_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::credit_offer_update_operation::fee_params_t, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::credit_offer_accept_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::credit_deal_repay_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::credit_deal_expired_operation::fee_params_t, ) // VIRTUAL
FC_REFLECT( graphene::protocol::credit_deal_update_operation::fee_params_t, (fee) )

FC_REFLECT( graphene::protocol::credit_offer_create_operation,
            (fee)
            (owner_account)
            (asset_type)
            (balance)
            (fee_rate)
            (max_duration_seconds)
            (min_deal_amount)
            (enabled)
            (auto_disable_time)
            (acceptable_collateral)
            (acceptable_borrowers)
            (extensions)
          )

FC_REFLECT( graphene::protocol::credit_offer_delete_operation,
            (fee)
            (owner_account)
            (offer_id)
            (extensions)
          )

FC_REFLECT( graphene::protocol::credit_offer_update_operation,
            (fee)
            (owner_account)
            (offer_id)
            (delta_amount)
            (fee_rate)
            (max_duration_seconds)
            (min_deal_amount)
            (enabled)
            (auto_disable_time)
            (acceptable_collateral)
            (acceptable_borrowers)
            (extensions)
          )

FC_REFLECT( graphene::protocol::credit_offer_accept_operation::ext,
            (auto_repay)
          )

FC_REFLECT( graphene::protocol::credit_offer_accept_operation,
            (fee)
            (borrower)
            (offer_id)
            (borrow_amount)
            (collateral)
            (max_fee_rate)
            (min_duration_seconds)
            (extensions)
          )

FC_REFLECT( graphene::protocol::credit_deal_repay_operation,
            (fee)
            (account)
            (deal_id)
            (repay_amount)
            (credit_fee)
            (extensions)
          )

FC_REFLECT( graphene::protocol::credit_deal_expired_operation,
            (fee)
            (deal_id)
            (offer_id)
            (offer_owner)
            (borrower)
            (unpaid_amount)
            (collateral)
            (fee_rate)
          )

FC_REFLECT( graphene::protocol::credit_deal_update_operation,
            (fee)
            (account)
            (deal_id)
            (auto_repay)
            (extensions)
          )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation::ext )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_create_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_delete_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_update_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_repay_operation::fee_params_t )
// Note: credit_deal_expired_operation is virtual so no external serialization for its fee_params_t
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_update_operation::fee_params_t )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_delete_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_offer_accept_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_repay_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_expired_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::credit_deal_update_operation )
