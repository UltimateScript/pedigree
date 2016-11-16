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

#include "Xterm.h"
#include "Font.h"
#include "Terminal.h"
#include <sys/klog.h>
#include <string.h>
#include <time.h>

#define XTERM_BOLD      0x1
#define XTERM_UNDERLINE 0x2
#define XTERM_INVERSE   0x4
#define XTERM_BRIGHTFG  0x8
#define XTERM_BRIGHTBG  0x10
#define XTERM_BORDER    0x20
#define XTERM_ITALIC    0x40

#define C_BLACK   0
#define C_RED     1
#define C_GREEN   2
#define C_YELLOW  3
#define C_BLUE    4
#define C_MAGENTA 5
#define C_CYAN    6
#define C_WHITE   7

#define C_BRIGHT  8

// Set to 1 to use Framebuffer::line instead of Unicode lines.
// Fairly buggy unfortunately.
#define USE_FRAMEBUFFER_LINES       1

#include "environment.h"

#include <native/config/Config.h>
#include <native/graphics/Graphics.h>

#include <cairo/cairo.h>

 // RGBA -> BGRA

extern uint32_t g_Colours[];
extern uint32_t g_BrightColours[];

uint8_t g_DefaultFg = 7;
uint8_t g_DefaultBg = 0;

bool g_FontsPrecached = false;

static void getXtermColorFromDb(const char *colorName, uint8_t &color)
{
    // Use defaults on Linux.
#ifndef TARGET_LINUX
    // The query string
    std::string sQuery;

    // Create the query string
    sQuery += "select r from 'colour_scheme' where name='";
    sQuery += colorName;
    sQuery += "';";

    // Query the database
    Config::Result *pResult = Config::query(sQuery.c_str());

    // Did the query fail?
    if(!pResult)
    {
        klog(LOG_ALERT, "TUI: Error looking up '%s' colour.", colorName);
        return;
    }
    if(!pResult->succeeded())
    {
        klog(LOG_ALERT, "TUI: Error looking up '%s' colour: %s\n", colorName, pResult->errorMessage().c_str());
        delete pResult;
        return;
    }

    // Get the color from the query result
    color = pResult->getNum(0, "r");

    // Dispose of the query result
    delete pResult;
#endif
}

Xterm::Xterm(PedigreeGraphics::Framebuffer *pFramebuffer, size_t nWidth, size_t nHeight, size_t offsetLeft, size_t offsetTop, Terminal *pT, class Widget *pWidget, Tui *pTui, Font *pNormalFont, Font *pBoldFont) :
    m_ActiveBuffer(0), m_Cmd(), m_OsCtl(), m_bChangingState(false), m_bContainedBracket(false),
    m_bContainedParen(false), m_bIsOsControl(false), m_Flags(0), m_Modes(0), m_TabStops(0),
    m_SavedX(0), m_SavedY(0), m_pT(pT), m_bFbMode(false), m_pWidget(pWidget), m_pTui(pTui)
{
    setFonts(pNormalFont, pBoldFont);

    size_t width = nWidth / m_pNormalFont->getWidth();
    size_t height = nHeight / m_pNormalFont->getHeight();

    m_pWindows[0] = new Window(height, width, pFramebuffer, 1000, offsetLeft, offsetTop, nWidth, this);
    m_pWindows[1] = new Window(height, width, pFramebuffer, 1000, offsetLeft, offsetTop, nWidth, this);

    getXtermColorFromDb("xterm-bg", g_DefaultBg);
    getXtermColorFromDb("xterm-fg", g_DefaultFg);

    // Set default tab stops.
    m_TabStops = new char[getStride()];
    memset(m_TabStops, 0, getStride());
    for(size_t i = 0; i < getStride(); i += 8)
    {
        m_TabStops[i] = '|';
    }

    // Set default modes.
    m_Modes |= AnsiVt52;

    m_Cmd.cur_param = 0;
    m_Cmd.has_param = false;

    m_OsCtl.cur_param = 0;
    m_OsCtl.has_param = false;
}

Xterm::~Xterm()
{
    delete [] m_TabStops;
    delete m_pWindows[0];
    delete m_pWindows[1];
}

void Xterm::processKey(uint64_t key)
{
    if (key & Keyboard::Special)
    {
        // Handle specially.
        uint32_t utf32 = key & ~0UL;
        char *str = reinterpret_cast<char*> (&utf32);
        if (!strncmp(str, "left", 4))
        {
            m_pT->addToQueue('\e');
            if((m_Modes & AnsiVt52) == AnsiVt52)
            {
                if(m_Modes & CursorKey)
                    m_pT->addToQueue('O');
                else
                    m_pT->addToQueue('[');
            }
            m_pT->addToQueue('D');
        }
        if (!strncmp(str, "righ", 4))
        {
            m_pT->addToQueue('\e');
            if((m_Modes & AnsiVt52) == AnsiVt52)
            {
                if(m_Modes & CursorKey)
                    m_pT->addToQueue('O');
                else
                    m_pT->addToQueue('[');
            }
            m_pT->addToQueue('C');
        }
        if (!strncmp(str, "up", 2))
        {
            m_pT->addToQueue('\e');
            if((m_Modes & AnsiVt52) == AnsiVt52)
            {
                if(m_Modes & CursorKey)
                    m_pT->addToQueue('O');
                else
                    m_pT->addToQueue('[');
            }
            m_pT->addToQueue('A');
        }
        if (!strncmp(str, "down", 4))
        {
            m_pT->addToQueue('\e');
            if((m_Modes & AnsiVt52) == AnsiVt52)
            {
                if(m_Modes & CursorKey)
                    m_pT->addToQueue('O');
                else
                    m_pT->addToQueue('[');
            }
            m_pT->addToQueue('B');
        }
    }
    else if (key & Keyboard::Alt)
    {
        // Xterm ALT escape = ESC <char>
        m_pT->addToQueue('\e');
        m_pT->addToQueue(key & 0x7F);
    }
    else if(key == '\n' || key == '\r')
    {
        m_pT->addToQueue('\r');
        if(m_Modes & LineFeedNewLine)
            m_pT->addToQueue('\n');
    }
    else
    {
        uint32_t utf32 = key&0xFFFFFFFF;

        // Begin Utf-32 -> Utf-8 conversion.
        char buf[4];
        size_t nbuf = 0;
        if (utf32 <= 0x7F)
        {
            buf[0] = utf32&0x7F;
            nbuf = 1;
        }
        else if (utf32 <= 0x7FF)
        {
            buf[0] = 0xC0 | ((utf32>>6) & 0x1F);
            buf[1] = 0x80 | (utf32 & 0x3F);
            nbuf = 2;
        }
        else if (utf32 <= 0xFFFF)
        {
            buf[0] = 0xE0 | ((utf32>>12) & 0x0F);
            buf[1] = 0x80 | ((utf32>>6) & 0x3F);
            buf[2] = 0x80 | (utf32 & 0x3F);
            nbuf = 3;
        }
        else if (utf32 <= 0x10FFFF)
        {
            buf[0] = 0xE0 | ((utf32>>18) & 0x07);
            buf[1] = 0x80 | ((utf32>>12) & 0x3F);
            buf[2] = 0x80 | ((utf32>>6) & 0x3F);
            buf[3] = 0x80 | (utf32 & 0x3F);
            nbuf = 4;
        }

        // End Utf-32 -> Utf-8 conversion.
        for (size_t i = 0; i < nbuf; i++)
            m_pT->addToQueue(buf[i]);
    }

    m_pT->addToQueue(0, true);
}

bool Xterm::setFlagsForUtf32(uint32_t utf32)
{
    size_t oldFlags = m_Flags;
    switch(utf32)
    {
        case '\e':
            m_Flags |= Escape;
            break;

        case '?':
            m_Flags |= Question;
            break;

        case ' ':
            m_Flags |= Space;
            break;

        case '#':
            m_Flags |= Hash;
            break;

        case '%':
            m_Flags |= Percent;
            break;

        case '(':
            m_Flags |= LeftRound;
            break;

        case ')':
            m_Flags |= RightRound;
            break;

        case '[':
            m_Flags |= LeftSquare;
            break;

        case ']':
            m_Flags |= RightSquare;
            break;

        case '_':
            m_Flags |= Underscore;
            break;

        case '/':
            m_Flags |= Slash;
            break;

        case '.':
            m_Flags |= Period;
            break;

        case '-':
            m_Flags |= Minus;
            break;

        case '+':
            m_Flags |= Plus;
            break;

        case '*':
            m_Flags |= Asterisk;
            break;

        case '\\':
            m_Flags |= Backslash;
            break;

        case '!':
            m_Flags |= Bang;
            break;

        case '\'':
            m_Flags |= Apostrophe;
            break;

        case '"':
            m_Flags |= Quote;
            break;

        case '$':
            m_Flags |= Dollar;
            break;

        default:
            break;
    }

    return oldFlags != m_Flags;
}

