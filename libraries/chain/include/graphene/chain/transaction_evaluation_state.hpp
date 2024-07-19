// AcloudBank
#pragma once
#include <graphene/protocol/operations.hpp>

namespace graphene {
namespace protocol { class signed_transaction; }
namespace chain {
   class database;
   using protocol::signed_transaction;

   /**
    *  Place holder for state tracked while processing a transaction. This class provides helper methods that are
    *  common to many different operations and also tracks which keys have signed the transaction
    */
   class transaction_evaluation_state
   {
      public:
         transaction_evaluation_state( database* db = nullptr )
         :_db(db){}


         database& db()const { assert( _db ); return *_db; }
         vector<operation_result> operation_results;

         const signed_transaction*        _trx = nullptr;
         database*                        _db = nullptr;
         bool                             _is_proposed_trx = false;
         bool                             skip_fee_schedule_check = false;
         bool                             skip_limit_order_price_check = false; // Used in limit_order_update_op
   };
} } // namespace graphene::chain
