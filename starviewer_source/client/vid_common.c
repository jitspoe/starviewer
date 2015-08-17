/*
Copyright (C) 1997-2001 Id Software, Inc., 2008 Nathan "jitspoe" Wulf

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// Cross-platform compatible code to avoid redundant functions for each OS.


#include <assert.h>
#include <float.h>

#include "client.h"


/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_ref->modified = true;
}


/*
** VID_GetModeInfo
*/

vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240",    320, 240,   0 },
	{ "Mode 1: 400x300",    400, 300,   1 },
	{ "Mode 2: 512x384",    512, 384,   2 },
	{ "Mode 3: 640x480",    640, 480,   3 },
	{ "Mode 4: 800x600",    800, 600,   4 },
	{ "Mode 5: 960x720",    960, 720,   5 },
	{ "Mode 6: 1024x768",   1024, 768,  6 },
	{ "Mode 7: 1152x864",   1152, 864,  7 },
	{ "Mode 8: 1280x960",   1280, 960,  8 },
	{ "Mode 9: 1280x1024",  1280, 1024, 9 }, // jit
	{ "Mode 10: 1600x1200", 1600, 1200, 10 },
	{ "Mode 11: 2048x1536", 2048, 1536, 11 },
	 // jit
	{ "blah", 720, 480,   12 },
	{ "blah", 720, 576,   13 },
	{ "blah", 848, 480,   14 },
	{ "blah", 960, 600,   15 },
	{ "blah", 1088, 612,  16 },
	{ "blah", 1280, 720,  17 },
	{ "blah", 1280, 768,  18 },
	{ "blah", 1280, 800,  19 },
	{ "blah", 1680, 1050, 20 },
	{ "blah", 1440, 900,  21 },
	{ "blah", 1920, 1200, 22 },
	{ "blah", 1920, 1080, 23 }, // T3RR0R15T
	{ "blah", 1920, 1440, 24 }, // T3RR0R15T
	{ "blah", 1366, 768,  25 }, // T3RR0R15T
	{ "blah", 1600, 900,  26 }, // T3RR0R15T
	// jitodo, custom resolution
};

#define VID_NUM_MODES (sizeof(vid_modes) / sizeof(vid_modes[0]))

qboolean VID_GetModeInfo (int *width, int *height, int mode)
{
	if (mode < 0 || mode >= VID_NUM_MODES)
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}


/*
** VID_NewWindow
*/
void VID_NewWindow (int width, int height)
{
	viddef.width = width;
	viddef.height = height;
	cl.force_refdef = true;		// can't use a paused refdef - Linux code didn't have this, should we remove it for linux?
}


void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;
	
	va_start(argptr,fmt);
	_vsnprintf(msg, sizeof(msg), fmt, argptr); // jitsecurity -- prevent buffer overruns
	va_end(argptr);
	NULLTERMINATE(msg); // jitsecurity -- make sure string is null terminated.
	Com_Error(err_level,"%s", msg);
}

