
namespace Seahorse {

	public abstract class Object : GLib.Object {

		/* PREDICATE -------------------------------------------------- */
		
	        
		public delegate bool PredicateFunc (Object obj);
	
		public struct Predicate {
			public GLib.Quark tag;
			public GLib.Quark id;
			public Location location;
			public Usage usage;
			public uint flags;
			public uint nflags;
			public weak Source? source;
			public PredicateFunc? custom;
			
			public bool match(Object obj)
			{
				/* Check all the fields */
				if (this.tag != 0 && (this.tag != obj._tag))
					return false;
				if (this.id != 0 && (this.id != obj._id))
					return false;
				if (this.location != 0 && (this.location != obj._location))
					return false;
				if (this.usage != 0 && (this.usage != obj._usage))
					return false;
				if (this.flags != 0 && (this.flags & obj._flags) == 0)
					return false;
				if (this.nflags != 0 && (this.nflags & obj._flags) != 0)
					return false;
				if (this.source != null && (this.source != obj._source))
					return false;
				
				/* And any custom stuff */
				if (this.custom != null && !this.custom(obj))
					return false;
				
				return true;
			}
		}
		
		/* INTERFACE -------------------------------------------------- */

		/* TODO: Are we sure we need this? */
        	public static enum Change {
        		ALL = 1,
        		LOCATION,
        		PREFERRED,
        		MAX 
        	}

		protected GLib.Quark _tag;
		public GLib.Quark tag { 
			get { return _tag; }
		}
		
		protected GLib.Quark _id;
		public GLib.Quark id { 
			get { return _id; }
		}
		
		protected Location _location;
		public Location location { 
			get { return _location; }
			set { _location = value; fire_changed (Change.LOCATION); }
		}
		
		protected Usage _usage;
		public Usage usage {
			get { return _usage; }
		}
		
		protected uint _flags;
		public uint flags {
			get { return _flags; }
		}
		
		private weak Source _source;
		public Source source {
			get { return _source; }
			set { 	
				if (_source != null)
					_source.remove_weak_pointer (&_source);
				_source = value;
				if (_source != null)
					_source.add_weak_pointer (&_source);
			}
		}
		
		private weak Object _preferred;
		public Object preferred {
			get { return _preferred; }
			set { 	
				if (_preferred == value)
					return;
				if (_preferred != null)
					_preferred.remove_weak_pointer (&_preferred);
				_preferred = value;
				if (_preferred != null)
					_preferred.add_weak_pointer (&_preferred);
				fire_changed (Change.PREFERRED);
			}
		}
		
		public weak Context attached_to;
		
		public signal void changed(Change what);
		public signal void hierarchy();
		public signal void destroy();
		
		public abstract string# display_name { get; }
		public abstract string# markup { get; }
		public abstract weak string# stock_id { get; }
		
		public Object? parent { 
			get {
				return _parent;
			}
			set {
				assert (_parent != this);
				if (value == _parent)
					return;
				
				/* Set the new parent/child relationship */
				if(_parent != null)
					_parent.unregister_child(this);
				if(value != null)
					value.register_child(this);
				assert(_parent == value);
				
				/* Fire a signal to let watchers know this changed */
				this.hierarchy();
			}
		}
		
		public GLib.List<weak Object> get_children()
		{
			return _children.copy();
		}

		protected void fire_changed(Change what)
		{
			this.changed(what);
		}

		/* IMPLEMENTATION --------------------------------------------- */
		
		private weak Object? _parent;
		private weak GLib.List<weak Object> _children;
		
		construct {
			_parent = null;
			_children = null;
		}
		
		~Object () {

			/* Fire our destroy signal, so that any watchers can go away */
			this.destroy();

			if (_source != null)
				_source.remove_weak_pointer (&_source);
			if (_preferred != null)
				_source.remove_weak_pointer (&_preferred);

			/* 
			 * When an object is destroyed, we reparent all
			 * children to this objects parent. If no parent
			 * of this object, all children become root objects.
			 */
			
			Object new_parent = this._parent;
			foreach (Object child in _children) {
				unregister_child(child);
				if (new_parent != null)
					new_parent.register_child(child);
				
				/* Fire signal to let anyone know this has moved */
				child.hierarchy();
			}
		}
		
		private void register_child(Object child)
		{
			assert(child._parent == null);
			child._parent = this;
			_children.append(child);
		}
		
		private void unregister_child(Object child)
		{
			assert(child._parent == this);
			child._parent = null;
			_children.remove(child);
		}
	}
}
