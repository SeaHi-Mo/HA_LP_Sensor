#include <errno.h>
#include <aiio_log.h>
#include <aiio_err.h>
#include <http_parser.h>

#include "aiio_http_server.h"
#include "aiio_httpd_priv.h"


static bool httpd_uri_match_simple(const char *uri1, const char *uri2, size_t len2)
{
    return strlen(uri1) == len2 &&          // First match lengths
        (strncmp(uri1, uri2, len2) == 0);   // Then match actual URIs
}

bool aiio_httpd_uri_match_wildcard(const char *template, const char *uri, size_t len)
{
    const size_t tpl_len = strlen(template);
    size_t exact_match_chars = tpl_len;

    /* Check for trailing question mark and asterisk */
    const char last = (const char) (tpl_len > 0 ? template[tpl_len - 1] : 0);
    const char prevlast = (const char) (tpl_len > 1 ? template[tpl_len - 2] : 0);
    const bool asterisk = last == '*' || (prevlast == '*' && last == '?');
    const bool quest = last == '?' || (prevlast == '?' && last == '*');

    /* Minimum template string length must be:
     *      0 : if neither of '*' and '?' are present
     *      1 : if only '*' is present
     *      2 : if only '?' is present
     *      3 : if both are present
     *
     * The expression (asterisk + quest*2) serves as a
     * case wise generator of these length values
     */

    /* abort in cases such as "?" with no preceding character (invalid template) */
    if (exact_match_chars < asterisk + quest*2) {
        return false;
    }

    /* account for special characters and the optional character if "?" is used */
    exact_match_chars -= asterisk + quest*2;

    if (len < exact_match_chars) {
        return false;
    }

    if (!quest) {
        if (!asterisk && len != exact_match_chars) {
            /* no special characters and different length - strncmp would return false */
            return false;
        }
        /* asterisk allows arbitrary trailing characters, we ignore these using
         * exact_match_chars as the length limit */
        return (strncmp(template, uri, exact_match_chars) == 0);
    } else {
        /* question mark present */
        if (len > exact_match_chars && template[exact_match_chars] != uri[exact_match_chars]) {
            /* the optional character is present, but different */
            return false;
        }
        if (strncmp(template, uri, exact_match_chars) != 0) {
            /* the mandatory part differs */
            return false;
        }
        /* Now we know the URI is longer than the required part of template,
         * the mandatory part matches, and if the optional character is present, it is correct.
         * Match is OK if we have asterisk, i.e. any trailing characters are OK, or if
         * there are no characters beyond the optional character. */
        return asterisk || len <= exact_match_chars + 1;
    }
}

/* Find handler with matching URI and method, and set
 * appropriate error code if URI or method not found */
static aiio_httpd_uri_t* httpd_find_uri_handler(struct aiio_httpd_data *hd,
                                           const char *uri, size_t uri_len,
                                           aiio_httpd_method_t method,
                                           aiio_httpd_err_code_t *err)
{
    if (err) {
        *err = HTTPD_404_NOT_FOUND;
    }

    for (int i = 0; i < hd->config.max_uri_handlers; i++) {
        if (!hd->hd_calls[i]) {
            break;
        }
        aiio_log_d("[%d] = %s", i, hd->hd_calls[i]->uri);

        /* Check if custom URI matching function is set,
         * else use simple string compare */
        if (hd->config.uri_match_fn ?
            hd->config.uri_match_fn(hd->hd_calls[i]->uri, uri, uri_len) :
            httpd_uri_match_simple(hd->hd_calls[i]->uri, uri, uri_len)) {
            /* URIs match. Now check if method is supported */
            if (hd->hd_calls[i]->method == method) {
                /* Match found! */
                if (err) {
                    /* Unset any error that may
                     * have been set earlier */
                    *err = 0;
                }
                return hd->hd_calls[i];
            }
            /* URI found but method not allowed.
             * If URI is found later then this
             * error must be set to 0 */
            if (err) {
                *err = HTTPD_405_METHOD_NOT_ALLOWED;
            }
        }
    }
    return NULL;
}