void Xterm::write(uint32_t utf32, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG_EXTRA
    klog(LOG_INFO, "XTerm::write(%c/%x)", (char) utf32, utf32);
#endif

    // Special cases, for controls that require readahead.
    if (m_Flags & Vt52SetCursorWaitY)
    {
        m_Cmd.params[0] = (char) utf32 - 32;
        m_Flags &= ~Vt52SetCursorWaitY;
        m_Flags |= Vt52SetCursorWaitX;
        return;
    }
    else if (m_Flags & Vt52SetCursorWaitX)
    {
        m_Cmd.params[1] = (char) utf32 - 32;
        m_pWindows[m_ActiveBuffer]->setCursor(m_Cmd.params[1], m_Cmd.params[0], rect);
        m_Flags = 0;
        return;
    }

    if(!m_Flags)
    {
        switch(utf32)
        {
            case 0x05:
                {
                    // Reply with answerback.
                    const char *answerback = "\e[1;2c";
                    while(*answerback)
                    {
                        m_pT->addToQueue(*answerback);
                        ++answerback;
                    }
                }
            case 0x08:
                m_pWindows[m_ActiveBuffer]->backspace(rect);
                break;

            case '\n':
            case 0x0B:
            case 0x0C:
                if(m_Modes & LineFeedNewLine)
                    m_pWindows[m_ActiveBuffer]->cursorDownAndLeftToMargin(rect);
                else
                    m_pWindows[m_ActiveBuffer]->cursorDown(1, rect);
                break;

            case '\t':
                m_pWindows[m_ActiveBuffer]->cursorTab(rect);
                break;

            case '\r':
                m_pWindows[m_ActiveBuffer]->cursorLeftToMargin(rect);
                break;

            case 0x0E:
            case 0x0F:
                // SHIFT-IN, SHIFT-OUT (character sets)
                break;

            case '\e':
                m_Flags = Escape;

                m_Cmd.cur_param = 0;
                m_Cmd.has_param = false;
                memset(m_Cmd.params, 0, sizeof(m_Cmd.params[0]) * XTERM_MAX_PARAMS);

                m_OsCtl.cur_param = 0;
                m_OsCtl.has_param = false;
                for(size_t i = 0; i < XTERM_MAX_PARAMS; ++i)
                {
                    m_OsCtl.params[i] = "";
                }
                break;

            default:
                // Handle line drawing.
                if(m_pWindows[m_ActiveBuffer]->getLineRenderMode())
                {
                    switch(utf32)
                    {
                        case 'j': utf32 = 0x2518; break; // Lower right corner
                        case 'k': utf32 = 0x2510; break; // Upper right corner
                        case 'l': utf32 = 0x250c; break; // Upper left corner
                        case 'm': utf32 = 0x2514; break; // Lower left corner
                        case 'n': utf32 = 0x253c; break; // Crossing lines.
                        case 'q': utf32 = 0x2500; break; // Horizontal line.
                        case 't': utf32 = 0x251c; break; // Left 'T'
                        case 'u': utf32 = 0x2524; break; // Right 'T'
                        case 'v': utf32 = 0x2534; break; // Bottom 'T'
                        case 'w': utf32 = 0x252c; break; // Top 'T'
                        case 'x': utf32 = 0x2502; break; // Vertical bar
                        default:
                        ;
                    }
                }

                // Printable character?
                if(utf32 >= 32)
                {
                    m_pWindows[m_ActiveBuffer]->addChar(utf32, rect);
                }
        }
    }
    else if((m_Flags & (Escape | LeftSquare | RightSquare | Underscore)) == (Escape | LeftSquare))
    {
        setFlagsForUtf32(utf32);

        switch(utf32)
        {
            case 0x08:
                // Backspace within control sequence.
                m_pWindows[m_ActiveBuffer]->backspace(rect);
                break;

            case '\n':
            case 0x0B:
                if(m_Modes & LineFeedNewLine)
                    m_pWindows[m_ActiveBuffer]->cursorLeftToMargin(rect);
                m_pWindows[m_ActiveBuffer]->cursorDown(1, rect);
                break;

            case '\r':
                m_pWindows[m_ActiveBuffer]->cursorLeftToMargin(rect);
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                m_Cmd.params[m_Cmd.cur_param] = m_Cmd.params[m_Cmd.cur_param] * 10 + (utf32-'0');
                m_Cmd.has_param = true;
                break;

            case ';':
            case ',':
                m_Cmd.cur_param++;
                break;

            case '@':
                m_pWindows[m_ActiveBuffer]->insertCharacters(m_Cmd.params[0] ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'A':
                if (m_Cmd.has_param && m_Cmd.params[0])
                    m_pWindows[m_ActiveBuffer]->cursorUpWithinMargin(m_Cmd.params[0], rect);
                else
                    m_pWindows[m_ActiveBuffer]->cursorUpWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'B':
                if (m_Cmd.has_param && m_Cmd.params[0])
                    m_pWindows[m_ActiveBuffer]->cursorDownWithinMargin(m_Cmd.params[0], rect);
                else
                    m_pWindows[m_ActiveBuffer]->cursorDownWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'C':
                if (m_Cmd.has_param && m_Cmd.params[0])
                    m_pWindows[m_ActiveBuffer]->cursorRightWithinMargin(m_Cmd.params[0], rect);
                else
                    m_pWindows[m_ActiveBuffer]->cursorRightWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'D':
                if (m_Cmd.has_param && m_Cmd.params[0])
                    m_pWindows[m_ActiveBuffer]->cursorLeftWithinMargin(m_Cmd.params[0], rect);
                else
                    m_pWindows[m_ActiveBuffer]->cursorLeftWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'E':
                m_pWindows[m_ActiveBuffer]->cursorLeftToMargin(rect);
                m_pWindows[m_ActiveBuffer]->cursorDown(m_Cmd.params[0] ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'F':
                m_pWindows[m_ActiveBuffer]->cursorLeftToMargin(rect);
                m_pWindows[m_ActiveBuffer]->cursorUp(m_Cmd.params[0] ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'G':
                // Absolute column reference. (XTERM)
                m_pWindows[m_ActiveBuffer]->setCursorX((m_Cmd.params[0]) ? m_Cmd.params[0]-1 : 0, rect);
                m_Flags = 0;
                break;

            case 'H':
            case 'f':
                {
                    if (m_Cmd.has_param)
                    {
                        size_t x = (m_Cmd.params[1]) ? m_Cmd.params[1] - 1 : 0;
                        size_t y = (m_Cmd.params[0]) ? m_Cmd.params[0] - 1 : 0;
                        if (m_Modes & Origin)
                        {
                            m_pWindows[m_ActiveBuffer]->setCursorRelOrigin(x, y, rect);
                        }
                        else
                        {
                            m_pWindows[m_ActiveBuffer]->setCursor(x, y, rect);
                        }
                    }
                    else
                    {
                        if (m_Modes & Origin)
                        {
                            m_pWindows[m_ActiveBuffer]->cursorToOrigin();
                        }
                        else
                        {
                            m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                        }
                    }
                    m_Flags = 0;
                }
                break;

            case 'I':
                if(!m_Cmd.params[0])
                    ++m_Cmd.params[0];

                for(int n = 0; n < m_Cmd.params[0]; ++n)
                {
                    m_pWindows[m_ActiveBuffer]->cursorTab(rect);
                }
                m_Flags = 0;
                break;

            case 'J':
                switch (m_Cmd.params[0])
                {
                    case 0: // Erase down.
                         m_pWindows[m_ActiveBuffer]->eraseDown(rect);
                        break;
                    case 1: // Erase up.
                        m_pWindows[m_ActiveBuffer]->eraseUp(rect);
                        break;
                    case 2: // Erase entire screen and move to home.
                        m_pWindows[m_ActiveBuffer]->eraseScreen(rect);
                        break;
                }
                m_Flags = 0;
                break;

            case 'K':
                switch (m_Cmd.params[0])
                {
                    case 0: // Erase end of line.
                        m_pWindows[m_ActiveBuffer]->eraseEOL(rect);
                        break;
                    case 1: // Erase start of line.
                        m_pWindows[m_ActiveBuffer]->eraseSOL(rect);
                        break;
                    case 2: // Erase entire line.
                        m_pWindows[m_ActiveBuffer]->eraseLine(rect);
                        break;
                }
                m_Flags = 0;
                break;

            case 'L':
                m_pWindows[m_ActiveBuffer]->insertLines(m_Cmd.params[0] ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'M':
                m_pWindows[m_ActiveBuffer]->deleteLines(m_Cmd.params[0] ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'P':
                m_pWindows[m_ActiveBuffer]->deleteCharacters((m_Cmd.params[0]) ? m_Cmd.params[0] : 1, rect);
                m_Flags = 0;
                break;

            case 'S':
                {
                    size_t nScrollLines = (m_Cmd.params[0]) ? m_Cmd.params[0] : 1;
                    m_pWindows[m_ActiveBuffer]->scrollDown(nScrollLines, rect);
                    m_Flags = 0;
                }
                break;

            case 'T':
                if(m_Flags & RightAngle)
                {
                    /// \todo Changes how title modes are set...
                }
                else if(m_Cmd.cur_param > 1)
                {
                    klog(LOG_INFO, "XTERM: highlight mouse tracking is not supported.");
                }
                else
                {
                    size_t nScrollLines = (m_Cmd.params[0]) ? m_Cmd.params[0] : 1;
                    m_pWindows[m_ActiveBuffer]->scrollUp(nScrollLines, rect);
                }
                m_Flags = 0;
                break;

            case 'X': // Erase characters (XTERM)
                 m_pWindows[m_ActiveBuffer]->eraseChars((m_Cmd.params[0]) ? m_Cmd.params[0]-1 : 1, rect);
                m_Flags = 0;
                break;

            case 'Z':
                if(!m_Cmd.params[0])
                    ++m_Cmd.params[0];

                for(int n = 0; n < m_Cmd.params[0]; ++n)
                {
                    m_pWindows[m_ActiveBuffer]->cursorTabBack(rect);
                }
                m_Flags = 0;
                break;

            case 'c':
                if(m_Cmd.params[0])
                {
                    klog(LOG_INFO, "XTERM: Device Attributes command with non-zero parameter");
                }
                else if(m_Flags & RightAngle)
                {
                    // Secondary Device Attributes
                    const char *attribs = "\e[?85;95;0c";
                    while(*attribs)
                    {
                        m_pT->addToQueue(*attribs);
                        ++attribs;
                    }
                }
                else
                {
                    // Primary Device Attributes
                    const char *attribs = "\e[?1;2c";
                    while(*attribs)
                    {
                        m_pT->addToQueue(*attribs);
                        ++attribs;
                    }
                }
                m_Flags = 0;
                break;

            case 'd':
                // Absolute row reference. (XTERM)
                m_pWindows[m_ActiveBuffer]->setCursorY((m_Cmd.params[0]) ? m_Cmd.params[0]-1 : 0, rect);
                m_Flags = 0;
                break;

            case 'e':
                // Relative row reference. (XTERM)
                m_pWindows[m_ActiveBuffer]->cursorDown((m_Cmd.params[0]) ? m_Cmd.params[0]-1 : 1, rect);
                m_Flags = 0;
                break;

            case 'g':
                if((!m_Cmd.has_param) || (m_Cmd.params[0] == 0))
                    m_pWindows[m_ActiveBuffer]->clearTabStop();
                else if(m_Cmd.has_param && (m_Cmd.params[0] == 3))
                    m_pWindows[m_ActiveBuffer]->clearAllTabStops();
                m_Flags = 0;
                break;

            case 'h':
            case 'l':
                {
                size_t modesToChange = 0;

                if(m_Flags & Question)
                {
                    // DEC private mode set.
                    for(int i = 0; i <= m_Cmd.cur_param; ++i)
                    {
                        switch(m_Cmd.params[i])
                        {
                            case 1:
                                modesToChange |= CursorKey;
                                break;
                            case 2:
                                modesToChange |= AnsiVt52;
                                break;
                            case 3:
                                modesToChange |= Column;
                                break;
                            case 4:
                                modesToChange |= Scrolling;
                                break;
                            case 5:
                                modesToChange |= Screen;
                                break;
                            case 6:
                                modesToChange |= Origin;
                                break;
                            case 7:
                                modesToChange |= AutoWrap;
                                break;
                            case 8:
                                modesToChange |= AutoRepeat;
                                break;
                            case 40:
                                /// \todo Do something about this.
                                klog(LOG_INFO, "(Dis)Allowing 80->132 mode.");
                                break;
                            case 45:
                                klog(LOG_INFO, "Reverse-wraparound mode.");
                                break;
                            case 67:
                                modesToChange |= AppKeypad;
                                break;
                            case 69:
                                modesToChange |= Margin;
                                break;
                            case 1048:
                            case 1049:
                                if(utf32 == 'h')
                                {
                                    m_SavedX = m_pWindows[m_ActiveBuffer]->getCursorX();
                                    m_SavedY = m_pWindows[m_ActiveBuffer]->getCursorY();
                                }
                                else
                                {
                                    m_pWindows[m_ActiveBuffer]->setCursor(m_SavedX, m_SavedY, rect);
                                }
                            case 47:
                            case 1047:
                                // 1048 is only save/restore cursor.
                                if(m_Cmd.params[i] != 1048)
                                {
                                    m_ActiveBuffer = (utf32 == 'h') ? 1 : 0;
                                    if((utf32 == 'h') && (m_Cmd.params[i] == 1049))
                                    {
                                        // Clear new buffer.
                                        m_pWindows[m_ActiveBuffer]->eraseScreen(rect);
                                    }
                                    else
                                    {
                                        // Merely switch to the new buffer.
                                        m_pWindows[m_ActiveBuffer]->renderAll(rect, m_pWindows[0]);
                                    }
                                }
                                break;
                            default:
                                klog(LOG_INFO, "XTERM: unknown DEC Private Mode %d", m_Cmd.params[i]);
                                break;
                        }
                    }
                }
                else
                {
                    for(int i = 0; i <= m_Cmd.cur_param; ++i)
                    {
                        switch(m_Cmd.params[i])
                        {
                            case 4:
                                modesToChange |= Insert;
                                break;
                            case 20:
                                modesToChange |= LineFeedNewLine;
                                break;
                            default:
                                klog(LOG_INFO, "XTERM: unknown standard mode %d", m_Cmd.params[i]);
                                break;
                        }
                    }
                }

                if(utf32 == 'h')
                {
                    m_Modes |= modesToChange;
                }
                else
                {
                    m_Modes &= ~(modesToChange);
                }

                // Setting some modes causes things to change.
                if(modesToChange & Origin)
                {
                    if (m_Modes & Origin)
                    {
                        // Reset origin to margins.
                        m_pWindows[m_ActiveBuffer]->cursorToOrigin();
                    }
                    else
                    {
                        // Move to top left corner.
                        m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                    }
                }

                if (modesToChange & Column)
                {
                    // Reset margins.
                    if (m_Modes & Column)
                    {
                        m_pWindows[m_ActiveBuffer]->setMargins(0, XTERM_WIDE);
                    }
                    else
                    {
                        m_pWindows[m_ActiveBuffer]->setMargins(0, XTERM_STANDARD);
                    }

                    m_pWindows[m_ActiveBuffer]->eraseScreen(rect);
                    m_pWindows[m_ActiveBuffer]->setScrollRegion(-1, -1);

                    // Cursor to home.
                    m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                }

                if (modesToChange & Screen)
                {
                    renderAll(rect);
                }

                }
                m_Flags = 0;
                break;

            case 'm':
                {
                    // Character attributes.
                    for (int i = 0; i < m_Cmd.cur_param + 1; i++)
                    {
                        switch (m_Cmd.params[i])
                        {
                            case 0:
                            {
                                // Reset all attributes.
                                m_pWindows[m_ActiveBuffer]->setFlags(0);
                                m_pWindows[m_ActiveBuffer]->setForeColour(g_DefaultFg);
                                m_pWindows[m_ActiveBuffer]->setBackColour(g_DefaultBg);
                                break;
                            }
                            case 1:
                            {
                                // Bold
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                if(!(flags & XTERM_BOLD))
                                {
                                    flags |= XTERM_BOLD;
                                    m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                }
                                break;
                            }
                            case 3:
                            {
                                // Italic
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                if(!(flags & XTERM_ITALIC))
                                {
                                    flags |= XTERM_ITALIC;
                                    m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                }
                                break;
                            }
                            case 4:
                            {
                                // Underline
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                if(!(flags & XTERM_UNDERLINE))
                                {
                                    flags |= XTERM_UNDERLINE;
                                    m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                }
                                break;
                            }
                            case 7:
                            {
                                // Inverse
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                if(flags & XTERM_INVERSE)
                                    flags &= ~XTERM_INVERSE;
                                else
                                    flags |= XTERM_INVERSE;
                                m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                break;
                            }
                            case 22:
                            {
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                flags &= ~XTERM_ITALIC;
                                m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                break;
                            }
                            case 23:
                            {
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                flags &= ~XTERM_ITALIC;
                                m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                break;
                            }
                            case 24:
                            {
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                flags &= ~XTERM_UNDERLINE;
                                m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                break;
                            }
                            case 27:
                            {
                                uint8_t flags = m_pWindows[m_ActiveBuffer]->getFlags();
                                flags &= ~XTERM_INVERSE;
                                m_pWindows[m_ActiveBuffer]->setFlags(flags);
                                break;
                            }
                            case 30:
                            case 31:
                            case 32:
                            case 33:
                            case 34:
                            case 35:
                            case 36:
                            case 37:
                                // Foreground.
                                m_pWindows[m_ActiveBuffer]->setForeColour(m_Cmd.params[i]-30);
                                break;
                            case 38:
                                // xterm-256 foreground
                                if(m_Cmd.params[i+1] == 5)
                                {
                                    m_pWindows[m_ActiveBuffer]->setForeColour(m_Cmd.params[i+2]);
                                    i += 3;
                                }
                                break;
                            case 39:
                                m_pWindows[m_ActiveBuffer]->setForeColour(g_DefaultFg);
                                break;
                            case 40:
                            case 41:
                            case 42:
                            case 43:
                            case 44:
                            case 45:
                            case 46:
                            case 47:
                                // Background.
                                m_pWindows[m_ActiveBuffer]->setBackColour(m_Cmd.params[i]-40);
                                break;
                            case 48:
                                // xterm-256 background
                                if(m_Cmd.params[i+1] == 5)
                                {
                                    m_pWindows[m_ActiveBuffer]->setBackColour(m_Cmd.params[i+2]);
                                    i += 3;
                                }
                                break;
                            case 49:
                                m_pWindows[m_ActiveBuffer]->setForeColour(g_DefaultBg);
                                break;

                            default:
                                // Do nothing.
                                klog(LOG_INFO, "XTERM: unknown character attribute %d", m_Cmd.params[i]);
                                break;
                        }
                    }
                    m_Flags = 0;
                }
                break;

            case 'n':
                if((m_Flags & RightAngle) == 0)
                {
                    // Device Status Reports
                    switch(m_Cmd.params[0])
                    {
                        case 5:
                            {
                                // Ready, no malfunction status.
                                const char *status = "\e[0n";
                                while(*status)
                                {
                                    m_pT->addToQueue(*status);
                                    ++status;
                                }
                            }
                            break;

                        case 6:
                            {
                                size_t reportX = m_pWindows[m_ActiveBuffer]->getCursorX() + 1;
                                size_t reportY = m_pWindows[m_ActiveBuffer]->getCursorY() + 1;

                                if (m_Modes & Origin)
                                {
                                    reportX = m_pWindows[m_ActiveBuffer]->getCursorXRelOrigin() + 1;
                                    reportY = m_pWindows[m_ActiveBuffer]->getCursorYRelOrigin() + 1;
                                }

                                // Report cursor position.
                                char buf[128];
                                sprintf(buf, "\e[%zd;%zdR", reportY, reportX);
                                const char *p = (const char *) buf;
                                while(*p)
                                {
                                    m_pT->addToQueue(*p);
                                    ++p;
                                }
                            }
                            break;

                        default:
                            klog(LOG_INFO, "XTERM: unknown device status request %d", m_Cmd.params[0]);
                            break;
                    }
                }
                m_Flags = 0;
                break;

            case 'p':
                // Could be soft terminal reset, request ANSI mode, set resource level, etc...
                m_Flags = 0;
                break;

            case 'q':
                if(m_Flags & Space)
                {
                    // Set cursor style.
                }

                m_Flags = 0;
                break;

            case 'r':
                if((m_Flags & Question) == 0)
                {
                    if (m_Cmd.has_param)
                    {
                        m_pWindows[m_ActiveBuffer]->setScrollRegion(m_Cmd.params[0] - 1, m_Cmd.params[1] - 1);
                    }
                    else
                    {
                        m_pWindows[m_ActiveBuffer]->setScrollRegion(-1, -1);
                    }

                    if (m_Modes & Origin)
                    {
                        m_pWindows[m_ActiveBuffer]->cursorToOrigin();
                    }
                    else
                    {
                        m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                    }
                }
                m_Flags = 0;
                break;

            case 's':
                if(!m_Cmd.has_param)
                {
                    if((m_Modes & Margin) == 0)
                    {
                        m_SavedX = m_pWindows[m_ActiveBuffer]->getCursorX();
                        m_SavedY = m_pWindows[m_ActiveBuffer]->getCursorY();
                    }
                }
                else if(m_Modes & Margin)
                {
                    // Set left/right margins.
                    m_pWindows[m_ActiveBuffer]->setMargins((m_Cmd.params[0]) ? m_Cmd.params[0]-1 : ~0,
                                                           (m_Cmd.params[1]) ? m_Cmd.params[1]-1 : ~0);
                }
                m_Flags = 0;
                break;

            case 'u':
                if((m_Flags & Space) == 0)
                {
                    m_pWindows[m_ActiveBuffer]->setCursor(m_SavedX, m_SavedY, rect);
                }
                m_Flags = 0;
                break;

            case 'x':
                if((m_Flags & Asterisk) == 0)
                {
                    if(m_Cmd.params[0] <= 1)
                    {
                        const char *termparams = 0;
                        if(m_Cmd.params[0] == 1)
                            termparams = "\e[3;1;1;120;120;1;0x";
                        else
                            termparams = "\e[2;1;1;120;120;1;0x";

                        while(*termparams)
                        {
                            m_pT->addToQueue(*termparams);
                            ++termparams;
                        }
                    }
                }
                m_Flags = 0;
                break;

            default:
                break;
        }
    }
    else if((m_Flags & (Escape | LeftSquare | RightSquare | Underscore)) == Escape)
    {
        bool seenFlag = setFlagsForUtf32(utf32);

        switch(utf32)
        {
            case 0x08:
                // Backspace within escape sequence.
                m_pWindows[m_ActiveBuffer]->backspace(rect);
                break;

            case '0':
                if(m_Flags & LeftRound) // \e(0
                {
                    // Set DEC Special Character and Line Drawing Set
                    m_pWindows[m_ActiveBuffer]->setLineRenderMode(true);
                }
                m_Flags = 0;
                break;

            case '3':
                klog(LOG_ALERT, "XTERM: double-height lines not supported");
                m_Flags = 0;
                break;

            case '4':
                klog(LOG_ALERT, "XTERM: double-height lines not supported");
                m_Flags = 0;
                break;

            case '5':
                // Single-width line set.
                m_Flags = 0;
                break;

            case '6':
                klog(LOG_ALERT, "XTERM: double-width lines not supported");
                m_Flags = 0;
                break;

            case '7':
                m_SavedX = m_pWindows[m_ActiveBuffer]->getCursorX();
                m_SavedY = m_pWindows[m_ActiveBuffer]->getCursorY();
                m_Flags = 0;
                break;

            case '8':
                if(m_Flags & Hash)
                {
                    // DECALN: DEC Screen Alignment Test (fill screen with 'E')

                    // DECALN resets margins
                    if (m_Modes & Column)
                    {
                        m_pWindows[m_ActiveBuffer]->setMargins(0, XTERM_WIDE);
                    }
                    else
                    {
                        m_pWindows[m_ActiveBuffer]->setMargins(0, XTERM_STANDARD);
                    }
                    m_pWindows[m_ActiveBuffer]->setScrollRegion(-1, -1);

                    // Fill the space with 'E'.
                    m_pWindows[m_ActiveBuffer]->fillChar('E', rect);

                    // Cursor to home.
                    m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                }
                else
                {
                    m_pWindows[m_ActiveBuffer]->setCursor(m_SavedX, m_SavedY, rect);
                }
                m_Flags = 0;
                break;

            case '<':
                m_Modes |= AnsiVt52;
                m_Flags = 0;
                break;

            case '>':
                /// \todo Alternate keypad mode
                m_Flags = 0;
                break;

            case 'A':
                m_pWindows[m_ActiveBuffer]->cursorUpWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'B':
                if(m_Flags & LeftRound) // \e(B
                {
                    // Set USASCII character set.
                    m_pWindows[m_ActiveBuffer]->setLineRenderMode(false);
                }
                else
                {
                    m_pWindows[m_ActiveBuffer]->cursorDownWithinMargin(1, rect);
                }
                m_Flags = 0;
                break;

            case 'C':
                m_pWindows[m_ActiveBuffer]->cursorRightWithinMargin(1, rect);
                m_Flags = 0;
                break;

            case 'D':
                if(m_Modes & AnsiVt52)
                {
                    m_pWindows[m_ActiveBuffer]->cursorDown(1, rect);
                }
                else
                {
                    m_pWindows[m_ActiveBuffer]->cursorLeftWithinMargin(1, rect);
                }
                m_Flags = 0;
                break;

            case 'E':
                m_pWindows[m_ActiveBuffer]->cursorDownAndLeftToMargin(rect);
                m_Flags = 0;
                break;

            case 'F':
                /// \note We don't handle "\e F" (7-bit controls)
                // \eF is enabled by the hpLowerleftBugCompat resource

                /// \todo In VT52 mode, we need to go into VT52 graphics mode...
                m_Flags = 0;
                break;

            case 'G':
                /// \todo In VT52 mode, exit VT52 graphics mode...
                m_Flags = 0;
                break;

            case 'H':
                if(m_Modes & AnsiVt52)
                {
                    // Set tab stop.
                    m_pWindows[m_ActiveBuffer]->setTabStop();
                }
                else
                {
                    // Cursor to home.
                    m_pWindows[m_ActiveBuffer]->setCursor(0, 0, rect);
                }
                m_Flags = 0;
                break;

            case 'I':
                if(!(m_Modes & AnsiVt52))
                {
                    // Reverse line feed.
                    m_pWindows[m_ActiveBuffer]->cursorUp(1, rect);
                }
                m_Flags = 0;
                break;

            case 'J':
                if(!(m_Modes & AnsiVt52))
                {
                    m_pWindows[m_ActiveBuffer]->eraseDown(rect);
                }
                m_Flags = 0;
                break;

            case 'K':
                if(!(m_Modes & AnsiVt52))
                {
                    m_pWindows[m_ActiveBuffer]->eraseEOL(rect);
                }
                m_Flags = 0;
                break;

            case 'M':
                if((m_Modes & AnsiVt52) && !(m_Flags & Space))
                {
                    // Reverse Index.
                    m_pWindows[m_ActiveBuffer]->cursorUp(1, rect);
                }
                m_Flags = 0;
                break;

            case 'N':
            case 'O':
                // Single shift select for G2/3 character sets.
                m_Flags = 0;
                break;

            case 'Y':
                if(!(m_Modes & AnsiVt52))
                {
                    // Set cursor position.
                    m_Flags |= Vt52SetCursorWaitY;
                }
                else
                {
                    m_Flags = 0;
                }
                break;

            case 'Z':
                // Terminal identification.
                {
                    const char *answerback = 0;
                    if(m_Modes & AnsiVt52)
                        answerback = "\e[1;2c";
                    else
                        answerback = "\e/Z";

                    while(*answerback)
                    {
                        m_pT->addToQueue(*answerback);
                        ++answerback;
                    }
                }
                m_Flags = 0;
                break;

            default:
                if (!seenFlag)
                {
                    klog(LOG_INFO, "XTERM: unknown ESCAPE control '%c'", utf32);
                    m_Flags = 0;
                }
                break;
        }
    }
    else if((m_Flags & (Escape | LeftSquare | RightSquare | Underscore)) == (Escape | RightSquare))
    {
        switch(utf32)
        {
            case '\007':
            case 0x9C:
                if(!m_OsCtl.has_param)
                {
                    klog(LOG_INFO, "XTERM: not enough parameters for OS control");
                }
                else
                {
                    if(m_OsCtl.params[0] == "0" ||
                        m_OsCtl.params[0] == "1" ||
                        m_OsCtl.params[0] == "2")
                    {
                        if (m_pWidget)
                        {
                            m_pWidget->setTitle(m_OsCtl.params[1]);
                        }
                    }
                    else
                    {
                        klog(LOG_INFO, "XTERM: unhandled OS control '%s'", m_OsCtl.params[0].c_str());
                    }
                }
                m_Flags = 0;
                break;

            case ';':
            case ':':
                m_OsCtl.cur_param++;
                break;

            default:
                if(utf32 >= ' ')
                {
                    m_OsCtl.params[m_OsCtl.cur_param] += static_cast<char>(utf32);
                }
                break;
        }
    }
    else if((m_Flags & (Escape | LeftSquare | RightSquare | Underscore)) == (Escape | Underscore))
    {
        // No application program commands in xterm.
        if(utf32 == 0x9C)
            m_Flags = 0;
    }

    m_pT->addToQueue(0, true);
}

void Xterm::renderAll(DirtyRectangle &rect)
{
    m_pWindows[m_ActiveBuffer]->renderAll(rect, m_pWindows[m_ActiveBuffer]);
}

Xterm::Window::Window(size_t nRows, size_t nCols, PedigreeGraphics::Framebuffer *pFb, size_t nMaxScrollback, size_t offsetLeft, size_t offsetTop, size_t fbWidth, Xterm *parent) :
    m_pBuffer(0), m_BufferLength(0), m_pFramebuffer(pFb), m_FbWidth(fbWidth), m_Width(nCols), m_Height(nRows), m_Stride(XTERM_MIN_WIDTH), m_OffsetLeft(offsetLeft), m_OffsetTop(offsetTop), m_nMaxScrollback(nMaxScrollback), m_CursorX(0), m_CursorY(0), m_ScrollStart(0), m_ScrollEnd(nRows-1),
    m_pInsert(0), m_pView(0), m_Fg(g_DefaultFg), m_Bg(g_DefaultBg), m_Flags(0), m_bCursorFilled(true), m_bLineRender(false), m_pParentXterm(parent)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::Window() dimensions %zdx%zd", nCols, nRows);
#endif

    if (m_Width > m_Stride)
        m_Stride = m_Width;

    // Using malloc() instead of new[] so we can use realloc()
    m_pBuffer = reinterpret_cast<TermChar*>(malloc(m_Stride * m_Height * sizeof(TermChar)));

    m_BufferLength = m_Stride * m_Height;

    TermChar blank;
    blank.fore = m_Fg;
    blank.back = m_Bg;
    blank.utf32 = ' ';
    blank.flags = 0;
    for (size_t i = 0; i < m_Stride * m_Height; i++)
        m_pBuffer[i] = blank;

    if(m_pParentXterm->m_pCairo)
    {
        cairo_save(m_pParentXterm->m_pCairo);
        cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

        cairo_set_source_rgba(
                m_pParentXterm->m_pCairo,
                ((g_Colours[m_Bg] >> 16) & 0xFF) / 256.0,
                ((g_Colours[m_Bg] >> 8) & 0xFF) / 256.0,
                ((g_Colours[m_Bg]) & 0xFF) / 256.0,
                0.8);

        cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft, m_OffsetTop, nCols * m_pParentXterm->m_pNormalFont->getWidth(), nRows * m_pParentXterm->m_pNormalFont->getHeight());
        cairo_fill(m_pParentXterm->m_pCairo);

        cairo_restore(m_pParentXterm->m_pCairo);
    }

    m_pInsert = m_pView = m_pBuffer;

    m_LeftMargin = 0;
    m_RightMargin = m_Width;
}

Xterm::Window::~Window()
{
    free(m_pBuffer);
}

void Xterm::Window::showCursor(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::showCursor");
#endif

    render(rect, m_bCursorFilled ? XTERM_INVERSE : XTERM_BORDER);
}

void Xterm::Window::hideCursor(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::hideCursor");
#endif

    render(rect);
}

void Xterm::Window::resize(size_t nWidth, size_t nHeight, bool bActive)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::resize(%zd, %zd)", nWidth, nHeight);
#endif

    m_pFramebuffer = 0;

    size_t cols = nWidth / m_pParentXterm->m_pNormalFont->getWidth();
    size_t rows = nHeight / m_pParentXterm->m_pNormalFont->getHeight();

#ifdef XTERM_DEBUG
    klog(LOG_INFO, " -> cols %zd, %zd", nWidth, nHeight);
#endif

    if(bActive && m_Bg && m_pParentXterm->m_pCairo)
    {
        size_t bg = m_Bg;
        if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
        {
            bg = m_Fg;
        }

        cairo_save(m_pParentXterm->m_pCairo);
        cairo_set_source_rgba(
                m_pParentXterm->m_pCairo,
                ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
                ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
                ((g_Colours[bg]) & 0xFF) / 256.0,
                1.0);

        cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft, m_OffsetTop, nWidth, nHeight);
        cairo_fill(m_pParentXterm->m_pCairo);

        cairo_restore(m_pParentXterm->m_pCairo);
    }

    size_t previousStride = m_Stride;

    if (m_Stride < XTERM_MIN_WIDTH)
        m_Stride = XTERM_MIN_WIDTH;

    if (cols > m_Stride)
        m_Stride = cols;

    TermChar *newBuf = reinterpret_cast<TermChar*>(malloc(m_Stride * rows * sizeof(TermChar)));

    TermChar blank;
    blank.fore = m_Fg;
    blank.back = m_Bg;
    blank.utf32 = ' ';
    blank.flags = 0;
    for (size_t i = 0; i < m_Stride * rows; i++)
        newBuf[i] = blank;

    for (size_t r = 0; r < m_Height; r++)
    {
        if (r >= rows) break;
        for (size_t c = 0; c < m_Stride; c++)
        {
            if (c >= cols) break;

            newBuf[r * m_Stride + c] = m_pInsert[r * previousStride + c];
        }
    }

    free(m_pBuffer);

    m_pInsert = m_pView = m_pBuffer = newBuf;
    m_BufferLength = rows * m_Stride;

    if(m_RightMargin > (ssize_t) cols)
        m_RightMargin = cols;
    if(m_LeftMargin > (ssize_t) cols)
        m_LeftMargin = cols;

    m_FbWidth = cols * m_pParentXterm->m_pNormalFont->getWidth();
    m_Width = cols;
    m_Height = rows;
    m_ScrollStart = 0;
    m_ScrollEnd = rows - 1;
    if (m_CursorX >= m_RightMargin)
        m_CursorX = m_RightMargin - 1;
    if (m_CursorY > m_ScrollEnd)
        m_CursorY = m_ScrollEnd;

    // Set default tab stops.
    delete [] m_pParentXterm->m_TabStops;
    m_pParentXterm->m_TabStops = new char[m_Stride];
    memset(m_pParentXterm->m_TabStops, 0, m_Stride);
    for(size_t i = 0; i < m_Stride; i += 8)
    {
        m_pParentXterm->m_TabStops[i] = '|';
    }
}

void Xterm::Window::setScrollRegion(int start, int end)
{
    if (start == -1)
        start = 0;
    if (end == -1)
        end = m_Height - 1;

#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setScrollRegion(%d, %d)", start, end);
#endif

    m_ScrollStart = start;
    m_ScrollEnd = end;

    if (m_ScrollStart > m_ScrollEnd)
    {
        size_t tmp = m_ScrollStart;
        m_ScrollStart = m_ScrollEnd;
        m_ScrollEnd = tmp;
    }

    if(m_ScrollStart >= (ssize_t) m_Height)
        m_ScrollStart = m_Height - 1;
    if(m_ScrollEnd >= (ssize_t) m_Height)
        m_ScrollEnd = m_Height - 1;
}

void Xterm::Window::setForeColour(uint8_t fgColour)
{
    m_Fg = fgColour;
}

void Xterm::Window::setBackColour(uint8_t bgColour)
{
    m_Bg = bgColour;
}

void Xterm::Window::setFlags(uint8_t flags)
{
    m_Flags = flags;
}

uint8_t Xterm::Window::getFlags()
{
    return m_Flags;
}

void Xterm::Window::setMargins(size_t left, size_t right)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setMargins(%zd, %zd)", left, right);
#endif

    if(left > m_Stride)
        left = m_Stride;
    if(right > m_Stride)
        right = m_Stride;

    m_LeftMargin = left;
    m_RightMargin = right;
}

