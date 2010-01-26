/*
 * Copyright © 2006 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include "annotate.h"

COMPIZ_PLUGIN_20090315 (annotate, AnnoPluginVTable);

void
AnnoScreen::cairoClear (cairo_t    *cr)
{
    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);

    content = false;
}

cairo_t *
AnnoScreen::cairoContext ()
{
    if (!cairo)
    {
	XRenderPictFormat *format;
	Screen		  *xScreen;
	int		  w, h;

	xScreen = ScreenOfDisplay (screen->dpy (), screen->screenNum ());

	w = screen->width ();
	h = screen->height ();

	format = XRenderFindStandardFormat (screen->dpy (),
					    PictStandardARGB32);

	pixmap = XCreatePixmap (screen->dpy (), screen->root (), w, h, 32);

	texture = GLTexture::bindPixmapToTexture (pixmap, w, h, 32);

	if (texture.empty ())
	{
	    compLogMessage ("annotate", CompLogLevelError,
			    "Couldn't bind pixmap 0x%x to texture",
			    (int) pixmap);

	    XFreePixmap (screen->dpy (), pixmap);

	    return NULL;
	}

	surface =
	    cairo_xlib_surface_create_with_xrender_format (screen->dpy (),
							   pixmap, xScreen,
							   format, w, h);

	cairo = cairo_create (surface);

	cairoClear (cairo);
    }

    return cairo;
}

void
AnnoScreen::setSourceColor (cairo_t	   *cr,
		    	    unsigned short *color)
{
    cairo_set_source_rgba (cr,
			   (double) color[0] / 0xffff,
			   (double) color[1] / 0xffff,
			   (double) color[2] / 0xffff,
			   (double) color[3] / 0xffff);
}

void
AnnoScreen::drawCircle (double	       xc,
			double	       yc,
			double	       radius,
			unsigned short *fillColor,
			unsigned short *strokeColor,
			double	       strokeWidth)
{
    cairo_t *cr;

    cr = cairoContext ();
    if (cr)
    {
	double  ex1, ey1, ex2, ey2;

	setSourceColor (cr, fillColor);
	cairo_arc (cr, xc, yc, radius, 0, 2 * M_PI);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, strokeWidth);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	setSourceColor (cr, strokeColor);
	cairo_stroke (cr);

	CompRegion reg (ex1, ey1, ex2 -ex1, ey2 - ex1);

	content = true;
	cScreen->damageRegion (reg);
    }
}

void
AnnoScreen::drawRectangle (double	  x,
			   double	  y,
			   double	  w,
			   double	  h,
			   unsigned short *fillColor,
			   unsigned short *strokeColor,
			   double	  strokeWidth)
{
    cairo_t *cr;

    cr = cairoContext ();
    if (cr)
    {
	double  ex1, ey1, ex2, ey2;

	setSourceColor (cr, fillColor);
	cairo_rectangle (cr, x, y, w, h);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, strokeWidth);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	setSourceColor (cr, strokeColor);
	cairo_stroke (cr);

	CompRegion reg (ex1, ey1, ex2 -ex1, ey2 - ex1);

	content = true;
	cScreen->damageRegion (reg);
    }
}

void
AnnoScreen::drawLine (double	     x1,
		      double	     y1,
		      double	     x2,
		      double	     y2,
		      double	     width,
		      unsigned short *color)
{
    cairo_t *cr;

    cr = cairoContext ();
    if (cr)
    {
	double ex1, ey1, ex2, ey2;

	cairo_set_line_width (cr, width);
	cairo_move_to (cr, x1, y1);
	cairo_line_to (cr, x2, y2);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	setSourceColor (cr, color);
	cairo_stroke (cr);

	CompRegion reg (ex1, ey1, ex2 -ex1, ey2 - ey1);

	content = true;
	cScreen->damageRegion (reg);
    }
}

void
AnnoScreen::drawText (double	     		     x,
		      double	     		     y,
		      const char	     	     *text,
		      const char	     	     *fontFamily,
		      double	     		     fontSize,
		      cairo_font_slant_t	     fontSlant,
		      cairo_font_weight_t	     fontWeight,
		      unsigned short 		     *fillColor,
		      unsigned short 		     *strokeColor,
		      double	     		     strokeWidth)
{
    REGION reg;
    cairo_t *cr;

    cr = cairoContext ();
    if (cr)
    {
	cairo_text_extents_t extents;

	cairo_set_line_width (cr, strokeWidth);
	setSourceColor (cr, fillColor);
	cairo_select_font_face (cr, fontFamily, fontSlant, fontWeight);
	cairo_set_font_size (cr, fontSize);
	cairo_text_extents (cr, text, &extents);
	cairo_save (cr);
	cairo_move_to (cr, x, y);
	cairo_text_path (cr, text);
	cairo_fill_preserve (cr);
	setSourceColor (cr, strokeColor);
	cairo_stroke (cr);
	cairo_restore (cr);

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = x;
	reg.extents.y1 = y + extents.y_bearing - 2.0;
	reg.extents.x2 = x + extents.width + 20.0;
	reg.extents.y2 = y + extents.height;

	CompRegion region (reg.extents.x1, reg.extents.y1, reg.extents.x2 - 
							   reg.extents.x1,
							   reg.extents.y2 -
							   reg.extents.y1);

	content = true;
	cScreen->damageRegion (region);
    }
}

/* DBUS Interface (TODO: plugin interface) */

