#!/usr/bin/python

import dbus

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nALL DBUS SERVICES -------------------------------------"
print dbus_iface.ListNames()


print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/keys')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.KeyService')

types = service.GetKeyTypes()
print "GetKeyTypes(): ", types

if not len(types):
    print "No key types found"
    sys.exit(0)

path = service.GetKeyset(types[0])
print "GetKeySet(): ", path

proxy_obj = bus.get_object('org.gnome.seahorse', path)
keyset = dbus.Interface(proxy_obj, "org.gnome.seahorse.Keys")

keys = keyset.ListKeys()

if not len(keys):
    print "No keys found"
    sys.exit(0)
    
print keyset.GetKeyField(keys[0], "display-name")
print keyset.GetKeyFields(keys[0], [ "display-name", "simple-name", "fingerprint" ])
print service.ExportKeys("openpgp", [ keys[0] ])
