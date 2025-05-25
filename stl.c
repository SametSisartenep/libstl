#include <u.h>
#include <libc.h>
#include <bio.h>
#include "stl.h"

#define min(a,b) ((a)<(b)?(a):(b))

enum {
	TFACET, TNORMAL,
	TOUTER, TLOOP,
	TVERTEX,
	TENDLOOP,
	TENDFACET,
	TENDSOLID,
	TNUM,
	TNL,
};

typedef struct Line Line;
typedef struct Token Token;
typedef struct Tokenizer Tokenizer;

struct Line
{
	char *file;
	ulong line;
};

struct Token
{
	int t;
	char *s;
	double v;
};

struct Tokenizer
{
	Biobuf *in;
	Token tok;
	Line *ctx;
	char *line;
	char *f[10];
	int nf;
	int cur;
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
Bputs(Biobuf *b, u16int s)
{
	uchar buf[2];

	buf[0] = s;
	buf[1] = s >> 8;
	if(Bwrite(b, buf, 2) != 2){
		werrstr("could not put 2 bytes");
		return -1;
	}
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
Bputl(Biobuf *b, u32int l)
{
	uchar buf[4];

	buf[0] = l;
	buf[1] = l >> 8;
	buf[2] = l >> 16;
	buf[3] = l >> 24;
	if(Bwrite(b, buf, 4) != 4){
		werrstr("could not put 4 bytes");
		return -1;
	}
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
Bputf(Biobuf *b, float f)
{
	u32int l;

	l = *(u32int*)&f;
	if(Bputl(b, l) < 0){
		werrstr("Bputl: %r");
		return -1;
	}
	return 0;
}

static int bunpack(Biobuf*, char*, ...);
static int bpack(Biobuf*, char*, ...);

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
vbpack(Biobuf *b, char *fmt, va_list a)
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
			s = va_arg(a, ushort);
			if(Bputs(b, s) < 0)
				goto error;
			break;
		case 'l':
			l = va_arg(a, ulong);
			if(Bputl(b, l) < 0)
				goto error;
			break;
		case 'f':
			f = va_arg(a, double);
			if(Bputf(b, f) < 0)
				goto error;
			break;
		case 'v':
			v = va_arg(a, float*);
			if(bpack(b, "fff", v[0], v[1], v[2]) < 0)
				goto error;
			break;
		case '[':
			p = va_arg(a, void*);
			s = va_arg(a, ushort);
			if(Bwrite(b, p, s) != s){
				werrstr("Bwrite: could not write %ud bytes", s);
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

static int
bpack(Biobuf *b, char *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vbpack(b, fmt, a);
	va_end(a);

	return n;
}

static char *
getline(Biobuf *b)
{
	char *line;

	if((line = Brdstr(b, '\n', 1)) == nil)
		return nil;
	return line;
}

static int
idtoken(char *tok)
{
	static char *tokens[] = {
	 [TFACET]	"facet",
	 [TNORMAL]	"normal",
	 [TOUTER]	"outer",
	 [TLOOP]	"loop",
	 [TVERTEX]	"vertex",
	 [TENDLOOP]	"endloop",
	 [TENDFACET]	"endfacet",
	 [TENDSOLID]	"endsolid",
	};
	int i;

	for(i = 0; i < nelem(tokens); i++)
		if(strcmp(tok, tokens[i]) == 0)
			return i;
	return -1;
}

#define isnum(c) ((c)>='0'&&(c)<='9')
#define couldbenum(c) ((c)=='+'||c=='-'||isnum(c))

static int
lex(Tokenizer *t)
{
	char *ep;
	int new;

	new = t->line == nil;

	if(t->cur < t->nf){
		t->tok.s = t->f[t->cur++];
		if(couldbenum(t->tok.s[0])){
			t->tok.t = TNUM;
			t->tok.v = strtod(t->tok.s, &ep);
			if(*ep != 0){
				werrstr("misleading number-like '%s'", t->tok.s);
				return -1;
			}
		}else{
			t->tok.t = idtoken(t->tok.s);
			if(t->tok.t < 0){
				werrstr("unknown token '%s'", t->tok.s);
				return -1;
			}
		}
	}else{
		free(t->line);
		if(!new){
			t->line = nil;
			t->tok.t = TNL;
			return TNL;
		}
		if((t->line = getline(t->in)) == nil){
			werrstr("could not read a line");
			return -1;
		}
		t->ctx->line++;
		t->nf = tokenize(t->line, t->f, nelem(t->f));
		t->cur = 0;
		t->tok.t = lex(t);
	}
	return t->tok.t;
}

static Stl *
parsetxt(Biobuf *bin)
{
	Line ctx;
	Tokenizer tok;
	Stltri *tri;
	Stl *stl;
	char *line;
	int i, j;

	memset(&ctx, 0, sizeof(Line));
	memset(&tok, 0, sizeof(Tokenizer));
	tok.in = bin;
	tok.ctx = &ctx;
	ctx.line = 1;

	/* header */
	if((line = getline(bin)) == nil){
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
	free(line);

	for(;;){
		tri = nil;

		/* facet normal i j k */
		if(lex(&tok) != TFACET || lex(&tok) != TNORMAL){
faceerr:
			error(&ctx, "expected 'facet normal i j k'");
			goto error;
		}
		tri = mallocz(sizeof(Stltri), 1);
		if(tri == nil){
			werrstr("mallocz: %r");
			goto error;
		}
		for(i = 0; i < 3; i++){
			if(lex(&tok) != TNUM){
				error(&ctx, "expected number got '%s'", tok.tok.s);
				goto error;
			}
			tri->n[i] = tok.tok.v;
		}
		if(lex(&tok) != TNL)
			goto faceerr;

		/* outer loop */
		if(lex(&tok) != TOUTER || lex(&tok) != TLOOP || lex(&tok) != TNL){
			error(&ctx, "expected 'outer loop'");
			goto error;
		}

		/* [3]vertex x y z */
		for(i = 0; i < 3; i++){
			if(lex(&tok) != TVERTEX){
verterr:
				error(&ctx, "expected 'vertex x y z'");
				goto error;
			}
			for(j = 0; j < 3; j++){
				if(lex(&tok) != TNUM){
					error(&ctx, "expected number got '%s'", tok.tok.s);
					goto error;
				}
				tri->v[i][j] = tok.tok.v;
			}
			if(lex(&tok) != TNL)
				goto verterr;
		}

		/* endloop */
		if(lex(&tok) != TENDLOOP || lex(&tok) != TNL){
			error(&ctx, "expected 'endloop'");
			goto error;
		}

		/* endfacet */
		if(lex(&tok) != TENDFACET || lex(&tok) != TNL){
			error(&ctx, "expected 'endfacet'");
			goto error;
		}
		stl->tris = realloc(stl->tris, (stl->ntris+1)*sizeof(*stl->tris));
		if(stl->tris == nil){
			error(&ctx, "realloc1: %r");
			goto error;
		}
		stl->tris[stl->ntris++] = tri;

		/* endsolid? */
		if(lex(&tok) != TENDSOLID){
			tok.cur--;	/* unget */
			continue;
		}
		while(lex(&tok) != TNL)
			;
		break;
	}

	return stl;
error:
	free(tri);
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

static usize
writetxt(Biobuf *b, Stl *stl)
{
	usize n;
	int i, j;

	n = Bprint(b, "solid");
	if(strlen((char*)stl->hdr) > 0)
		n += Bprint(b, " %s", (char*)stl->hdr);
	n += Bprint(b, "\n");

	for(i = 0; i < stl->ntris; i++){
		n += Bprint(b, "facet normal %g %g %g\n",
			stl->tris[i]->n[0], stl->tris[i]->n[1], stl->tris[i]->n[2]);
		n += Bprint(b, "\touter loop\n");
		for(j = 0; j < 3; j++)
			n += Bprint(b, "\t\tvertex %g %g %g\n",
				stl->tris[i]->v[j][0], stl->tris[i]->v[j][1], stl->tris[i]->v[j][2]);
		n += Bprint(b, "\tendloop\n");
		n += Bprint(b, "endfacet\n");
	}

	n += Bprint(b, "endsolid\n");
	return n;
}

static usize
writebin(Biobuf *b, Stl *stl)
{
	Stltri **tri;
	usize n;

	if(bpack(b, "[l", stl->hdr, sizeof(stl->hdr), stl->ntris) < 0){
		werrstr("hdr pack: %r");
		return 0;
	}
	n = sizeof(stl->hdr) + 4;

	for(tri = stl->tris; tri < stl->tris+stl->ntris; tri++){
		if(bpack(b, "vvvvs", (*tri)->n, (*tri)->v+0, (*tri)->v+1, (*tri)->v+2, (*tri)->attrlen) < 0){
			werrstr("tri pack0: %r");
			return 0;
		}
		n += 4*3*4+2;
		if(bpack(b, "[", (*tri)->attrs, (*tri)->attrlen) < 0){
			werrstr("tri pack1: %r");
			return 0;
		}
		n += (*tri)->attrlen;
	}
	return n;
}

usize
writestl(int fd, Stl *stl, int fmt)
{
	Biobuf *b;
	usize n;

	b = Bfdopen(fd, OWRITE);
	if(b == nil)
		sysfatal("Bfdopen: %r");

	n = 0;
	switch(fmt){
	case STLTEXT:
		n = writetxt(b, stl);
		break;
	case STLBINARY:
		n = writebin(b, stl);
		break;
	default:
		werrstr("unknown format '%d'", fmt);
	}
	Bterm(b);
	return n;
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
