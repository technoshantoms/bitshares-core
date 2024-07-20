/*
 * Acloudbank
 */

#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol {

   /**
    * @brief Create a new liquidity pool
    * @ingroup operations
    */
   struct liquidity_pool_create_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 50 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset           fee;                         ///< Operation fee
      account_id_type account;                     ///< The account who creates the liquidity pool
      asset_id_type   asset_a;                     ///< Type of the first asset in the pool
      asset_id_type   asset_b;                     ///< Type of the second asset in the pool
      asset_id_type   share_asset;                 ///< Type of the share asset aka the LP token
      uint16_t        taker_fee_percent = 0;       ///< Taker fee percent
      uint16_t        withdrawal_fee_percent = 0;  ///< Withdrawal fee percent

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

   /**
    * @brief Delete a liquidity pool
    * @ingroup operations
    */
   struct liquidity_pool_delete_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 0; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who owns the liquidity pool
      liquidity_pool_id_type   pool;               ///< ID of the liquidity pool

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

   /**
    * @brief Update a liquidity pool
    * @ingroup operations
    */
   struct liquidity_pool_update_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 1 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who owns the liquidity pool
      liquidity_pool_id_type   pool;               ///< ID of the liquidity pool
      optional<uint16_t>       taker_fee_percent;       ///< Taker fee percent
      optional<uint16_t>       withdrawal_fee_percent;  ///< Withdrawal fee percent

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

   /**
    * @brief Deposit to a liquidity pool
    * @ingroup operations
    */
   struct liquidity_pool_deposit_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION / 10; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who deposits to the liquidity pool
      liquidity_pool_id_type   pool;               ///< ID of the liquidity pool
      asset                    amount_a;           ///< The amount of the first asset to deposit
      asset                    amount_b;           ///< The amount of the second asset to deposit

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

   /**
    * @brief Withdraw from a liquidity pool
    * @ingroup operations
    */
   struct liquidity_pool_withdraw_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 5 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who withdraws from the liquidity pool
      liquidity_pool_id_type   pool;               ///< ID of the liquidity pool
      asset                    share_amount;       ///< The amount of the share asset to use

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

   /**
    * @brief Exchange with a liquidity pool
    * @ingroup operations
    * @note The result of this operation is a @ref generic_exchange_operation_result.
    *       There are 3 fees in the result, stored in this order:
    *       * maker market fee
    *       * taker market fee
    *       * liquidity pool taker fee
    */
   struct liquidity_pool_exchange_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 1 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                    fee;                ///< Operation fee
      account_id_type          account;            ///< The account who exchanges with the liquidity pool
      liquidity_pool_id_type   pool;               ///< ID of the liquidity pool
      asset                    amount_to_sell;     ///< The amount of one asset type to sell
      asset                    min_to_receive;     ///< The minimum amount of the other asset type to receive

      extensions_type extensions;  ///< Unused. Reserved for future use.

      account_id_type fee_payer()const { return account; }
      void            validate()const;
   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::liquidity_pool_create_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::liquidity_pool_delete_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::liquidity_pool_update_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::liquidity_pool_deposit_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::liquidity_pool_withdraw_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::liquidity_pool_exchange_operation::fee_params_t, (fee) )

FC_REFLECT( graphene::protocol::liquidity_pool_create_operation,
            (fee)(account)(asset_a)(asset_b)(share_asset)
            (taker_fee_percent)(withdrawal_fee_percent)(extensions) )
FC_REFLECT( graphene::protocol::liquidity_pool_delete_operation,
            (fee)(account)(pool)(extensions) )
FC_REFLECT( graphene::protocol::liquidity_pool_update_operation,
            (fee)(account)(pool)(taker_fee_percent)(withdrawal_fee_percent)(extensions) )
FC_REFLECT( graphene::protocol::liquidity_pool_deposit_operation,
            (fee)(account)(pool)(amount_a)(amount_b)(extensions) )
FC_REFLECT( graphene::protocol::liquidity_pool_withdraw_operation,
            (fee)(account)(pool)(share_amount)(extensions) )
FC_REFLECT( graphene::protocol::liquidity_pool_exchange_operation,
            (fee)(account)(pool)(amount_to_sell)(min_to_receive)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_create_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_delete_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_update_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_deposit_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_withdraw_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_exchange_operation::fee_params_t )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_delete_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_deposit_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_withdraw_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::liquidity_pool_exchange_operation )
