// AcloudBank
#pragma once
#include <graphene/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

/**
 * special_authority_object only exists to help with a specific indexing problem.
 * We want to be able to iterate over all accounts that contain a special authority.
 * However, accounts which have a special_authority are very rare.  So rather
 * than indexing account_object by the special_authority fields (requiring additional
 * bookkeeping for every account), we instead maintain a special_authority_object
 * pointing to each account which has special_authority (requiring additional
 * bookkeeping only for every account which has special_authority).
 *
 * This class is an implementation detail.
 */

class special_authority_object : public graphene::db::abstract_object<special_authority_object,
                                           implementation_ids, impl_special_authority_object_type>
{
   public:
      account_id_type account;
};

struct by_account;

using special_authority_multi_idx_typ = multi_index_container<
   special_authority_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_account>,
         member< special_authority_object, account_id_type, &special_authority_object::account> >
   >
>;

using special_authority_index = generic_index< special_authority_object, special_authority_multi_idx_typ >;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::special_authority_object)

FC_REFLECT_TYPENAME( graphene::chain::special_authority_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::special_authority_object )
