/**
 * @file gtkstock.h GTK+ Stock resources
 *
 * seahorse
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with the gaim
 * source distribution.
 * 
 * Also (c) 2005 Adam Schreiber <sadam@clemson.edu> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _SEAHORSE_GTKSTOCK_H_
#define _SEAHORSE_GTKSTOCK_H_

/* The default stock icons */
#define SEAHORSE_STOCK_KEY          "seahorse-key"
#define SEAHORSE_STOCK_PERSON       "seahorse-person"
#define SEAHORSE_STOCK_SIGN         "seahorse-sign"
#define SEAHORSE_STOCK_SEAHORSE     "seahorse"
#define SEAHORSE_STOCK_SECRET       "seahorse-secret"
#define SEAHORSE_STOCK_SHARE_KEYS   "seahorse-share-keys"
#define SEAHORSE_STOCK_KEY_SSH      "seahorse-key-ssh"
#define SEAHORSE_STOCK_KEY_SSH_LRG  "seahorse-key-ssh-large"

typedef struct _SeahorseStockIcon {
    const char *id;
    const char *dir;
    const char *filename;
} SeahorseStockIcon;

void    seahorse_gtkstock_init          (void);

void    seahorse_gtkstock_add_icons     (const SeahorseStockIcon *icons);

#endif /* _SEAHORSE_GTKSTOCK_H_ */
