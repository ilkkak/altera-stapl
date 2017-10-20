/* JTAG GNU/Linux FTDI FT2232 low-level I/O

Copyright (C) 2005-2013 Uwe Bonnes bon@elektron.ikp.physik.tu-darmstadt.de
Copyright (C) 2006 Dmitry Teytelman

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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ftdi.h>

#include "ioftdi.h"

int IOFtdi_Init(unsigned int freq) {
  bptr = 0;
  calls_rd = 0;
  calls_wr = 0;
  retries = 0;

  char *fname = getenv("FTDI_DEBUG");
  if (fname)
    fp_dbg = fopen(fname,"wb");
  else
    fp_dbg = NULL;

  ftdi_handle = 0;
  unsigned char   buf[9] = { SET_BITS_LOW, 0x00, 0x0b,
                             TCK_DIVISOR,  0x03, 0x00 ,
                             SET_BITS_HIGH,0x00, 0x00};
  unsigned int vendor = VENDOR_FTDI, product = DEVICE_DEF;
  // set GPIO pin to enable programming from Tegra
  unsigned int dbus_data = 0x10, dbus_en = 0x10, cbus_data= 0, cbus_en = 0;
  unsigned int divisor;
  int res;

  /* set for now. If we have a fast device, correct later */
  if ((freq == 0 )|| (freq >= 6000000)) /* freq = 0 means max rate, 3 MHz for now*/
    divisor = 0;
  else
    divisor = 6000000/freq - ((6000000&freq)?0:1);
  if (divisor > 0xffff)
    divisor = 0xffff;

  buf[4] = divisor & 0xff;
  buf[5] = (divisor >> 8) & 0xff;

  // allocate and initialize FTDI structure
  ftdi_handle = ftdi_new();

  // Set interface, FT232H support only
  res = ftdi_set_interface(ftdi_handle, 0);
  if(res < 0)
  {
    fprintf(stderr, "ftdi_set_interface: %s\n",
            ftdi_get_error_string(ftdi_handle));
    return res;
  }

  // Open device
  res = ftdi_usb_open_desc(ftdi_handle, vendor, product,
                           NULL, NULL);
  if(res == 0)
  {
    res = ftdi_set_bitmode(ftdi_handle, 0x00, BITMODE_RESET);
    if(res < 0)
    {
      fprintf(stderr, "ftdi_set_bitmode: %s",
              ftdi_get_error_string(ftdi_handle));
      return res;
    }
    res = ftdi_usb_purge_buffers(ftdi_handle);
    if(res < 0)
    {
      fprintf(stderr, "ftdi_usb_purge_buffers: %s",
              ftdi_get_error_string(ftdi_handle));
      return res;
    }
    //Set the latency time to a low value
    res = ftdi_set_latency_timer(ftdi_handle, 1);
    if( res <0)
    {
      fprintf(stderr, "ftdi_set_latency_timer: %s",
              ftdi_get_error_string(ftdi_handle));
      return res;
    }

    // Set mode to MPSSE
    res = ftdi_set_bitmode(ftdi_handle, 0xfb, BITMODE_MPSSE);
    if(res< 0)
    {
      fprintf(stderr, "ftdi_set_bitmode: %s",
              ftdi_get_error_string(ftdi_handle));
      return res;
    }

    device_has_fast_clock = true;
  }
  else {
    fprintf(stderr, "could not open usb device\n");
    return res;
  }

  if (!ftdi_handle)
  {
    fprintf(stderr, "Unable to access FTDI device with libftdi\n");
    res = 1;
    return res;
  }

  // Prepare for JTAG operation
  buf[1] |= dbus_data;
  buf[2] |= dbus_en;
  buf[7] = cbus_data;
  buf[8] = cbus_en;
  
  IOFtdi_mpsse_add_cmd(buf, 9);
  IOFtdi_mpsse_send();
  /* On H devices, use the non-divided clock*/
  if (device_has_fast_clock && ((freq == 0) ||(freq > 458)))
  {
    if ((freq == 0) ||(freq >= 30000000)) /* freq = 0 means max rate, 30 MHz for now*/
      divisor = 0;
    else
      divisor = 30000000/freq -((30000000%freq)?0:1);
    if (divisor > 0xffff)
      divisor = 0xffff;
#ifndef DIS_DIV_5
#define DIS_DIV_5 0x8a
#endif
    buf[0] = DIS_DIV_5;
    buf[1] = TCK_DIVISOR;
    buf[2] =  divisor & 0xff;
    buf[3] = (divisor >> 8) & 0xff;
    IOFtdi_mpsse_add_cmd(buf, 4);
    IOFtdi_mpsse_send();
    tck_freq = 30000000/(1+divisor);
  }
  else {
    tck_freq = 6000000/(1+divisor);
  }

  if (tck_freq > 1000000)
    fprintf(stderr,"Using JTAG frequency %3d.%03d MHz from undivided clock\n",
            tck_freq/1000000,(tck_freq%1000000)/1000);
  else
    fprintf(stderr,"Using JTAG frequency %3d.%03d kHz from undivided clock\n",
            tck_freq/1000, tck_freq%1000);

  return 0;
}

