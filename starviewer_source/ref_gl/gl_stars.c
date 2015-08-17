
#include "gl_local.h"
#include "gl_refl.h"

//#define FOR_SKYBOX // applies special rotation so we have desired view angles for the paintball2 skybox.  Also adds moon.
#define USE_FBO


cvar_t *hyg_map_to_sphere = NULL;


int g_writeskybox = 0;
int g_renderskybox_resolution = 1024;

#ifdef USE_FBO
GLuint g_skybox_framebuffer;
GLuint g_skybox_renderbuffer;
#endif

// http://astronexus.com/node/34
//#define HYG_STARS_MAP_TO_SPHERE
#ifdef ENABLE_HYG_STARS

#define MAX_HYG_STARS 180000
//#define HYG_MIN_STARDIST 200.0f
#define HYG_MIN_STARDIST 2000.0f
#define HYG_STARDIST_SCALE 30.0f
#define HYG_MIN_MAG 10

//static vec3_t g_starpositions[MAX_HYG_STARS];
typedef struct {
	vec3_t origin;
	byte color[4];
	float scale;
} starparticle_t;

static starparticle_t g_starparticles[MAX_HYG_STARS];
//particle_t g_starparticles[MAX_HYG_STARS];
//particle_t g_starparticles_white[MAX_HYG_STARS];
static qboolean g_starsloaded = false;
static qboolean g_numstars = 0;

#define MAX_NAMED_STARS 128
#define MAX_STAR_NAME_LEN 32

static int g_numnamedstars = 0;
static vec3_t g_namedstarpositions[MAX_NAMED_STARS];
char g_namedstarnames[MAX_NAMED_STARS][MAX_STAR_NAME_LEN];

// VC's atof is super slow on long buffers because it does a strlen.
// Copy it to a small buffer temporarily to speed it up.  Meh.
float Meh_AtoF (const char *s)
{
	char meh[32];

	Q_strncpyzna(meh, s, sizeof(meh));
	return atof(meh);
}


image_t *g_skybox_image;

void R_InitSkybox (void) // jitskybox
{
	GLenum err;

	g_skybox_image = GL_CreateBlankImage("_skybox", g_renderskybox_resolution, g_renderskybox_resolution, it_reflection);
#ifdef USE_FBO
	// FBO initialization
	qgl.GenFramebuffersEXT(1, &g_skybox_framebuffer);
	assert((err = qgl.GetError()) == GL_NO_ERROR);
	qgl.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_skybox_framebuffer);
	assert((err = qgl.GetError()) == GL_NO_ERROR);

	qgl.GenRenderbuffersEXT(1, &g_skybox_renderbuffer);
	qgl.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, g_skybox_renderbuffer);
	qgl.RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGB, g_renderskybox_resolution, g_renderskybox_resolution);
	qgl.FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, g_skybox_renderbuffer);
	assert((err = qgl.GetError()) == GL_NO_ERROR);

	if (gl_debug->value)
	{
		int wtf = g_skybox_image->texnum;
		ri.Con_Printf(PRINT_ALL, "Reflective texture bound = %d\n", wtf);
	}

	GL_Bind(g_skybox_image->texnum);
	qgl.FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_skybox_image->texnum, 0);

	{
		GLenum status = qgl.CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		assert(status == GL_FRAMEBUFFER_COMPLETE_EXT);
	}

	// Unbind framebuffer
	qgl.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif // USE_FBO
}