void Xterm::Window::renderAll(DirtyRectangle &rect, Xterm::Window *pPrevious)
{
    TermChar *pOld = (pPrevious) ? pPrevious->m_pInsert : 0;
    TermChar *pNew = m_pInsert;

    // "Cleverer" full redraw - only redraw those glyphs that are different from the previous window.
    for (size_t y = 0; y < m_Height; y++)
    {
        for (size_t x = 0; x < m_Width; --x)
        {
            if ((!pOld) || (pOld[y*m_Stride+x] != pNew[y*m_Stride+x]) ||
                (m_pParentXterm->getModes() != pPrevious->m_pParentXterm->getModes()))
            {
                render(rect, 0, x, y);
            }
        }
    }
}

void Xterm::Window::renderArea(DirtyRectangle &rect, size_t x, size_t y, size_t w, size_t h)
{
    if (x == ~0UL)
        x = 0;
    if (y == ~0UL)
        y = 0;
    if (w == ~0UL)
        w = m_Width;
    if (h == ~0UL)
        h = m_Height;

    for (size_t cy = y; cy < (y + h); ++cy)
    {
        for (size_t cx = x; cx < (x + w); ++cx)
        {
            render(rect, 0, cx, cy);
        }
    }
}

void Xterm::Window::setChar(uint32_t utf32, size_t x, size_t y)
{
    if(x > m_Stride)
        return;
    if(y > m_Height)
        return;

    TermChar *c = &m_pInsert[y * m_Stride + x];

    c->fore = m_Fg;
    c->back = m_Bg;

    c->flags = m_Flags;
    c->utf32 = utf32;
}