/* Here, you can use DBUS or any other plugin via the action system to draw on
 * the screen using cairo. Parameters are as follows:
 * "tool": ["rectangle", "circle", "line", "text"] default: "line"
 * - This allows you to select what you want to draw
 * Tool-specific parameters:
 * - * "circle"
 * - * - "xc" float, default: 0 - X Center
 * - * - "yc" float, default: 0 - Y Center
 * - * - "radius" float, default: 0 - Radius
 * - * "rectangle"
 * - * - "x" float, default: 0 - X Point
 * - * - "y" float, default: 0 - Y Point
 * - * - "width" float, default: 0 - Width
 * - * - "height" float, default: 0 - Height
 * - * "line"
 * - * - "x1" float, default: 0 - X Point 1
 * - * - "y1" float, default: 0 - Y Point 1
 * - * - "x2" float, default: 0 - X Point 2
 * - * - "y2" float, default: 0 - Y Point 2
 * - * "text"
 * - * - "slant" string, default: "" - ["oblique", "italic", ""] - Text Slant
 * - * - "weight" string, default: " - ["bold", ""] - Text Weight
 * - * - "text" string, default: "" - Any Character - The text to display
 * - * - "family" float, default: "Sans" - The font family
 * - * - "size" float, default: 36.0 - Font Size
 * - * - "x" float, default: 0 - X Point
 * - * - "u" float, default: 0 - Y Point
 * Other parameters are:
 * - * - "fill_color" float, default: 0 - Drawing Fill Color
 * - * - "stroke_color" float, default: 0 - Drawing Border Color
 * - * - "line_width" float, default: 0 - Drawing Width
 * - * - "stroke_width" float, default: 0 - Drawing Height
 * - * - All of these are taken from the builtin options if not provided
 */ 

