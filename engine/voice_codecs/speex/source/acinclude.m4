AC_DEFUN(AC_FIND_FILE,
[
$3=NONE
for i in $2;
do
  for j in $1;
  do
    if test -r "$i/$j"; then
      $3=$i
      break 2
    fi
  done
done
])

AC_DEFUN(AC_PATH_LIBOGG,
[
OGG_LIBS="-logg"

AC_MSG_CHECKING([for libogg])

ac_ogg_includes=NONE ac_ogg_libraries=NONE ac_ogg_bindir=NONE
ogg_libraries=""
ogg_includes=""
AC_ARG_WITH(ogg-dir,
    [  --with-ogg-dir=DIR       where the root of OGG is installed ],
    [  ac_ogg_includes="$withval"/include
       ac_ogg_libraries="$withval"/lib
    ])

AC_ARG_WITH(ogg-includes,
    [  --with-ogg-includes=DIR  where the OGG includes are. ],
    [  
       ac_ogg_includes="$withval"
    ])
    
ogg_libs_given=no

AC_ARG_WITH(ogg-libraries,
    [  --with-ogg-libraries=DIR where the OGG library is installed.],
    [  ac_ogg_libraries="$withval"
       ogg_libs_given=yes
    ])

ogg_incdirs="/usr/include /usr/lib/ogg/include /opt/include /usr/local/ogg/include /usr/include/ogg /usr/include /usr/local/include"
if test ! "$ac_ogg_includes" = "NONE"; then
  ogg_incdirs="$ac_ogg_includes $ac_ogg_includes/.. $ogg_incdirs"
fi
AC_FIND_FILE(ogg/ogg.h, $ogg_incdirs, ogg_incdir)
echo "Ogg includes in $ogg_incdir"


ogg_libdirs="$ac_ogg_libraries /usr/lib/ogg/lib /usr/lib /opt/lib /usr/local/ogg/lib /usr/local/lib /usr/lib/ogg /usr/local/lib"
test -n "$OGGDIR" && ogg_libdirs="$OGGDIR/lib $OGGDIR $ogg_libdirs"
if test ! "$ac_ogg_libraries" = "NONE"; then
  ogg_libdirs="$ac_ogg_libraries $ogg_libdirs"
fi

test=NONE
ogg_libdir=NONE
for dir in $ogg_libdirs; do
  try="ls -1 $dir/libogg*"
  if test=`eval $try 2> /dev/null`; then ogg_libdir=$dir; break; else echo "tried $dir" >&AC_FD_CC ; fi
done

echo "Ogg libraries in $ogg_libdir"

if test "$ogg_libdir" = "NONE" || test "$ogg_incdir" = "NONE"; then
   have_libogg=no
else
   have_libogg=yes
   AC_DEFINE(HAVE_LIBOGG)
fi

OGG_INCLUDES="-I$ogg_incdir"
OGG_LDFLAGS="-L$ogg_libdir"


AC_SUBST(OGG_LIBS)
AC_SUBST(OGG_INCLUDES)
AC_SUBST(OGG_LDFLAGS)

])

