/* $RCSfile: doio.c,v $$Revision: 4.1 $$Date: 92/08/07 17:19:42 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	doio.c,v $
 * Revision 4.1  92/08/07  17:19:42  lwall
 * Stage 6 Snapshot
 * 
 * Revision 4.0.1.6  92/06/11  21:08:16  lwall
 * patch34: some systems don't declare h_errno extern in header files
 * 
 * Revision 4.0.1.5  92/06/08  13:00:21  lwall
 * patch20: some machines don't define ENOTSOCK in errno.h
 * patch20: new warnings for failed use of stat operators on filenames with \n
 * patch20: wait failed when STDOUT or STDERR reopened to a pipe
 * patch20: end of file latch not reset on reopen of STDIN
 * patch20: seek(HANDLE, 0, 1) went to eof because of ancient Ultrix workaround
 * patch20: fixed memory leak on system() for vfork() machines
 * patch20: get*by* routines now return something useful in a scalar context
 * patch20: h_errno now accessible via $?
 * 
 * Revision 4.0.1.4  91/11/05  16:51:43  lwall
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: perl mistook some streams for sockets because they return mode 0 too
 * patch11: reopening STDIN, STDOUT and STDERR failed on some machines
 * patch11: certain perl errors should set EBADF so that $! looks better
 * patch11: truncate on a closed filehandle could dump
 * patch11: stats of _ forgot whether prior stat was actually lstat
 * patch11: -T returned true on NFS directory
 * 
 * Revision 4.0.1.3  91/06/10  01:21:19  lwall
 * patch10: read didn't work from character special files open for writing
 * patch10: close-on-exec wrongly set on system file descriptors
 * 
 * Revision 4.0.1.2  91/06/07  10:53:39  lwall
 * patch4: new copyright notice
 * patch4: system fd's are now treated specially
 * patch4: added $^F variable to specify maximum system fd, default 2
 * patch4: character special files now opened with bidirectional stdio buffers
 * patch4: taintchecks could improperly modify parent in vfork()
 * patch4: many, many itty-bitty portability fixes
 * 
 * Revision 4.0.1.1  91/04/11  17:41:06  lwall
 * patch1: hopefully straightened out some of the Xenix mess
 * 
 * Revision 4.0  91/03/20  01:07:06  lwall
 * 4.0 baseline.
 * 
 */

#include "EXTERN.h"
#include "perl.h"

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#include <sys/ipc.h>
#ifdef HAS_MSG
#include <sys/msg.h>
#endif
#ifdef HAS_SEM
#include <sys/sem.h>
#endif
#ifdef HAS_SHM
#include <sys/shm.h>
#endif
#endif

#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

