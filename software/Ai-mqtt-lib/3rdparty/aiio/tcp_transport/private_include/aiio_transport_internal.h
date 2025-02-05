
#ifndef _TRANSPORT_INTERNAL_H_
#define _TRANSPORT_INTERNAL_H_

#include "aiio_transport.h"
#include "sys/queue.h"

typedef int (*get_socket_func)(aiio_transport_handle_t t);

typedef struct aiio_foundation_transport {
    struct aiio_transport_error_storage  *error_handle;           /*!< Pointer to the transport error container */
    struct transport_tls            *transport_tls;      /*!< Pointer to the base transport which uses aiio-tls */
} aiio_foundation_transport_t;

/**
 * Transport layer structure, which will provide functions, basic properties for transport types
 */
struct aiio_transport_item_t {
    int             port;
    char            *scheme;        /*!< Tag name */
    void            *data;          /*!< Additional transport data */
    connect_func    _connect;       /*!< Connect function of this transport */
    io_read_func    _read;          /*!< Read */
    io_func         _write;         /*!< Write */
    trans_func      _close;         /*!< Close */
    poll_func       _poll_read;     /*!< Poll and read */
    poll_func       _poll_write;    /*!< Poll and write */
    trans_func      _destroy;       /*!< Destroy and free transport */
    connect_async_func _connect_async;      /*!< non-blocking connect function of this transport */
    payload_transfer_func  _parent_transfer;        /*!< Function returning underlying transport layer */
    get_socket_func        _get_socket;             /*!< Function returning the transport's socket */
    aiio_transport_keep_alive_t *keep_alive_cfg;     /*!< TCP keep-alive config */
    struct aiio_foundation_transport *base;          /*!< Foundation transport pointer available from each transport */

    STAILQ_ENTRY(aiio_transport_item_t) next;
};

/**
 * @brief Internal error types for TCP connection issues not covered in socket's errno
 */
enum tcp_transport_errors {
    ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT,
    ERR_TCP_TRANSPORT_CANNOT_RESOLVE_HOSTNAME,
    ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN,
    ERR_TCP_TRANSPORT_CONNECTION_FAILED,
    ERR_TCP_TRANSPORT_SETOPT_FAILED,
    ERR_TCP_TRANSPORT_NO_MEM,
};

/**
 * @brief      Captures internal tcp connection error
 *
 * This is internally translated to aiio-tls return codes of int type, since the aiio-tls
 * will be used as TCP transport layer
 *
 * @param[in] t The transport handle
 * @param[in] error Internal tcp-transport's error
 *
 */
void capture_tcp_transport_error(aiio_transport_handle_t t, enum tcp_transport_errors error);

/**
 * @brief Returns underlying socket for the supplied transport handle
 *
 * @param t Transport handle
 *
 * @return Socket file descriptor in case of success
 *         -1 in case of error
 */
int aiio_transport_get_socket(aiio_transport_handle_t t);

/**
 * @brief      Captures the current errno
 *
 * @param[in] t The transport handle
 * @param[in] sock_errno Socket errno to store in internal transport structures
 *
 */
void aiio_transport_capture_errno(aiio_transport_handle_t t, int sock_errno);

/**
 * @brief      Creates aiio-tls transport used in the foundation transport
 *
 * @return     transport aiio-tls handle
 */
struct transport_tls* aiio_transport_tls_create(void);

/**
 * @brief      Destroys aiio-tls transport used in the foundation transport
 *
 * @param[in]  transport aiio-tls handle
 */
void aiio_transport_tls_destroy(struct transport_tls* transport_tls);

/**
 * @brief      Sets error to common transport handle
 *
 *             Note: This function copies the supplied error handle object to tcp_transport's internal
 *             error handle object
 *
 * @param[in]  A transport handle
 *
 */
void aiio_transport_set_errors(aiio_transport_handle_t t, const aiio_tls_error_handle_t error_handle);

#endif //_TRANSPORT_INTERNAL_H_
