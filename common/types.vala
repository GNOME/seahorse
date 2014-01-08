/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

namespace Seahorse {

/* Again, never change these values */
public enum Usage {
	NONE = 0,
	SYMMETRIC_KEY = 1,
	PUBLIC_KEY = 2,
	PRIVATE_KEY = 3,
	CREDENTIALS = 4,
	IDENTITY = 5,
	OTHER = 10
}

[Flags]
public enum Flags {
	[CCode (cname = "SEAHORSE_FLAG_NONE")]
	NONE = 0,

	[CCode (cname = "SEAHORSE_FLAG_IS_VALID")]
	IS_VALID =    0x00000001,

	[CCode (cname = "SEAHORSE_FLAG_CAN_ENCRYPT")]
	CAN_ENCRYPT = 0x00000002,

	[CCode (cname = "SEAHORSE_FLAG_CAN_SIGN")]
	CAN_SIGN =    0x00000004,

	[CCode (cname = "SEAHORSE_FLAG_EXPIRED")]
	EXPIRED =     0x00000100,

	[CCode (cname = "SEAHORSE_FLAG_REVOKED")]
	REVOKED =     0x00000200,

	[CCode (cname = "SEAHORSE_FLAG_DISABLED")]
	DISABLED =    0x00000400,

	[CCode (cname = "SEAHORSE_FLAG_TRUSTED")]
	TRUSTED =     0x00001000,

	[CCode (cname = "SEAHORSE_FLAG_PERSONAL")]
	PERSONAL =    0x00002000,

	[CCode (cname = "SEAHORSE_FLAG_EXPORTABLE")]
	EXPORTABLE =  0x00100000,

	[CCode (cname = "SEAHORSE_FLAG_DELETABLE")]
	DELETABLE =   0x10000000
}

}
