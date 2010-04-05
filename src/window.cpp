/**
 *
 * Compiz group plugin
 *
 * window.cpp
 *
 * Copyright : (C) 2006-2009 by Patrick Niklaus, Roi Cohen, Danny Baumann,
 *				Sam Spilsbury
 * Authors: Patrick Niklaus <patrick.niklaus@googlemail.com>
 *          Roi Cohen       <roico.beryl@gmail.com>
 *          Danny Baumann   <maniac@opencompositing.org>
 *	    Sam Spilsbury   <smspillaz@gmail.com>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **/

#include "group.h"

/*
 * GroupWindow::checkProperty
 * 
 * Description: Checks our set window property on startup - groups windows
 * according to those properties
 *
 */
bool
GroupWindow::checkProperty (long int   &id,
			    bool       &tabbed,
			    GLushort   *color)
{
    Atom          type;
    int           retval, fmt;
    unsigned long nitems, exbyte;
    long int      *data;
    
    GROUP_SCREEN (screen);

    retval = XGetWindowProperty (screen->dpy (), window->id (),
	     			 gs->groupWinPropertyAtom, 0, 5, False,
	     			 XA_CARDINAL, &type, &fmt, &nitems, &exbyte,
	     			 (unsigned char **)&data);

    if (retval == Success)
    {
	if (type == XA_CARDINAL && fmt == 32 && nitems == 5)
	{
	    id = data[0];

	    tabbed = (bool) data[1];
	    color[0] = (GLushort) data[2];
	    color[1] = (GLushort) data[3];
	    color[2] = (GLushort) data[4];

	    XFree (data);
	    return true;
	}
	else if (fmt != 0)
	    XFree (data);
    }

    return false;
}

/*
 * GroupWindow::updateProperty
 *
 * On color change / group change / tabbing - update the X Window property
 *
 * FIXME: broken
 */
void
GroupWindow::updateProperty ()
{
    GROUP_SCREEN (screen);

    // Do not change anything in this case
    if (readOnlyProperty)
	return;

    if (group)
    {
	long int buffer[5];

	buffer[0] = group->identifier;
	buffer[1] = (tab) ? true : false;

	/* group color RGB */
	buffer[2] = group->color[0];
	buffer[3] = group->color[1];
	buffer[4] = group->color[2];

	XChangeProperty (screen->dpy (), window->id (),
			 gs->groupWinPropertyAtom, XA_CARDINAL, 32,
			 PropModeReplace,  (unsigned char *) buffer, 5);
    }
    else
    {
	XDeleteProperty (screen->dpy (), window->id (),
						      gs->groupWinPropertyAtom);
    }
}

/*
 * GroupWindow::damagePaintRectangle ()
 *
 * Description: Sets a redraw (damage) hint on the rectangle where the stretched
 * window is drawn
 */

void
GroupScreen::damagePaintRectangle (CompRect box)
{
    CompRegion reg (box.x () - 1, box.y () -1,
		    box.width () + 1, box.height () + 1);

    cScreen->damageRegion (reg);
}

/*
 * GroupWindow::getStretchRectangle ()
 *
 * Description: Gets the stretch rectangle for a window based on the resize
 * geometry of the window being resized 
 *
 *
 */

void
GroupWindow::getStretchRectangle (CompRect &pBox,
			          float    *xScaleRet,
				  float    *yScaleRet)
{
    CompRect box;
    int      width, height;
    float    xScale, yScale;

    box.setX (resizeGeometry.x () - window->input ().left);
    box.setY (resizeGeometry.y () - window->input ().top);
    box.setWidth (resizeGeometry.x () + resizeGeometry.width () +
	     window->serverGeometry ().border () * 2 + window->input ().right -
	     							      box.x ());

    if (window->shaded ())
    {
	box.setHeight (resizeGeometry.y () + window->height () +
			window->input ().bottom - box.y ());
    }
    else
    {
	box.setHeight (resizeGeometry.height () +
	       window->serverGeometry ().border () * 2 + window->input ().bottom
		+ window->input ().top);
    }

    width  = window->width ()  + window->input ().left + window->input ().right;
    height = window->height () + window->input ().top  + window->input ().bottom;

    xScale = (width)  ? (box.x2 () - box.x1 ()) / (float) width  : 1.0f;
    yScale = (height) ? (box.y2 () - box.y1 ()) / (float) height : 1.0f;

    pBox.setX (box.x1 () - (window->output ().left - window->input ().left)
						 * xScale);
    pBox.setY (box.y1 () - (window->output ().top - window->input ().top)
						 * yScale);
    pBox.setWidth (box.x2 () + window->output ().right * xScale - box.x ());
    pBox.setHeight (box.y2 () + window->output ().bottom * yScale - box.y ());

    if (xScaleRet)
	*xScaleRet = xScale;
    if (yScaleRet)
	*yScaleRet = yScale;
}

