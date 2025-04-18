/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __SEEPROM_H__
#define __SEEPROM_H__

int seeprom_read(void *dst, int offset, int size);
int seeprom_write(void *src, int offset, int size);
void seeprom_erase(void);

#endif
