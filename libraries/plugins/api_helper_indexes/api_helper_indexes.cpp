/*
 * Acloudbank
 */

#include <graphene/api_helper_indexes/api_helper_indexes.hpp>
#include <graphene/chain/liquidity_pool_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/chain_property_object.hpp>

namespace graphene { namespace api_helper_indexes {

void amount_in_collateral_index::object_inserted( const object& objct )
{ try {
   const call_order_object& o = static_cast<const call_order_object&>( objct );

   {
      auto itr = in_collateral.find( o.collateral_type() );
      if( itr == in_collateral.end() )
         in_collateral[o.collateral_type()] = o.collateral;
      else
         itr->second += o.collateral;
   }

   {
      auto itr = backing_collateral.find( o.debt_type() );
      if( itr == backing_collateral.end() )
         backing_collateral[o.debt_type()] = o.collateral;
      else
         itr->second += o.collateral;
   }

} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

void amount_in_collateral_index::object_removed( const object& objct )
{ try {
   const call_order_object& o = static_cast<const call_order_object&>( objct );

   {
      auto itr = in_collateral.find( o.collateral_type() );
      if( itr != in_collateral.end() ) // should always be true
         itr->second -= o.collateral;
   }

   {
      auto itr = backing_collateral.find( o.debt_type() );
      if( itr != backing_collateral.end() ) // should always be true
         itr->second -= o.collateral;
   }

} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

void amount_in_collateral_index::about_to_modify( const object& objct )
{ try {
   object_removed( objct );
} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

void amount_in_collateral_index::object_modified( const object& objct )
{ try {
   object_inserted( objct );
} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

share_type amount_in_collateral_index::get_amount_in_collateral( const asset_id_type& asst )const
{ try {
   auto itr = in_collateral.find( asst );
   if( itr == in_collateral.end() ) return 0;
   return itr->second;
} FC_CAPTURE_AND_RETHROW( (asst) ) } // GCOVR_EXCL_LINE

share_type amount_in_collateral_index::get_backing_collateral( const asset_id_type& asst )const
{ try {
   auto itr = backing_collateral.find( asst );
   if( itr == backing_collateral.end() ) return 0;
   return itr->second;
} FC_CAPTURE_AND_RETHROW( (asst) ) } // GCOVR_EXCL_LINE

void asset_in_liquidity_pools_index::object_inserted( const object& objct )
{ try {
   const auto& o = static_cast<const liquidity_pool_object&>( objct );
   const liquidity_pool_id_type pool_id = o.get_id();
   asset_in_pools_map[ o.asset_a ].insert( pool_id ); // Note: [] operator will create an entry if not found
   asset_in_pools_map[ o.asset_b ].insert( pool_id );
} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

void asset_in_liquidity_pools_index::object_removed( const object& objct )
{ try {
   const auto& o = static_cast<const liquidity_pool_object&>( objct );
   const liquidity_pool_id_type pool_id = o.get_id();
   asset_in_pools_map[ o.asset_a ].erase( pool_id );
   asset_in_pools_map[ o.asset_b ].erase( pool_id );
   // Note: do not erase entries with an empty set from the map in order to avoid read/write race conditions
} FC_CAPTURE_AND_RETHROW( (objct) ) } // GCOVR_EXCL_LINE

void asset_in_liquidity_pools_index::about_to_modify( const object& objct )
{
   // this secondary index has no interest in the modifications, nothing to do here
}

void asset_in_liquidity_pools_index::object_modified( const object& objct )
{
   // this secondary index has no interest in the modifications, nothing to do here
}

const flat_set<liquidity_pool_id_type>& asset_in_liquidity_pools_index::get_liquidity_pools_by_asset(
            const asset_id_type& a )const
{
   auto itr = asset_in_pools_map.find( a );
   if( itr != asset_in_pools_map.end() )
      return itr->second;
   return empty_set;
}

namespace detail
{

class api_helper_indexes_impl
{
   public:
      explicit api_helper_indexes_impl(api_helper_indexes& _plugin)
         : _self( _plugin )
      {  }

      graphene::chain::database& database()
      {
         return _self.database();
      }

   private:
      api_helper_indexes& _self;
};

} // end namespace detail

api_helper_indexes::api_helper_indexes(graphene::app::application& app) :
   plugin(app),
   my( std::make_unique<detail::api_helper_indexes_impl>(*this) )
{
   // Nothing else to do
}

api_helper_indexes::~api_helper_indexes() = default;

std::string api_helper_indexes::plugin_name()const
{
   return "api_helper_indexes";
}
std::string api_helper_indexes::plugin_description()const
{
   return "Provides some helper indexes used by various API calls";
}

void api_helper_indexes::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
}

void api_helper_indexes::plugin_initialize(const boost::program_options::variables_map& options)
{
}

void api_helper_indexes::plugin_startup()
{
   ilog("api_helper_indexes: plugin_startup() begin");
   amount_in_collateral_idx = database().add_secondary_index< primary_index<call_order_index>,
                                                              amount_in_collateral_index >();
   for( const auto& call : database().get_index_type<call_order_index>().indices() )
      amount_in_collateral_idx->object_inserted( call );

   auto& account_members = *database().add_secondary_index< primary_index<account_index>, account_member_index >();
   for( const auto& account : database().get_index_type< account_index >().indices() )
      account_members.object_inserted( account );

   auto& approvals = *database().add_secondary_index< primary_index<proposal_index>, required_approval_index >();
   for( const auto& proposal : database().get_index_type< proposal_index >().indices() )
      approvals.object_inserted( proposal );

   asset_in_liquidity_pools_idx = database().add_secondary_index< primary_index<liquidity_pool_index>,
                                                        asset_in_liquidity_pools_index >();
   for( const auto& pool : database().get_index_type<liquidity_pool_index>().indices() )
      asset_in_liquidity_pools_idx->object_inserted( pool );

   next_object_ids_idx = database().add_secondary_index< primary_index<simple_index<chain_property_object>>,
                                                        next_object_ids_index >();
   refresh_next_ids();
   // connect with no group specified to process after the ones with a group specified
   database().applied_block.connect( [this]( const chain::signed_block& )
   {
      refresh_next_ids();
      _next_ids_map_initialized = true;
   });
}

void api_helper_indexes::refresh_next_ids()
{
   const auto& db = database();
   if( _next_ids_map_initialized )
   {
      for( auto& item : next_object_ids_idx->_next_ids )
      {
         item.second = db.get_index( item.first.first, item.first.second ).get_next_id();
      }
      return;
   }

   // Assuming that all indexes have been created when processing the first block,
   // for better performance, only do this twice, one on plugin startup, the other on the first block.
   size_t count = 0;
   size_t failed_count = 0;
   for( uint8_t space = 0; space < chain::database::_index_size; ++space )
   {
      for( uint8_t type = 0; type < chain::database::_index_size; ++type )
      {
         try
         {
            const auto& idx = db.get_index( space, type );
            next_object_ids_idx->_next_ids[ std::make_pair( space, type ) ] = idx.get_next_id();
            ++count;
         }
         catch( const fc::exception& )
         {
            ++failed_count;
         }
      }
   }
   dlog( "${count} indexes detected, ${failed_count} not found", ("count",count)("failed_count",failed_count) );
}

object_id_type next_object_ids_index::get_next_id( uint8_t space_id, uint8_t type_id ) const
{ try {
   return _next_ids.at( std::make_pair( space_id, type_id ) );
} FC_CAPTURE_AND_RETHROW( (space_id)(type_id) ) } // GCOVR_EXCL_LINE

} }