/*
 * GroupWindow::updateResizeRectangle ()
 *
 * Description: This function updates the resize rectangle based on the resize
 * of the currently resized window of the group
 *
 */

unsigned int
GroupWindow::updateResizeRectangle (CompRect   masterGeometry,
				    bool       damage)
{
    CompRect     newGeometry;
    unsigned int mask = 0;
    int          newWidth, newHeight;
    int          widthDiff, heightDiff;

    GROUP_SCREEN (screen);

    if (resizeGeometry.isEmpty () || !gs->resizeInfo)
    {
	return 0;
    }

    newGeometry.setX (WIN_X (window) + (masterGeometry.x () -
				 gs->resizeInfo->origGeometry.x ()));
    newGeometry.setY (WIN_Y (window) + (masterGeometry.y () -
				 gs->resizeInfo->origGeometry.y ()));

    widthDiff = masterGeometry.width () - gs->resizeInfo->origGeometry.width ();
    newGeometry.setWidth (MAX (1, WIN_WIDTH (window) + widthDiff));
    heightDiff = masterGeometry.height () -
    					 gs->resizeInfo->origGeometry.height ();
    newGeometry.setHeight (MAX (1, WIN_HEIGHT (window) + heightDiff));

    if (window->constrainNewWindowSize (newGeometry.width (),
    				        newGeometry.height (),
					&newWidth, &newHeight))
    {

	newGeometry.setWidth (newWidth);
	newGeometry.setHeight (newHeight);
    }

    if (damage)
    {
	if (newGeometry != resizeGeometry)
	{
	    cWindow->addDamage ();
	}
    }

    if (newGeometry.x () != resizeGeometry.x ())
    {
	resizeGeometry.setX (newGeometry.x ());
	mask |= CWX;
    }
    if (newGeometry.y () != resizeGeometry.y ())
    {
	resizeGeometry.setY (newGeometry.y ());
	mask |= CWY;
    }
    if (newGeometry.width () != resizeGeometry.width ())
    {
	resizeGeometry.setWidth (newGeometry.width ());
	mask |= CWWidth;
    }
    if (newGeometry.height () != resizeGeometry.height ())
    {
	resizeGeometry.setHeight (newGeometry.height ());
	mask |= CWHeight;
    }

    return mask;
}

/*
 * GroupWindow::windowNotify ()
 *
 * Description: This function catches changes in the window state (shaded,
 * minimized, etc) and applies them to other windows.
 *
 */

void
GroupWindow::windowNotify (CompWindowNotify n)
{
    GROUP_SCREEN (screen);
    bool enqueue = false;

    window->windowNotify (n);

    if (!group || gs->ignoreMode || gs->queued)
	return;

    switch (n)
    {
	case CompWindowNotifyMinimize:
	case CompWindowNotifyUnminimize:
	    if (gs->optionGetMinimizeAll ())
		enqueue = true;
	    break;
	case CompWindowNotifyShade:
	case CompWindowNotifyUnshade:
	    if (gs->optionGetShadeAll ())
		enqueue = true;
	    break;
	case CompWindowNotifyRestack:
	    if (gs->optionGetRaiseAll ())
		enqueue = true;
	    break;
	default:
	    break;
    }

    if (enqueue)
	gs->enqueueWindowNotify (window, n);
}

/*
 * GroupWindow::stateChangeNotify ()
 *
 * Description: This function catches changes in the window state (maximized,
 * etc). If a window is maximized and in a group, and if it is necessary to do
 * so then maximize all windows in the group. Also vice versa
 */

void
GroupWindow::stateChangeNotify (unsigned int lastState)
{
    GROUP_SCREEN (screen);

    if (group && !gs->ignoreMode)
    {
	if (((lastState & MAXIMIZE_STATE) !=
	      (window->state () & MAXIMIZE_STATE)) &&
	    gs->optionGetMaximizeUnmaximizeAll ())
	{
	    foreach (CompWindow *cw, group->windows)
	    {
		if (!cw)
		    continue;

		if (cw->id () == window->id ())
		    continue;

		cw->maximize (window->state () & MAXIMIZE_STATE);
	    }
	}
    }

    window->stateChangeNotify (lastState);
}

