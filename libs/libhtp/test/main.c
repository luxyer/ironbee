/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "../htp/bstr.h"
#include "../htp/htp.h"
#include "test.h"

char *home = NULL;

int callback_transaction_start(htp_connp_t *connp) {
    printf("-- Callback: transaction_start\n");
    return 0;
}

int callback_request_line(htp_connp_t *connp) {
    printf("-- Callback: request_line\n");
    return 0;
}

int callback_request_headers(htp_connp_t *connp) {
    printf("-- Callback: request_headers\n");
    bstr *raw = htp_tx_get_request_headers_raw(connp->in_tx);
    fprint_raw_data(stdout, "REQUEST HEADERS RAW 1", (unsigned char*)bstr_ptr(raw), bstr_len(raw));
    return 0;
}

int callback_request_body_data(htp_tx_data_t *d) {
    /*
    if (d->data != NULL) {
        printf("-- Callback: request_body_data\n");
        fprint_raw_data(stdout, __FUNCTION__, d->data, d->len);
    } else {
        printf("-- Callback: request_body_data (LAST)\n");
    }
    */
    return 0;
}

int callback_request_trailer(htp_connp_t *connp) {
    printf("-- Callback: request_trailer\n");
    return 0;
}

int callback_request(htp_connp_t *connp) {
    printf("-- Callback: request\n");
    return 0;
}

int callback_response_line(htp_connp_t *connp) {
    printf("-- Callback: response_line\n");
    return 0;
}

int callback_response_headers(htp_connp_t *connp) {
    printf("-- Callback: response_headers\n");
    return 0;
}

int callback_response_body_data(htp_tx_data_t *d) {
    if (d->data != NULL) {
        printf("-- Callback: response_body_data\n");
        fprint_raw_data(stdout, __FUNCTION__, d->data, d->len);
    } else {
        printf("-- Callback: response_body_data (LAST)\n");
    }
    return 0;
}

int callback_request_file_data(htp_file_data_t *file_data) {
    if (file_data->data != NULL) {
        printf("-- Callback: request_file_data\n");
        fprint_raw_data(stdout, __FUNCTION__, file_data->data, file_data->len);
    } else {
        printf("-- Callback: request_file_data (LAST)\n");
    }
    return 0;
}

int callback_response_trailer(htp_connp_t *connp) {
    printf("-- Callback: response_trailer\n");
    return 0;
}

int callback_response(htp_connp_t *connp) {
    printf("-- Callback: response\n");
    return 0;
}

int callback_response_destroy(htp_connp_t *connp) {
    htp_tx_destroy(connp->out_tx);
    printf("-- Destroyed transaction\n");
    return 0;
}

int callback_log(htp_log_t *log) {
    htp_print_log(stdout, log);
    return 0;
}

static void print_tx(htp_connp_t *connp, htp_tx_t *tx) {
    char *request_line = bstr_util_strdup_to_c(tx->request_line);
    htp_header_t *h_user_agent = table_get_c(tx->request_headers, "user-agent");
    htp_header_t *h_referer = table_get_c(tx->request_headers, "referer");
    char *referer, *user_agent;
    char buf[256];

    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    strftime(buf, 255, "%d/%b/%Y:%T %z", tmp);

    if (h_user_agent == NULL) user_agent = strdup("-");
    else {
        user_agent = bstr_util_strdup_to_c(h_user_agent->value);
    }

    if (h_referer == NULL) referer = strdup("-");
    else {
        referer = bstr_util_strdup_to_c(h_referer->value);
    }

    printf("%s - - [%s] \"%s\" %i %zu \"%s\" \"%s\"\n", connp->conn->remote_addr, buf,
        request_line, tx->response_status_number, tx->response_message_len,
        referer, user_agent);

    free(referer);
    free(user_agent);
    free(request_line);
}

