#include <u.h>
#include <libc.h>
#include <bio.h>
#include "stl.h"

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
Bgetfv(Biobuf *b, float *f)
{
	int i;

	for(i = 0; i < 3; i++)
		if(Bgetf(b, &f[i]) < 0){
			werrstr("Bgetf: %r");
			return -1;
		}
	return 0;
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
		werrstr("mallocz: %r");
		goto error;
	}
	if(Bread(bin, stl->hdr, sizeof(stl->hdr)) != sizeof(stl->hdr)){
		werrstr("Bread: %r");
		goto error;
	}
	if(Bgetl(bin, &ntris) < 0){
		werrstr("Bgetl: %r");
		goto error;
	}

	for(i = 0; i < ntris; i++){
		tri = mallocz(sizeof(Stltri), 1);
		if(tri == nil){
			free(tri);
			werrstr("mallocz: %r");
			goto error;
		}
		if(Bgetfv(bin, tri->n) < 0){
			free(tri);
			werrstr("Bgetfv: %r");
			goto error;
		}
		if(Bgetfv(bin, tri->p0) < 0){
			free(tri);
			werrstr("Bgetfv: %r");
			goto error;
		}
		if(Bgetfv(bin, tri->p1) < 0){
			free(tri);
			werrstr("Bgetfv: %r");
			goto error;
		}
		if(Bgetfv(bin, tri->p2) < 0){
			free(tri);
			werrstr("Bgetfv: %r");
			goto error;
		}

		if(Bgets(bin, &tri->attrlen) < 0){
			free(tri);
			werrstr("Bgets: %r");
			goto error;
		}
		tri = realloc(tri, sizeof(Stltri) + tri->attrlen);
		if(tri == nil){
			werrstr("realloc: %r");
			goto error;
		}
		if(Bread(bin, tri->attrs, tri->attrlen) != tri->attrlen){
			free(tri);
			werrstr("Bread: %r");
			goto error;
		}

		stl->tris = realloc(stl->tris, (stl->ntris+1)*sizeof(*stl->tris));
		if(stl->tris == nil){
			werrstr("realloc: %r");
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