Xterm::Window::TermChar Xterm::Window::getChar(size_t x, size_t y)
{
    if (x == ~0UL) x = m_CursorX;
    if (y == ~0UL) y = m_CursorY;
    return m_pInsert[y * m_Stride + x];
}

void Xterm::Window::cursorDown(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorDown(%zd)", n);
#endif

    m_CursorY += n;
    checkScroll(rect);
}

void Xterm::Window::cursorUp(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorUp(%zd)", n);
#endif

    m_CursorY -= n;
    checkScroll(rect);
}

void Xterm::Window::cursorUpWithinMargin(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorUpWithinMargin(%zd)", n);
#endif

    m_CursorY -= n;
    if (m_CursorY < m_ScrollStart)
        m_CursorY = m_ScrollStart;
}

void Xterm::Window::cursorLeftWithinMargin(size_t n, DirtyRectangle &)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorLeftWithinMargin(%zd)", n);
#endif

    m_CursorX -= n;

    if (m_CursorX < m_LeftMargin)
        m_CursorX = m_LeftMargin;
}

void Xterm::Window::cursorRightWithinMargin(size_t n, DirtyRectangle &)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorRightWithinMargin(%zd)", n);
#endif

    m_CursorX += n;

    if (m_CursorX >= m_RightMargin)
        m_CursorX = m_RightMargin - 1;
}