/*
 * GroupWindow::resizeNotify ()
 *
 * Description: This function catches resizeNotify on the wrappable function
 * chain. Unlike functions below, this is funciton is not used to apply resize
 * operations to other windows since resizeNotify is only called when the X
 * Server confirms a window has been resized.
 *
 * See handleEvent, case gs->resizeNotifyAtom for handling window resize as it
 * happens.
 *
 * Once this event has been called, we can confirm the window has been resize
 * get rid of the current resize rectangle.
 *
 */

void
GroupWindow::resizeNotify (int dx,
			   int dy,
			   int dwidth,
			   int dheight)
{
    GROUP_SCREEN (screen);

    GLTexture::Matrix mat = gs->glowTexture.texture.front ()->matrix ();

    if (!resizeGeometry.isEmpty ())
    {
	resizeGeometry.setX (0);
	resizeGeometry.setY (0);
	resizeGeometry.setHeight (0);
	resizeGeometry.setWidth (0);
    }

    window->resizeNotify (dx, dy, dwidth, dheight);


    if (glowQuads)
	computeGlowQuads (&mat);


    if (group && group->tabBar && IS_TOP_TAB (window, group))
    {
	if (group->tabBar->state != Layer::PaintOff)
	{
	    group->tabBar->recalcPos (pointerX, WIN_X (window),
	                              WIN_X (window) + WIN_WIDTH (window));
	}
    }
}

/*
 * GroupWindow::moveNotify ()
 *
 * Description: This function catches moveNotify on the wrappable function chain
 * and handles if the move was a result of a viewport change or a window
 * being moved. If the group is tabbed and a tab bar is visible, the tab
 * bar is also moved and spring friction is applied.
 *
 * Other windows are not moved straight away - moving other windows while
 * in the middle of a moveNotify change causes weirdness since the chain is
 * not fully wrapped. Rather they are 'enqueued' using a 0-time timer
 * which will be executed on the next handleEvent (). This means there is a
 * slight (one-pass) delay for the other windows to be moved, but there will
 * also be no weirdness.
 */

void
GroupWindow::moveNotify (int  dx,
			 int  dy,
			 bool immediate)
{
    bool       viewportChange;
    GLTexture::Matrix mat;

    GROUP_SCREEN (screen);
    
    mat = gs->glowTexture.texture.front ()->matrix ();

    /* Pass the call chain to other plugins */
    window->moveNotify (dx, dy, immediate);

    if (glowQuads)
	computeGlowQuads (&mat);

    /* Skip any further enqueueing if a queue is already in place. This will
     * prevent us from acting on the moveNotify that will come out of moving
     * the other windows */

    if (!group || gs->queued)
    {
	return;
    }

    /* FIXME: we need a reliable, 100% safe way to detect window
       moves caused by viewport changes here */
    viewportChange = ((dx && !(dx % screen->width ())) ||
		      (dy && !(dy % screen->height ())));

    if (viewportChange && (animateState & IS_ANIMATED))
    {
	destination.setX (destination.x () + dx);
	destination.setY (destination.y () + dy);
    }

    if (group->tabBar && IS_TOP_TAB (window, group))
    {
	group->tabBar->rightSpringX += dx;
	group->tabBar->leftSpringX += dx;

	group->tabBar->moveRegion (dx, dy, true);

	foreach (Tab *tab, *group->tabBar)
	{
	    tab->region.translate (dx, dy);
	    tab->springX += dx;
	}
    }


    /* Do not enqueue windows if:
     * - we have not specified to move all windows
     * - windows are currently being tabbed into or out of a group
     * - the grabed window is not grabbed for moving */

    if ((!gs->optionGetMoveAll () || gs->ignoreMode) ||
	(group->tabBar && group->tabBar->tabbingState != TabBar::NoTabbing) ||
	(group->grabWindow != window->id () ||
	!(group->grabMask & CompWindowGrabMoveMask)))
    {
	return;
    }
    foreach (CompWindow *cw, group->windows)
    {
	if (!cw)
	    continue;

	if (cw->id () == window->id ())
	    continue;

	if (cw->state () & MAXIMIZE_STATE)
	{
	    if (viewportChange)
		gs->enqueueMoveNotify (cw, dx, dy, immediate, true);
	}
	else if (!viewportChange)
	{
	    needsPosSync = true;
	    gs->enqueueMoveNotify (cw, dx, dy, immediate, true);
	}
    }
}

/*
 * GroupWindow::grabNotify ()
 *
 * Description: This function catches when a window is grabbed by the cursor
 * as reported by other plugins. Grabs for other windows must be enqueued as
 * per the reasons above.
 *
 * Since we begin resizing when a window is grabbed, we must determine
 * if we are resizing here and if so do some initial setup for when
 * we get atom change events for resize in the X Server.
 */

