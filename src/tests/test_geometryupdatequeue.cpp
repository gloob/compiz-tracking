/*
 * Copyright © 2012 Sam Spilsbury
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Canonical Ltd. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Canonical Ltd. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * CANONICAL, LTD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL CANONICAL, LTD. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authored by: Sam Spilsbury <smspillaz@gmail.com>
 */
#include <deque>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <X11/Xlib.h>

#include "geometryupdatequeue.h"
#include "asyncserverwindow.h"

namespace crb = compiz::window::configure_buffers;
namespace cw = compiz::window;

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::Invoke;

class MockAsyncServerWindow :
    public cw::AsyncServerWindow
{
    public:

	MOCK_CONST_METHOD2 (Configure, int (const XWindowChanges &, unsigned int));
	MOCK_CONST_METHOD0 (HasCustomShape, bool ());
};

namespace
{
int REQUEST_X = 1;
int REQUEST_Y = 2;
int REQUEST_WIDTH = 3;
int REQUEST_HEIGHT = 4;
int REQUEST_BORDER = 5;

Window REQUEST_ABOVE = 6;
unsigned int REQUEST_MODE = 7;

crb::BufferLock::Ptr
CreateNormalLock (crb::CountedFreeze *cf)
{
    return boost::make_shared <crb::ConfigureBufferLock> (cf);
}

}

MATCHER_P2 (MaskXWC, xwc, vm, "Matches XWindowChanges")
{
    if (vm & CWX)
	if (xwc.x != arg.x)
	    return false;

    if (vm & CWY)
	if (xwc.y != arg.y)
	    return false;

    if (vm & CWWidth)
	if (xwc.width != arg.width)
	    return false;

    if (vm & CWHeight)
	if (xwc.height != arg.height)
	    return false;

    if (vm & CWBorderWidth)
	if (xwc.border_width != arg.border_width)
	    return false;

    if (vm & CWStackMode)
	if (xwc.stack_mode != arg.stack_mode)
	    return false;

    if (vm & CWSibling)
	if (xwc.sibling != arg.sibling)
	    return false;

    return true;
}

class ConfigureRequestBuffer :
    public testing::Test
{
    public:

	ConfigureRequestBuffer ()
	{
	    /* Initialize xwc, we control it
	     * through the value masks */
	    xwc.x = REQUEST_X;
	    xwc.y = REQUEST_Y;
	    xwc.width = REQUEST_WIDTH;
	    xwc.height = REQUEST_HEIGHT;
	    xwc.border_width = REQUEST_BORDER;
	    xwc.sibling = REQUEST_ABOVE;
	    xwc.stack_mode = REQUEST_MODE;
	}

    protected:

	XWindowChanges xwc;
	MockAsyncServerWindow       asyncServerWindow;
};