bool
AnnoScreen::draw (CompAction         *action,
		  CompAction::State  state,
		  CompOption::Vector options)
{
    cairo_t *cr;

    cr = cairoContext ();
    if (cr)
    {
        const char	   *tool;
        unsigned short *fillColor, *strokeColor;
        double	   lineWidth, strokeWidth;

        tool =
	 CompOption::getStringOptionNamed (options, "tool", "line").c_str ();

        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

        fillColor = optionGetFillColor ();
        fillColor = CompOption::getColorOptionNamed (options, "fill_color",
				         fillColor);

        strokeColor = optionGetStrokeColor ();
        strokeColor = CompOption::getColorOptionNamed (options,
				           "stroke_color", strokeColor);

        strokeWidth = optionGetStrokeWidth ();
        strokeWidth = CompOption::getFloatOptionNamed (options, "stroke_width",
				           strokeWidth);

        lineWidth = optionGetLineWidth ();
        lineWidth = CompOption::getFloatOptionNamed (options, "line_width",
				         lineWidth);

        if (strcasecmp (tool, "rectangle") == 0)
        {
	    double x, y, w, h;

	    x = CompOption::getFloatOptionNamed (options, "x", 0);
	    y = CompOption::getFloatOptionNamed (options, "y", 0);
	    w = CompOption::getFloatOptionNamed (options, "w", 100);
	    h = CompOption::getFloatOptionNamed (options, "h", 100);

	    drawRectangle (x, y, w, h, fillColor, strokeColor,
			       strokeWidth);
        }
        else if (strcasecmp (tool, "circle") == 0)
        {
	    double xc, yc, r;

	    xc = CompOption::getFloatOptionNamed (options, "xc", 0);
	    yc = CompOption::getFloatOptionNamed (options, "yc", 0);
	    r  = CompOption::getFloatOptionNamed (options, "radius", 100);

	    drawCircle (xc, yc, r, fillColor, strokeColor,
			    strokeWidth);
        }
        else if (strcasecmp (tool, "line") == 0)
        {
	    double x1, y1, x2, y2;

	    x1 = CompOption::getFloatOptionNamed (options, "x1", 0);
	    y1 = CompOption::getFloatOptionNamed (options, "y1", 0);
	    x2 = CompOption::getFloatOptionNamed (options, "x2", 100);
	    y2 = CompOption::getFloatOptionNamed (options, "y2", 100);

	    drawLine (x1, y1, x2, y2, lineWidth, fillColor);
        }
        else if (strcasecmp (tool, "text") == 0)
        {
	    double	     x, y, size;
	    const char	     *text, *family;
	    cairo_font_slant_t slant;
	    cairo_font_weight_t weight;
	    const char	     *str;

	    str =
	       CompOption::getStringOptionNamed (options, "slant", "").c_str ();
	    if (strcasecmp (str, "oblique") == 0)
	        slant = CAIRO_FONT_SLANT_OBLIQUE;
	    else if (strcasecmp (str, "italic") == 0)
	        slant = CAIRO_FONT_SLANT_ITALIC;
	    else
	        slant = CAIRO_FONT_SLANT_NORMAL;

	    str = 
	      CompOption::getStringOptionNamed (options, "weight", "").c_str ();
	    if (strcasecmp (str, "bold") == 0)
	        weight = CAIRO_FONT_WEIGHT_BOLD;
	    else
	        weight = CAIRO_FONT_WEIGHT_NORMAL;

	    x      = CompOption::getFloatOptionNamed (options, "x", 0);
	    y      = CompOption::getFloatOptionNamed (options, "y", 0);
	    text   = 
		CompOption::getStringOptionNamed (options, "text", "").c_str ();
	    family = CompOption::getStringOptionNamed (options, "family",
				           "Sans").c_str ();
	    size   = CompOption::getFloatOptionNamed (options, "size", 36.0);

	    drawText (x, y, text, family, size, slant, weight,
		          fillColor, strokeColor, strokeWidth);
        }
    }

    return true;
}

bool
AnnoScreen::initiate (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options)
{
    if (screen->otherGrabExist (0))
        return false;

    if (!grabIndex)
        grabIndex = screen->pushGrab (None, "annotate");

    if (state & CompAction::StateInitButton)
        action->setState (action->state () | CompAction::StateTermButton);

    if (state & CompAction::StateInitKey)
        action->setState (action->state () | CompAction::StateTermKey);

    annoLastPointerX = pointerX;
    annoLastPointerY = pointerY;

    eraseMode = false;

    screen->handleEventSetEnabled (this, true);

    return true;
}

