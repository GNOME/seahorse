Seahorse
========
Seahorse is a graphical interface for managing and using encryption keys.
Currently it supports PGP keys (using GPG/GPGME) and SSH keys. Its goal is to
provide an easy to use key management tool, along with an easy to use interface
for encryption operations.

Building
--------
You can build and install Seahorse using [Meson]:

```sh
meson setup _build
meson compile -C _build
meson install -C _build
```

Contributing
------------
You can browse the code, issues and more at Seahorse's [GitLab repository].

If you find a bug in Seahorse, please file an issue on the [issue tracker].
Please try to add reproducible steps and the relevant version of Seahorse.

If you want to contribute functionality or bug fixes, please open a Merge
Request (MR). For more info on how to do this, see GitLab's [help pages on
MR's].

If Seahorse is not translated in your language or you believe that the
current translation has errors, you can join one of the various translation
teams in GNOME. Translators do not commit directly to Git, but are advised to
use our separate translation infrastructure instead. More info can be found at
the [translation project wiki page].

More information
----------------
Seahorse has its own web page on https://wiki.gnome.org/Apps/Seahorse.

To discuss issues with developers and other users, you can go to [GNOME's
Discourse instance] and use the "seahorse" tag, or join [#gnome-seahorse] on
irc.libera.chat.

License
-------
Seahorse is released under the GPL. See [COPYING] for more info.


[GNOME]: https://www.gnome.org
[Meson]: http://mesonbuild.com
[GitLab repository]: https://gitlab.gnome.org/GNOME/seahorse
[help pages on MR's]: https://docs.gitlab.com/ee/gitlab-basics/add-merge-request.html
[issue tracker]: https://gitlab.gnome.org/GNOME/seahorse/issues
[translation project wiki page]: https://wiki.gnome.org/TranslationProject/
[GNOME's Discourse instance]: https://discourse.gnome.org
[#seahorse]: irc://irc.libera.chat/gnome-seahorse
[COPYING]: https://gitlab.gnome.org/GNOME/seahorse/COPYING
