
#include <string.h>
#include <stdlib.h>

#include "aiio_tls.h"
#include "aiio_log.h"

#include "aiio_transport.h"
#include "aiio_transport_ssl.h"
#include "aiio_transport_utils.h"
#include "aiio_transport_internal.h"

#define INVALID_SOCKET (-1)

#define GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t)         \
    transport_tls_t *ssl = ssl_get_context_data(t);  \
    if (!ssl) { return; }

static const char *TAG = "TRANSPORT_BASE";

typedef enum {
    TRANS_SSL_INIT = 0,
    TRANS_SSL_CONNECTING,
} transport_ssl_conn_state_t;

/**
 *  mbedtls specific transport data
 */
typedef struct transport_tls {
    aiio_tls_t                *tls;
    aiio_tls_cfg_t            cfg;
    bool                     ssl_initialized;
    transport_ssl_conn_state_t conn_state;
    int                      sockfd;
} transport_tls_t;

static inline struct transport_tls * ssl_get_context_data(aiio_transport_handle_t t)
{
    if (!t) {
        return NULL;
    }
    if (t->data) {  // Prefer internal ssl context (independent from the list)
        return (transport_tls_t*)t->data;
    }
    if (t->base && t->base->transport_tls) {    // Next one is the lists inherent context
        t->data = t->base->transport_tls;       // Optimize: if we have base context, use it as internal
        return t->base->transport_tls;
    }
    // If we don't have a valid context, let's to create one
    transport_tls_t *ssl = aiio_transport_tls_create();
    AIIO_TRANSPORT_MEM_CHECK( ssl, return NULL)
    t->data = ssl;
    return ssl;
}

static int aiio_tls_connect_async(aiio_transport_handle_t t, const char *host, int port, int timeout_ms, bool is_plain_tcp)
{
    transport_tls_t *ssl = ssl_get_context_data(t);
    if (ssl->conn_state == TRANS_SSL_INIT) {
        ssl->cfg.timeout_ms = timeout_ms;
        ssl->cfg.is_plain_tcp = is_plain_tcp;
        ssl->cfg.non_block = true;
        ssl->ssl_initialized = true;
        ssl->tls = aiio_tls_init();
        if (!ssl->tls) {
            return -1;
        }
        ssl->conn_state = TRANS_SSL_CONNECTING;
        ssl->sockfd = INVALID_SOCKET;
    }
    if (ssl->conn_state == TRANS_SSL_CONNECTING) {
        int progress = aiio_tls_conn_new_async(host, strlen(host), port, &ssl->cfg, ssl->tls);
        if (progress >= 0) {
            ssl->sockfd = ssl->tls->sockfd;
        }
        return progress;

    }
    return 0;
}

static inline int ssl_connect_async(aiio_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    return aiio_tls_connect_async(t, host, port, timeout_ms, false);
}

static inline int tcp_connect_async(aiio_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    return aiio_tls_connect_async(t, host, port, timeout_ms, true);
}

static int ssl_connect(aiio_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_tls_t *ssl = ssl_get_context_data(t);

    ssl->cfg.timeout_ms = timeout_ms;
    ssl->cfg.is_plain_tcp = false;

    ssl->ssl_initialized = true;
    ssl->tls = aiio_tls_init();
    if (ssl->tls == NULL) {
        aiio_log_e( "Failed to initialize new connection object");
        capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_NO_MEM);
        return -1;
    }
    if (aiio_tls_conn_new_sync(host, strlen(host), port, &ssl->cfg, ssl->tls) <= 0) {
        aiio_log_e( "Failed to open a new connection");
        aiio_transport_set_errors(t, ssl->tls->error_handle);
        aiio_tls_conn_destroy(ssl->tls);
        ssl->tls = NULL;
        ssl->sockfd = INVALID_SOCKET;
        return -1;
    }
    ssl->sockfd = ssl->tls->sockfd;
    return 0;
}

static int tcp_connect(aiio_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_tls_t *ssl = ssl_get_context_data(t);
    aiio_tls_last_error_t *err_handle = aiio_transport_get_error_handle(t);

    ssl->cfg.timeout_ms = timeout_ms;
    int err = aiio_tls_plain_tcp_connect(host, strlen(host), port, &ssl->cfg, err_handle, &ssl->sockfd);
    if (err != AIIO_OK) {
        aiio_log_e( "Failed to open a new connection: %d", err);
        err_handle->last_error = err;
        ssl->sockfd = INVALID_SOCKET;
        return -1;
    }
    return 0;
}

