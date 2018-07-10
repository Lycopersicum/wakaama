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

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <jansson.h>
#include <regex.h>
#include <ulfius.h>

#include "security.h"
#include "logging.h"
#include "http_codes.h"

static char *read_file(const char *filename)
{
    char *buffer = NULL;
    long length;
    FILE *f = fopen(filename, "rb");
    if (filename != NULL)
    {

        if (f)
        {
            fseek(f, 0, SEEK_END);
            length = ftell(f);
            fseek(f, 0, SEEK_SET);
            buffer = malloc(length + 1);
            if (buffer)
            {
                fread(buffer, 1, length, f);
            }
            buffer[length] = '\0';
            fclose(f);
        }
        return buffer;
    }
    else
    {
        return NULL;
    }
}

int security_load(http_security_settings_t *settings)
{
    if (settings->private_key == NULL || settings->certificate == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "Not enough security files provided\n");
        return 1;
    }

    settings->private_key_file = read_file(settings->private_key);
    settings->certificate_file = read_file(settings->certificate);

    if (settings->private_key_file == NULL || settings->certificate_file == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "Failed to read security files\n");
        return 1;
    }
    log_message(LOG_LEVEL_TRACE, "Successfully loaded security configuration\n");

    return 0;
}

user_t *security_user_new(void)
{
    user_t *user;

    user = malloc(sizeof(user_t));
    if (user == NULL)
    {
        log_message(LOG_LEVEL_FATAL, "[JWT] Failed to allocate user memory");
        exit(1);
    }

    memset(user, 0, sizeof(user_t));

    return user;
}

void security_user_delete(user_t *user)
{
    if (user->name)
    {
        memset(user->name, 0, strlen(user->name));
    }

    if (user->secret)
    {
        memset(user->secret, 0, strlen(user->secret));
    }

    if (user->j_scope_list)
    {
        json_decref(user->j_scope_list);
        user->j_scope_list = NULL;
    }

    free(user);
}

int security_user_set(user_t *user, const char *name, const char *secret, json_t *scope)
{
    if (user->name)
    {
        free((void *)user->name);
        user->name = NULL;
    }

    if (user->secret)
    {
        free((void *)user->secret);
        user->secret = NULL;
    }

    if (user->j_scope_list)
    {
        json_decref(user->j_scope_list);
    }

    if (name != NULL)
    {
        user->name = strdup(name);
    }

    if (secret != NULL)
    {
        user->secret = strdup(secret);
    }

    user->j_scope_list = scope == NULL ? json_array() : json_deep_copy(scope);

    if (json_is_array(user->j_scope_list) == 0 || user->name == NULL || user->secret == NULL)
    {
        return 1;
    }

    return 0;
}

int security_unload(http_security_settings_t *settings)
{
    memset(settings->private_key, 0, strlen(settings->private_key));
    memset(settings->certificate, 0, strlen(settings->certificate));
    memset(settings->private_key_file, 0, strlen(settings->private_key_file));
    memset(settings->certificate_file, 0, strlen(settings->certificate_file));

    log_message(LOG_LEVEL_TRACE, "Successfully unloaded security");
    return 0;
}

void jwt_users_cleanup(rest_list_t *users_list)
{
    rest_list_entry_t *entry;
    user_t *user;

    for (entry = users_list->head; entry != NULL; entry = entry->next)
    {
        user = (user_t *) entry->data;
        security_user_delete(user);
    }

    rest_list_delete(users_list);
}

static int get_request_token(const struct _u_request *request, jwt_settings_t *jwt_settings,
                             char **token_string)
{
    const char *token, *authorization_header;

    if (token_string == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "[JWT] No token string address specified\n");
        return 1;
    }

    switch (jwt_settings->method)
    {
    case HEADER:
        authorization_header = u_map_get(request->map_header, HEADER_AUTHORIZATION);

        if (authorization_header == NULL)
        {
            log_message(LOG_LEVEL_TRACE, "[JWT] Failed to find authorization header in request\n");
            return 1;
        }

        if (strstr(authorization_header, HEADER_PREFIX_BEARER) != authorization_header)
        {
            log_message(LOG_LEVEL_TRACE, "[JWT] Authorization type is not %s\n", HEADER_PREFIX_BEARER);
            return 1;
        }

        token = authorization_header + strlen(HEADER_PREFIX_BEARER);
        break;

    case BODY:
        token = u_map_get(request->map_post_body, BODY_PARAMETER);

        if (token == NULL)
        {
            log_message(LOG_LEVEL_TRACE, "[JWT] Access token parameter not found in request body\n");
            return 1;
        }

        if (strstr(u_map_get(request->map_header, ULFIUS_HTTP_HEADER_CONTENT),
                   MHD_HTTP_POST_ENCODING_FORM_URLENCODED) == NULL)
        {
            log_message(LOG_LEVEL_TRACE, "[JWT] Access token parameter not encoded in request body\n");
            return 1;
        }
        break;

    default:
        log_message(LOG_LEVEL_TRACE, "[JWT] Invalid JWT method specified\n");
        return 1;
    }

    *token_string = (char *)malloc(strlen(token) + 1);

    if (*token_string == NULL)
    {
        log_message(LOG_LEVEL_FATAL, "[JWT] Failed to allocate token string memory");
        exit(1);
    }

    strcpy(*token_string, token);

    return 0;
}

