#include <u.h>
#include <libc.h>
#include <bio.h>
#include "stl.h"

static int bunpack(Biobuf*, char*, ...);

static int
Bgets(Biobuf *b, u16int *s)
{
	uchar buf[2];

	if(Bread(b, buf, 2) != 2){
		werrstr("could not get 2 bytes");
		return -1;
	}
	*s = buf[0] | buf[1] << 8;
	return 0;
}

static int
Bgetl(Biobuf *b, u32int *l)
{
	uchar buf[4];

	if(Bread(b, buf, 4) != 4){
		werrstr("could not get 4 bytes");
		return -1;
	}
	*l = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
	return 0;
}

static int
Bgetf(Biobuf *b, float *f)
{
	u32int l;

	if(Bgetl(b, &l) < 0){
		werrstr("Bgetl: %r");
		return -1;
	}
	*f = *(float*)&l;
	return 0;
}

static int
vbunpack(Biobuf *b, char *fmt, va_list a)
{
	u16int s;
	u32int l;
	float f, *v;
	void *p;

	for(;;){
		switch(*fmt++){
		case '\0':
			return 0;
		case 's':
			if(Bgets(b, &s) < 0)
				goto error;
			*va_arg(a, ushort*) = s;
			break;
		case 'l':
			if(Bgetl(b, &l) < 0)
				goto error;
			*va_arg(a, ulong*) = l;
			break;
		case 'f':
			if(Bgetf(b, &f) < 0)
				goto error;
			*va_arg(a, float*) = f;
			break;
		case 'v':
			v = va_arg(a, float*);
			if(bunpack(b, "fff", v+0, v+1, v+2) < 0)
				goto error;
			break;
		case '[':
			p = va_arg(a, void*);
			s = va_arg(a, ushort);
			if(Bread(b, p, s) != s){
				werrstr("Bread: could not read %ud bytes", s);
				goto error;
			}
			break;
		}
	}
error:
	return -1;
}

static int
bunpack(Biobuf *b, char *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vbunpack(b, fmt, a);
	va_end(a);

	return n;
}

Stl *
readstl(int fd)
{
	Biobuf *bin;
	Stltri *tri;
	Stl *stl;
	u32int i, ntris;

	bin = Bfdopen(fd, OREAD);
	if(bin == nil){
		werrstr("Bfdopen: %r");
		return nil;
	}

	stl = mallocz(sizeof(Stl), 1);
	if(stl == nil){
		werrstr("mallocz0: %r");
		goto error;
	}
	if(bunpack(bin, "[l", stl->hdr, sizeof(stl->hdr), &ntris) < 0){
		werrstr("hdr bunpack: %r");
		goto error;
	}

	for(i = 0; i < ntris; i++){
		tri = mallocz(sizeof(Stltri), 1);
		if(tri == nil){
			werrstr("mallocz1: %r");
			goto error;
		}
		if(bunpack(bin, "vvvvs", tri->n, tri->p0, tri->p1, tri->p2, &tri->attrlen) < 0){
			free(tri);
			werrstr("tri bunpack0: %r");
			goto error;
		}
		tri = realloc(tri, sizeof(Stltri) + tri->attrlen);
		if(tri == nil){
			werrstr("realloc0: %r");
			goto error;
		}
		if(bunpack(bin, "[", tri->attrs, tri->attrlen) < 0){
			free(tri);
			werrstr("tri bunpack1: %r");
			goto error;
		}

		stl->tris = realloc(stl->tris, (stl->ntris+1)*sizeof(*stl->tris));
		if(stl->tris == nil){
			free(tri);
			werrstr("realloc1: %r");
			goto error;
		}
		stl->tris[stl->ntris++] = tri;
	}

	Bterm(bin);
	return stl;
error:
	Bterm(bin);
	freestl(stl);
	return nil;
}

void
freestl(Stl *stl)
{
	int i;

	if(stl == nil)
		return;

	for(i = 0; i < stl->ntris; i++)
		free(stl->tris[i]);
	free(stl->tris);
	free(stl);
}
