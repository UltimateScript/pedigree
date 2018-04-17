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

#ifndef KERNEL_UTILITIES_HASHTABLE_H
#define KERNEL_UTILITIES_HASHTABLE_H

#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/utilities/lib.h"
#include "pedigree/kernel/utilities/Iterator.h"
#include "pedigree/kernel/utilities/Result.h"
#include "pedigree/kernel/utilities/Pair.h"
#include "pedigree/kernel/Log.h"

/** @addtogroup kernelutilities
 * @{ */

namespace HashTableError
{
    enum Error
    {
        HashTableEmpty,
        NotFound,
        IterationComplete
    };
}

/**
 * Hash table class.
 *
 * Handles hash collisions by chaining keys.
 *
 * The key type 'K' should have a method hash() which returns a
 * size_t hash that can be used to index into the bucket array.
 *
 * The key type 'K' should also be able to compare against other 'K'
 * types for equality.
 *
 * GrowthFactor defines how quickly the bucket count should grow. The default
 * of two balances memory usage against performance, but some use cases would
 * be better served by significant growth in each resize.
 *
 * \todo check InitialBuckets for is a power of two
 */
template <class K, class V, size_t InitialBuckets = 4, bool QuadraticProbe = true, size_t GrowthFactor = 2>
class HashTable
{
   private:
    typedef HashTable<K, V, InitialBuckets, QuadraticProbe, GrowthFactor> SelfType;

    static_assert(
        InitialBuckets > 0, "At least one initial bucket must be available.");

    struct bucket
    {
        bucket() : key(), value(), set(false), parent(nullptr)
        {
        }

        K key;
        V value;
        bool set;
        HashTable *parent;

        struct bucket *next()
        {
            if (!parent)
            {
                return nullptr;
            }

            struct bucket *node = this;
            while (++node < (parent->m_Buckets + parent->m_nBuckets))
            {
                if (node->set)
                {
                    return node;
                }
            }

            return nullptr;
        }

        struct bucket *previous()
        {
            if (!parent)
            {
                return nullptr;
            }

            struct bucket *node = this;
            while (--node >= parent->m_Buckets)
            {
                if (node->set)
                {
                    return node;
                }
            }

            return nullptr;
        }
    };

   public:
    typedef ::Iterator<V, struct bucket> Iterator;
    typedef typename Iterator::Const ConstIterator;

    typedef Result<const V &, HashTableError::Error> LookupResult;
    typedef Result<Pair<K, V>, HashTableError::Error> PairLookupResult;

    HashTable() : m_Buckets(nullptr), m_Default(), m_nBuckets(0), m_nItems(0), m_nMask(0)
    {
    }

    /**
     * Constructor with custom default value.
     */
    HashTable(const V &customDefault) : HashTable()
    {
        m_Default = customDefault;
    }

    /**
     * Clear the HashTable.
     */
    void clear()
    {
        delete [] m_Buckets;
        m_Buckets = nullptr;
        m_nItems = 0;
    }

    ~HashTable()
    {
        clear();
    }

