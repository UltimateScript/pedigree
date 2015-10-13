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

#ifndef KERNEL_LOCKGUARD_H
#define KERNEL_LOCKGUARD_H

/** @addtogroup kernel
 * @{ */

#include <Lock.h>

template<class T>
class SCOPED_CAPABILITY LockGuard
{
  public:
    inline LockGuard(T &Lock, bool Condition = true) ACQUIRE(m_Lock)
      : m_Lock(Lock), m_bCondition(Condition)
    {
      if (m_bCondition)
        m_Lock.acquire();
    }
    inline virtual ~LockGuard() RELEASE()
    {
      if (m_bCondition)
        m_Lock.release();
    }

  private:
    LockGuard();
    LockGuard(const LockGuard &);
    LockGuard &operator = (const LockGuard &);

    T &m_Lock;
    bool m_bCondition;
};

/** @} */

#endif