aiio_err_t aiio_httpd_register_uri_handler(aiio_httpd_handle_t handle,
                                     const aiio_httpd_uri_t *uri_handler)
{
    if (handle == NULL || uri_handler == NULL) {
        return AIIO_ERR_INVALID_ARG;
    }

    struct aiio_httpd_data *hd = (struct aiio_httpd_data *) handle;

    /* Make sure another handler with matching URI and method
     * is not already registered. This will also catch cases
     * when a registered URI wildcard pattern already accounts
     * for the new URI being registered */
    if (httpd_find_uri_handler(handle, uri_handler->uri,
                               strlen(uri_handler->uri),
                               uri_handler->method, NULL) != NULL) {
        aiio_log_w("handler %s with method %d already registered",
                 uri_handler->uri, uri_handler->method);
        return AIIO_ERR_HTTPD_HANDLER_EXISTS;
    }

    for (int i = 0; i < hd->config.max_uri_handlers; i++) {
        if (hd->hd_calls[i] == NULL) {
            hd->hd_calls[i] = malloc(sizeof(aiio_httpd_uri_t));
            if (hd->hd_calls[i] == NULL) {
                /* Failed to allocate memory */
                return AIIO_ERR_HTTPD_ALLOC_MEM;
            }

            /* Copy URI string */
            hd->hd_calls[i]->uri = strdup(uri_handler->uri);
            if (hd->hd_calls[i]->uri == NULL) {
                /* Failed to allocate memory */
                free(hd->hd_calls[i]);
                return AIIO_ERR_HTTPD_ALLOC_MEM;
            }

            /* Copy remaining members */
            hd->hd_calls[i]->method   = uri_handler->method;
            hd->hd_calls[i]->handler  = uri_handler->handler;
            hd->hd_calls[i]->user_ctx = uri_handler->user_ctx;
#ifdef CONFIG_HTTPD_WS_SUPPORT
            hd->hd_calls[i]->is_websocket = uri_handler->is_websocket;
            hd->hd_calls[i]->handle_ws_control_frames = uri_handler->handle_ws_control_frames;
            if (uri_handler->supported_subprotocol) {
                hd->hd_calls[i]->supported_subprotocol = strdup(uri_handler->supported_subprotocol);
            } else {
                hd->hd_calls[i]->supported_subprotocol = NULL;
            }
#endif
            aiio_log_d("[%d] installed %s", i, uri_handler->uri);
            return AIIO_OK;
        }
        aiio_log_d("[%d] exists %s", i, hd->hd_calls[i]->uri);
    }
    aiio_log_w("no slots left for registering handler");
    return AIIO_ERR_HTTPD_HANDLERS_FULL;
}

aiio_err_t aiio_httpd_unregister_uri_handler(aiio_httpd_handle_t handle,
                                       const char *uri, aiio_httpd_method_t method)
{
    if (handle == NULL || uri == NULL) {
        return AIIO_ERR_INVALID_ARG;
    }

    struct aiio_httpd_data *hd = (struct aiio_httpd_data *) handle;
    for (int i = 0; i < hd->config.max_uri_handlers; i++) {
        if (!hd->hd_calls[i]) {
            break;
        }
        if ((hd->hd_calls[i]->method == method) &&       // First match methods
            (strcmp(hd->hd_calls[i]->uri, uri) == 0)) {  // Then match URI string
            aiio_log_d("[%d] removing %s", i, hd->hd_calls[i]->uri);

            free((char*)hd->hd_calls[i]->uri);
            free(hd->hd_calls[i]);
            hd->hd_calls[i] = NULL;

            /* Shift the remaining non null handlers in the array
             * forward by 1 so that order of insertion is maintained */
            for (i += 1; i < hd->config.max_uri_handlers; i++) {
                if (!hd->hd_calls[i]) {
                    break;
                }
                hd->hd_calls[i-1] = hd->hd_calls[i];
            }
            /* Nullify the following non null entry */
            hd->hd_calls[i-1] = NULL;
            return AIIO_OK;
        }
    }
    aiio_log_w("handler %s with method %d not found", uri, method);
    return AIIO_ERR_NOT_FOUND;
}

