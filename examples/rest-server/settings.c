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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rest-list.h"
#include "settings.h"
#include "version.h"
#include "security.h"

const char *argp_program_version = RESTSERVER_FULL_VERSION;

static char doc[] = "Restserver - interface to LwM2M server and all clients connected to it";

static struct argp_option options[] =
{
    {"log",   'l', "LOGGING_LEVEL", 0, "Specify logging level (0-5)" },
    {"config",   'c', "FILE", 0, "Specify parameters configuration file" },
    {"private_key",   'k', "PRIVATE_KEY", 0, "Specify TLS security private key file" },
    {"certificate",   'C', "CERTIFICATE", 0, "Specify TLS security certificate file" },
    { 0 }
};

static void set_coap_settings(json_t *section, coap_settings_t *settings)
{
    const char *key;
    const char *section_name = "coap";
    json_t *value;

    json_object_foreach(section, key, value)
    {
        if (strcmp(key, "port") == 0)
        {
            settings->port = (uint16_t) json_integer_value(value);
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file key: %s.%s\n",
                    section_name, key);
        }
    }
}

static int set_user_settings(json_t *user_settings, rest_list_t *users_list)
{
    const char *user_name;
    user_t *user, *user_entry;
    json_t *j_name, *j_secret, *j_scope;
    rest_list_entry_t *entry;

    j_name = json_object_get(user_settings, "name");
    j_secret = json_object_get(user_settings, "secret");
    j_scope = json_object_get(user_settings, "scope");

    if (!json_is_string(j_name) || strlen(json_string_value(j_name)) < 1)
    {
        fprintf(stdout, "User configured without name.\n");
        return 1;
    }

    user_name = json_string_value(j_name);

    for (entry = users_list->head; entry != NULL; entry = entry->next)
    {
        user_entry = entry->data;

        if (strcmp(user_entry->name, user_name) == 0)
        {
            fprintf(stdout, "Found duplicate \"%s\" user name in config\n", user_name);
            return 1;
        }
    }

    if (!json_is_string(j_secret) || strlen(json_string_value(j_secret)) < 1)
    {
        fprintf(stdout, "User \"%s\" configured without valid secret key.\n", user_name);
        return 1;
    }

    if (!json_is_array(j_scope))
    {
        fprintf(stdout, "User \"%s\" configured without valid scope. Setting default scope.\n", user_name);
        j_scope = json_array();
    }

    user = security_user_new();

    security_user_set(user, user_name, json_string_value(j_secret), j_scope);

    rest_list_add(users_list, user);

    return 0;
}

static void set_jwt_settings(json_t *section, jwt_settings_t *settings)
{
    size_t user_index;
    const char *key, *method;
    const char *section_name = "http.security.jwt";
    json_t *value, *user_settings;

    json_object_foreach(section, key, value)
    {
        if (strcasecmp(key, "algorithm") == 0)
        {
            settings->algorithm = jwt_str_alg(json_string_value(value));
        }
        else if (strcasecmp(key, "expiration_time") == 0)
        {
            if (json_is_integer(value))
            {
                settings->expiration_time = json_integer_value(value);
            }
            else
            {
                fprintf(stdout, "Token %s must be an integer\n", key);
            }
        }
        else if (strcasecmp(key, "method") == 0)
        {
            method = json_string_value(value);
            if (strcasecmp(method, "header") == 0)
            {
                settings->method = HEADER;
            }
            else if (strcasecmp(method, "body") == 0)
            {
                settings->method = BODY;
            }
            else
            {
                fprintf(stdout, "Unrecognised %s.%s value: %s\n",
                        section_name, key, method);
            }
        }
        else if (strcasecmp(key, "decode_key") == 0)
        {
            if (json_is_string(value))
            {
                settings->jwt_decode_key = (unsigned char *) json_string_value(value);
            }
            else
            {
                fprintf(stdout, "Token %s must be a string\n", key);
            }
        }
        else if (strcasecmp(key, "users") == 0)
        {
            json_array_foreach(value, user_index, user_settings)
            {
                set_user_settings(user_settings, settings->users_list);
            }
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file key: %s.%s\n",
                    section_name, key);
        }
    }
}

static void set_http_security_settings(json_t *section, http_security_settings_t *settings)
{
    const char *key;
    const char *section_name = "http.security";
    json_t *value;

    json_object_foreach(section, key, value)
    {
        if (strcasecmp(key, "private_key") == 0)
        {
            settings->private_key = (char *) json_string_value(value);
        }
        else if (strcasecmp(key, "certificate") == 0)
        {
            settings->certificate = (char *) json_string_value(value);
        }
        else if (strcasecmp(key, "jwt") == 0)
        {
            set_jwt_settings(value, &settings->jwt);
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file key: %s.%s\n",
                    section_name, key);
        }
    }
}

static void set_http_settings(json_t *section, http_settings_t *settings)
{
    const char *key, *security_key;
    const char *section_name = "http";
    json_t *value, *security_value;

    json_object_foreach(section, key, value)
    {
        if (strcmp(key, "port") == 0)
        {
            settings->port = (uint16_t) json_integer_value(value);
        }
        else if (strcmp(key, "security") == 0)
        {
            set_http_security_settings(value, &settings->security);
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file key: %s.%s\n",
                    section_name, key);
        }
    }
}

static void set_logging_settings(json_t *section, logging_settings_t *settings)
{
    const char *key;
    const char *section_name = "logging";
    json_t *value;

    json_object_foreach(section, key, value)
    {
        if (strcmp(key, "level") == 0)
        {
            settings->level = (logging_level_t) json_integer_value(value);
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file key: %s.%s\n",
                    section_name, key);
        }
    }
}

int read_config(char *config_name, settings_t *settings)
{
    json_error_t error;
    const char *section;
    json_t *value;

    json_t *settings_json = json_object();

    settings_json = json_load_file(config_name, 0, &error);

    if (settings_json == NULL)
    {
        fprintf(stderr, "%s:%d:%d error:%s \n",
                config_name, error.line, error.column, error.text);
        return 1;
    }

    json_object_foreach(settings_json, section, value)
    {
        if (strcmp(section, "coap") == 0)
        {
            set_coap_settings(value, &settings->coap);
        }
        else if (strcmp(section, "http") == 0)
        {
            set_http_settings(value, &settings->http);
        }
        else if (strcmp(section, "logging") == 0)
        {
            set_logging_settings(value, &settings->logging);
        }
        else
        {
            fprintf(stdout, "Unrecognised configuration file section: %s\n", section);
        }
    }

    return 0;
}

error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    settings_t *settings = state->input;

    switch (key)
    {
    case 'l':
        settings->logging.level = atoi(arg);
        break;

    case 'c':
        if (read_config(arg, settings) != 0)
        {
            argp_usage(state);
            return 1;
        }
        break;

    case 'C':
        settings->http.security.certificate = arg;
        break;

    case 'k':
        settings->http.security.private_key = arg;
        break;


    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc };

int settings_init(int argc, char *argv[], settings_t *settings)
{
    return argp_parse(&argp, argc, argv, 0, 0, settings);
}
