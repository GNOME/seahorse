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

proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/keys/openpgp')
keyset = dbus.Interface(proxy_obj, "org.gnome.seahorse.Keys")

keys = keyset.DiscoverKeys(['E67A96AAEA0398B6', '786AF9FD33B46A1A'], 0)
print keys
