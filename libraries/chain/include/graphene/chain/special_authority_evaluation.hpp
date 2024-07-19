// AcloudBank
#pragma once

#include <graphene/protocol/special_authority.hpp>

namespace graphene { namespace chain {

class database;
using namespace protocol;

void evaluate_special_authority( const database& db, const special_authority& auth );

} } // graphene::chain
