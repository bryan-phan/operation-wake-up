#ifndef NET_H
#define NET_H

#include <stddef.h>
#include "esp_err.h"

// Called for each chunk of the HTTP response body as it streams in.
typedef void (*net_chunk_cb_t)(const char *data, size_t len, void *ctx);

// HTTPS GET `url`, streaming the response body to `cb` chunk by chunk.
// Validates the server certificate against the bundled Mozilla CA roots.
// Returns ESP_OK on a 2xx response, an error otherwise.
esp_err_t net_https_get(const char *url, net_chunk_cb_t cb, void *ctx);

#endif