bool
do_open(gv,name,len)
GV *gv;
register char *name;
I32 len;
{
    FILE *fp;
    register IO *io = GvIO(gv);
    char *myname = savestr(name);
    int result;
    int fd;
    int writing = 0;
    char mode[3];		/* stdio file mode ("r\0" or "r+\0") */
    FILE *saveifp = Nullfp;
    FILE *saveofp = Nullfp;
    char savetype = ' ';

    mode[0] = mode[1] = mode[2] = '\0';
    name = myname;
    forkprocess = 1;		/* assume true if no fork */
    while (len && isSPACE(name[len-1]))
	name[--len] = '\0';
    if (!io)
	io = GvIO(gv) = newIO();
    else if (io->ifp) {
	fd = fileno(io->ifp);
	if (io->type == '-')
	    result = 0;
	else if (fd <= maxsysfd) {
	    saveifp = io->ifp;
	    saveofp = io->ofp;
	    savetype = io->type;
	    result = 0;
	}
	else if (io->type == '|')
	    result = my_pclose(io->ifp);
	else if (io->ifp != io->ofp) {
	    if (io->ofp) {
		result = fclose(io->ofp);
		fclose(io->ifp);	/* clear stdio, fd already closed */
	    }
	    else
		result = fclose(io->ifp);
	}
	else
	    result = fclose(io->ifp);
	if (result == EOF && fd > maxsysfd)
	    fprintf(stderr,"Warning: unable to close filehandle %s properly.\n",
	      GvENAME(gv));
	io->ofp = io->ifp = Nullfp;
    }
    if (*name == '+' && len > 1 && name[len-1] != '|') {	/* scary */
	mode[1] = *name++;
	mode[2] = '\0';
	--len;
	writing = 1;
    }
    else  {
	mode[1] = '\0';
    }
    io->type = *name;
    if (*name == '|') {
	/*SUPPRESS 530*/
	for (name++; isSPACE(*name); name++) ;
	if (strNE(name,"-"))
	    TAINT_ENV();
	TAINT_PROPER("piped open");
	fp = my_popen(name,"w");
	writing = 1;
    }
    else if (*name == '>') {
	TAINT_PROPER("open");
	name++;
	if (*name == '>') {
	    mode[0] = io->type = 'a';
	    name++;
	}
	else
	    mode[0] = 'w';
	writing = 1;
	if (*name == '&') {
	  duplicity:
	    name++;
	    while (isSPACE(*name))
		name++;
	    if (isDIGIT(*name))
		fd = atoi(name);
	    else {
		gv = gv_fetchpv(name,FALSE);
		if (!gv || !GvIO(gv)) {
#ifdef EINVAL
		    errno = EINVAL;
#endif
		    goto say_false;
		}
		if (GvIO(gv) && GvIO(gv)->ifp) {
		    fd = fileno(GvIO(gv)->ifp);
		    if (GvIO(gv)->type == 's')
			io->type = 's';
		}
		else
		    fd = -1;
	    }
	    if (!(fp = fdopen(fd = dup(fd),mode))) {
		close(fd);
	    }
	}
	else {
	    while (isSPACE(*name))
		name++;
	    if (strEQ(name,"-")) {
		fp = stdout;
		io->type = '-';
	    }
	    else  {
		fp = fopen(name,mode);
	    }
	}
    }
    else {
	if (*name == '<') {
	    mode[0] = 'r';
	    name++;
	    while (isSPACE(*name))
		name++;
	    if (*name == '&')
		goto duplicity;
	    if (strEQ(name,"-")) {
		fp = stdin;
		io->type = '-';
	    }
	    else
		fp = fopen(name,mode);
	}
	else if (name[len-1] == '|') {
	    name[--len] = '\0';
	    while (len && isSPACE(name[len-1]))
		name[--len] = '\0';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strNE(name,"-"))
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    fp = my_popen(name,"r");
	    io->type = '|';
	}
	else {
	    io->type = '<';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strEQ(name,"-")) {
		fp = stdin;
		io->type = '-';
	    }
	    else
		fp = fopen(name,"r");
	}
    }
    if (!fp) {
	if (dowarn && io->type == '<' && index(name, '\n'))
	    warn(warn_nl, "open");
	Safefree(myname);
	goto say_false;
    }
    Safefree(myname);
    if (io->type &&
      io->type != '|' && io->type != '-') {
	if (fstat(fileno(fp),&statbuf) < 0) {
	    (void)fclose(fp);
	    goto say_false;
	}
	if (S_ISSOCK(statbuf.st_mode))
	    io->type = 's';	/* in case a socket was passed in to us */
#ifdef HAS_SOCKET
	else if (
#ifdef S_IFMT
	    !(statbuf.st_mode & S_IFMT)
#else
	    !statbuf.st_mode
#endif
	) {
	    I32 buflen = sizeof tokenbuf;
	    if (getsockname(fileno(fp), tokenbuf, &buflen) >= 0
		|| errno != ENOTSOCK)
		io->type = 's'; /* some OS's return 0 on fstat()ed socket */
				/* but some return 0 for streams too, sigh */
	}
#endif
    }
    if (saveifp) {		/* must use old fp? */
	fd = fileno(saveifp);
	if (saveofp) {
	    fflush(saveofp);		/* emulate fclose() */
	    if (saveofp != saveifp) {	/* was a socket? */
		fclose(saveofp);
		if (fd > 2)
		    Safefree(saveofp);
	    }
	}
	if (fd != fileno(fp)) {
	    int pid;
	    SV *sv;

	    dup2(fileno(fp), fd);
	    sv = *av_fetch(fdpid,fileno(fp),TRUE);
	    SvUPGRADE(sv, SVt_IV);
	    pid = SvIV(sv);
	    SvIV(sv) = 0;
	    sv = *av_fetch(fdpid,fd,TRUE);
	    SvUPGRADE(sv, SVt_IV);
	    SvIV(sv) = pid;
	    fclose(fp);

	}
	fp = saveifp;
	clearerr(fp);
    }
#if defined(HAS_FCNTL) && defined(FFt_SETFD)
    fd = fileno(fp);
    fcntl(fd,FFt_SETFD,fd > maxsysfd);
#endif
    io->ifp = fp;
    if (writing) {
	if (io->type == 's'
	  || (io->type == '>' && S_ISCHR(statbuf.st_mode)) ) {
	    if (!(io->ofp = fdopen(fileno(fp),"w"))) {
		fclose(fp);
		io->ifp = Nullfp;
		goto say_false;
	    }
	}
	else
	    io->ofp = fp;
    }
    return TRUE;

say_false:
    io->ifp = saveifp;
    io->ofp = saveofp;
    io->type = savetype;
    return FALSE;
}

