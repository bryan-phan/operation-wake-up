#include "esp_err.h"
#include "sd-card.h"

void app_main(void)
{
    ESP_ERROR_CHECK(sdc_init());
}
