

#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/protocol/market.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( settle_tests, database_fixture )

BOOST_AUTO_TEST_CASE( settle_rounding_test )
{
   try {
      // get around Graphene issue #615 feed expiration bug
      generate_blocks(HARDFORK_615_TIME);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db, trx );

      ACTORS((paul)(michael)(rachel)(alice)(bob)(ted)(joe)(jim));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& bitcny = create_bitasset("CNYBIT", paul_id);
      const auto& core   = asset_id_type()(db);
      asset_id_type bitusd_id = bitusd.get_id();
      asset_id_type bitcny_id = bitcny.get_id();
      asset_id_type core_id = core.get_id();

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id, asset(10000000));
      transfer(committee_account, alice_id, asset(10000000));
      transfer(committee_account, bob_id, asset(10000000));
      transfer(committee_account, jim_id, asset(10000000));

      // add a feed to asset
      update_feed_producers( bitusd, {paul.get_id()} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(5);
      publish_feed( bitusd, paul, current_feed );

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul, bitusd.amount(1000), core.amount(100) );
      call_order_id_type call_paul_id = call_paul.get_id();
      BOOST_REQUIRE_EQUAL( get_balance( paul, bitusd ), 1000 );

      // and transfer some to rachel
      transfer(paul.get_id(), rachel.get_id(), asset(200, bitusd.get_id()));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 200);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 100000000);

      // michael gets some bitusd
      const call_order_object& call_michael = *borrow(michael, bitusd.amount(6), core.amount(8));
      call_order_id_type call_michael_id = call_michael.get_id();

      // add settle order and check rounding issue
      operation_result result = force_settle(rachel, bitusd.amount(4));

      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 4 );

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 196);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 6);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul, core), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul, bitusd), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul.debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul.collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael.debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael.collateral.value );

      generate_blocks( db.head_block_time() + fc::hours(20) );
      set_expiration( db, trx );

      // default feed and settlement expires at the same time
      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 100 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(6) );

      // checks
      BOOST_CHECK( !db.find( settle_id ) );
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0); // rachel paid 4 usd and got nothing
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 196);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 996, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1002 ); // 1000 + 6 - 4

      // settle more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test less than it
      set_expiration( db, trx );
      operation_result result2 = force_settle(rachel_id(db), bitusd_id(db).amount(34));

      force_settlement_id_type settle_id2 { *result2.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id2(db).balance.amount.value, 34 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 162); // 196-34
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 996, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      generate_blocks( db.head_block_time() + fc::hours(10) );
      set_expiration( db, trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 100 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(16) );
      set_expiration( db, trx );

      // checks
      BOOST_CHECK( !db.find( settle_id2 ) );
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1); // rachel got 1 core and paid 34 usd
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 162);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 962, call_paul_id(db).debt.value ); // 996 - 34
      BOOST_CHECK_EQUAL( 99, call_paul_id(db).collateral.value ); // 100 - 1
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 968 ); // 1002 - 34

      // prepare for more tests
      transfer(paul_id, rachel_id, asset(300, bitusd_id));
      borrow(michael_id(db), bitusd_id(db).amount(2), core_id(db).amount(3));

      // settle even more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test more than it
      const operation_result result3 = force_settle(rachel_id(db), bitusd_id(db).amount(3));
      const operation_result result4 = force_settle(rachel_id(db), bitusd_id(db).amount(434));
      const operation_result result5 = force_settle(rachel_id(db), bitusd_id(db).amount(5));

      force_settlement_id_type settle_id3 { *result3.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id3(db).balance.amount.value, 3 );

      force_settlement_id_type settle_id4 { *result4.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 434 );

      force_settlement_id_type settle_id5 { *result5.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20); // 162 + 300 - 3 - 434 - 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 8); // 6 + 2
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999989); // 99999992 - 3
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500); // 800 - 300

      BOOST_CHECK_EQUAL( 962, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 99, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).debt.value ); // 6 + 2
      BOOST_CHECK_EQUAL( 11, call_michael_id(db).collateral.value ); // 8 + 3

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 970 ); // 968 + 2

      generate_blocks( db.head_block_time() + fc::hours(4) );
      set_expiration( db, trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(22) );
      set_expiration( db, trx );

      // checks
      // maximum amount that can be settled now is round_down(970 * 20%) = 194.
      // settle_id3 (amount was 3) will be filled and get nothing.
      // settle_id4 will pay 194 - 3 = 191 usd, will get round_down(191*5/101) = 9 core
      BOOST_CHECK( !db.find( settle_id3 ) );
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 243 ); // 434 - 191
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10); // 1 + 9
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 8);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999989);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 768, call_paul_id(db).debt.value ); // 962 - 3 - 191
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value ); // 99 - 9
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 11, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 776 ); // 970 - 3 - 191
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 194 ); // 3 + 191

      generate_block();

      // michael borrows more
      set_expiration( db, trx );
      borrow(michael_id(db), bitusd_id(db).amount(18), core_id(db).amount(200));

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 243 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 26); // 8 + 18
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999789); // 99999989 - 200
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 768, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(db).debt.value ); // 8 + 18
      BOOST_CHECK_EQUAL( 211, call_michael_id(db).collateral.value ); // 11 + 200

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 794 ); // 776 + 18
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 194 );

      generate_block();

      // maximum amount that can be settled now is round_down((794+194) * 20%) = 197,
      //   already settled 194, so 197 - 194 = 3 more usd can be settled,
      //   so settle_id3 will pay 3 usd and get nothing
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 240 ); // 243 - 3
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 26);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999789);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 765, call_paul_id(db).debt.value ); // 768 - 3
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 211, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 791 ); // 794 - 3
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 197 ); // 194 + 3

      // michael borrows a little more
      set_expiration( db, trx );
      borrow(michael_id(db), bitusd_id(db).amount(20), core_id(db).amount(20));

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 240 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46); // 26 + 20
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769); // 99999789 - 20
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 765, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value ); // 26 + 20
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value ); // 211 + 20

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 811 ); // 791 + 20
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 197 );

      generate_block();

      // maximum amount that can be settled now is round_down((811+197) * 20%) = 201,
      //   already settled 197, so 201 - 197 = 4 more usd can be settled,
      //   so settle_id4 will pay 4 usd and get nothing

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 236 ); // 240 - 4
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 761, call_paul_id(db).debt.value ); // 765 - 4
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 807 ); // 811 - 4
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 201 ); // 197 + 4

      generate_block();

      // jim borrow some cny
      call_order_id_type call_jim_id = borrow(jim_id(db), bitcny_id(db).amount(2000), core_id(db).amount(2000))
                                          ->get_id();

      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).collateral.value );

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 2000);

      // jim transfer some cny to joe
      transfer(jim_id, joe_id, asset(1500, bitcny_id));

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 1500);

      generate_block();

      // give ted some usd
      transfer(paul_id, ted_id, asset(100, bitusd_id));
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 100); // new: 100
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400); // 500 - 100

      // ted settle
      const operation_result result6 = force_settle(ted_id(db), bitusd_id(db).amount(20));
      const operation_result result7 = force_settle(ted_id(db), bitusd_id(db).amount(21));
      const operation_result result8 = force_settle(ted_id(db), bitusd_id(db).amount(22));

      force_settlement_id_type settle_id6 { *result6.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 );

      force_settlement_id_type settle_id7 { *result7.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 );

      force_settlement_id_type settle_id8 { *result8.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 );

      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37); // 100 - 20 - 21 - 22

      // joe settle
      const operation_result result101 = force_settle(joe_id(db), bitcny_id(db).amount(100));
      const operation_result result102 = force_settle(joe_id(db), bitcny_id(db).amount(1000));
      const operation_result result103 = force_settle(joe_id(db), bitcny_id(db).amount(300));

      force_settlement_id_type settle_id101 { *result101.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id101(db).balance.amount.value, 100 );

      force_settlement_id_type settle_id102 { *result102.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 1000 );

      force_settlement_id_type settle_id103 { *result103.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 );

      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100); // 1500 - 100 - 1000 - 300

      generate_block();

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // get to another maintenance interval
      generate_blocks( db.head_block_time() + fc::hours(22) );
      set_expiration( db, trx );

      // maximum amount that can be settled now is round_down(807 * 20%) = 161,
      // settle_id4 will pay 161 usd, will get round_down(161*5/101) = 7 core
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 75 ); // 236 - 161
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 ); // no change since not expired

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 17); // 10 + 7
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37);

      BOOST_CHECK_EQUAL( 600, call_paul_id(db).debt.value ); // 761 - 161
      BOOST_CHECK_EQUAL( 83, call_paul_id(db).collateral.value ); // 90 - 7
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 646 ); // 807 - 161
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 161 ); // reset to 0, then 161 more

      // current cny data
      BOOST_CHECK_EQUAL( settle_id101(db).balance.amount.value, 100 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 1000 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since not expired

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100); // 1500 - 100 - 1000 - 300

      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 2000 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 0 );

      // bob borrow some
      const call_order_object& call_bob = *borrow( bob_id(db), bitusd_id(db).amount(19), core_id(db).amount(2) );
      call_order_id_type call_bob_id = call_bob.get_id();

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 9999998); // 10000000 - 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19); // new

      BOOST_CHECK_EQUAL( 19, call_bob_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 665 ); // 646 + 19
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 161 );

      generate_block();

      // maximum amount that can be settled now is round_down((665+161) * 20%) = 165,
      // settle_id4 will pay 165-161=4 usd, will get nothing
      // bob's call order will get partially settled since its collateral ratio is the lowest
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 71 ); // 75 - 4
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 ); // no change since not expired

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 9999998);
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 17); // no change
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37);

      BOOST_CHECK_EQUAL( 15, call_bob_id(db).debt.value ); // 19 - 4
      BOOST_CHECK_EQUAL( 2, call_bob_id(db).collateral.value ); // no change
      BOOST_CHECK_EQUAL( 600, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 83, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 661 ); // 665 - 4
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 165 ); // 161 + 4

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // generate some blocks
      generate_blocks( db.head_block_time() + fc::hours(10) );
      set_expiration( db, trx );

      // check cny
      // maximum amount that can be settled now is round_down(2000 * 20%) = 400,
      //   settle_id101's remaining amount is 100, so it can be fully processed,
      //      according to price 50 core / 101 cny, it will get 49 core and pay 100 cny;
      //   settle_id102's remaining amount is 1000, so 400-100=300 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 148 core and pay 300 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK( !db.find( settle_id101 ) );
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 700 ); // 1000 - 300
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since it's after settle_id102

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 197); // 49 + 148
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100);

      BOOST_CHECK_EQUAL( 1600, call_jim_id(db).debt.value ); // 2000 - 100 - 300
      BOOST_CHECK_EQUAL( 1803, call_jim_id(db).collateral.value ); // 2000 - 49 - 148

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 1600 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 400 ); // 100 + 300

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // get to another maintenance interval
      generate_blocks( db.head_block_time() + fc::hours(14) );
      set_expiration( db, trx );

      // maximum amount that can be settled now is round_down(661 * 20%) = 132,
      //   settle_id4's remaining amount is 71,
      //      firstly it will pay 15 usd to call_bob and get nothing,
      //        call_bob will pay off all debt, so it will be closed and remaining collateral (2 core) will be returned;
      //      then it will pay 71-15=56 usd to call_paul and get round_down(56*5/101) = 2 core;
      //   settle_id5 (has 5 usd) will pay 5 usd and get nothing;
      //   settle_id6 (has 20 usd) will pay 20 usd and get nothing;
      //   settle_id7 (has 21 usd) will pay 21 usd and get 1 core;
      //   settle_id8 (has 22 usd) will pay 15 usd and get nothing, since reached 132
      BOOST_CHECK( !db.find( settle_id4 ) );
      BOOST_CHECK( !db.find( settle_id5 ) );
      BOOST_CHECK( !db.find( settle_id6 ) );
      BOOST_CHECK( !db.find( settle_id7 ) );
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 7 ); // 22 - 15

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 10000000); // 9999998 + 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 19); // 17 + 2
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 1); // 0 + 1
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37);

      BOOST_CHECK( !db.find( call_bob_id ) );
      BOOST_CHECK_EQUAL( 483, call_paul_id(db).debt.value ); // 600 - 56 - 5 - 20 - 21 - 15
      BOOST_CHECK_EQUAL( 80, call_paul_id(db).collateral.value ); // 83 - 2 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 529 ); // 661 - 132
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 132 ); // reset to 0, then 132 more

      // check cny
      // maximum amount that can be settled now is round_down(1600 * 20%) = 320,
      //   settle_id102's remaining amount is 700, so 320 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 158 core and pay 320 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK( !db.find( settle_id101 ) );
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 380 ); // 700 - 320
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since it's after settle_id102

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 355); // 197 + 158
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100);

      BOOST_CHECK_EQUAL( 1280, call_jim_id(db).debt.value ); // 1600 - 320
      BOOST_CHECK_EQUAL( 1645, call_jim_id(db).collateral.value ); // 1803 - 158

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 1280 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 320 ); // reset to 0, then 320

      generate_block();

      // Note: the scenario that a big settle order matching several smaller call orders,
      //       and another scenario about force_settlement_offset_percent parameter,
      //       are tested in force_settle_test in operation_test2.cpp.

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( settle_rounding_test_after_hf_184 )
{
   try {
      auto mi = db.get_global_properties().parameters.maintenance_interval;

      if(hf2481)
         generate_blocks(HARDFORK_CORE_2481_TIME - mi);
      else if(hf1270)
         generate_blocks(HARDFORK_CORE_1270_TIME - mi);
      else
         generate_blocks(HARDFORK_CORE_184_TIME - mi);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db, trx );

      ACTORS((paul)(michael)(rachel)(alice)(bob)(ted)(joe)(jim));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& bitcny = create_bitasset("CNYBIT", paul_id);
      const auto& core   = asset_id_type()(db);
      asset_id_type bitusd_id = bitusd.get_id();
      asset_id_type bitcny_id = bitcny.get_id();
      asset_id_type core_id = core.get_id();

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id, asset(10000000));
      transfer(committee_account, alice_id, asset(10000000));
      transfer(committee_account, bob_id, asset(10000000));
      transfer(committee_account, jim_id, asset(10000000));

      // add a feed to asset
      update_feed_producers( bitusd, {paul.get_id()} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(5);
      publish_feed( bitusd, paul, current_feed );

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul, bitusd.amount(1000), core.amount(100) );
      call_order_id_type call_paul_id = call_paul.get_id();
      BOOST_REQUIRE_EQUAL( get_balance( paul, bitusd ), 1000 );

      // and transfer some to rachel
      transfer(paul.get_id(), rachel.get_id(), asset(200, bitusd.get_id()));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 200);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 100000000);

      // michael gets some bitusd
      const call_order_object& call_michael = *borrow(michael, bitusd.amount(6), core.amount(8));
      call_order_id_type call_michael_id = call_michael.get_id();

      // add settle order and check rounding issue
      const operation_result result = force_settle(rachel, bitusd.amount(4));

      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 4 );

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 196);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 6);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul, core), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul, bitusd), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul.debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul.collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael.debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael.collateral.value );

      generate_blocks( db.head_block_time() + fc::hours(20) );
      set_expiration( db, trx );

      // default feed and settlement expires at the same time
      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(6) );

      // checks
      BOOST_CHECK( !db.find( settle_id ) );
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200); // rachel's settle order is cancelled and he get refunded
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1006 ); // 1000 + 6

      // settle more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test less than it
      set_expiration( db, trx );
      const operation_result result2 = force_settle(rachel_id(db), bitusd_id(db).amount(34));

      force_settlement_id_type settle_id2 { *result2.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id2(db).balance.amount.value, 34 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 166); // 200-34
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      generate_blocks( db.head_block_time() + fc::hours(10) );
      set_expiration( db, trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(16) );
      set_expiration( db, trx );

      // checks
      BOOST_CHECK( !db.find( settle_id2 ) );
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1); // rachel got 1 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 179); // paid 21 usd since 1 core worths a little more than 20 usd
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 800);

      BOOST_CHECK_EQUAL( 979, call_paul_id(db).debt.value ); // 1000 - 21
      BOOST_CHECK_EQUAL( 99, call_paul_id(db).collateral.value ); // 100 - 1
      BOOST_CHECK_EQUAL( 6, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 985 ); // 1006 - 21

      // prepare for more tests
      transfer(paul_id, rachel_id, asset(300, bitusd_id));
      borrow(michael_id(db), bitusd_id(db).amount(2), core_id(db).amount(3));

      // settle even more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test more than it
      const operation_result result3 = force_settle(rachel_id(db), bitusd_id(db).amount(3));
      const operation_result result4 = force_settle(rachel_id(db), bitusd_id(db).amount(434));
      const operation_result result5 = force_settle(rachel_id(db), bitusd_id(db).amount(5));

      force_settlement_id_type settle_id3 { *result3.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id3(db).balance.amount.value, 3 );

      force_settlement_id_type settle_id4 { *result4.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 434 );

      force_settlement_id_type settle_id5 { *result5.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 37); // 179 + 300 - 3 - 434 - 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 8); // 6 + 2
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999989); // 99999992 - 3
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500); // 800 - 300

      BOOST_CHECK_EQUAL( 979, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 99, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).debt.value ); // 6 + 2
      BOOST_CHECK_EQUAL( 11, call_michael_id(db).collateral.value ); // 8 + 3

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 987 ); // 985 + 2

      generate_blocks( db.head_block_time() + fc::hours(4) );
      set_expiration( db, trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // now yes expire settlement
      generate_blocks( db.head_block_time() + fc::hours(22) );
      set_expiration( db, trx );

      // checks
      // settle_id3 will be cancelled due to too small.
      // maximum amount that can be settled now is round_down(987 * 20%) = 197,
      //   according to price (101/5), the amount worths more than 9 core but less than 10 core, so 9 core will be settled,
      //   and 9 core worths 181.5 usd, so rachel will pay 182 usd and get 9 core
      BOOST_CHECK( !db.find( settle_id3 ) );
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 252 ); // 434 - 182
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10); // 1 + 9
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40); // 37 + 3
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 8);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999989);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(db).debt.value ); // 979 - 182
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value ); // 99 - 9
      BOOST_CHECK_EQUAL( 8, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 11, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 805 ); // 987 - 182
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 182 );

      generate_block();

      // michael borrows more
      set_expiration( db, trx );
      borrow(michael_id(db), bitusd_id(db).amount(18), core_id(db).amount(200));

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 252 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 26); // 8 + 18
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999789); // 99999989 - 200
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(db).debt.value ); // 8 + 18
      BOOST_CHECK_EQUAL( 211, call_michael_id(db).collateral.value ); // 11 + 200

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 823 ); // 805 + 18
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 182 );

      generate_block();

      // maximum amount that can be settled now is round_down((823+182) * 20%) = 201,
      //   already settled 182, so 201 - 182 = 19 more usd can be settled,
      //   according to price (101/5), the amount worths less than 1 core,
      //   so nothing will happen.
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 252 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 26);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999789);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 211, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 823 );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 182 );

      // michael borrows a little more
      set_expiration( db, trx );
      borrow(michael_id(db), bitusd_id(db).amount(20), core_id(db).amount(20));

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 252 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46); // 26 + 20
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769); // 99999789 - 20
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value ); // 26 + 20
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value ); // 211 + 20

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 843 ); // 823 + 20
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 182 );

      generate_block();

      // maximum amount that can be settled now is round_down((843+182) * 20%) = 205,
      //   already settled 182, so 205 - 182 = 23 more usd can be settled,
      //   according to price (101/5), the amount worths more than 1 core but less than 2 core,
      //   so settle order will fill 1 more core, since 1 core worth more than 20 usd but less than 21 usd,
      //   so rachel will pay 21 usd and get 1 core

      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 231 ); // 252 - 21
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 11); // 10 + 1
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 500);

      BOOST_CHECK_EQUAL( 776, call_paul_id(db).debt.value ); // 797 - 21
      BOOST_CHECK_EQUAL( 89, call_paul_id(db).collateral.value ); // 90 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 822 ); // 843 - 21
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 203 ); // 182 + 21

      // jim borrow some cny
      call_order_id_type call_jim_id = borrow(jim_id(db), bitcny_id(db).amount(2000), core_id(db).amount(2000))
                                          ->get_id();

      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).collateral.value );

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 2000);

      // jim transfer some cny to joe
      transfer(jim_id, joe_id, asset(1500, bitcny_id));

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 1500);

      generate_block();

      // give ted some usd
      transfer(paul_id, ted_id, asset(100, bitusd_id));
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 100); // new: 100
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400); // 500 - 100

      // ted settle
      const operation_result result6 = force_settle(ted_id(db), bitusd_id(db).amount(20));
      const operation_result result7 = force_settle(ted_id(db), bitusd_id(db).amount(21));
      const operation_result result8 = force_settle(ted_id(db), bitusd_id(db).amount(22));

      force_settlement_id_type settle_id6 { *result6.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 );

      force_settlement_id_type settle_id7 { *result7.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 );

      force_settlement_id_type settle_id8 { *result8.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 );

      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37); // 100 - 20 - 21 - 22

      // joe settle
      const operation_result result101 = force_settle(joe_id(db), bitcny_id(db).amount(100));
      const operation_result result102 = force_settle(joe_id(db), bitcny_id(db).amount(1000));
      const operation_result result103 = force_settle(joe_id(db), bitcny_id(db).amount(300));

      force_settlement_id_type settle_id101 { *result101.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id101(db).balance.amount.value, 100 );

      force_settlement_id_type settle_id102 { *result102.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 1000 );

      force_settlement_id_type settle_id103 { *result103.get<extendable_operation_result>()
                                                        .value.new_objects->begin() };
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 );

      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100); // 1500 - 100 - 1000 - 300

      generate_block();

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // get to another maintenance interval
      generate_blocks( db.head_block_time() + fc::hours(22) );
      set_expiration( db, trx );

      // maximum amount that can be settled now is round_down(822 * 20%) = 164,
      //   according to price (101/5), the amount worths more than 8 core but less than 9 core,
      //   so settle order will fill 8 more core, since 8 core worth more than 161 usd but less than 162 usd,
      //   so rachel will pay 162 usd and get 8 core
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 69 ); // 231 - 162
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 ); // no change, since it's after settle_id4
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 ); // no change since not expired

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 19); // 11 + 8
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37);

      BOOST_CHECK_EQUAL( 614, call_paul_id(db).debt.value ); // 776 - 162
      BOOST_CHECK_EQUAL( 81, call_paul_id(db).collateral.value ); // 89 - 8
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 660 ); // 822 - 162
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 162 ); // reset to 0, then 162 more

      // current cny data
      BOOST_CHECK_EQUAL( settle_id101(db).balance.amount.value, 100 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 1000 ); // no change since not expired
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since not expired

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 100); // 1500 - 100 - 1000 - 300

      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 2000 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 0 );

      // bob borrow some
      const call_order_object& call_bob = *borrow( bob_id(db), bitusd_id(db).amount(19), core_id(db).amount(2) );
      call_order_id_type call_bob_id = call_bob.get_id();

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 9999998); // 10000000 - 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19); // new

      BOOST_CHECK_EQUAL( 19, call_bob_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 679 ); // 660 + 19
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 162 );

      generate_block();

      // maximum amount that can be settled now is round_down((679+162) * 20%) = 168,
      //   already settled 162, so 168 - 162 = 6 more usd can be settled,
      //   according to price (101/5), the amount worths less than 1 core,
      //   so nothing will happen.
      BOOST_CHECK_EQUAL( settle_id4(db).balance.amount.value, 69 );
      BOOST_CHECK_EQUAL( settle_id5(db).balance.amount.value, 5 );
      BOOST_CHECK_EQUAL( settle_id6(db).balance.amount.value, 20 );
      BOOST_CHECK_EQUAL( settle_id7(db).balance.amount.value, 21 );
      BOOST_CHECK_EQUAL( settle_id8(db).balance.amount.value, 22 );

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 9999998);
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 37);

      BOOST_CHECK_EQUAL( 19, call_bob_id(db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 614, call_paul_id(db).debt.value );
      BOOST_CHECK_EQUAL( 81, call_paul_id(db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 679 );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 162 );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // generate some blocks
      generate_blocks( db.head_block_time() + fc::hours(10) );
      set_expiration( db, trx );

      // check cny
      // maximum amount that can be settled now is round_down(2000 * 20%) = 400,
      //   settle_id101's remaining amount is 100, so it can be fully processed,
      //      according to price 50 core / 101 cny, it will get 49 core and pay 99 cny, the rest (1 cny) will be refunded;
      //   settle_id102's remaining amount is 1000, so 400-99=301 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 149 core and pay 301 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK( !db.find( settle_id101 ) );
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 699 ); // 1000 - 301
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since it's after settle_id102

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 198); // 49 + 149
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 101); // 100 + 1

      BOOST_CHECK_EQUAL( 1600, call_jim_id(db).debt.value ); // 2000 - 99 - 301
      BOOST_CHECK_EQUAL( 1802, call_jim_id(db).collateral.value ); // 2000 - 49 - 149

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 1600 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 400 ); // 99 + 301

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 101 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), alice_id(db), current_feed );

      update_feed_producers( bitcny_id(db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(db).amount( 101 ) / core_id(db).amount(50);
      publish_feed( bitcny_id(db), alice_id(db), current_feed );

      // get to another maintenance interval
      generate_blocks( db.head_block_time() + fc::hours(14) );
      set_expiration( db, trx );

      // maximum amount that can be settled now is round_down(679 * 20%) = 135,
      //   settle_id4's remaining amount is 69, so it can be fully processed:
      //     firstly call_bob will be matched, since it owes only 19 usd which worths less than 1 core,
      //       it will pay 1 core, and the rest (2-1=1 core) will be returned, short position will be closed;
      //     then call_paul will be matched,
      //       according to price (101/5), the amount (69-19=50 usd) worths more than 2 core but less than 3 core,
      //       so settle_id4 will get 2 more core, since 2 core worth more than 40 usd but less than 41 usd,
      //       call_rachel will pay 41 usd and get 2 core, the rest (50-41=9 usd) will be returned due to too small.
      //   settle_id5 (has 5 usd) will be cancelled due to too small;
      //   settle_id6 (has 20 usd) will be cancelled as well due to too small;
      //   settle_id7 (has 21 usd) will be filled and get 1 core, since it worths more than 1 core; but no more fund can be returned;
      //   settle_id8 (has 22 usd) will be filled and get 1 core, and 1 usd will be returned.
      BOOST_CHECK( !db.find( settle_id4 ) );
      BOOST_CHECK( !db.find( settle_id5 ) );
      BOOST_CHECK( !db.find( settle_id6 ) );
      BOOST_CHECK( !db.find( settle_id7 ) );
      BOOST_CHECK( !db.find( settle_id8 ) );

      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core_id(db)), 9999999); // 9999998 + 1
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), bitusd_id(db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 22); // 19 + 1 + 2
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 54); // 40 + 9 + 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), core_id(db)), 2); // 0 + 1 + 1
      BOOST_CHECK_EQUAL(get_balance(ted_id(db), bitusd_id(db)), 58); // 37 + 20 + 1

      BOOST_CHECK( !db.find( call_bob_id ) );
      BOOST_CHECK_EQUAL( 531, call_paul_id(db).debt.value ); // 614 - 41 - 21 - 21
      BOOST_CHECK_EQUAL( 77, call_paul_id(db).collateral.value ); // 81 - 2 - 1 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 577 ); // 679 - 19 - 41 - 21 - 21
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).force_settled_volume.value, 102 ); // reset to 0, then 19 + 41 + 21 + 21

      // check cny
      // maximum amount that can be settled now is round_down(1600 * 20%) = 320,
      //   settle_id102's remaining amount is 699, so 320 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 158 core and pay 320 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK( !db.find( settle_id101 ) );
      BOOST_CHECK_EQUAL( settle_id102(db).balance.amount.value, 379 ); // 699 - 320
      BOOST_CHECK_EQUAL( settle_id103(db).balance.amount.value, 300 ); // no change since it's after settle_id102

      BOOST_CHECK_EQUAL(get_balance(jim_id(db), core_id(db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(db), bitcny_id(db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), core_id(db)), 356); // 198 + 158
      BOOST_CHECK_EQUAL(get_balance(joe_id(db), bitcny_id(db)), 101);

      BOOST_CHECK_EQUAL( 1280, call_jim_id(db).debt.value ); // 1600 - 320
      BOOST_CHECK_EQUAL( 1644, call_jim_id(db).collateral.value ); // 1802 - 158

      BOOST_CHECK_EQUAL( bitcny_id(db).dynamic_data(db).current_supply.value, 1280 );
      BOOST_CHECK_EQUAL( bitcny_id(db).bitasset_data(db).force_settled_volume.value, 320 ); // reset to 0, then 320

      generate_block();

      // Note: the scenario that a big settle order matching several smaller call orders,
      //       and another scenario about force_settlement_offset_percent parameter,
      //       are tested in force_settle_test in operation_test2.cpp.

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_settle_rounding_test )
{
   try {
      // get around Graphene issue #615 feed expiration bug
      generate_blocks(HARDFORK_615_TIME);
      generate_block();
      set_expiration( db, trx );

      ACTORS((paul)(michael)(rachel)(alice));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& core   = asset_id_type()(db);
      asset_id_type bitusd_id = bitusd.get_id();
      asset_id_type core_id = core.get_id();

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id,    asset(  10000000 ) );
      transfer(committee_account, alice_id,   asset(  10000000 ) );

      // allow global settle in bitusd
      asset_update_operation op;
      op.issuer = bitusd.issuer;
      op.asset_to_update = bitusd.id;
      op.new_options.issuer_permissions = global_settle;
      op.new_options.flags = bitusd.options.flags;
      op.new_options.core_exchange_rate = price( asset(1,bitusd_id), asset(1,core_id) );
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db, trx);
      generate_block();
      trx.clear();

      // add a feed to asset
      update_feed_producers( bitusd_id(db), {paul_id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 100 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), paul_id(db), current_feed );

      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 10000000);

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul_id(db), bitusd_id(db).amount(1001), core_id(db).amount(101));
      call_order_id_type call_paul_id = call_paul.get_id();
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(db), bitusd_id(db) ), 1001 );
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(db), core_id(db) ), 10000000-101);

      // and transfer some to rachel
      transfer(paul_id, rachel_id, asset(200, bitusd_id));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // michael borrow some bitusd
      const call_order_object& call_michael = *borrow(michael_id(db), bitusd_id(db).amount(6), core_id(db).amount(8));
      call_order_id_type call_michael_id = call_michael.get_id();

      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000-8);

      // add global settle
      force_global_settle(bitusd_id(db), bitusd_id(db).amount(10) / core_id(db).amount(1));
      generate_block();

      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 100 ); // 100 from paul, and 0 from michael
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000); // michael paid nothing for 6 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900); // paul paid 100 core for 1001 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // all call orders are gone after global settle
      BOOST_CHECK( !db.find(call_paul_id) );
      BOOST_CHECK( !db.find(call_michael_id) );

      // add settle order and check rounding issue
      force_settle(rachel_id(db), bitusd_id(db).amount(4));
      generate_block();

      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 100 ); // paid nothing
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1003 ); // settled 4 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 196); // rachel paid 4 usd and got nothing
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // rachel settle more than 1 core
      force_settle(rachel_id(db), bitusd_id(db).amount(13));
      generate_block();

      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 99 ); // paid 1 core
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 990 ); // settled 13 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 183); // rachel paid 13 usd and got 1 core
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_settle_rounding_test_after_hf_184 )
{
   try {
      auto mi = db.get_global_properties().parameters.maintenance_interval;

      if(hf2481)
         generate_blocks(HARDFORK_CORE_2481_TIME - mi);
      else if(hf1270)
         generate_blocks(HARDFORK_CORE_1270_TIME - mi);
      else
         generate_blocks(HARDFORK_CORE_184_TIME - mi); // assume that hf core-184 and core-342 happen at same time

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db, trx );

      ACTORS((paul)(michael)(rachel)(alice));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& core   = asset_id_type()(db);
      asset_id_type bitusd_id = bitusd.get_id();
      asset_id_type core_id = core.get_id();

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id,    asset(  10000000 ) );
      transfer(committee_account, alice_id,   asset(  10000000 ) );

      // allow global settle in bitusd
      asset_update_operation op;
      op.issuer = bitusd_id(db).issuer;
      op.asset_to_update = bitusd_id;
      op.new_options.issuer_permissions = global_settle;
      op.new_options.flags = bitusd.options.flags;
      op.new_options.core_exchange_rate = price( asset(1,bitusd_id), asset(1,core_id) );
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db, trx);
      generate_block();
      trx.clear();

      // add a feed to asset
      update_feed_producers( bitusd_id(db), {paul_id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(db).amount( 100 ) / core_id(db).amount(5);
      publish_feed( bitusd_id(db), paul_id(db), current_feed );

      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 10000000);

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul_id(db), bitusd_id(db).amount(1001), core_id(db).amount(101));
      call_order_id_type call_paul_id = call_paul.get_id();
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(db), bitusd_id(db) ), 1001 );
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(db), core_id(db) ), 10000000-101);

      // and transfer some to rachel
      transfer(paul_id, rachel_id, asset(200, bitusd_id));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // michael borrow some bitusd
      const call_order_object& call_michael = *borrow(michael_id(db), bitusd_id(db).amount(6), core_id(db).amount(8));
      call_order_id_type call_michael_id = call_michael.get_id();

      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 100000000-8);

      // add global settle
      force_global_settle(bitusd_id(db), bitusd_id(db).amount(10) / core_id(db).amount(1));
      generate_block();

      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 102 ); // 101 from paul, and 1 from michael
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999999); // michael paid 1 core for 6 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999899); // paul paid 101 core for 1001 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // all call orders are gone after global settle
      BOOST_CHECK( !db.find(call_paul_id));
      BOOST_CHECK( !db.find(call_michael_id));

      // settle order will not execute after HF due to too small
      GRAPHENE_REQUIRE_THROW( force_settle(rachel_id(db), bitusd_id(db).amount(4)), fc::exception );

      generate_block();

      // balances unchanged
      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 102 );
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999999);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);

      // rachel settle more than 1 core
      force_settle(rachel_id(db), bitusd_id(db).amount(13));
      generate_block();

      BOOST_CHECK( bitusd_id(db).bitasset_data(db).settlement_price
                   == price( bitusd_id(db).amount(1007), core_id(db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(db).bitasset_data(db).settlement_fund.value, 101 ); // paid 1 core
      BOOST_CHECK_EQUAL( bitusd_id(db).dynamic_data(db).current_supply.value, 997 ); // settled 10 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), core_id(db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(db), bitusd_id(db)), 190); // rachel paid 10 usd and got 1 core, 3 usd returned
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), bitusd_id(db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(db), core_id(db)), 99999999);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), core_id(db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(db), bitusd_id(db)), 801);


   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_bitassets )
{
   try {

      generate_blocks( HARDFORK_480_TIME ); // avoid being affected by the price feed bug
      generate_block();
      set_expiration( db, trx );

      ACTORS((paul)(rachelregistrar)(rachelreferrer));

      upgrade_to_lifetime_member(rachelregistrar);
      upgrade_to_lifetime_member(rachelreferrer);

      constexpr auto market_fee_percent      = 50 * GRAPHENE_1_PERCENT;
      constexpr auto biteur_reward_percent   = 90 * GRAPHENE_1_PERCENT;
      constexpr auto referrer_reward_percent = 10 * GRAPHENE_1_PERCENT;

      const auto& biteur = create_bitasset( "EURBIT", paul_id, market_fee_percent, charge_market_fee, 2 );
      asset_id_type biteur_id = biteur.get_id();

      const auto& bitusd = create_bitasset( "USDBIT", paul_id, market_fee_percent, charge_market_fee, 2, biteur_id );

      const account_object rachel  = create_account( "rachel", rachelregistrar, rachelreferrer,
                                                     referrer_reward_percent );

      transfer( committee_account, rachelregistrar_id, asset( 10000000 ) );
      transfer( committee_account, rachelreferrer_id, asset( 10000000 ) );
      transfer( committee_account, rachel.get_id(), asset( 10000000) );
      transfer( committee_account, paul_id, asset( 10000000000LL ) );

      asset_update_operation op;
      op.issuer = biteur.issuer;
      op.asset_to_update = biteur_id;
      op.new_options.issuer_permissions = charge_market_fee;
      op.new_options.extensions.value.reward_percent = biteur_reward_percent;
      op.new_options.flags = bitusd.options.flags | charge_market_fee;
      op.new_options.core_exchange_rate = price( asset(20,biteur_id), asset(1,asset_id_type()) );
      op.new_options.market_fee_percent = market_fee_percent;
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db, trx);
      generate_block();
      trx.clear();
      set_expiration( db, trx );
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( market_fee_of_settle_order_before_hardfork_1780 )
{
   try {
      INVOKE(create_bitassets);

      GET_ACTOR(paul);
      GET_ACTOR(rachel);
      GET_ACTOR(rachelregistrar);
      GET_ACTOR(rachelreferrer);

      const asset_object &biteur = get_asset( "EURBIT" );
      asset_id_type biteur_id = biteur.get_id();
      const asset_object &bitusd = get_asset( "USDBIT" );
      asset_id_type bitusd_id = bitusd.get_id();

      const auto& core = asset_id_type()(db);

      {// add a feed to asset bitusd
         update_feed_producers( bitusd, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( bitusd.amount(100), biteur.amount(5) );
         feed.core_exchange_rate = price( bitusd.amount(100), asset(1) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( bitusd_id, paul_id, feed );
      }

      {// add a feed to asset biteur
         update_feed_producers( biteur, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( biteur.amount(100), core.amount(5) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( biteur_id, paul_id, feed );
      }

      enable_fees();

      // paul gets some bitusd and biteur
      borrow( paul_id, biteur.amount(20000), core.amount(2000) );
      borrow( paul_id, bitusd.amount(10000), biteur.amount(1000) );

      // and transfer some bitusd to rachel
      constexpr auto rachel_bitusd_count = 1000;
      transfer( paul_id, rachel_id, asset(rachel_bitusd_count, bitusd_id) );

      force_settle( rachel, bitusd.amount(rachel_bitusd_count) );
      generate_block();
      generate_blocks( db.head_block_time() + fc::hours(24) );
      set_expiration( db, trx );

      // Check results
      int64_t biteur_balance = 0;
      int64_t biteur_accumulated_fee = 0;
      int64_t bitusd_accumulated_fee = 0;
      {
         // 1 biteur = 20 bitusd see publish_feed
         const auto biteur_expected_result = rachel_bitusd_count/20;
         const auto biteur_market_fee = biteur_expected_result / 2; // market fee percent = 50%
         biteur_balance += biteur_expected_result - biteur_market_fee;

         BOOST_CHECK_EQUAL( get_balance(rachel, biteur), biteur_balance );
         BOOST_CHECK_EQUAL( get_balance(rachel, bitusd), 0 );

         const auto rachelregistrar_reward = get_market_fee_reward( rachelregistrar, biteur );
         const auto rachelreferrer_reward = get_market_fee_reward( rachelreferrer, biteur );

         BOOST_CHECK_EQUAL( rachelregistrar_reward, 0 );
         BOOST_CHECK_EQUAL( rachelreferrer_reward, 0 );

         // market fee
         biteur_accumulated_fee += biteur_market_fee;
         bitusd_accumulated_fee += 0; // usd market fee percent 50%, but call orders don't pay
         BOOST_CHECK_EQUAL( biteur.dynamic_data(db).accumulated_fees.value, biteur_accumulated_fee);
         BOOST_CHECK_EQUAL( bitusd.dynamic_data(db).accumulated_fees.value, bitusd_accumulated_fee );
      }

      // Update the feed to asset bitusd to trigger a global settlement
      {
         price_feed feed;
         feed.settlement_price = price( bitusd.amount(10), biteur.amount(5) );
         feed.core_exchange_rate = price( bitusd.amount(100), asset(1) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( bitusd_id, paul_id, feed );
      }

      // Transfer more bitusd to rachel
      transfer( paul_id, rachel_id, asset(rachel_bitusd_count, bitusd_id) );
      // Instant settlement
      force_settle( rachel, bitusd.amount(rachel_bitusd_count) );

      // Check results
      {
         // 950 biteur = 9000 bitusd in settlement fund
         const auto biteur_expected_result = rachel_bitusd_count * 950 / 9000;
         const auto biteur_market_fee = 0; // market fee percent = 50%, but doesn't pay before hf
         biteur_balance += biteur_expected_result - biteur_market_fee;

         BOOST_CHECK_EQUAL( get_balance(rachel, biteur), biteur_balance );
         BOOST_CHECK_EQUAL( get_balance(rachel, bitusd), 0 );

         const auto rachelregistrar_reward = get_market_fee_reward( rachelregistrar, biteur );
         const auto rachelreferrer_reward = get_market_fee_reward( rachelreferrer, biteur );

         BOOST_CHECK_EQUAL( rachelregistrar_reward, 0 );
         BOOST_CHECK_EQUAL( rachelreferrer_reward, 0 );

         // No market fee for instant settlement before hf
         biteur_accumulated_fee += 0;
         bitusd_accumulated_fee += 0;
         BOOST_CHECK_EQUAL( biteur.dynamic_data(db).accumulated_fees.value, biteur_accumulated_fee);
         BOOST_CHECK_EQUAL( bitusd.dynamic_data(db).accumulated_fees.value, bitusd_accumulated_fee );
      }

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( market_fee_of_settle_order_after_hardfork_1780 )
{
   try {
      INVOKE(create_bitassets);

      if(hf2481)
      {
         auto mi = db.get_global_properties().parameters.maintenance_interval;
         generate_blocks(HARDFORK_CORE_2481_TIME - mi);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      }
      else
         generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      GET_ACTOR(paul);
      GET_ACTOR(rachel);
      GET_ACTOR(rachelregistrar);
      GET_ACTOR(rachelreferrer);

      const asset_object &biteur = get_asset( "EURBIT" );
      asset_id_type biteur_id = biteur.get_id();
      const asset_object &bitusd = get_asset( "USDBIT" );
      asset_id_type bitusd_id = bitusd.get_id();

      const auto& core = asset_id_type()(db);

      {// add a feed to asset bitusd
         update_feed_producers( bitusd, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( bitusd.amount(100), biteur.amount(5) );
         feed.core_exchange_rate = price( bitusd.amount(100), asset(1) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( bitusd_id, paul_id, feed );
      }

      {// add a feed to asset biteur
         update_feed_producers( biteur, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( biteur.amount(100), core.amount(5) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( biteur_id, paul_id, feed );
      }

      enable_fees();

      // paul gets some bitusd and biteur
      borrow( paul_id, biteur.amount(20000), core.amount(2000) );
      borrow( paul_id, bitusd.amount(10000), biteur.amount(1000) );

      // and transfer some bitusd to rachel
      constexpr auto rachel_bitusd_count = 1000;
      transfer( paul_id, rachel_id, asset(rachel_bitusd_count, bitusd_id) );

      force_settle( rachel, bitusd.amount(rachel_bitusd_count) );
      generate_block();
      generate_blocks( db.head_block_time() + fc::hours(24) );
      set_expiration( db, trx );

      // Check results
      int64_t biteur_balance = 0;
      int64_t biteur_accumulated_fee = 0;
      int64_t bitusd_accumulated_fee = 0;
      {
         // 1 biteur = 20 bitusd see publish_feed
         const auto biteur_expected_result = rachel_bitusd_count/20;
         const auto biteur_market_fee = biteur_expected_result / 2; // market fee percent = 50%
         biteur_balance += biteur_expected_result - biteur_market_fee;

         BOOST_CHECK_EQUAL( get_balance(rachel, biteur), biteur_balance );
         BOOST_CHECK_EQUAL( get_balance(rachel, bitusd), 0 );

         const auto rachelregistrar_reward = get_market_fee_reward( rachelregistrar, biteur );
         const auto rachelreferrer_reward = get_market_fee_reward( rachelreferrer, biteur );

         const auto biteur_reward = biteur_market_fee * 9 / 10; // 90%
         const auto referrer_reward = biteur_reward / 10; // 10%
         const auto registrar_reward = biteur_reward - referrer_reward;

         BOOST_CHECK_EQUAL( rachelregistrar_reward, registrar_reward  );
         BOOST_CHECK_EQUAL( rachelreferrer_reward, referrer_reward );

         // market fee
         biteur_accumulated_fee += biteur_market_fee - biteur_reward;
         bitusd_accumulated_fee += 0; // usd market fee percent 50%, but call orders don't pay
         BOOST_CHECK_EQUAL( biteur.dynamic_data(db).accumulated_fees.value, biteur_accumulated_fee);
         BOOST_CHECK_EQUAL( bitusd.dynamic_data(db).accumulated_fees.value, bitusd_accumulated_fee );

      }

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( market_fee_of_instant_settle_order_after_hardfork_1780 )
{
   try {
      INVOKE(create_bitassets);

      if(hf2481)
      {
         auto mi = db.get_global_properties().parameters.maintenance_interval;
         generate_blocks(HARDFORK_CORE_2481_TIME - mi);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      }
      else
         generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      GET_ACTOR(paul);
      GET_ACTOR(rachel);
      GET_ACTOR(rachelregistrar);
      GET_ACTOR(rachelreferrer);

      const asset_object &biteur = get_asset( "EURBIT" );
      asset_id_type biteur_id = biteur.get_id();
      const asset_object &bitusd = get_asset( "USDBIT" );
      asset_id_type bitusd_id = bitusd.get_id();

      const auto& core = asset_id_type()(db);

      {// add a feed to asset bitusd
         update_feed_producers( bitusd, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( bitusd.amount(100), biteur.amount(5) );
         feed.core_exchange_rate = price( bitusd.amount(100), asset(1) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( bitusd_id, paul_id, feed );
      }

      {// add a feed to asset biteur
         update_feed_producers( biteur, {paul_id} );
         price_feed feed;
         feed.settlement_price = price( biteur.amount(100), core.amount(5) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( biteur_id, paul_id, feed );
      }

      enable_fees();

      // paul gets some bitusd and biteur
      borrow( paul_id, biteur.amount(20000), core.amount(2000) );
      borrow( paul_id, bitusd.amount(10000), biteur.amount(1000) );

      // Update the feed to asset bitusd to trigger a global settlement
      {
         price_feed feed;
         feed.settlement_price = price( bitusd.amount(10), biteur.amount(5) );
         feed.core_exchange_rate = price( bitusd.amount(100), asset(1) );
         feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         feed.maximum_short_squeeze_ratio = 110 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
         publish_feed( bitusd_id, paul_id, feed );
      }

      // Transfer some bitusd to rachel
      constexpr auto rachel_bitusd_count = 1000;
      transfer( paul_id, rachel_id, asset(rachel_bitusd_count, bitusd_id) );
      // Instant settlement
      force_settle( rachel, bitusd.amount(rachel_bitusd_count) ); // instant settlement

      // Check results
      int64_t biteur_balance = 0;
      int64_t biteur_accumulated_fee = 0;
      int64_t bitusd_accumulated_fee = 0;
      {
         // before hf2481: 1000 biteur = 10000 bitusd in settlement fund
         // after hf2481: round_up(10000/11) in settlement fund
         const auto biteur_expected_result = hf2481 ? (rachel_bitusd_count+10)/11 : rachel_bitusd_count/10;
         const auto biteur_market_fee = biteur_expected_result / 2; // market fee percent = 50%
         biteur_balance += biteur_expected_result - biteur_market_fee;

         BOOST_CHECK_EQUAL( get_balance(rachel, biteur), biteur_balance );
         BOOST_CHECK_EQUAL( get_balance(rachel, bitusd), 0 );

         const auto rachelregistrar_reward = get_market_fee_reward( rachelregistrar, biteur );
         const auto rachelreferrer_reward = get_market_fee_reward( rachelreferrer, biteur );

         const auto biteur_reward = biteur_market_fee * 9 / 10; // 90%
         const auto referrer_reward = biteur_reward / 10; // 10%
         const auto registrar_reward = biteur_reward - referrer_reward;

         BOOST_CHECK_EQUAL( rachelregistrar_reward, registrar_reward  );
         BOOST_CHECK_EQUAL( rachelreferrer_reward, referrer_reward );

         // market fee
         biteur_accumulated_fee += biteur_market_fee - biteur_reward;
         bitusd_accumulated_fee += 0; // usd market fee percent 50%, but call orders don't pay
         BOOST_CHECK_EQUAL( biteur.dynamic_data(db).accumulated_fees.value, biteur_accumulated_fee);
         BOOST_CHECK_EQUAL( bitusd.dynamic_data(db).accumulated_fees.value, bitusd_accumulated_fee );

      }

   } FC_LOG_AND_RETHROW()
}

/// Tests instant settlement:
/// * After hf core-2591, forced-settlements are filled at margin call order price (MCOP) when applicable
BOOST_AUTO_TEST_CASE( collateral_fee_of_instant_settlement_test )
{ try {

   // Advance to a recent hard fork
   generate_blocks(HARDFORK_CORE_2582_TIME);
   generate_block();

   // multiple passes,
   // i == 0 : before hf core-2591, extreme feed price
   // i == 1 : before hf core-2591, normal feed price
   // i == 2 : before hf core-2591, normal feed price, and price recovers after GS
   // i == 3 : after hf core-2591, extreme feed price
   // i == 4 : after hf core-2591, normal feed price
   // i == 5 : after hf core-2591, normal feed price, and price recovers after GS
   for( int i = 0; i < 6; ++ i )
   {
      idump( (i) );

      if( 3 == i )
      {
         // Advance to core-2591 hard fork
         generate_blocks(HARDFORK_CORE_2591_TIME);
         generate_block();
      }

      set_expiration( db, trx );

      ACTORS((sam)(feeder)(borrower)(seller));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );
      fund( borrower, asset(init_amount) );

      using bsrm_type = bitasset_options::black_swan_response_type;

      // Create asset
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMMPA";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100; // 1%
      acop.common_options.flags = charge_market_fee;
      acop.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      acop.bitasset_opts = bitasset_options();
      acop.bitasset_opts->minimum_feeds = 1;
      acop.bitasset_opts->extensions.value.margin_call_fee_ratio = 11;

      trx.operations.clear();
      trx.operations.push_back( acop );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& mpa = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type mpa_id = mpa.get_id();

      BOOST_CHECK( mpa.bitasset_data(db).get_black_swan_response_method()
                   == bsrm_type::global_settlement );

      // add a price feed publisher and publish a feed
      update_feed_producers( mpa_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,mpa_id), asset(1) );
      f.core_exchange_rate = price( asset(100,mpa_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      uint16_t feed_icr = 1900;

      publish_feed( mpa_id, feeder_id, f, feed_icr );

      BOOST_CHECK( mpa_id(db).bitasset_data(db).median_feed.settlement_price == f.settlement_price );
      BOOST_CHECK( mpa_id(db).bitasset_data(db).current_feed.settlement_price == f.settlement_price );
      BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_current_feed_price_capped() );
      BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_globally_settled() );
      BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_individually_settled_to_fund() );
      BOOST_CHECK( !db.find_settled_debt_order(mpa_id) );

      // borrowers borrow some
      // undercollateralization price = 100000:2000 * 1250:1000 = 100000:1600
      const call_order_object* call_ptr = borrow( borrower, asset(100000, mpa_id), asset(2000) );
      BOOST_REQUIRE( call_ptr );
      call_order_id_type call_id = call_ptr->get_id();

      // Transfer funds to sellers
      transfer( borrower, seller, asset(100000,mpa_id) );

      BOOST_CHECK_EQUAL( call_id(db).debt.value, 100000 );
      BOOST_CHECK_EQUAL( call_id(db).collateral.value, 2000 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // publish a new feed so that borrower's debt position is undercollateralized
      if( 0 == i || 3 == i )
         f.settlement_price = price( asset(1,mpa_id), asset(GRAPHENE_MAX_SHARE_SUPPLY) );
      else
         f.settlement_price = price( asset(100,mpa_id), asset(2) );
      publish_feed( mpa_id, feeder_id, f, feed_icr );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).median_feed.settlement_price == f.settlement_price );
      BOOST_CHECK( mpa_id(db).bitasset_data(db).is_globally_settled() );
      BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_individually_settled_to_fund() );
      BOOST_CHECK( !db.find_settled_debt_order(mpa_id) );

      // fund receives 1600
      BOOST_CHECK_EQUAL( mpa_id(db).bitasset_data(db).settlement_fund.value, 1600 );

      BOOST_CHECK( mpa_id(db).dynamic_data(db).accumulated_collateral_fees == 400 );

      BOOST_CHECK( mpa_id(db).bitasset_data(db).settlement_price
                   == price( asset(100000,mpa_id), asset(1600) ) );

      BOOST_CHECK( !db.find( call_id ) );

      if( 2 == i || 5 == i )
      {
         // price recovers
         f.settlement_price = price( asset(100,mpa_id), asset(1) );
         // call pays price  (MSSP) = 100:1 * 1000:1250 = 100000:1250 = 80
         // call match price (MCOP) = 100:1 * 1000:1239 = 100000:1239 = 80.710250202
         publish_feed( mpa_id, feeder_id, f, feed_icr );
      }

      // seller settles
      share_type amount_to_settle = 56789;
      auto result = force_settle( seller, asset(amount_to_settle, mpa_id) );
      auto op_result = result.get<extendable_operation_result>().value;

      auto check_result = [&]
      {
         BOOST_CHECK( !op_result.new_objects.valid() ); // force settlement order not created

         if( 5 == i )
         {
            // settlement fund pays = round_down(56789 * 1600 / 100000) = 908
            // seller pays = round_up(908 * 100000 / 1600) = 56750
            // settlement fund = 1600 - 908 = 692
            // settlement debt = 100000 - 56750 = 43250
            // seller would receive = round_up(56750 * 1239 / 10000 ) = 704 (<908, so ok)
            // collateral fee = 908 - 704 = 204
            BOOST_REQUIRE( op_result.paid.valid() && 1U == op_result.paid->size() );
            BOOST_CHECK( *op_result.paid->begin() == asset( 56750, mpa_id ) );
            BOOST_REQUIRE( op_result.received.valid() && 1U == op_result.received->size() );
            BOOST_CHECK( *op_result.received->begin() == asset( 704 ) );
            BOOST_REQUIRE( op_result.fees.valid() && 2U == op_result.fees->size() );
            BOOST_CHECK( *op_result.fees->begin() == asset( 204 ) );

            BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_individually_settled_to_fund() );
            BOOST_CHECK( mpa_id(db).bitasset_data(db).is_globally_settled() );
            BOOST_CHECK_EQUAL( mpa_id(db).bitasset_data(db).settlement_fund.value, 692 );

            BOOST_CHECK( mpa_id(db).dynamic_data(db).accumulated_collateral_fees == 604 ); // 400 + 204

            BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 43250 );
            BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 704 );
         }
         else
         {
            // receives = round_down(56789 * 1600 / 100000) = 908
            // pays = round_up(908 * 100000 / 1600) = 56750
            // settlement fund = 1600 - 908 = 692
            // settlement debt = 100000 - 56750 = 43250
            BOOST_REQUIRE( op_result.paid.valid() && 1U == op_result.paid->size() );
            BOOST_CHECK( *op_result.paid->begin() == asset( 56750, mpa_id ) );
            BOOST_REQUIRE( op_result.received.valid() && 1U == op_result.received->size() );
            BOOST_CHECK( *op_result.received->begin() == asset( 908 ) );
            BOOST_REQUIRE( op_result.fees.valid() && 1U == op_result.fees->size() );
            BOOST_CHECK( *op_result.fees->begin() == asset( 0 ) );

            BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_individually_settled_to_fund() );
            BOOST_CHECK( mpa_id(db).bitasset_data(db).is_globally_settled() );
            BOOST_CHECK_EQUAL( mpa_id(db).bitasset_data(db).settlement_fund.value, 692 );

            BOOST_CHECK( mpa_id(db).dynamic_data(db).accumulated_collateral_fees == 400 );

            BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 43250 );
            BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 908 );
         }

      };

      check_result();

      BOOST_TEST_MESSAGE( "Generate a block" );
      generate_block();

      check_result();

      // reset
      db.pop_block();

   } // for i

} FC_LOG_AND_RETHROW() }

/// Tests instant settlement:
/// * After hf core-2591, for prediction markets, forced-settlements are NOT filled at margin call order price (MCOP)
BOOST_AUTO_TEST_CASE( pm_instant_settlement_price_test )
{ try {

   // Advance to a recent hard fork
   generate_blocks(HARDFORK_CORE_2582_TIME);
   generate_block();

   // multiple passes,
   // i == 0 : before hf core-2591
   // i == 1 : after hf core-2591
   for( int i = 0; i < 2; ++ i )
   {
      idump( (i) );

      if( 1 == i )
      {
         // Advance to core-2591 hard fork
         generate_blocks(HARDFORK_CORE_2591_TIME);
         generate_block();
      }

      set_expiration( db, trx );

      ACTORS((judge)(alice)(feeder));

      const auto& pmark = create_prediction_market("PMARK", judge_id);
      const auto& core  = asset_id_type()(db);

      asset_id_type pm_id = pmark.get_id();

      int64_t init_balance(1000000);
      transfer(committee_account, judge_id, asset(init_balance));
      transfer(committee_account, alice_id, asset(init_balance));

      BOOST_TEST_MESSAGE( "Open position with equal collateral" );
      borrow( alice, pmark.amount(1000), asset(1000) );

      BOOST_CHECK_EQUAL( get_balance( alice_id, pm_id ), 1000 );
      BOOST_CHECK_EQUAL( get_balance( alice_id, asset_id_type() ), init_balance - 1000 );

      // add a price feed publisher and publish a feed
      update_feed_producers( pm_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,pm_id), asset(1) );
      f.core_exchange_rate = price( asset(100,pm_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      uint16_t feed_icr = 1900;

      publish_feed( pm_id, feeder_id, f, feed_icr );

      BOOST_CHECK_EQUAL( get_balance( alice_id, pm_id ), 1000 );
      BOOST_CHECK_EQUAL( get_balance( alice_id, asset_id_type() ), init_balance - 1000 );

      BOOST_TEST_MESSAGE( "Globally settling" );
      force_global_settle( pmark, pmark.amount(1) / core.amount(1) );

      BOOST_CHECK_EQUAL( get_balance( alice_id, pm_id ), 1000 );
      BOOST_CHECK_EQUAL( get_balance( alice_id, asset_id_type() ), init_balance - 1000 );

      // alice settles
      auto result = force_settle( alice, asset(300, pm_id) );
      auto op_result = result.get<extendable_operation_result>().value;

      BOOST_CHECK( !op_result.new_objects.valid() ); // force settlement order not created

      BOOST_REQUIRE( op_result.paid.valid() && 1U == op_result.paid->size() );
      BOOST_CHECK( *op_result.paid->begin() == asset( 300, pm_id ) );
      BOOST_REQUIRE( op_result.received.valid() && 1U == op_result.received->size() );
      BOOST_CHECK( *op_result.received->begin() == asset( 300 ) );
      BOOST_REQUIRE( op_result.fees.valid() && 1U == op_result.fees->size() );
      BOOST_CHECK( *op_result.fees->begin() == asset( 0 ) );

      auto check_result = [&]
      {
         BOOST_CHECK( !pm_id(db).bitasset_data(db).is_individually_settled_to_fund() );
         BOOST_CHECK( pm_id(db).bitasset_data(db).is_globally_settled() );
         BOOST_CHECK_EQUAL( pm_id(db).bitasset_data(db).settlement_fund.value, 700 );

         BOOST_CHECK_EQUAL( pm_id(db).dynamic_data(db).accumulated_collateral_fees.value, 0 );

         BOOST_CHECK_EQUAL( get_balance( alice_id, pm_id ), 700 );
         BOOST_CHECK_EQUAL( get_balance( alice_id, asset_id_type() ), init_balance - 700 );
      };

      check_result();

      BOOST_TEST_MESSAGE( "Generate a block" );
      generate_block();

      check_result();

      // reset
      db.pop_block();

   } // for i

} FC_LOG_AND_RETHROW() }

/**
 * Test case to reproduce https://github.com/bitshares/bitshares-core/issues/1883.
 * When there is only one fill_order object in the ticker rolling buffer, it should only be rolled out once.
 */
BOOST_AUTO_TEST_CASE( global_settle_ticker_test )
{
   try {
      generate_block();

      const auto& meta_idx = db.get_index_type<simple_index<graphene::market_history::market_ticker_meta_object>>();
      const auto& ticker_idx = db.get_index_type<graphene::market_history::market_ticker_index>().indices();
      const auto& history_idx = db.get_index_type<graphene::market_history::history_index>().indices();

      BOOST_CHECK_EQUAL( meta_idx.size(), 0 );
      BOOST_CHECK_EQUAL( ticker_idx.size(), 0 );
      BOOST_CHECK_EQUAL( history_idx.size(), 0 );

      ACTORS((judge)(alice));

      const auto& pmark = create_prediction_market("PMARK", judge_id);
      const auto& core  = asset_id_type()(db);

      int64_t init_balance(1000000);
      transfer(committee_account, judge_id, asset(init_balance));
      transfer(committee_account, alice_id, asset(init_balance));

      BOOST_TEST_MESSAGE( "Open position with equal collateral" );
      borrow( alice, pmark.amount(1000), asset(1000) );

      BOOST_TEST_MESSAGE( "Globally settling" );
      force_global_settle( pmark, pmark.amount(1) / core.amount(1) );

      generate_block();
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == false );

         BOOST_CHECK( tick.base_volume == 1000 );
         BOOST_CHECK( tick.quote_volume == 1000 );
      }

      generate_blocks( db.head_block_time() + 86000 ); // less than a day
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // nothing changes
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == false );

         BOOST_CHECK( tick.base_volume == 1000 );
         BOOST_CHECK( tick.quote_volume == 1000 );
      }

      generate_blocks( db.head_block_time() + 4000 ); // now more than 24 hours
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // the history is rolled out, new 24h volume should be 0
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == true ); // the order should be skipped on next roll

         BOOST_CHECK( tick.base_volume == 0 );
         BOOST_CHECK( tick.quote_volume == 0 );
      }

      generate_block();
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // nothing changes
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == true );

         BOOST_CHECK( tick.base_volume == 0 );
         BOOST_CHECK( tick.quote_volume == 0 );
      }


   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/// Tests a scenario that force settlements get cancelled on the block when the asset is globally settled
BOOST_AUTO_TEST_CASE( settle_order_cancel_due_to_gs )
{
   try {

      // Advance to a desired hard fork time
      // Note: this test doesn't apply after hf2481
      generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      ACTORS((sam)(feeder)(borrower)(seller));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );
      fund( borrower, asset(init_amount) );

      // Create asset
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMMPA";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100; // 1%
      acop.common_options.flags = charge_market_fee;
      acop.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      acop.bitasset_opts = bitasset_options();
      acop.bitasset_opts->minimum_feeds = 1;
      acop.bitasset_opts->feed_lifetime_sec = 800;
      acop.bitasset_opts->force_settlement_delay_sec = 600;

      trx.operations.clear();
      trx.operations.push_back( acop );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& mpa = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type mpa_id = mpa.get_id();

      // add a price feed publisher and publish a feed
      update_feed_producers( mpa_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,mpa_id), asset(1) );
      f.core_exchange_rate = price( asset(100,mpa_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      publish_feed( mpa_id, feeder_id, f );

      // borrowers borrow some
      // undercollateralization price = 100000:2000 * 1250:1000 = 100000:1600
      const call_order_object* call_ptr = borrow( borrower, asset(100000, mpa_id), asset(2000) );
      BOOST_REQUIRE( call_ptr );
      call_order_id_type call_id = call_ptr->get_id();

      // Transfer funds to seller
      transfer( borrower, seller, asset(100000,mpa_id) );

      BOOST_CHECK_EQUAL( call_id(db).debt.value, 100000 );
      BOOST_CHECK_EQUAL( call_id(db).collateral.value, 2000 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // seller settles some
      auto result = force_settle( seller, asset(11100,mpa_id) );
      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      BOOST_CHECK( !mpa_id(db).bitasset_data(db).is_globally_settled() );

      // generate a block
      generate_block();

      // check again
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // publish a new feed so that the asset is globally settled
      f.settlement_price = price( asset(100,mpa_id), asset(10) );
      publish_feed( mpa_id, feeder_id, f );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).is_globally_settled() );

      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // generate a block
      generate_block();

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).is_globally_settled() );

      // the settle order is cancelled
      BOOST_REQUIRE( !db.find(settle_id) );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/// Tests a scenario that force settlements get cancelled on expiration when there is no sufficient feed
