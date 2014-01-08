/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/**
 * For parsing and noting key validity, trust, etc...
 */
 
#ifndef __SEAHORSE_VALIDITY_H__
#define __SEAHORSE_VALIDITY_H__

#include <gtk/gtk.h>

typedef enum {
    SEAHORSE_VALIDITY_REVOKED =     -3,
    SEAHORSE_VALIDITY_DISABLED =    -2,
    SEAHORSE_VALIDITY_NEVER =       -1,
    SEAHORSE_VALIDITY_UNKNOWN =      0,
    SEAHORSE_VALIDITY_MARGINAL =     1,
    SEAHORSE_VALIDITY_FULL =         5,
    SEAHORSE_VALIDITY_ULTIMATE =    10
} SeahorseValidity;

const gchar*        seahorse_validity_get_string    (SeahorseValidity validity);

#endif /* __SEAHORSE_VALIDITY_H__ */