void IOFtdi_settype(int sub_type)
{
  subtype = sub_type;
}

static int nBits = 0;
static unsigned char tdi_send = 0;
static int nTDIBits = 0;

void buffer_flush() {
  unsigned char buf[3];

  if(nTDIBits>0) {
    buf[0] = MPSSE_DO_WRITE|MPSSE_LSB|MPSSE_BITMODE|MPSSE_WRITE_NEG;
    buf[1] = nTDIBits-1;
    buf[2] = tdi_send;
    IOFtdi_mpsse_add_cmd (buf, 3);
    nTDIBits = 0;
  }

  tdi_send = 0;
}

// buffer single bits into byte
void buffer_tdi(unsigned char tdi_bit) {
  if(nTDIBits > 7) {
    buffer_flush();
  }

  tdi_send = tdi_send | ((tdi_bit ? 1:0) << nTDIBits);
  nTDIBits++;
}

int IOFtdi_txrx_bits(int tms, int tdi, int read_tdo) {
  int tdo = 0;
  unsigned char tdo_bit;
  unsigned char tdi_bit = ((unsigned char) (tdi ? 1:0));
  unsigned char tms_bit = ((unsigned char) (tms ? 1:0));

  unsigned char buf[3];

  if(read_tdo) {
    buffer_flush();
    buf[0] = MPSSE_WRITE_TMS|MPSSE_DO_READ|
        MPSSE_DO_WRITE|MPSSE_LSB|MPSSE_BITMODE|MPSSE_WRITE_NEG;
    buf[1] = 0;
    buf[2] = (tdi_bit<<7)|tms_bit;
    IOFtdi_mpsse_add_cmd (buf, 3);

    IOFtdi_readusb(&tdo_bit, 1);
    tdo = tdo_bit>>7;
  }
  else {
    if(tms_bit) {
      buffer_flush();
      buf[0] = MPSSE_WRITE_TMS|MPSSE_DO_WRITE|MPSSE_LSB|MPSSE_BITMODE|MPSSE_WRITE_NEG;
      buf[1] = 0;
      buf[2] = (tdi_bit<<7)|1;
      IOFtdi_mpsse_add_cmd (buf, 3);
    }
    else {
      buffer_tdi(tdi_bit);
    }
  }
  nBits++;
  const int printDivisor = 800000;
  if(nBits % printDivisor == 0){
    fprintf(stderr, ".");
  }

  return (tdo);
}

