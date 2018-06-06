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

#include "pedigree/kernel/config/MemoryBackend.h"
#include "pedigree/kernel/utilities/new"

MemoryBackend::MemoryBackend(const String &configStore)
    : ConfigurationBackend(configStore), m_Tables(), m_TypeName("MemoryBackend")
{
}

MemoryBackend::~MemoryBackend()
{
    ConfigurationManager::instance().removeBackend(m_ConfigStore);
}

size_t MemoryBackend::createTable(const String &table)
{
    m_Tables.insert(table, new Table());
    return 0;
}

void MemoryBackend::insert(
    const String &table, const String &key, const ConfigValue &value)
{
    RadixTree<Table *>::LookupType result = m_Tables.lookup(table);
    if (!result.hasValue())
        return;

    ConfigValue *pConfigValue = new ConfigValue(value);
    result.value()->m_Rows.insert(key, pConfigValue);
}

ConfigValue &MemoryBackend::select(const String &table, const String &key)
{
    static ConfigValue v;
    v.type = Invalid;

    RadixTree<Table *>::LookupType result = m_Tables.lookup(table);
    if (!result.hasValue())
        return v;

    RadixTree<ConfigValue *>::LookupType val =
        result.value()->m_Rows.lookup(key);
    if (val.hasValue())
        return *val.value();
    else
        return v;
}

void MemoryBackend::watch(
    const String &table, const String &key, ConfigurationWatcher watcher)
{
    RadixTree<Table *>::LookupType result = m_Tables.lookup(table);
    if (!result.hasValue())
        return;

    RadixTree<ConfigValue *>::LookupType val =
        result.value()->m_Rows.lookup(key);
    if (val.hasValue())
    {
        for (int i = 0; i < MAX_WATCHERS; i++)
        {
            if (val.value()->watchers[i] == 0)
            {
                val.value()->watchers[i] = watcher;
                break;
            }
        }
    }
}

void MemoryBackend::unwatch(
    const String &table, const String &key, ConfigurationWatcher watcher)
{
    RadixTree<Table *>::LookupType result = m_Tables.lookup(table);
    if (!result.hasValue())
        return;

    RadixTree<ConfigValue *>::LookupType val =
        result.value()->m_Rows.lookup(key);
    if (val.hasValue())
    {
        for (int i = 0; i < MAX_WATCHERS; i++)
        {
            if (val.value()->watchers[i] == watcher)
            {
                val.value()->watchers[i] = 0;
                break;
            }
        }
    }
}

const String &MemoryBackend::getTypeName()
{
    return m_TypeName;
}