TEST_F (ConfigureRequestBuffer, PushDirectUpdate)
{
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    unsigned int   valueMask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth |
			       CWSibling | CWStackMode;

    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillOnce (Return (false));
    EXPECT_CALL (asyncServerWindow, Configure (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer.pushConfigureRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBuffer, PushUpdateLocked)
{
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    crb::Releasable::Ptr lock (buffer.obtainLock ());
;
    unsigned int   valueMask = 0;

    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    buffer.pushConfigureRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBuffer, PushCombinedUpdateLocked)
{
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    crb::Releasable::Ptr lock (buffer.obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    buffer.pushConfigureRequest (xwc, valueMask);

    valueMask |= CWWidth | CWHeight;

    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillRepeatedly (Return (false));
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    buffer.pushConfigureRequest (xwc, valueMask);

    EXPECT_CALL (asyncServerWindow, Configure (MaskXWC (xwc, valueMask),
					       valueMask));

    lock->release ();
}

TEST_F (ConfigureRequestBuffer, UnlockBuffer)
{
    MockAsyncServerWindow       asyncServerWindow;
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    crb::Releasable::Ptr lock (buffer.obtainLock ());

    unsigned int   valueMask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;

    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillRepeatedly (Return (false));
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    buffer.pushConfigureRequest (xwc, valueMask);

    EXPECT_CALL (asyncServerWindow, Configure (MaskXWC (xwc, valueMask),
					       valueMask));

    lock->release ();
}

TEST_F (ConfigureRequestBuffer, ForceImmediateConfigureOnRestack)
{
    MockAsyncServerWindow       asyncServerWindow;
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    crb::Releasable::Ptr lock (buffer.obtainLock ());

    unsigned int   valueMask = CWStackMode | CWSibling;

    EXPECT_CALL (asyncServerWindow, Configure (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer.pushConfigureRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBuffer, ForceImmediateConfigureOnShapedWindowSizeChange)
{
    MockAsyncServerWindow       asyncServerWindow;
    crb::ConfigureRequestBuffer::LockFactory factory (
		boost::bind (CreateNormalLock, _1));
    crb::ConfigureRequestBuffer buffer (&asyncServerWindow, factory);

    crb::Releasable::Ptr lock (buffer.obtainLock ());

    unsigned int   valueMask = CWWidth | CWHeight;

    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillOnce (Return (true));
    EXPECT_CALL (asyncServerWindow, Configure (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer.pushConfigureRequest (xwc, valueMask);
}

namespace
{
class MockLock :
    public crb::BufferLock
{
    public:

	void OperateOver (crb::CountedFreeze *cf)
	{
	    ON_CALL (*this, lock ()).WillByDefault (
			Invoke (cf, &crb::CountedFreeze::freeze));
	    ON_CALL (*this, release ()).WillByDefault (
			Invoke (cf, &crb::CountedFreeze::release));
	}

	typedef boost::shared_ptr <MockLock> Ptr;

	MOCK_METHOD0 (lock, void ());
	MOCK_METHOD0 (release, void ());

    private:

	crb::CountedFreeze *countedFreeze;
};

class MockLockFactory
{
    public:

	crb::BufferLock::Ptr
	CreateMockLock (crb::CountedFreeze  *cf)
	{
	    MockLock::Ptr mockLock (locks.front ());
	    mockLock->OperateOver (cf);

	    locks.pop_front ();

	    return mockLock;
	}

	void
	QueueLockForCreation (const MockLock::Ptr &lock)
	{
	    locks.push_back (lock);
	}

    private:

	std::deque <MockLock::Ptr> locks;
};
}

TEST_F (ConfigureRequestBuffer, RearmBufferLockOnRelease)
{
    typedef NiceMock <MockAsyncServerWindow> NiceServerWindow;
    typedef crb::ConfigureRequestBuffer::LockFactory LockFactory;

    NiceServerWindow asyncServerWindow;
    MockLock::Ptr    lock (boost::make_shared <MockLock> ());
    MockLockFactory  mockLockFactory;

    mockLockFactory.QueueLockForCreation (lock);

    LockFactory      factory (
	boost::bind (&MockLockFactory::CreateMockLock, &mockLockFactory, _1));

    crb::ConfigureRequestBuffer buffer (&asyncServerWindow,
					factory);

    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer.obtainLock ());

    unsigned int   valueMask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;

    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillRepeatedly (Return (false));
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    buffer.pushConfigureRequest (xwc, valueMask);

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* Now the buffer will dispatch is configure request */
    EXPECT_CALL (asyncServerWindow, Configure (_, _));

    /* Rearm locks on release */
    EXPECT_CALL (*lock, lock ());

    /* Directly release the queue */
    releasable->release ();
}

TEST_F (ConfigureRequestBuffer, NoRearmBufferLockNoReleaseRequired)
{
    typedef NiceMock <MockAsyncServerWindow> NiceServerWindow;
    typedef crb::ConfigureRequestBuffer::LockFactory LockFactory;

    NiceServerWindow asyncServerWindow;
    MockLock::Ptr    lock (boost::make_shared <MockLock> ());
    MockLockFactory  mockLockFactory;

    mockLockFactory.QueueLockForCreation (lock);

    LockFactory      factory (
	boost::bind (&MockLockFactory::CreateMockLock, &mockLockFactory, _1));

    crb::ConfigureRequestBuffer buffer (&asyncServerWindow,
					factory);

    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer.obtainLock ());

    /* No call to Configure if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No rearm - we haven't released the whole buffer */
    EXPECT_CALL (*lock, lock ()).Times (0);

    /* Directly release the queue */
    releasable->release ();
}

TEST_F (ConfigureRequestBuffer, RearmWhenPushReady)
{
    typedef NiceMock <MockAsyncServerWindow> NiceServerWindow;
    typedef crb::ConfigureRequestBuffer::LockFactory LockFactory;

    NiceServerWindow asyncServerWindow;
    MockLock::Ptr    lock (boost::make_shared <MockLock> ());
    MockLockFactory  mockLockFactory;

    mockLockFactory.QueueLockForCreation (lock);

    LockFactory      factory (
	boost::bind (&MockLockFactory::CreateMockLock, &mockLockFactory, _1));

    crb::ConfigureRequestBuffer buffer (&asyncServerWindow,
					factory);

    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer.obtainLock ());

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No call to Configure if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    /* No rearm - we haven't released it */
    EXPECT_CALL (*lock, lock ()).Times (0);

    /* Directly release the queue */
    releasable->release ();

    /* Since we're now going to push something to a queue
     * that's effectively not locked, the locks should now
     * be released */
    unsigned int   valueMask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;

    /* Now rearm it */
    EXPECT_CALL (asyncServerWindow, HasCustomShape ()).WillOnce (Return (false));
    EXPECT_CALL (asyncServerWindow, Configure (_, _));
    EXPECT_CALL (*lock, lock ());

    buffer.pushConfigureRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBuffer, NoRearmBufferLockOnNoRelease)
{
    typedef NiceMock <MockAsyncServerWindow> NiceServerWindow;
    typedef crb::ConfigureRequestBuffer::LockFactory LockFactory;

    NiceServerWindow asyncServerWindow;
    MockLock::Ptr    lock (boost::make_shared <MockLock> ());
    MockLock::Ptr    second (boost::make_shared <MockLock> ());
    MockLockFactory  mockLockFactory;

    mockLockFactory.QueueLockForCreation (lock);
    mockLockFactory.QueueLockForCreation (second);

    LockFactory      factory (
	boost::bind (&MockLockFactory::CreateMockLock, &mockLockFactory, _1));

    crb::ConfigureRequestBuffer buffer (&asyncServerWindow,
					factory);

    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    EXPECT_CALL (*second, lock ());

    crb::Releasable::Ptr releasable (buffer.obtainLock ());
    crb::Releasable::Ptr otherReleasable (buffer.obtainLock ());

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No call to Configure if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, Configure (_, _)).Times (0);

    /* No rearm - we haven't released it */
    EXPECT_CALL (*lock, lock ()).Times (0);

    releasable->release ();
}