void Xterm::Window::cursorDownWithinMargin(size_t n, DirtyRectangle &)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorDownWithinMargin(%zd)", n);
#endif

    m_CursorY += n;

    if (m_CursorY > m_ScrollEnd)
        m_CursorY = m_ScrollEnd;
}


void Xterm::Window::backspace(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::backspace()");
#endif

    if(m_CursorX == m_RightMargin)
        --m_CursorX;

    if(m_CursorX > m_LeftMargin)
        --m_CursorX;
}

void Xterm::Window::render(DirtyRectangle &rect, size_t flags, size_t x, size_t y)
{
    if (x == ~0UL) x = m_CursorX;
    if (y == ~0UL) y = m_CursorY;
    if (x > m_Width || y > m_Height)
    {
        return;
    }

    TermChar c = m_pView[y * m_Stride + x];

    c.flags |= flags;

    uint32_t fg = g_Colours[c.fore];
    if (c.flags & XTERM_BRIGHTFG)
        fg = g_BrightColours[c.fore];
    uint32_t bg = g_Colours[c.back];
    if (c.flags & XTERM_BRIGHTBG)
        bg = g_BrightColours[c.back];

    if (c.flags & XTERM_INVERSE)
    {
        uint32_t tmp = fg;
        fg = bg;
        bg = tmp;
    }

    if (m_pParentXterm->getModes() & Screen)
    {
        // DECSCNM only applies to cells without custom color.
        if (c.fore == g_DefaultFg && c.back == g_DefaultBg)
        {
            uint32_t tmp = fg;
            fg = bg;
            bg = tmp;
        }
    }

    uint32_t utf32 = c.utf32;

    // Ensure the painted area is marked dirty.
    rect.point((x * m_pParentXterm->m_pNormalFont->getWidth()) + m_OffsetLeft,
               (y * m_pParentXterm->m_pNormalFont->getHeight()) + m_OffsetTop);
    rect.point(((x + 1) * m_pParentXterm->m_pNormalFont->getWidth()) + m_OffsetLeft,
               ((y + 1) * m_pParentXterm->m_pNormalFont->getHeight()) + m_OffsetTop);

    Font *pFont = m_pParentXterm->m_pNormalFont;
    bool bBold = (c.flags & XTERM_BOLD) == XTERM_BOLD;
    bool bItalic = (c.flags & XTERM_ITALIC) == XTERM_ITALIC;
    bool bUnderline = (c.flags & XTERM_UNDERLINE) == XTERM_UNDERLINE;

    pFont->render(m_pFramebuffer, utf32,
                  (x * pFont->getWidth()) + m_OffsetLeft,
                  (y * pFont->getHeight()) + m_OffsetTop,
                  fg, bg, true, bBold, bItalic, bUnderline);

    if(c.flags & XTERM_BORDER)
    {
        // Border around the cell.
        cairo_save(m_pParentXterm->m_pCairo);
        cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

        cairo_set_line_width(m_pParentXterm->m_pCairo, 1.0);
        cairo_set_line_join(m_pParentXterm->m_pCairo, CAIRO_LINE_JOIN_MITER);
        cairo_set_antialias(m_pParentXterm->m_pCairo, CAIRO_ANTIALIAS_NONE);

        cairo_set_source_rgba(
                m_pParentXterm->m_pCairo,
                ((fg >> 16) & 0xFF) / 256.0,
                ((fg >> 8) & 0xFF) / 256.0,
                (fg & 0xFF) / 256.0,
                0.8);

        cairo_rectangle(
                m_pParentXterm->m_pCairo,
                (x * pFont->getWidth()) + m_OffsetLeft + 1,
                (y * pFont->getHeight()) + m_OffsetTop + 1,
                pFont->getWidth() - 2,
                pFont->getHeight() - 2);
        cairo_stroke(m_pParentXterm->m_pCairo);

        cairo_restore(m_pParentXterm->m_pCairo);
    }
}

