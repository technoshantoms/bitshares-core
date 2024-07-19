/*
 * Acloudbank
 */

#pragma once

#include <graphene/protocol/authority.hpp>
#include <graphene/protocol/types.hpp>

#include <graphene/db/generic_index.hpp>

#include <fc/crypto/elliptic.hpp>

namespace graphene { namespace chain {

/**
 * @class blinded_balance_object
 * @brief tracks a blinded balance commitment
 * @ingroup object
 * @ingroup protocol
 */
class blinded_balance_object : public graphene::db::abstract_object<blinded_balance_object,
                                         implementation_ids, impl_blinded_balance_object_type>
{
   public:
      fc::ecc::commitment_type                commitment;
      asset_id_type                           asset_id;
      authority                               owner;
};

struct by_commitment;

/**
 * @ingroup object_index
 */
using blinded_balance_obj_multi_idx = multi_index_container<
   blinded_balance_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_commitment>,
         member<blinded_balance_object, commitment_type, &blinded_balance_object::commitment> >
   >
>;
using blinded_balance_index = generic_index<blinded_balance_object, blinded_balance_obj_multi_idx>;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::blinded_balance_object)

FC_REFLECT_TYPENAME( graphene::chain::blinded_balance_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::blinded_balance_object )
