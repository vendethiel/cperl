/* $RCSfile: dump.c,v $$Revision: 4.1 $$Date: 92/08/07 17:20:03 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	dump.c,v $
 * Revision 4.1  92/08/07  17:20:03  lwall
 * Stage 6 Snapshot
 * 
 * Revision 4.0.1.2  92/06/08  13:14:22  lwall
 * patch20: removed implicit int declarations on funcions
 * patch20: fixed confusion between a *var's real name and its effective name
 * 
 * Revision 4.0.1.1  91/06/07  10:58:44  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0  91/03/20  01:08:25  lwall
 * 4.0 baseline.
 * 
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef DEBUGGING

static void dump();

void
dump_sequence(op)
register OP *op;
{
    extern I32 op_seq;

    for (; op; op = op->op_next) {
	if (op->op_seq)
	    return;
	op->op_seq = ++op_seq;
    }
}

void
dump_all()
{
    register I32 i;
    register GV *gv;
    register HE *entry;
    SV *sv = sv_mortalcopy(&sv_undef);

    setlinebuf(stderr);
    dump_sequence(main_start);
    dump_op(main_root);
    for (i = 0; i <= 127; i++) {
	for (entry = HvARRAY(defstash)[i]; entry; entry = entry->hent_next) {
	    gv = (GV*)entry->hent_val;
	    if (GvCV(gv)) {
		gv_fullname(sv,gv);
		dump("\nSUB %s = ", SvPV(sv));
		if (CvUSERSUB(GvCV(gv)))
		    dump("(usersub 0x%x %d)\n",
			(long)CvUSERSUB(GvCV(gv)),
			CvUSERINDEX(GvCV(gv)));
		else {
		    dump_sequence(CvSTART(GvCV(gv)));
		    dump_op(CvROOT(GvCV(gv)));
		}
	    }
	}
    }
}

void
dump_eval()
{
    register I32 i;
    register GV *gv;
    register HE *entry;

    dump_sequence(eval_start);
    dump_op(eval_root);
}

void
dump_op(op)
register OP *op;
{
    SV *tmpsv;

    if (!op->op_seq)
	dump_sequence(op);
    dump("{\n");
    fprintf(stderr, "%-4d", op->op_seq);
    dump("TYPE = %s  ===> ", op_name[op->op_type]);
    if (op->op_next)
	fprintf(stderr, "%d\n", op->op_next->op_seq);
    else
	fprintf(stderr, "DONE\n");
    dumplvl++;
    if (op->op_targ)
	dump("TARG = %d\n", op->op_targ);
#ifdef NOTDEF
    dump("ADDR = 0x%lx => 0x%lx\n",op, op->op_next);
#endif
    if (op->op_flags) {
	*buf = '\0';
	if (op->op_flags & OPf_KNOW) {
	    if (op->op_flags & OPf_LIST)
		(void)strcat(buf,"LIST,");
	    else
		(void)strcat(buf,"SCALAR,");
	}
	else
	    (void)strcat(buf,"UNKNOWN,");
	if (op->op_flags & OPf_KIDS)
	    (void)strcat(buf,"KIDS,");
	if (op->op_flags & OPf_PARENS)
	    (void)strcat(buf,"PARENS,");
	if (op->op_flags & OPf_STACKED)
	    (void)strcat(buf,"STACKED,");
	if (op->op_flags & OPf_LVAL)
	    (void)strcat(buf,"LVAL,");
	if (op->op_flags & OPf_LOCAL)
	    (void)strcat(buf,"LOCAL,");
	if (op->op_flags & OPf_SPECIAL)
	    (void)strcat(buf,"SPECIAL,");
	if (*buf)
	    buf[strlen(buf)-1] = '\0';
	dump("FLAGS = (%s)\n",buf);
    }
    if (op->op_private) {
	*buf = '\0';
	if (op->op_type == OP_AASSIGN) {
	    if (op->op_private & OPpASSIGN_COMMON)
		(void)strcat(buf,"COMMON,");
	}
	else if (op->op_type == OP_TRANS) {
	    if (op->op_private & OPpTRANS_SQUASH)
		(void)strcat(buf,"SQUASH,");
	    if (op->op_private & OPpTRANS_DELETE)
		(void)strcat(buf,"DELETE,");
	    if (op->op_private & OPpTRANS_COMPLEMENT)
		(void)strcat(buf,"COMPLEMENT,");
	}
	else if (op->op_type == OP_REPEAT) {
	    if (op->op_private & OPpREPEAT_DOLIST)
		(void)strcat(buf,"DOLIST,");
	}
	else if (op->op_type == OP_ENTERSUBR) {
	    if (op->op_private & OPpSUBR_DB)
		(void)strcat(buf,"DB,");
	}
	else if (op->op_type == OP_CONST) {
	    if (op->op_private & OPpCONST_BARE)
		(void)strcat(buf,"BARE,");
	}
	else if (op->op_type == OP_FLIP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		(void)strcat(buf,"LINENUM,");
	}
	else if (op->op_type == OP_FLOP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		(void)strcat(buf,"LINENUM,");
	}
	if (*buf) {
	    buf[strlen(buf)-1] = '\0';
	    dump("PRIVATE = (%s)\n",buf);
	}
    }

    switch (op->op_type) {
    case OP_GV:
	if (cGVOP->op_gv) {
	    tmpsv = NEWSV(0,0);
	    gv_fullname(tmpsv,cGVOP->op_gv);
	    dump("GV = %s\n", SvPVn(tmpsv));
	    sv_free(tmpsv);
	}
	else
	    dump("GV = NULL\n");
	break;
    case OP_CONST:
	dump("SV = %s\n", SvPEEK(cSVOP->op_sv));
	break;
    case OP_CURCOP:
	if (cCOP->cop_line)
	    dump("LINE = %d\n",cCOP->cop_line);
	if (cCOP->cop_label)
	    dump("LABEL = \"%s\"\n",cCOP->cop_label);
	break;
    case OP_ENTERLOOP:
	dump("REDO ===> ");
	if (cLOOP->op_redoop) {
	    dump_sequence(cLOOP->op_redoop);
	    fprintf(stderr, "%d\n", cLOOP->op_redoop->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	dump("NEXT ===> ");
	if (cLOOP->op_nextop) {
	    dump_sequence(cLOOP->op_nextop);
	    fprintf(stderr, "%d\n", cLOOP->op_nextop->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	dump("LAST ===> ");
	if (cLOOP->op_lastop) {
	    dump_sequence(cLOOP->op_lastop);
	    fprintf(stderr, "%d\n", cLOOP->op_lastop->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	break;
    case OP_COND_EXPR:
	dump("TRUE ===> ");
	if (cCONDOP->op_true) {
	    dump_sequence(cCONDOP->op_true);
	    fprintf(stderr, "%d\n", cCONDOP->op_true->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	dump("FALSE ===> ");
	if (cCONDOP->op_false) {
	    dump_sequence(cCONDOP->op_false);
	    fprintf(stderr, "%d\n", cCONDOP->op_false->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	break;
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
    case OP_METHOD:
	dump("OTHER ===> ");
	if (cLOGOP->op_other) {
	    dump_sequence(cLOGOP->op_other);
	    fprintf(stderr, "%d\n", cLOGOP->op_other->op_seq);
	}
	else
	    fprintf(stderr, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_SUBST:
	dump_pm(op);
	break;
    }
    if (op->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling)
	    dump_op(kid);
    }
    dumplvl--;
    dump("}\n");
}

void
dump_gv(gv)
register GV *gv;
{
    SV *sv;

    if (!gv) {
	fprintf(stderr,"{}\n");
	return;
    }
    sv = sv_mortalcopy(&sv_undef);
    dumplvl++;
    fprintf(stderr,"{\n");
    gv_fullname(sv,gv);
    dump("GV_NAME = %s", SvPV(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname(sv,GvEGV(gv));
	dump("-> %s", SvPV(sv));
    }
    dump("\n");
    dumplvl--;
    dump("}\n");
}

void
dump_pm(pm)
register PMOP *pm;
{
    char ch;

    if (!pm) {
	dump("{}\n");
	return;
    }
    dump("{\n");
    dumplvl++;
    if (pm->op_pmflags & PMf_ONCE)
	ch = '?';
    else
	ch = '/';
    if (pm->op_pmregexp)
	dump("PMf_PRE %c%s%c\n",ch,pm->op_pmregexp->precomp,ch);
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplroot) {
	dump("PMf_REPL = ");
	dump_op(pm->op_pmreplroot);
    }
    if (pm->op_pmshort) {
	dump("PMf_SHORT = %s\n",SvPEEK(pm->op_pmshort));
    }
    if (pm->op_pmflags) {
	*buf = '\0';
	if (pm->op_pmflags & PMf_USED)
	    (void)strcat(buf,"USED,");
	if (pm->op_pmflags & PMf_ONCE)
	    (void)strcat(buf,"ONCE,");
	if (pm->op_pmflags & PMf_SCANFIRST)
	    (void)strcat(buf,"SCANFIRST,");
	if (pm->op_pmflags & PMf_ALL)
	    (void)strcat(buf,"ALL,");
	if (pm->op_pmflags & PMf_SKIPWHITE)
	    (void)strcat(buf,"SKIPWHITE,");
	if (pm->op_pmflags & PMf_FOLD)
	    (void)strcat(buf,"FOLD,");
	if (pm->op_pmflags & PMf_CONST)
	    (void)strcat(buf,"CONST,");
	if (pm->op_pmflags & PMf_KEEP)
	    (void)strcat(buf,"KEEP,");
	if (pm->op_pmflags & PMf_GLOBAL)
	    (void)strcat(buf,"GLOBAL,");
	if (pm->op_pmflags & PMf_RUNTIME)
	    (void)strcat(buf,"RUNTIME,");
	if (pm->op_pmflags & PMf_EVAL)
	    (void)strcat(buf,"EVAL,");
	if (*buf)
	    buf[strlen(buf)-1] = '\0';
	dump("PMFLAGS = (%s)\n",buf);
    }

    dumplvl--;
    dump("}\n");
}

/* VARARGS1 */
static void dump(arg1,arg2,arg3,arg4,arg5)
char *arg1;
long arg2, arg3, arg4, arg5;
{
    I32 i;

    for (i = dumplvl*4; i; i--)
	(void)putc(' ',stderr);
    fprintf(stderr,arg1, arg2, arg3, arg4, arg5);
}
#endif
