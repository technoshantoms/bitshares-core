/*
 * AcloudBank
 */
#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/types.hpp>

namespace graphene { namespace chain {

   class account_object;
   class asset_object;
   class asset_bitasset_data_object;
   class call_order_object;
   class limit_order_object;
   class collateral_bid_object;

   class limit_order_create_evaluator : public evaluator<limit_order_create_evaluator>
   {
      public:
         using operation_type = limit_order_create_operation;

         void_result do_evaluate( const limit_order_create_operation& o );
         object_id_type do_apply( const limit_order_create_operation& o ) const;

         /** override the default behavior defined by generic_evaluator
          */
         void convert_fee() override;

         /** override the default behavior defined by generic_evaluator which is to
          * post the fee to fee_paying_account_stats.pending_fees
          */
         void pay_fee() override;

      private:
         share_type                          _deferred_fee  = 0;
         asset                               _deferred_paid_fee;
         const account_object*               _seller        = nullptr;
         const asset_object*                 _sell_asset    = nullptr;
         const asset_object*                 _receive_asset = nullptr;
   };

   class limit_order_update_evaluator : public evaluator<limit_order_update_evaluator>
   {
   public:
       using operation_type = limit_order_update_operation;

       void_result do_evaluate(const limit_order_update_operation& o);
       void_result do_apply(const limit_order_update_operation& o);

       /** override the default behavior defined by generic_evaluator
        */
       void convert_fee() override;

       /** override the default behavior defined by generic_evaluator which is to
        * post the fee to fee_paying_account_stats.pending_fees
        */
       void pay_fee() override;

   private:
       void process_deferred_fee();
       /// Check if the linked take profit order is still compatible with the current order after update
       bool is_linked_tp_order_compatible( const limit_order_update_operation& o ) const;

       share_type                _deferred_fee;
       asset                     _deferred_paid_fee;
       const limit_order_object* _order = nullptr;
       const account_statistics_object* _seller_acc_stats = nullptr;
   };

   class limit_order_cancel_evaluator : public evaluator<limit_order_cancel_evaluator>
   {
      public:
         using operation_type = limit_order_cancel_operation;

         void_result do_evaluate( const limit_order_cancel_operation& o );
         asset do_apply( const limit_order_cancel_operation& o ) const;

      private:
         const limit_order_object* _order = nullptr;
   };

   class call_order_update_evaluator : public evaluator<call_order_update_evaluator>
   {
      public:
         using operation_type = call_order_update_operation;

         void_result do_evaluate( const call_order_update_operation& o );
         object_id_type do_apply( const call_order_update_operation& o );

      private:
         bool _closing_order = false;
         const asset_object* _debt_asset = nullptr;
         const call_order_object* call_ptr = nullptr;
         share_type new_debt;
         share_type new_collateral;
         const asset_bitasset_data_object* _bitasset_data = nullptr;
         const asset_dynamic_data_object*  _dynamic_data_obj = nullptr;
   };

   class bid_collateral_evaluator : public evaluator<bid_collateral_evaluator>
   {
      public:
         using operation_type = bid_collateral_operation;

         void_result do_evaluate( const bid_collateral_operation& o );
         void_result do_apply( const bid_collateral_operation& o ) const;

      private:
         const asset_object* _debt_asset = nullptr;
         const asset_bitasset_data_object* _bitasset_data = nullptr;
         const collateral_bid_object* _bid = nullptr;
   };

} } // graphene::chain
