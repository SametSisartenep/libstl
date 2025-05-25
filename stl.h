/*
 * 	UINT8[80]    – Header                 - 80 bytes
 * 	UINT32       – Number of triangles    - 04 bytes
 * 	foreach triangle                      - 50 bytes
 * 	    REAL32[3] – Normal vector         - 12 bytes
 * 	    REAL32[3] – Vertex 1              - 12 bytes
 * 	    REAL32[3] – Vertex 2              - 12 bytes
 * 	    REAL32[3] – Vertex 3              - 12 bytes
 * 	    UINT16    – Attribute byte count  - 02 bytes
 * 	end
*/

enum {
	STLTEXT,
	STLBINARY,
};

typedef struct Stltri Stltri;
typedef struct Stl Stl;

struct Stltri
{
	float	n[3];
	float	v[3][3];
	u16int	attrlen;
	u8int	attrs[];
};

struct Stl
{
	u8int	hdr[80];
	u32int	ntris;
	Stltri	**tris;
};

Stl *readstl(int);
usize writestl(int, Stl*, int);
void freestl(Stl*);
