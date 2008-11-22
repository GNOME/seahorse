
/* TODO: Load objects other than certificates. */

namespace Seahorse.Pkcs11 {
	
	static const ulong[] ATTRIBUTE_TYPES = {
		P11.CKA_LABEL,
		P11.CKA_ID,
		P11.CKA_CLASS,
		P11.CKA_TOKEN,
		P11.CKA_GNOME_USER_TRUST,
		P11.CKA_START_DATE,
		P11.CKA_END_DATE,
		P11.CKA_EXTRACTABLE,
		P11.CKA_VALUE
	};
	
	public class Source : Seahorse.Source {
		
		private GP11.Slot _slot;
		public GP11.Slot slot {
			get { return _slot; }
			construct { _slot = value; }
		}
		
		public Location location {
			get { return Location.LOCAL; } 
		}
		
		public GLib.Quark key_type {
			get { return Pkcs11.TYPE; }
		}
		
		public string key_desc {
			get { return _("X509 Certificate"); }
		}
		
		/* ------------------------------------------------------------------------------
		 * Base class for several operations that all load objects (refresh, load, import)
		 */ 
		
		private abstract class Updater : Operation {

			/* For cancelling the GP11 operations */
			private GLib.Cancellable _cancellable;
			public GLib.Cancellable cancellable { 
				get { return _cancellable; }
			}

			/* The source we're updating into */
			protected Source _source;
			public Source source {
				get { return _source; }
				construct { _source = value; }
			}
			
			/* The PKCS#11 session to load from */
			protected GP11.Session _session;            

			/* The list of objects that we found, need to load attrs for */
			private GLib.List<GP11.Object> _objects;  
			
			/* Total number of objects that loading/loaded loaded */
			private int _total;

			construct {
				_cancellable = new Cancellable();
				_session = null;
				
				/* Step 1. Load the session */
				_source.slot.open_session_async(P11.CKF_RW_SESSION, _cancellable, on_open_session);
				mark_start ();
			}
			
			protected void complete(GLib.Error? err = null) {
				if (err == null) {
					mark_done(false, null);
				} else {
					if (err.code == P11.CKR_FUNCTION_CANCELED)
						mark_done(true, null);
					else
						mark_done(false, err);
				} 
			}
			
			private void on_open_session(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Slot slot = (GP11.Slot)obj;
				
				/* Check for errors */
				try {
					_session = slot.open_session_finish(result);
				} catch (GLib.Error err) {
					complete(err);
					return;
				}

				/* Step 2. Load all the objects that we want */
				load_objects();
			}
			
			protected abstract void load_objects();
			
			protected void loaded_objects(GLib.List<GP11.Object> objects) {
				
				_objects = new GLib.List<GP11.Object>();
				foreach(var object in objects)
					_objects.append(object);
				_total = (int)_objects.length();

				/* Step 3. Load information for each object */
				do_get_attributes();
			}
			
			private void do_get_attributes() {
				
				/* No more objects to load? */
				if (_objects.length() == 0) {
					complete();
					
				/* Load the next object */ 
				} else {
					mark_progress(_("Loading..."), (int)_objects.length() - _total, _total);
		
					var object = _objects.data;
					_objects.remove(object);
					object.get_attributes_async(ATTRIBUTE_TYPES, _cancellable, on_get_attributes);
				}
			}
			
			private void on_get_attributes(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Object object = (GP11.Object)obj;
				GP11.Attributes attrs;
				
				/* Get the results */
				try {
					attrs = object.get_attributes_finish(result);
				} catch (GLib.Error err) {
					/* Ignore objects that have gone away */
					if (err.code != P11.CKR_OBJECT_HANDLE_INVALID) {
						complete(err);
						return;
					}
				}
				
				/* Process this object */
				_source.receive_object(object, attrs);
				
				/* Do the next object */
				do_get_attributes();
			}
			
