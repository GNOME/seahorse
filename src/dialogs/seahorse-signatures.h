/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_SIGNATURES_H__
#define __SEAHORSE_SIGNATURES_H__

/* SeahorseSignatures is a SeahorseKeyWidget that contains a signature status.
 * It will show the signature's status and the signing key if possible.
 * A SeahorseSignature is only valid for a single signature with one key. */

#include <gpgme.h>

#include "seahorse-context.h"

/* Load a new signature dialog */
void	seahorse_signatures_new			(SeahorseContext	*sctx,
						 GpgmeSigStat		status);

#endif /* __SEAHORSE_SIGNATURES_H__ */
