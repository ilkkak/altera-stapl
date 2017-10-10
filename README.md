altera-stapl
============

Alteras Jam STAPL Bytecode Player with 64-bit support. This is a user-space
port of the altera-stapl driver from the linux kernel. The source released
by Altera wasn't 64-bit compatible. Additionally, the linux version is much
cleaner and old cruft was removed.

Compared to the original Altera sources this has the following new
features:
  * much cleaner source code
  * GPLv2 license

Modified from Michael Walle's GPIO version to use FTDI FT232H in MPSSE mode
by Ilkka Kalliomäki. FTDI initialization and buffering routines from xc3sprog.

Authors
-------
  * Michael Walle <michael.walle@kontron.com>
  * Uwe Bonnes <bon@elektron.ikp.physik.tu-darmstadt.de>
  * Dmitry Teytelman
  * Ilkka Kalliomäki <ilkka.kalliomaki@multitaction.com>
