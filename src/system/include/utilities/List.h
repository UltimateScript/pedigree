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

#ifndef KERNEL_UTILITIES_LIST_H
#define KERNEL_UTILITIES_LIST_H

#include <processor/types.h>
#include <utilities/Iterator.h>
#include <utilities/IteratorAdapter.h>
#include <utilities/ObjectPool.h>
#include <utilities/assert.h>
#include <utilities/template.h>

/** @addtogroup kernelutilities
 * @{ */

/** One node in the list
 *\brief One node in the list
 *\param[in] T the element type */
template <typename T>
struct _ListNode_t
{
    static_assert(
        sizeof(T) <= 16, "List<T> should not be used with large objects");

    /** Get the next data structure in the list
     *\return pointer to the next data structure in the list */
    _ListNode_t *next()
    {
        return m_Next;
    }
    /** Get the previous data structure in the list
     *\return pointer to the previous data structure in the list */
    _ListNode_t *previous()
    {
        return m_Previous;
    }

    /** Pointer to the next node */
    _ListNode_t *m_Next;
    /** Pointer to the previous node */
    _ListNode_t *m_Previous;
    /** The value of the node */
    T value;
};

template <class T, size_t nodePoolSize = 16>
class List
{
    /** The data structure of the list's nodes */
    typedef _ListNode_t<T> node_t;

    public:
    /** Type of the bidirectional iterator */
    typedef ::Iterator<T, node_t> Iterator;
    /** Type of the constant bidirectional iterator */
    typedef typename Iterator::Const ConstIterator;
    /** Type of the reverse iterator */
    typedef typename Iterator::Reverse ReverseIterator;
    /** Type of the constant reverse iterator */
    typedef typename Iterator::ConstReverse ConstReverseIterator;

    /** Default constructor, does nothing */
    List();
    /** Copy-constructor
     *\param[in] x reference object */
    List(const List &x);
    /** Destructor, deallocates memory */
    ~List();

    /** Assignment operator
     *\param[in] x the object that should be copied */
    List &operator=(const List &x);

    /** Get the number of elements we reserved space for
     *\return number of elements we reserved space for */
    size_t size() const;
    /** Get the number of elements in the List */
    size_t count() const;
    /** Add a value to the end of the List
     *\param[in] value the value that should be added */
    void pushBack(T value);
    /** Remove the last element from the List
     *\return the previously last element */
    T popBack();
    /** Add a value to the front of the List
     *\param[in] value the value that should be added */
    void pushFront(T value);
    /** Remove the first element in the List
     *\return the previously first element */
    T popFront();
    /** Erase an element
     *\param[in] iterator the iterator that points to the element */
    Iterator erase(Iterator &Iter);
    /** Erase an element with a reverse iterator
     *\param[in] reverse iterator the iterator that points to the element */
    ReverseIterator erase(ReverseIterator &Iter);

    /** Get an iterator pointing to the beginning of the List
     *\return iterator pointing to the beginning of the List */
    inline Iterator begin()
    {
        return Iterator(m_First);
    }
    /** Get a constant iterator pointing to the beginning of the List
     *\return constant iterator pointing to the beginning of the List */
    inline ConstIterator begin() const
    {
        return ConstIterator(m_First);
    }
    /** Get an iterator pointing to the end of the List + 1
     *\return iterator pointing to the end of the List + 1 */
    inline Iterator end()
    {
        return Iterator(0);
    }
    /** Get a constant iterator pointing to the end of the List + 1
     *\return constant iterator pointing to the end of the List + 1 */
    inline ConstIterator end() const
    {
        return ConstIterator(0);
    }
    /** Get an iterator pointing to the reverse beginning of the List
     *\return iterator pointing to the reverse beginning of the List */
    inline ReverseIterator rbegin()
    {
        return ReverseIterator(m_Last);
    }
    /** Get a constant iterator pointing to the reverse beginning of the List
     *\return constant iterator pointing to the reverse beginning of the List */
    inline ConstReverseIterator rbegin() const
    {
        return ConstReverseIterator(m_Last);
    }
    /** Get an iterator pointing to the reverse end of the List + 1
     *\return iterator pointing to the reverse end of the List + 1 */
    inline ReverseIterator rend()
    {
        return ReverseIterator(0);
    }
    /** Get a constant iterator pointing to the reverse end of the List + 1
     *\return constant iterator pointing to the reverse end of the List + 1 */
    inline ConstReverseIterator rend() const
    {
        return ConstReverseIterator(0);
    }

    /** Remove all elements from the List */
    void clear();
    /** Copy the content of a List into this List
     *\param[in] x the reference List */
    void assign(const List &x);

    private:
    /** The number of Nodes/Elements in the List */
    size_t m_Count;
    /** Pointer to the first Node in the List */
    node_t *m_First;
    /** Pointer to the last Node in the List */
    node_t *m_Last;

    uint32_t m_Magic;

    /** Pool of node objects (to reduce impact of lots of node allocs/deallocs).
     */
    ObjectPool<node_t, nodePoolSize> m_NodePool;
};