void Xterm::Window::scrollRegionUp(size_t numRows, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::scrollRegionUp(%zd)", numRows);
#endif

    size_t targetY = m_ScrollStart;
    size_t sourceY = m_ScrollStart + numRows;

    size_t movedRows = (m_ScrollEnd + 1) - sourceY;
    size_t blankFrom = (m_ScrollEnd + 1) - numRows;
    size_t blankLength = (m_ScrollEnd + 1) - blankFrom;

    // Because we need to bitblit to copy, we need to flush anything that hasn't
    // been written just yet.
    rect.point(m_OffsetLeft, m_OffsetTop + (targetY + m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + ((blankFrom + blankLength) * m_pParentXterm->m_pNormalFont->getHeight()));
    m_pParentXterm->m_pTui->redraw(rect);
    rect.reset();

    cairo_save(m_pParentXterm->m_pCairo);

    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    // Copying the scroll region.
    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (targetY * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - m_OffsetLeft, movedRows * m_pParentXterm->m_pNormalFont->getHeight());

    cairo_push_group(m_pParentXterm->m_pCairo);
    cairo_set_source_surface(m_pParentXterm->m_pCairo, m_pParentXterm->m_pCairoSurface,
        0, -((double) (numRows * m_pParentXterm->m_pNormalFont->getHeight())));
    cairo_fill_preserve(m_pParentXterm->m_pCairo);
    cairo_pop_group_to_source(m_pParentXterm->m_pCairo);

    //cairo_fill(m_pParentXterm->m_pCairo);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    // Blanking the region that used to exist.
    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (blankFrom * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - m_OffsetLeft, blankLength * m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    rect.point(m_OffsetLeft, m_OffsetTop + (targetY + m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_FbWidth, m_OffsetTop + ((blankFrom + blankLength) * m_pParentXterm->m_pNormalFont->getHeight()));

    memmove(&m_pInsert[targetY * m_Stride],
            &m_pInsert[sourceY * m_Stride],
            movedRows * m_Stride * sizeof(TermChar));

    TermChar blank;
    blank.fore = m_Fg;
    blank.back = m_Bg;
    blank.utf32 = ' ';
    blank.flags = 0;
    for (size_t i = blankFrom * m_Stride; i < (blankFrom + blankLength) * m_Stride; i++)
        m_pInsert[i] = blank;

    renderArea(rect, 0, targetY, m_Width, movedRows + blankLength);
}

void Xterm::Window::scrollRegionDown(size_t numRows, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::scrollRegionDown(%zd)", numRows);
#endif

    size_t targetY = m_ScrollStart + numRows;
    size_t sourceY = m_ScrollStart;

    size_t movedRows = (m_ScrollEnd + 1) - targetY;
    size_t blankFrom = sourceY;
    size_t blankLength = numRows;

    // Because we need to bitblit to copy, we need to flush anything that hasn't
    // been written just yet.
    rect.point(m_OffsetLeft, m_OffsetTop + (sourceY + m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + (movedRows * m_pParentXterm->m_pNormalFont->getHeight()));
    m_pParentXterm->m_pTui->redraw(rect);
    rect.reset();

    cairo_save(m_pParentXterm->m_pCairo);

    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    // Copying the scroll region.
    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (targetY * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - m_OffsetLeft, movedRows * m_pParentXterm->m_pNormalFont->getHeight());

    cairo_push_group(m_pParentXterm->m_pCairo);
    cairo_set_source_surface(m_pParentXterm->m_pCairo, m_pParentXterm->m_pCairoSurface,
        0, (double) (numRows * m_pParentXterm->m_pNormalFont->getHeight()));
    cairo_fill_preserve(m_pParentXterm->m_pCairo);
    cairo_pop_group_to_source(m_pParentXterm->m_pCairo);

    //cairo_fill(m_pParentXterm->m_pCairo);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    // Blanking the region that used to exist.
    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (blankFrom * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - m_OffsetLeft, blankLength * m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    rect.point(m_OffsetLeft, m_OffsetTop + (targetY + m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_FbWidth, m_OffsetTop + ((blankFrom + blankLength) * m_pParentXterm->m_pNormalFont->getHeight()));

    memmove(&m_pInsert[targetY * m_Stride],
            &m_pInsert[sourceY * m_Stride],
            movedRows * m_Stride * sizeof(TermChar));

    TermChar blank;
    blank.fore = m_Fg;
    blank.back = m_Bg;
    blank.utf32 = ' ';
    blank.flags = 0;
    for (size_t i = blankFrom * m_Stride; i < (blankFrom + blankLength) * m_Stride; i++)
        m_pInsert[i] = blank;

    renderArea(rect, 0, blankFrom, m_Width, movedRows + blankLength);
}

void Xterm::Window::setCursorRelOrigin(size_t x, size_t y, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setCursorRelOrigin(%zd, %zd)", x, y);
#endif

    x += m_LeftMargin;
    y += m_ScrollStart;
    setCursor(x, y, rect);
}

void Xterm::Window::setCursor(size_t x, size_t y, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setCursor(%zd, %zd)", x, y);
#endif

    m_CursorX = x;
    m_CursorY = y;
}

void Xterm::Window::setCursorX(size_t x, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setCursorX(%zd)", x);
#endif

    setCursor(x, m_CursorY, rect);
}
void Xterm::Window::setCursorY(size_t y, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setCursorY(%zd)", y);
#endif

    setCursor(m_CursorX, y, rect);
}

ssize_t Xterm::Window::getCursorX() const
{
    return m_CursorX;
}
ssize_t Xterm::Window::getCursorY() const
{
    return m_CursorY;
}

ssize_t Xterm::Window::getCursorXRelOrigin() const
{
    if((m_CursorX > m_LeftMargin) && (m_CursorX <= m_RightMargin))
        return m_CursorX - m_LeftMargin;

    return m_CursorX;
}

ssize_t Xterm::Window::getCursorYRelOrigin() const
{
    if((m_CursorY > m_ScrollStart) && (m_CursorY <= m_ScrollEnd))
        return m_CursorY - m_ScrollStart;

    return m_CursorY;
}

void Xterm::Window::cursorToOrigin()
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorToOrigin()");
#endif

    m_CursorX = m_LeftMargin;
    m_CursorY = m_ScrollStart;
}

void Xterm::Window::cursorLeft(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorLeft()");
#endif

    if (m_CursorX > m_LeftMargin)
        --m_CursorX;
}

void Xterm::Window::cursorLeftNum(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorLeftNum(%zd)", n);
#endif

    m_CursorX -= n;

    if (m_CursorX < m_LeftMargin)
        m_CursorX = m_LeftMargin;
}

void Xterm::Window::cursorDownAndLeftToMargin(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorDownAndLeftToMargin()");
#endif

    cursorDown(1, rect);
    m_CursorX = m_LeftMargin;
}

void Xterm::Window::cursorLeftToMargin(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorLeftToMargin()");
#endif

    m_CursorX = m_LeftMargin;
}

void Xterm::Window::cursorTab(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorTab()");
#endif

    bool tabStopFound = false;
    for(ssize_t x = (m_CursorX + 1); x < m_RightMargin; ++x)
    {
        if(m_pParentXterm->m_TabStops[x] != 0)
        {
            m_CursorX = x;
            tabStopFound = true;
            break;
        }
    }

    if(!tabStopFound)
    {
        m_CursorX = m_RightMargin - 1;
    }
    else if(m_CursorX >= m_RightMargin)
        m_CursorX = m_RightMargin - 1;
}

void Xterm::Window::cursorTabBack(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::cursorTabBack()");
#endif

    bool tabStopFound = false;
    for(ssize_t x = (m_CursorX - 1); x >= 0; --x)
    {
        if(m_pParentXterm->m_TabStops[x] != 0)
        {
            m_CursorX = x;
            tabStopFound = true;
            break;
        }
    }

    if(!tabStopFound)
    {
        m_CursorX = m_LeftMargin;
    }
    else if(m_CursorX < m_LeftMargin)
        m_CursorX = m_LeftMargin;
}

void Xterm::Window::fillChar(uint32_t utf32, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::fillChar(%c)", (char) utf32);
#endif

    for (ssize_t y = m_ScrollStart; y < m_ScrollEnd; ++y)
    {
        for (ssize_t x = m_LeftMargin; x < m_RightMargin; ++x)
        {
            setChar(utf32, x, y);
        }
    }

    renderArea(rect, m_LeftMargin, m_ScrollStart, m_RightMargin - m_LeftMargin, m_ScrollEnd - m_ScrollStart);
}

void Xterm::Window::addChar(uint32_t utf32, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG_EXTRA
    klog(LOG_INFO, "Xterm::Window::addChar(%c) [@ %zd, %zd]", (char) utf32, m_CursorX, m_CursorY);
#endif

    if (utf32 >= ' ')
    {
        checkWrap(rect);

        if (m_CursorX >= (ssize_t) m_Stride)
            return;

        if (m_pParentXterm->getModes() & Insert)
        {
            // We need some space, first.
            insertCharacters(1, rect);
        }

        TermChar tc = getChar();
        setChar(utf32, m_CursorX, m_CursorY);
        if (getChar() != tc)
            render(rect);

        ++m_CursorX;
    }
}

void Xterm::Window::scrollUp(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::scrollUp(%zd)", n);
#endif

    scrollRegionDown(n, rect);
}

void Xterm::Window::scrollDown(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::scrollDown(%zd)", n);
#endif

    scrollRegionUp(n, rect);
}

void Xterm::Window::eraseScreen(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseScreen()");
#endif

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, 0, 0, m_FbWidth, m_Height * m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // One good fillRect should do the job nicely.
    // m_pFramebuffer->rect(0, 0, m_FbWidth, m_Height * m_pParentXterm->m_pNormalFont->getHeight(), g_Colours[m_Bg], PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft, m_OffsetTop);
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + (m_Height * m_pParentXterm->m_pNormalFont->getHeight()));

    for(size_t row = 0; row < m_Height; row++)
    {
        for(size_t col = 0; col < m_Width; col++)
        {
            setChar(' ', col, row);
        }
    }

    renderArea(rect);
}

void Xterm::Window::eraseEOL(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseEOL()");
#endif

    size_t l = (m_CursorX * m_pParentXterm->m_pNormalFont->getWidth());

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft + l, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - l, m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // Again, one fillRect should do it.
    //m_pFramebuffer->rect(l, m_CursorY * m_pParentXterm->m_pNormalFont->getHeight(), m_FbWidth - l, m_pParentXterm->m_pNormalFont->getHeight(), g_Colours[m_Bg], PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + ((m_CursorY + 1) * m_pParentXterm->m_pNormalFont->getHeight()));

    size_t row = m_CursorY;
    for(size_t col = m_CursorX; col < m_Width; col++)
    {
        setChar(' ', col, row);
    }

    renderArea(rect, m_CursorX, m_CursorY, m_Width, 1);
}

void Xterm::Window::eraseSOL(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseSOL()");
#endif

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()),
        (m_CursorX + 1) * m_pParentXterm->m_pNormalFont->getWidth(), m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    rect.point(
        m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(
        m_OffsetLeft + ((m_CursorX + 1) * m_pParentXterm->m_pNormalFont->getWidth()),
        m_OffsetTop + ((m_CursorY + 1) * m_pParentXterm->m_pNormalFont->getHeight()));

    size_t row = m_CursorY;
    for(ssize_t col = 0; col <= m_CursorX; col++)
    {
        setChar(' ', col, row);
    }

    renderArea(rect, 0, m_CursorY, m_CursorX, 1);
}

void Xterm::Window::eraseLine(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseLine()");
#endif

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo,
        m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()),
        m_FbWidth - m_OffsetLeft, m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    rect.point(m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + ((m_CursorY + 1) * m_pParentXterm->m_pNormalFont->getHeight()));

    size_t row = m_CursorY;
    for(size_t col = 0; col < m_Width; col++)
    {
        setChar(' ', col, row);
    }

    renderArea(rect, 0, m_CursorY, m_Width, 1);
}

void Xterm::Window::eraseChars(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseChars(%zd)", n);
#endif

    // Again, one fillRect should do it.
    size_t left = (m_CursorX * m_pParentXterm->m_pNormalFont->getWidth());
    if((m_CursorX + (ssize_t) n) > m_RightMargin)
        n = m_RightMargin - m_CursorX;
    size_t width = n * m_pParentXterm->m_pNormalFont->getWidth();

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, left, m_CursorY * m_pParentXterm->m_pNormalFont->getHeight(), width, m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // m_pFramebuffer->rect(left, m_CursorY * m_pParentXterm->m_pNormalFont->getHeight(), width, m_pParentXterm->m_pNormalFont->getHeight(), g_Colours[m_Bg], PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft, m_OffsetTop + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight()));
    rect.point(m_OffsetLeft + width, m_OffsetTop + ((m_CursorY + 1) * m_pParentXterm->m_pNormalFont->getHeight()));

    size_t row = m_CursorY;
    for(size_t col = m_CursorX; col < (m_CursorX + n); col++)
    {
        setChar(' ', col, row);
    }

    renderArea(rect, m_CursorX, m_CursorY, n, 1);
}

void Xterm::Window::eraseUp(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseUp()");
#endif

    // Erase to the start of the line first. Essentially, we're erasing from the
    // current position to the top of the screen.
    eraseSOL(rect);

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft, m_OffsetTop, m_FbWidth, m_pParentXterm->m_pNormalFont->getHeight() * m_CursorY);
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // m_pFramebuffer->rect(0, 0, m_FbWidth, m_pParentXterm->m_pNormalFont->getHeight() * m_CursorY, g_Colours[m_Bg], PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft, m_OffsetTop);
    rect.point(m_OffsetLeft + m_FbWidth, m_OffsetTop + ((m_CursorY + 1) * m_pParentXterm->m_pNormalFont->getHeight()));

    for(ssize_t row = 0; row < m_CursorY; row++)
    {
        for(size_t col = 0; col < m_Width; col++)
        {
            setChar(' ', col, row);
        }
    }

    renderArea(rect, 0, 0, m_Width, m_CursorY);
}

void Xterm::Window::eraseDown(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::eraseDown()");
#endif

    // Erase to the end of the line first. Essentially, we're erasing from the
    // current position to the end of the screen.
    eraseEOL(rect);

    size_t eraseStart = m_CursorY + 1;

    size_t top = eraseStart * m_pParentXterm->m_pNormalFont->getHeight();

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft, m_OffsetTop + top, m_FbWidth, m_pParentXterm->m_pNormalFont->getHeight() * (m_Height - eraseStart));
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // m_pFramebuffer->rect(0, top, m_FbWidth, m_pParentXterm->m_pNormalFont->getHeight() * (m_Height - eraseStart), g_Colours[m_Bg], PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft, top + m_OffsetTop);
    rect.point(m_OffsetLeft + m_FbWidth, top + m_OffsetTop + ((m_Height - eraseStart) * m_pParentXterm->m_pNormalFont->getHeight()));

    for(size_t row = eraseStart; row < m_Height; row++)
    {
        for(size_t col = 0; col < m_Width; col++)
        {
            setChar(' ', col, row);
        }
    }

    renderArea(rect, 0, eraseStart, m_Width, m_Height - eraseStart);
}

void Xterm::Window::deleteCharacters(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::deleteCharacters(%zd)", n);
#endif

    // Start of the delete region
    ssize_t deleteStart = m_CursorX;

    // End of the delete region
    ssize_t deleteEnd = deleteStart + n;
    if (deleteEnd > m_RightMargin)
        deleteEnd = m_RightMargin;

    // Number of characters to shift
    ssize_t numChars = m_RightMargin - deleteEnd;

    // Shift all the characters across from the end of the delete area to the start.
    memmove(&m_pInsert[(m_CursorY * m_Stride) + deleteStart],
            &m_pInsert[(m_CursorY * m_Stride) + deleteEnd],
            numChars * sizeof(TermChar));

    // Now that the characters have been shifted, clear the space after
    // the region we copied.
    size_t left = (m_RightMargin - n) * m_pParentXterm->m_pNormalFont->getWidth();
    size_t top = m_CursorY * m_pParentXterm->m_pNormalFont->getHeight();

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft + left, m_OffsetTop + top,
        n * m_pParentXterm->m_pNormalFont->getWidth(), m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    // Update the moved section
    ssize_t row = m_CursorY, col = 0;
    for(col = (m_RightMargin - n); col < m_RightMargin; col++)
    {
        setChar(' ', col, row);
    }
    for(col = deleteStart; col < m_RightMargin; col++)
    {
        render(rect, 0, col, row);
    }

    renderArea(rect, deleteStart, m_CursorY, m_RightMargin - deleteStart, 1);
}

void Xterm::Window::insertCharacters(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::insertCharacters(%zd)", n);
#endif

    // Start of the insertion region
    ssize_t insertStart = m_CursorX;

    // End of the insertion region
    ssize_t insertEnd = insertStart + n;

    // Number of characters to shift.
    ssize_t numChars = m_RightMargin - insertEnd;

    // Shift characters.
    memmove(&m_pInsert[(m_CursorY * m_Stride) + insertEnd],
            &m_pInsert[(m_CursorY * m_Stride) + insertStart],
            numChars * sizeof(TermChar));

    // Now that the characters have been shifted, clear the space inside the region we inserted
    size_t left = insertStart * m_pParentXterm->m_pNormalFont->getWidth();
    size_t top = m_CursorY * m_pParentXterm->m_pNormalFont->getHeight();

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    size_t bg = m_Bg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg))
    {
        bg = m_Fg;
    }

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((g_Colours[bg] >> 16) & 0xFF) / 256.0,
            ((g_Colours[bg] >> 8) & 0xFF) / 256.0,
            ((g_Colours[bg]) & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, m_OffsetLeft + left, m_OffsetTop + top,
        n * m_pParentXterm->m_pNormalFont->getWidth(), m_pParentXterm->m_pNormalFont->getHeight());
    //cairo_fill(m_pParentXterm->m_pCairo);

    cairo_restore(m_pParentXterm->m_pCairo);

    rect.point(m_OffsetLeft + left, m_OffsetTop + top);
    rect.point(m_OffsetLeft + left + (n * m_pParentXterm->m_pNormalFont->getWidth()), m_OffsetTop + top + m_pParentXterm->m_pNormalFont->getHeight());

    // Update the inserted section
    ssize_t row = m_CursorY, col = 0;
    for(col = insertStart; col < insertEnd; col++)
    {
        setChar(' ', col, row);
    }

    // Update the moved section
    for(col = insertStart; col < m_RightMargin; col++)
    {
        render(rect, 0, col, row);
    }

    renderArea(rect, insertStart, m_CursorY, m_RightMargin - insertStart, 1);
}

