#include "pgp-mood-light.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MOOD_LED_GPIO GPIO_NUM_8
#define MOOD_RMT_RESOLUTION_HZ 10000000
#define MOOD_FLASH_LEVEL 64
#define MOOD_FLASH_ON_MS 160
#define MOOD_FLASH_OFF_MS 140
#define MOOD_FLASH_COUNT 3

typedef enum {
	MOOD_EVENT_POKEMON_CAUGHT,
	MOOD_EVENT_POKESTOP_SPUN,
} mood_event_t;

static const char *TAG = "GO_MOOD";
static QueueHandle_t s_event_queue;
static rmt_channel_handle_t s_tx_channel;
static rmt_encoder_handle_t s_bytes_encoder;

static esp_err_t set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
	/* WS2812B pixels expect their color bytes in GRB order. */
	uint8_t pixel[] = {green, red, blue};
	rmt_transmit_config_t transmit_config = {
		.loop_count = 0,
	};

	/* The channel holds a CPU power lock while enabled, even with a dark LED. */
	esp_err_t error = rmt_enable(s_tx_channel);
	if (error != ESP_OK) {
		return error;
	}
	error = rmt_transmit(s_tx_channel, s_bytes_encoder, pixel,
					 sizeof(pixel), &transmit_config);
	if (error == ESP_OK) {
		error = rmt_tx_wait_all_done(s_tx_channel, 100);
	}
	/* Also stop the channel after a failed or timed-out transmission. */
	esp_err_t disable_error = rmt_disable(s_tx_channel);
	return error != ESP_OK ? error : disable_error;
}

static void mood_light_task(void *context)
{
	(void)context;
	for (;;) {
		mood_event_t event;
		if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) != pdTRUE) {
			continue;
		}

		uint8_t red = event == MOOD_EVENT_POKESTOP_SPUN ? MOOD_FLASH_LEVEL : 0;
		uint8_t green = event == MOOD_EVENT_POKEMON_CAUGHT ? MOOD_FLASH_LEVEL : 0;
		ESP_LOGI(TAG, "%s: flashing %s three times",
			 event == MOOD_EVENT_POKEMON_CAUGHT ? "Pokemon caught" : "Pokestop spun",
			 green ? "green" : "red");

		for (int flash = 0; flash < MOOD_FLASH_COUNT; flash++) {
			if (set_rgb(red, green, 0) != ESP_OK) {
				ESP_LOGW(TAG, "could not turn mood light on");
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(MOOD_FLASH_ON_MS));
			if (set_rgb(0, 0, 0) != ESP_OK) {
				ESP_LOGW(TAG, "could not turn mood light off");
				break;
			}
			if (flash + 1 < MOOD_FLASH_COUNT) {
				vTaskDelay(pdMS_TO_TICKS(MOOD_FLASH_OFF_MS));
			}
		}
		set_rgb(0, 0, 0);
	}
}

static void release_resources(void)
{
	if (s_bytes_encoder != NULL) {
		rmt_del_encoder(s_bytes_encoder);
		s_bytes_encoder = NULL;
	}
	if (s_tx_channel != NULL) {
		rmt_del_channel(s_tx_channel);
		s_tx_channel = NULL;
	}
	if (s_event_queue != NULL) {
		vQueueDelete(s_event_queue);
		s_event_queue = NULL;
	}
}

bool pgp_mood_light_init(void)
{
	s_event_queue = xQueueCreate(4, sizeof(mood_event_t));
	if (s_event_queue == NULL) {
		return false;
	}

	rmt_tx_channel_config_t channel_config = {
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.gpio_num = MOOD_LED_GPIO,
		.mem_block_symbols = 64,
		.resolution_hz = MOOD_RMT_RESOLUTION_HZ,
		.trans_queue_depth = 4,
	};
	esp_err_t error = rmt_new_tx_channel(&channel_config, &s_tx_channel);
	if (error != ESP_OK) {
		ESP_LOGE(TAG, "could not create RMT channel: %s", esp_err_to_name(error));
		release_resources();
		return false;
	}

	rmt_bytes_encoder_config_t encoder_config = {
		.bit0 = {
			.level0 = 1,
			.duration0 = 3,
			.level1 = 0,
			.duration1 = 9,
		},
		.bit1 = {
			.level0 = 1,
			.duration0 = 9,
			.level1 = 0,
			.duration1 = 3,
		},
		.flags.msb_first = 1,
	};
	error = rmt_new_bytes_encoder(&encoder_config, &s_bytes_encoder);
	if (error == ESP_OK) error = set_rgb(0, 0, 0);
	if (error != ESP_OK) {
		ESP_LOGE(TAG, "could not initialize mood light: %s", esp_err_to_name(error));
		release_resources();
		return false;
	}

	if (xTaskCreate(mood_light_task, "go_mood", 3072, NULL, 6, NULL) != pdPASS) {
		release_resources();
		return false;
	}
	ESP_LOGI(TAG, "mood light ready and idle off");
	return true;
}

static void queue_event(mood_event_t event)
{
	if (s_event_queue != NULL && xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
		ESP_LOGW(TAG, "mood light event queue full");
	}
}

void pgp_mood_light_pokemon_caught(void)
{
	queue_event(MOOD_EVENT_POKEMON_CAUGHT);
}

void pgp_mood_light_pokestop_spun(void)
{
	queue_event(MOOD_EVENT_POKESTOP_SPUN);
}