BOOST_AUTO_TEST_CASE( settle_order_cancel_due_to_no_feed )
{
   try {

      // Advance to a desired hard fork time
      if(hf2467)
      {
         auto mi = db.get_global_properties().parameters.maintenance_interval;
         generate_blocks(HARDFORK_CORE_2467_TIME - mi);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      }
      else
         generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      ACTORS((sam)(feeder)(borrower)(seller));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );
      fund( borrower, asset(init_amount) );

      // Create asset
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMMPA";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100; // 1%
      acop.common_options.flags = charge_market_fee;
      acop.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      acop.bitasset_opts = bitasset_options();
      acop.bitasset_opts->minimum_feeds = 1;
      acop.bitasset_opts->feed_lifetime_sec = 400;
      acop.bitasset_opts->force_settlement_delay_sec = 600;

      trx.operations.clear();
      trx.operations.push_back( acop );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& mpa = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type mpa_id = mpa.get_id();

      // add a price feed publisher and publish a feed
      update_feed_producers( mpa_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,mpa_id), asset(1) );
      f.core_exchange_rate = price( asset(100,mpa_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      publish_feed( mpa_id, feeder_id, f );

      // borrowers borrow some
      // undercollateralization price = 100000:2000 * 1250:1000 = 100000:1600
      const call_order_object* call_ptr = borrow( borrower, asset(100000, mpa_id), asset(2000) );
      BOOST_REQUIRE( call_ptr );
      call_order_id_type call_id = call_ptr->get_id();

      // Transfer funds to seller
      transfer( borrower, seller, asset(100000,mpa_id) );

      BOOST_CHECK_EQUAL( call_id(db).debt.value, 100000 );
      BOOST_CHECK_EQUAL( call_id(db).collateral.value, 2000 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // seller settles some
      auto result = force_settle( seller, asset(11100,mpa_id) );
      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // let the price feed expire
      generate_blocks( db.head_block_time() + fc::seconds(400) );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).current_feed.settlement_price.is_null() );

      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // let the settle order expire
      generate_blocks( db.head_block_time() + fc::seconds(200) );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).current_feed.settlement_price.is_null() );

      // the settle order is cancelled
      BOOST_REQUIRE( !db.find(settle_id) );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // unable to settle when no feed
      BOOST_CHECK_THROW( force_settle( seller, asset(11100,mpa_id) ), fc::exception );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/// Tests a scenario that force settlements get cancelled on expiration when the amount is zero
