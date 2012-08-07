---
title: scmpc - An Audioscrobbler client for MPD
layout: default
---

About
-----

**scmpc** is a client for [MPD](http://musicpd.org) which submits your tracks to [Last.fm](http://last.fm).

"*Another one?*", I hear you cry? Yes, I know about [mpdscribble](http://mpd.wikia.com/wiki/Client:Mpdscribble), but when I started this project it didn't do entirely what I wanted it to. It didn't run as a daemon, for example, and it didn't work if you enable crossfading in MPD. I didn't add the support for these in mpdscribble, partly because GNU coding style scares me, and I got slightly lost in the source code, but also because I created this as a way of teaching myself C programming.

Features
--------

* Can be run as a daemon, either as a user or as root.
* Keeps unsubmitted songs in a queue which is saved to a file at a configurable interval.
* Will try to reconnect to MPD and Audioscrobbler if the network connection fails.
* Works with a password-protected MPD server.
* Works when crossfading is enabled.

News
----

{% for post in site.posts %}
**{{ post.title }}**

{{ post.content }}
{% endfor %}

Downloads
---------

[Downloads are available from our github project page.](https://github.com/cmende/scmpc/downloads)

Requirements
------------

scmpc requires the following to be installed:

* [confuse](http://www.nongnu.org/confuse/)
* [glib](http://www.gtk.org) (>= 2.16)
* [libcurl](http://curl.haxx.se/libcurl/) (>= 7.15.4)
* [libmpdclient](http://www.musicpd.org)

Installation
------------

Download the tarball, and then you just need to run `./configure; make; make install` as usual.

The following distributions already ship scmpc packages:

* [Gentoo Linux](http://packages.gentoo.org/package/media-sound/scmpc)
* [Arch Linux](http://aur.archlinux.org/packages.php?ID=10220) (thanks to Mark Taylor)
* [FreeBSD](http://freshports.org/audio/scmpc/) (thanks to Павел Мотырев)
* [OpenBSD](http://openports.se/audio/scmpc) (thanks to Nicholas Marriott and Jasper Lievisse Adriaanse)

Your distribution ships it too? [Tell me](mailto:mende.christoph@gmail.com)!

Development
-----------

If you want to get involved, have any feature requests or find any bugs, please write to the [mailing list](https://groups.google.com/group/scmpc-devel) or open a [new issue](https://github.com/cmende/scmpc/issues). You can also contact us via IRC on irc.freenode.net, #scmpc.