static jwt_error_t validate_token(jwt_settings_t *settings, json_t *j_token)
{
    time_t current_time;
    json_int_t expiration_time;
    json_t *j_issuing_time, *j_user_name;

    j_user_name = json_object_get(j_token, "name");
    if (j_user_name == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] User is not specified in access token\n");
        return J_ERROR_INVALID_TOKEN;
    }
    else if (!json_is_string(j_user_name) && strlen(json_string_value(j_user_name)) < 1)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] Name specified in token must be not empty string\n");
        return J_ERROR_INVALID_TOKEN;
    }

    j_issuing_time = json_object_get(j_token, "iat");
    if (j_issuing_time == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] Token issuing time is unspecified\n");
        return J_ERROR_INVALID_TOKEN;
    }

    expiration_time = json_integer_value(j_issuing_time) + settings->expiration_time;
    time(&current_time);

    if (current_time >= expiration_time)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] User \"%s\" submitted expired token\n",
                    json_string_value(j_user_name));
        return J_ERROR_EXPIRED_TOKEN;
    }

    return J_OK;
}

static int validate_authentication_body(json_t *authentication_json)
{
    json_t *jname, *jsecret;

    if (authentication_json == NULL)
    {
        return 1;
    }

    if (!json_is_object(authentication_json)
        || json_object_size(authentication_json) != 2)
    {
        return 1;
    }

    jname = json_object_get(authentication_json, "name");
    jsecret = json_object_get(authentication_json, "secret");

    if (!json_is_string(jname) || !json_is_string(jsecret))
    {
        return 1;
    }

    return 0;
}

static int check_user_scope(char *required_scope, user_t *user)
{
    size_t index;
    json_t *j_scope_pattern;
    const char *scope_pattern;
    regex_t regex;

    json_array_foreach(user->j_scope_list, index, j_scope_pattern)
    {
        scope_pattern = json_string_value(j_scope_pattern);

        regcomp(&regex, scope_pattern, REG_EXTENDED);

        if (regexec(&regex, required_scope, 0, NULL, REG_EXTENDED) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static jwt_error_t check_request_token_scope(const struct _u_request *request,
                                             jwt_settings_t *jwt_settings, char *required_scope)
{
    char *token_string, *grants_string;
    const char *user_name;
    json_t *j_grants;
    rest_list_entry_t *entry;
    user_t *user = NULL, *user_entry;
    jwt_t *jwt;
    jwt_error_t status;

    if (jwt_settings == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] Unspecified JWT settings\n");
        return J_ERROR_INTERNAL;
    }

    if (get_request_token(request, jwt_settings, &token_string) != 0)
    {
        return J_ERROR_INVALID_REQUEST;
    }

    if (jwt_decode(&jwt, token_string, jwt_settings->jwt_decode_key,
                   strlen((char *) jwt_settings->jwt_decode_key)))
    {
        log_message(LOG_LEVEL_TRACE,
                    "[JWT] Invalid or corrupt token given (unable to decode and verify)\n");
        free(token_string);
        return J_ERROR_INVALID_TOKEN;
    }

    grants_string = jwt_get_grants_json(jwt, NULL);
    j_grants = json_loads(grants_string, JSON_DECODE_ANY, NULL);

    if (j_grants == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] Invalid or corrupt token given (JWT is missing grants)\n");
        free(token_string);
        jwt_free(jwt);
        return J_ERROR_INVALID_TOKEN;
    }

    status = validate_token(jwt_settings, j_grants);
    if (status != J_OK)
    {
        goto exit;
    }

    user_name = json_string_value(json_object_get(j_grants, "name"));

    for (entry = jwt_settings->users_list->head; entry != NULL; entry = entry->next)
    {
        user_entry = entry->data;

        if (strcmp(user_entry->name, user_name) == 0)
        {
            user = user_entry;
            break;
        }
    }

    if (user == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] User \"%s\" not found in configured users list\n", user_name);
        status = J_ERROR_INSUFFICIENT_SCOPE;
        goto exit;
    }

    if (check_user_scope(required_scope, user) != 0)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] User \"%s\" does not have scope to %s\n", user_name,
                    required_scope);
        status = J_ERROR_INSUFFICIENT_SCOPE;
        goto exit;
    }

    status = J_OK;

exit:
    free(token_string);
    free(grants_string);
    jwt_free(jwt);
    return status;
}

