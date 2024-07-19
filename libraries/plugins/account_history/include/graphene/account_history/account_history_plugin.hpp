// AcloudBank
#pragma once

#include <graphene/app/plugin.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace account_history {
   using namespace chain;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef ACCOUNT_HISTORY_SPACE_ID
#define ACCOUNT_HISTORY_SPACE_ID 4
#endif

enum account_history_object_type
{
   exceeded_account_object_type = 0
};

/// This struct tracks accounts that have exceeded the max-ops-per-account limit
struct exceeded_account_object : public abstract_object<exceeded_account_object,
                                           ACCOUNT_HISTORY_SPACE_ID, exceeded_account_object_type>
{
   /// The ID of the account
   account_id_type account_id;
   /// The height of the block containing the oldest (not yet removed) operation related to this account
   uint32_t        block_num;
};

struct by_account;
struct by_block_num;
using exceeded_account_multi_idx_type = multi_index_container<
   exceeded_account_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_account>,
         member< exceeded_account_object, account_id_type, &exceeded_account_object::account_id > >,
      ordered_unique< tag<by_block_num>,
         composite_key<
            exceeded_account_object,
            member< exceeded_account_object, uint32_t, &exceeded_account_object::block_num >,
            member< object, object_id_type, &object::id >
         >
      >
   >
>;

using exceeded_account_index = generic_index< exceeded_account_object, exceeded_account_multi_idx_type >;

namespace detail
{
    class account_history_plugin_impl;
}

class account_history_plugin : public graphene::app::plugin
{
   public:
      explicit account_history_plugin(graphene::app::application& app);
      ~account_history_plugin() override;

      std::string plugin_name()const override;
      void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      void plugin_initialize(const boost::program_options::variables_map& options) override;
      void plugin_startup() override;

      flat_set<account_id_type> tracked_accounts()const;

   private:
      std::unique_ptr<detail::account_history_plugin_impl> my;
};

} } //graphene::account_history

FC_REFLECT_DERIVED( graphene::account_history::exceeded_account_object, (graphene::db::object),
                    (account_id)(block_num) )
