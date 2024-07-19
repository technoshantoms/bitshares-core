/*
 * Acloudbank
 */
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/protocol/types.hpp>

#include <fc/thread/future.hpp>
#include <fc/container/flat.hpp>

namespace graphene { namespace debug_witness_plugin {

class debug_witness_plugin : public graphene::app::plugin {
public:
   using graphene::app::plugin::plugin;
   ~debug_witness_plugin() override;

   std::string plugin_name()const override;

   void plugin_set_program_options(
      boost::program_options::options_description &command_line_options,
      boost::program_options::options_description &config_file_options
      ) override;

   void plugin_initialize( const boost::program_options::variables_map& options ) override;
   void plugin_startup() override;
   void plugin_shutdown() override;

   void set_json_object_stream( const std::string& filename );
   void flush_json_object_stream();

private:
   void cleanup();

   void on_changed_objects( const std::vector<graphene::db::object_id_type>& ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts );
   void on_removed_objects( const std::vector<graphene::db::object_id_type>& ids, const std::vector<const graphene::db::object*> objs, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts );
   void on_applied_block( const graphene::chain::signed_block& b );

   boost::program_options::variables_map _options;

   std::map<chain::public_key_type, fc::ecc::private_key, chain::pubkey_comparator> _private_keys;

   std::shared_ptr< std::ofstream > _json_object_stream;
   boost::signals2::scoped_connection _applied_block_conn;
   boost::signals2::scoped_connection _changed_objects_conn;
   boost::signals2::scoped_connection _removed_objects_conn;
};

} } //graphene::debug_witness_plugin
