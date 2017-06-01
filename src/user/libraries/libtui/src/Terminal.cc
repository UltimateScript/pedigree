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

#include "Terminal.h"
#include "environment.h"
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>

#include <native/graphics/Graphics.h>

#include <cairo/cairo.h>

extern PedigreeGraphics::Framebuffer *g_pFramebuffer;

Terminal::Terminal(
    char *pName, size_t nWidth, size_t nHeight, size_t offsetLeft,
    size_t offsetTop, rgb_t *pBackground, cairo_t *pCairo,
    class Widget *pWidget, class Tui *pTui, class Font *pNormalFont,
    class Font *pBoldFont)
    : m_pBuffer(0), m_pFramebuffer(0), m_pXterm(0), m_Len(0),
      m_WriteBufferLen(0), m_bHasPendingRequest(false), m_PendingRequestSz(0),
      m_Pid(0), m_OffsetLeft(offsetLeft), m_OffsetTop(offsetTop), m_Cancel(0),
      m_WriteInProgress(0)
{
    cairo_save(pCairo);
    cairo_set_operator(pCairo, CAIRO_OPERATOR_SOURCE);

    cairo_set_source_rgba(pCairo, 0, 0, 0, 0.8);

    cairo_rectangle(pCairo, m_OffsetLeft, m_OffsetTop, nWidth, nHeight);
    cairo_fill(pCairo);

    cairo_restore(pCairo);

    strncpy(m_pName, pName, 256);
    m_pName[255] = 0;

#ifndef NEW_XTERM
    m_pXterm = new Xterm(
        0, nWidth, nHeight, m_OffsetLeft, m_OffsetTop, this, pWidget, pTui,
        pNormalFont, pBoldFont);
#else
    Display::ScreenMode mode;
    mode.width = nWidth - 1;
    mode.height = nHeight - offsetTop - 1;
    mode.pf.mRed = 0xFF;
    mode.pf.mGreen = 0xFF;
    mode.pf.mBlue = 0xFF;
    mode.pf.pRed = 16;
    mode.pf.pGreen = 8;
    mode.pf.pBlue = 0;
    mode.pf.nBpp = 24;
    mode.pf.nPitch = nWidth * 3;
    m_pXterm = new Vt100(
        mode, reinterpret_cast<uint8_t *>(m_pBuffer) + nWidth * offsetTop);
#endif
}

