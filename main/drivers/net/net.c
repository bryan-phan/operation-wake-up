#include "net.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

static const char *TAG = "net";

// Small wrapper so the esp_http_client event handler can reach our callback.
typedef struct {
    net_chunk_cb_t cb;
    void          *ctx;
} wrap_t;

static esp_err_t on_event(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        wrap_t *w = (wrap_t *)evt->user_data;
        if (w && w->cb && evt->data_len > 0) {
            w->cb((const char *)evt->data, evt->data_len, w->ctx);
        }
    }
    return ESP_OK;
}

esp_err_t net_https_get(const char *url, net_chunk_cb_t cb, void *ctx)
{
    wrap_t w = { .cb = cb, .ctx = ctx };

    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,  // TLS: trust Mozilla CA roots
        .event_handler     = on_event,
        .user_data         = &w,
        .timeout_ms        = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "client init failed");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);  // DNS + TCP + TLS + HTTP
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %d for %s", status, url);
        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