			/* Cancel the operation */
			public override void cancel() {
				_cancellable.cancel();
			}
		}
		
		/* ------------------------------------------------------------------------------- 
		 * REFRESH OPREATION
		 */
		
		private class Refresher : Updater {
			
			/* Checks so we can determine those that are missing */
			private GLib.HashTable<uint, Seahorse.Object> _checks;
			
			public Refresher(Source source) {
				this.source = source;
			}

			construct {

				/* Load all the current item into the check table */
				_checks = new GLib.HashTable<uint, Seahorse.Object>(GLib.direct_hash, GLib.direct_equal);
				foreach(var object in Context.for_app().get_objects(_source)) {
					if (object.get_type() == typeof(Pkcs11.Certificate)) {
						Certificate certificate = (Pkcs11.Certificate)object;
						if (certificate.pkcs11_object != null)
							_checks.insert(certificate.pkcs11_object.handle, object);
					}
				}
			}
			
			/* Search for all token objects */
			protected override void load_objects() {
				var attrs = new GP11.Attributes();
				attrs.add_boolean(P11.CKA_TOKEN, true);
				attrs.add_ulong(P11.CKA_CLASS, P11.CKO_CERTIFICATE);
				_session.find_objects_async(attrs, cancellable, on_find_objects);
			}
			
			private void on_find_objects(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Session session = (GP11.Session)obj;
				GLib.List<GP11.Object> objects;
				
				/* Get the results */
				try {
					objects = session.find_objects_finish(result);
				} catch (GLib.Error err) {
					complete(err);
					return;
				}

				/* Remove all objects that were found, from the check table */
				foreach (var object in objects)
					_checks.remove(object.handle);
				
				/* Remove everything not found from the context */
				_checks.for_each((k, v) => { Context.for_app().remove_object((Seahorse.Object)v); });

				loaded_objects(objects);
			}
		}

		/* ------------------------------------------------------------------------------- 
		 * LOAD OPREATION
		 */

		private class Loader : Updater {
			
			/* The handle to load */
			private GP11.Attributes _unique_attrs;
			public GP11.Attributes unique_attrs {
				get { return _unique_attrs; }
				construct { _unique_attrs = value; }
			}
			
			public Loader(Source source, GP11.Attributes unique_attrs) {
				this.source = source;
				this.unique_attrs = unique_attrs;
			}
			
			/* Search for the object to load */
			protected override void load_objects() {
				_session.find_objects_async(_unique_attrs, cancellable, on_find_objects);
			}
			
			private void on_find_objects(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Session session = (GP11.Session)obj;
				/* Get the results */
				try {
					var objects = session.find_objects_finish(result);
					loaded_objects(objects);
				} catch (GLib.Error err) {
					complete(err);
					return;
				}
			}
		}

		/* ------------------------------------------------------------------------------- 
		 * IMPORT OPREATION
		 */

		private class Importer : Updater {
			
			private GP11.Attributes _import_data;
			public GP11.Attributes import_data {
				get { return _import_data; }
			}
			
			public Importer(Source source, GP11.Attributes import) {
				this.source = source;
			}

			/* Search for the object to load */
			protected override void load_objects() {
				_session.create_object_async(_import_data, cancellable, on_create_object);
			}
			
			private void on_create_object(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Session session = (GP11.Session)obj;
				GP11.Object import;
				
				/* Get the results */
				try {
					import = session.create_object_finish(result);
				} catch (GLib.Error err) {
					complete(err);
					return;
				}
				
				/* Get the list of objects imported */
				import.get_attribute_async(P11.CKA_GNOME_IMPORT_OBJECTS, cancellable, on_get_attribute);
			}
			
			private void on_get_attribute(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Object import = (GP11.Object)obj;
				GP11.Attribute imported_handles;
				
				/* Get the results */
				try {
					imported_handles = import.get_attribute_finish(result);
				} catch (GLib.Error err) {
					complete(err);
					return;
				}
				
				loaded_objects(GP11.Object.from_handle_array(_session, imported_handles));
			}
		}
		
