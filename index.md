---
title: scmpc - An Audioscrobbler client for MPD
layout: default
description: scmpc is a small daemon that submits the tracks you play in MPD to Last.fm.
latest: 0.5.1
---

About
-----

**scmpc** is a client for [MPD](https://www.musicpd.org/) which submits your tracks to [Last.fm](https://www.last.fm/).

"*Another one?*", I hear you cry? Yes, I know about [mpdscribble](https://www.musicpd.org/clients/mpdscribble/), but when I started this project it didn't do entirely what I wanted it to. It didn't run as a daemon, for example, and it didn't work if you enable crossfading in MPD. I didn't add the support for these in mpdscribble, partly because GNU coding style scares me, and I got slightly lost in the source code, but also because I created this as a way of teaching myself C programming.

Features
--------

* Can be run as a daemon, either as a user or as root.
* Sends Now Playing notifications to Last.fm.
* Keeps unsubmitted songs in a queue which is saved to a file at a configurable interval.
* Will try to reconnect to MPD and Audioscrobbler if the network connection fails.
* Works with a password-protected MPD server.
* Works when crossfading is enabled.

News
----

{% for post in site.posts limit:3 %}
<article class="post" markdown="1">
<time datetime="{{ post.date | date: "%Y-%m-%d" }}">{{ post.date | date: "%-d %B %Y" }}</time>

### {{ post.title }}

{{ post.content }}
</article>
{% endfor %}

<details class="older" markdown="1">
<summary>Older news</summary>
{% for post in site.posts offset:3 %}
<article class="post" markdown="1">
<time datetime="{{ post.date | date: "%Y-%m-%d" }}">{{ post.date | date: "%-d %B %Y" }}</time>

### {{ post.title }}

{{ post.content }}
</article>
{% endfor %}
</details>

Downloads
---------

[Downloads are available from the GitHub releases page.](https://github.com/nyxdis/scmpc/releases)

Requirements
------------

scmpc requires MPD 0.14 or later and the following to be installed:

* [confuse](https://github.com/libconfuse/libconfuse)
* [glib](https://www.gtk.org) (>= 2.28)
* [libcurl](https://curl.se/libcurl/) (>= 7.15.4)
* [libmpdclient](https://www.musicpd.org/libs/libmpdclient/) (>= 2.3)

Installation
------------

Download the tarball, and then you just need to run `./configure; make; make install` as usual.

To build from a git checkout, run `./autogen.sh` first, which needs autoconf and automake. `make check` runs the tests.

The following distributions already ship scmpc packages:

* [Gentoo Linux](https://packages.gentoo.org/packages/media-sound/scmpc)
* [OpenBSD](https://openports.pl/path/audio/scmpc) (thanks to Nicholas Marriott and Jasper Lievisse Adriaanse)

Your distribution ships it too? [Tell me](mailto:freydis.mende@proton.me)!

Development
-----------

If you want to get involved, have any feature requests or find any bugs, please write to the [mailing list](https://groups.google.com/g/scmpc-devel) or open a [new issue](https://github.com/nyxdis/scmpc/issues).