static int run_directory(char *dirname, htp_cfg_t *cfg) {
    struct dirent *entry;
    DIR *d = opendir(dirname);
    htp_connp_t *connp;

    if (d == NULL) {
        printf("Failed to open directory: %s\n", dirname);
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "stream", 6) == 0) {
            int rc = test_run(dirname, entry->d_name, cfg, &connp);

            if (rc < 0) {
                if (connp != NULL) {
                    htp_log_t *last_error = htp_connp_get_last_error(connp);
                    if (last_error != NULL) {
                        printf(" -- failed: %s\n", last_error->msg);
                    } else {
                        printf(" -- failed: ERROR NOT AVAILABLE\n");
                    }

                    return 0;
                } else {
                    return -1;
                }
            } else {
                printf(" -- %zu transaction(s)\n", list_size(connp->conn->transactions));

                htp_tx_t *tx = NULL;
                list_iterator_reset(connp->conn->transactions);
                while ((tx = list_iterator_next(connp->conn->transactions)) != NULL) {
                    printf("    ");
                    print_tx(connp, tx);
                }

                printf("\n");

                htp_connp_destroy_all(connp);
            }
        }
    }

    closedir(d);

    return 1;
}

int main_dir(int argc, char** argv) {
    //int main(int argc, char** argv) {
    htp_cfg_t *cfg = htp_config_create();
    htp_config_register_log(cfg, callback_log);
    htp_config_register_response(cfg, callback_response_destroy);

    run_directory("C:\\http_traces\\run1", cfg);
    //run_directory("/home/ivanr/work/traces/run3/", cfg);

    htp_config_destroy(cfg);

    return 0;
}

