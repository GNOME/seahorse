#!/usr/bin/python

import os
import base64

os.chdir("/tmp")

def clean(path):
	if os.path.exists(path):
		os.unlink(path)

for i in range (512, 768, 8):
	fname = "test-key-%d" % i
	clean(fname)
	clean("%s.pub" % fname)

	os.system("ssh-keygen -b %d -t rsa -f %s -N '' -C blah > /dev/null" % (i, fname))
	data = open("%s.pub" % fname).read()

	parts = data.split(" ")
	raw = base64.b64decode(parts[1])

	l = len(raw)
	r = (l - 21)
	guess = (r * 8)

	if guess == i:
		print "%d ok" % i
	else :
		print "%d != %d" % (i, guess)
	
