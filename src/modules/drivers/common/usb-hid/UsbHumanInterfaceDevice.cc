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

#include "modules/system/usb/UsbConstants.h"
#include "modules/system/usb/UsbDevice.h"
#include "modules/system/usb/UsbHub.h"
#include "pedigree/kernel/machine/HidInputManager.h"
#include "pedigree/kernel/machine/InputManager.h"
#include "pedigree/kernel/utilities/PointerGuard.h"
#include <hid/HidReport.h>

#include "UsbHumanInterfaceDevice.h"

UsbHumanInterfaceDevice::UsbHumanInterfaceDevice(UsbDevice *pDev)
    : UsbDevice(pDev), m_pInEndpoint(0)
{
}

UsbHumanInterfaceDevice::~UsbHumanInterfaceDevice()
{
}

void UsbHumanInterfaceDevice::initialiseDriver()
{
    // Get the HID descriptor
    HidDescriptor *pHidDescriptor = 0;
    for (size_t i = 0; i < m_pInterface->otherDescriptorList.count(); i++)
    {
        UnknownDescriptor *pDescriptor = m_pInterface->otherDescriptorList[i];
        if (pDescriptor->nType == 0x21)
        {
            pHidDescriptor = new HidDescriptor(pDescriptor);
            break;
        }
    }
    if (!pHidDescriptor)
    {
        ERROR("USB: HID: No HID descriptor");
        return;
    }
    PointerGuard<HidDescriptor> guard(pHidDescriptor);

    // Set Idle Rate to 0
    controlRequest(
        UsbRequestType::Class | UsbRequestRecipient::Interface,
        UsbRequest::GetInterface, 0, 0);

    // Get the report descriptor
    uint16_t nReportDescriptorSize = pHidDescriptor->nDescriptorLength;
    uint8_t *pReportDescriptor = static_cast<uint8_t *>(getDescriptor(
        0x22, 0, nReportDescriptorSize, UsbRequestRecipient::Interface));
    PointerGuard<uint8_t> guard2(pReportDescriptor);

    // Create a new report instance and use it to parse the report descriptor
    m_pReport = new HidReport();
    m_pReport->parseDescriptor(pReportDescriptor, nReportDescriptorSize);

    // Search for an interrupt IN endpoint
    for (size_t i = 0; i < m_pInterface->endpointList.count(); i++)
    {
        Endpoint *pEndpoint = m_pInterface->endpointList[i];
        if (pEndpoint->nTransferType == Endpoint::Interrupt && pEndpoint->bIn)
        {
            m_pInEndpoint = pEndpoint;
            break;
        }
    }

    // Did we end up with no interrupt IN endpoint?
    if (!m_pInEndpoint)
    {
        ERROR("USB: HID: No Interrupt IN endpoint");
        delete m_pReport;
        return;
    }

    // Allocate the input report buffers and zero them
    m_pInReportBuffer = new uint8_t[m_pInEndpoint->nMaxPacketSize];
    m_pOldInReportBuffer = new uint8_t[m_pInEndpoint->nMaxPacketSize];
    ByteSet(m_pInReportBuffer, 0, m_pInEndpoint->nMaxPacketSize);
    ByteSet(m_pOldInReportBuffer, 0, m_pInEndpoint->nMaxPacketSize);

    // Add the input handler
    addInterruptInHandler(
        m_pInEndpoint, reinterpret_cast<uintptr_t>(m_pInReportBuffer),
        m_pInEndpoint->nMaxPacketSize, callback,
        reinterpret_cast<uintptr_t>(this));

    m_UsbState = HasDriver;
}

void UsbHumanInterfaceDevice::callback(uintptr_t pParam, ssize_t ret)
{
    UsbHumanInterfaceDevice *pHid =
        reinterpret_cast<UsbHumanInterfaceDevice *>(pParam);
    pHid->inputHandler();
}

void UsbHumanInterfaceDevice::inputHandler()
{
    // Do we have a report instance?
    if (!m_pReport)
        return;

    // Feed the report instance with the input we just got
    m_pReport->feedInput(
        m_pInReportBuffer, m_pOldInReportBuffer, m_pInEndpoint->nMaxPacketSize);
}