void
GroupWindow::grabNotify (int          x,
			 int	      y,
			 unsigned int state,
			 unsigned int mask)
{
    GROUP_SCREEN (screen);

    if (group && !gs->ignoreMode && !gs->queued)
    {
	bool doResizeAll;

	doResizeAll = gs->optionGetResizeAll () &&
	              (mask & CompWindowGrabResizeMask);


	if (group->tabBar)
	    group->tabBar->setVisibility (false, 0);


	foreach (CompWindow *cw, group->windows)
	{
	    if (!cw)
		continue;

	    if (cw->id () != window->id ())
	    {
		GroupWindow *gcw = GroupWindow::get (cw);

		gs->enqueueGrabNotify (cw, x, y, state, mask);

		if (doResizeAll && !(cw->state () & MAXIMIZE_STATE))
		{

		    if (gcw->resizeGeometry.isEmpty ())
		    {
			gcw->resizeGeometry.setX (WIN_X (cw));
			gcw->resizeGeometry.setY (WIN_Y (cw));
			gcw->resizeGeometry.setWidth (WIN_WIDTH (cw));
			gcw->resizeGeometry.setHeight (WIN_HEIGHT (cw));
		    }
		}
	    }
	}

	if (doResizeAll)
	{
	    if (!gs->resizeInfo)
		gs->resizeInfo = new GroupScreen::GroupResizeInfo;

	    if (gs->resizeInfo)
	    {
		gs->resizeInfo->resizedWindow       = window;
		gs->resizeInfo->origGeometry.setX (WIN_X (window));
		gs->resizeInfo->origGeometry.setY (WIN_Y (window));
		gs->resizeInfo->origGeometry.setWidth (WIN_WIDTH (window));
		gs->resizeInfo->origGeometry.setHeight (WIN_HEIGHT (window));
	    }
	}

	group->grabWindow = window->id ();
	group->grabMask = mask;
    }

    window->grabNotify (x, y, state, mask);
}

/*
 * GroupWindow::ungrabNotify ()
 *
 * Description: This function catches when a window is ungrabbed by the cursor
 * as reported by other plugins. Ungrabs for other windows must be enqueued as
 * per the reasons above.
 *
 * If we started resizing in grabNotify (as noted by the presence of
 * resizeGeometry), then apply those resize changes to the windows
 * being resized.
 */

void
GroupWindow::ungrabNotify ()
{
    GROUP_SCREEN (screen);

    if (group && !gs->ignoreMode && !gs->queued)
    {
	CompRect   rect;

	gs->dequeueMoveNotifies ();

	if (gs->resizeInfo)
	{
	    rect.setX (WIN_X (window));
	    rect.setY (WIN_Y (window));
	    rect.setWidth (WIN_WIDTH (window));
	    rect.setHeight (WIN_HEIGHT (window));
	}


	foreach (CompWindow *cw, group->windows)
	{
	    if (!cw)
		continue;

	    if (cw->id () != window->id ())
	    {
		GROUP_WINDOW (cw);

		if (!gw->resizeGeometry.isEmpty ())
		{
		    unsigned int mask;

		    gw->resizeGeometry.setX (WIN_X (cw));
		    gw->resizeGeometry.setY (WIN_Y (cw));
		    gw->resizeGeometry.setWidth (WIN_WIDTH (cw));
		    gw->resizeGeometry.setHeight (WIN_HEIGHT (cw));

		    mask = gw->updateResizeRectangle (rect, false);
		    if (mask)
		    {
			XWindowChanges xwc;
			xwc.x      = gw->resizeGeometry.x ();
			xwc.y      = gw->resizeGeometry.y ();
			xwc.width  = gw->resizeGeometry.width ();
			xwc.height = gw->resizeGeometry.height ();

			if (window->mapNum () && (mask & (CWWidth | CWHeight)))
			    window->sendSyncRequest ();

			cw->configureXWindow (mask, &xwc);
		    }

		    gw->resizeGeometry.setX (0);
		    gw->resizeGeometry.setY (0);
		    gw->resizeGeometry.setWidth (0);
		    gw->resizeGeometry.setHeight (0);
		}

		if (gw->needsPosSync)
		{
		    cw->syncPosition ();
		    gw->needsPosSync = false;
		}


		gs->enqueueUngrabNotify (cw);


	    }
	}

	if (gs->resizeInfo)
	{
	    delete gs->resizeInfo;
	    gs->resizeInfo = NULL;
	}


	group->grabWindow = None;
	group->grabMask = 0;
    }

    window->ungrabNotify ();
}

