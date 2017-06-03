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

#include <iostream>
#include <string>

#include "pedigree/native/config/Config.h"

void usage()
{
    std::cout << "config: Query and update the Pedigree configuration manager."
              << std::endl
              << "usage: config <sql>" << std::endl;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage();
        return 1;
    }

    const char *sql = argv[1];

    Config::Result *pResult = Config::query(sql);

    // Check for query fail
    if (!pResult)
    {
        std::cerr << "Unable to query" << std::endl;
        return 0;
    }

    // Check for query error
    if (!pResult->succeeded())
    {
        std::cerr << "error: " << pResult->errorMessage() << std::endl;
        delete pResult;
        return 1;
    }

    // Is this an empty set?
    if (!pResult->rows())
    {
        std::cout << "Ø" << std::endl;
        delete pResult;
        return 0;
    }

    size_t cols = pResult->cols();
    size_t *col_lens = new size_t[cols];

    // Print the column names
    for (size_t col = 0; col < cols; col++)
    {
        std::string colName = pResult->getColumnName(col);
        std::cout << " " << colName;
        col_lens[col] = colName.length();
        while (col_lens[col] < 15)
        {
            std::cout << " ";
            col_lens[col]++;
        }
        std::cout << " |";
    }
    std::cout << std::endl;

    // Print delimiter row
    for (size_t col = 0; col < cols; col++)
    {
        std::cout << "-";
        for (size_t i = 0; i < col_lens[col]; i++)
            std::cout << "-";
        std::cout << "-+";
    }
    std::cout << std::endl;

    // Print the results
    for (size_t row = 0; row < pResult->rows(); row++)
    {
        for (size_t col = 0; col < cols; col++)
        {
            std::string value = pResult->getStr(row, col);
            std::cout << " " << value << "\t";
            for (size_t i = value.length(); i < col_lens[col]; i++)
                std::cout << " ";
            std::cout << " |";
        }
        std::cout << std::endl;
    }

    // Cleanup
    delete[] col_lens;
    delete pResult;

    return 0;
}