void StarColorIndexToRGB (float colorindex, vec_t *color)
{
	// I have no idea how accurate this is.  I'm just guessing based on this:
	// http://domeofthesky.com/clicks/bv.html
	// -.29 = blue
	// 0 = white
	// .59 = yellow
	// .82 = orange
	// 1.41 = red
	// and http://curious.astro.cornell.edu/question.php?number=715
	// -.33 = blue
	// 0.15 = bluish white
	// 0.44 = yellow-white
	// 0.68 = yellow
	// 1.15 = orange
	// 1.64 = red
	int i;

	VectorSet(color, 1.0f, 1.0f, 1.0f);

	colorindex -= 0.4f; // shift so 0 is white

	if (colorindex < 0)
	{
		// remove red and green to make blue
		color[0] += colorindex * 1.5f;
		color[1] += colorindex * 1.5f;
	}
	else
	{
		// Just kind of guessing with arbitrary at this point to get something that looks decent.
		color[1] -= colorindex / 2; // remove green
		color[2] -= colorindex / 1.5; // remove blue
	}

	for (i = 0; i < 3; ++i)
	{
		if (color[i] < 0.0f)
			color[i] = 0.0f;

		if (color[i] > 1.0f)
			color[i] = 1.0f;

		color[i] += 1.0f;
	}

	VectorNormalize(color);
}



void GL_Skybox_f (void)
{
	g_writeskybox = 1;
}



void R_LoadStars (void) // jittemp
{
	char *buff;
	int file_len = ri.FS_LoadFileZ("hygxyz.csv", (void **)&buff);
	int proper_name_count = 0;
	vec3_t x_axis = { 1.0f, 0.0f, 0.0f };
	vec3_t y_axis = { 0.0f, 1.0f, 0.0f };

	g_numstars = 0;
	g_numnamedstars = 0;
	hyg_map_to_sphere = ri.Cvar_Get("hyg_map_to_sphere", "0", 1);
	hyg_map_to_sphere->modified = false;
	
	if (file_len > 0)
	{
		int commacount = 0;
		char *s = buff;
		int n = 0;
		int i;

		ri.Con_Printf(PRINT_ALL, "hygxyz.csv loaded.\n");
		// Skip first 2 lines (header and sun)
		for (i = 0; i < 2; ++i)
		{
			while (n < file_len && *s != '\n')
			{
				++n;
				++s;
			}

			++n;
			++s;
		}

		while (n < file_len && g_numstars < MAX_HYG_STARS)
		{
			float absmag = 9999999.f;
			float mag = 99999.9f;
			float colorindex = 0.0f;
			qboolean has_proper_name = false;

			while (*s != '\n' && n < file_len)
			{
				if (*s == ',')
				{
					++commacount;

					if (commacount == 17) // X = up (vernal equinox)
						g_starparticles[g_numstars].origin[2] = Meh_AtoF(s + 1);
					else if (commacount == 18) // Y = along equator (RA)
						g_starparticles[g_numstars].origin[0] = Meh_AtoF(s + 1);
					else if (commacount == 19) // Z = toward north pole
						g_starparticles[g_numstars].origin[1] = Meh_AtoF(s + 1);
					else if (commacount == 14) // absmag
						absmag = Meh_AtoF(s + 1);
					else if (commacount == 13) // mag
						mag = Meh_AtoF(s + 1);
					else if (commacount == 16) // colorindex
						colorindex = Meh_AtoF(s + 1);
					else if (commacount == 6)
					{
						if (s[1] != ',')
						{
							if (g_numnamedstars < MAX_NAMED_STARS)
							{
								int i;

								for (i = 0; i < MAX_STAR_NAME_LEN - 1 && s[i + 1] != ','; ++i)
									g_namedstarnames[g_numnamedstars][i] = s[i + 1];

								g_namedstarnames[g_numnamedstars][i] = 0;
							}

							++proper_name_count;
							has_proper_name = true;
						}
					}

				}

				++n;
				++s;
			}

			++n;
			++s;
			commacount = 0;

			if (absmag < 0)
				absmag = 0;

			if (absmag > 15)
				absmag = 15;

			if (mag < HYG_MIN_MAG)
			{
				float colormagscale;
				float magscale = (HYG_MIN_MAG - absmag) / HYG_MIN_MAG;
				vec3_t color;

				if (hyg_map_to_sphere->value)
					magscale = (HYG_MIN_MAG - /*abs*/mag) / HYG_MIN_MAG;

				
				StarColorIndexToRGB(colorindex, color);

				if (magscale > 1.0f)
					magscale = 1.0f;

				if (magscale < 0.0f)
					continue;

				if (hyg_map_to_sphere->value)
				{
					colormagscale = 0.01f + powf(magscale, 2.5f) * 2.0f;

					if (colormagscale > 1.0f)
						colormagscale = 1.0f;
				}
				else
				{
					colormagscale = magscale * magscale;
				}

				if (hyg_map_to_sphere->value)
				{
					VectorNormalize(g_starparticles[g_numstars].origin);

					//RotatePointAroundVector(g_starparticles[g_numstars].origin, y_axis, g_starparticles[g_numstars].origin, -100.0f);
					//RotatePointAroundVector(g_starparticles[g_numstars].origin, x_axis, g_starparticles[g_numstars].origin, 35.0f); // latitude of 35 degrees

					VectorScale(g_starparticles[g_numstars].origin, HYG_MIN_STARDIST, g_starparticles[g_numstars].origin);
					g_starparticles[g_numstars].scale = 4.0f + magscale * magscale* magscale * magscale * magscale * 30.0f;

					if (g_starparticles[g_numstars].scale > 14.0f)
						g_starparticles[g_numstars].scale = 14.0f;

					g_starparticles[g_numstars].scale *= 0.8f;
				}
				else
				{
					VectorScale(g_starparticles[g_numstars].origin, HYG_STARDIST_SCALE, g_starparticles[g_numstars].origin);
					g_starparticles[g_numstars].scale = 1 + magscale * magscale * magscale * magscale * 20;
				}

				g_starparticles[g_numstars].color[0] = 255 * color[0] * colormagscale;
				g_starparticles[g_numstars].color[1] = 255 * color[1] * colormagscale;// * (has_proper_name ? 1 : 0);
				g_starparticles[g_numstars].color[2] = 255 * color[2] * colormagscale;
				g_starparticles[g_numstars].color[3] = 255 * colormagscale;

				if (has_proper_name)
				{
					VectorCopy(g_starparticles[g_numstars].origin, g_namedstarpositions[g_numnamedstars]);
					++g_numnamedstars;
				}
				

				++g_numstars;
			}
		}

		ri.FS_FreeFile(buff);
		g_starsloaded = true;
	}
	else
	{
		ri.Con_Printf(PRINT_ALL, "hygxyz.csv failed to load.\n");
	}
}