/*
 * GroupWindow::getOutputExents
 *
 * Description: Our glow is painted outside the window texture (see
 * addWindowGeometry. This wrapped function overrides the current output
 * extents (w->output and w->outputRect) to include to glow, such that it
 * will be damaged correctly by other plugins
 *
 */
void
GroupWindow::getOutputExtents (CompWindowExtents &output)
{
    GROUP_SCREEN (screen);
    
    window->getOutputExtents (output);

    if (group && !group->windows.empty ())
    {

	int glowSize = gs->optionGetGlowSize ();
	int glowType = gs->optionGetGlowType ();
	int glowTextureSize = gs->glowTexture.properties[glowType].textureSize;
	int glowOffset = gs->glowTexture.properties[glowType].glowOffset;

	glowSize = glowSize * (glowTextureSize - glowOffset) / glowTextureSize;

	/* glowSize is the size of the glow outside the window decoration
	 * (w->input), while w->output includes the size of w->input
	 * this is why we have to add w->input here */
	output.left   = MAX (output.left, glowSize + window->input ().left);
	output.right  = MAX (output.right, glowSize + window->input ().right);
	output.top    = MAX (output.top, glowSize + window->input ().top);
	output.bottom = MAX (output.bottom, glowSize + window->input ().bottom);
    }
}

/* GroupWindow::activate ()
 *
 * Change tab on a window activation
 *
 */
void
GroupWindow::activate ()
{
    if (group && group->tabBar && !IS_TOP_TAB (window, group))
	group->tabBar->changeTab (tab, TabBar::RotateUncertain);

    window->activate ();
}

/*
 * GroupWindow::deleteGroupWindow ()
 *
 * Description: This function cleans up all the allocated memory and pointers
 * that may have been created while this window was in a group. Not to be
 * confused with removeFromGroup () which actually removes the window
 * from any group and serves as a wrapper function around this.
 *
 */

void
GroupWindow::deleteGroupWindow ()
{
    GROUP_SCREEN (screen);

    if (!group)
	return;

    if (group->tabBar && tab)
    {
	/* Are we the dragged slot ? */
	if (gs->draggedSlot && gs->dragged &&
	    gs->draggedSlot->window->id () == window->id ())
	{
	    group->tabBar->unhookTab (tab, false);
	}
	else
	{
	    delete tab;
	    tab = NULL;
	}
    }

    if (!group->windows.empty ())
    {
	if (group->windows.size () > 1)
	{
	    group->windows.remove (window);

	    if (group->windows.size () == 1)
	    {
		/* Glow was removed from this window, too */
		CompositeWindow::get (group->windows.front ())->damageOutputExtents ();
		group->windows.front ()->updateWindowOutputExtents ();

		if (gs->optionGetAutoUngroup ())
		{

		    if (group->tabBar &&
		    	group->tabBar->changeState != TabBar::NoTabChange)
		    {
			/* a change animation is pending: this most
			   likely means that a window must be moved
			   back onscreen, so we do that here */
			CompWindow *lw = group->windows.front ();

			GroupWindow::get (lw)->setVisibility (true);
		    }

		    if (!gs->optionGetAutotabCreate ())
		    {
			group->destroy (false);
			group = NULL;
		    }
		}
	    }
	}
	else
	{
	    fprintf (stderr, "destroying group\n");
	    group->destroy (false);
	    group = NULL;
	}

	window->updateWindowOutputExtents ();
	cWindow->damageOutputExtents ();
	updateProperty ();

	group = NULL;
    }
}

/*
 * GroupWindow::removeFromGroup ()
 *
 */
