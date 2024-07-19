/*
 * Acloudbank
 */
#pragma once

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

/**
 * buyback_authority_object only exists to help with a specific indexing problem.
 * We want to be able to iterate over all assets that have a buyback program.
 * However, assets which have a buyback program are very rare.  So rather
 * than indexing asset_object by the buyback field (requiring additional
 * bookkeeping for every asset), we instead maintain a buyback_object
 * pointing to each asset which has buyback (requiring additional
 * bookkeeping only for every asset which has buyback).
 *
 * This class is an implementation detail.
 */

class buyback_object : public graphene::db::abstract_object< buyback_object,
                                 implementation_ids, impl_buyback_object_type >
{
   public:
      asset_id_type asset_to_buy;
};

struct by_asset;

typedef multi_index_container<
   buyback_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_asset>, member< buyback_object, asset_id_type, &buyback_object::asset_to_buy> >
   >
> buyback_multi_index_type;

typedef generic_index< buyback_object, buyback_multi_index_type > buyback_index;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::buyback_object)

FC_REFLECT_TYPENAME( graphene::chain::buyback_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::buyback_object )