void IOFtdi_deinit(void)
{
  int read;
  /* Before shutdown, we must wait until everything is shifted out
     Do this by temporary enabling loopback mode, write something
     and wait until we can read it back */
  static unsigned char   tbuf[16] = { SET_BITS_LOW, 0xff, 0x00,
                                      SET_BITS_HIGH, 0xff, 0x00,
                                      LOOPBACK_START,
                                      MPSSE_DO_READ|MPSSE_READ_NEG|
                                      MPSSE_DO_WRITE|MPSSE_WRITE_NEG|MPSSE_LSB,
                                      0x04, 0x00,
                                      0xaa, 0x55, 0x00, 0xff, 0xaa,
                                      LOOPBACK_END};
  IOFtdi_mpsse_add_cmd(tbuf, 16);
  read = IOFtdi_readusb( tbuf,5);
  if  (read != 5)
    fprintf(stderr,"Loopback failed, expect problems on later runs\n");
  
  ftdi_set_bitmode(ftdi_handle, 0, BITMODE_RESET);
  ftdi_usb_reset(ftdi_handle);
  ftdi_usb_close(ftdi_handle);
  ftdi_deinit(ftdi_handle);

  fprintf(stderr, "USB transactions: Write %d read %d retries %d\n",
          calls_wr, calls_rd, retries);
}

unsigned int IOFtdi_readusb(unsigned char * rbuf, unsigned long len)
{
  unsigned char buf[1] = { SEND_IMMEDIATE};
  IOFtdi_mpsse_add_cmd(buf,1);
  IOFtdi_mpsse_send();
#ifdef USE_FTD2XX
  DWORD read = 0;
#else
  unsigned int read = 0;
#endif
  {
    int length = (int) len;
    int timeout=0, last_errno, last_read;
    calls_rd++;
    last_read = ftdi_read_data(ftdi_handle, rbuf, length );
    if (last_read > 0)
      read += last_read;
    while (((int)read <length) && ( timeout <1000))
    {
      last_errno = 0;
      retries++;
      last_read = ftdi_read_data(ftdi_handle, rbuf+read, length -read);
      if (last_read > 0)
        read += last_read;
      else
        last_errno = errno;
      timeout++;
    }
    if (timeout >= 1000)
    {
      fprintf(stderr,"readusb waiting too long for %ld bytes, only %d read\n",
              len, last_read);
      if (last_errno)
      {
        fprintf(stderr,"error %s\n", strerror(last_errno));
        IOFtdi_deinit();
        //                throw  io_exception();
      }
    }
    if (last_read <0)
    {
      fprintf(stderr,"Error %d str: %s\n", -last_read, strerror(-last_read));
      IOFtdi_deinit();
      //            throw  io_exception();
    }
  }
  if(fp_dbg)
  {
    unsigned int i;
    fprintf(fp_dbg,"readusb len %ld:", len);
    for(i=0; i<len; i++)
      fprintf(fp_dbg," %02x",rbuf[i]);
    fprintf(fp_dbg,"\n");
  }

  return read;
}

void IOFtdi_mpsse_add_cmd(unsigned char const *const buf, int const len) {
  /* The TX FIFO has 128 Byte. It can easily be overrun
    So send only chunks of the TX Buffersize and hope
    that the OS USB scheduler gives the MPSSE machine
    enough time empty the buffer
 */
  if(fp_dbg)
  {
    int i;
    fprintf(fp_dbg,"mpsse_add_cmd len %d:", len);
    for(i=0; i<len; i++)
      fprintf(fp_dbg," %02x",buf[i]);
    fprintf(fp_dbg,"\n");
  }
  if (bptr + len +1 >= TX_BUF)
    IOFtdi_mpsse_send();
  memcpy(usbuf + bptr, buf, len);
  bptr += len;
}

void IOFtdi_mpsse_send() {
  if(bptr == 0)  return;

  if(fp_dbg)
    fprintf(fp_dbg,"mpsse_send %d\n", bptr);

  {
    calls_wr++;
    int written = ftdi_write_data(ftdi_handle, usbuf, bptr);
    if(written != (int) bptr)
    {
      fprintf(stderr,"mpsse_send: Short write %d vs %d at run %d, Err: %s\n",
              written, bptr, calls_wr, ftdi_get_error_string(ftdi_handle));
      //          throw  io_exception();
    }
  }

  bptr = 0;
}


