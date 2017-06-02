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

#include "Keyboard.h"
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/machine/HidInputManager.h"
#include "pedigree/kernel/machine/KeymapManager.h"
#include "pedigree/kernel/machine/Machine.h"

#ifdef DEBUGGER
#include "pedigree/kernel/debugger/Debugger.h"

#ifdef TRACK_PAGE_ALLOCATIONS
#include "pedigree/kernel/debugger/commands/AllocationCommand.h"
#endif
#endif

#ifdef DEBUGGER
#include "pedigree/kernel/debugger/commands/SlamCommand.h"
#endif

#include "pedigree/kernel/core/SlamAllocator.h"

X86Keyboard::X86Keyboard(uint32_t portBase)
    : m_bDebugState(false), m_Escape(KeymapManager::EscapeNone), m_pBase(0),
      m_IrqId(0), m_LedState(0)
{
}

X86Keyboard::~X86Keyboard()
{
}

void X86Keyboard::initialise()
{
    auto f = [this](Device *p) {
        if (m_pBase)
        {
            return p;
        }

        if (p->addresses().count() > 0)
        {
            if (p->addresses()[0]->m_Name == "ps2-base")
            {
                m_pBase = p->addresses()[0]->m_Io;
            }
        }

        return p;
    };
    auto c = pedigree_std::make_callable(f);
    Device::foreach (c, 0);

    if (!m_pBase)
    {
        // Handle the impossible case properly
        FATAL("X86Keyboard: Could not find the PS/2 base ports in the device "
              "tree");
    }

    // Register the irq
    IrqManager &irqManager = *Machine::instance().getIrqManager();
    m_IrqId = irqManager.registerIsaIrqHandler(1, this, true);
    if (m_IrqId == 0)
    {
        ERROR("X86Keyboard: failed to register IRQ handler!");
    }

    irqManager.control(1, IrqManager::MitigationThreshold, 100);
}

bool X86Keyboard::irq(irq_id_t number, InterruptState &state)
{
    if (m_bDebugState)
        return true;

    // Get the keyboard's status byte
    uint8_t status = m_pBase->read8(4);
    if (!(status & 0x01))
        return true;

    // Get the scancode for the pending keystroke
    uint8_t scancode = m_pBase->read8(0);

// Check for keys with special functions
#ifdef DEBUGGER
#if CRIPPLINGLY_VIGILANT
    if (scancode == 0x43)  // F9
        SlamAllocator::instance().setVigilance(true);
    if (scancode == 0x44)  // F10
        SlamAllocator::instance().setVigilance(false);
#endif
    if (scancode == 0x57)  // F11
    {
#ifdef TRACK_PAGE_ALLOCATIONS
        g_AllocationCommand.checkpoint();
#endif
        g_SlamCommand.clean();
    }
    if (scancode == 0x58)  // F12
    {
        LargeStaticString sError;
        sError += "User-induced breakpoint";
        Debugger::instance().start(state, sError);
    }
#endif

    // Check for LED manipulation
    if (scancode & 0x80)
    {
        uint8_t code = scancode & ~0x80;
        if (code == 0x3A)
        {
            DEBUG_LOG("X86Keyboard: Caps Lock toggled");
            m_LedState ^= Keyboard::CapsLock;
            setLedState(m_LedState);
        }
        else if (code == 0x45)
        {
            DEBUG_LOG("X86Keyboard: Num Lock toggled");
            m_LedState ^= Keyboard::NumLock;
            setLedState(m_LedState);
        }
        else if (code == 0x46)
        {
            DEBUG_LOG("X86Keyboard: Scroll Lock toggled");
            m_LedState ^= Keyboard::ScrollLock;
            setLedState(m_LedState);
        }
    }

    // Get the HID keycode corresponding to the scancode
    uint8_t keyCode =
        KeymapManager::instance().convertPc102ScancodeToHidKeycode(
            scancode, m_Escape);
    if (!keyCode)
        return true;

    // Send the keycode to the HID input manager
    if (scancode & 0x80)
        HidInputManager::instance().keyUp(keyCode);
    else
        HidInputManager::instance().keyDown(keyCode);
    return true;
}

