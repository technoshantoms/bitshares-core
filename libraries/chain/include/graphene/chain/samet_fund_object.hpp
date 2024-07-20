/*
 * Acloudbank
 */

#pragma once

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

/**
 *  @brief A SameT Fund is a fund which can be used by a borrower and have to be repaid in the same transaction
 *  @ingroup object
 *  @ingroup protocol
 *
 */
class samet_fund_object : public abstract_object<samet_fund_object, protocol_ids, samet_fund_object_type>
{
   public:
      account_id_type owner_account;         ///< Owner of the fund
      asset_id_type   asset_type;            ///< Asset type in the fund
      share_type      balance;               ///< Usable amount in the fund
      uint32_t        fee_rate = 0;          ///< Fee rate, the demominator is GRAPHENE_FEE_RATE_DENOM
      share_type      unpaid_amount;         ///< Unpaid amount
};

struct by_unpaid;      // for protocol
struct by_owner;       // for API
struct by_asset_type;  // for API

/**
* @ingroup object_index
*/
using samet_fund_multi_index_type = multi_index_container<
   samet_fund_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_unpaid>,
         composite_key< samet_fund_object,
            member< samet_fund_object, share_type, &samet_fund_object::unpaid_amount >,
            member< object, object_id_type, &object::id>
         >,
         composite_key_compare< std::greater<share_type>, std::less<object_id_type> >
      >,
      ordered_unique< tag<by_owner>,
         composite_key< samet_fund_object,
            member< samet_fund_object, account_id_type, &samet_fund_object::owner_account >,
            member< object, object_id_type, &object::id>
         >
      >,
      ordered_unique< tag<by_asset_type>,
         composite_key< samet_fund_object,
            member< samet_fund_object, asset_id_type, &samet_fund_object::asset_type >,
            member< object, object_id_type, &object::id>
         >
      >
   >
>;

/**
* @ingroup object_index
*/
using samet_fund_index = generic_index<samet_fund_object, samet_fund_multi_index_type>;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE( graphene::chain::samet_fund_object )

FC_REFLECT_TYPENAME( graphene::chain::samet_fund_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::samet_fund_object )