void Xterm::Window::insertLines(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::insertLines(%zd)", n);
#endif

    if (m_CursorY + (ssize_t) n >= m_ScrollEnd)
        n = m_ScrollEnd - m_CursorY;

    // This will perform the correct scroll down, but then we need to erase
    // the current line to actually give the illusion of insertion.
    eraseLine(rect);
    scrollRegionDown(n, rect);
}

void Xterm::Window::deleteLines(size_t n, DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::deleteLines(%zd)", n);
#endif

    if (m_CursorY + (ssize_t) n >= m_ScrollEnd)
        n = m_ScrollEnd - m_CursorY;

    // This will scroll the rest of the lines into this one.
    scrollRegionUp(n, rect);
}

void Xterm::Window::lineRender(uint32_t utf32, DirtyRectangle &rect)
{
    klog(LOG_NOTICE, "line render: %c", utf32);

    size_t left = m_OffsetLeft + (m_LeftMargin * m_pParentXterm->m_pNormalFont->getWidth()) + (m_CursorX * m_pParentXterm->m_pNormalFont->getWidth());
    size_t top = m_OffsetTop + (m_ScrollStart * m_pParentXterm->m_pNormalFont->getHeight()) + (m_CursorY * m_pParentXterm->m_pNormalFont->getHeight());

    checkWrap(rect);
    if (m_CursorX < m_RightMargin)
        ++m_CursorX;

    size_t fullWidth = m_pParentXterm->m_pNormalFont->getWidth();
    size_t fullHeight = m_pParentXterm->m_pNormalFont->getHeight();

    size_t halfWidth = (fullWidth / 2) + 1;
    size_t halfHeight = (fullHeight / 2) + 1;

    size_t bg = m_Bg;
    size_t fg = m_Fg;
    if ((m_pParentXterm->getModes() & Screen) && (bg == g_DefaultBg) && (fg == g_DefaultFg))
    {
        bg = m_Fg;
        fg = m_Bg;
    }

    uint32_t bgColourInt = g_Colours[bg];
    uint32_t fgColourInt = g_Colours[fg];

    cairo_save(m_pParentXterm->m_pCairo);
    cairo_set_operator(m_pParentXterm->m_pCairo, CAIRO_OPERATOR_SOURCE);

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((bgColourInt >> 16) & 0xFF) / 256.0,
            ((bgColourInt >> 8) & 0xFF) / 256.0,
            (bgColourInt & 0xFF) / 256.0,
            0.8);

    cairo_rectangle(m_pParentXterm->m_pCairo, left, top, fullWidth, fullHeight);
    cairo_fill(m_pParentXterm->m_pCairo);

    cairo_set_source_rgba(
            m_pParentXterm->m_pCairo,
            ((fgColourInt >> 16) & 0xFF) / 256.0,
            ((fgColourInt >> 8) & 0xFF) / 256.0,
            (fgColourInt & 0xFF) / 256.0,
            1.0);

    // m_pFramebuffer->rect(left, top, fullWidth, fullHeight, bgColourInt, PedigreeGraphics::Bits24_Rgb);

    rect.point(m_OffsetLeft + left, m_OffsetTop + top);
    rect.point(m_OffsetLeft + left + fullWidth, m_OffsetTop + top + fullHeight);

    switch(utf32 & 0xFF)
    {
        case 'j':
            // Bottom right corner

            // Middle left to Center
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Center to Middle top
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'k':
            // Upper right corner

            // Middle left to Center
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Center to Middle bottom
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'l':
            // Upper left corner

            // Center to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Center to Middle bottom
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'm':
            // Lower left corner

            // Center to Middle top
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Center to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'n':
            // Crossing lines

            // Middle left to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Middle top to Middle bottom
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'q':
            // Horizontal line

            // Middle left to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 't':
            // Left 'T'
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'u':
            // Right 'T'

            // Middle top to Middle bottom
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Middle left to Center
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'v':
            // Bottom 'T'

            // Middle left to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Middle top to Center
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'w':
            // Top 'T'

            // Middle left to Middle right
            cairo_move_to(m_pParentXterm->m_pCairo, left, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + fullWidth, top + halfHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left, top + halfHeight, left + fullWidth, top + halfHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);

            // Middle bottom to Center
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top + halfHeight);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top + halfHeight, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        case 'x':
            // Vertical line

            // Middle top to Middle bottom
            cairo_move_to(m_pParentXterm->m_pCairo, left + halfWidth, top);
            cairo_line_to(m_pParentXterm->m_pCairo, left + halfWidth, top + fullHeight);
            cairo_stroke(m_pParentXterm->m_pCairo);

            //m_pFramebuffer->line(left + halfWidth, top, left + halfWidth, top + fullHeight, fgColourInt, PedigreeGraphics::Bits24_Rgb);
            break;
        default:
            break;
    }

    cairo_restore(m_pParentXterm->m_pCairo);
}