static int base_poll_read(aiio_transport_handle_t t, int timeout_ms)
{
    transport_tls_t *ssl = ssl_get_context_data(t);
    int ret = -1;
    int remain = 0;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET(ssl->sockfd, &readset);
    FD_SET(ssl->sockfd, &errset);

    if (ssl->tls && (remain = aiio_tls_get_bytes_avail(ssl->tls)) > 0) {
        aiio_log_d( "remain data in cache, need to read again");
        return remain;
    }
    ret = select(ssl->sockfd + 1, &readset, NULL, &errset, aiio_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(ssl->sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        aiio_transport_capture_errno(t, sock_errno);
        aiio_log_e( "poll_read select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), ssl->sockfd);
        ret = -1;
    }
    return ret;
}

static int base_poll_write(aiio_transport_handle_t t, int timeout_ms)
{
    transport_tls_t *ssl = ssl_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET(ssl->sockfd, &writeset);
    FD_SET(ssl->sockfd, &errset);
    ret = select(ssl->sockfd + 1, NULL, &writeset, &errset, aiio_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(ssl->sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        aiio_transport_capture_errno(t, sock_errno);
        aiio_log_e( "poll_write select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), ssl->sockfd);
        ret = -1;
    }
    return ret;
}

static int ssl_write(aiio_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = aiio_transport_poll_write(t, timeout_ms)) <= 0) {
        aiio_log_w( "Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d", strerror(errno), ssl->sockfd, timeout_ms);
        return poll;
    }
    int ret = aiio_tls_conn_write(ssl->tls, (const unsigned char *) buffer, len);
    if (ret < 0) {
        aiio_log_e( "aiio_tls_conn_write error, errno=%s", strerror(errno));
        aiio_transport_set_errors(t, ssl->tls->error_handle);
    }
    return ret;
}

static int tcp_write(aiio_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = aiio_transport_poll_write(t, timeout_ms)) <= 0) {
        aiio_log_w( "Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d", strerror(errno), ssl->sockfd, timeout_ms);
        return poll;
    }
    int ret = send(ssl->sockfd,(const unsigned char *) buffer, len, 0);
    if (ret < 0) {
        aiio_log_e( "tcp_write error, errno=%s", strerror(errno));
        aiio_transport_capture_errno(t, errno);
    }
    return ret;
}

static int ssl_read(aiio_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = aiio_transport_poll_read(t, timeout_ms)) <= 0) {
        return poll;
    }
    int ret = aiio_tls_conn_read(ssl->tls, (unsigned char *)buffer, len);
    if (ret < 0) {
        aiio_log_e( "aiio_tls_conn_read error, errno=%s", strerror(errno));
        aiio_transport_set_errors(t, ssl->tls->error_handle);
    }
    if (ret == 0) {
        if (poll > 0) {
            // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
            capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
        }
        ret = -1;
    }
    return ret;
}

static int tcp_read(aiio_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = aiio_transport_poll_read(t, timeout_ms)) <= 0) {
        return poll;
    }
    int ret = recv(ssl->sockfd, (unsigned char *)buffer, len, 0);
    if (ret < 0) {
        aiio_log_e( "tcp_read error, errno=%s", strerror(errno));
        aiio_transport_capture_errno(t, errno);
    }
    if (ret == 0) {
        if (poll > 0) {
            // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
            capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
        }
        ret = -1;
    }
    return ret;
}

static int base_close(aiio_transport_handle_t t)
{
    int ret = -1;
    transport_tls_t *ssl = ssl_get_context_data(t);
    if (ssl && ssl->ssl_initialized) {
        ret = aiio_tls_conn_destroy(ssl->tls);
        ssl->tls = NULL;
        ssl->conn_state = TRANS_SSL_INIT;
        ssl->ssl_initialized = false;
        ssl->sockfd = INVALID_SOCKET;
    } else if (ssl && ssl->sockfd >= 0) {
        ret = close(ssl->sockfd);
        ssl->sockfd = INVALID_SOCKET;
    }
    return ret;
}

static int base_destroy(aiio_transport_handle_t t)
{
    transport_tls_t *ssl = ssl_get_context_data(t);
    if (ssl) {
        aiio_transport_close(t);
        if (t->base && t->base->transport_tls &&
            t->data == t->base->transport_tls) {
            // if internal ssl the same as the foundation transport,
            // just zero out, it will be freed on list destroy
            t->data = NULL;
        }
        aiio_transport_tls_destroy(t->data); // okay to pass NULL
    }
    return 0;
}

void aiio_transport_ssl_enable_global_ca_store(aiio_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.use_global_ca_store = true;
}

#ifdef CONFIG_TLS_PSK_VERIFICATION
void aiio_transport_ssl_set_psk_key_hint(aiio_transport_handle_t t, const psk_hint_key_t* psk_hint_key)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.psk_hint_key =  psk_hint_key;
}
#endif

