/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/path.h>

#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

ib_status_t ib_util_mkpath(const char *path, mode_t mode)
{
    /* Mutable copy of path. */
    char *path_work = NULL;

    /* End of path_work; points to the terminating NULL. */
    char *path_end;

    /* path_work to exists_end is the part of path that exists. */
    char *exists_end;

    /* final result code. */
    ib_status_t rc = IB_OK;

    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        return IB_OK;
    }

    /* Could save one pass by using malloc and memcpy instead of strdup.
     * But this is simpler.
     */
    path_work = strdup(path);
    path_end = path_work + strlen(path_work);
    if (path_work == NULL) {
        return IB_EALLOC;
    }

    /* Find portion of path that already exists. */
    exists_end = path_work;
    if (*path_work == '/') {
        ++exists_end;
    }
    while (exists_end < path_end) {
        char *old_exists_end = exists_end;
        struct stat stat_result;
        int stat_rc;

        /* Find next / */
        for (;exists_end < path_end && *exists_end !='/'; ++exists_end);

        /* Check if path so far exists. */
        *exists_end = '\0';
        stat_rc = stat(path_work, &stat_result);
        if (exists_end != path_end) {
            *exists_end = '/';
        }
        if (stat_rc != 0) {
            exists_end = old_exists_end;
            break;
        }

        /* If done, exit. */
        if (exists_end == path_end) {
            break;
        }

        ++exists_end;
    }

    /* If entire path exists, we're done. */
    if (exists_end == path_end) {
        rc = IB_OK;
        goto finish;
    }

    /* Otherwise, keep going, creating as we go. */
    while (exists_end < path_end) {
        int mkdir_rc;

        /* Find next / */
        for (;exists_end < path_end && *exists_end !='/'; ++exists_end);
        *exists_end = '\0';

        mkdir_rc = mkdir(path_work, mode);
        if (mkdir_rc != 0) {
            ib_util_log_error("Failed to create path \"%s\": %s (%d)",
                              path_work, strerror(errno), errno);
        }
        if (exists_end != path_end) {
            *exists_end = '/';
        }
        if (mkdir_rc != 0) {
            rc = IB_EINVAL;
            goto finish;
        }

        ++exists_end;
    }

finish:
    if (path_work != NULL) {
        free(path_work);
    }

    return rc;
}

char *ib_util_path_join(ib_mpool_t *mp,
                        const char *parent,
                        const char *file_path)
{
    size_t len;
    size_t plen;  /* Length of parent */
    size_t flen;  /* Length of file_path */
    char *out;

    /* Strip off extraneous trailing slash chars */
    plen = strlen(parent);
    while( (plen >= 2) && (*(parent+(plen-1)) == '/') ) {
        --plen;
    }

    /* Ignore leading and trailing slash chars in file_path */
    flen = strlen(file_path);
    while ( (flen > 1) && (*file_path == '/') ) {
        ++file_path;
        --flen;
    }
    while ( (flen > 1) && (*(file_path+(flen-1)) == '/') ) {
        --flen;
    }

    /* Allocate & generate the include file name */
    len = plen;                /* Parent directory */
    if ( (plen > 1) || ((plen == 1) && (*parent == '.')) ) {
        len += 1;              /* slash */
    }
    len += flen;               /* file name */
    len += 1;                  /* NUL */

    out = (char *)ib_mpool_calloc(mp, len, 1);
    if (out == NULL) {
        return NULL;
    }

    strncpy(out, parent, plen);
    if ( (plen > 1) || ((plen == 1) && (*parent == '.')) ) {
        strcat(out, "/");
    }
    strncat(out, file_path, flen);

    return out;
}

char *ib_util_relative_file(ib_mpool_t *mp,
                            const char *ref_file,
                            const char *file_path)
{
    char *refcopy;       /* Copy of reference file */
    const char *ref_dir; /* Reference directory */
    char *tmp;

    /* If file_path is absolute, just use it */
    if (*file_path == '/') {
        tmp = ib_mpool_strdup(mp, file_path);
        return tmp;
    }

    /* Make a copy of cur_file because dirname() modifies it's input */
    refcopy = (char *)ib_mpool_strdup(mp, ref_file);
    if (refcopy == NULL) {
        return NULL;
    }

    /* Finally, extract the directory portion of the copy, use it to
     * build the final path. */
    ref_dir = dirname(refcopy);
    tmp = ib_util_path_join(mp, ref_dir, file_path);
    return tmp;
}

ib_status_t ib_util_normalize_path(char *data,
                                   bool win,
                                   ib_flags_t *result)
{
    ib_status_t rc;
    size_t len;
    rc = ib_util_normalize_path_ex((uint8_t *)data,
                                   strlen(data),
                                   win,
                                   &len,
                                   result);
    if (rc == IB_OK) {
        *(data+len) = '\0';
    }
    return rc;
}

ib_status_t ib_util_normalize_path_cow(
    ib_mpool_t *mp,
    const char *data_in,
    bool win,
    char **data_out,
    ib_flags_t *result)
{
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    size_t len;
    char *buf;

    /* Make a copy of the original */
    buf = ib_mpool_strdup(mp, data_in);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    /* Let normalize_path_ex() do the real work. */
    rc = ib_util_normalize_path_ex((uint8_t *)buf,
                                   strlen(data_in),
                                   win,
                                   &len,
                                   result);
    if (rc != IB_OK) {
        return rc;
    }

    /* If it's modified, point at the newly allocated buffer. */
    if (*result & IB_STRFLAG_MODIFIED) {
        *result = (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED );
        *(buf + len) = '\0';
        *data_out = buf;
    }
    else {
        *data_out = (char *)data_in;
    }

    return IB_OK;
}

ib_status_t ib_util_normalize_path_cow_ex(ib_mpool_t *mp,
                                          const uint8_t *data_in,
                                          size_t dlen_in,
                                          bool win,
                                          uint8_t **data_out,
                                          size_t *dlen_out,
                                          ib_flags_t *result)
{
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    uint8_t *buf;

    /* Make a copy of the original */
    buf = ib_mpool_alloc(mp, dlen_in);
    if (buf == NULL) {
        return IB_EALLOC;
    }
    memcpy(buf, data_in, dlen_in);

    /* Let normalize_path_ex() do the real work. */
    rc = ib_util_normalize_path_ex(buf, dlen_in, win, dlen_out, result);
    if (rc != IB_OK) {
        return rc;
    }

    /* If it's modified, point at the newly allocated buffer. */
    if (*result & IB_STRFLAG_MODIFIED) {
        *result = (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED);
        *data_out = buf;
    }
    else {
        *data_out = (uint8_t *)data_in;
    }

    return IB_OK;
}
