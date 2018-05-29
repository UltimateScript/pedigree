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

#include "pedigree/kernel/ServiceManager.h"
#include "pedigree/kernel/utilities/new"

class Service;

ServiceManager ServiceManager::m_Instance;

ServiceManager::ServiceManager() : m_Services()
{
}

ServiceManager::~ServiceManager()
{
    /// \todo Delete all the pointers!
}

ServiceFeatures *ServiceManager::enumerateOperations(const String &serviceName)
{
    RadixTree<InternalService *>::LookupType result =
        m_Services.lookup(serviceName);
    if (result.hasValue())
        return result.value()->pFeatures;
    else
        return 0;
}

void ServiceManager::addService(
    const String &serviceName, Service *s, ServiceFeatures *feats)
{
    InternalService *p = new InternalService;
    p->pService = s;
    p->pFeatures = feats;
    m_Services.insert(serviceName, p);
}

void ServiceManager::removeService(const String &serviceName)
{
    m_Services.remove(serviceName);
}

Service *ServiceManager::getService(const String &serviceName)
{
    RadixTree<InternalService *>::LookupType result =
        m_Services.lookup(serviceName);
    if (result.hasValue())
        return result.value()->pService;
    else
        return 0;
}
