#include <u.h>
#include <libc.h>
#include "../stl.h"

static char fd0[] = "/fd/0";

void
usage(void)
{
	fprint(2, "usage: %s [file]\n", argv0);
	exits("usage");
}

static void
printfv(float *f)
{
	print("%f\t%f\t%f", f[0], f[1], f[2]);
}

static void
printstl(Stl *stl)
{
	int i;

	print("header: %.*s\n", sizeof(stl->hdr), (char*)stl->hdr);
	print("ntris: %ud\n", stl->ntris);
	print("tris:\n");
	for(i = 0; i < stl->ntris; i++){
		print("\t%08d\n\t\tn\t", i);
		printfv(stl->tris[i]->n);
		print("\n\t\tp0\t");
		printfv(stl->tris[i]->v[0]);
		print("\n\t\tp1\t");
		printfv(stl->tris[i]->v[1]);
		print("\n\t\tp2\t");
		printfv(stl->tris[i]->v[2]);
		print("\n\t\tattrlen\t%d\n", stl->tris[i]->attrlen);
		print("\n\t\tattrs: %.*s\n", stl->tris[i]->attrlen, (char*)stl->tris[i]->attrs);
	}
}

void
main(int argc, char *argv[])
{
	Stl *stl;
	char *f;
	int fd;

	f = fd0;
	ARGBEGIN{
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
	freestl(stl);
	exits(nil);
}
