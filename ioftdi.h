/* JTAG GNU/Linux FTDI FT2232 low-level I/O

Copyright (C) 2006 Dmitry Teytelman
Additions (C) 2005-2013  Uwe Bonnes
                         bon@elektron.ikp.physik.tu-darmstadt.de

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */



#ifndef IOFTDI_H
#define IOFTDI_H

#include <usb.h>
#include <ftdi.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

// FT232H
#define VENDOR_FTDI 0x0403
#define DEVICE_DEF  0x6014

#define TX_BUF (4096)

struct ftdi_context *ftdi_handle;
unsigned char usbuf[TX_BUF];
int buflen;
unsigned int bptr;
int calls_rd, calls_wr, subtype, retries;
FILE *fp_dbg;
bool device_has_fast_clock;
unsigned int tck_freq;

int IOFtdi_Init(unsigned int freq);
void IOFtdi_mpsse_add_cmd(unsigned char const *const buf, int const len);
void IOFtdi_mpsse_send();
unsigned int IOFtdi_readusb(unsigned char * rbuf, unsigned long len);
int IOFtdi_txrx_bits(int tms, int tdi, int read_tdo);

#endif // IOFTDI_H
