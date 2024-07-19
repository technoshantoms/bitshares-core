/*
 * Acloudbank
 */
#include <graphene/protocol/custom.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

void custom_operation::validate()const
{
   FC_ASSERT( fee.amount > 0 );
}
share_type custom_operation::calculate_fee(const fee_params_t& k)const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::custom_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::custom_operation )
