#!/usr/bin/python

import dbus

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/crypto')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.CryptoService')

encrypted = service.EncryptText(["openpgp:XXXXXXXXXXX", "openpgp:XXXXXXX"], "", 0, "cleartext")
print "Encrypted: ", encrypted

(decrypted, signer) = service.DecryptText("openpgp", 0, encrypted)
print "Decrypted: ", decrypted
print "Signer: ", signer


signed = service.SignText("openpgp:XXXXXXXXXXXXXXXXXXXXX", 0, "cleartext")
print "Signed: ", signed

(verified, signer) = service.VerifyText("openpgp", 0, signed)
print "Verified: ", verified
print "Signer: ", signer


