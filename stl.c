#include <u.h>
#include <libc.h>
#include <bio.h>
#include "stl.h"

#define min(a,b) ((a)<(b)?(a):(b))

typedef struct Line Line;
struct Line
{
	char *file;
	ulong line;
};

static void
error(Line *l, char *fmt, ...)
{
	va_list va;
	char buf[ERRMAX], *bp;

	va_start(va, fmt);
	bp = seprint(buf, buf + sizeof(buf), "line %lud: ", l->line);
	vseprint(bp, buf + sizeof(buf), fmt, va);
	va_end(va);
	werrstr("%s", buf);
}

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

static int bunpack(Biobuf*, char*, ...);

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

static char *
getline(Line *l, Biobuf *b)
{
	char *line;

	line = Brdstr(b, '\n', 1);
	if(line == nil){
		werrstr("could not read a line");
		return nil;
	}
	l->line++;
	return line;
}

static Stl *
parsetxt(Biobuf *bin)
{
	Line ctx;
	Stltri *tri;
	Stl *stl;
	char *line, *f[5];
	int nf, i;

	memset(&ctx, 0, sizeof(Line));

	/* header */
	if((line = getline(&ctx, bin)) == nil){
		error(&ctx, "getline: %r");
		return nil;
	}

	stl = mallocz(sizeof(Stl), 1);
	if(stl == nil){
		error(&ctx, "mallocz0: %r");
		free(line);
		return nil;
	}
	memmove(stl->hdr, line, min(sizeof(stl->hdr), Blinelen(bin)));

	for(;;){
		/* facet normal i j k */
		if((line = getline(&ctx, bin)) == nil){
badline0:
			error(&ctx, "getline: %r");
			goto error;
		}
		nf = tokenize(line, f, nelem(f));
repeat:
		if(nf != 5){
notenough0:
			error(&ctx, "not enough fields %d", nf);
cleanup0:
			free(line);
			goto error;
		}
		if(strcmp(f[0], "facet") == 0 && strcmp(f[1], "normal") == 0){
			tri = mallocz(sizeof(Stltri), 1);
			if(tri == nil){
				werrstr("mallocz1: %r");
				goto cleanup0;
			}
			tri->n[0] = strtod(f[2], nil);
			tri->n[1] = strtod(f[3], nil);
			tri->n[2] = strtod(f[4], nil);
		}else{
			error(&ctx, "expected \"facet normal n₀ n₁ n₂\"");
			goto cleanup0;
		}
		free(line);

		/* outer loop */
		if((line = getline(&ctx, bin)) == nil){
badline1:
			free(tri);
			goto badline0;
		}
		nf = tokenize(line, f, nelem(f));
		if(nf != 2){
notenough1:
			free(tri);
			goto notenough0;
		}
		if(strcmp(f[0], "outer") != 0 && strcmp(f[1], "loop") != 0){
			error(&ctx, "expected \"outer loop\"");
cleanup1:
			free(tri);
			goto cleanup0;
		}
		free(line);

		/* [3]vertex x y z */
		for(i = 0; i < 3; i++){
			if((line = getline(&ctx, bin)) == nil)
				goto badline1;
			nf = tokenize(line, f, nelem(f));
			if(nf != 4)
				goto notenough1;
			if(strcmp(f[0], "vertex") == 0){
				tri->v[i][0] = strtod(f[1], nil);
				tri->v[i][1] = strtod(f[2], nil);
				tri->v[i][2] = strtod(f[3], nil);
			}else{
				error(&ctx, "expected \"vertex x y z\"");
				goto cleanup1;
			}
			free(line);
		}

		/* endloop */
		if((line = getline(&ctx, bin)) == nil)
			goto badline1;
		nf = tokenize(line, f, nelem(f));
		if(nf != 1)
			goto notenough1;
		if(strcmp(f[0], "endloop") != 0){
			error(&ctx, "expected \"endloop\"");
			goto cleanup1;
		}
		free(line);

		/* endfacet */
		if((line = getline(&ctx, bin)) == nil)
			goto badline1;
		nf = tokenize(line, f, nelem(f));
		if(nf != 1)
			goto notenough1;
		if(strcmp(f[0], "endfacet") == 0){
			stl->tris = realloc(stl->tris, (stl->ntris+1)*sizeof(*stl->tris));
			if(stl->tris == nil){
				error(&ctx, "realloc1: %r");
				goto cleanup1;
			}
			stl->tris[stl->ntris++] = tri;
		}else{
			error(&ctx, "expected \"endfacet\"");
			goto cleanup1;
		}
		free(line);

		/* endsolid? */
		if((line = getline(&ctx, bin)) == nil)
			goto badline0;
		nf = tokenize(line, f, nelem(f));
		if(nf < 2)
			goto notenough0;
		if(nf == 2 && strcmp(f[0], "endsolid") == 0){
			free(line);
			break;
		}else
			goto repeat;
	}

	return stl;
error:
	freestl(stl);
	return nil;
}

static Stl *
parsebin(Biobuf *bin, char *magic)
{
	Stltri *tri;
	Stl *stl;
	u32int i, ntris;

	stl = mallocz(sizeof(Stl), 1);
	if(stl == nil){
		werrstr("mallocz0: %r");
		return nil;
	}
	memmove(stl->hdr, magic, 5);
	if(bunpack(bin, "[l", stl->hdr+5, sizeof(stl->hdr)-5, &ntris) < 0){
		werrstr("hdr bunpack: %r");
		goto error;
	}

	for(i = 0; i < ntris; i++){
		tri = mallocz(sizeof(Stltri), 1);
		if(tri == nil){
			werrstr("mallocz1: %r");
			goto error;
		}
		if(bunpack(bin, "vvvvs", tri->n, tri->v[0], tri->v[1], tri->v[2], &tri->attrlen) < 0){
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

	return stl;
error:
	freestl(stl);
	return nil;
}

Stl *
readstl(int fd)
{
	Biobuf *bin;
	Stl *stl;
	char magic[5+1];

	stl = nil;

	bin = Bfdopen(fd, OREAD);
	if(bin == nil){
		werrstr("Bfdopen: %r");
		return nil;
	}

	if(Bread(bin, magic, 5) != 5){
		werrstr("Bread: could not read 5 bytes");
		goto out;
	}
	magic[5] = 0;

	if(strcmp(magic, "solid") == 0){
		if((stl = parsetxt(bin)) == nil)
			werrstr("parsetxt: %r");
	}else
		if((stl = parsebin(bin, magic)) == nil)
			werrstr("parsebin: %r");

out:
	Bterm(bin);
	return stl;
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
