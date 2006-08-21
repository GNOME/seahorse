#!/usr/bin/python

import os
import base64

os.chdir("/tmp")

def clean(path):
	if os.path.exists(path):
		os.unlink(path)

# MIN 512 MAX 32???

for i in range (512, 1025, 64):
	fname = "test-key-%d" % i

	clean(fname)
	clean("%s.pub" % fname)

	os.system("ssh-keygen -b %d -t dsa -f %s -N '' > /dev/null" % (i, fname))
	data = open("%s.pub" % fname).read()

	parts = data.split(" ")
	raw = base64.b64decode(parts[1])

	l = len(raw)
	n = ((l - 50) * 8) / 3
	guess = ((n / 64) + ((n % 64) > 32 ? 0 : 1)) * 64

	if guess == i:
		print "%d ok" % i
	else :
		print "%d != %d" % (i, guess)
