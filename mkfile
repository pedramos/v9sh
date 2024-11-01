# ape mkfile for v9 sh
</$objtype/mkfile
BIN=$home/bin/$objtype

TARG=v9sh
OFILES=main.$O args.$O cmd.$O ctype.$O defs.$O error.$O expand.$O \
	fault.$O func.$O io.$O macro.$O msg.$O name.$O pathserv.$O \
	print.$O service.$O spname.$O stak.$O string.$O word.$O xec.$O
HFILES=ctype.h defs.h mac.h mode.h name.h stak.h sym.h

CFLAGS = -FTVw -DPLAN9 -D_POSIX_SOURCE -D_BSD_EXTENSION -D_SUSV2_SOURCE # -DBSD4_2
CC=pcc -c
LD=pcc

</sys/src/cmd/mkone
