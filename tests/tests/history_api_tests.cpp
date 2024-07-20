

#include <boost/test/unit_test.hpp>

#include <graphene/app/api.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;
BOOST_FIXTURE_TEST_SUITE( history_api_tests, database_fixture )

BOOST_AUTO_TEST_CASE(get_account_history) {
   try {
      graphene::app::history_api hist_api(app);

      //account_id_type() do 3 ops
      create_bitasset("USD", account_id_type());
      create_account( "dan", account_id_type()(db), GRAPHENE_WITNESS_ACCOUNT(db) );
      create_account( "bob", account_id_type()(db), GRAPHENE_TEMP_ACCOUNT(db) );

      generate_block();
      fc::usleep(fc::milliseconds(100));

      int asset_create_op_id = operation::tag<asset_create_operation>::value;
      int account_create_op_id = operation::tag<account_create_operation>::value;

      //account_id_type() did 3 ops and includes id0
      vector<operation_history_object> histories = hist_api.get_account_history("1.2.0", operation_history_id_type(),
                                                      100, operation_history_id_type());

      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);
      BOOST_CHECK_EQUAL(histories[2].op.which(), asset_create_op_id);
      BOOST_CHECK( histories[2].block_time == db.head_block_time() );
      BOOST_CHECK( !histories[2].is_virtual );

      // 1 account_create op larger than id1
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK(histories[0].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // Limit 2 returns 2 result
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(),
                                                      2, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK(histories[1].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[1].op.which(), account_create_op_id);

      // bob has 1 op
      histories = hist_api.get_account_history("bob", operation_history_id_type(),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // witness-account has 0 op
      histories = hist_api.get_account_history("witness-account", operation_history_id_type(),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // get_block_operation_history
      auto head_block_num = db.head_block_num();
      histories = hist_api.get_block_operation_history(head_block_num);
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      histories = hist_api.get_block_operation_history(head_block_num, 1u);
      BOOST_CHECK_EQUAL(histories.size(), 1u);

      // get_block_operations_by_time
      auto time1 = db.head_block_time();
      histories = hist_api.get_block_operations_by_time(time1);
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      histories = hist_api.get_block_operations_by_time(time1 + fc::seconds(1));
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      histories = hist_api.get_block_operations_by_time(time1 - fc::seconds(1));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      generate_block();
      auto time2 = db.head_block_time();

      histories = hist_api.get_block_operations_by_time(time1);
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      histories = hist_api.get_block_operations_by_time(time1 - fc::seconds(1));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      histories = hist_api.get_block_operations_by_time(time2);
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      create_bitasset("USX", account_id_type());
      generate_block();
      auto time3 = db.head_block_time();

      histories = hist_api.get_block_operations_by_time(time2);
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      histories = hist_api.get_block_operations_by_time(time3);
      BOOST_CHECK_EQUAL(histories.size(), 1u);

      histories = hist_api.get_block_operations_by_time(time3 + fc::seconds(1));
      BOOST_CHECK_EQUAL(histories.size(), 1u);

   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(get_account_history_virtual_operation_test)
{ try {
      graphene::app::history_api hist_api(app);

      asset_id_type usd_id = create_user_issued_asset("USD").get_id();

      ACTORS( (dan)(bob) );
      fund( dan, asset(100) );
      issue_uia( bob_id, asset(100, usd_id) );

      create_sell_order( dan_id, asset(100), asset(100, usd_id) );
      create_sell_order( bob_id, asset(100, usd_id), asset(100) );

      generate_block();
      fc::usleep(fc::milliseconds(100));

      auto fill_order_op_id = operation::tag<fill_order_operation>::value;

      vector<operation_history_object> histories = hist_api.get_account_history("dan", operation_history_id_type(),
                                                      100, operation_history_id_type());

      BOOST_REQUIRE_GT( histories.size(), 0 );
      BOOST_CHECK_EQUAL( histories.front().op.which(), fill_order_op_id );
      BOOST_CHECK( histories.front().block_time == db.head_block_time() );
      BOOST_CHECK( histories.front().is_virtual );

      // Create a limit order that expires in 300 seconds
      create_sell_order( dan_id, asset(10, usd_id), asset(10), db.head_block_time() + 300 );

      generate_block();
      fc::usleep(fc::milliseconds(100));

      auto order_create_op_id = operation::tag<limit_order_create_operation>::value;

      histories = hist_api.get_account_history("dan", operation_history_id_type(),
                                                      100, operation_history_id_type());

      BOOST_REQUIRE_GT( histories.size(), 0 );
      BOOST_CHECK_EQUAL( histories.front().op.which(), order_create_op_id );
      BOOST_CHECK( histories.front().block_time == db.head_block_time() );
      BOOST_CHECK( !histories.front().is_virtual );

      // Let the limit order expire
      generate_blocks( db.head_block_time() + 300 );
      generate_block();
      fc::usleep(fc::milliseconds(100));

      auto order_cancel_op_id = operation::tag<limit_order_cancel_operation>::value;

      histories = hist_api.get_account_history("dan", operation_history_id_type(),
                                                      100, operation_history_id_type());

      BOOST_REQUIRE_GT( histories.size(), 0 );
      BOOST_CHECK_EQUAL( histories.front().op.which(), order_cancel_op_id );
      BOOST_CHECK( histories.front().is_virtual );
      BOOST_CHECK( histories.front().result.is_type<asset>() );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(get_account_history_notify_all_on_creation) {
   try {
      // Pass hard fork time
      generate_blocks(HARDFORK_CORE_265_TIME);
      set_expiration( db, trx );

      graphene::app::history_api hist_api(app);

      //account_id_type() do 3 ops
      create_bitasset("USD", account_id_type());
      create_account( "dan", account_id_type()(db), GRAPHENE_WITNESS_ACCOUNT(db) );
      create_account( "bob", account_id_type()(db), GRAPHENE_TEMP_ACCOUNT(db) );

      generate_block();
      fc::usleep(fc::milliseconds(100));

      int asset_create_op_id = operation::tag<asset_create_operation>::value;
      int account_create_op_id = operation::tag<account_create_operation>::value;

      //account_id_type() did 3 ops and includes id0
      vector<operation_history_object> histories = hist_api.get_account_history("1.2.0", operation_history_id_type(),
                                                      100, operation_history_id_type());

      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);
      BOOST_CHECK_EQUAL(histories[2].op.which(), asset_create_op_id);

      // 1 account_create op larger than id1
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK(histories[0].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // Limit 2 returns 2 result
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(),
                                                      2, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK(histories[1].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[1].op.which(), account_create_op_id);

      // bob has 1 op
      histories = hist_api.get_account_history("bob", operation_history_id_type(),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // witness-account has 1 op
      histories = hist_api.get_account_history("witness-account", operation_history_id_type(),
                                                      100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(get_account_history_additional) {
   try {
      graphene::app::history_api hist_api(app);

      // A = account_id_type() with records { 5, 3, 1, 0 }, and
      // B = dan with records { 6, 4, 2, 1 }
      // account_id_type() and dan share operation id 1(account create) - share can be also in id 0

      // no history at all in the chain
      vector<operation_history_object> histories =
            hist_api.get_account_history("1.2.0", operation_history_id_type(0), 4, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      create_bitasset("USD", account_id_type()); // create op 0
      generate_block();
      // what if the account only has one history entry and it is 0?
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      const account_object& dan = create_account("dan"); // create op 1

      create_bitasset("CNY", dan.get_id()); // create op 2
      create_bitasset("BTC", account_id_type()); // create op 3
      create_bitasset("XMR", dan.get_id()); // create op 4
      create_bitasset("EUR", account_id_type()); // create op 5
      create_bitasset("OIL", dan.get_id()); // create op 6

      generate_block();
      fc::usleep(fc::milliseconds(100));

      // f(A, 0, 4, 9) = { 5, 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      // f(A, 0, 4, 6) = { 5, 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(6));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      // f(A, 0, 4, 5) = { 5, 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(5));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      // f(A, 0, 4, 4) = { 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(4));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

      // f(A, 0, 4, 3) = { 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(3));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

      // f(A, 0, 4, 2) = { 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

      // f(A, 0, 4, 1) = { 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

      // f(A, 0, 4, 0) = { 5, 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      // f(A, 1, 5, 9) = { 5, 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      // f(A, 1, 5, 6) = { 5, 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(6));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      // f(A, 1, 5, 5) = { 5, 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(5));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      // f(A, 1, 5, 4) = { 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(4));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);

      // f(A, 1, 5, 3) = { 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(3));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);

      // f(A, 1, 5, 2) = { }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(A, 1, 5, 1) = { }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(A, 1, 5, 0) = { 5, 3 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      // f(A, 0, 3, 9) = { 5, 3, 1 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(A, 0, 3, 6) = { 5, 3, 1 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(6));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(A, 0, 3, 5) = { 5, 3, 1 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(5));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(A, 0, 3, 4) = { 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(4));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

      // f(A, 0, 3, 3) = { 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(3));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

      // f(A, 0, 3, 2) = { 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

      // f(A, 0, 3, 1) = { 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

      // f(A, 0, 3, 0) = { 5, 3, 1 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(B, 0, 4, 9) = { 6, 4, 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      // f(B, 0, 4, 6) = { 6, 4, 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(6));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      // f(B, 0, 4, 5) = { 4, 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(5));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(B, 0, 4, 4) = { 4, 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(4));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

      // f(B, 0, 4, 3) = { 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(3));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);

      // f(B, 0, 4, 2) = { 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);

      // f(B, 0, 4, 1) = { 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);

      // f(B, 0, 4, 0) = { 6, 4, 2, 1 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      // f(B, 2, 4, 9) = { 6, 4 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

      // f(B, 2, 4, 6) = { 6, 4 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(6));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

      // f(B, 2, 4, 5) = { 4 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(5));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);

      // f(B, 2, 4, 4) = { 4 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(4));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);

      // f(B, 2, 4, 3) = { }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(3));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(B, 2, 4, 2) = { }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(B, 2, 4, 1) = { }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(B, 2, 4, 0) = { 6, 4 }
      histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

      // 0 limits
      histories = hist_api.get_account_history("dan", operation_history_id_type(0), 0, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(3), 0,
                                                        operation_history_id_type(9));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // non existent account
      histories = hist_api.get_account_history("1.2.18", operation_history_id_type(0), 4,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // create a new account C = alice { 7 }
      create_account("alice");

      generate_block();

      // f(C, 0, 4, 10) = { 7 }
      histories = hist_api.get_account_history("alice", operation_history_id_type(0), 4,
                                                        operation_history_id_type(10));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 7u);

      // f(C, 8, 4, 10) = { }
      histories = hist_api.get_account_history("alice", operation_history_id_type(8), 4,
                                                        operation_history_id_type(10));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // f(A, 0, 10, 0) = { 7, 5, 3, 1, 0 }
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 5u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 7u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[4].id.instance(), 0u);

   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(get_account_history_by_time) {
   try {
      graphene::app::history_api hist_api(app);

      auto time1 = db.head_block_time();

      // A = account_id_type() with records { 5, 3, 1, 0 }, and
      // B = dan with records { 6, 4, 2, 1 }
      // account_id_type() and dan share operation id 1(account create) - share can be also in id 0

      // no history at all in the chain
      vector<operation_history_object> histories = hist_api.get_account_history_by_time("1.2.0");
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      create_bitasset("USD", account_id_type()); // create op 0
      generate_block();
      fc::usleep(fc::milliseconds(100));

      auto time2 = db.head_block_time();

      // what if the account only has one history entry and it is 0?
      histories = hist_api.get_account_history_by_time("1.2.0");
      BOOST_REQUIRE_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", 0);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", 2, time1);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", 10, time2);
      BOOST_REQUIRE_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", {}, time2);
      BOOST_REQUIRE_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      BOOST_CHECK_THROW( hist_api.get_account_history_by_time( "1.2.0", 102 ), fc::exception );

      const account_object& dan = create_account("dan"); // create op 1

      create_bitasset("CNY", dan.get_id()); // create op 2
      create_bitasset("BTC", account_id_type()); // create op 3
      create_bitasset("XMR", dan.get_id()); // create op 4
      create_bitasset("EUR", account_id_type()); // create op 5
      create_bitasset("OIL", dan.get_id()); // create op 6

      generate_block();
      fc::usleep(fc::milliseconds(100));

      auto time3 = db.head_block_time();

      histories = hist_api.get_account_history_by_time("1.2.0", {}, time2);
      BOOST_REQUIRE_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", {}, time2 + fc::seconds(1));
      BOOST_REQUIRE_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", {}, time3);
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("1.2.0", 2, time3);
      BOOST_REQUIRE_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      histories = hist_api.get_account_history_by_time("1.2.0", 2);
      BOOST_REQUIRE_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      histories = hist_api.get_account_history_by_time("1.2.0");
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      histories = hist_api.get_account_history_by_time("dan", {}, time3);
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      histories = hist_api.get_account_history_by_time("dan", 5, time3);
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      histories = hist_api.get_account_history_by_time("dan", 5);
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      histories = hist_api.get_account_history_by_time("dan");
      BOOST_REQUIRE_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

      histories = hist_api.get_account_history_by_time("dan", 2, time3);
      BOOST_REQUIRE_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

      histories = hist_api.get_account_history_by_time("dan", 2, time2);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      histories = hist_api.get_account_history_by_time("dan", {}, time1);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      histories = hist_api.get_account_history_by_time("nathan", 2);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(track_account) {
   try {
      graphene::app::history_api hist_api(app);

      // account_id_type() is not tracked

      // account_id_type() creates alice(not tracked account)
      create_account("alice");

      //account_id_type() creates some ops
      create_bitasset("CNY", account_id_type());
      create_bitasset("USD", account_id_type());

      // account_id_type() creates dan(account tracked)
      const account_object& dan = create_account("dan");
      auto dan_id = dan.get_id();

      // dan makes 1 op
      create_bitasset("EUR", dan_id);

      generate_block();

      // anything against account_id_type() should be {}
      vector<operation_history_object> histories =
            hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 1,
                                                        operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // anything against alice should be {}
      histories = hist_api.get_account_history("alice", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("alice", operation_history_id_type(1), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("alice", operation_history_id_type(1), 1,
                                                        operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // dan should have history
      histories = hist_api.get_account_history("dan", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

      // create more ops, starting with an untracked account
      create_bitasset( "BTC", account_id_type() );
      create_bitasset( "GBP", dan_id );

      generate_block();

      histories = hist_api.get_account_history("dan", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 3u);

      db.pop_block();

      // Try again, should result in same object IDs
      create_bitasset( "BTC", account_id_type() );
      create_bitasset( "GBP", dan_id );

      generate_block();

      histories = hist_api.get_account_history("dan", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 3u);
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(track_account2) {
   try {
      graphene::app::history_api hist_api(app);

      // account_id_type() is tracked

      // account_id_type() creates alice(tracked account)
      const account_object& alice = create_account("alice");
      auto alice_id = alice.get_id();

      //account_id_type() creates some ops
      create_bitasset("CNY", account_id_type());
      create_bitasset("USD", account_id_type());

      // alice makes 1 op
      create_bitasset("EUR", alice_id);

      // account_id_type() creates dan(account not tracked)
      create_account("dan");

      generate_block();

      // all account_id_type() should have 4 ops {4,2,1,0}
      vector<operation_history_object> histories = hist_api.get_account_history("committee-account",
            operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 4u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 2u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
      BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

      // all alice account should have 2 ops {3, 0}
      histories = hist_api.get_account_history("alice", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
      BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

      // alice first op should be {0}
      histories = hist_api.get_account_history("alice", operation_history_id_type(0), 1,
                                                        operation_history_id_type(1));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);

      // alice second op should be {3}
      histories = hist_api.get_account_history("alice", operation_history_id_type(1), 1,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);

      // anything against dan should be {}
      histories = hist_api.get_account_history("dan", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("dan", operation_history_id_type(1), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);
      histories = hist_api.get_account_history("dan", operation_history_id_type(1), 1, operation_history_id_type(2));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(min_blocks_to_keep_test) {
   try {

      graphene::app::history_api hist_api(app);

      generate_block();
      generate_block();
      generate_block();
      generate_block();
      generate_block();

      vector<operation_history_object> histories =
            hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10, operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // max-ops-per-account = 2
      // min-blocks-to-keep = 3
      // max-ops-per-acc-by-min-blocks = 5

      //account_id_type() creates some ops
      create_bitasset("USA", account_id_type());
      create_bitasset("USB", account_id_type());
      create_bitasset("USC", account_id_type());

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_REQUIRE_EQUAL(histories.size(), 3u);
      operation_history_id_type oldest_op_hist_id { histories.back().id };
      BOOST_CHECK( db.find(oldest_op_hist_id) );

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 3u);

      create_bitasset("USD", account_id_type());
      create_bitasset("USE", account_id_type());
      create_bitasset("USF", account_id_type());
      create_bitasset("USG", account_id_type());

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 5u);
      BOOST_CHECK( !db.find(oldest_op_hist_id) );

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 4u);

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 4u);

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);

      generate_block();
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10,
                                                        operation_history_id_type(0));
      BOOST_CHECK_EQUAL(histories.size(), 2u);

   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(get_account_history_operations) {
   try {
      graphene::app::history_api hist_api(app);

      //account_id_type() do 3 ops
      create_bitasset("CNY", account_id_type());
      create_account("sam");
      create_account("alice");

      generate_block();
      fc::usleep(fc::milliseconds(100));

      int asset_create_op_id = operation::tag<asset_create_operation>::value;
      int account_create_op_id = operation::tag<account_create_operation>::value;

      //account_id_type() did 1 asset_create op
      vector<operation_history_object> histories = hist_api.get_account_history_operations(
            "committee-account", asset_create_op_id, operation_history_id_type(), operation_history_id_type(), 100);
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), asset_create_op_id);

      //account_id_type() did 2 account_create ops
      histories = hist_api.get_account_history_operations(
            "committee-account", account_create_op_id, operation_history_id_type(), operation_history_id_type(), 100);
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // No asset_create op larger than id1
      histories = hist_api.get_account_history_operations(
            "committee-account", asset_create_op_id, operation_history_id_type(), operation_history_id_type(1), 100);
      BOOST_CHECK_EQUAL(histories.size(), 0u);

      // Limit 1 returns 1 result
      histories = hist_api.get_account_history_operations(
            "committee-account", account_create_op_id, operation_history_id_type(),operation_history_id_type(), 1);
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // alice has 1 op
      histories = hist_api.get_account_history_operations(
         "alice", account_create_op_id, operation_history_id_type(),operation_history_id_type(), 100);
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // create a bunch of accounts
      for(int i = 0; i < 80; ++i)
      {
         std::string acct_name = "mytempacct" + std::to_string(i);
         create_account(acct_name);
      }
      generate_block();

      // history is set to limit transactions to 75 (see database_fixture.cpp)
      // so asking for more should only return 75 (and not throw exception,
      // see https://github.com/bitshares/bitshares-core/issues/1490
      histories = hist_api.get_account_history_operations(
            "committee-account", account_create_op_id, operation_history_id_type(), operation_history_id_type(), 100);
      BOOST_CHECK_EQUAL(histories.size(), 75u);
      if (histories.size() > 0)
         BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);


   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}
//new test case for increasing the limit based on the config file
BOOST_AUTO_TEST_CASE(api_limit_get_account_history_operations) {
 try {
   graphene::app::history_api hist_api(app);
   //account_id_type() do 3 ops
   create_bitasset("CNY", account_id_type());
   create_account("sam");
   create_account("alice");

   generate_block();
   fc::usleep(fc::milliseconds(100));

   int asset_create_op_id = operation::tag<asset_create_operation>::value;
   int account_create_op_id = operation::tag<account_create_operation>::value;

   //account_id_type() did 1 asset_create op
   vector<operation_history_object> histories = hist_api.get_account_history_operations(
      "committee-account", asset_create_op_id, operation_history_id_type(), operation_history_id_type(), 200);
   BOOST_CHECK_EQUAL(histories.size(), 1u);
   BOOST_CHECK_EQUAL(histories[0].id.instance(), 0u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), asset_create_op_id);

   //account_id_type() did 2 account_create ops
   histories = hist_api.get_account_history_operations(
      "committee-account", account_create_op_id, operation_history_id_type(), operation_history_id_type(), 200);
   BOOST_CHECK_EQUAL(histories.size(), 2u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   // No asset_create op larger than id1
   histories = hist_api.get_account_history_operations(
      "committee-account", asset_create_op_id, operation_history_id_type(), operation_history_id_type(1), 200);
   BOOST_CHECK_EQUAL(histories.size(), 0u);

   // Limit 1 returns 1 result
   histories = hist_api.get_account_history_operations(
      "committee-account", account_create_op_id, operation_history_id_type(),operation_history_id_type(), 1);
   BOOST_CHECK_EQUAL(histories.size(), 1u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   // alice has 1 op
   histories = hist_api.get_account_history_operations(
      "alice", account_create_op_id, operation_history_id_type(),operation_history_id_type(), 200);
   BOOST_CHECK_EQUAL(histories.size(), 1u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   // create a bunch of accounts
   for(int i = 0; i < 126; ++i)
   {
      std::string acct_name = "mytempacct" + std::to_string(i);
      create_account(acct_name);
   }
   generate_block();

   // history is set to limit transactions to 125 (see database_fixture.cpp)
   // so asking for more should only return 125 (and not throw exception,
   // see https://github.com/bitshares/bitshares-core/issues/1490
   GRAPHENE_CHECK_THROW( hist_api.get_account_history_operations("commitee-account", account_create_op_id,
                               operation_history_id_type(),operation_history_id_type(), 301),
                         fc::exception );
   histories = hist_api.get_account_history_operations("committee-account", account_create_op_id,
                         operation_history_id_type(), operation_history_id_type(), 200);
   BOOST_REQUIRE_EQUAL( histories.size(), 125u );
 }
 catch (fc::exception &e)
 {
   edump((e.to_detail_string()));
   throw;
 }
}

BOOST_AUTO_TEST_CASE(api_limit_get_account_history) {
 try{
   graphene::app::history_api hist_api(app);
   //account_id_type() do 3 ops
   create_bitasset("USD", account_id_type());
   create_account("dan");
   create_account("bob");

   generate_block();
   fc::usleep(fc::milliseconds(100));

   int asset_create_op_id = operation::tag<asset_create_operation>::value;
   int account_create_op_id = operation::tag<account_create_operation>::value;
   //account_id_type() did 3 ops and includes id0
   vector<operation_history_object> histories =
         hist_api.get_account_history("1.2.0", operation_history_id_type(), 210, operation_history_id_type());

   BOOST_CHECK_EQUAL(histories.size(), 3u);
   BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);
   BOOST_CHECK_EQUAL(histories[2].op.which(), asset_create_op_id);

   // 1 account_create op larger than id1
   histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 210, operation_history_id_type());
   BOOST_CHECK_EQUAL(histories.size(), 1u);
   BOOST_CHECK(histories[0].id.instance() != 0u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);


   // Limit 2 returns 2 result
   histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 2, operation_history_id_type());
   BOOST_CHECK_EQUAL(histories.size(), 2u);
   BOOST_CHECK(histories[1].id.instance() != 0u);
   BOOST_CHECK_EQUAL(histories[1].op.which(), account_create_op_id);
   // bob has 1 op
   histories = hist_api.get_account_history("bob", operation_history_id_type(), 210, operation_history_id_type());
   BOOST_CHECK_EQUAL(histories.size(), 1u);
   BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   // create a bunch of accounts
   for(int i = 0; i < 126; ++i)
   {
      std::string acct_name = "mytempacct" + std::to_string(i);
      create_account(acct_name);
   }
   generate_block();

   GRAPHENE_CHECK_THROW( hist_api.get_account_history("1.2.0", operation_history_id_type(), 260,
                               operation_history_id_type()),
                         fc::exception );
   histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 210, operation_history_id_type());
   BOOST_REQUIRE_EQUAL( histories.size(), 125u );
 } catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
 }
}
BOOST_AUTO_TEST_CASE(api_limit_get_relative_account_history) {
 try{
   graphene::app::history_api hist_api(app);
   //account_id_type() do 3 ops
   create_bitasset("USD", account_id_type());
   create_account("dan");
   create_account("bob");

   generate_block();
   fc::usleep(fc::milliseconds(100));

   GRAPHENE_CHECK_THROW(hist_api.get_relative_account_history("1.2.0", 126, 260, 0), fc::exception);
   vector<operation_history_object> histories = hist_api.get_relative_account_history("1.2.0", 126, 210, 0);
   BOOST_REQUIRE_EQUAL( histories.size(), 0u );

 } catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
 }
}

BOOST_AUTO_TEST_CASE(api_limit_get_account_history_by_operations) {
 try {
   graphene::app::history_api hist_api(app);
   flat_set<uint16_t> operation_types;
   //account_id_type() do 3 ops
   create_bitasset("USD", account_id_type());
   create_account("dan");
   create_account("bob");
   generate_block();
   fc::usleep(fc::milliseconds(100));
   GRAPHENE_CHECK_THROW(hist_api.get_account_history_by_operations("1.2.0", operation_types, 0, 260), fc::exception);
   auto histories = hist_api.get_account_history_by_operations("1.2.0", operation_types, 0, 210);
   BOOST_REQUIRE_EQUAL( histories.total_count, 3u );
 }
 catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
 }
}

BOOST_AUTO_TEST_SUITE_END()