void Xterm::Window::checkWrap(DirtyRectangle &rect)
{
    if(m_CursorX >= m_RightMargin)
    {
        // Default autowrap mode is off - new characters at
        // the right margin replace any that are already there.
        if(m_pParentXterm->getModes() & AutoWrap)
        {
            m_CursorX = m_LeftMargin;
            ++m_CursorY;

            checkScroll(rect);
        }
        else
        {
            m_CursorX = m_RightMargin - 1;
        }
    }
}

void Xterm::Window::checkScroll(DirtyRectangle &rect)
{
    // Handle scrolling, which can take place due to linefeeds and
    // other such cursor movements.
    if(m_CursorY < m_ScrollStart)
    {
        // By how much have we exceeded the scroll region?
        size_t numRows = (m_ScrollStart - m_CursorY);
        scrollRegionDown(numRows, rect);
        m_CursorY = m_ScrollStart;
    }
    else if(m_CursorY > m_ScrollEnd)
    {
        // By how much have we exceeded the scroll region?
        size_t numRows = (m_CursorY - m_ScrollEnd);
        scrollRegionUp(numRows, rect);
        m_CursorY = m_ScrollEnd;
    }
}

void Xterm::Window::invert(DirtyRectangle &rect)
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::invert()");
#endif

    // Invert the entire screen, if using default colours.
    for (size_t y = 0; y < m_Height; y++)
    {
        for (size_t x = 0; x < m_Width; x++)
        {
            TermChar *pChar = &m_pView[(y * m_Stride) + x];
            if((pChar->fore == g_DefaultFg) && (pChar->back == g_DefaultBg))
            {
                uint8_t fore = pChar->fore;
                pChar->fore = pChar->back;
                pChar->back = fore;
            }
        }
    }

    // Redraw.
    renderAll(rect, this);
}

void Xterm::Window::setTabStop()
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::setTabStop() [@%zd]", m_CursorX);
#endif

    m_pParentXterm->m_TabStops[m_CursorX] = '|';
}

void Xterm::Window::clearTabStop()
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::clearTabStop() [@%zd]", m_CursorX);
#endif

    m_pParentXterm->m_TabStops[m_CursorX] = 0;
}

void Xterm::Window::clearAllTabStops()
{
#ifdef XTERM_DEBUG
    klog(LOG_INFO, "Xterm::Window::clearAllTabStops()");
#endif

    memset(m_pParentXterm->m_TabStops, 0, m_Stride);
}