void
GroupWindow::removeFromGroup ()
{
    if (!group)
	return;

    if (group->tabBar && !(animateState & IS_UNGROUPING) &&
	(group->windows.size () > 1))
    {
	/* if the group is tabbed, setup untabbing animation. The
	   window will be deleted from the group at the
	   end of the untabbing. */

	if (HAS_TOP_WIN (group))
	{
	    CompWindow *tw = TOP_TAB (group);
	    int        oldX = orgPos.x ();
	    int        oldY = orgPos.y ();

	    orgPos.setX (WIN_CENTER_X (tw) - (WIN_WIDTH (window) / 2));
	    orgPos.setY (WIN_CENTER_Y (tw) - (WIN_HEIGHT (window) / 2));

	    destination.setX (orgPos.x () + mainTabOffset.x ());
	    destination.setY (orgPos.y () + mainTabOffset.y ());

	    mainTabOffset.setX (oldX);
	    mainTabOffset.setY (oldY);

	    if (tx || ty)
	    {
		tx -= (orgPos.x () - oldX);
		ty -= (orgPos.y () - oldY);
	    }

	    animateState = IS_ANIMATED;
	    xVelocity = yVelocity = 0.0f;
	}

	/* Although when there is no top-tab, it will never really
	   animate anything, if we don't start the animation,
	   the window will never get removed. */
	group->startTabbingAnimation (false);

	GroupWindow::get (window)->setVisibility (true);
	group->ungroupState = Group::UngroupSingle;
	animateState |= IS_UNGROUPING;
    }
    else
    {
	/* no tab bar - delete immediately */
	deleteGroupWindow ();

	if (GroupScreen::get (screen)->optionGetAutotabCreate () && isGroupable ())
	{
	    Selection sel;
	
	    sel.push_back (window);
	
	    group = sel.toGroup ();
	    if (group)
	        group->tab (window);
	}
    }
}

/*
 * GroupWindow::glPaint ()
 *
 * Description: Adjust window paint parameters. We need to account for animations
 * such as tab into / untab and rotation between tabs. This uses
 * some booleans to determine where to enter:
 *
 * doRotate: we should rotate this window as it is being tabbed
 * doTabbing: this window is being tabbed into / out of a group
 * showTabbar: we should draw the Tab Bar on this window texture
 * inSelection: we should dim this window to show it is being selected for
 * grouping
 * !resizeGeometry.isEmpty () : we should stretch this window because it is
 * being resized with its group
 */

