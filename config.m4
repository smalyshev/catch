dnl $Id$
dnl config.m4 for extension catch

PHP_ARG_ENABLE(catch, whether to enable catch support,
 [  --enable-catch           Enable catch support])

if test "$PHP_CATCH" != "no"; then
  PHP_SUBST(CATCH_SHARED_LIBADD)
  PHP_NEW_EXTENSION(catch, catch.c, $ext_shared)
fi
