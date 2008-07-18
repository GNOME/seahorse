
namespace Seahorse {
	/* 
	 * These types should never change. These values are exported via DBUS. In the 
	 * case of a key being in multiple locations, the highest location always 'wins'.
	 */
	public enum Location {
		INVALID = 0,
		MISSING = 10,
		SEARCHING = 20,
		REMOTE = 50,
		LOCAL = 100
	}
	
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
}
