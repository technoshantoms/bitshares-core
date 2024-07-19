
#pragma once
#include <graphene/db/object.hpp>
#include <deque>
#include <fc/exception/exception.hpp>

namespace graphene { namespace db {

   class object_database;

   struct undo_state
   {
      std::unordered_map<object_id_type, std::unique_ptr<object> > old_values;
      std::unordered_map<object_id_type, object_id_type>           old_index_next_ids;
      std::unordered_set<object_id_type>                           new_ids;
      std::unordered_map<object_id_type, std::unique_ptr<object> > removed;
   };


   /**
    * @class undo_database
    * @brief tracks changes to the state and allows changes to be undone
    *
    */
   class undo_database
   {
      public:
         explicit undo_database( object_database& db ):_db(db){}

         class session
         {
            public:
               session( session&& mv )
               :_db(mv._db),_apply_undo(mv._apply_undo)
               {
                  mv._apply_undo = false;
               }
               ~session();
               void commit() { _apply_undo = false; _db.commit();  }
               void undo()   { if( _apply_undo ) _db.undo(); _apply_undo = false; }
               void merge()  { if( _apply_undo ) _db.merge(); _apply_undo = false; }

               session& operator = ( session&& mv )
               { try {
                  if( this == &mv ) return *this;
                  if( _apply_undo ) _db.undo();
                  _apply_undo = mv._apply_undo;
                  mv._apply_undo = false;
                  return *this;
               } FC_CAPTURE_AND_RETHROW() }

            private:
               friend class undo_database;

               explicit session(undo_database& db, bool disable_on_exit = false)
               : _db(db),_disable_on_exit(disable_on_exit) {}

               undo_database& _db;
               bool _apply_undo = true;
               bool _disable_on_exit = false;
         };

         void    disable();
         void    enable();
         bool    enabled()const { return !_disabled; }

         session start_undo_session( bool force_enable = false );
         /**
          * This should be called just after an object is created
          */
         void on_create( const object& obj );
         /**
          * This should be called just before an object is modified
          *
          * If it's a new object as of this undo state, its pre-modification value is not stored, because prior to this
          * undo state, it did not exist. Any modifications in this undo state are irrelevant, as the object will simply
          * be removed if we undo.
          */
         void on_modify( const object& obj );
         /**
          * This should be called just before an object is removed.
          *
          * If it's a new object as of this undo state, its pre-removal value is not stored, because prior to this undo
          * state, it did not exist. Now that it's been removed, it doesn't exist again, so nothing has happened.
          * Instead, remove it from the list of newly created objects (which must be deleted if we undo), as we don't
          * want to re-delete it if this state is undone.
          */
         void on_remove( const object& obj );

         /**
          *  Removes the last committed session,
          *  note... this is dangerous if there are
          *  active sessions... thus active sessions should
          *  track
          */
         void pop_commit();

         std::size_t size()const { return _stack.size(); }
         void set_max_size(size_t new_max_size) { _max_size = new_max_size; }
         size_t max_size()const { return _max_size; }
         uint32_t active_sessions()const { return _active_sessions; }

         const undo_state& head()const;

      private:
         void undo();
         void merge();
         void commit();

         uint32_t                _active_sessions = 0;
         bool                    _disabled = true;
         std::deque<undo_state>  _stack;
         object_database&        _db;
         size_t                  _max_size = 256;
   };

} } // graphene::db
