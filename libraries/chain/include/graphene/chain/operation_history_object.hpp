/*
 * AcloudBank
 */
#pragma once

#include <graphene/protocol/operations.hpp>
#include <graphene/db/object.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

   /**
    * @brief tracks the history of all logical operations on blockchain state
    * @ingroup object
    * @ingroup implementation
    *
    *  All operations and virtual operations result in the creation of an
    *  operation_history_object that is maintained on disk as a stack.  Each
    *  real or virtual operation is assigned a unique ID / sequence number that
    *  it can be referenced by.
    *
    *  @note  by default these objects are not tracked, the account_history_plugin must
    *  be loaded fore these objects to be maintained.
    *
    *  @note  this object is READ ONLY it can never be modified
    */
   class operation_history_object : public abstract_object<operation_history_object,
                                              protocol_ids, operation_history_object_type>
   {
      public:
         explicit operation_history_object( const operation& o ):op(o){}
         operation_history_object() = default;
         operation_history_object( const operation& o, uint32_t bn, uint16_t tib, uint16_t oit, uint32_t vo, bool iv,
                                   const time_point_sec& bt )
         : op(o), block_num(bn), trx_in_block(tib), op_in_trx(oit), virtual_op(vo), is_virtual(iv), block_time(bt) {}

         operation         op;
         operation_result  result;
         /** the block that caused this operation */
         uint32_t          block_num = 0;
         /** the transaction in the block */
         uint16_t          trx_in_block = 0;
         /** the operation within the transaction */
         uint16_t          op_in_trx = 0;
         /** any virtual operations implied by operation in block */
         uint32_t          virtual_op = 0;
         /** Whether this is a virtual operation */
         bool              is_virtual = false;
         /** The timestamp of the block that caused this operation */
         time_point_sec    block_time;
   };

   /**
    *  @brief a node in a linked list of operation_history_objects
    *  @ingroup implementation
    *  @ingroup object
    *
    *  Account history is important for users and wallets even though it is
    *  not part of "core validation".   Account history is maintained as
    *  a linked list stored on disk in a stack.  Each account will point to the
    *  most recent account history object by ID.  When a new operation relativent
    *  to that account is processed a new account history object is allcoated at
    *  the end of the stack and intialized to point to the prior object.
    *
    *  This data is never accessed as part of chain validation and therefore
    *  can be kept on disk as a memory mapped file.  Using a memory mapped file
    *  will help the operating system better manage / cache / page files and
    *  also accelerates load time.
    *
    *  When the transaction history for a particular account is requested the
    *  linked list can be traversed with relatively effecient disk access because
    *  of the use of a memory mapped stack.
    */
   class account_history_object :  public abstract_object<account_history_object,
                                             implementation_ids, impl_account_history_object_type>
   {
      public:
         account_id_type                      account; /// the account this operation applies to
         operation_history_id_type            operation_id;
         uint64_t                             sequence = 0; /// the operation position within the given account
         account_history_id_type              next;
   };

   struct by_block;
   struct by_time;

   using operation_history_mlti_idx_type = multi_index_container<
      operation_history_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_block>,
            composite_key< operation_history_object,
               member< operation_history_object, uint32_t, &operation_history_object::block_num>,
               member< operation_history_object, uint16_t, &operation_history_object::trx_in_block>,
               member< operation_history_object, uint16_t, &operation_history_object::op_in_trx>,
               member< operation_history_object, uint32_t, &operation_history_object::virtual_op>
            >
         >,
         ordered_unique< tag<by_time>,
            composite_key< operation_history_object,
               member< operation_history_object, time_point_sec, &operation_history_object::block_time>,
               member< object, object_id_type, &object::id >
            >,
            composite_key_compare<
               std::greater< time_point_sec >,
               std::greater< object_id_type >
            >
         >
      >
   >;

   using operation_history_index = generic_index< operation_history_object, operation_history_mlti_idx_type >;

   struct by_seq;
   struct by_op;
   struct by_opid;

   using account_history_multi_idx_type = multi_index_container<
      account_history_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_seq>,
            composite_key< account_history_object,
               member< account_history_object, account_id_type, &account_history_object::account>,
               member< account_history_object, uint64_t, &account_history_object::sequence>
            >
         >,
         ordered_unique< tag<by_op>,
            composite_key< account_history_object,
               member< account_history_object, account_id_type, &account_history_object::account>,
               member< account_history_object, operation_history_id_type, &account_history_object::operation_id>
            >,
            composite_key_compare<
               std::less< account_id_type >,
               std::greater< operation_history_id_type >
            >
         >,
         ordered_non_unique< tag<by_opid>,
            member< account_history_object, operation_history_id_type, &account_history_object::operation_id>
         >
      >
   >;

   using account_history_index = generic_index< account_history_object, account_history_multi_idx_type >;


} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::operation_history_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::account_history_object)

FC_REFLECT_TYPENAME( graphene::chain::operation_history_object )
FC_REFLECT_TYPENAME( graphene::chain::account_history_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::operation_history_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::account_history_object )
