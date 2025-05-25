#include <u.h>
#include <libc.h>
#include "../stl.h"

static char fd0[] = "/fd/0";

void
usage(void)
{
	fprint(2, "usage: %s [-o [tb]] [file]\n", argv0);
	exits("usage");
}

static void
printfv(float *f)
{
	fprint(2, "%f\t%f\t%f", f[0], f[1], f[2]);
}

static void
printstl(Stl *stl)
{
	int i;

	fprint(2, "header: %.*s\n", sizeof(stl->hdr), (char*)stl->hdr);
	fprint(2, "ntris: %ud\n", stl->ntris);
	fprint(2, "tris:\n");
	for(i = 0; i < stl->ntris; i++){
		fprint(2, "\t%08d\n\t\tn\t", i);
		printfv(stl->tris[i]->n);
		fprint(2, "\n\t\tp0\t");
		printfv(stl->tris[i]->v[0]);
		fprint(2, "\n\t\tp1\t");
		printfv(stl->tris[i]->v[1]);
		fprint(2, "\n\t\tp2\t");
		printfv(stl->tris[i]->v[2]);
		fprint(2, "\n\t\tattrlen\t%d", stl->tris[i]->attrlen);
		fprint(2, "\n\t\tattrs: %.*s\n", stl->tris[i]->attrlen, (char*)stl->tris[i]->attrs);
	}
}

void
main(int argc, char *argv[])
{
	Stl *stl;
	char *f, *fmt;
	int fd, ofmt;

	f = fd0;
	ofmt = -1;
	ARGBEGIN{
	case 'o':
		fmt = EARGF(usage());
		switch(*fmt){
		case 't': ofmt = STLTEXT; break;
		case 'b': ofmt = STLBINARY; break;
		default: sysfatal("unknown format '%d'", *fmt);
		}
		break;
	default: usage();
	}ARGEND;
	if(argc > 1)
		usage();
	if(argc == 1)
		f = argv[0];

	fd = open(f, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	stl = readstl(fd);
	if(stl == nil)
		sysfatal("readstl: %r");
	close(fd);

	printstl(stl);
	if(ofmt >= 0)
		writestl(1, stl, ofmt);
	freestl(stl);
	exits(nil);
}
