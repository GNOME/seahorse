/*
 * Seahorse
 *
 * Copyright (C) 2023 Niels De Graef
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

void main(string[] args) {
  Test.init(ref args);

  Test.add_func("/ssh/key-parse/pubkey-simple", test_key_parse_pubkey_simple);
  Test.add_func("/ssh/key-parse/pubkey-bad-algo", test_key_parse_pubkey_bad_algo);
  Test.add_func("/ssh/key-parse/pubkey-multple", test_key_parse_pubkey_multiple);

  Test.add_func("/ssh/key-parse/private-key-simple", test_key_parse_private_key_simple);
  Test.add_func("/ssh/key-parse/private-key-pw-protected", test_key_parse_private_key_pw_protected);

  Test.run();
}

const string PUBKEY_SIMPLE = """
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCi9ZNp78OzcMpR9QeSKKCybNxTR+ailXs3cwizr1R9Dlx/EobQBXOwE2Ed5PqSU5HEgtYRoKqlTxMogMXMX508dedC0ADTzM09B3OBqpZ7YnMuyLbtk1MNP8xcvVmHwwfw3Y79xxZZeqjUTI7cSE6jcNyz/k/Dl+6RYI552ab80b1kgDDwOyUL75hFllEZ9vHCcAOtk7y5LyeUpnRu5WJq0YBPVQljeYs23ZiTSo5NkJd7pvV9hs68ZAYqm1POXwCcAKOj4HXW3AL83AD49g8MJOAelCMaJpUkOgn4n4QTtqLEC108sqZgwWiadbN/ZHt3Idbn3AIxMMhD/wdkSwkfm9tAohMrqYpSiG31xyifH61mcoBMSxRMQhUscCGV3kLo6P/dZtxRbu4r74r/Ae2Jg4pzYrVFVzfObXdlTjtxJmR8UvnZg60OE0RwYMs1LJTE6xakcAg22O9i3bau00MoIIYEPgiFFP5t0Tw3D06BcEzr/2wEzlvbxy0qzDTr40U= testsimple
""";

private void test_key_parse_pubkey_simple() {
  var input = new MemoryInputStream.from_data(PUBKEY_SIMPLE.data);

  var mainloop = new GLib.MainLoop();
  Seahorse.Ssh.Key.parse.begin(input, null, (obj, res) => {
      try {
          var parse_result = Seahorse.Ssh.Key.parse.end(res);

          assert_true(parse_result.public_keys.length == 1);
          unowned var keydata = parse_result.public_keys[0];
          assert_true(keydata.comment == "testsimple");
          assert_true(keydata.algo == Seahorse.Ssh.Algorithm.RSA);

          assert_true(parse_result.secret_keys.length == 0);
      } catch (Error err) {
          error("Couldn't parse public key: %s", err.message);
      } finally {
          mainloop.quit();
      }
  });
  mainloop.run();
}

// Basically the same as PUBKEY_SIMPLE, but a bogus algorithm
const string PUBKEY_BAD_ALGO = """
ssh-blabla AAAAB3NzaC1yc2EAAAADAQABAAABgQCi9ZNp78OzcMpR9QeSKKCybNxTR+ailXs3cwizr1R9Dlx/EobQBXOwE2Ed5PqSU5HEgtYRoKqlTxMogMXMX508dedC0ADTzM09B3OBqpZ7YnMuyLbtk1MNP8xcvVmHwwfw3Y79xxZZeqjUTI7cSE6jcNyz/k/Dl+6RYI552ab80b1kgDDwOyUL75hFllEZ9vHCcAOtk7y5LyeUpnRu5WJq0YBPVQljeYs23ZiTSo5NkJd7pvV9hs68ZAYqm1POXwCcAKOj4HXW3AL83AD49g8MJOAelCMaJpUkOgn4n4QTtqLEC108sqZgwWiadbN/ZHt3Idbn3AIxMMhD/wdkSwkfm9tAohMrqYpSiG31xyifH61mcoBMSxRMQhUscCGV3kLo6P/dZtxRbu4r74r/Ae2Jg4pzYrVFVzfObXdlTjtxJmR8UvnZg60OE0RwYMs1LJTE6xakcAg22O9i3bau00MoIIYEPgiFFP5t0Tw3D06BcEzr/2wEzlvbxy0qzDTr40U= testbad
""";

private void test_key_parse_pubkey_bad_algo() {
  var input = new MemoryInputStream.from_data(PUBKEY_BAD_ALGO.data);

  var mainloop = new GLib.MainLoop();
  Seahorse.Ssh.Key.parse.begin(input, null, (obj, res) => {
      try {
          Seahorse.Ssh.Key.parse.end(res);
          assert_not_reached();
      } catch (Error err) {
          // Expected
      } finally {
          mainloop.quit();
      }
  });
  mainloop.run();
}

const string PUBKEY_MULTIPLE = """
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCi9ZNp78OzcMpR9QeSKKCybNxTR+ailXs3cwizr1R9Dlx/EobQBXOwE2Ed5PqSU5HEgtYRoKqlTxMogMXMX508dedC0ADTzM09B3OBqpZ7YnMuyLbtk1MNP8xcvVmHwwfw3Y79xxZZeqjUTI7cSE6jcNyz/k/Dl+6RYI552ab80b1kgDDwOyUL75hFllEZ9vHCcAOtk7y5LyeUpnRu5WJq0YBPVQljeYs23ZiTSo5NkJd7pvV9hs68ZAYqm1POXwCcAKOj4HXW3AL83AD49g8MJOAelCMaJpUkOgn4n4QTtqLEC108sqZgwWiadbN/ZHt3Idbn3AIxMMhD/wdkSwkfm9tAohMrqYpSiG31xyifH61mcoBMSxRMQhUscCGV3kLo6P/dZtxRbu4r74r/Ae2Jg4pzYrVFVzfObXdlTjtxJmR8UvnZg60OE0RwYMs1LJTE6xakcAg22O9i3bau00MoIIYEPgiFFP5t0Tw3D06BcEzr/2wEzlvbxy0qzDTr40U= test1
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQDADxtTchMRaGP2YCG5Iiu/sCkdKYkogkZ9NrawGutvwcoQL4D3oIC+r+ka3YzenICz0Rpu4ZIyGzn5s7VISzjcGK99a8/UoXsSyTUy37ihbpAbkYju+avtJZyCMOrRyrXp0W8gSKnUuxDJJNlG5+Jhjdh35rGo7ENSDIGRBXx0uLmKvxbPGSQoULbUzxJRVes6HW7cr44RXG4GL5uekS5hSHN/wJuc/OBqT2ETn+Ivn8cj2fooR6Y7Ei6uFNwbNebYgzDzNOTfaGssCXoEqWL9dJA8FAXA9u4BQskIaUm9SiQblzKGZSKtoPPFh7Sp7Ii1k2TAG0g9VW8gYqUZgLZY4OKjARR+SkdyzlnJKOD1QuSIrnyDNOgl7SBwtujT6gg+9bcQzSPVRWuPldYj2qYEZiR7LPood7AvzDL2tZwb8r60O5KDip+66inj5BxmPO6Vmeo84DLXdZNhEnoCfaL5J9qArhTvxYPjU8RiuBoy6nqN00IWeT2e3RZTPjpq1wc= test2
""";

private void test_key_parse_pubkey_multiple() {
  var input = new MemoryInputStream.from_data(PUBKEY_MULTIPLE.data);

  var mainloop = new GLib.MainLoop();
  Seahorse.Ssh.Key.parse.begin(input, null, (obj, res) => {
      try {
          var parse_result = Seahorse.Ssh.Key.parse.end(res);
          assert_true(parse_result.public_keys.length == 2);
          assert_true(parse_result.secret_keys.length == 0);
      } catch (Error err) {
          error("Couldn't parse public key: %s", err.message);
      } finally {
          mainloop.quit();
      }
  });
  mainloop.run();
}

const string PRIVATE_KEY_SIMPLE = """
-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAABlwAAAAdzc2gtcn
NhAAAAAwEAAQAAAYEAvR0sY6LuyQBk0eWtHlblilTu5ywOxIPtX5Xz1DQ/nTs3+EiuUjpX
7wpBd0X8KLVwF8XqrgsS/cZGKKwwTTIthifiOkIin2M1c5zqjq5mYXLd8OVy7FPGz3kRcY
bqcZ5IhH6rrccLKgqz8F8YDZ1PP/zViRV6pjL4M9V+vM50JNbc09o199rOhrIfzWpbmbsU
L98ADNFdyAexJAo46I5KF4ABp43y8fxKVkOQmouN92ao5s4tktkU2RDieAQEGS5tCcuabM
vyq+nsZ3SxhAAM+k1A98F4sDaPBLMvQrGTRfIhbtPEqJsffzmoNiLsuFooCB8bbrpb5gp0
pvoQCDomWPR618KJSt6c2JCpxd0v64N+xaGjeuKnH+nGeQ2I8KjGz49bpTsNU/XHSAJHS8
72GiVZ1sVeEGxU2nR58Faixg6quEQr/SjjFCLTjV1v9zXmYLGUC2WoYrvh8Xv1/5u11WG9
jxbZrwNeHR5b0mL4espyAaxhyP093cRj0am3HX03AAAFiLLBfcCywX3AAAAAB3NzaC1yc2
EAAAGBAL0dLGOi7skAZNHlrR5W5YpU7ucsDsSD7V+V89Q0P507N/hIrlI6V+8KQXdF/Ci1
cBfF6q4LEv3GRiisME0yLYYn4jpCIp9jNXOc6o6uZmFy3fDlcuxTxs95EXGG6nGeSIR+q6
3HCyoKs/BfGA2dTz/81YkVeqYy+DPVfrzOdCTW3NPaNffazoayH81qW5m7FC/fAAzRXcgH
sSQKOOiOSheAAaeN8vH8SlZDkJqLjfdmqObOLZLZFNkQ4ngEBBkubQnLmmzL8qvp7Gd0sY
QADPpNQPfBeLA2jwSzL0Kxk0XyIW7TxKibH385qDYi7LhaKAgfG266W+YKdKb6EAg6Jlj0
etfCiUrenNiQqcXdL+uDfsWho3ripx/pxnkNiPCoxs+PW6U7DVP1x0gCR0vO9holWdbFXh
BsVNp0efBWosYOqrhEK/0o4xQi041db/c15mCxlAtlqGK74fF79f+btdVhvY8W2a8DXh0e
W9Ji+HrKcgGsYcj9Pd3EY9Gptx19NwAAAAMBAAEAAAGAEepQGLZIObl0U6AW+N9RinvGUB
cP5RT8aUg625kBh8Mi57326apGR0po7kQugarCjjX9J/S7nVfpsJOzVbTRtDpWB5/ZSNEs
sKGmZNLntwabOOV7sCC1nlUBTohx8EaG5ypa2DEZgSeXaUeQ70U+SzkH/58Nye3dLofkpD
1Iqm7CZ71tzGeplgAM3DhdqiAbZveQuSYiZL85zEi9oGZOZZCGV5mucLcuUaK/8awTzGKo
0Iiqr5UqEPA1DBqRStNnbfu265Zc/VacI0Z+00dMiO7VQ1lB5TnO9MkuEU3Kd59sHKvuX0
Hw5V2BqvJVqdqF5j6yxAExD7J8JNybcpE+LXzgysTBAEg2QPQa4GzWERytWSddVU+LTrEu
BOb8o2bMV4eHbo7Y06ziq+MJQIDWJr7qNMbDtaV5UCNWcIlYaKX9X8si0qFBzHPRTx53aV
beb4C5G8Ce4HU4Q3eEC0idCSpALEZEb8NIqxHigWuQKlqAhju31IIFjPoJ8uvs/49VAAAA
wD2DtKmKzpZrguKjUrMTtCyh8cRxvvACe5ij6oPlhnJJ3Hlyb5QkH/FMqudJhkOaZrLjz0
JFLXQ4SUoUxdnolBb5B6wNfL66CXCurwR8MzHxHZyNL2+EB/jHhgF5Oh631LShe9GHJ+nE
umN9zzDmW4pfWIoUZbzaNi2IQS4Qv+pJWk/uQFzqnS39HQkdgNh8s3sPJQUvykgbD24vNu
qzML23S3NxsJfxJzsl8HL7LTaf08toSXeNqif4iPxuy0F9nAAAAMEA+MjSgU7GxqAjgE+7
zTKbEAw2WZhSWk+sHDSzTpt7T6JhTbsD0I+ZXqb3lcrKTnxjGP8bj4zWeH/mtpVCRDb/xE
cOjku/aszvCBHKcD9naE+U/pOqwWR60jPb65+2hvBxqCFKb+2FVeM7E7BYL4kSQ/FV3svu
rCQ0+liFnD4VxDbvNHDXz2h9RlqYTjZ90/d5I4oibuHPvcy3jMri99qBuvPj/8BhfPBmKw
o+VZpJbWHEfuDZx1Rr3vRx3VtmfLLFAAAAwQDCmU8SYZ/GYR/EQU3MfIK/3wVKiasBh3a/
gIgD1+wnobau0MHd8vvGdehLPNVHKQ0/QkOsIdQl1XEKXFaNNGo8ZMHsawacdRze7ulfpe
LSYdlqKqu73m2wy8LsY+IGSZGRkjCK1LNJevU8ec0nHSUGIn49VK+jPPjyNGHZw5yIL8QE
nmvLcmgqiGQHjsArmk4NAdA5LGJSF3EfSKDx1Rs1CKCn/3+tpIbTpVZl6r1IDpuLPOvtYO
f5GQO1BGgcf8sAAAAQbmRlZ3JhZWZAdG9vbGJveAECAw==
-----END OPENSSH PRIVATE KEY-----
""";

private void test_key_parse_private_key_simple() {
  var input = new MemoryInputStream.from_data(PRIVATE_KEY_SIMPLE.data);

  var mainloop = new GLib.MainLoop();
  Seahorse.Ssh.Key.parse.begin(input, null, (obj, res) => {
      try {
          var parse_result = Seahorse.Ssh.Key.parse.end(res);
          assert_true(parse_result.public_keys.length == 0);

          assert_true(parse_result.secret_keys.length == 1);
          unowned var secdata = parse_result.secret_keys[0];
          assert_null(secdata.comment);
          assert_true(secdata.algo == Seahorse.Ssh.Algorithm.RSA);
      } catch (Error err) {
          error("Couldn't parse private key: %s", err.message);
      } finally {
          mainloop.quit();
      }
  });
  mainloop.run();
}

// Key with "test" password
const string PRIVATE_KEY_PW_PROTECTED = """
-----BEGIN OPENSSH PRIVATE KEY-----
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
Znmynq4dXIf58Nj4+TpgQkxQwxM=
-----END OPENSSH PRIVATE KEY-----
""";

private void test_key_parse_private_key_pw_protected() {
  var input = new MemoryInputStream.from_data(PRIVATE_KEY_PW_PROTECTED.data);

  var mainloop = new GLib.MainLoop();
  Seahorse.Ssh.Key.parse.begin(input, null, (obj, res) => {
      try {
          var parse_result = Seahorse.Ssh.Key.parse.end(res);
          assert_true(parse_result.public_keys.length == 0);

          assert_true(parse_result.secret_keys.length == 1);
          unowned var secdata = parse_result.secret_keys[0];
          assert_null(secdata.comment);
          assert_true(secdata.algo == Seahorse.Ssh.Algorithm.RSA);
      } catch (Error err) {
          error("Couldn't parse private key: %s", err.message);
      } finally {
          mainloop.quit();
      }
  });
  mainloop.run();
}