aiio_err_t aiio_httpd_unregister_uri(aiio_httpd_handle_t handle, const char *uri)
{
    if (handle == NULL || uri == NULL) {
        return AIIO_ERR_INVALID_ARG;
    }

    struct aiio_httpd_data *hd = (struct aiio_httpd_data *) handle;
    bool found = false;

    int i = 0, j = 0; // For keeping count of removed entries
    for (; i < hd->config.max_uri_handlers; i++) {
        if (!hd->hd_calls[i]) {
            break;
        }
        if (strcmp(hd->hd_calls[i]->uri, uri) == 0) {   // Match URI strings
            aiio_log_d("[%d] removing %s", i, uri);

            free((char*)hd->hd_calls[i]->uri);
            free(hd->hd_calls[i]);
            hd->hd_calls[i] = NULL;
            found = true;

            j++; // Update count of removed entries
        } else {
            /* Shift the remaining non null handlers in the array
             * forward by j so that order of insertion is maintained */
            hd->hd_calls[i-j] = hd->hd_calls[i];
        }
    }
    /* Nullify the following non null entries */
    for (int k = (i - j); k < i; k++) {
        hd->hd_calls[k] = NULL;
    }

    if (!found) {
        aiio_log_w("no handler found for URI %s", uri);
    }
    return (found ? AIIO_OK : AIIO_ERR_NOT_FOUND);
}

void aiio_httpd_unregister_all_uri_handlers(struct aiio_httpd_data *hd)
{
    for (unsigned i = 0; i < hd->config.max_uri_handlers; i++) {
        if (!hd->hd_calls[i]) {
            break;
        }
        aiio_log_d("[%d] removing %s", i, hd->hd_calls[i]->uri);

        free((char*)hd->hd_calls[i]->uri);
        free(hd->hd_calls[i]);
        hd->hd_calls[i] = NULL;
    }
}

aiio_err_t aiio_httpd_uri(struct aiio_httpd_data *hd)
{
    aiio_httpd_uri_t            *uri = NULL;
    aiio_httpd_req_t            *req = &hd->hd_req;
    struct http_parser_url *res = &hd->hd_req_aux.url_parse_res;

    /* For conveying URI not found/method not allowed */
    aiio_httpd_err_code_t err = 0;

    aiio_log_d("request for %s with type %d", req->uri, req->method);

    /* URL parser result contains offset and length of path string */
    if (res->field_set & (1 << UF_PATH)) {
        uri = httpd_find_uri_handler(hd, req->uri + res->field_data[UF_PATH].off,
                                     res->field_data[UF_PATH].len, req->method, &err);
    }

    /* If URI with method not found, respond with error code */
    if (uri == NULL) {
        switch (err) {
            case HTTPD_404_NOT_FOUND:
                aiio_log_w("URI '%s' not found", req->uri);
                return aiio_httpd_req_handle_err(req, HTTPD_404_NOT_FOUND);
            case HTTPD_405_METHOD_NOT_ALLOWED:
                aiio_log_w("Method '%d' not allowed for URI '%s'",
                         req->method, req->uri);
                return aiio_httpd_req_handle_err(req, HTTPD_405_METHOD_NOT_ALLOWED);
            default:
                return AIIO_FAIL;
        }
    }

    /* Attach user context data (passed during URI registration) into request */
    req->user_ctx = uri->user_ctx;

    /* Final step for a WebSocket handshake verification */
#ifdef CONFIG_HTTPD_WS_SUPPORT
    struct aiio_httpd_req_aux   *aux = req->aux;
    if (uri->is_websocket && aux->ws_handshake_detect && uri->method == HTTP_GET) {
        aiio_log_d("Responding WS handshake to sock %d", aux->sd->fd);
        aiio_err_t ret = aiio_httpd_ws_respond_server_handshake(&hd->hd_req, uri->supported_subprotocol);
        if (ret != AIIO_OK) {
            return ret;
        }

        aux->sd->ws_handshake_done = true;
        aux->sd->ws_handler = uri->handler;
        aux->sd->ws_control_frames = uri->handle_ws_control_frames;
        aux->sd->ws_user_ctx = uri->user_ctx;
    }
#endif

    /* Invoke handler */
    if (uri->handler(req) != AIIO_OK) {
        /* Handler returns error, this socket should be closed */
        aiio_log_w("uri handler execution failed");
        return AIIO_FAIL;
    }
    return AIIO_OK;
}