int authenticate_user_cb(const struct _u_request *request, struct _u_response *response,
                         void *user_data)
{
    jwt_settings_t *jwt_settings = (jwt_settings_t *)user_data;
    const char *user_name, *user_secret;
    char *token;
    time_t issuing_time;
    jwt_t *jwt;
    char *method_string;
    json_t *j_request_body, *j_response_body;
    rest_list_entry_t *entry;
    user_t *user = NULL, *user_entry;
    int status = U_ERROR;

    j_response_body = json_object();

    log_message(LOG_LEVEL_TRACE, "[JWT] Authentication callback begins!\n");

    j_request_body = json_loadb(request->binary_body, request->binary_body_length, 0, NULL);

    if (validate_authentication_body(j_request_body) != 0)
    {
        log_message(LOG_LEVEL_INFO, "[JWT] Invalid authentication request body\n");

        json_object_set_new(j_response_body, "error", json_string("invalid_request"));

        ulfius_set_json_body_response(response, HTTP_400_BAD_REQUEST, j_response_body);

        return U_CALLBACK_COMPLETE;
    }

    user_name = json_string_value(json_object_get(j_request_body, "name"));
    user_secret = json_string_value(json_object_get(j_request_body, "secret"));

    for (entry = jwt_settings->users_list->head; entry != NULL; entry = entry->next)
    {
        user_entry = entry->data;

        if (strcmp(user_entry->name, user_name) == 0
            && strcmp(user_entry->secret, user_secret) == 0)
        {
            user = user_entry;
            break;
        }
    }

    if (user == NULL)
    {
        log_message(LOG_LEVEL_TRACE, "[JWT] User \"%s\" failed to authenticate\n", user_name);

        json_object_set_new(j_response_body, "error", json_string("invalid_client"));

        ulfius_set_json_body_response(response, HTTP_400_BAD_REQUEST, j_response_body);

        return U_CALLBACK_COMPLETE;
    }

    if (jwt_new(&jwt) != 0)
    {
        log_message(LOG_LEVEL_WARN, "[JWT] Unable to create new JWT object\n");
        return U_ERROR;
    }

    time(&issuing_time);

    jwt_set_alg(jwt, jwt_settings->algorithm, jwt_settings->jwt_decode_key,
                strlen((char *) jwt_settings->jwt_decode_key));

    jwt_add_grant(jwt, "name", user->name);
    jwt_add_grant_int(jwt, "iat", (long) issuing_time);

    token = jwt_encode_str(jwt);

    switch (jwt_settings->method)
    {
    case HEADER:
        method_string = "header";
        break;
    case BODY:
        method_string = "body";
        break;
    default:
        log_message(LOG_LEVEL_WARN, "[JWT] Invalid JWT method specified in jwt settings\n");
        goto exit;
    }

    json_object_set_new(j_response_body, "access_token", json_string(token));
    json_object_set_new(j_response_body, "method", json_string(method_string));
    json_object_set_new(j_response_body, "expires_in", json_integer(jwt_settings->expiration_time));

    log_message(LOG_LEVEL_INFO, "[JWT] Access token issued to user \"%s\".\n", user->name);

    ulfius_set_json_body_response(response, HTTP_200_OK, j_response_body);

    status = U_OK;

exit:
    json_decref(j_request_body);
    json_decref(j_response_body);
    free(jwt);
    return status;
}

int validate_request_scope(const struct _u_request *request, struct _u_response *response,
                           jwt_settings_t *jwt_settings)
{
    jwt_error_t token_scope_status;

    char *required_scope = malloc(strlen(request->http_verb) + strlen(request->http_url) + 2);

    if (required_scope == NULL)
    {
        log_message(LOG_LEVEL_FATAL, "[JWT] Failed to allocate required scope string memory");
        exit(1);
    }

    strcpy(required_scope, request->http_verb);
    strcat(required_scope, " ");
    strcat(required_scope, request->http_url);

    if (jwt_settings->users_list->head != NULL)
    {
        token_scope_status = check_request_token_scope(request, jwt_settings, required_scope);
        free(required_scope);

        switch (token_scope_status)
        {
        case J_OK:
            return U_CALLBACK_CONTINUE;
        case J_ERROR_INVALID_REQUEST:
            u_map_put(response->map_header, HEADER_UNAUTHORIZED,
                      "error=\"invalid_request\",error_description=\"The access token is missing\"");
            return U_CALLBACK_UNAUTHORIZED;
        case J_ERROR_INVALID_TOKEN:
        case J_ERROR_EXPIRED_TOKEN:
            u_map_put(response->map_header, HEADER_UNAUTHORIZED,
                      "error=\"invalid_token\",error_description=\"The access token is invalid\"");
            return U_CALLBACK_UNAUTHORIZED;
        case J_ERROR_INSUFFICIENT_SCOPE:
            u_map_put(response->map_header, HEADER_UNAUTHORIZED,
                      "error=\"invalid_scope\",error_description=\"The scope is invalid\"");
            return U_CALLBACK_UNAUTHORIZED;
        case J_ERROR:
        case J_ERROR_INTERNAL:
        default:
            return U_CALLBACK_ERROR;
        }
    }

    free(required_scope);
    return U_CALLBACK_CONTINUE;
}