FILE *
nextargv(gv)
register GV *gv;
{
    register SV *sv;
#ifndef FLEXFILENAMES
    int filedev;
    int fileino;
#endif
    int fileuid;
    int filegid;

    if (!argvoutgv)
	argvoutgv = gv_fetchpv("ARGVOUT",TRUE);
    if (filemode & (S_ISUID|S_ISGID)) {
	fflush(GvIO(argvoutgv)->ifp);  /* chmod must follow last write */
#ifdef HAS_FCHMOD
	(void)fchmod(lastfd,filemode);
#else
	(void)chmod(oldname,filemode);
#endif
    }
    filemode = 0;
    while (av_len(GvAV(gv)) >= 0) {
	sv = av_shift(GvAV(gv));
	sv_setsv(GvSV(gv),sv);
	SvSETMAGIC(GvSV(gv));
	oldname = SvPVnx(GvSV(gv));
	if (do_open(gv,oldname,SvCUR(GvSV(gv)))) {
	    if (inplace) {
		TAINT_PROPER("inplace open");
		if (strEQ(oldname,"-")) {
		    sv_free(sv);
		    defoutgv = gv_fetchpv("STDOUT",TRUE);
		    return GvIO(gv)->ifp;
		}
#ifndef FLEXFILENAMES
		filedev = statbuf.st_dev;
		fileino = statbuf.st_ino;
#endif
		filemode = statbuf.st_mode;
		fileuid = statbuf.st_uid;
		filegid = statbuf.st_gid;
		if (!S_ISREG(filemode)) {
		    warn("Can't do inplace edit: %s is not a regular file",
		      oldname );
		    do_close(gv,FALSE);
		    sv_free(sv);
		    continue;
		}
		if (*inplace) {
#ifdef SUFFIX
		    add_suffix(sv,inplace);
#else
		    sv_catpv(sv,inplace);
#endif
#ifndef FLEXFILENAMES
		    if (stat(SvPV(sv),&statbuf) >= 0
		      && statbuf.st_dev == filedev
		      && statbuf.st_ino == fileino ) {
			warn("Can't do inplace edit: %s > 14 characters",
			  SvPV(sv) );
			do_close(gv,FALSE);
			sv_free(sv);
			continue;
		    }
#endif
#ifdef HAS_RENAME
#ifndef DOSISH
		    if (rename(oldname,SvPV(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPV(sv), strerror(errno) );
			do_close(gv,FALSE);
			sv_free(sv);
			continue;
		    }
#else
		    do_close(gv,FALSE);
		    (void)unlink(SvPV(sv));
		    (void)rename(oldname,SvPV(sv));
		    do_open(gv,SvPV(sv),SvCUR(GvSV(gv)));
#endif /* MSDOS */
#else
		    (void)UNLINK(SvPV(sv));
		    if (link(oldname,SvPV(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPV(sv), strerror(errno) );
			do_close(gv,FALSE);
			sv_free(sv);
			continue;
		    }
		    (void)UNLINK(oldname);
#endif
		}
		else {
#ifndef DOSISH
		    if (UNLINK(oldname) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPV(sv), strerror(errno) );
			do_close(gv,FALSE);
			sv_free(sv);
			continue;
		    }
#else
		    fatal("Can't do inplace edit without backup");
#endif
		}

		sv_setpvn(sv,">",1);
		sv_catpv(sv,oldname);
		errno = 0;		/* in case sprintf set errno */
		if (!do_open(argvoutgv,SvPV(sv),SvCUR(sv))) {
		    warn("Can't do inplace edit on %s: %s",
		      oldname, strerror(errno) );
		    do_close(gv,FALSE);
		    sv_free(sv);
		    continue;
		}
		defoutgv = argvoutgv;
		lastfd = fileno(GvIO(argvoutgv)->ifp);
		(void)fstat(lastfd,&statbuf);
#ifdef HAS_FCHMOD
		(void)fchmod(lastfd,filemode);
#else
		(void)chmod(oldname,filemode);
#endif
		if (fileuid != statbuf.st_uid || filegid != statbuf.st_gid) {
#ifdef HAS_FCHOWN
		    (void)fchown(lastfd,fileuid,filegid);
#else
#ifdef HAS_CHOWN
		    (void)chown(oldname,fileuid,filegid);
#endif
#endif
		}
	    }
	    sv_free(sv);
	    return GvIO(gv)->ifp;
	}
	else
	    fprintf(stderr,"Can't open %s: %s\n",SvPVn(sv), strerror(errno));
	sv_free(sv);
    }
    if (inplace) {
	(void)do_close(argvoutgv,FALSE);
	defoutgv = gv_fetchpv("STDOUT",TRUE);
    }
    return Nullfp;
}

#ifdef HAS_PIPE
void
do_pipe(sv, rgv, wgv)
SV *sv;
GV *rgv;
GV *wgv;
{
    register IO *rstio;
    register IO *wstio;
    int fd[2];

    if (!rgv)
	goto badexit;
    if (!wgv)
	goto badexit;

    rstio = GvIO(rgv);
    wstio = GvIO(wgv);

    if (!rstio)
	rstio = GvIO(rgv) = newIO();
    else if (rstio->ifp)
	do_close(rgv,FALSE);
    if (!wstio)
	wstio = GvIO(wgv) = newIO();
    else if (wstio->ifp)
	do_close(wgv,FALSE);

    if (pipe(fd) < 0)
	goto badexit;
    rstio->ifp = fdopen(fd[0], "r");
    wstio->ofp = fdopen(fd[1], "w");
    wstio->ifp = wstio->ofp;
    rstio->type = '<';
    wstio->type = '>';
    if (!rstio->ifp || !wstio->ofp) {
	if (rstio->ifp) fclose(rstio->ifp);
	else close(fd[0]);
	if (wstio->ofp) fclose(wstio->ofp);
	else close(fd[1]);
	goto badexit;
    }

    sv_setsv(sv,&sv_yes);
    return;

badexit:
    sv_setsv(sv,&sv_undef);
    return;
}
#endif

bool
do_close(gv,explicit)
GV *gv;
bool explicit;
{
    bool retval = FALSE;
    register IO *io;
    int status;

    if (!gv)
	gv = argvgv;
    if (!gv) {
	errno = EBADF;
	return FALSE;
    }
    io = GvIO(gv);
    if (!io) {		/* never opened */
	if (dowarn && explicit)
	    warn("Close on unopened file <%s>",GvENAME(gv));
	return FALSE;
    }
    if (io->ifp) {
	if (io->type == '|') {
	    status = my_pclose(io->ifp);
	    retval = (status == 0);
	    statusvalue = (unsigned short)status & 0xffff;
	}
	else if (io->type == '-')
	    retval = TRUE;
	else {
	    if (io->ofp && io->ofp != io->ifp) {		/* a socket */
		retval = (fclose(io->ofp) != EOF);
		fclose(io->ifp);	/* clear stdio, fd already closed */
	    }
	    else
		retval = (fclose(io->ifp) != EOF);
	}
	io->ofp = io->ifp = Nullfp;
    }
    if (explicit) {
	io->lines = 0;
	io->page = 0;
	io->lines_left = io->page_len;
    }
    io->type = ' ';
    return retval;
}

bool
do_eof(gv)
GV *gv;
{
    register IO *io;
    int ch;

    io = GvIO(gv);

    if (!io)
	return TRUE;

    while (io->ifp) {

#ifdef STDSTDIO			/* (the code works without this) */
	if (io->ifp->_cnt > 0)	/* cheat a little, since */
	    return FALSE;		/* this is the most usual case */
#endif

	ch = getc(io->ifp);
	if (ch != EOF) {
	    (void)ungetc(ch, io->ifp);
	    return FALSE;
	}
#ifdef STDSTDIO
	if (io->ifp->_cnt < -1)
	    io->ifp->_cnt = -1;
#endif
	if (gv == argvgv) {		/* not necessarily a real EOF yet? */
	    if (!nextargv(argvgv))	/* get another fp handy */
		return TRUE;
	}
	else
	    return TRUE;		/* normal fp, definitely end of file */
    }
    return TRUE;
}

long
do_tell(gv)
GV *gv;
{
    register IO *io;

    if (!gv)
	goto phooey;

    io = GvIO(gv);
    if (!io || !io->ifp)
	goto phooey;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(io->ifp))
	(void)fseek (io->ifp, 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return ftell(io->ifp);

phooey:
    if (dowarn)
	warn("tell() on unopened file");
    errno = EBADF;
    return -1L;
}

bool
do_seek(gv, pos, whence)
GV *gv;
long pos;
int whence;
{
    register IO *io;

    if (!gv)
	goto nuts;

    io = GvIO(gv);
    if (!io || !io->ifp)
	goto nuts;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(io->ifp))
	(void)fseek (io->ifp, 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return fseek(io->ifp, pos, whence) >= 0;

nuts:
    if (dowarn)
	warn("seek() on unopened file");
    errno = EBADF;
    return FALSE;
}

I32
do_ctl(optype,gv,func,argstr)
I32 optype;
GV *gv;
I32 func;
SV *argstr;
{
    register IO *io;
    register char *s;
    I32 retval;

    if (!gv || !argstr || !(io = GvIO(gv)) || !io->ifp) {
	errno = EBADF;	/* well, sort of... */
	return -1;
    }

    if (SvPOK(argstr) || !SvNIOK(argstr)) {
	if (!SvPOK(argstr))
	    s = SvPVn(argstr);

#ifdef IOCPARM_MASK
#ifndef IOCPARM_LEN
#define IOCPARM_LEN(x)  (((x) >> 16) & IOCPARM_MASK)
#endif
#endif
#ifdef IOCPARM_LEN
	retval = IOCPARM_LEN(func);	/* on BSDish systes we're safe */
#else
	retval = 256;			/* otherwise guess at what's safe */
#endif
	if (SvCUR(argstr) < retval) {
	    Sv_Grow(argstr,retval+1);
	    SvCUR_set(argstr, retval);
	}

	s = SvPV(argstr);
	s[SvCUR(argstr)] = 17;	/* a little sanity check here */
    }
    else {
	retval = SvIVn(argstr);
#ifdef DOSISH
	s = (char*)(long)retval;		/* ouch */
#else
	s = (char*)retval;		/* ouch */
#endif
    }

#ifndef lint
    if (optype == OP_IOCTL)
	retval = ioctl(fileno(io->ifp), func, s);
    else
#ifdef DOSISH
	fatal("fcntl is not implemented");
#else
#ifdef HAS_FCNTL
	retval = fcntl(fileno(io->ifp), func, s);
#else
	fatal("fcntl is not implemented");
#endif
#endif
#else /* lint */
    retval = 0;
#endif /* lint */

    if (SvPOK(argstr)) {
	if (s[SvCUR(argstr)] != 17)
	    fatal("Return value overflowed string");
	s[SvCUR(argstr)] = 0;		/* put our null back */
    }
    return retval;
}

#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE) && defined(FFt_FREESP)
	/* code courtesy of William Kucharski */
#define HAS_CHSIZE

I32 chsize(fd, length)
I32 fd;			/* file descriptor */
off_t length;		/* length to set file to */
{
    extern long lseek();
    struct flock fl;
    struct stat filebuf;

    if (fstat(fd, &filebuf) < 0)
	return -1;

    if (filebuf.st_size < length) {

	/* extend file length */

	if ((lseek(fd, (length - 1), 0)) < 0)
	    return -1;

	/* write a "0" byte */

	if ((write(fd, "", 1)) != 1)
	    return -1;
    }
    else {
	/* truncate length */

	fl.l_whence = 0;
	fl.l_len = 0;
	fl.l_start = length;
	fl.l_type = FFt_WRLCK;    /* write lock on file space */

	/*
	* This relies on the UNDOCUMENTED FFt_FREESP argument to
	* fcntl(2), which truncates the file so that it ends at the
	* position indicated by fl.l_start.
	*
	* Will minor miracles never cease?
	*/

	if (fcntl(fd, FFt_FREESP, &fl) < 0)
	    return -1;

    }

    return 0;
}
#endif /* FFt_FREESP */

I32
looks_like_number(sv)
SV *sv;
{
    register char *s;
    register char *send;

    if (!SvPOK(sv))
	return TRUE;
    s = SvPV(sv); 
    send = s + SvCUR(sv);
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return FALSE;
    if (*s == '+' || *s == '-')
	s++;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == '.') 
	s++;
    else if (s == SvPV(sv))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == 'e' || *s == 'E') {
	s++;
	if (*s == '+' || *s == '-')
	    s++;
	while (isDIGIT(*s))
	    s++;
    }
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return TRUE;
    return FALSE;
}