bool Terminal::initialise()
{
    m_MasterPty = posix_openpt(O_RDWR | O_NOCTTY);
    if (m_MasterPty < 0)
    {
        klog(LOG_INFO, "TUI: Couldn't create terminal: %s", strerror(errno));
        return false;
    }

#ifdef TARGET_LINUX
    grantpt(m_MasterPty);
    unlockpt(m_MasterPty);
#endif
    char slavename[16] = {0};
    strncpy(slavename, ptsname(m_MasterPty), 16);
    slavename[15] = 0;

    struct winsize ptySize;
    memset(&ptySize, 0, sizeof(ptySize));
    ptySize.ws_row = m_pXterm->getRows();
    ptySize.ws_col = m_pXterm->getCols();
    ioctl(m_MasterPty, TIOCSWINSZ, &ptySize);

    // Fire up a shell session.
    int pid = m_Pid = fork();
    if (pid == -1)
    {
        klog(LOG_INFO, "TUI: Couldn't fork: %s", strerror(errno));
        DirtyRectangle rect;
        write("Couldn't fork: ", rect);
        write(strerror(errno), rect);
        redrawAll(rect);
        return false;
    }
    else if (pid == 0)
    {
        close(0);
        close(1);
        close(2);
        close(m_MasterPty);

        // Create a new session for the shell, which will also wipe any
        // existing CTTY.
        setsid();

        // Open the slave terminal with correct rights.
        int n = open(slavename, O_RDONLY);
        open(slavename, O_WRONLY);
        open(slavename, O_WRONLY);

        if (n < 0)
        {
            klog(LOG_INFO, "opening %s failed", slavename);
            klog(
                LOG_INFO, "opening stdin failed %d %s", errno, strerror(errno));
        }

        // Mark opened slave as our ctty.
        klog(LOG_INFO, "Trying to set CTTY");
        ioctl(1, TIOCSCTTY, 0);

        // Set ourselves as the terminal's foreground process group.
        tcsetpgrp(1, getpgrp());

        // We emulate an xterm, so ensure that's set in the environment.
        setenv("TERM", "xterm", 1);

        // Get current user's shell.
        struct passwd *pw = getpwuid(getuid());

        // Program to run.
        const char *prog = pw->pw_shell;
        if (!prog)
        {
            prog = getenv("SHELL");
            if (!prog)
            {
                // Fall back to bash
                klog(
                    LOG_WARNING,
                    "$SHELL unset, falling back to /applications/bash");
                prog = "/applications/bash";
            }
        }

// Create utmpx entry.
#ifndef TARGET_LINUX
        /// \todo Clean it up when the child terminates.
        struct utmpx ut;
        ut.ut_type = USER_PROCESS;
        ut.ut_pid = getpid();
        const char *ttyid = slavename + strlen("/dev/");
        strncpy(ut.ut_id, ttyid + strlen("tty"), UT_LINESIZE);
        strncpy(ut.ut_line, ttyid, UT_LINESIZE);
        strncpy(ut.ut_user, pw->pw_name, UT_NAMESIZE);
        gettimeofday(&ut.ut_tv, NULL);

        setutxent();
        pututxline(&ut);
        endutxent();
#endif

        // Launch the shell now.
        execl(prog, prog, NULL);
        klog(
            LOG_ALERT,
            "Launching shell failed (next line is the error in errno...)");
        klog(LOG_ALERT, "error: %s", strerror(errno));

        DirtyRectangle rect;
        write("Couldn't load shell for this terminal... ", rect);
        write(strerror(errno), rect);
        write(
            "\r\n\r\nYour installation of Pedigree may not be complete, or you "
            "may have hit a bug.",
            rect);
        redrawAll(rect);

        exit(1);
    }

    m_Pid = pid;

    return true;
}

Terminal::~Terminal()
{
    if (m_Pid)
    {
        // Kill child.
        kill(m_Pid, SIGTERM);

        // Reap child.
        waitpid(m_Pid, 0, 0);
    }

    delete m_pXterm;
}

bool Terminal::isAlive()
{
    if (m_Pid)
    {
        // Is our child still alive?
        pid_t result = waitpid(m_Pid, 0, WNOHANG);
        if (result == m_Pid)
        {
            m_Pid = 0;
            return false;
        }
    }
    return true;
}

void Terminal::renewBuffer(size_t nWidth, size_t nHeight)
{
    m_pXterm->resize(nWidth, nHeight, 0);

    /// \todo Send SIGWINCH in console layer.
    struct winsize ptySize;
    ptySize.ws_row = m_pXterm->getRows();
    ptySize.ws_col = m_pXterm->getCols();
    ioctl(m_MasterPty, TIOCSWINSZ, &ptySize);
}

void Terminal::processKey(uint64_t key)
{
    m_pXterm->processKey(key);
}

char Terminal::getFromQueue()
{
    if (m_Len > 0)
    {
        char c = m_pQueue[0];
        for (size_t i = 0; i < m_Len - 1; i++)
            m_pQueue[i] = m_pQueue[i + 1];
        m_Len--;
        return c;
    }
    else
        return 0;
}

void Terminal::clearQueue()
{
    m_Len = 0;
}

