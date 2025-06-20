/*
cloudbank
 */
#include <graphene/app/api.hpp>
#include <graphene/app/api_access.hpp>
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>

#include <graphene/chain/db_with.hpp>
#include <graphene/chain/genesis_state.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/protocol/types.hpp>

#include <graphene/egenesis/egenesis.hpp>

#include <graphene/net/core_messages.hpp>
#include <graphene/net/exceptions.hpp>

#include <graphene/utilities/key_conversion.hpp>
#include <graphene/chain/worker_evaluator.hpp>

#include <fc/asio.hpp>
#include <fc/io/fstream.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/base64.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/signals2.hpp>
#include <boost/range/algorithm/reverse.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>

#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#include <boost/range/adaptor/reversed.hpp>

namespace graphene { namespace app {
using net::item_hash_t;
using net::item_id;
using net::message;
using net::block_message;
using net::trx_message;

using chain::block_header;
using chain::signed_block_header;
using chain::signed_block;
using chain::block_id_type;

using std::vector;

namespace bpo = boost::program_options;

namespace detail {

   graphene::chain::genesis_state_type create_example_genesis() {
      auto nathan_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      dlog("Allocating all stake to ${key}", ("key", utilities::key_to_wif(nathan_key)));
      graphene::chain::genesis_state_type initial_state;
      initial_state.initial_parameters.get_mutable_fees() = fee_schedule::get_default();
      initial_state.initial_active_witnesses = GRAPHENE_DEFAULT_MIN_WITNESS_COUNT;
      initial_state.initial_timestamp = time_point_sec(time_point::now().sec_since_epoch() /
            initial_state.initial_parameters.block_interval *
            initial_state.initial_parameters.block_interval);
      for( uint64_t i = 0; i < initial_state.initial_active_witnesses; ++i )
      {
         auto name = "init"+fc::to_string(i);
         initial_state.initial_accounts.emplace_back(name,
                                                     nathan_key.get_public_key(),
                                                     nathan_key.get_public_key(),
                                                     true);
         initial_state.initial_committee_candidates.push_back({name});
         initial_state.initial_witness_candidates.push_back({name, nathan_key.get_public_key()});
      }

      initial_state.initial_accounts.emplace_back("nathan", nathan_key.get_public_key());
      initial_state.initial_balances.push_back({address(nathan_key.get_public_key()),
                                                GRAPHENE_SYMBOL,
                                                GRAPHENE_MAX_SHARE_SUPPLY});
      initial_state.initial_chain_id = fc::sha256::hash( "BOGUS" );

      return initial_state;
   }


}

}}

#include "application_impl.hxx"