bool
do_print(sv,fp)
register SV *sv;
FILE *fp;
{
    register char *tmps;
    SV* tmpstr;

    /* assuming fp is checked earlier */
    if (!sv)
	return TRUE;
    if (ofmt) {
	if (SvMAGICAL(sv))
	    mg_get(sv);
        if (SvIOK(sv) && SvIV(sv) != 0) {
	    fprintf(fp, ofmt, (double)SvIV(sv));
	    return !ferror(fp);
	}
	if (  (SvNOK(sv) && SvNV(sv) != 0.0)
	   || (looks_like_number(sv) && sv_2nv(sv) != 0.0) ) {
	    fprintf(fp, ofmt, SvNV(sv));
	    return !ferror(fp);
	}
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	return TRUE;
    case SVt_REF:
	fprintf(fp, "%s", sv_2pv(sv));
	return !ferror(fp);
    case SVt_IV:
	if (SvMAGICAL(sv))
	    mg_get(sv);
	fprintf(fp, "%d", SvIV(sv));
	return !ferror(fp);
    default:
	tmps = SvPVn(sv);
	break;
    }
    if (SvCUR(sv) && (fwrite(tmps,1,SvCUR(sv),fp) == 0 || ferror(fp)))
	return FALSE;
    return TRUE;
}