bool
GroupWindow::glPaint (const GLWindowPaintAttrib &attrib,
		      const GLMatrix		&transform,
		      const CompRegion		&region,
		      unsigned int		mask)
{
    bool       status;
    bool       doRotate, doTabbing, showTabbar;

    GROUP_SCREEN (screen);

    if (group)
    {
	doRotate = group->tabBar && 
		    (group->tabBar->changeState != TabBar::NoTabChange) &&
	           HAS_TOP_WIN (group) && HAS_PREV_TOP_WIN (group) &&
	           (IS_TOP_TAB (window, group) ||
	            IS_PREV_TOP_TAB (window, group));

	doTabbing = group->tabBar && 
		    (animateState & (IS_ANIMATED | FINISHED_ANIMATION)) &&
	            !(IS_TOP_TAB (window, group) &&
		      (group->tabBar->tabbingState == TabBar::Tabbing));

	showTabbar = group->tabBar &&
		     (group->tabBar->state != Layer::PaintOff) &&
	             (((IS_TOP_TAB (window, group)) &&
		       ((group->tabBar->changeState == TabBar::NoTabChange) ||
			(group->tabBar->changeState == TabBar::TabChangeNewIn))) ||
		      (IS_PREV_TOP_TAB (window, group) &&
		       (group->tabBar->changeState == TabBar::TabChangeOldOut)));
    }
    else
    {
	doRotate   = false;
	doTabbing  = false;
	showTabbar = false;
    }

    if (windowHideInfo)
    {
	mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;
    }

    if (inSelection || !resizeGeometry.isEmpty () || doRotate ||
	doTabbing || showTabbar)
    {
	GLWindowPaintAttrib wAttrib = attrib;
	GLMatrix            wTransform = transform;
	float               animProgress = 0.0f;
	int                 drawnPosX = 0, drawnPosY = 0;

	if (inSelection)
	{
	    wAttrib.opacity    = OPAQUE * gs->optionGetSelectOpacity () / 100;
	    wAttrib.saturation = COLOR * gs->optionGetSelectSaturation () / 100;
	    wAttrib.brightness = BRIGHT * gs->optionGetSelectBrightness () / 100;
	}

	if (doTabbing)
	{
	    /* fade the window out */
	    float progress;
	    int   distanceX, distanceY;
	    float origDistance, distance;

	    if (animateState & FINISHED_ANIMATION)
	    {
		drawnPosX = destination.x ();
		drawnPosY = destination.y ();
	    }
	    else
	    {
		drawnPosX = orgPos.x () + tx;
		drawnPosY = orgPos.y () + ty;
	    }

	    distanceX = drawnPosX - destination.x ();
	    distanceY = drawnPosY - destination.y ();
	    distance = sqrt (pow (distanceX, 2) + pow (distanceY, 2));

	    distanceX = (orgPos.x () - destination.x ());
	    distanceY = (orgPos.y () - destination.y ());
	    origDistance = sqrt (pow (distanceX, 2) + pow (distanceY, 2));

	    if (!distanceX && !distanceY)
		progress = 1.0f;
	    else
		progress = 1.0f - (distance / origDistance);

	    animProgress = progress;

	    progress = MAX (progress, 0.0f);
	    if (group->tabBar->tabbingState == TabBar::Tabbing)
		progress = 1.0f - progress;

	    wAttrib.opacity = (float) wAttrib.opacity * progress;
	}

	if (doRotate)
	{
	    float timeLeft = group->tabBar->changeAnimationTime;
	    int   animTime = gs->optionGetChangeAnimationTime () * 500;

	    if (group->tabBar->changeState == TabBar::TabChangeOldOut)
		timeLeft += animTime;

	    /* 0 at the beginning, 1 at the end */
	    animProgress = 1 - (timeLeft / (2 * animTime));
	}

	if (!resizeGeometry.isEmpty ())
	{
	    int      xOrigin, yOrigin;
	    float    xScale, yScale;
	    CompRect box;

	    getStretchRectangle (box, &xScale, &yScale);

	    xOrigin = window->x ()- window->input ().left;
	    yOrigin = window->y () - window->input ().top;

	    wTransform.translate (xOrigin, yOrigin, 0.0f);
	    wTransform.scale (xScale, yScale, 1.0f);
	    wTransform.translate ((resizeGeometry.x () - window->x ()) /
			     	   xScale - xOrigin,
				  (resizeGeometry.y () - window->y ()) /
			     	   yScale - yOrigin,
			     	   0.0f);

	    mask |= PAINT_WINDOW_TRANSFORMED_MASK;
	}

	else if (doRotate || doTabbing)
	{
	    float      animWidth, animHeight;
	    float      animScaleX, animScaleY;
	    CompWindow *morphBase, *morphTarget;

	    if (doTabbing)
	    {
		if (group->tabBar->tabbingState == TabBar::Tabbing)
		{
		    morphBase   = window;
		    morphTarget = TOP_TAB (group);
		}
		else
		{
		    morphTarget = window;
		    if (HAS_TOP_WIN (group))
			morphBase = TOP_TAB (group);
		    else
			morphBase = group->tabBar->lastTopTab;
		}
	    }
	    else
	    {
		morphBase   = PREV_TOP_TAB (group);
		morphTarget = TOP_TAB (group);
	    }

	    animWidth = (1 - animProgress) * WIN_REAL_WIDTH (morphBase) +
		        animProgress * WIN_REAL_WIDTH (morphTarget);
	    animHeight = (1 - animProgress) * WIN_REAL_HEIGHT (morphBase) +
		         animProgress * WIN_REAL_HEIGHT (morphTarget);

	    animWidth = MAX (1.0f, animWidth);
	    animHeight = MAX (1.0f, animHeight);
	    animScaleX = animWidth / WIN_REAL_WIDTH (window);
	    animScaleY = animHeight / WIN_REAL_HEIGHT (window);

	    if (doRotate)
		wTransform.scale (1.0f, 1.0f, 1.0f / screen->width ());

	    wTransform.translate (WIN_REAL_X (window) +
	    			   WIN_REAL_WIDTH (window) / 2.0f,
			          WIN_REAL_Y (window) +
			           WIN_REAL_HEIGHT (window) / 2.0f,
			          0.0f);

	    if (doRotate)
	    {
		float rotateAngle = animProgress * 180.0f;
		if (IS_TOP_TAB (window, group))
		    rotateAngle += 180.0f;

		if (group->tabBar->changeAnimationDirection < 0)
		    rotateAngle *= -1.0f;

		wTransform.rotate (rotateAngle, 0.0f, 1.0f, 0.0f);
	    }

	    if (doTabbing)
		wTransform.translate (drawnPosX - WIN_X (window),
				      drawnPosY - WIN_Y (window), 0.0f);

	    wTransform.scale (animScaleX, animScaleY, 1.0f);

	    wTransform.translate (-(WIN_REAL_X (window) +
	    			     WIN_REAL_WIDTH (window) / 2.0f),
				  -(WIN_REAL_Y (window) +
				     WIN_REAL_HEIGHT (window) / 2.0f),
				  0.0f);

	    mask |= PAINT_WINDOW_TRANSFORMED_MASK;
	}


	status = gWindow->glPaint (wAttrib, wTransform, region, mask);


	if (showTabbar)
	    group->tabBar->draw (wAttrib, wTransform, mask, region);

    }
    else
	status = gWindow->glPaint (attrib, transform, region, mask);

    return status;
}

/*
 * GroupWindow::glDraw
 *
 * This adds the Glow geometry and paints it
 *
 */
