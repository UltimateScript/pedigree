/*
 * Copyright (c) 2008-2014, Pedigree Developers
 *
 * Please see the CONTRIB file in the root of the source tree for a full
 * list of contributors.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <Widget.h>
#include <native/graphics/Graphics.h>
#include <native/types.h>

#include <iostream>

using namespace PedigreeGraphics;

class TestWidget : public Widget
{
  public:
    TestWidget(uint32_t rgb) : Widget(), m_Rgb(rgb){};
    virtual ~TestWidget()
    {
    }

    virtual bool render(Rect &rt, Rect &dirty)
    {
        std::cout << "uitest: rendering widget." << std::endl;

        void *pFramebuffer = getRawFramebuffer();
        uint32_t *buffer = (uint32_t *) pFramebuffer;
        for (size_t y = 0; y < rt.getH(); ++y)
        {
            for (size_t x = 0; x < rt.getW(); ++x)
                buffer[y * rt.getW() + x] = m_Rgb;
        }

        dirty.update(0, 0, rt.getW(), rt.getH());

        return true;
    }

  private:
    uint32_t m_Rgb;
};

volatile bool bRun = true;

bool callback(WidgetMessages message, size_t msgSize, const void *msgData)
{
    std::cout << "uitest: callback for '" << static_cast<int>(message) << "'."
              << std::endl;

    if (message == Terminate)
    {
        bRun = false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    std::cout << "uitest: starting up" << std::endl;

    Rect rt(20, 20, 20, 20);

    Widget *pWidgetA = new TestWidget(createRgb(0xFF, 0, 0));
    if (!pWidgetA->construct("uitest.A", "UI Test A", callback, rt))
    {
        std::cerr << "uitest: widget A construction failed" << std::endl;
        delete pWidgetA;
        return 1;
    }

    Widget *pWidgetB = new TestWidget(createRgb(0, 0xFF, 0));
    if (!pWidgetB->construct("uitest.B", "UI Test B", callback, rt))
    {
        std::cerr << "uitest: widget B construction failed" << std::endl;
        delete pWidgetA;
        delete pWidgetB;
        return 1;
    }

    std::cout << "uitest: widgets created (handles are "
              << pWidgetA->getHandle() << ", " << pWidgetB->getHandle() << ")."
              << std::endl;

    pWidgetA->visibility(true);
    pWidgetB->visibility(true);

    std::cout << "Widgets are visible!" << std::endl;

    Rect dirtyA, dirtyB;
    pWidgetA->render(rt, dirtyA);
    pWidgetB->render(rt, dirtyB);
    pWidgetA->redraw(dirtyA);
    pWidgetB->redraw(dirtyB);

    // Main loop
    while (bRun)
    {
        Widget::checkForEvents();
    }

    delete pWidgetB;
    delete pWidgetA;

    return 0;
}