void Terminal::write(const char *pStr, DirtyRectangle &rect)
{
    m_pXterm->hideCursor(rect);

    bool bWasAlreadyRunning = m_WriteInProgress;
    m_WriteInProgress = true;
    // klog(LOG_NOTICE, "Beginning write...");
    while (!m_Cancel && (*pStr || m_WriteBufferLen))
    {
        // Fill the buffer.
        while (*pStr && !m_Cancel)
        {
            if (m_WriteBufferLen < 4)
                m_pWriteBuffer[m_WriteBufferLen++] = *pStr++;
            else
                break;
        }
        if (m_Cancel)  // Check break point from above loop
            break;
        // Begin UTF-8 -> UTF-32 conversion.
        /// \todo Add some checking - every successive byte should start with
        /// 0b10.
        uint32_t utf32;
        size_t nBytes;
        if ((m_pWriteBuffer[0] & 0x80) == 0x00)
        {
            utf32 = static_cast<uint32_t>(m_pWriteBuffer[0]);
            nBytes = 1;
        }
        else if ((m_pWriteBuffer[0] & 0xE0) == 0xC0)
        {
            if (m_WriteBufferLen < 2)
                return;
            utf32 = ((static_cast<uint32_t>(m_pWriteBuffer[0]) & 0x1F) << 6) |
                    (static_cast<uint32_t>(m_pWriteBuffer[1]) & 0x3F);
            nBytes = 2;
        }
        else if ((m_pWriteBuffer[0] & 0xF0) == 0xE0)
        {
            if (m_WriteBufferLen < 3)
                return;
            utf32 = ((static_cast<uint32_t>(m_pWriteBuffer[0]) & 0x0F) << 12) |
                    ((static_cast<uint32_t>(m_pWriteBuffer[1]) & 0x3F) << 6) |
                    (static_cast<uint32_t>(m_pWriteBuffer[2]) & 0x3F);
            nBytes = 3;
        }
        else if ((m_pWriteBuffer[0] & 0xF8) == 0xF0)
        {
            if (m_WriteBufferLen < 4)
                return;
            utf32 = ((static_cast<uint32_t>(m_pWriteBuffer[0]) & 0x0F) << 18) |
                    ((static_cast<uint32_t>(m_pWriteBuffer[1]) & 0x3F) << 12) |
                    ((static_cast<uint32_t>(m_pWriteBuffer[2]) & 0x3F) << 6) |
                    (static_cast<uint32_t>(m_pWriteBuffer[3]) & 0x3F);
            nBytes = 4;
        }
        else
        {
            m_WriteBufferLen = 0;
            continue;
        }

        memmove(m_pWriteBuffer, &m_pWriteBuffer[nBytes], 4 - nBytes);
        m_WriteBufferLen -= nBytes;

// End UTF-8 -> UTF-32 conversion.
#ifndef NEW_XTERM
        m_pXterm->write(utf32, rect);
#else
        rect.point(m_OffsetLeft, m_OffsetTop);
        rect.point(
            m_pXterm->getCols() * 8 + m_OffsetLeft,
            m_pXterm->getRows() * 16 + m_OffsetTop);
        m_pXterm->write(static_cast<uint8_t>(utf32 & 0xFF));
#endif
    }
    // klog(LOG_NOTICE, "Completed write [%scancelled]...", m_Cancel ? "" : "not
    // ");

    if (!bWasAlreadyRunning)
    {
        if (m_Cancel)
            m_Cancel = 0;
        m_WriteInProgress = false;
    }

    m_pXterm->showCursor(rect);
}

void Terminal::addToQueue(char c, bool bFlush)
{
    // Don't allow keys to be pressed past the buffer's size
    if ((c != 0) && (m_Len >= 256))
    {
        return;
    }
    if ((c == 0) && (!m_Len))
    {
        return;
    }

    if (c)
        m_pQueue[m_Len++] = c;

    if (bFlush)
    {
        ssize_t result = ::write(m_MasterPty, m_pQueue, m_Len);
        if (result >= 0)
        {
            size_t missing = m_Len - result;
            if (missing)
                memmove(m_pQueue, &m_pQueue[result], missing);
            m_Len = missing;
        }
        else
        {
            klog(LOG_ALERT, "Terminal::addToQueue flush failed");
        }
    }
}

void Terminal::setActive(bool b, DirtyRectangle &rect)
{
    // Force complete redraw
    // m_pFramebuffer->redraw(0, 0, m_pFramebuffer->getWidth(),
    // m_pFramebuffer->getHeight(), false);

    // if (b)
    //    Syscall::setCurrentBuffer(m_pBuffer);
}