    /**
     * Check if the given key exists in the hash table.
     */
    bool contains(const K &k) const
    {
        if ((!m_Buckets) || (!m_nItems))
        {
            return false;
        }

        size_t hash = k.hash() & m_nMask;

        const bucket *b = &m_Buckets[hash];
        if (!b->set)
        {
            return false;
        }

        if (b->key != k)
        {
            b = findNextSet(hash, k);
            if (!b)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * Do a lookup of the given key, and return either the value,
     * or NULL if the key is not in the hashtable.
     *
     * O(1) in the average case, with a hash function that rarely
     * collides.
     */
    LookupResult lookup(const K &k) const
    {
        HashTableError::Error err;
        const struct bucket *b = doLookup(k, err);
        if (b)
        {
            return LookupResult::withValue(b->value);
        }
        else
        {
            return LookupResult::withError(err);
        }
    }

    /**
     * Get the nth item in the hash table.
     *
     * Because the table is unordered, this should only be used to provide an
     * indexed access into the table rather than used to find a specific item.
     * Insertions and removals may completely change the order of the table.
     */
    PairLookupResult getNth(size_t n) const
    {
        if (n < count())
        {
            size_t j = 0;
            for (size_t i = 0; i < m_nBuckets; ++i)
            {
                if (!m_Buckets[i].set)
                {
                    continue;
                }

                if (j++ == n)
                {
                    Pair<K, V> result(m_Buckets[i].key, m_Buckets[i].value);
                    return PairLookupResult::withValue(result);
                }
            }
        }

        return PairLookupResult::withError(HashTableError::IterationComplete);
    }

    /**
     * Insert the given value with the given key.
     */
    bool insert(const K &k, const V &v)
    {
        check();

        // Ensure we have space for the new item
        reserve(m_nItems + 1);

        size_t hash = k.hash() & m_nMask;

        // Do we need to chain?
        bucket *b = &m_Buckets[hash];
        if (b->set)
        {
            // If key matches, this is more than just a hash collision.
            if (b->key == k)
            {
                return false;
            }

            // Probe for an empty bucket.
            b = findNextEmpty(hash);
            if (!b)
            {
                return false;
            }
        }

        b->set = true;
        b->key = k;
        b->value = v;
        bool wasEmpty = m_nItems == 0;
        ++m_nItems;

        return true;
    }

    /**
     * Update the value at the given key.
     */
    bool update(const K &k, const V &v)
    {
        check();

        HashTableError::Error err;
        struct bucket *b = const_cast<struct bucket *>(doLookup(k, err));
        if (!b)
        {
            return false;
        }

        b->value = v;
        return true;
    }

    /**
     * Remove the given key.
     */
    void remove(const K &k)
    {
        if (!m_Buckets)
        {
            return;
        }

        size_t hash = k.hash() & m_nMask;

        bucket *b = &m_Buckets[hash];
        if (!b->set)
        {
            return;
        }

        bool didRemove = false;

        if (b->key == k)
        {
            b->set = false;
            b->value = m_Default;
            didRemove = true;
        }
        else
        {
            b = findNextSet(hash, k);
            if (b)
            {
                b->set = false;
                b->value = m_Default;
                didRemove = true;
            }
        }

        if (didRemove)
        {
            --m_nItems;

            if (m_nItems)
            {
                // Must rehash as we use linear probing for collision handling.
                rehash();
            }
        }
    }

    /**
     * Reserve space for the given number of items in the hash table.
     */
    void reserve(size_t numItems)
    {
        check();

        if (numItems < m_nBuckets)
        {
            // No resize necessary, reserve is completely contained within the
            // current bucket array.
            return;
        }

        if (!numItems)
        {
            ++numItems;
        }

        // Round up to next power of two (if not already)
        size_t nextpow2 = 1ULL << (64 - __builtin_clzll(numItems));
        size_t numItemsRounded = max(InitialBuckets, nextpow2);
        if (m_nBuckets == numItemsRounded)
        {
            // no need to resize here
            return;
        }

        // Handle resize and associated rehash if the table is full.
        size_t origCount = m_nBuckets;
        m_nBuckets = numItemsRounded;
        m_nMask = m_nBuckets - 1;
        if (m_nItems)
        {
            rehash(origCount);
        }
        else
        {
            // No items in the array, just recreate it here
            delete [] m_Buckets;
            m_Buckets = new bucket[m_nBuckets];
            setDefaults();
            resetParents();
        }
    }

    size_t count() const
    {
        return m_nItems;
    }

    Iterator begin()
    {
        return m_nItems ? Iterator(getFirstSetBucket()) : end();
    }

    ConstIterator begin() const
    {
        return m_nItems ? ConstIterator(getFirstSetBucket()) : end();
    }

    Iterator end()
    {
        return Iterator(0);
    }

    ConstIterator end() const
    {
        return ConstIterator(0);
    }

    /**
     * Erase the value at the given iterator position.
     */
    Iterator erase(Iterator &at)
    {
        struct bucket *node = at.__getNode();
        if (node && node->set)
        {
            node->set = false;
            --m_nItems;
            rehash();

            // This is the only safe way to continue iterating - the rehash in
            // remove makes everything else incorrect.
            return begin();
        }
        else
        {
            return at;
        }
    }

    /**
     * \note We allow move assignment but not copy assignment/construction for
     * HashTable. To copy a HashTable, the owner of the table needs to manage
     * this itself - allowing for correct handling of copying elements (e.g.
     * if V is a pointer type) without it happening "behind the scenes".
     */
    HashTable(const SelfType &other) = delete;
    SelfType &operator = (const SelfType &p) = delete;

    /**
     * Forceful opt-in to copy values from the other table into this one.
     * This is an explicit call which can be used for data types that are safe
     * to copy, or in cases where callers accept the risk or will handle the
     * lack of safety some other way.
     */
    void copyFrom(const SelfType &other)
    {
        clear();

        m_Default = other.m_Default;
        m_nBuckets = other.m_nBuckets;
        m_nItems = other.m_nItems;
        m_nMask = other.m_nMask;
        m_Buckets = new bucket[m_nBuckets];
        pedigree_std::copy(m_Buckets, other.m_Buckets, m_nBuckets);
        resetParents();
    }

    SelfType &operator=(SelfType &&p)
    {
        clear();

        m_Default = pedigree_std::move(p.m_Default);
        m_nBuckets = pedigree_std::move(p.m_nBuckets);
        m_nItems = pedigree_std::move(p.m_nItems);
        m_nMask = pedigree_std::move(p.m_nMask);
        m_Buckets = pedigree_std::move(p.m_Buckets);

        p.m_Buckets = nullptr;
        p.clear();

        return *this;
    }

  private:
    struct bucket *getFirstSetBucket() const
    {
        struct bucket *result = m_Buckets;
        while (result < (m_Buckets + m_nBuckets))
        {
            if (result->set)
            {
                return result;
            }

            ++result;
        }

        return nullptr;
    }

    void check()
    {
        if (m_Buckets == nullptr)
        {
            m_Buckets = new bucket[InitialBuckets];
            m_nBuckets = InitialBuckets;
            m_nMask = InitialBuckets - 1;
            setDefaults();
            resetParents();
        }
    }

    void rehash(size_t oldCount = 0)
    {
        if (oldCount == 0)
        {
            oldCount = m_nBuckets;
        }

        bucket *oldBuckets = m_Buckets;
        m_Buckets = new bucket[m_nBuckets];
        setDefaults();
        resetParents();

        if (m_nItems)
        {
            // Performing a new insert, clear out the number of items as
            // insert() will increment otherwise.
            m_nItems = 0;

            for (size_t i = 0; i < oldCount; ++i)
            {
                if (oldBuckets[i].set)
                {
                    insert(oldBuckets[i].key, oldBuckets[i].value);
                }
            }
        }
        delete [] oldBuckets;
    }

    size_t nextIndex(size_t i, size_t &index, size_t &step) const
    {
        if (QuadraticProbe)
        {
            index = (index + step) & m_nMask;
            ++step;
        }
        else
        {
            index = i;
        }

        return index;
    }

    bucket *findNextEmpty(size_t currentHash)
    {
        size_t index = 0;
        size_t step = 1;
        for (size_t i = 0; i < m_nBuckets; ++i)
        {
            size_t nextHash = (currentHash + nextIndex(i, index, step)) & m_nMask;
            bucket *b = &m_Buckets[nextHash];
            if (!b->set)
            {
                return b;
            }
        }

        return nullptr;
    }

    bucket *findNextSet(size_t currentHash, const K &k)
    {
        size_t index = 0;
        size_t step = 1;
        for (size_t i = 0; i < m_nBuckets; ++i)
        {
            size_t nextHash = (currentHash + nextIndex(i, index, step)) & m_nMask;
            bucket *b = &m_Buckets[nextHash];

            // Hash comparison is likely to be faster than raw object
            // comparison so we save the latter for when we have a candidate.
            if (b->set && (b->key.hash() == k.hash()))
            {
                if (b->key == k)
                {
                    return b;
                }
            }
        }

        return nullptr;
    }

    const bucket *findNextSet(size_t currentHash, const K &k) const
    {
        size_t index = 0;
        size_t step = 1;
        for (size_t i = 0; i < m_nBuckets; ++i)
        {
            size_t nextHash = (currentHash + nextIndex(i, index, step)) & m_nMask;
            const bucket *b = &m_Buckets[nextHash];

            // Hash comparison is likely to be faster than raw object
            // comparison so we save the latter for when we have a candidate.
            if (b->set && (b->key.hash() == k.hash()))
            {
                if (b->key == k)
                {
                    return b;
                }
            }
        }

        return nullptr;
    }

    const struct bucket *doLookup(const K &k, HashTableError::Error &err) const
    {
        if ((!m_Buckets) || (!m_nItems))
        {
            err = HashTableError::HashTableEmpty;
            return nullptr;
        }

        size_t hash = k.hash() & m_nMask;

        const bucket *b = &m_Buckets[hash];
        if (!b->set)
        {
            err = HashTableError::NotFound;
            return nullptr;
        }

        if (b->key != k)
        {
            b = findNextSet(hash, k);
            if (!b)
            {
                err = HashTableError::NotFound;
                return nullptr;
            }
        }

        return b;
    }

    /** Set default value on all un-set buckets. */
    void setDefaults()
    {
        for (size_t i = 0; i < m_nBuckets; ++i)
        {
            if (m_Buckets[i].set)
            {
                continue;
            }

            m_Buckets[i].value = m_Default;
        }
    }

    void resetParents()
    {
        for (size_t i = 0; i < m_nBuckets; ++i)
        {
            m_Buckets[i].parent = this;
        }
    }

    bucket *m_Buckets;
    V m_Default;
    size_t m_nBuckets;
    size_t m_nItems;
    size_t m_nMask;
};

/** @} */

#endif
