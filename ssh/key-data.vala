/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2016 Niels De Graef
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.Ssh.KeyData : GLib.Object {

    /* Used by callers */
    public string privfile { get; set; }        /* The secret key file */
    public string pubfile { get; set; }         /* The public key file */
    public bool partial { get; set; }       /* Only part of the public key file */
    public bool authorized { get; set; default = false; }    /* Is in authorized_keys */

    // These props are filled in by the parser
    public string rawdata { get; internal set; }         /* The raw data of the public key */
    public string? comment { get; internal set; }         /* The comment for the public key */
    public string? fingerprint { get; internal set; }     /* The full fingerprint hash */
    public uint length { get; internal set; }           /* Number of bits */
    public Algorithm algo { get; internal set; }             /* Key algorithm */

    public bool is_valid() {
        return this.fingerprint != null;
    }

    /**
     * Checks whether this key and another (in the form of a string) match.
     */
    public bool match(string line) {
        if (!is_valid())
            return false;

        try {
            KeyData other = parse_line(line);
            return (other.fingerprint != null) && (this.fingerprint == other.fingerprint);
        } catch (GLib.Error e) {
            warning(e.message);
        }

        return false;
    }

    public static KeyData parse_line(string? line) throws GLib.Error {
        if (line == null || line.strip() == "")
            throw new Error.GENERAL("Can't parse key from empty line.");

        string no_leading = line.chug();
        KeyData result = new KeyData();
        result.rawdata = no_leading;

        // Get the type
        string[] type_rest = no_leading.split_set(" \t", 2);
        if (type_rest.length != 2)
            throw new Error.GENERAL("Can't distinguish type from data (space missing).");

        string type = type_rest[0];
        if (type == "")
            throw new Error.GENERAL("Key doesn't have a type.");

        result.algo = Algorithm.guess_from_string(type);
        if (result.algo == Algorithm.UNKNOWN)
            throw new Error.GENERAL("Key doesn't have a valid type (%s).".printf(type));

        // Prepare for decoding
        string rest = type_rest[1];
        if (rest == "")
            throw new Error.GENERAL("Key doesn't have any data.");
        string[] data_comment = rest.split_set(" \t", 2);

        // Decode it, and parse binary stuff
        uchar[] bytes = Base64.decode(data_comment[0].strip());
        result.fingerprint = parse_key_blob(bytes);

        // The number of bits
        result.length = calc_bits(result.algo, bytes.length);

        // And the rest is the comment
        if (data_comment.length == 2) {
            string comment = data_comment[1];

            if (!comment.validate()) // If not utf8-valid, assume latin1
                result.comment = convert(comment, comment.length, "UTF-8", "ISO-8859-1");
            else
                result.comment = comment;
        }

        return result;
    }

    internal static string parse_key_blob(uchar[] bytes) throws GLib.Error {
        string digest = Checksum.compute_for_data(ChecksumType.MD5, bytes);
        if (digest == null)
            throw new Error.GENERAL("Can't calculate fingerprint from key.");

        var fingerprint = new StringBuilder.sized((digest.length * 3) / 2);
        for (size_t i = 0; i < digest.length; i += 2) {
            if (i > 0)
                fingerprint.append_c(':');
            fingerprint.append(digest.substring((long) i, 2));
        }

        return fingerprint.str;
    }

    internal static uint calc_bits (Algorithm algo, uint len) {
        // To keep us from having to parse a BIGNUM and link to openssl, these
        // are from the hip guesses at the bits of a key based on the size of
        // the binary blob in the public key.
        switch (algo) {
            case Algorithm.RSA:
                // Seems accurate to nearest 8 bits
                return ((len - 23) * 8);

            case Algorithm.DSA:
                // DSA keys seem to only work at 'bits % 64 == 0' boundaries
                uint n = ((len - 50) * 8) / 3;
                return ((n / 64) + (((n % 64) > 32) ? 1 : 0)) * 64; // round to 64

            case Algorithm.ED25519:
                return 256;

            default:
                return 0;
        }
    }

    // Adds and/or removes a keydata to a file (if added is already there, it is added at the back of the file).
    public static void filter_file(string filename, KeyData? add, KeyData? remove = null) throws GLib.Error {
        // By default filter out the one we're adding
        if (remove == null)
            remove = add;

        string contents;
        FileUtils.get_contents(filename, out contents);

        var results = new StringBuilder();

        // Load each line
        bool first = true;
        string[] lines = (contents ?? "").split("\n");
        foreach (string line in lines) {
            if (remove != null && remove.match(line))
                continue;

            if (!first)
                results.append_c('\n');
            first = false;
            results.append(line);
        }

        // Add any that need adding
        if (add != null) {
            if (!first)
                results.append_c('\n');
            results.append(add.rawdata);
        }

        FileUtils.set_contents(filename, results.str);
    }

    public unowned string? get_location() {
        return this.privfile != null ? this.privfile : this.pubfile;
    }
}