I32
my_stat(ARGS)
dARGS
{
    dSP;
    IO *io;

    if (op->op_flags & OPf_SPECIAL) {
	EXTEND(sp,1);
	io = GvIO(cGVOP->op_gv);
	if (io && io->ifp) {
	    statgv = cGVOP->op_gv;
	    sv_setpv(statname,"");
	    laststype = OP_STAT;
	    return (laststatval = fstat(fileno(io->ifp), &statcache));
	}
	else {
	    if (cGVOP->op_gv == defgv)
		return laststatval;
	    if (dowarn)
		warn("Stat on unopened file <%s>",
		  GvENAME(cGVOP->op_gv));
	    statgv = Nullgv;
	    sv_setpv(statname,"");
	    return (laststatval = -1);
	}
    }
    else {
	dPOPss;
	PUTBACK;
	statgv = Nullgv;
	sv_setpv(statname,SvPVn(sv));
	laststype = OP_STAT;
	laststatval = stat(SvPVn(sv),&statcache);
	if (laststatval < 0 && dowarn && index(SvPVn(sv), '\n'))
	    warn(warn_nl, "stat");
	return laststatval;
    }
}

I32
my_lstat(ARGS)
dARGS
{
    dSP;
    SV *sv;
    if (op->op_flags & OPf_SPECIAL) {
	EXTEND(sp,1);
	if (cGVOP->op_gv == defgv) {
	    if (laststype != OP_LSTAT)
		fatal("The stat preceding -l _ wasn't an lstat");
	    return laststatval;
	}
	fatal("You can't use -l on a filehandle");
    }

    laststype = OP_LSTAT;
    statgv = Nullgv;
    sv = POPs;
    PUTBACK;
    sv_setpv(statname,SvPVn(sv));
#ifdef HAS_LSTAT
    laststatval = lstat(SvPVn(sv),&statcache);
#else
    laststatval = stat(SvPVn(sv),&statcache);
#endif
    if (laststatval < 0 && dowarn && index(SvPVn(sv), '\n'))
	warn(warn_nl, "lstat");
    return laststatval;
}