namespace graphene { namespace app { namespace detail {

application_impl::~application_impl()
{
   this->shutdown();
}

void application_impl::reset_p2p_node(const fc::path& data_dir)
{ try {
   _p2p_network = std::make_shared<net::node>("BitShares Reference Implementation");

   _p2p_network->load_configuration(data_dir / "p2p");
   _p2p_network->set_node_delegate(shared_from_this());

   if( _options->count("seed-node") > 0 )
   {
      auto seeds = _options->at("seed-node").as<vector<string>>();
      _p2p_network->add_seed_nodes(seeds);
   }

   if( _options->count("seed-nodes") > 0 )
   {
      auto seeds_str = _options->at("seed-nodes").as<string>();
      auto seeds = fc::json::from_string(seeds_str).as<vector<string>>(2);
      _p2p_network->add_seed_nodes(seeds);
   }
   else
   {
      vector<string> seeds = {
         #include "../egenesis/seed-nodes.txt"
      };
      _p2p_network->add_seed_nodes(seeds);
   }

   if( _options->count( "p2p-advertise-peer-algorithm" ) > 0 )
   {
      std::string algo = _options->at("p2p-advertise-peer-algorithm").as<string>();
      std::vector<std::string> list;
      if( algo == "list" && _options->count("p2p-advertise-peer-endpoint") > 0 )
         list = _options->at("p2p-advertise-peer-endpoint").as<std::vector<std::string>>();
      else if( algo == "exclude_list" && _options->count("p2p-exclude-peer-endpoint") > 0 )
         list = _options->at("p2p-exclude-peer-endpoint").as<std::vector<std::string>>();
      _p2p_network->set_advertise_algorithm( algo, list );
   }

   if( _options->count("p2p-endpoint") > 0 )
      _p2p_network->set_listen_endpoint( fc::ip::endpoint::from_string(_options->at("p2p-endpoint").as<string>()),
                                         true );
   // else try to listen on the default port first, if failed, use a random port

   if( _options->count("p2p-inbound-endpoint") > 0 )
      _p2p_network->set_inbound_endpoint( fc::ip::endpoint::from_string(_options->at("p2p-inbound-endpoint")
                                              .as<string>()) );

   if ( _options->count("p2p-accept-incoming-connections") > 0 )
      _p2p_network->set_accept_incoming_connections( _options->at("p2p-accept-incoming-connections").as<bool>() );

   if ( _options->count("p2p-connect-to-new-peers") > 0 )
      _p2p_network->set_connect_to_new_peers( _options->at( "p2p-connect-to-new-peers" ).as<bool>() );

   _p2p_network->listen_to_p2p_network();
   fc::ip::endpoint listening_endpoint = _p2p_network->get_actual_listening_endpoint();
   if( listening_endpoint.port() != 0 )
      ilog( "Configured p2p node to listen on ${ip}", ("ip", listening_endpoint) );
   else
      ilog( "Configured p2p node to not listen for incoming connections" );

   _p2p_network->connect_to_p2p_network();
   _p2p_network->sync_from(net::item_id(net::core_message_type_enum::block_message_type,
                                        _chain_db->head_block_id()),
                           std::vector<uint32_t>());
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

void application_impl::new_connection( const fc::http::websocket_connection_ptr& c )
{
   auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(c, GRAPHENE_NET_MAX_NESTED_OBJECTS);
   auto login = std::make_shared<graphene::app::login_api>( _self );

    // Try to extract login information from "Authorization" header if present
   std::string auth = c->get_request_header("Authorization");
   if( boost::starts_with(auth, "Basic ") ) {

      FC_ASSERT( auth.size() > 6 );
      auto user_pass = fc::base64_decode(auth.substr(6));

      std::vector<std::string> parts;
      boost::split( parts, user_pass, boost::is_any_of(":") );

      FC_ASSERT(parts.size() == 2);

      const string& username = parts[0];
      const string& password = parts[1];
      login->login(username, password);
   }
   else
      login->login("", "");

   // API set ID 0. Note: changing it may break client applications
   if( login->is_database_api_allowed() )
      wsc->register_api(login->database());
   else
      wsc->register_api(login->dummy());
   // API set ID 1. Note: changing it may break client applications
   wsc->register_api(fc::api<graphene::app::login_api>(login));
   c->set_session_data( wsc );
}

void application_impl::reset_websocket_server()
{ try {
   if( 0 == _options->count("rpc-endpoint") )
      return;

   string proxy_forward_header;
   if( _options->count("proxy-forwarded-for-header") > 0 )
      proxy_forward_header = _options->at("proxy-forwarded-for-header").as<string>();

   _websocket_server = std::make_shared<fc::http::websocket_server>( proxy_forward_header );
   _websocket_server->on_connection( std::bind(&application_impl::new_connection, this, std::placeholders::_1) );

   ilog("Configured websocket rpc to listen on ${ip}", ("ip",_options->at("rpc-endpoint").as<string>()));
   _websocket_server->listen( fc::ip::endpoint::from_string(_options->at("rpc-endpoint").as<string>()) );
   _websocket_server->start_accept();
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

void application_impl::reset_websocket_tls_server()
{ try {
   if( 0 == _options->count("rpc-tls-endpoint") )
      return;
   if( 0 == _options->count("server-pem") )
   {
      wlog( "Please specify a server-pem to use rpc-tls-endpoint" );
      return;
   }

   string proxy_forward_header;
   if( _options->count("proxy-forwarded-for-header") > 0 )
      proxy_forward_header = _options->at("proxy-forwarded-for-header").as<string>();

   string password = ( _options->count("server-pem-password") > 0 ) ?
                        _options->at("server-pem-password").as<string>() : "";
   _websocket_tls_server = std::make_shared<fc::http::websocket_tls_server>(
                                 _options->at("server-pem").as<string>(), password, proxy_forward_header );
   _websocket_tls_server->on_connection( std::bind(&application_impl::new_connection, this, std::placeholders::_1) );

   ilog("Configured websocket TLS rpc to listen on ${ip}", ("ip",_options->at("rpc-tls-endpoint").as<string>()));
   _websocket_tls_server->listen( fc::ip::endpoint::from_string(_options->at("rpc-tls-endpoint").as<string>()) );
   _websocket_tls_server->start_accept();
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

void application_impl::initialize(const fc::path& data_dir, shared_ptr<boost::program_options::variables_map> options)
{
   _data_dir = data_dir;
   _options = options;

   if ( _options->count("io-threads") > 0 )
   {
      const uint16_t num_threads = _options->at("io-threads").as<uint16_t>();
      fc::asio::default_io_service_scope::set_num_threads(num_threads);
   }

   if( _options->count("force-validate") > 0 )
   {
      ilog( "All transaction signatures will be validated" );
      _force_validate = true;
   }

   if ( _options->count("enable-subscribe-to-all") > 0 )
      _app_options.enable_subscribe_to_all = _options->at( "enable-subscribe-to-all" ).as<bool>();

   set_api_limit();

   if( is_plugin_enabled( "market_history" ) )
      _app_options.has_market_history_plugin = true;
   else
      ilog("Market history plugin is not enabled");

   if( is_plugin_enabled( "api_helper_indexes" ) )
      _app_options.has_api_helper_indexes_plugin = true;
   else
      ilog("API helper indexes plugin is not enabled");

   if (_options->count("api-node-info") > 0)
      _node_info = _options->at("api-node-info").as<string>();

   if( _options->count("api-access") > 0 )
   {

      fc::path api_access_file = _options->at("api-access").as<boost::filesystem::path>();

      FC_ASSERT( fc::exists(api_access_file),
            "Failed to load file from ${path}", ("path", api_access_file) );

      _apiaccess = fc::json::from_file( api_access_file ).as<api_access>( 20 );
      ilog( "Using api access file from ${path}",
            ("path", api_access_file) );
   }
   else
   {
      // TODO:  Remove this generous default access policy
      // when the UI logs in properly
      _apiaccess = api_access();
      api_access_info wild_access("*", "*");
      wild_access.allowed_apis.insert( "database_api" );
      wild_access.allowed_apis.insert( "network_broadcast_api" );
      wild_access.allowed_apis.insert( "history_api" );
      wild_access.allowed_apis.insert( "orders_api" );
      wild_access.allowed_apis.insert( "custom_operations_api" );
      _apiaccess.permission_map["*"] = wild_access;
   }

   initialize_plugins();
}

void application_impl::set_api_limit() {
   if (_options->count("api-limit-get-account-history-operations") > 0) {
      _app_options.api_limit_get_account_history_operations =
            _options->at("api-limit-get-account-history-operations").as<uint32_t>();
   }
   if(_options->count("api-limit-get-account-history") > 0){
      _app_options.api_limit_get_account_history =
            _options->at("api-limit-get-account-history").as<uint32_t>();
   }
   if(_options->count("api-limit-get-grouped-limit-orders") > 0){
      _app_options.api_limit_get_grouped_limit_orders =
            _options->at("api-limit-get-grouped-limit-orders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-market-history") > 0){
      _app_options.api_limit_get_market_history =
            _options->at("api-limit-get-market-history").as<uint32_t>();
   }
   if(_options->count("api-limit-get-relative-account-history") > 0){
      _app_options.api_limit_get_relative_account_history =
            _options->at("api-limit-get-relative-account-history").as<uint32_t>();
   }
   if(_options->count("api-limit-get-account-history-by-operations") > 0){
      _app_options.api_limit_get_account_history_by_operations =
            _options->at("api-limit-get-account-history-by-operations").as<uint32_t>();
   }
   if(_options->count("api-limit-get-asset-holders") > 0){
      _app_options.api_limit_get_asset_holders =
            _options->at("api-limit-get-asset-holders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-key-references") > 0){
      _app_options.api_limit_get_key_references =
            _options->at("api-limit-get-key-references").as<uint32_t>();
   }
   if(_options->count("api-limit-get-htlc-by") > 0) {
      _app_options.api_limit_get_htlc_by =
            _options->at("api-limit-get-htlc-by").as<uint32_t>();
   }
   if(_options->count("api-limit-get-full-accounts") > 0) {
      _app_options.api_limit_get_full_accounts =
            _options->at("api-limit-get-full-accounts").as<uint32_t>();
   }
   if(_options->count("api-limit-get-full-accounts-lists") > 0) {
      _app_options.api_limit_get_full_accounts_lists =
            _options->at("api-limit-get-full-accounts-lists").as<uint32_t>();
   }
   if(_options->count("api-limit-get-full-accounts-subscribe") > 0) {
      _app_options.api_limit_get_full_accounts_subscribe =
            _options->at("api-limit-get-full-accounts-subscribe").as<uint32_t>();
   }
   if(_options->count("api-limit-get-top-voters") > 0) {
      _app_options.api_limit_get_top_voters =
            _options->at("api-limit-get-top-voters").as<uint32_t>();
   }
   if(_options->count("api-limit-get-call-orders") > 0) {
      _app_options.api_limit_get_call_orders =
            _options->at("api-limit-get-call-orders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-settle-orders") > 0) {
      _app_options.api_limit_get_settle_orders =
            _options->at("api-limit-get-settle-orders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-assets") > 0) {
      _app_options.api_limit_get_assets =
            _options->at("api-limit-get-assets").as<uint32_t>();
   }
   if(_options->count("api-limit-get-limit-orders") > 0){
      _app_options.api_limit_get_limit_orders =
            _options->at("api-limit-get-limit-orders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-limit-orders-by-account") > 0){
      _app_options.api_limit_get_limit_orders_by_account =
            _options->at("api-limit-get-limit-orders-by-account").as<uint32_t>();
   }
   if(_options->count("api-limit-get-order-book") > 0){
      _app_options.api_limit_get_order_book =
            _options->at("api-limit-get-order-book").as<uint32_t>();
   }
   if(_options->count("api-limit-list-htlcs") > 0){
      _app_options.api_limit_list_htlcs =
            _options->at("api-limit-list-htlcs").as<uint32_t>();
   }
   if(_options->count("api-limit-lookup-accounts") > 0) {
      _app_options.api_limit_lookup_accounts =
            _options->at("api-limit-lookup-accounts").as<uint32_t>();
   }
   if(_options->count("api-limit-lookup-witness-accounts") > 0) {
      _app_options.api_limit_lookup_witness_accounts =
            _options->at("api-limit-lookup-witness-accounts").as<uint32_t>();
   }
   if(_options->count("api-limit-lookup-committee-member-accounts") > 0) {
      _app_options.api_limit_lookup_committee_member_accounts =
            _options->at("api-limit-lookup-committee-member-accounts").as<uint32_t>();
   }
   if(_options->count("api-limit-lookup-vote-ids") > 0) {
      _app_options.api_limit_lookup_vote_ids =
            _options->at("api-limit-lookup-vote-ids").as<uint32_t>();
   }
   if(_options->count("api-limit-get-account-limit-orders") > 0) {
      _app_options.api_limit_get_account_limit_orders =
            _options->at("api-limit-get-account-limit-orders").as<uint32_t>();
   }
   if(_options->count("api-limit-get-collateral-bids") > 0) {
      _app_options.api_limit_get_collateral_bids =
            _options->at("api-limit-get-collateral-bids").as<uint32_t>();
   }
   if(_options->count("api-limit-get-top-markets") > 0) {
      _app_options.api_limit_get_top_markets =
            _options->at("api-limit-get-top-markets").as<uint32_t>();
   }
   if(_options->count("api-limit-get-trade-history") > 0) {
      _app_options.api_limit_get_trade_history =
            _options->at("api-limit-get-trade-history").as<uint32_t>();
   }
   if(_options->count("api-limit-get-trade-history-by-sequence") > 0) {
      _app_options.api_limit_get_trade_history_by_sequence =
            _options->at("api-limit-get-trade-history-by-sequence").as<uint32_t>();
   }
   if(_options->count("api-limit-get-withdraw-permissions-by-giver") > 0) {
      _app_options.api_limit_get_withdraw_permissions_by_giver =
            _options->at("api-limit-get-withdraw-permissions-by-giver").as<uint32_t>();
   }
   if(_options->count("api-limit-get-withdraw-permissions-by-recipient") > 0) {
      _app_options.api_limit_get_withdraw_permissions_by_recipient =
            _options->at("api-limit-get-withdraw-permissions-by-recipient").as<uint32_t>();
   }
   if(_options->count("api-limit-get-tickets") > 0) {
      _app_options.api_limit_get_tickets =
            _options->at("api-limit-get-tickets").as<uint32_t>();
   }
   if(_options->count("api-limit-get-liquidity-pools") > 0) {
      _app_options.api_limit_get_liquidity_pools =
            _options->at("api-limit-get-liquidity-pools").as<uint32_t>();
   }
   if(_options->count("api-limit-get-liquidity-pool-history") > 0) {
      _app_options.api_limit_get_liquidity_pool_history =
            _options->at("api-limit-get-liquidity-pool-history").as<uint32_t>();
   }
   if(_options->count("api-limit-get-samet-funds") > 0) {
      _app_options.api_limit_get_samet_funds =
            _options->at("api-limit-get-samet-funds").as<uint32_t>();
   }
   if(_options->count("api-limit-get-credit-offers") > 0) {
      _app_options.api_limit_get_credit_offers =
            _options->at("api-limit-get-credit-offers").as<uint32_t>();
   }
   if(_options->count("api-limit-get-storage-info") > 0) {
      _app_options.api_limit_get_storage_info =
            _options->at("api-limit-get-storage-info").as<uint32_t>();
   }
}

graphene::chain::genesis_state_type application_impl::initialize_genesis_state() const
{
   try {
      ilog("Initializing database...");
      if( _options->count("genesis-json") > 0 )
      {
         std::string genesis_str;
         fc::read_file_contents( _options->at("genesis-json").as<boost::filesystem::path>(), genesis_str );
         auto genesis = fc::json::from_string( genesis_str ).as<graphene::chain::genesis_state_type>( 20 );
         bool modified_genesis = false;
         if( _options->count("genesis-timestamp") > 0 )
         {
            genesis.initial_timestamp = fc::time_point_sec( fc::time_point::now() )
                                      + genesis.initial_parameters.block_interval
                                      + _options->at("genesis-timestamp").as<uint32_t>();
            genesis.initial_timestamp -= ( genesis.initial_timestamp.sec_since_epoch()
                                           % genesis.initial_parameters.block_interval );
            modified_genesis = true;

            ilog(
               "Used genesis timestamp:  ${timestamp} (PLEASE RECORD THIS)",
               ("timestamp", genesis.initial_timestamp.to_iso_string())
            );
         }
         if( _options->count("dbg-init-key") > 0 )
         {
            std::string init_key = _options->at( "dbg-init-key" ).as<string>();
            FC_ASSERT( genesis.initial_witness_candidates.size() >= genesis.initial_active_witnesses );
            genesis.override_witness_signing_keys( init_key );
            modified_genesis = true;
            ilog("Set init witness key to ${init_key}", ("init_key", init_key));
         }
         if( modified_genesis )
         {
            wlog("WARNING:  GENESIS WAS MODIFIED, YOUR CHAIN ID MAY BE DIFFERENT");
            genesis_str += "BOGUS";
         }
         genesis.initial_chain_id = fc::sha256::hash( genesis_str );
         return genesis;
      }
      else
      {
         std::string egenesis_json;
         graphene::egenesis::compute_egenesis_json( egenesis_json );
         FC_ASSERT( egenesis_json != "" );
         FC_ASSERT( graphene::egenesis::get_egenesis_json_hash() == fc::sha256::hash( egenesis_json ) );
         auto genesis = fc::json::from_string( egenesis_json ).as<graphene::chain::genesis_state_type>( 20 );
         genesis.initial_chain_id = fc::sha256::hash( egenesis_json );
         return genesis;
      }
   } FC_CAPTURE_AND_RETHROW() // GCOVR_EXCL_LINE
}

void application_impl::open_chain_database() const
{ try {
   fc::create_directories(_data_dir / "blockchain");

   if( _options->count("resync-blockchain") > 0 )
      _chain_db->wipe(_data_dir / "blockchain", true);

   flat_map<uint32_t,block_id_type> loaded_checkpoints;
   if( _options->count("checkpoint") > 0 )
   {
      auto cps = _options->at("checkpoint").as<vector<string>>();
      loaded_checkpoints.reserve( cps.size() );
      for( auto cp : cps )
      {
         auto item = fc::json::from_string(cp).as<std::pair<uint32_t,block_id_type> >( 2 );
         loaded_checkpoints[item.first] = item.second;
      }
   }
   _chain_db->add_checkpoints( loaded_checkpoints );

   if( _options->count("enable-standby-votes-tracking") > 0 )
   {
      _chain_db->enable_standby_votes_tracking( _options->at("enable-standby-votes-tracking").as<bool>() );
   }

   if( _options->count("replay-blockchain") > 0 || _options->count("revalidate-blockchain") > 0 )
      _chain_db->wipe( _data_dir / "blockchain", false );

   try
   {
      // these flags are used in open() only, i. e. during replay
      uint32_t skip;
      if( _options->count("revalidate-blockchain") > 0 ) // see also handle_block()
      {
         if( !loaded_checkpoints.empty() )
            wlog( "Warning - revalidate will not validate before last checkpoint" );
         if( _options->count("force-validate") > 0 )
            skip = graphene::chain::database::skip_nothing;
         else
            skip = graphene::chain::database::skip_transaction_signatures;
      }
      else // no revalidate, skip most checks
         skip = graphene::chain::database::skip_witness_signature |
                graphene::chain::database::skip_block_size_check |
                graphene::chain::database::skip_merkle_check |
                graphene::chain::database::skip_transaction_signatures |
                graphene::chain::database::skip_transaction_dupe_check |
                graphene::chain::database::skip_tapos_check |
                graphene::chain::database::skip_witness_schedule_check;

      auto genesis_loader = [this](){
         return initialize_genesis_state();
      };

      graphene::chain::detail::with_skip_flags( *_chain_db, skip, [this, &genesis_loader] () {
         _chain_db->open( _data_dir / "blockchain", genesis_loader, GRAPHENE_CURRENT_DB_VERSION );
      });
   }
   catch( const fc::exception& e )
   {
      elog( "Caught exception ${e} in open(), you might want to force a replay", ("e", e.to_detail_string()) );
      throw;
   }
} FC_LOG_AND_RETHROW() }

void application_impl::startup()
{ try {
   bool enable_p2p_network = true;
   if( _options->count("enable-p2p-network") > 0 )
      enable_p2p_network = _options->at("enable-p2p-network").as<bool>();

   open_chain_database();

   startup_plugins();

   if( enable_p2p_network && _active_plugins.find( "delayed_node" ) == _active_plugins.end() )
      reset_p2p_node(_data_dir);

   reset_websocket_server();
   reset_websocket_tls_server();
} FC_LOG_AND_RETHROW() }

optional< api_access_info > application_impl::get_api_access_info(const string& username)const
{
   optional< api_access_info > result;
   auto it = _apiaccess.permission_map.find(username);
   if( it == _apiaccess.permission_map.end() )
   {
      it = _apiaccess.permission_map.find("*");
      if( it == _apiaccess.permission_map.end() )
         return result;
   }
   return it->second;
}

void application_impl::set_api_access_info(const string& username, api_access_info&& permissions)
{
   _apiaccess.permission_map.insert(std::make_pair(username, std::move(permissions)));
}

bool application_impl::is_plugin_enabled(const string& name) const
{
   return !(_active_plugins.find(name) == _active_plugins.end());
}

/*
 * If delegate has the item, the network has no need to fetch it.
 */
bool application_impl::has_item(const net::item_id& id)
{
   try
   {
      if( id.item_type == graphene::net::block_message_type )
         return _chain_db->is_known_block(id.item_hash);
      else
         return _chain_db->is_known_transaction(id.item_hash);
   }
   FC_CAPTURE_AND_RETHROW( (id) ) // GCOVR_EXCL_LINE
}

/*
 * @brief allows the application to validate an item prior to broadcasting to peers.
 *
 * @param sync_mode true if the message was fetched through the sync process, false during normal operation
 * @returns true if this message caused the blockchain to switch forks, false if it did not
 *
 * @throws exception if error validating the item, otherwise the item is safe to broadcast on.
 */
bool application_impl::handle_block(const graphene::net::block_message& blk_msg, bool sync_mode,
                          std::vector<graphene::net::message_hash_type>& contained_transaction_msg_ids)
{ try {

   auto latency = fc::time_point::now() - blk_msg.block.timestamp;
   if (!sync_mode || blk_msg.block.block_num() % 10000 == 0)
   {
      const auto& witness = blk_msg.block.witness(*_chain_db);
      const auto& witness_account = witness.witness_account(*_chain_db);
      auto last_irr = _chain_db->get_dynamic_global_properties().last_irreversible_block_num;
      ilog("Got block: #${n} ${bid} time: ${t} transaction(s): ${x} "
           "latency: ${l} ms from: ${w}  irreversible: ${i} (-${d})",
           ("t",blk_msg.block.timestamp)
           ("n", blk_msg.block.block_num())
           ("bid", blk_msg.block.id())
           ("x", blk_msg.block.transactions.size())
           ("l", (latency.count()/1000))
           ("w",witness_account.name)
           ("i",last_irr)("d",blk_msg.block.block_num()-last_irr) );
   }
   GRAPHENE_ASSERT( latency.count()/1000 > -2500, // 2.5 seconds
                    graphene::net::block_timestamp_in_future_exception,
                    "Rejecting block with timestamp in the future", );

   try {
      const uint32_t skip = (_is_block_producer || _force_validate) ?
                               database::skip_nothing : database::skip_transaction_signatures;
      bool result = valve.do_serial( [this,&blk_msg,skip] () {
         _chain_db->precompute_parallel( blk_msg.block, skip ).wait();
      }, [this,&blk_msg,skip] () {
         // TODO: in the case where this block is valid but on a fork that's too old for us to switch to,
         // you can help the network code out by throwing a block_older_than_undo_history exception.
         // when the net code sees that, it will stop trying to push blocks from that chain, but
         // leave that peer connected so that they can get sync blocks from us
         return _chain_db->push_block( blk_msg.block, skip );
      });

      // the block was accepted, so we now know all of the transactions contained in the block
      if (!sync_mode)
      {
         // if we're not in sync mode, there's a chance we will be seeing some transactions
         // included in blocks before we see the free-floating transaction itself.  If that
         // happens, there's no reason to fetch the transactions, so  construct a list of the
         // transaction message ids we no longer need.
         // during sync, it is unlikely that we'll see any old
         contained_transaction_msg_ids.reserve( contained_transaction_msg_ids.size()
                                                    + blk_msg.block.transactions.size() );
         for (const processed_transaction& ptrx : blk_msg.block.transactions)
         {
            graphene::net::trx_message transaction_message(ptrx);
            contained_transaction_msg_ids.emplace_back(graphene::net::message(transaction_message).id());
         }
      }

      return result;
   } catch ( const graphene::chain::unlinkable_block_exception& e ) {
      // translate to a graphene::net exception
      elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      FC_THROW_EXCEPTION( graphene::net::unlinkable_block_exception,
                          "Error when pushing block:\n${e}",
                          ("e", e.to_detail_string()) );
   } catch( const fc::exception& e ) {
      elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      throw;
   }

   if( !_is_finished_syncing && !sync_mode )
   {
      _is_finished_syncing = true;
      _self.syncing_finished();
   }
} FC_CAPTURE_AND_RETHROW( (blk_msg)(sync_mode) ) return false; } // GCOVR_EXCL_LINE

void application_impl::handle_transaction(const graphene::net::trx_message& transaction_message)
{ try {
   static fc::time_point last_call;
   static int trx_count = 0;
   ++trx_count;
   auto now = fc::time_point::now();
   if( now - last_call > fc::seconds(1) ) {
      ilog("Got ${c} transactions from network", ("c",trx_count) );
      last_call = now;
      trx_count = 0;
   }

   _chain_db->precompute_parallel( transaction_message.trx ).wait();
   _chain_db->push_transaction( transaction_message.trx );
} FC_CAPTURE_AND_RETHROW( (transaction_message) ) } // GCOVR_EXCL_LINE

void application_impl::handle_message(const message& message_to_process)
{
   // not a transaction, not a block
   FC_THROW( "Invalid Message Type" );
}

bool application_impl::is_included_block(const block_id_type& block_id)
{
  uint32_t block_num = block_header::num_from_id(block_id);
  block_id_type block_id_in_preferred_chain = _chain_db->get_block_id_for_num(block_num);
  return block_id == block_id_in_preferred_chain;
}

/*
 * Assuming all data elements are ordered in some way, this method should
 * return up to limit ids that occur *after* the last ID in synopsis that
 * we recognize.
 *
 * On return, remaining_item_count will be set to the number of items
 * in our blockchain after the last item returned in the result,
 * or 0 if the result contains the last item in the blockchain
 */
std::vector<item_hash_t> application_impl::get_block_ids(const std::vector<item_hash_t>& blockchain_synopsis,
                                               uint32_t& remaining_item_count,
                                               uint32_t limit)
{ try {
   vector<block_id_type> result;
   remaining_item_count = 0;
   if( _chain_db->head_block_num() == 0 )
      return result;

   result.reserve(limit);
   block_id_type last_known_block_id;

   if (blockchain_synopsis.empty() ||
       (blockchain_synopsis.size() == 1 && blockchain_synopsis[0] == block_id_type()))
   {
     // peer has sent us an empty synopsis meaning they have no blocks.
     // A bug in old versions would cause them to send a synopsis containing block 000000000
     // when they had an empty blockchain, so pretend they sent the right thing here.

     // do nothing, leave last_known_block_id set to zero
   }
   else
   {
     bool found_a_block_in_synopsis = false;
     for (const item_hash_t& block_id_in_synopsis : boost::adaptors::reverse(blockchain_synopsis))
       if (block_id_in_synopsis == block_id_type() ||
           (_chain_db->is_known_block(block_id_in_synopsis) && is_included_block(block_id_in_synopsis)))
       {
         last_known_block_id = block_id_in_synopsis;
         found_a_block_in_synopsis = true;
         break;
       }
     if (!found_a_block_in_synopsis)
       FC_THROW_EXCEPTION( graphene::net::peer_is_on_an_unreachable_fork,
                           "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis" );
   }
   for( uint32_t num = block_header::num_from_id(last_known_block_id);
        num <= _chain_db->head_block_num() && result.size() < limit;
        ++num )
      if( num > 0 )
         result.push_back(_chain_db->get_block_id_for_num(num));

   if( !result.empty() && block_header::num_from_id(result.back()) < _chain_db->head_block_num() )
      remaining_item_count = _chain_db->head_block_num() - block_header::num_from_id(result.back());

   return result;
} FC_CAPTURE_AND_RETHROW( (blockchain_synopsis)(remaining_item_count)(limit) ) } // GCOVR_EXCL_LINE

/*
 * Given the hash of the requested data, fetch the body.
 */
message application_impl::get_item(const item_id& id)
{ try {
  // ilog("Request for item ${id}", ("id", id));
   if( id.item_type == graphene::net::block_message_type )
   {
      auto opt_block = _chain_db->fetch_block_by_id(id.item_hash);
      if( !opt_block )
         elog("Couldn't find block ${id} -- corresponding ID in our chain is ${id2}",
              ("id", id.item_hash)("id2", _chain_db->get_block_id_for_num(block_header::num_from_id(id.item_hash))));
      FC_ASSERT( opt_block.valid() );
      // ilog("Serving up block #${num}", ("num", opt_block->block_num()));
      return block_message(std::move(*opt_block));
   }
   return trx_message( _chain_db->get_recent_transaction( id.item_hash ) );
} FC_CAPTURE_AND_RETHROW( (id) ) } // GCOVR_EXCL_LINE

chain_id_type application_impl::get_chain_id() const
{
   return _chain_db->get_chain_id();
}

/*
 * Returns a synopsis of the blockchain used for syncing.  This consists of a list of
 * block hashes at intervals exponentially increasing towards the genesis block.
 * When syncing to a peer, the peer uses this data to determine if we're on the same
 * fork as they are, and if not, what blocks they need to send us to get us on their
 * fork.
 *
 * In the over-simplified case, this is a straighforward synopsis of our current
 * preferred blockchain; when we first connect up to a peer, this is what we will be sending.
 * It looks like this:
 *   If the blockchain is empty, it will return the empty list.
 *   If the blockchain has one block, it will return a list containing just that block.
 *   If it contains more than one block:
 *     the first element in the list will be the hash of the highest numbered block that
 *         we cannot undo
 *     the second element will be the hash of an item at the half way point in the undoable
 *         segment of the blockchain
 *     the third will be ~3/4 of the way through the undoable segment of the block chain
 *     the fourth will be at ~7/8...
 *       &c.
 *     the last item in the list will be the hash of the most recent block on our preferred chain
 * so if the blockchain had 26 blocks labeled a - z, the synopsis would be:
 *    a n u x z
 * the idea being that by sending a small (<30) number of block ids, we can summarize a huge
 * blockchain.  The block ids are more dense near the end of the chain where because we are
 * more likely to be almost in sync when we first connect, and forks are likely to be short.
 * If the peer we're syncing with in our example is on a fork that started at block 'v',
 * then they will reply to our synopsis with a list of all blocks starting from block 'u',
 * the last block they know that we had in common.
 *
 * In the real code, there are several complications.
 *
 * First, as an optimization, we don't usually send a synopsis of the entire blockchain, we
 * send a synopsis of only the segment of the blockchain that we have undo data for.  If their
 * fork doesn't build off of something in our undo history, we would be unable to switch, so there's
 * no reason to fetch the blocks.
 *
 * Second, when a peer replies to our initial synopsis and gives us a list of the blocks they think
 * we are missing, they only send a chunk of a few thousand blocks at once.  After we get those
 * block ids, we need to request more blocks by sending another synopsis (we can't just say "send me
 * the next 2000 ids" because they may have switched forks themselves and they don't track what
 * they've sent us).  For faster performance, we want to get a fairly long list of block ids first,
 * then start downloading the blocks.
 * The peer doesn't handle these follow-up block id requests any different from the initial request;
 * it treats the synopsis we send as our blockchain and bases its response entirely off that.  So to
 * get the response we want (the next chunk of block ids following the last one they sent us, or,
 * failing that, the shortest fork off of the last list of block ids they sent), we need to construct
 * a synopsis as if our blockchain was made up of:
 *    1. the blocks in our block chain up to the fork point (if there is a fork) or the head block (if no fork)
 *    2. the blocks we've already pushed from their fork (if there's a fork)
 *    3. the block ids they've previously sent us
 * Segment 3 is handled in the p2p code, it just tells us the number of blocks it has (in
 * number_of_blocks_after_reference_point) so we can leave space in the synopsis for them.
 * We're responsible for constructing the synopsis of Segments 1 and 2 from our active blockchain and
 * fork database.  The reference_point parameter is the last block from that peer that has been
 * successfully pushed to the blockchain, so that tells us whether the peer is on a fork or on
 * the main chain.
 */
std::vector<item_hash_t> application_impl::get_blockchain_synopsis(const item_hash_t& reference_point,
                                                         uint32_t number_of_blocks_after_reference_point)
{ try {
    std::vector<item_hash_t> synopsis;
    synopsis.reserve(30);
    uint32_t high_block_num;
    uint32_t non_fork_high_block_num;
    uint32_t low_block_num = _chain_db->last_non_undoable_block_num();
    std::vector<block_id_type> fork_history;

    if (reference_point != item_hash_t())
    {
      // the node is asking for a summary of the block chain up to a specified
      // block, which may or may not be on a fork
      // for now, assume it's not on a fork
      if (is_included_block(reference_point))
      {
        // reference_point is a block we know about and is on the main chain
        uint32_t reference_point_block_num = block_header::num_from_id(reference_point);
        assert(reference_point_block_num > 0);
        high_block_num = reference_point_block_num;
        non_fork_high_block_num = high_block_num;

        if (reference_point_block_num < low_block_num)
        {
          // we're on the same fork (at least as far as reference_point) but we've passed
          // reference point and could no longer undo that far if we diverged after that
          // block.  This should probably only happen due to a race condition where
          // the network thread calls this function, and then immediately pushes a bunch of blocks,
          // then the main thread finally processes this function.
          // with the current framework, there's not much we can do to tell the network
          // thread what our current head block is, so we'll just pretend that
          // our head is actually the reference point.
          // this *may* enable us to fetch blocks that we're unable to push, but that should
          // be a rare case (and correctly handled)
          low_block_num = reference_point_block_num;
        }
      }
      else
      {
        // block is a block we know about, but it is on a fork
        try
        {
          fork_history = _chain_db->get_block_ids_on_fork(reference_point);
          // returns a vector where the last element is the common ancestor with the preferred chain,
          // and the first element is the reference point you passed in
          assert(fork_history.size() >= 2);

          if( fork_history.front() != reference_point )
          {
             edump( (fork_history)(reference_point) );
             assert(fork_history.front() == reference_point);
          }
          block_id_type last_non_fork_block = fork_history.back();
          fork_history.pop_back();  // remove the common ancestor
          boost::reverse(fork_history);

          if (last_non_fork_block == block_id_type()) // if the fork goes all the way back to genesis (does graphene's fork db allow this?)
            non_fork_high_block_num = 0;
          else
            non_fork_high_block_num = block_header::num_from_id(last_non_fork_block);

          high_block_num = non_fork_high_block_num + fork_history.size();
          assert(high_block_num == block_header::num_from_id(fork_history.back()));
        }
        catch (const fc::exception& e)
        {
          // unable to get fork history for some reason.  maybe not linked?
          // we can't return a synopsis of its chain
          elog( "Unable to construct a blockchain synopsis for reference hash ${hash}: ${exception}",
                ("hash", reference_point)("exception", e) );
          throw;
        }
        if (non_fork_high_block_num < low_block_num)
        {
          wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
               "(our chains diverge after block #${non_fork_high_block_num} but only undoable to block #${low_block_num})",
               ("low_block_num", low_block_num)
               ("non_fork_high_block_num", non_fork_high_block_num));
          FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is are on a fork I'm unable to switch to");
        }
      }
    }
    else
    {
      // no reference point specified, summarize the whole block chain
      high_block_num = _chain_db->head_block_num();
      non_fork_high_block_num = high_block_num;
      if (high_block_num == 0)
        return synopsis; // we have no blocks
    }

    if( low_block_num == 0)
       low_block_num = 1;

    // at this point:
    // low_block_num is the block before the first block we can undo,
    // non_fork_high_block_num is the block before the fork (if the peer is on a fork, or otherwise it is the same as high_block_num)
    // high_block_num is the block number of the reference block, or the end of the chain if no reference provided

    // true_high_block_num is the ending block number after the network code appends any item ids it
    // knows about that we don't
    uint32_t true_high_block_num = high_block_num + number_of_blocks_after_reference_point;
    do
    {
      // for each block in the synopsis, figure out where to pull the block id from.
      // if it's <= non_fork_high_block_num, we grab it from the main blockchain;
      // if it's not, we pull it from the fork history
      if (low_block_num <= non_fork_high_block_num)
        synopsis.push_back(_chain_db->get_block_id_for_num(low_block_num));
      else
        synopsis.push_back(fork_history[low_block_num - non_fork_high_block_num - 1]);
      low_block_num += (true_high_block_num - low_block_num + 2) / 2;
    }
    while (low_block_num <= high_block_num);

    //idump((synopsis));
    return synopsis;
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

/*
 * Call this after the call to handle_message succeeds.
 *
 * @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
 * @param item_count the number of items known to the node that haven't been sent to handle_item() yet.
 *                   After `item_count` more calls to handle_item(), the node will be in sync
 */
void application_impl::sync_status(uint32_t item_type, uint32_t item_count)
{
   // any status reports to GUI go here
}

/*
 * Call any time the number of connected peers changes.
 */
void application_impl::connection_count_changed(uint32_t c)
{
  // any status reports to GUI go here
}

uint32_t application_impl::get_block_number(const item_hash_t& block_id)
{ try {
   return block_header::num_from_id(block_id);
} FC_CAPTURE_AND_RETHROW( (block_id) ) } // GCOVR_EXCL_LINE

/*
 * Returns the time a block was produced (if block_id = 0, returns genesis time).
 * If we don't know about the block, returns time_point_sec::min()
 */
fc::time_point_sec application_impl::get_block_time(const item_hash_t& block_id)
{ try {
   auto opt_block = _chain_db->fetch_block_by_id( block_id );
   if( opt_block.valid() ) return opt_block->timestamp;
   return fc::time_point_sec::min();
} FC_CAPTURE_AND_RETHROW( (block_id) ) } // GCOVR_EXCL_LINE

item_hash_t application_impl::get_head_block_id() const
{
   return _chain_db->head_block_id();
}

uint32_t application_impl::estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const
{
   return 0; // there are no forks in graphene
}

void application_impl::error_encountered(const std::string& message, const fc::oexception& error)
{
   // notify GUI or something cool
}

uint8_t application_impl::get_current_block_interval_in_seconds() const
{
   FC_ASSERT( _chain_db, "Chain database is not operational" );
   return _chain_db->get_global_properties().parameters.block_interval;
}

void application_impl::shutdown()
{
   ilog( "Shutting down application" );
   if( _websocket_tls_server )
      _websocket_tls_server.reset();
   if( _websocket_server )
      _websocket_server.reset();
   // TODO wait until all connections are closed and messages handled?

   // plugins E.G. witness_plugin may send data to p2p network, so shutdown them first
   ilog( "Shutting down plugins" );
   shutdown_plugins();

   if( _p2p_network )
   {
      ilog( "Disconnecting from P2P network" );
      // FIXME wait() is called in close() but it doesn't block this thread
      _p2p_network->close();
      _p2p_network.reset();
   }
   else
      ilog( "P2P network is disabled" );

   if( _chain_db )
   {
      ilog( "Closing chain database" );
      _chain_db->close();
      _chain_db.reset();
   }
   else
      ilog( "Chain database is not open" );
}

void application_impl::enable_plugin( const string& name )
{
   FC_ASSERT(_available_plugins[name], "Unknown plugin '" + name + "'");
   _active_plugins[name] = _available_plugins[name];
}

void application_impl::initialize_plugins() const
{
   for( const auto& entry : _active_plugins )
   {
      ilog( "Initializing plugin ${name}", ( "name", entry.second->plugin_name() ) );
      entry.second->plugin_initialize( *_options );
      ilog( "Initialized plugin ${name}", ( "name", entry.second->plugin_name() ) );
   }
}

void application_impl::startup_plugins() const
{
   for( const auto& entry : _active_plugins )
   {
      ilog( "Starting plugin ${name}", ( "name", entry.second->plugin_name() ) );
      entry.second->plugin_startup();
      ilog( "Started plugin ${name}", ( "name", entry.second->plugin_name() ) );
   }
}

void application_impl::shutdown_plugins() const
{
   for( const auto& entry : _active_plugins )
   {
      ilog( "Stopping plugin ${name}", ( "name", entry.second->plugin_name() ) );
      entry.second->plugin_shutdown();
      ilog( "Stopped plugin ${name}", ( "name", entry.second->plugin_name() ) );
   }
}

void application_impl::add_available_plugin(std::shared_ptr<graphene::app::abstract_plugin> p)
{
   _available_plugins[p->plugin_name()] = p;
}

void application_impl::set_block_production(bool producing_blocks)
{
   _is_block_producer = producing_blocks;
}

} } } // namespace graphene namespace app namespace detail

namespace graphene { namespace app {

application::application()
   : my(std::make_shared<detail::application_impl>(*this))
{
   //nothing else to do
}

application::~application()
{
   ilog("Application quitting");
   my->shutdown();
}

void application::set_program_options(boost::program_options::options_description& command_line_options,
                                      boost::program_options::options_description& configuration_file_options) const
{
   const auto& default_opts = application_options::get_default();
   configuration_file_options.add_options()
         ("enable-p2p-network", bpo::value<bool>()->implicit_value(true),
          "Whether to enable P2P network (default: true). Note: if delayed_node plugin is enabled, "
          "this option will be ignored and P2P network will always be disabled.")
         ("p2p-accept-incoming-connections", bpo::value<bool>()->implicit_value(true),
          "Whether to accept incoming P2P connections (default: true)")
         ("p2p-endpoint", bpo::value<string>(),
          "The endpoint (local IP address:port) on which the node will listen for P2P connections. "
          "Specify 0.0.0.0 as address to listen on all IP addresses")
         ("p2p-inbound-endpoint", bpo::value<string>(),
          "The endpoint (external IP address:port) that other P2P peers should connect to. "
          "If the address is unknown or dynamic, specify 0.0.0.0")
         ("p2p-connect-to-new-peers", bpo::value<bool>()->implicit_value(true),
          "Whether the node will connect to new P2P peers advertised by other peers (default: true)")
         ("p2p-advertise-peer-algorithm", bpo::value<string>()->implicit_value("all"),
          "Determines which P2P peers are advertised in response to address requests from other peers. "
          "Algorithms: 'all', 'nothing', 'list', exclude_list'. (default: all)")
         ("p2p-advertise-peer-endpoint", bpo::value<vector<string>>()->composing(),
          "The endpoint (IP address:port) of the P2P peer to advertise, only takes effect when algorithm "
          "is 'list' (may specify multiple times)")
         ("p2p-exclude-peer-endpoint", bpo::value<vector<string>>()->composing(),
          "The endpoint (IP address:port) of the P2P peer to not advertise, only takes effect when algorithm "
          "is 'exclude_list' (may specify multiple times)")
         ("seed-node,s", bpo::value<vector<string>>()->composing(),
          "The endpoint (IP address:port) of the P2P peer to connect to on startup (may specify multiple times)")
         ("seed-nodes", bpo::value<string>()->composing(),
          "JSON array of P2P peers to connect to on startup")
         ("checkpoint,c", bpo::value<vector<string>>()->composing(),
          "Pairs of [BLOCK_NUM,BLOCK_ID] that should be enforced as checkpoints.")
         ("rpc-endpoint", bpo::value<string>()->implicit_value("127.0.0.1:8090"),
          "Endpoint for websocket RPC to listen on")
         ("rpc-tls-endpoint", bpo::value<string>()->implicit_value("127.0.0.1:8089"),
          "Endpoint for TLS websocket RPC to listen on")
         ("server-pem,p", bpo::value<string>()->implicit_value("server.pem"),
          "The TLS certificate file for this server")
         ("server-pem-password,P", bpo::value<string>()->implicit_value(""), "Password for this certificate")
         ("proxy-forwarded-for-header", bpo::value<string>()->implicit_value("X-Forwarded-For-Client"),
          "A HTTP header similar to X-Forwarded-For (XFF), used by the RPC server to extract clients' address info, "
          "usually added by a trusted reverse proxy")
         ("genesis-json", bpo::value<boost::filesystem::path>(), "File to read Genesis State from")
         ("dbg-init-key", bpo::value<string>(),
          "Block signing key to use for init witnesses, overrides genesis file, for debug")
         ("api-node-info", bpo::value<string>(),
          "A string defined by the node operator, which can be retrieved via the login_api::get_info API")
         ("api-access", bpo::value<boost::filesystem::path>(), "JSON file specifying API permissions")
         ("io-threads", bpo::value<uint16_t>()->implicit_value(0),
          "Number of IO threads, default to 0 for auto-configuration")
         ("enable-subscribe-to-all", bpo::value<bool>()->implicit_value(true),
          "Whether allow API clients to subscribe to universal object creation and removal events")
         ("enable-standby-votes-tracking", bpo::value<bool>()->implicit_value(true),
          "Whether to enable tracking of votes of standby witnesses and committee members. "
          "Set it to true to provide accurate data to API clients, set to false for slightly better performance.")
         ("api-limit-get-account-history-operations",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_account_history_operations),
          "For history_api::get_account_history_operations to set max limit value")
         ("api-limit-get-account-history",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_account_history),
          "For history_api::get_account_history to set max limit value")
         ("api-limit-get-grouped-limit-orders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_grouped_limit_orders),
          "For orders_api::get_grouped_limit_orders to set max limit value")
         ("api-limit-get-market-history",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_market_history),
          "Maximum number of records to return for the history_api::get_market_history API")
         ("api-limit-get-relative-account-history",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_relative_account_history),
          "For history_api::get_relative_account_history to set max limit value")
         ("api-limit-get-account-history-by-operations",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_account_history_by_operations),
          "For history_api::get_account_history_by_operations to set max limit value")
         ("api-limit-get-asset-holders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_asset_holders),
          "For asset_api::get_asset_holders to set max limit value")
         ("api-limit-get-key-references",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_key_references),
          "For database_api_impl::get_key_references to set max limit value")
         ("api-limit-get-htlc-by",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_htlc_by),
          "For database_api_impl::get_htlc_by_from and get_htlc_by_to to set max limit value")
         ("api-limit-get-full-accounts",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_full_accounts),
          "For database_api_impl::get_full_accounts to set max accounts to query at once")
         ("api-limit-get-full-accounts-lists",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_full_accounts_lists),
          "For database_api_impl::get_full_accounts to set max items to return in the lists")
         ("api-limit-get-full-accounts-subscribe",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_full_accounts_subscribe),
          "Maximum number of accounts allowed to subscribe per connection with the get_full_accounts API")
         ("api-limit-get-top-voters",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_top_voters),
          "For database_api_impl::get_top_voters to set max limit value")
         ("api-limit-get-call-orders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_call_orders),
          "For database_api_impl::get_call_orders and get_call_orders_by_account to set max limit value")
         ("api-limit-get-settle-orders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_settle_orders),
          "For database_api_impl::get_settle_orders and get_settle_orders_by_account to set max limit value")
         ("api-limit-get-assets",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_assets),
          "For database_api_impl::list_assets and get_assets_by_issuer to set max limit value")
         ("api-limit-get-limit-orders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_limit_orders),
          "For database_api_impl::get_limit_orders to set max limit value")
         ("api-limit-get-limit-orders-by-account",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_limit_orders_by_account),
          "For database_api_impl::get_limit_orders_by_account to set max limit value")
         ("api-limit-get-order-book",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_order_book),
          "For database_api_impl::get_order_book to set max limit value")
         ("api-limit-list-htlcs",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_list_htlcs),
          "For database_api_impl::list_htlcs to set max limit value")
         ("api-limit-lookup-accounts",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_lookup_accounts),
          "For database_api_impl::lookup_accounts to set max limit value")
         ("api-limit-lookup-witness-accounts",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_lookup_witness_accounts),
          "For database_api_impl::lookup_witness_accounts to set max limit value")
         ("api-limit-lookup-committee-member-accounts",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_lookup_committee_member_accounts),
          "For database_api_impl::lookup_committee_member_accounts to set max limit value")
         ("api-limit-lookup-vote-ids",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_lookup_vote_ids),
          "For database_api_impl::lookup_vote_ids to set max limit value")
         ("api-limit-get-account-limit-orders",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_account_limit_orders),
          "For database_api_impl::get_account_limit_orders to set max limit value")
         ("api-limit-get-collateral-bids",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_collateral_bids),
          "For database_api_impl::get_collateral_bids to set max limit value")
         ("api-limit-get-top-markets",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_top_markets),
          "For database_api_impl::get_top_markets to set max limit value")
         ("api-limit-get-trade-history",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_trade_history),
          "For database_api_impl::get_trade_history to set max limit value")
         ("api-limit-get-trade-history-by-sequence",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_trade_history_by_sequence),
          "For database_api_impl::get_trade_history_by_sequence to set max limit value")
         ("api-limit-get-withdraw-permissions-by-giver",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_withdraw_permissions_by_giver),
          "For database_api_impl::get_withdraw_permissions_by_giver to set max limit value")
         ("api-limit-get-withdraw-permissions-by-recipient",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_withdraw_permissions_by_recipient),
          "For database_api_impl::get_withdraw_permissions_by_recipient to set max limit value")
         ("api-limit-get-tickets",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_tickets),
          "Set maximum limit value for database APIs which query for tickets")
         ("api-limit-get-liquidity-pools",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_liquidity_pools),
          "Set maximum limit value for database APIs which query for liquidity pools")
         ("api-limit-get-liquidity-pool-history",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_liquidity_pool_history),
          "Set maximum limit value for APIs which query for history of liquidity pools")
         ("api-limit-get-samet-funds",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_samet_funds),
          "Set maximum limit value for database APIs which query for SameT Funds")
         ("api-limit-get-credit-offers",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_credit_offers),
          "Set maximum limit value for database APIs which query for credit offers or credit deals")
         ("api-limit-get-storage-info",
          bpo::value<uint32_t>()->default_value(default_opts.api_limit_get_storage_info),
          "Set maximum limit value for APIs which query for account storage info")
         ;
   command_line_options.add(configuration_file_options);
   command_line_options.add_options()
         ("replay-blockchain", "Rebuild object graph by replaying all blocks without validation")
         ("revalidate-blockchain", "Rebuild object graph by replaying all blocks with full validation")
         ("resync-blockchain", "Delete all blocks and re-sync with network from scratch")
         ("force-validate", "Force validation of all transactions during normal operation")
         ("genesis-timestamp", bpo::value<uint32_t>(),
          "Replace timestamp from genesis.json with current time plus this many seconds (experts only!)")
         ;
   command_line_options.add(_cli_options);
   configuration_file_options.add(_cfg_options);
}

