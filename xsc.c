/*
 * xsc -- copy standard input into X11 cliboard selection.
 *
 * Using -d option, a number of seconds, after which the just-copied selection
 * will be wiped. If within -d seconds just-copied selection was overwritten
 * (by any means: either by manually setting the selection or by using another
 * xsc instance), then no further actions taken. If during this time the
 * selection was not overwritten, then data that used to be the selection before
 * invoking xsc, is restored.
 */

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>

#define BUF_SZ 4096

static Atom CLIPBOARD;
static Atom STR;
static Atom XSTR;
static Atom UTF8;
static Atom TXTATM;
static Atom TARGETS;
static Atom PREV_SEL;

static unsigned char* data = NULL; /* The data to be copied. */
static int inlen = 0;
static unsigned char* prev_sel = NULL;
static unsigned long prev_sel_len = 0;
static int do_dur;
static long dur;
static int restore_prev_sel;
static Display* dpl;
static Window w;


void
usage(int err)
{
	fprintf(stderr, "%s\n%s\n",
	    "usage: xsc [-d duration]",
	    "       xsc -h");
	if (err) {
		exit(2);
	}
	exit(EXIT_SUCCESS);
}

/*
 * Obtain atoms.
 */
void
obtatms()
{
	CLIPBOARD = XInternAtom(dpl, "CLIPBOARD", False);
	STR = XInternAtom(dpl, "STRING", False);
	XSTR = XInternAtom(dpl, "XA_STRING", False);
	UTF8 = XInternAtom(dpl, "UTF8_STRING", False);
	TXTATM = XInternAtom(dpl, "TEXT", False);
	TARGETS = XInternAtom(dpl, "TARGETS", False);
	PREV_SEL = XInternAtom(dpl, "XSC_PREV_SEL", False);
}

/*
 * Selection notification.
 */
void
selntf(XSelectionRequestEvent* srev, Atom prop)
{
	XSelectionEvent snev;
	snev.type = SelectionNotify;
	snev.serial = srev->serial;
	snev.send_event = srev->send_event;
	snev.display = srev->display;
	snev.requestor = srev->requestor;
	snev.selection = srev->selection;
	snev.target = srev->target;
	snev.property = prop;
	snev.time=srev->time;
	XSendEvent(srev->display, srev->requestor, False, NoEventMask,
	    (XEvent*)&snev);
}

/*
 * Selection denial.
 */
void
seldeny(XSelectionRequestEvent* srev)
{
	selntf(srev, None);
}

void
wdie()
{
	XCloseDisplay(dpl);
	free(data);
	exit(EXIT_SUCCESS);
}

unsigned char*
get_current_sel(unsigned long* len)
{
	int got, di;
	unsigned long dul;
	XEvent ev;
	XSelectionEvent* snev;
	Atom tp, da;
	unsigned char* sel;
	
	XConvertSelection(dpl, CLIPBOARD, UTF8, PREV_SEL, w, CurrentTime);
	got = 0;
	while (!got) {
		XNextEvent(dpl, &ev);
		switch (ev.type) {
		case SelectionNotify:
			got = 1;
			snev = &(ev.xselection);
			if (snev->property == None)
				break;
			else {
				XGetWindowProperty(dpl, w, PREV_SEL, 0, 0, False,
				    AnyPropertyType, &tp, &di, &dul, len, &sel);
				XGetWindowProperty(dpl, w, PREV_SEL, 0, *len, False,
				    AnyPropertyType, &da, &di, &dul, &dul, &sel);
			}
			break;
		}
	}
	return (sel);
}

void
maybe_restore_prev_sel()
{	
	if (!restore_prev_sel)
		return;
	free(data);
	data = prev_sel;
	inlen = prev_sel_len;
}

void
wrun(void)
{
	XEvent ev;
	
	if ((dpl = XOpenDisplay(NULL)) == NULL)
		err(1, "XOpenDisplay()");
	
	obtatms();
	w = XCreateSimpleWindow(dpl, DefaultRootWindow(dpl), 0, 0, 1, 1, 0, 0,
	    0);
	
	if (do_dur) {
		prev_sel = get_current_sel(&prev_sel_len);
		restore_prev_sel = 1;
		signal(SIGALRM, &maybe_restore_prev_sel);
		alarm(dur);
	}
	
	while (XGetSelectionOwner(dpl, CLIPBOARD) != w)
		XSetSelectionOwner(dpl, CLIPBOARD, w, CurrentTime);
	
	for (;;) {
		XNextEvent(dpl, &ev);
		switch(ev.type) {
		case SelectionRequest: {
			XSelectionRequestEvent* srev = &(ev.xselectionrequest);
			if (srev->property == None) {
				seldeny(srev);
				break;
			}
			/* Reply available selection targets. */
			if (srev->target == TARGETS) {
				Atom trgts[] = { UTF8, XSTR, STR, TXTATM };
				
				XChangeProperty(srev->display, srev->requestor,
				    srev->property, srev->target, 32,
				    PropModeReplace, (unsigned char*)trgts,
				    (int)(sizeof(trgts)/sizeof(Atom)));
				selntf(srev, srev->property);
				break;
			}
			/*
			 * Actual selection reply.
			 */
			else if ((srev->target == STR) || (srev->target ==
			    XSTR) || (srev->target == UTF8) ||
			    (srev->target == TXTATM)) {
				XChangeProperty(srev->display, srev->requestor,
				    srev->property, srev->target, 8,
				    PropModeReplace, data, inlen);
				selntf(srev, srev->property);
				break;
			}
			else {
				seldeny(srev);
				break;
			}
		}
		case SelectionClear:
			restore_prev_sel = 0;
			wdie();
		}
	}
}

int
main(int argc, char** argv)
{
	char buf[BUF_SZ];
	char opt;
	char* dur_p;
	char* in;
	ssize_t rdr;
	
	while ((opt = getopt(argc, argv, "d:h")) != -1) {
	switch (opt) {
	case 'd':
		do_dur = 1;
		dur = strtol(optarg, &dur_p, 10);
		if (dur_p == optarg)
			errx(1, "Wrong duration: %s is not a number", optarg);
		break;
	case 'h':
		usage(0);
		break;
	default:
		usage(1);
		break;
	}
	}
	
	in = NULL;
	/* Fill the input buffer. */
	while((rdr = read(STDIN_FILENO, &buf, BUF_SZ)) > 0) {
		buf[rdr] = 0;
		inlen += rdr;
		if ((in = realloc(in, inlen)) == NULL)
			err(1, "realloc()");
		strcat(in, buf);
	}
	data = (unsigned char*)(in);
	
	switch (fork()) {
	case -1:
		free(data);
		err(1, "fork()");
	case 0: /* Child. */
		wrun();
		/* NOTREACHED. */
	default:
		free(data);
		break;
	}
	
	return EXIT_SUCCESS;
}