void R_UnloadStars (void)
{
	g_numstars = 0;
	g_numnamedstars = 0;
	g_starsloaded = false;
}



void R_DrawStarHelper (const vec_t *origin, const vec_t *up_i, const vec_t *right_i, const byte *color, float scale)
{
	vec3_t v, up, right;

	VectorScale(up_i, scale, up);
	VectorScale(right_i, scale, right);

	qgl.Color4ubv(color);

	// Upper left
	qgl.TexCoord2f(0.0f, 0.0f);
	VectorAdd(origin, up, v);
	VectorSubtract(v, right, v);
	qgl.Vertex3fv(v);
	// Upper right
	qgl.TexCoord2f(1.0f, 0.0f);
	VectorAdd(origin, up, v);
	VectorAdd(v, right, v);
	qgl.Vertex3fv(v);
	// Lower right
	qgl.TexCoord2f(1.0f, 1.0f);
	VectorSubtract(origin, up, v);
	VectorAdd(v, right, v);
	qgl.Vertex3fv(v);
	// Lower left
	qgl.TexCoord2f(0.0f, 1.0f);
	VectorSubtract(origin, up, v);
	VectorSubtract(v, right, v);
	qgl.Vertex3fv(v);
}


void R_DrawMoon (void)
{
	static image_t *r_moontexture = NULL;
	vec3_t moon_angles = { -37.62f, -97.73f, 0.0f };
	vec3_t moon_pos;
	vec3_t vec_from_moon;
	vec3_t up, right;
	byte white[4] = { 255, 255, 255, 255 };
	vec3_t			worldup = { 0.0f, 0.0f, 1.0f };

	if (!r_moontexture)
	{
		r_moontexture = GL_FindImage("textures/temp/moon1.tga", it_wall);
	}

	AngleVectors(moon_angles, moon_pos, NULL, NULL);
	VectorScale(moon_pos, HYG_MIN_STARDIST, moon_pos);

	VectorSubtract(r_origin, moon_pos, vec_from_moon);
	CrossProduct(worldup, vec_from_moon, right);
	CrossProduct(vec_from_moon, right, up);
	VectorNormalize(up);
	VectorNormalize(right);
	GL_Bind(r_moontexture->texnum);
	qgl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qgl.Begin(GL_QUADS);
	R_DrawStarHelper(moon_pos, up, right, white, 90.0f);
	qgl.End();
}