		/* ------------------------------------------------------------------------------- 
		 * REMOVE OPREATION
		 */

		private class Remover : Operation {

			/* For cancelling the GP11 operations */
			private GLib.Cancellable _cancellable;
			public GLib.Cancellable cancellable { 
				get { return _cancellable; }
			}

			/* The source we're removing from */
			private Source _source;
			public Source source {
				get { return _source; }
				construct { _source = value; }
			}
			
			private Certificate _certificate;
			public Certificate certificate {
				get { return _certificate; }
				construct { _certificate = value; }
			}

			public Remover(Certificate certificate) {
				this.certificate = certificate;
			}
			
			construct {
				_cancellable = new Cancellable();
				_certificate.pkcs11_object.destroy_async(_cancellable, on_destroy_object);
				mark_start ();
			}
			
			private void on_destroy_object(GLib.Object obj, GLib.AsyncResult result) {
				GP11.Object object = (GP11.Object)obj;
				
				/* Check for errors */
				try {
					object.destroy_finish(result);
				} catch (GLib.Error err) {
					if (err.code == P11.CKR_FUNCTION_CANCELED)
						mark_done(true, null);
					else
						mark_done(false, err);
					return;
				}
				
				Context.for_app().remove_object(certificate);
				mark_done(false, null);
			}
			
			/* Cancel the operation */
			public override void cancel() {
				_cancellable.cancel();
			}
		}
		
		/* ---------------------------------------------------------------------------------
		 * PUBLIC STUFF
		 */
		
		public Source(GP11.Slot slot) {
			this.slot = slot;
		}

		/* ---------------------------------------------------------------------------------
		 * VIRTUAL METHODS
		 */
		
		public override Operation load(GLib.Quark id) {
			
			/* Load all objects */
			if (id == 0) 
				return new Refresher(this);

			/* Load only objects described by the id */
			GP11.Attributes attrs = new GP11.Attributes();
			if (!Pkcs11.id_to_attributes (id, attrs))
				return new Operation.complete(new GLib.Error(ERROR_DOMAIN, 0, "%s", _("Invalid or unrecognized object.")));
			else 
				return new Loader(this, attrs);
		}
		
		public virtual Operation import (GLib.InputStream input) {
			uchar[] data = Util.read_to_memory(input);
			
			var attrs = new GP11.Attributes();
			attrs.add_boolean(P11.CKA_GNOME_IMPORT_TOKEN, true);
			attrs.add_data(P11.CKA_VALUE, data);
			
			return new Importer(this, attrs);
		}
		
		public virtual Operation export (GLib.List<weak Object> objects, 
		                                 GLib.OutputStream output) {
			return new Operation.complete(new GLib.Error(ERROR_DOMAIN, 0, "%s", _("Exporting is not yet supported.")));
		}
		
		public virtual Operation remove (Object object) {
			return new Remover((Pkcs11.Certificate)object);
		}
		
		/* --------------------------------------------------------------------------------
		 * HELPER METHODS
		 */
		
		void receive_object(GP11.Object object, GP11.Attributes attrs) {
			
			/* Build up an identifier for this object */
			GLib.Quark id = Pkcs11.id_from_attributes(attrs);
			return_if_fail(id != 0);

			bool created = false;
			Pkcs11.Certificate cert;
			
			/* Look for an already present object */
			var prev = Context.for_app().get_object(this, id);
			if (prev != null && prev.get_type() == typeof(Pkcs11.Certificate)) {
				cert = (Pkcs11.Certificate)prev;
				cert.pkcs11_object = object;
				cert.pkcs11_attributes = attrs;
				return;
			}
			
			/* Create a new object */
			cert = new Pkcs11.Certificate(object, attrs);
			cert.source = this;
			Context.for_app().add_object(cert);
		}
	}
}
