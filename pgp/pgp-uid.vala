/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

public class Seahorse.Pgp.Uid : Object {
    /**
     * Parent key
     */
    public Key parent { get; set construct; }

    /**
     * Signatures on this UID
     */
    public List<Pgp.Signature> signatures {
        get { return this._signatures; }
        set { set_signatures(value); }
    }
    private List<Pgp.Signature> _signatures;

    /**
     * Validity of this identity
     */
    public Seahorse.Validity validity { get; set; }

    /**
     * User ID name
     */
    public string name {
        get { return this._name; }
        set { set_name(value); }
    }
    private string _name = "";

    /**
     * User ID email
     */
    public string email {
        get { return this._email; }
        set { set_email(value); }
    }
    private string _email = "";

    /**
     * User ID comment (usually the email address)
     */
    public string comment {
        get { return this._comment; }
        set { set_comment(value); }
    }
    private string _comment = "";

    private bool realized; //XXX

    /**
     * Creates a new OpenPGP Uid.
     *
     * @parent: The {@link PGP.Key} this Uid belongs to.
     * @uid_string: The Uid in textual form.
     */
    public Uid(Key parent, string? uid_string) {
        this.parent = parent;

        if (uid_string != null)
            parse_user_id(uid_string, out this._name, out this._email, out this._comment);

        this.icon = null;
        this.usage = Seahorse.Usage.IDENTITY;

        realize();

        return uid;
    }

    public void realize() {
        // Don't realize if no name present
        if (!this.name)
            return;

        this.realized = true;

        this.label = calc_label(this.name, this.email, this.comment);
        this.markup = calc_markup(this.name, this.email, this.comment, 0);
    }

    public void set_signatures(List<Signature> signatures) {
        this._signatures = signatures.copy();
        notify_property("signatures");	
    }

    public void set_name(string name) {
        this._name = convert_string(name);

        if (!this.realized)
            realize();
        notify_property("name");
    }

    public void set_comment(string comment) {
        this._comment = convert_string(comment);

        if (!this.realized)
            realize();
        notify_property("comment");
    }

    public void set_email(string email) {
        this._email = convert_string(email);

        if (!this.realized)
            realize();
        notify_property("email");
    }

    public string calc_label(string? name, string? email, string? comment) {
        if (name == null)
            return null;

        StringBuilder label = new StringBuilder(name);
        if (email != null && email != "") {
            label.append(" <");
            label.append(email);
            label.append(">");
        }

        if (comment != null && comment != "") {
            label.append(" (");
            label.append(comment);
            label.append(")");
        }

        return label.str;
    }

    public string calc_markup (string? name, string? email, string? comment, Seahorse.Flags flags) {
        if (name == null)
            return null;

        bool strike = (Seahorse.Flags.EXPIRED in flags
                       || Seahorse.Flags.REVOKED in flags
                       || Seahorse.Flags.DISABLED in flags);

        bool grayed = !(Seahorse.Flags.TRUSTED in flags);

        string format;
        if (strike && grayed)
            format = "<span strikethrough='true' foreground='#555555'>%s<span size='small' rise='0'>%s%s%s%s%s</span></span>";
        else if (grayed)
            format = "<span foreground='#555555'>%s<span size='small' rise='0'>%s%s%s%s%s</span></span>";
        else if (strike)
            format = "<span strikethrough='true'>%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span></span>";
        else
            format = "%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span>";

        return Markup.printf_escaped(format, name,
                      email && email[0] ? "  " : "",
                      email && email[0] ? email : "",
                      comment && comment[0] ? "  '" : "",
                      comment && comment[0] ? comment : "",
                      comment && comment[0] ? "'" : "");
    }

    public Quark calc_id (Quark key_id, uint index) {
        string str = "%s:%u".printf(key_id.to_string(), index + 1);
        Quark id = Quark.from_string(str);

        return id;
    }

    // Copied from GPGME, later turned into Vala
    // Parses a user id, such as "Niels De Graef (optional random comment) <nielsdegraef@gmail.com>"
    private void parse_user_id (string uid, out string name, out string email, out string comment) {
        string src = uid;
        string tail = uid;
        string x = uid;
        bool in_name = false;
        int in_email = 0;
        int in_comment = 0;

        while(src != null) {
            if (in_email) {
                if (src[0] == '<')
                    // Not legal but anyway.
                    in_email++;
                else if (src[0] == '>') {
                    if (!--in_email && !*email) {
                        *email = tail;
                        src[0] = 0;
                        tail = src + 1;
                    }
                }
            } else if (in_comment) {
                if (src[0] == '(')
                    in_comment++;
                else if (src[0] == ')') {
                    if (!--in_comment && !*comment) {
                        *comment = tail;
                        src[0] = 0;
                        tail = src + 1;
                    }
                }
            } else if (src[0] == '<') {
                if (in_name) {
                    if (!*name) {
                        *name = tail;
                        src[0] = 0;
                        tail = src + 1;
                    }
                    in_name = false;
                } else
                    tail = src + 1;

                in_email = 1;
            } else if (src[0] == '(') {
                if (in_name) {
                    if (!*name) {
                        *name = tail;
                        src[0] = 0;
                        tail = src + 1;
                    }
                    in_name = false;
                }
                in_comment = 1;
            } else if (!in_name && src[0] != ' ' && src[0] != '\t') {
                in_name = true;
            }
            src++;
        }

        if (in_name) {
            if (!*name) {
                *name = tail;
                src[0] = 0;
                tail = src + 1;
            }
        }

        // Let unused parts point to an EOS.
        name = name ?? "";
        email = email ?? "";
        comment = comment ?? "";

        name._strip();
        email._strip();
        comment._strip();
    }
}
