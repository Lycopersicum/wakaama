/*
 * MIT License
 *
 * Copyright (c) 2018 8devices
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <jwt.h>
#include <ulfius.h>
#include <stdint.h>

#include "rest-list.h"

#define HEADER_USER_NAME "Name"
#define HEADER_USER_SECRET "Secret"
#define HEADER_AUTHORIZATION "Authorization"
#define HEADER_UNAUTHORIZED "WWW-Authenticate"
#define BODY_URL_PARAMETER "access_token"

typedef enum
{
    J_OK,
    J_ERROR,
    J_ERROR_INTERNAL,
    J_ERROR_INVALID_REQUEST,
    J_ERROR_INVALID_TOKEN,
    J_ERROR_EXPIRED_TOKEN,
    J_ERROR_INSUFFICIENT_SCOPE
} jwt_error_t;

typedef enum { HEADER, BODY, URL } jwt_method_t;

typedef struct
{
    char *name;
    char *secret;
    json_t *j_scope_list;
} user_t;

typedef struct
{
    jwt_alg_t algorithm;
    jwt_method_t method;
    const unsigned char *jwt_decode_key;
    uint8_t accept_access_token;
    uint8_t accept_client_token;
    rest_list_t *users_list;
    json_int_t expiration_time;
} jwt_settings_t;

typedef struct
{
    char *private_key;
    char *certificate;
    char *private_key_file;
    char *certificate_file;
    jwt_settings_t jwt;
} http_security_settings_t;

int security_load(http_security_settings_t *settings);
int security_unload(http_security_settings_t *settings);

user_t *security_user_new();
int security_user_set(user_t *user, const char *name, const char *secret, json_t *scope);
void security_user_delete(user_t *user);

int authenticate_user_cb(const struct _u_request *request, struct _u_response *response,
                         void *user_data);

int validate_request_scope(const struct _u_request *request, struct _u_response *response,
                           jwt_settings_t *jwt_settings);

#endif // SECURITY_H
