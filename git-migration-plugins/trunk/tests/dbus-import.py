#!/usr/bin/python

import dbus
import sys

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nALL DBUS SERVICES -------------------------------------"
print dbus_iface.ListNames()

if len(sys.argv) < 2:
    print "Specify data to import on command line"
    sys.exit(2)
    
data = open(sys.argv[1]).read()

print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/keys')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.KeyService')

keys = service.ImportKeys("openpgp", data)
print keys