/**
 * Represents the data in a private key.
 */
public class Seahorse.Ssh.SecData : GLib.Object {
    public const string SSH_KEY_SECRET_SIG = "# SSH PRIVATE KEY: ";
    public const string SSH_PRIVATE_BEGIN = "-----BEGIN ";
    public const string SSH_PRIVATE_END = "-----END ";

    /**
     * Everything excluding the comment
     */
    public string rawdata { get; internal set; }

    public string? comment { get; internal set; }

    public Algorithm algo { get; internal set; }

    public static bool contains_private_key(string data) {
        return (SSH_KEY_SECRET_SIG in data) || (SSH_PRIVATE_BEGIN in data);
    }

    /**
     * Finds the first occurence of a private key in the given string and parses it if found.
     * NOTE: after parsing, it will *remove* the data with the private key from the string.
     *
     * @param data The data that contains a private key.
     */
    public static SecData parse_data(DataInputStream data, string initial_line) throws GLib.Error {
        var secdata = new SecData();

        // Get the comment
        if (initial_line.has_prefix(SSH_KEY_SECRET_SIG)) {
            secdata.comment = initial_line.substring(SSH_KEY_SECRET_SIG.length).strip();
        }

        // First get our raw data (if there is none, don't bother)
        string rawdata = parse_lines_block(data, initial_line, SSH_PRIVATE_BEGIN, SSH_PRIVATE_END);
        if (rawdata == null || rawdata == "")
            throw new Error.GENERAL("Private key contains no data.");

        secdata.rawdata = rawdata;

        // Guess the algorithm type by searching the base64 decoded data. (we
        // should properly exclude the start/end line, but it shouldn't harm
        // too much though afaik). Note that it's definitely not ideal though;
        // but the openssh format isn't exactly obvious
        var decoded = Base64.decode(rawdata.offset(initial_line.length));
        for (uint i = 0; i < decoded.length - 3; i++) {
            unowned var str = ((string) decoded).offset(i);
            var algo = Algorithm.from_string(str);
            if (algo != Algorithm.UNKNOWN) {
                secdata.algo = algo;
                break;
            }
        }

        return secdata;
    }

    /** Reads all lines from start until the end pattern and returns it */
    private static string parse_lines_block(DataInputStream data,
                                            string initial_line,
                                            string start, string end)
                                            throws GLib.Error {
        var result = new StringBuilder();

        string? line = initial_line;

        bool start_found = false;
        do {
            // Look for the beginning
            if (!start_found) {
                if (start in line) {
                    result.append(line);
                    result.append_c('\n');
                    start_found = true;
                    continue;
                }
            } else {
                // Look for the end
                result.append(line);
                result.append_c('\n');
                if (end in line)
                    break;
            }
        } while ((line = data.read_line_utf8()) != null);

        return result.str;
    }
}