void R_DrawStars (void)
{
	if (hyg_map_to_sphere && hyg_map_to_sphere->modified)
	{
		R_UnloadStars();
		hyg_map_to_sphere->modified = false;
	}

	if (!g_starsloaded)
	{
		R_LoadStars();
	}
	else
	{
		const starparticle_t *p;
		int				i;
		vec3_t			up, right, vec_from_star;
		vec3_t			worldup = { 0.0f, 0.0f, 1.0f };
		float			scale;

		qgl.PushMatrix();

		if (hyg_map_to_sphere->value)
		{
			qgl.Translatef(r_origin[0], r_origin[1], r_origin[2]);
		}

		qgl.PushMatrix();
#ifdef FOR_SKYBOX
		qgl.Rotatef(40.0f, 1.0f, 0.0f, 0.0f);
		qgl.Rotatef(-50.0f, 0.0f, 1.0f, 0.0f);
#endif

		GL_Bind(r_startexture->texnum);
		qgl.DepthMask(GL_FALSE);		// no z buffering
		qgl.BlendFunc(GL_ONE, GL_ONE);
		GLSTATE_ENABLE_BLEND;
		GL_TexEnv(GL_COMBINE_EXT);
		qgl.Begin(GL_QUADS);

		//VectorScale(vup, 1.5, up);
		//VectorScale(vright, 1.5, right);

		for (p = g_starparticles, i = 0; i < g_numstars; ++i, ++p)
		{
			VectorSubtract(r_origin, p->origin, vec_from_star);
			CrossProduct(worldup, vec_from_star, right);
			CrossProduct(vec_from_star, right, up);
			VectorNormalize(up);
			VectorNormalize(right);

			scale = p->scale;// * p->scale;
			R_DrawStarHelper(p->origin, up, right, p->color, scale);
			/*white_color[0] = p->color[3];
			white_color[1] = p->color[3];
			white_color[2] = p->color[3];
			white_color[3] = p->color[3];
			R_DrawStarHelper(p->origin, up, right, white_color, scale);*/
		}

		qgl.End();
		qgl.PopMatrix();

#ifdef FOR_SKYBOX
		if (hyg_map_to_sphere->value)
		{
			GL_TexEnv(GL_REPLACE);
			R_DrawMoon();
		}
#endif

		qgl.PopMatrix();
		GLSTATE_DISABLE_BLEND;
		qgl.Color4f(1.0f, 1.0f, 1.0f, 1.0f);
		qgl.DepthMask(GL_TRUE);		// back to normal Z buffering
		GL_TexEnv(GL_REPLACE);
	}
}
#endif // ENABLE_HYG_STARS


typedef struct {
	float yaw;
	float pitch;
	char *name;
} skybox_side_t;

const char *side_basename = "pbsky3";

skybox_side_t skybox_sides[] = {
	{ 90, 0, "bk" },
	{ 180, 0, "lf" },
	{ -90, 0, "ft" },
	{ 0, 0, "rt" },
	{ 0, -90, "up" },
	{ 0, 90, "dn" },
};

