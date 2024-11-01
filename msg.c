/*	@(#)msg.c	1.6	*/
/*
 *	UNIX shell
 *
 *	Bell Telephone Laboratories
 */


#include "defs.h"
#include "sym.h"

/*
 * error messages
 */
char	badopt[]	= "bad option(s)";
char	mailmsg[]	= "you have mail\n";
char	nospace[]	= "no memory";
char	nostack[]	= "no stack space";
char	synmsg[]	= "syntax error";

char	baddir[]	= "bad directory";
char	badexec[]	= "cannot execute";
char	badfile[]	= "bad file number";
char	badfname[]	= "illegal function name";
char	badnum[]	= "bad number";
char	badparam[]	= "parameter null or not set";
char	badreturn[]	= "cannot return when not in function";
char	badshift[]	= "cannot shift";
char	badsub[]	= "bad substitution";
char	badtrap[]	= "bad trap";
char	badunset[] 	= "cannot unset";
char	coredump[]	= " - core dumped";
char	nofork[]	= "fork failed - too many processes";
char	nohome[]	= "no home directory";
char	noswap[]	= "cannot fork: no swap space";
char	notbltin[]	= "not built in";
char	notfound[]	= "not found";
char	notid[]		= "is not a legal identifier";
char	piperr[]	= "cannot make pipe";
char	unset[]		= "parameter not set";

/*
 * built in names
 */
char	cdpname[]	= "CDPATH";
char	histname[]	= "HISTORY";
char	homename[]	= "HOME";
char	ifsname[]	= "IFS";
char	mailname[]	= "MAIL";
char	pathname[]	= "PATH";
char	ps1name[]	= "PS1";
char	ps2name[]	= "PS2";

/*
 * string constants
 */
char	nullstr[]	= "";
char	sptbnl[]	= " \t\n";
char	defpath[]	= ":/bin:/usr/bin";
char	colon[]		= ": ";
char	minus[]		= "-";
char	endoffile[]	= "end of file";
char	unexpected[] 	= " unexpected";
char	atline[]	= " at line ";
char	devnull[]	= "/dev/null";
char	execpmsg[]	= "+ ";
char	readmsg[]	= "> ";
char	stdprompt[]	= "$ ";
char	supprompt[]	= "# ";
char	profile[]	= ".profile";

/*
 * tables
 */

struct sysnod reserved[] = {
	{ "case",	CASYM	},
	{ "do",		DOSYM	},
	{ "done",	ODSYM	},
	{ "elif",	EFSYM	},
	{ "else",	ELSYM	},
	{ "esac",	ESSYM	},
	{ "fi",		FISYM	},
	{ "for",	FORSYM	},
	{ "if",		IFSYM	},
	{ "in",		INSYM	},
	{ "then",	THSYM	},
	{ "until",	UNSYM	},
	{ "while",	WHSYM	},
};

int no_reserved = sizeof reserved /sizeof reserved[0];

char *sysmsg[] = {
	0,
	"Hangup",
	0,		/* Interrupt */
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"abort",
	"EMT trap",
	"Floating exception",
	"Killed",
	"Bus error",
	"Memory fault",
	"Bad system call",
	0,	/* Broken pipe */
	"Alarm call",
	"Terminated",
	"Signal 16",
	"Signal 17",
	"Child death",
	"Power Fail"
};

char	export[] = "export";

struct sysnod commands[] = {
	{ ".",		SYSDOT	},
	{ ":",		SYSNULL	},
	{ "break",	SYSBREAK },
	{ "builtin",	SYSBLTIN },
	{ "cd",		SYSCD	},
	{ "continue",	SYSCONT	},
	{ "eval",	SYSEVAL	},
	{ "exec",	SYSEXEC	},
	{ "exit",	SYSEXIT	},
	{ "export",	SYSXPORT },
	{ "newgrp",	SYSNEWGRP },
	{ "read",	SYSREAD	},
	{ "return",	SYSRETURN },
	{ "set",	SYSSET	},
	{ "shift",	SYSSHFT	},
	{ "times",	SYSTIMES },
	{ "trap",	SYSTRAP	},
	{ "umask",	SYSUMASK },
	{ "unset", 	SYSUNS },
	{ "wait",	SYSWAIT	},
	{ "whatis",	SYSWHATIS }
};

int no_commands = sizeof commands /sizeof commands[0];
