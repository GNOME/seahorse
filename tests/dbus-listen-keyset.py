#!/usr/bin/python

import gobject 
import dbus
if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
    import dbus.glib

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/keys/openpgp')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.Keys')


def signal_callback(interface, signal_name, service, path, message):
    print "Received signal %s from %s" % (signal_name, interface)

def key_changed(id):
    print "Changed: ", id

def key_added(id):
    print "Added: ", id

def key_removed(id):
    print "Removed: ", id

service.connect_to_signal ('KeyChanged', key_changed)
service.connect_to_signal ('KeyAdded', key_added)
service.connect_to_signal ('KeyRemoved', key_removed)

#bus.add_signal_receiver (signal_callback, 'KeyChanged', 'org.gnome.seahorse.Keys', 
#                         None, '/')
#bus.add_signal_receiver (signal_callback, 'KeyAdded', 'org.gnome.seahorse.Keys', 
#                         None, '/')
#bus.add_signal_receiver (signal_callback, 'KeyRemoved', 'org.gnome.seahorse.Keys', 
#                         None, '/')
#bus.add_signal_receiver (signal_callback, 'KeyChanged', 'org.gnome.seahorse.Keys', 
#                         'org.gnome.seahorse', '/org/gnome/seahorse/keys/openpgp')
#bus.add_signal_receiver (signal_callback, 'KeyAdded', 'org.gnome.seahorse.Keys', 
#                         'org.gnome.seahorse', '/org/gnome/seahorse/keys/openpgp')
#bus.add_signal_receiver (signal_callback, 'KeyRemoved', 'org.gnome.seahorse.Keys', 
#                         'org.gnome.seahorse', '/org/gnome/seahorse/keys/openpgp')


mainloop = gobject.MainLoop()
mainloop.run()