BOOST_AUTO_TEST_CASE( settle_order_cancel_due_to_zero_amount )
{
   try {

      // Advance to a desired hard fork time
      if(hf2467)
      {
         auto mi = db.get_global_properties().parameters.maintenance_interval;
         generate_blocks(HARDFORK_CORE_2467_TIME - mi);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      }
      else
         generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      ACTORS((sam)(feeder)(borrower)(seller));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );
      fund( borrower, asset(init_amount) );

      // Create asset
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMMPA";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100; // 1%
      acop.common_options.flags = charge_market_fee;
      acop.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      acop.bitasset_opts = bitasset_options();
      acop.bitasset_opts->minimum_feeds = 1;
      acop.bitasset_opts->feed_lifetime_sec = 800;
      acop.bitasset_opts->force_settlement_delay_sec = 600;

      trx.operations.clear();
      trx.operations.push_back( acop );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& mpa = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type mpa_id = mpa.get_id();

      // add a price feed publisher and publish a feed
      update_feed_producers( mpa_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,mpa_id), asset(1) );
      f.core_exchange_rate = price( asset(100,mpa_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      publish_feed( mpa_id, feeder_id, f );

      // borrowers borrow some
      // undercollateralization price = 100000:2000 * 1250:1000 = 100000:1600
      const call_order_object* call_ptr = borrow( borrower, asset(100000, mpa_id), asset(2000) );
      BOOST_REQUIRE( call_ptr );
      call_order_id_type call_id = call_ptr->get_id();

      // Transfer funds to seller
      transfer( borrower, seller, asset(100000,mpa_id) );

      BOOST_CHECK_EQUAL( call_id(db).debt.value, 100000 );
      BOOST_CHECK_EQUAL( call_id(db).collateral.value, 2000 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // seller settles some
      auto result = force_settle( seller, asset(0,mpa_id) );
      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 0 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // generate a block
      generate_block();

      // check again
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 0 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // let the settle order expire
      generate_blocks( db.head_block_time() + fc::seconds(600) );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).current_feed.settlement_price == f.settlement_price );

      // the settle order is cancelled
      BOOST_REQUIRE( !db.find(settle_id) );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/// Tests a scenario that force settlements get cancelled on expiration when offset is 100%
BOOST_AUTO_TEST_CASE( settle_order_cancel_due_to_100_percent_offset )
{
   try {

      // Advance to a desired hard fork time
      if(hf2467)
      {
         auto mi = db.get_global_properties().parameters.maintenance_interval;
         generate_blocks(HARDFORK_CORE_2467_TIME - mi);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      }
      else
         generate_blocks( HARDFORK_CORE_1780_TIME );

      set_expiration( db, trx );

      ACTORS((sam)(feeder)(borrower)(seller));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );
      fund( borrower, asset(init_amount) );

      // Create asset
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMMPA";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100; // 1%
      acop.common_options.flags = charge_market_fee;
      acop.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      acop.bitasset_opts = bitasset_options();
      acop.bitasset_opts->minimum_feeds = 1;
      acop.bitasset_opts->feed_lifetime_sec = 800;
      acop.bitasset_opts->force_settlement_delay_sec = 600;
      acop.bitasset_opts->force_settlement_offset_percent = GRAPHENE_100_PERCENT;

      trx.operations.clear();
      trx.operations.push_back( acop );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& mpa = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type mpa_id = mpa.get_id();

      // add a price feed publisher and publish a feed
      update_feed_producers( mpa_id, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(100,mpa_id), asset(1) );
      f.core_exchange_rate = price( asset(100,mpa_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;
      f.maximum_short_squeeze_ratio = 1250;

      publish_feed( mpa_id, feeder_id, f );

      // borrowers borrow some
      // undercollateralization price = 100000:2000 * 1250:1000 = 100000:1600
      const call_order_object* call_ptr = borrow( borrower, asset(100000, mpa_id), asset(2000) );
      BOOST_REQUIRE( call_ptr );
      call_order_id_type call_id = call_ptr->get_id();

      // Transfer funds to seller
      transfer( borrower, seller, asset(100000,mpa_id) );

      BOOST_CHECK_EQUAL( call_id(db).debt.value, 100000 );
      BOOST_CHECK_EQUAL( call_id(db).collateral.value, 2000 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // seller settles some
      auto result = force_settle( seller, asset(11100,mpa_id) );
      force_settlement_id_type settle_id { *result.get<extendable_operation_result>().value.new_objects->begin() };
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // generate a block
      generate_block();

      // check again
      BOOST_REQUIRE( db.find(settle_id) );

      BOOST_CHECK_EQUAL( settle_id(db).balance.amount.value, 11100 );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 88900 ); // 100000 - 11100
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

      // let the settle order expire
      generate_blocks( db.head_block_time() + fc::seconds(600) );

      // check
      BOOST_CHECK( mpa_id(db).bitasset_data(db).current_feed.settlement_price == f.settlement_price );

      // the settle order is cancelled
      BOOST_REQUIRE( !db.find(settle_id) );

      BOOST_CHECK_EQUAL( get_balance( seller_id, mpa_id ), 100000 );
      BOOST_CHECK_EQUAL( get_balance( seller_id, asset_id_type() ), 0 );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(settle_order_cancel_due_to_no_feed_after_hf_2467)
{ try {
   hf2467 = true;
   INVOKE(settle_order_cancel_due_to_no_feed);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(settle_order_cancel_due_to_zero_amount_after_hf_2467)
{ try {
   hf2467 = true;
   INVOKE(settle_order_cancel_due_to_zero_amount);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(settle_order_cancel_due_to_100_percent_offset_after_hf_2467)
{ try {
   hf2467 = true;
   INVOKE(settle_order_cancel_due_to_100_percent_offset);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(settle_rounding_test_after_hf_1270)
{ try {
   hf1270 = true;
   INVOKE(settle_rounding_test_after_hf_184);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(settle_rounding_test_after_hf_2481)
{ try {
   hf2481 = true;
   INVOKE(settle_rounding_test_after_hf_184);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(global_settle_rounding_test_after_hf_1270)
{ try {
   hf1270 = true;
   INVOKE(global_settle_rounding_test_after_hf_184);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(global_settle_rounding_test_after_hf_2481)
{ try {
   hf2481 = true;
   INVOKE(global_settle_rounding_test_after_hf_184);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(market_fee_of_settle_order_after_hardfork_2481)
{ try {
   hf2481 = true;
   INVOKE(market_fee_of_settle_order_after_hardfork_1780);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(market_fee_of_instant_settle_order_after_hardfork_2481)
{ try {
   hf2481 = true;
   INVOKE(market_fee_of_instant_settle_order_after_hardfork_1780);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