//
// List<T> implementation
//

template <typename T, size_t nodePoolSize>
List<T, nodePoolSize>::List()
    : m_Count(0), m_First(0), m_Last(0), m_Magic(0x1BADB002), m_NodePool()
{
}

template <typename T, size_t nodePoolSize>
List<T, nodePoolSize>::List(const List &x)
    : m_Count(0), m_First(0), m_Last(0), m_Magic(0x1BADB002), m_NodePool()
{
    assign(x);
}
template <typename T, size_t nodePoolSize>
List<T, nodePoolSize>::~List()
{
    assert(m_Magic == 0x1BADB002);
    clear();
}

template <typename T, size_t nodePoolSize>
List<T, nodePoolSize> &List<T, nodePoolSize>::operator=(const List &x)
{
    assign(x);
    return *this;
}

template <typename T, size_t nodePoolSize>
size_t List<T, nodePoolSize>::size() const
{
    return m_Count;
}
template <typename T, size_t nodePoolSize>
size_t List<T, nodePoolSize>::count() const
{
    return m_Count;
}
template <typename T, size_t nodePoolSize>
void List<T, nodePoolSize>::pushBack(T value)
{
    node_t *newNode = m_NodePool.allocate();
    newNode->m_Next = 0;
    newNode->m_Previous = m_Last;
    newNode->value = value;

    if (m_Last == 0)
        m_First = newNode;
    else
        m_Last->m_Next = newNode;

    m_Last = newNode;
    ++m_Count;
}
template <typename T, size_t nodePoolSize>
T List<T, nodePoolSize>::popBack()
{
    // Handle an extremely unusual case
    if (!m_Last && !m_First)
        return T();

    node_t *node = m_Last;
    if (m_Last)
        m_Last = m_Last->m_Previous;
    if (m_Last != 0)
        m_Last->m_Next = 0;
    else
        m_First = 0;
    --m_Count;

    if (!node)
        return T();

    T value = node->value;
    m_NodePool.deallocate(node);
    return value;
}
template <typename T, size_t nodePoolSize>
void List<T, nodePoolSize>::pushFront(T value)
{
    node_t *newNode = m_NodePool.allocate();
    newNode->m_Next = m_First;
    newNode->m_Previous = 0;
    newNode->value = value;

    if (m_First == 0)
        m_Last = newNode;
    else
        m_First->m_Previous = newNode;

    m_First = newNode;
    ++m_Count;
}
template <typename T, size_t nodePoolSize>
T List<T, nodePoolSize>::popFront()
{
    // Handle an extremely unusual case
    if (!m_Last && !m_First)
        return T();

    node_t *node = m_First;
    if (m_First)
        m_First = m_First->m_Next;
    if (m_First != 0)
        m_First->m_Previous = 0;
    else
        m_Last = 0;
    --m_Count;

    if (!node)
        return T();

    T value = node->value;
    m_NodePool.deallocate(node);
    return value;
}
template <typename T, size_t nodePoolSize>
typename List<T, nodePoolSize>::Iterator
List<T, nodePoolSize>::erase(Iterator &Iter)
{
    node_t *Node = Iter.__getNode();
    if (Node->m_Previous == 0)
        m_First = Node->m_Next;
    else
        Node->m_Previous->m_Next = Node->m_Next;
    if (Node->m_Next == 0)
        m_Last = Node->m_Previous;
    else
        Node->m_Next->m_Previous = Node->m_Previous;
    --m_Count;

    node_t *pNext = Node->m_Next;
    // If pNext is NULL, this will be the same as 'end()'.
    Iterator tmp(pNext);
    delete Node;
    return tmp;
}

template <typename T, size_t nodePoolSize>
typename List<T, nodePoolSize>::ReverseIterator
List<T, nodePoolSize>::erase(ReverseIterator &Iter)
{
    node_t *Node = Iter.__getNode();
    if (Node->m_Next == 0)
        m_First = Node->m_Previous;
    else
        Node->m_Next->m_Previous = Node->m_Previous;
    if (Node->m_Previous == 0)
        m_Last = Node->m_Next;
    else
        Node->m_Previous->m_Next = Node->m_Next;
    --m_Count;

    node_t *pNext = Node->m_Previous;
    // If pNext is NULL, this will be the same as 'rend()'.
    ReverseIterator tmp(pNext);
    delete Node;
    return tmp;
}

template <typename T, size_t nodePoolSize>
void List<T, nodePoolSize>::clear()
{
    node_t *cur = m_First;
    for (size_t i = 0; i < m_Count; i++)
    {
        node_t *tmp = cur;
        cur = cur->m_Next;
        delete tmp;
    }

    m_Count = 0;
    m_First = 0;
    m_Last = 0;
}
template <typename T, size_t nodePoolSize>
void List<T, nodePoolSize>::assign(const List &x)
{
    if (m_Count != 0)
        clear();

    ConstIterator Cur(x.begin());
    ConstIterator End(x.end());
    for (; Cur != End; ++Cur)
        pushBack(*Cur);
}

/** @} */

#endif