bool
do_aexec(really,mark,sp)
SV *really;
register SV **mark;
register SV **sp;
{
    register char **a;
    char *tmps;

    if (sp > mark) {
	New(401,Argv, sp - mark + 1, char*);
	a = Argv;
	while (++mark <= sp) {
	    if (*mark)
		*a++ = SvPVnx(*mark);
	    else
		*a++ = "";
	}
	*a = Nullch;
	if (*Argv[0] != '/')	/* will execvp use PATH? */
	    TAINT_ENV();		/* testing IFS here is overkill, probably */
	if (really && *(tmps = SvPVn(really)))
	    execvp(tmps,Argv);
	else
	    execvp(Argv[0],Argv);
    }
    do_execfree();
    return FALSE;
}

void
do_execfree()
{
    if (Argv) {
	Safefree(Argv);
	Argv = Null(char **);
    }
    if (Cmd) {
	Safefree(Cmd);
	Cmd = Nullch;
    }
}

bool
do_exec(cmd)
char *cmd;
{
    register char **a;
    register char *s;
    char flags[10];

    /* save an extra exec if possible */

#ifdef CSH
    if (strnEQ(cmd,cshname,cshlen) && strnEQ(cmd+cshlen," -c",3)) {
	strcpy(flags,"-c");
	s = cmd+cshlen+3;
	if (*s == 'f') {
	    s++;
	    strcat(flags,"f");
	}
	if (*s == ' ')
	    s++;
	if (*s++ == '\'') {
	    char *ncmd = s;

	    while (*s)
		s++;
	    if (s[-1] == '\n')
		*--s = '\0';
	    if (s[-1] == '\'') {
		*--s = '\0';
		execl(cshname,"csh", flags,ncmd,(char*)0);
		*s = '\'';
		return FALSE;
	    }
	}
    }
#endif /* CSH */

    /* see if there are shell metacharacters in it */

    /*SUPPRESS 530*/
    for (s = cmd; *s && isALPHA(*s); s++) ;	/* catch VAR=val gizmo */
    if (*s == '=')
	goto doshell;
    for (s = cmd; *s; s++) {
	if (*s != ' ' && !isALPHA(*s) && index("$&*(){}[]'\";\\|?<>~`\n",*s)) {
	    if (*s == '\n' && !s[1]) {
		*s = '\0';
		break;
	    }
	  doshell:
	    execl("/bin/sh","sh","-c",cmd,(char*)0);
	    return FALSE;
	}
    }
    New(402,Argv, (s - cmd) / 2 + 2, char*);
    Cmd = nsavestr(cmd, s-cmd);
    a = Argv;
    for (s = Cmd; *s;) {
	while (*s && isSPACE(*s)) s++;
	if (*s)
	    *(a++) = s;
	while (*s && !isSPACE(*s)) s++;
	if (*s)
	    *s++ = '\0';
    }
    *a = Nullch;
    if (Argv[0]) {
	execvp(Argv[0],Argv);
	if (errno == ENOEXEC) {		/* for system V NIH syndrome */
	    do_execfree();
	    goto doshell;
	}
    }
    do_execfree();
    return FALSE;
}