void R_WriteSkybox (refdef_t *fd)
{
	int c, i, temp;
	int vidheight = vid.height;
	int vidwidth = vid.width;
	int refwidth = fd->width;
	int refheight = fd->height;
	float fovx = r_newrefdef.fov_x;
	float fovy = r_newrefdef.fov_y;
	FILE *f;
	int side;
	int numsides = sizeof(skybox_sides) / sizeof(skybox_side_t);
	byte *buffer = (byte *)malloc(g_renderskybox_resolution * g_renderskybox_resolution * 3 + 18);

#ifdef USE_FBO
	qgl.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_skybox_framebuffer);
#endif

	qgl.ClearColor(0, 0, 0, 1);
	
	// Bit of a hack to override the screen resolution and FOV
	vid.height = g_renderskybox_resolution;
	vid.width = g_renderskybox_resolution;
	fd->width = g_renderskybox_resolution;
	fd->height = g_renderskybox_resolution;
	fd->fov_x = 90.0f;
	fd->fov_y = 90.0f;
	fd->viewangles[ROLL] = 0.0f;

	// TODO: Render every direction.
	for (side = 0; side < numsides; ++side)
	{
		char side_name[256];

		fd->viewangles[YAW] = skybox_sides[side].yaw;
		fd->viewangles[PITCH] = skybox_sides[side].pitch;

		// Render!
		qgl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_RenderView(fd);

		// Generate TGA file
		memset(buffer, 0, 18);
		buffer[2] = 2;		// uncompressed type
		buffer[12] = g_renderskybox_resolution & 255;
		buffer[13] = g_renderskybox_resolution >> 8;
		buffer[14] = g_renderskybox_resolution & 255;
		buffer[15] = g_renderskybox_resolution >> 8;
		buffer[16] = 24;	// pixel size
		qgl.PixelStorei(GL_PACK_ALIGNMENT, 1);
		qgl.ReadPixels(0, 0, g_renderskybox_resolution, g_renderskybox_resolution, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18); 
		
		// swap rgb to bgr
		c = 18 + g_renderskybox_resolution * g_renderskybox_resolution * 3;

		for (i = 18; i < c; i += 3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}

		Com_sprintf(side_name, sizeof(side_name), "%s%s.tga", side_basename, skybox_sides[side].name);
		f = fopen(side_name, "wb");

		if (f)
		{
			fwrite(buffer, 1, c, f);
			fclose(f);
		}
	}

#ifdef USE_FBO
	qgl.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif
		
	free(buffer);

	// Restore values
	vid.height = vidheight;
	vid.width = vidwidth;
	fd->width = refwidth;
	fd->height = refheight;
	fd->fov_x = fovx;
	fd->fov_y = fovy;

	R_Clear();
}


void R_DrawStarNames (void)
{
	int drawnames = 1;
	vec3_t viewpos = { 0, 0, 0 };
	vec3_t viewvec;

	if (!ri.Cvar_Get("cl_drawstarnames", "1", 0)->value)
		return;

	AngleVectors(r_newrefdef.viewangles, viewvec, NULL, NULL);

	if (hyg_map_to_sphere && !hyg_map_to_sphere->value)
		VectorCopy(r_newrefdef.vieworg, viewpos);

	if (drawnames)
	{
		int namedindex;
		float offset = 0;
		float bestdot = -1;
		const char *nametodisplay = NULL;

		for (namedindex = 0; namedindex < g_numnamedstars; ++namedindex)
		{
			vec3_t posdiff;
			float dot;

			VectorSubtract(g_namedstarpositions[namedindex], viewpos, posdiff);
			VectorNormalizeAccurate(posdiff);
			VectorNormalizeAccurate(viewvec);
			dot = DotProduct(viewvec, posdiff);

			if (dot > .9995f && dot > bestdot)
			{
				bestdot = dot;
				nametodisplay = g_namedstarnames[namedindex];
			}
		}

		if (nametodisplay)
			Draw_String(r_newrefdef.width / 2 + 16, r_newrefdef.height / 2 - 4 * cl_hudscale->value, nametodisplay);
	}
}