void application::initialize(const fc::path& data_dir,
                             std::shared_ptr<boost::program_options::variables_map> options) const
{
   ilog( "Initializing application" );
   my->initialize( data_dir, options );
   ilog( "Done initializing application" );
}

void application::startup()
{
   try {
      ilog( "Starting up application" );
      my->startup();
      ilog( "Done starting up application" );
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   } catch ( ... ) {
      elog( "unexpected exception" );
      throw;
   }
}

void application::set_api_limit()
{
   try {
      my->set_api_limit();
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   } catch ( ... ) {
      elog( "unexpected exception" );
      throw;
   }
}
std::shared_ptr<abstract_plugin> application::get_plugin(const string& name) const
{
   return my->_active_plugins[name];
}

bool application::is_plugin_enabled(const string& name) const
{
   return my->is_plugin_enabled(name);
}

net::node_ptr application::p2p_node()
{
   return my->_p2p_network;
}

std::shared_ptr<chain::database> application::chain_database() const
{
   return my->_chain_db;
}

void application::set_block_production(bool producing_blocks)
{
   my->set_block_production(producing_blocks);
}

optional< api_access_info > application::get_api_access_info( const string& username )const
{
   return my->get_api_access_info( username );
}

void application::set_api_access_info(const string& username, api_access_info&& permissions)
{
   my->set_api_access_info(username, std::move(permissions));
}

bool application::is_finished_syncing() const
{
   return my->_is_finished_syncing;
}

void application::enable_plugin(const string& name) const
{
   my->enable_plugin(name);
}

void application::add_available_plugin(std::shared_ptr<graphene::app::abstract_plugin> p) const
{
   my->add_available_plugin(p);
}

const application_options& application::get_options() const
{
   return my->_app_options;
}

const string& application::get_node_info() const
{
   return my->_node_info;
}

// namespace detail
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::app::application_options )