void aiio_transport_ssl_set_cert_data(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.cacert_pem_buf = (void *)data;
    ssl->cfg.cacert_pem_bytes = len + 1;
}

void aiio_transport_ssl_set_cert_data_der(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.cacert_buf = (void *)data;
    ssl->cfg.cacert_bytes = len;
}

void aiio_transport_ssl_set_client_cert_data(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientcert_pem_buf = (void *)data;
    ssl->cfg.clientcert_pem_bytes = len + 1;
}

void aiio_transport_ssl_set_client_cert_data_der(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientcert_buf = (void *)data;
    ssl->cfg.clientcert_bytes = len;
}

void aiio_transport_ssl_set_client_key_data(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_pem_buf = (void *)data;
    ssl->cfg.clientkey_pem_bytes = len + 1;
}

void aiio_transport_ssl_set_client_key_password(aiio_transport_handle_t t, const char *password, int password_len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_password = (void *)password;
    ssl->cfg.clientkey_password_len = password_len;
}

void aiio_transport_ssl_set_client_key_data_der(aiio_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_buf = (void *)data;
    ssl->cfg.clientkey_bytes = len;
}

#if defined(CONFIG_MBEDTLS_SSL_ALPN) || defined(CONFIG_WOLFSSL_HAVE_ALPN)
void aiio_transport_ssl_set_alpn_protocol(aiio_transport_handle_t t, const char **alpn_protos)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.alpn_protos = alpn_protos;
}
#endif

void aiio_transport_ssl_skip_common_name_check(aiio_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.skip_common_name = true;
}

#ifdef CONFIG_TLS_USE_SECURE_ELEMENT
void aiio_transport_ssl_use_secure_element(aiio_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.use_secure_element = true;
}
#endif

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
void aiio_transport_ssl_crt_bundle_attach(aiio_transport_handle_t t, int ((*crt_bundle_attach)(void *conf)))
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.crt_bundle_attach = crt_bundle_attach;
}
#endif

static int base_get_socket(aiio_transport_handle_t t)
{
    transport_tls_t *ctx = ssl_get_context_data(t);
    if (ctx) {
        return ctx->sockfd;
    }
    return INVALID_SOCKET;
}

#ifdef CONFIG_TLS_USE_DS_PERIPHERAL
void aiio_transport_ssl_set_ds_data(aiio_transport_handle_t t, void *ds_data)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.ds_data = ds_data;
}
#endif

void aiio_transport_ssl_set_keep_alive(aiio_transport_handle_t t, aiio_transport_keep_alive_t *keep_alive_cfg)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.keep_alive_cfg = (tls_keep_alive_cfg_t *) keep_alive_cfg;
}

void aiio_transport_ssl_set_interface_name(aiio_transport_handle_t t, struct ifreq *if_name)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.if_name = if_name;
}

aiio_transport_handle_t aiio_transport_ssl_init(void)
{
    aiio_transport_handle_t t = aiio_transport_init();
    if (t == NULL) {
        return NULL;
    }
    aiio_transport_set_func(t, ssl_connect, ssl_read, ssl_write, base_close, base_poll_read, base_poll_write, base_destroy);
    aiio_transport_set_async_connect_func(t, ssl_connect_async);
    t->_get_socket = base_get_socket;
    return t;
}

struct transport_tls* aiio_transport_tls_create(void)
{
    transport_tls_t *transport_tls = calloc(1, sizeof(transport_tls_t));
    if (transport_tls == NULL) {
        return NULL;
    }
    transport_tls->sockfd = INVALID_SOCKET;
    return transport_tls;
}

void aiio_transport_tls_destroy(struct transport_tls* transport_tls)
{
    free(transport_tls);
}

aiio_transport_handle_t aiio_transport_tcp_init(void)
{
    aiio_transport_handle_t t = aiio_transport_init();
    if (t == NULL) {
        return NULL;
    }
    aiio_transport_set_func(t, tcp_connect, tcp_read, tcp_write, base_close, base_poll_read, base_poll_write, base_destroy);
    aiio_transport_set_async_connect_func(t, tcp_connect_async);
    t->_get_socket = base_get_socket;
    return t;
}

void aiio_transport_tcp_set_keep_alive(aiio_transport_handle_t t, aiio_transport_keep_alive_t *keep_alive_cfg)
{
    return aiio_transport_ssl_set_keep_alive(t, keep_alive_cfg);
}

void aiio_transport_tcp_set_interface_name(aiio_transport_handle_t t, struct ifreq *if_name)
{
    return aiio_transport_ssl_set_interface_name(t, if_name);
}