char X86Keyboard::getChar()
{
    if (m_bDebugState)
    {
        uint8_t scancode, status;
        do
        {
            // Get the keyboard's status byte
            status = m_pBase->read8(4);
        } while (!(status & 0x01));  // Spin until there's a key ready

        // Get the scancode for the pending keystroke
        scancode = m_pBase->read8(0);

        // Convert the scancode into ASCII and return it
        return scancodeToAscii(scancode);
    }
    else
    {
        ERROR("Keyboard::getChar() should not be called outside debug mode");
        return 0;
    }
}

char X86Keyboard::getCharNonBlock()
{
    if (m_bDebugState)
    {
        uint8_t scancode, status;
        // Get the keyboard's status byte
        status = m_pBase->read8(4);
        if (!(status & 0x01))
            return 0;

        // Get the scancode for the pending keystroke
        scancode = m_pBase->read8(0);

        // Convert the scancode into ASCII and return it
        return scancodeToAscii(scancode);
    }
    else
    {
        ERROR("Keyboard::getCharNonBlock should not be called outside debug "
              "mode");
        return 0;
    }
}

void X86Keyboard::setDebugState(bool enableDebugState)
{
    m_bDebugState = enableDebugState;
    IrqManager &irqManager = *Machine::instance().getIrqManager();
    if (m_bDebugState)
    {
        irqManager.enable(1, false);

        // Disable the PS/2 mouse
        m_pBase->write8(0xD4, 4);
        m_pBase->write8(0xF5, 4);
    }
    else
    {
        irqManager.enable(1, true);

        // Enable the PS/2 mouse
        m_pBase->write8(0xD4, 4);
        m_pBase->write8(0xF4, 4);
    }
}

bool X86Keyboard::getDebugState()
{
    return m_bDebugState;
}

char X86Keyboard::scancodeToAscii(uint8_t scancode)
{
    // Get the HID keycode corresponding to the scancode
    uint8_t keyCode =
        KeymapManager::instance().convertPc102ScancodeToHidKeycode(
            scancode, m_Escape);
    if (!keyCode)
        return 0;

    uint64_t key = 0;
    // Let KeymapManager handle the modifiers
    if (!KeymapManager::instance().handleHidModifier(
            keyCode, !(scancode & 0x80)) &&
        !(scancode & 0x80))
        key = KeymapManager::instance().resolveHidKeycode(
            keyCode);  // Get the actual key

    // Check for special keys
    if (key & Keyboard::Special)
    {
        char a = key & 0xFF, b = (key >> 8) & 0xFF, c = (key >> 16) & 0xFF,
             d = (key >> 24) & 0xFF;
        // Up
        if (a == 'u' && b == 'p')
            return 'j';
        // Down
        if (a == 'd' && b == 'o' && c == 'w' && d == 'n')
            return 'k';
        // PageUp
        if (a == 'p' && b == 'g' && c == 'u' && d == 'p')
            return '\b';
        // PageDown
        if (a == 'p' && b == 'g' && c == 'd' && d == 'n')
            return ' ';
    }
    // Discard non-ASCII keys
    if ((key & Keyboard::Special) || ((key & 0xFFFFFFFF) > 0x7f))
        return 0;
    return static_cast<char>(key & 0x7f);
}

char X86Keyboard::getLedState()
{
    return m_LedState;
}

void X86Keyboard::setLedState(char state)
{
    m_LedState = state;

    while (m_pBase->read8(4) & 2)
        ;
    m_pBase->write8(0xED, 0);
    while (m_pBase->read8(4) & 2)
        ;
    m_pBase->write8(state, 0);
}