I32
apply(type,mark,sp)
I32 type;
register SV **mark;
register SV **sp;
{
    register I32 val;
    register I32 val2;
    register I32 tot = 0;
    char *s;
    SV **oldmark = mark;

#ifdef TAINT
    while (++mark <= sp)
	TAINT_IF((*mark)->sv_tainted);
    mark = oldmark;
#endif
    switch (type) {
    case OP_CHMOD:
	TAINT_PROPER("chmod");
	if (++mark <= sp) {
	    tot = sp - mark;
	    val = SvIVnx(*mark);
	    while (++mark <= sp) {
		if (chmod(SvPVnx(*mark),val))
		    tot--;
	    }
	}
	break;
#ifdef HAS_CHOWN
    case OP_CHOWN:
	TAINT_PROPER("chown");
	if (sp - mark > 2) {
	    tot = sp - mark;
	    val = SvIVnx(*++mark);
	    val2 = SvIVnx(*++mark);
	    while (++mark <= sp) {
		if (chown(SvPVnx(*mark),val,val2))
		    tot--;
	    }
	}
	break;
#endif
#ifdef HAS_KILL
    case OP_KILL:
	TAINT_PROPER("kill");
	s = SvPVnx(*++mark);
	tot = sp - mark;
	if (isUPPER(*s)) {
	    if (*s == 'S' && s[1] == 'I' && s[2] == 'G')
		s += 3;
	    if (!(val = whichsig(s)))
		fatal("Unrecognized signal name \"%s\"",s);
	}
	else
	    val = SvIVnx(*mark);
	if (val < 0) {
	    val = -val;
	    while (++mark <= sp) {
		I32 proc = SvIVnx(*mark);
#ifdef HAS_KILLPG
		if (killpg(proc,val))	/* BSD */
#else
		if (kill(-proc,val))	/* SYSV */
#endif
		    tot--;
	    }
	}
	else {
	    while (++mark <= sp) {
		if (kill(SvIVnx(*mark),val))
		    tot--;
	    }
	}
	break;
#endif
    case OP_UNLINK:
	TAINT_PROPER("unlink");
	tot = sp - mark;
	while (++mark <= sp) {
	    s = SvPVnx(*mark);
	    if (euid || unsafe) {
		if (UNLINK(s))
		    tot--;
	    }
	    else {	/* don't let root wipe out directories without -U */
#ifdef HAS_LSTAT
		if (lstat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#else
		if (stat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#endif
		    tot--;
		else {
		    if (UNLINK(s))
			tot--;
		}
	    }
	}
	break;
    case OP_UTIME:
	TAINT_PROPER("utime");
	if (sp - mark > 2) {
#ifdef I_UTIME
	    struct utimbuf utbuf;
#else
	    struct {
		long    actime;
		long	modtime;
	    } utbuf;
#endif

	    Zero(&utbuf, sizeof utbuf, char);
	    utbuf.actime = SvIVnx(*++mark);    /* time accessed */
	    utbuf.modtime = SvIVnx(*++mark);    /* time modified */
	    tot = sp - mark;
	    while (++mark <= sp) {
		if (utime(SvPVnx(*mark),&utbuf))
		    tot--;
	    }
	}
	else
	    tot = 0;
	break;
    }
    return tot;
}

/* Do the permissions allow some operation?  Assumes statcache already set. */

I32
cando(bit, effective, statbufp)
I32 bit;
I32 effective;
register struct stat *statbufp;
{
#ifdef DOSISH
    /* [Comments and code from Len Reed]
     * MS-DOS "user" is similar to UNIX's "superuser," but can't write
     * to write-protected files.  The execute permission bit is set
     * by the Miscrosoft C library stat() function for the following:
     *		.exe files
     *		.com files
     *		.bat files
     *		directories
     * All files and directories are readable.
     * Directories and special files, e.g. "CON", cannot be
     * write-protected.
     * [Comment by Tom Dinger -- a directory can have the write-protect
     *		bit set in the file system, but DOS permits changes to
     *		the directory anyway.  In addition, all bets are off
     *		here for networked software, such as Novell and
     *		Sun's PC-NFS.]
     */

     /* Atari stat() does pretty much the same thing. we set x_bit_set_in_stat
      * too so it will actually look into the files for magic numbers
      */
     return (bit & statbufp->st_mode) ? TRUE : FALSE;

#else /* ! MSDOS */
    if ((effective ? euid : uid) == 0) {	/* root is special */
	if (bit == S_IXUSR) {
	    if (statbufp->st_mode & 0111 || S_ISDIR(statbufp->st_mode))
		return TRUE;
	}
	else
	    return TRUE;		/* root reads and writes anything */
	return FALSE;
    }
    if (statbufp->st_uid == (effective ? euid : uid) ) {
	if (statbufp->st_mode & bit)
	    return TRUE;	/* ok as "user" */
    }
    else if (ingroup((I32)statbufp->st_gid,effective)) {
	if (statbufp->st_mode & bit >> 3)
	    return TRUE;	/* ok as "group" */
    }
    else if (statbufp->st_mode & bit >> 6)
	return TRUE;	/* ok as "other" */
    return FALSE;
#endif /* ! MSDOS */
}

I32
ingroup(testgid,effective)
I32 testgid;
I32 effective;
{
    if (testgid == (effective ? egid : gid))
	return TRUE;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
    {
	GROUPSTYPE gary[NGROUPS];
	I32 anum;

	anum = getgroups(NGROUPS,gary);
	while (--anum >= 0)
	    if (gary[anum] == testgid)
		return TRUE;
    }
#endif
    return FALSE;
}

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)

I32
do_ipcget(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
    key_t key;
    I32 n, flags;

    key = (key_t)SvNVnx(*++mark);
    n = (optype == OP_MSGGET) ? 0 : SvIVnx(*++mark);
    flags = SvIVnx(*++mark);
    errno = 0;
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGGET:
	return msgget(key, flags);
#endif
#ifdef HAS_SEM
    case OP_SEMGET:
	return semget(key, n, flags);
#endif
#ifdef HAS_SHM
    case OP_SHMGET:
	return shmget(key, n, flags);
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	fatal("%s not implemented", op_name[optype]);
#endif
    }
    return -1;			/* should never happen */
}