bool
AnnoScreen::terminate (CompAction         *action,
		       CompAction::State  state,
		       CompOption::Vector options)
{
    if (grabIndex)
    {
        screen->removeGrab (grabIndex, NULL);
        grabIndex = 0;
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    /* No need to handle motion events for now */

    screen->handleEventSetEnabled (this, false);

    return false;
}

bool
AnnoScreen::eraseInitiate (CompAction         *action,
		           CompAction::State  state,
		           CompOption::Vector options)
{
    if (screen->otherGrabExist (0))
        return false;

    if (!grabIndex)
        grabIndex = screen->pushGrab (None, "annotate");

    if (state & CompAction::StateInitButton)
        action->setState (action->state () | CompAction::StateTermButton);

    if (state & CompAction::StateInitKey)
        action->setState (action->state () | CompAction::StateTermKey);

    annoLastPointerX = pointerX;
    annoLastPointerY = pointerY;

    eraseMode = true;

    screen->handleEventSetEnabled (this, true);

    return false;
}

bool
AnnoScreen::clear (CompAction         *action,
		   CompAction::State  state,
		   CompOption::Vector options)
{
    if (content)
    {
        cairo_t *cr;

        cr = cairoContext ();
        if (cr)
	    cairoClear (cairo);

        cScreen->damageScreen ();

	/* We don't need to refresh the screen anymore */
	gScreen->glPaintOutputSetEnabled (this, false);
    }

    return true;
}

bool
AnnoScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			   const GLMatrix	     &transform,
			   const CompRegion	     &region,
			   CompOutput 		     *output,
			   unsigned int		     mask)
{
    bool status;

    status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (status && content && !region.isEmpty ())
    {
	CompRect rect;
	GLMatrix sTransform = transform;
	int      numRect;
	int      pos = 0;

	/* This replaced prepareXCoords (s, output, -DEFAULT_Z_CAMERA) */
	sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	glPushMatrix ();
	glLoadMatrixf (sTransform.getMatrix ());

	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnable (GL_BLEND);

	foreach (GLTexture *tex, texture)
	{
	    CompRect::vector rect = region.rects ();
	    numRect = region.rects ().size ();

	    tex->enable (GLTexture::Fast);

	    glBegin (GL_QUADS);

	    while (numRect--)
	    {
	        glTexCoord2f (
		    COMP_TEX_COORD_X (tex->matrix (), rect.at (pos).x1 ()),
		    COMP_TEX_COORD_Y (tex->matrix (), rect.at (pos).y2 ()));
	        glVertex2i (rect.at (pos).x1 (), rect.at (pos).y2 ());

	        glTexCoord2f (
		    COMP_TEX_COORD_X (tex->matrix (), rect.at (pos).x2 ()),
		    COMP_TEX_COORD_Y (tex->matrix (), rect.at (pos).y2 ()));
	        glVertex2i (rect.at (pos).x2 (), rect.at (pos).y2 ());

	        glTexCoord2f (
		    COMP_TEX_COORD_X (tex->matrix (), rect.at (pos).x2 ()),
		    COMP_TEX_COORD_Y (tex->matrix (), rect.at (pos).y1 ()));
	        glVertex2i (rect.at (pos).x2 (), rect.at (pos).y1 ());

	        glTexCoord2f (
		    COMP_TEX_COORD_X (tex->matrix (), rect.at (pos).x1 ()),
		    COMP_TEX_COORD_Y (tex->matrix (), rect.at (pos).y1 ()));
	        glVertex2i (rect.at (pos).x1 (), rect.at (pos).y1 ());

	        pos++;
	    }

	    glEnd ();

	    tex->disable ();

	    glDisable (GL_BLEND);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

	    glPopMatrix ();
	}

    }

    return status;
}

void
AnnoScreen::handleMotionEvent (int	  xRoot,
		       	       int	  yRoot)
{
    if (grabIndex)
    {
	if (eraseMode)
	{
	    static unsigned short color[] = { 0, 0, 0, 0 };

	    drawLine (annoLastPointerX, annoLastPointerY,
		      xRoot, yRoot,
		      20.0, color);
	}
	else
	{
	    drawLine (annoLastPointerX, annoLastPointerY,
			  xRoot, yRoot,
			  optionGetLineWidth (),
			  optionGetFillColor ());
	}

	annoLastPointerX = xRoot;
	annoLastPointerY = yRoot;

	gScreen->glPaintOutputSetEnabled (this, true);
    }
}

void
AnnoScreen::handleEvent (XEvent      *event)
{
    switch (event->type) {
    case MotionNotify:
	handleMotionEvent (pointerX, pointerY);
    case EnterNotify:
    case LeaveNotify:
	handleMotionEvent (pointerX, pointerY);
    default:
	break;
    }

    screen->handleEvent (event);
}

AnnoScreen::AnnoScreen (CompScreen *screen) :
    PluginClassHandler <AnnoScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    pixmap (None),
    surface (NULL),
    cairo (NULL),
    content (false),
    eraseMode (false)
{
    ScreenInterface::setHandler (screen, false);
    GLScreenInterface::setHandler (gScreen, false);

    optionSetInitiateButtonInitiate (boost::bind (&AnnoScreen::initiate, this,
						  _1, _2, _3));
    optionSetInitiateButtonTerminate (boost::bind (&AnnoScreen::terminate, this,
						   _1, _2, _3));
    optionSetEraseButtonInitiate (boost::bind (&AnnoScreen::eraseInitiate, this,
						  _1, _2, _3));
    optionSetEraseButtonTerminate (boost::bind (&AnnoScreen::terminate, this,
						  _1, _2, _3));
    optionSetClearKeyInitiate (boost::bind (&AnnoScreen::clear, this,
					    _1, _2, _3));
    optionSetDrawInitiate (boost::bind (&AnnoScreen::draw, this,
					_1, _2, _3));
}

AnnoScreen::~AnnoScreen ()
{
    if (cairo)
	cairo_destroy (cairo);
    if (surface)
	cairo_surface_destroy (surface);
    if (pixmap)
	XFreePixmap (screen->dpy (), pixmap);
}

bool
AnnoPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION) ||
	!CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI) ||
	!CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return false;

    return true;
}
