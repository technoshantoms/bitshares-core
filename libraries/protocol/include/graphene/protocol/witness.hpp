
#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol { 

  /**
    * @brief Create a witness object, as a bid to hold a witness position on the network.
    * @ingroup operations
    *
    * Accounts which wish to become witnesses may use this operation to create a witness object which stakeholders may
    * vote on to approve its position as a witness.
    */
   struct witness_create_operation : public base_operation
   {
      struct fee_params_t { uint64_t fee = 5000 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset             fee;
      /// The account which owns the witness. This account pays the fee for this operation.
      account_id_type   witness_account;
      string            url;
      public_key_type   block_signing_key;

      account_id_type fee_payer()const { return witness_account; }
      void            validate()const;
   };

  /**
    * @brief Update a witness object's URL and block signing key.
    * @ingroup operations
    */
   struct witness_update_operation : public base_operation
   {
      struct fee_params_t
      {
         share_type fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset             fee;
      /// The witness object to update.
      witness_id_type   witness;
      /// The account which owns the witness. This account pays the fee for this operation.
      account_id_type   witness_account;
      /// The new URL.
      optional< string > new_url;
      /// The new block signing key.
      optional< public_key_type > new_signing_key;

      account_id_type fee_payer()const { return witness_account; }
      void            validate()const;
   };

   /// TODO: witness_resign_operation : public base_operation

} } // graphene::protocol

FC_REFLECT( graphene::protocol::witness_create_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::witness_create_operation, (fee)(witness_account)(url)(block_signing_key) )

FC_REFLECT( graphene::protocol::witness_update_operation::fee_params_t, (fee) )
FC_REFLECT( graphene::protocol::witness_update_operation, (fee)(witness)(witness_account)(new_url)(new_signing_key) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::witness_create_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::witness_update_operation::fee_params_t )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::witness_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::witness_update_operation )
