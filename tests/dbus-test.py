#!/usr/bin/python

import dbus

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nALL DBUS SERVICES -------------------------------------"
print dbus_iface.ListNames()


print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse.CryptoService', '/org/gnome/seahorse/Crypto/Service')
iface = dbus.Interface(proxy_obj, 'org.gnome.seahorse.CryptoService')

print iface.GetKeyTypes()
