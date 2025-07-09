/*
 * xsc -- copy standard input into X11 cliboard selection.
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>

#define BUF_SZ 4096

static Atom CLIPATM;
static Atom STRATM;
static Atom XSTRATM;
static Atom UTF8STRATM;
static Atom TXTATM;
static Atom TRGTSATM;

static unsigned char* data = NULL; /* The data to be copied. */
static int inlen = 0; /* Length of `data'. */

/*
 * Obtain atoms.
 */
void
obtatms(Display* dpl)
{
	CLIPATM = XInternAtom(dpl, "CLIPBOARD", False);
	STRATM = XInternAtom(dpl, "STRING", False);
	XSTRATM = XInternAtom(dpl, "XA_STRING", False);
	UTF8STRATM = XInternAtom(dpl, "UTF8_STRING", False);
	TXTATM = XInternAtom(dpl, "TEXT", False);
	TRGTSATM = XInternAtom(dpl, "TARGETS", False);	
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
wdie(Display* dpl)
{
	XCloseDisplay(dpl);
	free(data);
	exit(0);
}

void
wrun(void)
{
	Display* dpl;
	Window w;
	XEvent ev;
	
	if ((dpl = XOpenDisplay(NULL)) == NULL)
		err(1, "XOpenDisplay()");
	
	obtatms(dpl);
	w = XCreateSimpleWindow(dpl, DefaultRootWindow(dpl), 0, 0, 1, 1,
	    0, 0, 0);
	
	while(XGetSelectionOwner(dpl, CLIPATM) != w)
		XSetSelectionOwner(dpl, CLIPATM, w, CurrentTime);
	
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
			if (srev->target == TRGTSATM) {
				Atom trgts[] = {UTF8STRATM, XSTRATM, STRATM,
				    TXTATM};
				
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
			else if ((srev->target == STRATM) || (srev->target ==
			    XSTRATM) || (srev->target == UTF8STRATM) ||
			    (srev->target == TXTATM)) {
				XChangeProperty(srev->display, srev->requestor,
				    srev->property, srev->target,8,
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
			wdie(dpl);
		}
	}
}

int
main()
{
	char buf[BUF_SZ];
	char* in;
	ssize_t rdr;
	
	in = NULL;
	
	/* Fill the input buffer. */
	while((rdr = read(0, &buf, BUF_SZ)) > 0) {
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
	}
	
	return 0;
}
