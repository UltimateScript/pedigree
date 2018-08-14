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

#define PEDIGREE_EXTERNAL_SOURCE 1

#include <string.h>

#include <benchmark/benchmark.h>

#include "pedigree/kernel/utilities/StaticString.h"

static void BM_CxxStaticStringCreation(benchmark::State &state)
{
    while (state.KeepRunning())
    {
        HugeStaticString s;
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringCopy(benchmark::State &state)
{
    const char *assign = "Hello, world!";

    while (state.KeepRunning())
    {
        HugeStaticString s(assign);
        benchmark::DoNotOptimize(s);
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringContains(benchmark::State &state)
{
    HugeStaticString s("Hello, world!");

    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(s.contains("world"));
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringAppendString(benchmark::State &state)
{
    while (state.KeepRunning())
    {
        state.PauseTiming();
        HugeStaticString s("append right here -->", 21);
        state.ResumeTiming();

        s.append("hello");
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringAppendStaticString(benchmark::State &state)
{
    NormalStaticString append("hello");
    while (state.KeepRunning())
    {
        state.PauseTiming();
        HugeStaticString s("append right here -->", 21);
        state.ResumeTiming();

        s.append(append);
        benchmark::DoNotOptimize(s);
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringAppendInteger(benchmark::State &state)
{
    while (state.KeepRunning())
    {
        state.PauseTiming();
        HugeStaticString s("append right here -->");
        state.ResumeTiming();

        s.append(0xdeadbeef);
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringAppendHexInteger(benchmark::State &state)
{
    while (state.KeepRunning())
    {
        state.PauseTiming();
        HugeStaticString s("append right here -->");
        state.ResumeTiming();

        s.append(0xdeadbeef, 16);
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringAppendPaddedInteger(benchmark::State &state)
{
    while (state.KeepRunning())
    {
        state.PauseTiming();
        HugeStaticString s("append right here -->");
        state.ResumeTiming();

        s.append(0xdeadbeef, 10, 32, '@');
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringContainsCStr(benchmark::State &state)
{
    HugeStaticString s("llama llama llama llama llama llama alpaca! llama "
                       "llama llama llama llama llama");
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(s.contains("alpaca!"));
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringContainsCStrWorstCase(benchmark::State &state)
{
    HugeStaticString s("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab");
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(s.contains("aaaaaab"));
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void BM_CxxStaticStringContainsStaticString(benchmark::State &state)
{
    HugeStaticString s("llama llama llama llama llama llama alpaca! llama "
                       "llama llama llama llama llama");
    HugeStaticString other("alpaca!");
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(s.contains(other));
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

static void
BM_CxxStaticStringContainsStaticStringWorstCase(benchmark::State &state)
{
    HugeStaticString s("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab");
    HugeStaticString other("aaaaaab");
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(s.contains(other));
    }

    state.SetItemsProcessed(int64_t(state.iterations()));
}

BENCHMARK(BM_CxxStaticStringCreation);
BENCHMARK(BM_CxxStaticStringCopy);
BENCHMARK(BM_CxxStaticStringAppendString);
BENCHMARK(BM_CxxStaticStringAppendStaticString);
BENCHMARK(BM_CxxStaticStringAppendInteger);
BENCHMARK(BM_CxxStaticStringAppendHexInteger);
BENCHMARK(BM_CxxStaticStringAppendPaddedInteger);
BENCHMARK(BM_CxxStaticStringContainsCStr);
BENCHMARK(BM_CxxStaticStringContainsStaticString);
BENCHMARK(BM_CxxStaticStringContainsCStrWorstCase);
BENCHMARK(BM_CxxStaticStringContainsStaticStringWorstCase);
