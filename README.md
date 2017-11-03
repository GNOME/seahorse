# Seahorse
Seahorse is a graphical interface for managing and using encryption keys.
Currently it supports PGP keys (using GPG/GPGME) and SSH keys. Its goal is to
provide an easy to use Key Management Tool, along with an easy to use interface
for encryption operations.

Seahorse is integrated into the [GNOME Desktop Environment](https://www.gnome.org)
and allows users to perform operations from their regular applications, like
nautilus or gedit.

## Building
You can build and install Seahorse by issuing the following commands:
```sh
./autogen.sh
make
make install
```

## Issue tracker
Seahorse uses the GNOME Bugzilla, where you can check the
[list of open bugs](https://bugzilla.gnome.org/browse.cgi?product=seahorse).

If you'd like to report a bug in Seahorse or request an enhancement, please file
an issue using the
[appropriate form](https://bugzilla.gnome.org/enter_bug.cgi?product=seahorse).

In case of a bug, please also add reproducible steps and the version of Seahorse.

## Contributing
If you would like to contribute a patch, you should send it in to the GNOME
Bugzilla as well. If the patch fixes an existing bug, add the patch as an
attachment to that bug report; otherwise, enter a new bug report that describes
the patch, and attach the patch to that bug report.

For more information on the recommended workflow, please read
[this wiki page](https://wiki.gnome.org/Git/WorkingWithPatches).

## More information
Seahorse has its own web page on https://wiki.gnome.org/Apps/Seahorse.

To discuss issues with developers and other users, you can subscribe to the
[mailing list](https://mail.gnome.org/mailman/listinfo/seahorse-list)
or join [#seahorse](irc://irc.gnome.org/seahorse) on irc.gnome.org.

## License
Seahorse is released under the GPL. See COPYING for more info.