bool
GroupWindow::glDraw (const GLMatrix &transform,
		     GLFragment::Attrib &attrib,
		     const CompRegion	&region,
		     unsigned int mask)
{
    GROUP_SCREEN (screen);

    bool status;
    CompRegion newRegion (region);

    if (group && (!group->windows.empty () && glowQuads))
    {
	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    newRegion = infiniteRegion;

	if (!newRegion.rects ().empty ())
	{
	    int    i;

	    gWindow->geometry ().reset ();

	    for (i = 0; i < NUM_GLOWQUADS; i++)
	    {
		CompRegion box (glowQuads[i].box);
		
		if (box.boundingRect ().x1 () < box.boundingRect ().x2 () &&
		    box.boundingRect ().y1 () < box.boundingRect ().y2 ())
		{
		    GLTexture::MatrixList matricies;
		    
		    matricies.push_back (glowQuads[i].matrix);
		
		    gWindow->glAddGeometry (matricies, box, newRegion);
		}
	    }

	    if (gWindow->geometry ().vertices)
	    {
		GLFragment::Attrib fAttrib = attrib;
		GLushort       average;
		GLMatrix       gTransform (transform);
		GLushort       color[3] = {group->color[0],
		                           group->color[1],
		                           group->color[2]};

		/* Apply brightness to color. */
		color[0] *= (float)attrib.getBrightness () / BRIGHT;
		color[1] *= (float)attrib.getBrightness () / BRIGHT;
		color[2] *= (float)attrib.getBrightness () / BRIGHT;

		/* Apply saturation to color. */
		average = (color[0] + color[1] + color[2]) / 3;
		color[0] = average + (color[0] - average) *
		           attrib.getSaturation () / COLOR;
		color[1] = average + (color[1] - average) *
		           attrib.getSaturation () / COLOR;
		color[2] = average + (color[2] - average) *
		           attrib.getSaturation () / COLOR;

		fAttrib.setOpacity (OPAQUE);
		fAttrib.setSaturation (COLOR);
		fAttrib.setBrightness (BRIGHT);

		gs->gScreen->setTexEnvMode (GL_MODULATE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4us (color[0], color[1], color[2], attrib.getOpacity ());

		/* we use PAINT_WINDOW_TRANSFORMED_MASK here to force
		    the usage of a good texture filter */

		gWindow->glDrawTexture (gs->glowTexture.texture[0], fAttrib,
					mask | PAINT_WINDOW_BLEND_MASK |
					PAINT_WINDOW_TRANSLUCENT_MASK |
					PAINT_WINDOW_TRANSFORMED_MASK);

		glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		gs->gScreen->setTexEnvMode (GL_REPLACE);
		glColor4usv (defaultColor);
	    }
	}
    }

    status = gWindow->glDraw (transform, attrib, newRegion, mask);

    return status;
}


/*
 * GroupWindow::damageRect ()
 *
 * Description: Applies window damage / updates
 *
 * If initial is true and we should auto-tab, then automatically create
 * a window group and tab it
 *
 */

bool
GroupWindow::damageRect (bool		initial,
			 const CompRect &rect)
{
    bool       status;

    GROUP_SCREEN (screen);

    status = cWindow->damageRect (initial, rect);

    if (initial)
    {
	if (gs->optionGetAutotabCreate () && isGroupable ())
	{
	    if (!group && (windowState == WindowNormal))
	    {
                Selection sel;
                Group     *group;

                sel.push_back (window);

                group = sel.toGroup ();
                
                if (group)
                    group->tab (window);
	    }
	}
    }

    if (!resizeGeometry.isEmpty ())
    {
	CompRect box;

	getStretchRectangle (box, NULL, NULL);
	gs->damagePaintRectangle (box);
    }

    if (tab)
    {
	int    vx = 0, vy = 0;
	CompRegion reg = emptyRegion;

	tab->getDrawOffset (vx, vy);
	if (vx || vy)
	{
	    reg = reg.united (tab->region);
	    reg.translate (vx, vy);
	}
	else
	    reg = tab->region;

	gs->cScreen->damageRegion (reg);
    }

    return status;
}


/*
 * GroupWindow::isGroupable ()
 *
 * Description: Should we group this window
 *
 */

bool
GroupWindow::isGroupable ()
{
    if (window->overrideRedirect ())
	return false;

    if (window->type () & CompWindowTypeDesktopMask)
	return false;

    if (window->invisible ())
	return false;

    if (!GroupScreen::get (screen)->optionGetWindowMatch ().evaluate (window))
	return false;

    return true;
}
