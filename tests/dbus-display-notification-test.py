#!/usr/bin/python

import dbus

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/keys')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.KeyService')

service.DisplayNotification("The heading", "1 The text <key id='openpgp:A50CFA6E5DBAA916'/>", "", True)
service.DisplayNotification("The heading", "2 The text <key id='openpgp:A50CFA6E5DBAA916' field='simple-name'/>", "", True)
service.DisplayNotification("The heading", "3 The text <key id='openpgp:A50CFA6E5DBAA916' field='key-id'/>", "", True)
service.DisplayNotification("The heading", "4 The text <key id='openpgp:A50CFA6E5DBAA916' field='id'/>", "", True)

