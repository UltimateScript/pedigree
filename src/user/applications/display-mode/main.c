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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage()
{
    printf("display-mode: List and select the Pedigree display modes.\n");
    printf("usage:\tdisplay-mode list\n");
    printf("\tdisplay-mode select <width>x<height>x<depth>\n");
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        usage();
        exit(1);
    }
/*
    char buf[256];
    int result;
    unsigned int width, height, depth;

    if(argc == 2 && !strcmp(argv[1], "list"))
    {
        result = pedigree_config_query("select * from 'display_modes'");
        if (result == -1)
            fprintf(stderr, "Unable to get the modes.\n");

        if (pedigree_config_was_successful(result) != 0)
        {
            pedigree_config_get_error_message(result, buf, 256);
            fprintf(stderr, "Error: %s\n", buf);
            return 1;
        }

        if (pedigree_config_numrows(result) == 0)
            printf("No modes\n");
        else
            while (pedigree_config_nextrow(result) == 0)
                printf("Display[%d]: Mode %dx%dx%d\n", pedigree_config_getnum_s(result, "display_id"), pedigree_config_getnum_s(result, "width"), pedigree_config_getnum_s(result, "height"), pedigree_config_getnum_s(result, "depth"));

        pedigree_config_freeresult(result);

    }
    else if(argc == 3 && !strcmp(argv[1], "select"))
    {
        if(sscanf(argv[2], "%dx%dx%d", &width, &height, &depth) == 3)
        {
            sprintf(buf, "select * from 'display_modes' where width = %d and height = %d and depth = %d and display_id = 0", width, height, depth);
            result = pedigree_config_query(buf);
            memset(buf, 0, 256);
            if (result == -1)
                fprintf(stderr, "Unable to get the mode.\n");

            if (pedigree_config_was_successful(result) != 0)
            {
                pedigree_config_get_error_message(result, buf, 256);
                fprintf(stderr, "Error: %s\n", buf);
                return 1;
            }

            if (pedigree_config_numrows(result) == 0 || pedigree_config_nextrow(result) != 0)
            {
                pedigree_config_freeresult(result);
                printf("No such mode\n");
            }
            else
            {
                memset(buf, 0, 256);
                sprintf(buf, "update displays set mode_id = %d where id = 0", pedigree_config_getnum_s(result, "mode_id"));
                pedigree_config_freeresult(result);
                printf("Setting mode %dx%dx%d...\n", width, height, depth);
                result = pedigree_config_query(buf);
                memset(buf, 0, 256);
                if (result == -1)
                    fprintf(stderr, "Unable to set the mode.\n");

                if (pedigree_config_was_successful(result) != 0)
                {
                    pedigree_config_get_error_message(result, buf, 256);
                    fprintf(stderr, "Error: %s\n", buf);
                    return 1;
                }
                pedigree_config_freeresult(result);
            }
        }
        else
        {
            usage();
            exit(1);
        }
    }
    else
    {
        usage();
        exit(1);
    }

    exit(0);*/
}