#define RUN_TEST(X, Y) \
    {\
    tests++; \
    printf("---------------------------------\n"); \
    printf("Test: " #X "\n"); \
    int rc = X(Y); \
    if (rc < 0) { \
        printf("    Failed with %i\n", rc); \
        failures++; \
    } \
    printf("\n"); \
    }

/**
 * Dummy entry point.
 */
int main(int argc, char** argv) {
    return EXIT_SUCCESS;
}

int main_path_decoding_tests(int argc, char** argv) {
    htp_cfg_t *cfg = htp_config_create();
    htp_tx_t *tx = htp_tx_create(cfg, 0, NULL);
    char *str;
    bstr *path = NULL;

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven");
    cfg->path_case_insensitive = 1;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;
    cfg->path_backslash_separators = 1;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;
    cfg->path_backslash_separators = 1;
    cfg->path_decode_separators = 1;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;
    cfg->path_backslash_separators = 1;
    cfg->path_decode_separators = 1;
    cfg->path_invalid_encoding_handling = URL_DECODER_REMOVE_PERCENT;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven/%u0074");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;
    cfg->path_backslash_separators = 1;
    cfg->path_decode_separators = 1;
    cfg->path_invalid_encoding_handling = URL_DECODER_DECODE_INVALID;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    //
    path = bstr_dup_c("/One\\two///ThRee%2ffive%5csix/se%xxven/%u0074%u0100");
    cfg->path_case_insensitive = 1;
    cfg->path_compress_separators = 1;
    cfg->path_backslash_separators = 1;
    cfg->path_decode_separators = 1;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    cfg->path_decode_u_encoding = 1;

    str = bstr_util_strdup_to_c(path);
    printf("Before: %s\n", str);
    free(str);
    htp_decode_path_inplace(cfg, tx, path);
    str = bstr_util_strdup_to_c(path);
    printf("After: %s\n\n", str);
    free(str);
    bstr_free(&path);

    return 0;
}

void encode_utf8_2(uint8_t *data, uint32_t i) {
    i = i & 0x7ff;
    data[0] = 0xc0 + (i >> 6);
    data[1] = 0x80 + (i & 0x3f);
}

void encode_utf8_3(uint8_t *data, uint32_t i) {
    i = i & 0xffff;
    data[0] = 0xe0 + (i >> 12);
    data[1] = 0x80 + ((i >> 6) & 0x3f);
    data[2] = 0x80 + (i & 0x3f);
}

void encode_utf8_4(uint8_t *data, uint32_t i) {
    i = i & 0x10ffff;
    data[0] = 0xf0 + (i >> 18);
    data[1] = 0x80 + ((i >> 12) & 0x3f);
    data[2] = 0x80 + ((i >> 6) & 0x3f);
    data[3] = 0x80 + (i & 0x3f);
}

int main_utf8_decoder_tests(int argc, char** argv) {
    htp_cfg_t *cfg = htp_config_create();
    htp_tx_t *tx = htp_tx_create(cfg, 0, NULL);

    bstr *path = NULL;

    path = bstr_dup_c("//////////");
    uint8_t *data = (uint8_t *)bstr_ptr(path);

    int i = 0;

    for (i = 0; i < 0x80; i++) {
        memset(data, 0x2f, 10);
        tx->flags = 0;
        encode_utf8_2(data, i);
        htp_utf8_validate_path(tx, path);
        if (tx->flags != HTP_PATH_UTF8_OVERLONG) {
            printf("#2 i %i data %x %x flags %x\n", i, (uint8_t) data[0], (uint8_t) data[1], tx->flags);
        }
    }

    for (i = 0; i < 0x800; i++) {
        memset(data, 0x2f, 10);
        tx->flags = 0;
        encode_utf8_3(data, i);
        htp_utf8_validate_path(tx, path);
        if (tx->flags != HTP_PATH_UTF8_OVERLONG) {
            printf("#3 i %x data %x %x %x flags %x\n", i, (uint8_t) data[0], (uint8_t) data[1], (uint8_t) data[2], tx->flags);
        }
    }

    for (i = 0; i < 0x10000; i++) {
        memset(data, 0x2f, 10);
        tx->flags = 0;
        encode_utf8_4(data, i);
        htp_utf8_validate_path(tx, path);
        if ((i >= 0xff00) && (i <= 0xffff)) {
            if (tx->flags != (HTP_PATH_UTF8_OVERLONG | HTP_PATH_FULLWIDTH_EVASION)) {
                printf("#4 i %x data %x %x %x %x flags %x\n", i, (uint8_t) data[0], (uint8_t) data[1], (uint8_t) data[2], (uint8_t) data[3], tx->flags);
            }
        } else {
            if (tx->flags != HTP_PATH_UTF8_OVERLONG) {
                printf("#4 i %x data %x %x %x %x flags %x\n", i, (uint8_t) data[0], (uint8_t) data[1], (uint8_t) data[2], (uint8_t) data[3], tx->flags);
            }
        }
    }

    bstr_free(&path);

    return 0;
}

#define PATH_DECODE_TEST_BEFORE(NAME) \
    test_name = NAME; \
    tests++; \
    expected_status = 0; \
    expected_flags = -1; \
    success = 0; \
    cfg = htp_config_create(); \
    tx = htp_tx_create(cfg, 0, NULL);

#define PATH_DECODE_TEST_AFTER() \
    htp_decode_path_inplace(cfg, tx, input); \
    htp_utf8_decode_path_inplace(cfg, tx, input); \
    if (bstr_cmp(input, expected) == 0) success = 1; \
    printf("[%2i] %s: %s\n", tests, (success == 1 ? "SUCCESS" : "FAILURE"), test_name); \
    if ((success == 0)||((expected_status != 0)&&(expected_status != tx->response_status_expected_number))) { \
        char *s1 = bstr_util_strdup_to_c(input); \
        char *s2 = bstr_util_strdup_to_c(expected); \
        printf("      Output: [%s]\n", s1); \
        printf("    Expected: [%s]\n", s2); \
        if (expected_status != 0) { \
            printf("    Expected status %i; got %i\n", expected_status, tx->response_status_expected_number); \
        } \
        if (expected_flags != -1) { \
            printf("    Expected flags 0x%x; got 0x%x\n", expected_flags, tx->flags); \
        } \
        free(s2); \
        free(s1); \
        failures++; \
    } \
    htp_tx_destroy(tx); \
    htp_config_destroy(cfg); \
    bstr_free(&expected); \
    bstr_free(&input);

int main_path_tests(int argc, char** argv) {
    htp_cfg_t *cfg = NULL;
    htp_tx_t *tx = NULL;
    bstr *input = NULL;
    bstr *expected = NULL;
    int success = 0;
    int tests = 0;
    int failures = 0;
    int expected_status = 0;
    int expected_flags = 0;
    char *test_name = NULL;

    PATH_DECODE_TEST_BEFORE("URL-decoding");
    input = bstr_dup_c("/%64est");
    expected = bstr_dup_c("/dest");
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid URL-encoded, preserve %");
    input = bstr_dup_c("/%xxest");
    expected = bstr_dup_c("/%xxest");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid URL-encoded, remove %");
    input = bstr_dup_c("/%xxest");
    expected = bstr_dup_c("/xxest");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_invalid_encoding_handling = URL_DECODER_REMOVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid URL-encoded (end of string, test 1), preserve %");
    input = bstr_dup_c("/test/%2");
    expected = bstr_dup_c("/test/%2");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid URL-encoded (end of string, test 2), preserve %");
    input = bstr_dup_c("/test/%");
    expected = bstr_dup_c("/test/%");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid URL-encoded, preserve % and 400");
    input = bstr_dup_c("/%xxest");
    expected = bstr_dup_c("/%xxest");
    expected_status = 400;
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_invalid_encoding_handling = URL_DECODER_STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("%u decoding (expected not to decode; 400)");
    input = bstr_dup_c("/%u0064");
    expected = bstr_dup_c("/%u0064");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    expected_status = 400;
    cfg->path_invalid_encoding_handling = URL_DECODER_STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("%u decoding (decode; 400)");
    input = bstr_dup_c("/%u0064");
    expected = bstr_dup_c("/d");
    expected_status = 400;
    expected_flags = HTP_PATH_OVERLONG_U;
    cfg->path_decode_u_encoding = STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("%u decoding (also overlong)");
    input = bstr_dup_c("/%u0064");
    expected = bstr_dup_c("/d");
    expected_flags = HTP_PATH_OVERLONG_U;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding, leave; preserve percent");
    input = bstr_dup_c("/%uXXXX---");
    expected = bstr_dup_c("/%uXXXX---");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding, decode invalid; preserve percent");
    input = bstr_dup_c("/%uXXXX---");
    expected = bstr_dup_c("/?---");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_DECODE_INVALID;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding, decode invalid; preserve percent; 400");
    input = bstr_dup_c("/%uXXXX---");
    expected = bstr_dup_c("/?---");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    expected_status = 400;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding (not enough data 1), preserve percent");
    input = bstr_dup_c("/%u123");
    expected = bstr_dup_c("/%u123");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding (not enough data 2), preserve percent");
    input = bstr_dup_c("/%u12");
    expected = bstr_dup_c("/%u12");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid %u decoding (not enough data 3), preserve percent");
    input = bstr_dup_c("/%u1");
    expected = bstr_dup_c("/%u1");
    expected_flags = HTP_PATH_INVALID_ENCODING;
    cfg->path_decode_u_encoding = YES;
    cfg->path_invalid_encoding_handling = URL_DECODER_PRESERVE_PERCENT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("%u decoding, best-fit mapping");
    input = bstr_dup_c("/%u0107");
    expected = bstr_dup_c("/c");
    cfg->path_decode_u_encoding = YES;
    cfg->path_unicode_mapping = BESTFIT;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("%u decoding, 404 to UCS-2 characters");
    input = bstr_dup_c("/%u0107");
    expected = bstr_dup_c("/c");
    expected_status = 404;
    cfg->path_decode_u_encoding = YES;
    cfg->path_unicode_mapping = STATUS_404;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Forward slash (URL-encoded), not expect to decode");
    input = bstr_dup_c("/one%2ftwo");
    expected = bstr_dup_c("/one%2ftwo");
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Forward slash (URL-encoded), expect to decode");
    input = bstr_dup_c("/one%2ftwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Forward slash (URL-encoded), expect not do decode and 404");
    input = bstr_dup_c("/one%2ftwo");
    expected = bstr_dup_c("/one%2ftwo");
    expected_status = 404;
    cfg->path_decode_separators = STATUS_404;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Forward slash (%u-encoded), expect to decode");
    input = bstr_dup_c("/one%u002ftwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Forward slash (%u-encoded, fullwidth), expect to decode");
    input = bstr_dup_c("/one%uff0ftwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Backslash (URL-encoded), not a separator; expect to decode");
    input = bstr_dup_c("/one%5ctwo");
    expected = bstr_dup_c("/one\\two");
    cfg->path_decode_separators = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Backslash (URL-encoded), as path segment separator");
    input = bstr_dup_c("/one%5ctwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    cfg->path_backslash_separators = 1;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Backslash (not encoded), as path segment separator");
    input = bstr_dup_c("/one\\two");
    expected = bstr_dup_c("/one/two");
    cfg->path_backslash_separators = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Backslash (%u-encoded), as path segment separator");
    input = bstr_dup_c("/one%u005ctwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    cfg->path_backslash_separators = YES;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Backslash (%u-encoded, fullwidth), as path segment separator");
    input = bstr_dup_c("/one%uff3ctwo");
    expected = bstr_dup_c("/one/two");
    cfg->path_decode_separators = YES;
    cfg->path_backslash_separators = 1;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid UTF-8 encoding, encoded");
    input = bstr_dup_c("/%f7test");
    expected = bstr_dup_c("/\xf7test");
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Invalid UTF-8 encoding, encoded (400)");
    input = bstr_dup_c("/%f7test");
    expected = bstr_dup_c("/\xf7test");
    expected_status = 400;
    expected_flags = HTP_PATH_UTF8_INVALID;
    cfg->path_invalid_utf8_handling = STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (raw) in path; leave");
    input = bstr_dup_mem("/test\0text", 10);
    expected = bstr_dup_mem("/test\0text", 10);
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (raw) in path; terminate path");
    input = bstr_dup_mem("/test\0text", 10);
    expected = bstr_dup_c("/test");
    cfg->path_nul_raw_handling = TERMINATE;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (raw) in path; 400");
    input = bstr_dup_mem("/test\0text", 10);
    expected = bstr_dup_mem("/test\0text", 10);
    cfg->path_nul_raw_handling = STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (URL-encoded) in path; leave");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_mem("/test\0text", 10);
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (URL-encoded) in path; terminate path");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_c("/test");
    cfg->path_nul_encoded_handling = TERMINATE;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (URL-encoded) in path; 400");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_mem("/test\0text", 10);
    cfg->path_nul_encoded_handling = STATUS_400;
    expected_status = 400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (URL-encoded) in path; 404");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_mem("/test\0text", 10);
    cfg->path_nul_encoded_handling = STATUS_404;
    expected_status = 404;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (%u-encoded) in path; terminate path");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_c("/test");
    cfg->path_nul_encoded_handling = TERMINATE;
    cfg->path_decode_u_encoding = YES;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (%u-encoded) in path; 400");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_mem("/test\0text", 10);
    cfg->path_nul_encoded_handling = STATUS_400;
    cfg->path_decode_u_encoding = YES;
    expected_status = 400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("NUL byte (%u-encoded) in path; 404");
    input = bstr_dup_c("/test%00text");
    expected = bstr_dup_mem("/test\0text", 10);
    cfg->path_nul_encoded_handling = STATUS_404;
    cfg->path_decode_u_encoding = YES;
    expected_status = 404;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Control char in path, encoded (no effect)");
    input = bstr_dup_c("/%01test");
    expected = bstr_dup_c("/\x01test");
    cfg->path_control_char_handling = NONE;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Control char in path, raw (no effect)");
    input = bstr_dup_c("/\x01test");
    expected = bstr_dup_c("/\x01test");
    cfg->path_control_char_handling = NONE;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Control char in path, encoded (400)");
    input = bstr_dup_c("/%01test");
    expected = bstr_dup_c("/\x01test");
    expected_status = 400;
    cfg->path_control_char_handling = STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("Control char in path, raw (400)");
    input = bstr_dup_c("/\x01test");
    expected = bstr_dup_c("/\x01test");
    expected_status = 400;
    cfg->path_control_char_handling = STATUS_400;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("UTF-8; overlong 2-byte sequence");
    input = bstr_dup_c("/%c1%b4est");
    expected = bstr_dup_c("/test");
    expected_flags = HTP_PATH_UTF8_OVERLONG;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("UTF-8; overlong 3-byte sequence");
    input = bstr_dup_c("/%e0%81%b4est");
    expected = bstr_dup_c("/test");
    expected_flags = HTP_PATH_UTF8_OVERLONG;
    PATH_DECODE_TEST_AFTER();

    PATH_DECODE_TEST_BEFORE("UTF-8; overlong 4-byte sequence");
    input = bstr_dup_c("/%f0%80%81%b4est");
    expected = bstr_dup_c("/test");
    expected_flags = HTP_PATH_UTF8_OVERLONG;
    PATH_DECODE_TEST_AFTER();

    printf("\n");
    printf("Total tests: %i, %i failure(s).\n", tests, failures);

    return 0;
}

int main_multipart1(int argc, char** argv) {
// int main(int argc, char** argv) {
    htp_mpartp_t *mpartp = NULL;

    htp_cfg_t *cfg = htp_config_create();
    mpartp = htp_mpartp_create(cfg, "BBB");

    const char *i1 = "x0000x\n--BBB\nx1111x\n--\nx2222x\n--";
    const char *i2 = "BBB\nx3333x\n--B";
    const char *i3 = "B\nx4444x\n--B";
    const char *i4 = "B\n--BBB\n\nx5555x\r";
    const char *i5 = "\n--x6666x\r";
    const char *i6 = "-";
    const char *i7 = "-";

    htp_mpartp_parse(mpartp, (unsigned char *)i1, strlen(i1));
    htp_mpartp_parse(mpartp, (unsigned char *)i2, strlen(i2));
    htp_mpartp_parse(mpartp, (unsigned char *)i3, strlen(i3));
    htp_mpartp_parse(mpartp, (unsigned char *)i4, strlen(i4));
    htp_mpartp_parse(mpartp, (unsigned char *)i5, strlen(i5));
    htp_mpartp_parse(mpartp, (unsigned char *)i6, strlen(i6));
    htp_mpartp_parse(mpartp, (unsigned char *)i7, strlen(i7));
    htp_mpartp_finalize(mpartp);

    htp_mpart_part_t *part = NULL;
    list_iterator_reset(mpartp->parts);
    while ((part = (htp_mpart_part_t *) list_iterator_next(mpartp->parts)) != NULL) {
        if (part->name != NULL) fprint_bstr(stdout, "NAME", part->name);
        if (part->value != NULL) fprint_bstr(stdout, "VALUE", part->value);
    }

    htp_mpartp_destroy(&mpartp);

    // "x0000x"
    // "x1111x\n--\nx2222x"
    // "x3333x"
    // "\n--B"
    // "B\nx4444x"
    // "\n--B"
    // "B"
    // "\nx5555x"
    // "\r"
    // "\n--x6666x"

    return 0;
}

int main_multipart2(int argc, char** argv) {
//int main(int argc, char** argv) {
    htp_mpartp_t *mpartp = NULL;
    char boundary[] = "---------------------------41184676334";

    htp_cfg_t *cfg = htp_config_create();
    htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
    htp_config_register_request_file_data(cfg, callback_request_file_data);
    htp_config_register_urlencoded_parser(cfg);
    htp_config_register_multipart_parser(cfg);

    htp_connp_create(cfg);
    mpartp = htp_mpartp_create(cfg, boundary);

    mpartp->extract_files = 1;
    mpartp->extract_dir = "c:/temp";

    const char *parts[999];
    int i = 1;
    parts[i++] = "-----------------------------41184676334\r\n";
    parts[i++] = "Content-Disposition: form-data;\n name=\"field1\"\r\n";
    parts[i++] = "\r\n";
    parts[i++] = "0123456789\r\n-";
    parts[i++] = "-------------";
    parts[i++] = "---------------41184676334\r\n";
    parts[i++] = "Content-Disposition: form-data;\n name=\"field3\"\r\n";
    parts[i++] = "\r\n";
    parts[i++] = "0123456789\r\n-";
    parts[i++] = "-------------";
    parts[i++] = "--------------X\r\n";
    parts[i++] = "-----------------------------41184676334\r\n";
    parts[i++] = "Content-Disposition: form-data;\n";
    parts[i++] = " ";
    parts[i++] = "name=\"field2\"\r\n";
    parts[i++] = "\r\n";
    parts[i++] = "9876543210\r\n";
    parts[i++] = "-----------------------------41184676334\r\n";
    parts[i++] = "Content-Disposition: form-data; name=\"file1\"; filename=\"New Text Document.txt\"\r\nContent-Type: text/plain\r\n\r\n";
    parts[i++] = "1FFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n";
    parts[i++] = "2FFFFFFFFFFFFFFFFFFFFFFFFFFE\r";
    parts[i++] = "3FFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n4FFFFFFFFFFFFFFFFFFFFFFFFF123456789";
    parts[i++] = "\r\n";
    parts[i++] = "-----------------------------41184676334\r\n";
    parts[i++] = "Content-Disposition: form-data; name=\"file2\"; filename=\"New Text Document.txt\"\r\n";
    parts[i++] = "Content-Type: text/plain\r\n";
    parts[i++] = "\r\n";
    //parts[i++] = "FFFFFFFFFFFFFFFFFFFFFFFFFFFZ\r\n";
    //parts[i++] = "-----------------------------41184676334--\r\n";
    parts[i++] = NULL;

    i = 1;
    for (;;) {
        if (parts[i] == NULL) break;
        htp_mpartp_parse(mpartp, (unsigned char *)parts[i], strlen(parts[i]));
        i++;
    }

    //int fd = open("c:/temp/test.zip", O_RDONLY | O_BINARY);
    int fd = open("c:/temp/test.dat", O_RDONLY | O_BINARY);
    if (fd < 0) return -1;

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) {
        return -1;
    }

    char *buf = malloc(statbuf.st_size);
    int buflen = 0;

    int bytes_read = 0;
    while ((bytes_read = read(fd, buf + buflen, statbuf.st_size - buflen)) > 0) {
        buflen += bytes_read;
    }

    if (buflen != statbuf.st_size) {
        free(buf);
        return -2;
    }

    close(fd);

    htp_mpartp_parse(mpartp, (unsigned char *)buf, buflen);

    free(buf);

    char *final = "\r\n-----------------------------41184676334--";
    htp_mpartp_parse(mpartp, (unsigned char *)final, strlen(final));

    htp_mpartp_finalize(mpartp);

    /*
    htp_mpart_part_t *part = NULL;
    list_iterator_reset(mpartp->parts);
    while ((part = (htp_mpart_part_t *) list_iterator_next(mpartp->parts)) != NULL) {
        if (part->name != NULL) fprint_bstr(stdout, "NAME", part->name);
        if (part->value != NULL) fprint_bstr(stdout, "VALUE", part->value);
    }
    */

    htp_mpartp_destroy(&mpartp);

    return 0;
}


