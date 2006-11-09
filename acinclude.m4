dnl AS_AC_EXPAND(VAR, CONFIGURE_VAR)
dnl
dnl example
dnl AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
dnl will set SYSCONFDIR to /usr/local/etc if prefix=/usr/local

AC_DEFUN([AS_AC_EXPAND],
[
    EXP_VAR=[$1]
    FROM_VAR=[$2]

    dnl first expand prefix and exec_prefix if necessary
    prefix_save=$prefix
    exec_prefix_save=$exec_prefix

    dnl if no prefix given, then use /usr/local, the default prefix
    if test "x$prefix" = "xNONE"; then
        prefix=$ac_default_prefix
    fi
    dnl if no exec_prefix given, then use prefix
    if test "x$exec_prefix" = "xNONE"; then
        exec_prefix=$prefix
    fi

    full_var="$FROM_VAR"
    dnl loop until it doesn't change anymore
    while true; do
        new_full_var="`eval echo $full_var`"
        if test "x$new_full_var"="x$full_var"; then break; fi
        full_var=$new_full_var
    done

    dnl clean up
    full_var=$new_full_var
    AC_SUBST([$1], "$full_var")

    dnl restore prefix and exec_prefix
    prefix=$prefix_save
    exec_prefix=$exec_prefix_save
])


# Check if mlock needs root or not
AC_DEFUN([SEAHORSE_CHECK_MLOCK],
  [ AC_CHECK_FUNCS(mlock)
    if test "$ac_cv_func_mlock" = "yes"; then
        AC_MSG_CHECKING(whether mlock is callable as a normal user)
          AC_CACHE_VAL(seahorse_cv_have_user_mlock,
             AC_TRY_RUN([
                #include <stdlib.h>
                #include <unistd.h>
                #include <errno.h>
                #include <sys/mman.h>
                #include <sys/types.h>
                #include <fcntl.h>

                int main () {
                    pool;
                    long int pgsize = getpagesize ();
                    char *pool = malloc (4096 + pgsize);
                    if(!pool)
                        return 2;
                    pool += (pgsize - ((long int)pool % pgsize));
                    if (mlock (pool, 4096) < 0) {
                        if(errno == EPERM)
                            return 1;
                        return 2;
                    }
                    return 0;
                }
            ],
            seahorse_cv_have_user_mlock="no",
            seahorse_cv_have_user_mlock="yes",
            seahorse_cv_have_user_mlock="assume-no"
           )
         )
         AM_CONDITIONAL(HAVE_USER_MLOCK, test "$seahorse_cv_have_user_mlock" = "yes")
         if test "$seahorse_cv_have_user_mlock" = "yes"; then
             AC_DEFINE(HAVE_USER_MLOCK,1,
                       [Defined if the mlock() call can be used by a non-superuser])
             AC_MSG_RESULT(yes)
         else
            if test "$seahorse_cv_have_user_mlock" = "no"; then
                AC_MSG_RESULT(no)
            else
                AC_MSG_RESULT(assuming no)
            fi
         fi
    fi
  ])
