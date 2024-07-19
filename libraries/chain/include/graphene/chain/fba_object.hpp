/*
 * Acloudbank
 */

#pragma once

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

class database;

/**
 * fba_accumulator_object accumulates fees to be paid out via buyback or other FBA mechanism.
 */

class fba_accumulator_object : public graphene::db::abstract_object< fba_accumulator_object,
                                         implementation_ids, impl_fba_accumulator_object_type >
{
   public:
      share_type accumulated_fba_fees;
      optional< asset_id_type > designated_asset;

      bool is_configured( const database& db )const;
};

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::fba_accumulator_object)

FC_REFLECT_TYPENAME( graphene::chain::fba_accumulator_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fba_accumulator_object )