I32
do_ipcctl(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
    SV *astr;
    char *a;
    I32 id, n, cmd, infosize, getinfo, ret;

    id = SvIVnx(*++mark);
    n = (optype == OP_SEMCTL) ? SvIVnx(*++mark) : 0;
    cmd = SvIVnx(*++mark);
    astr = *++mark;
    infosize = 0;
    getinfo = (cmd == IPC_STAT);

    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct msqid_ds);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct shmid_ds);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct semid_ds);
	else if (cmd == GETALL || cmd == SETALL)
	{
	    struct semid_ds semds;
	    if (semctl(id, 0, IPC_STAT, &semds) == -1)
		return -1;
	    getinfo = (cmd == GETALL);
	    infosize = semds.sem_nsems * sizeof(short);
		/* "short" is technically wrong but much more portable
		   than guessing about u_?short(_t)? */
	}
	break;
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	fatal("%s not implemented", op_name[optype]);
#endif
    }

    if (infosize)
    {
	if (getinfo)
	{
	    SvGROW(astr, infosize+1);
	    a = SvPVn(astr);
	}
	else
	{
	    a = SvPVn(astr);
	    if (SvCUR(astr) != infosize)
	    {
		errno = EINVAL;
		return -1;
	    }
	}
    }
    else
    {
	I32 i = SvIVn(astr);
	a = (char *)i;		/* ouch */
    }
    errno = 0;
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	ret = msgctl(id, cmd, (struct msqid_ds *)a);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
	ret = semctl(id, n, cmd, (struct semid_ds *)a);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	ret = shmctl(id, cmd, (struct shmid_ds *)a);
	break;
#endif
    }
    if (getinfo && ret >= 0) {
	SvCUR_set(astr, infosize);
	*SvEND(astr) = '\0';
    }
    return ret;
}

I32
do_msgsnd(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_MSG
    SV *mstr;
    char *mbuf;
    I32 id, msize, flags;

    id = SvIVnx(*++mark);
    mstr = *++mark;
    flags = SvIVnx(*++mark);
    mbuf = SvPVn(mstr);
    if ((msize = SvCUR(mstr) - sizeof(long)) < 0) {
	errno = EINVAL;
	return -1;
    }
    errno = 0;
    return msgsnd(id, (struct msgbuf *)mbuf, msize, flags);
#else
    fatal("msgsnd not implemented");
#endif
}

I32
do_msgrcv(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_MSG
    SV *mstr;
    char *mbuf;
    long mtype;
    I32 id, msize, flags, ret;

    id = SvIVnx(*++mark);
    mstr = *++mark;
    msize = SvIVnx(*++mark);
    mtype = (long)SvIVnx(*++mark);
    flags = SvIVnx(*++mark);
    mbuf = SvPVn(mstr);
    if (SvCUR(mstr) < sizeof(long)+msize+1) {
	SvGROW(mstr, sizeof(long)+msize+1);
	mbuf = SvPVn(mstr);
    }
    errno = 0;
    ret = msgrcv(id, (struct msgbuf *)mbuf, msize, mtype, flags);
    if (ret >= 0) {
	SvCUR_set(mstr, sizeof(long)+ret);
	*SvEND(mstr) = '\0';
    }
    return ret;
#else
    fatal("msgrcv not implemented");
#endif
}

I32
do_semop(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_SEM
    SV *opstr;
    char *opbuf;
    I32 id, opsize;

    id = SvIVnx(*++mark);
    opstr = *++mark;
    opbuf = SvPVn(opstr);
    opsize = SvCUR(opstr);
    if (opsize < sizeof(struct sembuf)
	|| (opsize % sizeof(struct sembuf)) != 0) {
	errno = EINVAL;
	return -1;
    }
    errno = 0;
    return semop(id, (struct sembuf *)opbuf, opsize/sizeof(struct sembuf));
#else
    fatal("semop not implemented");
#endif
}

I32
do_shmio(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
#ifdef HAS_SHM
    SV *mstr;
    char *mbuf, *shm;
    I32 id, mpos, msize;
    struct shmid_ds shmds;
#ifndef VOIDSHMAT
    extern char *shmat();
#endif

    id = SvIVnx(*++mark);
    mstr = *++mark;
    mpos = SvIVnx(*++mark);
    msize = SvIVnx(*++mark);
    errno = 0;
    if (shmctl(id, IPC_STAT, &shmds) == -1)
	return -1;
    if (mpos < 0 || msize < 0 || mpos + msize > shmds.shm_segsz) {
	errno = EFAULT;		/* can't do as caller requested */
	return -1;
    }
    shm = (char*)shmat(id, (char*)NULL, (optype == OP_SHMREAD) ? SHM_RDONLY : 0);
    if (shm == (char *)-1)	/* I hate System V IPC, I really do */
	return -1;
    mbuf = SvPVn(mstr);
    if (optype == OP_SHMREAD) {
	if (SvCUR(mstr) < msize) {
	    SvGROW(mstr, msize+1);
	    mbuf = SvPVn(mstr);
	}
	Copy(shm + mpos, mbuf, msize, char);
	SvCUR_set(mstr, msize);
	*SvEND(mstr) = '\0';
    }
    else {
	I32 n;

	if ((n = SvCUR(mstr)) > msize)
	    n = msize;
	Copy(mbuf, shm + mpos, n, char);
	if (n < msize)
	    memzero(shm + mpos + n, msize - n);
    }
    return shmdt(shm);
#else
    fatal("shm I/O not implemented");
#endif
}

#endif /* SYSV IPC */
