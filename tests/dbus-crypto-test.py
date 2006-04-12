#!/usr/bin/python

import dbus

bus = dbus.SessionBus()
proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

print "\n\nSEAHORSE DBUS CALL ------------------------------------"
proxy_obj = bus.get_object('org.gnome.seahorse', '/org/gnome/seahorse/crypto')
service = dbus.Interface(proxy_obj, 'org.gnome.seahorse.CryptoService')

#encrypted = service.EncryptText(["openpgp:78C25AFC0BEE1BAD", "openpgp:FAD3A86D2505A4D5"], "openpgp:78C25AFC0BEE1BAD", 123, "cleartext")
#print "Encrypted: ", encrypted

#(decrypted, signer) = service.DecryptText(123, encrypted)
#print "Decrypted: ", decrypted
#print "Signer: ", signer


signed = service.SignText("openpgp:78C25AFC0BEE1BAD", 123, "cleartext")
print "Signed: ", signed

(verified, signer) = service.VerifyText(123, signed)
print "Verified: ", verified
print "Signer: ", signer


