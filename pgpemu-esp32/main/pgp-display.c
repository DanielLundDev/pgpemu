#include "pgp-display.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define LCD_HOST SPI2_HOST
#define LCD_WIDTH 172
#define LCD_HEIGHT 320
#define LCD_X_OFFSET 34
#define LCD_STRIPE_HEIGHT 16

#define LCD_PIN_MOSI 6
#define LCD_PIN_SCLK 7
#define LCD_PIN_CS 14
#define LCD_PIN_DC 15
#define LCD_PIN_RST 21
#define LCD_PIN_BACKLIGHT 22
#define LCD_BACKLIGHT_DUTY 256 /* 25% of the 10-bit PWM period. */
#define LCD_TIMEOUT_US (15LL * 1000 * 1000)

#define LCD_CMD_CASET 0x2a
#define LCD_CMD_RASET 0x2b
#define LCD_CMD_RAMWR 0x2c

#define RGB565(red, green, blue) \
	((uint16_t)((((uint16_t)(red) & 0xf8) << 8) | \
		    (((uint16_t)(green) & 0xfc) << 3) | \
		    ((uint16_t)(blue) >> 3)))

#define COLOR_CARD RGB565(20, 39, 61)
#define COLOR_CARD_EDGE RGB565(37, 61, 85)
#define COLOR_WHITE RGB565(244, 247, 250)
#define COLOR_MUTED RGB565(140, 158, 178)
#define COLOR_RED RGB565(236, 64, 72)
#define COLOR_GREEN RGB565(58, 205, 133)
#define COLOR_BLUE RGB565(71, 145, 255)
#define COLOR_YELLOW RGB565(255, 201, 66)
#define COLOR_BLACK RGB565(9, 12, 18)

typedef enum {
	DISPLAY_EVENT_CONNECTED,
	DISPLAY_EVENT_DISCONNECTED,
	DISPLAY_EVENT_CAUGHT,
	DISPLAY_EVENT_FLED,
	DISPLAY_EVENT_SPUN,
	DISPLAY_EVENT_AUTO_MODE,
	DISPLAY_EVENT_WAKE,
} display_event_t;

typedef enum {
	TOAST_WAITING,
	TOAST_READY,
	TOAST_CAUGHT,
	TOAST_FLED,
	TOAST_SPUN,
} toast_t;

typedef struct {
	uint32_t caught;
	uint32_t spun;
	bool connected;
	bool awake;
	pgp_auto_mode_t auto_mode;
	toast_t toast;
	int ball_offset;
	int confetti_frame;
} display_state_t;

typedef struct {
	display_event_t type;
	pgp_auto_mode_t auto_mode;
} display_event_item_t;

typedef struct {
	int stripe_y;
	int stripe_height;
	uint16_t *pixels;
} canvas_t;

static const char *TAG = "GO_DISPLAY";
static QueueHandle_t s_event_queue;
static SemaphoreHandle_t s_flush_done;
static esp_lcd_panel_io_handle_t s_lcd_io;
static DMA_ATTR uint16_t s_line_buffer[LCD_WIDTH * LCD_STRIPE_HEIGHT];
static portMUX_TYPE s_backlight_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_awake_until;
static bool s_display_ready;

static bool display_should_be_awake(void)
{
	int64_t now = esp_timer_get_time();
	portENTER_CRITICAL(&s_backlight_lock);
	bool awake = now < s_awake_until;
	portEXIT_CRITICAL(&s_backlight_lock);
	return awake;
}

static esp_err_t set_backlight(bool on)
{
	if (!on) {
		return ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
	}
	ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
					LCD_BACKLIGHT_DUTY), TAG, "set backlight brightness");
	return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	uint16_t color = ((uint16_t)(red & 0xf8) << 8) |
			 ((uint16_t)(green & 0xfc) << 3) |
			 (blue >> 3);
	return (color >> 8) | (color << 8);
}

static bool lcd_transfer_done(esp_lcd_panel_io_handle_t panel_io,
			      esp_lcd_panel_io_event_data_t *event_data,
			      void *user_ctx)
{
	(void)panel_io;
	(void)event_data;
	(void)user_ctx;
	BaseType_t task_woken = pdFALSE;
	xSemaphoreGiveFromISR(s_flush_done, &task_woken);
	return task_woken == pdTRUE;
}

static esp_err_t lcd_command(uint8_t command, const uint8_t *data, size_t length)
{
	return esp_lcd_panel_io_tx_param(s_lcd_io, command, data, length);
}

static esp_err_t lcd_init(void)
{
	s_flush_done = xSemaphoreCreateBinary();
	if (s_flush_done == NULL) {
		return ESP_ERR_NO_MEM;
	}

	gpio_config_t output_config = {
		.pin_bit_mask = (1ULL << LCD_PIN_RST) | (1ULL << LCD_PIN_BACKLIGHT),
		.mode = GPIO_MODE_OUTPUT,
	};
	ESP_RETURN_ON_ERROR(gpio_config(&output_config), TAG, "configure LCD control pins");
	gpio_set_level(LCD_PIN_BACKLIGHT, 0);
	ledc_timer_config_t backlight_timer = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.timer_num = LEDC_TIMER_0,
		.freq_hz = 5000,
		.clk_cfg = LEDC_USE_XTAL_CLK,
	};
	ESP_RETURN_ON_ERROR(ledc_timer_config(&backlight_timer), TAG, "configure backlight timer");
	ledc_channel_config_t backlight_channel = {
		.gpio_num = LCD_PIN_BACKLIGHT,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_0,
		.timer_sel = LEDC_TIMER_0,
		.duty = 0,
	};
	ESP_RETURN_ON_ERROR(ledc_channel_config(&backlight_channel), TAG, "configure backlight PWM");

	spi_bus_config_t bus_config = {
		.mosi_io_num = LCD_PIN_MOSI,
		.miso_io_num = -1,
		.sclk_io_num = LCD_PIN_SCLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.data4_io_num = -1,
		.data5_io_num = -1,
		.data6_io_num = -1,
		.data7_io_num = -1,
		.max_transfer_sz = sizeof(s_line_buffer),
	};
	ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO),
			    TAG, "initialize LCD SPI bus");

	esp_lcd_panel_io_spi_config_t io_config = {
		.cs_gpio_num = LCD_PIN_CS,
		.dc_gpio_num = LCD_PIN_DC,
		.spi_mode = 0,
		.pclk_hz = 20 * 1000 * 1000,
		.trans_queue_depth = 1,
		.on_color_trans_done = lcd_transfer_done,
		.lcd_cmd_bits = 8,
		.lcd_param_bits = 8,
	};
	ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
						       &io_config, &s_lcd_io),
			    TAG, "install LCD panel IO");

	gpio_set_level(LCD_PIN_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(LCD_PIN_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(20));

	ESP_RETURN_ON_ERROR(lcd_command(0x11, NULL, 0), TAG, "wake LCD");
	vTaskDelay(pdMS_TO_TICKS(120));

	// This module is mounted with the panel's X scan reversed.
	static const uint8_t madctl[] = {0x40};
	static const uint8_t pixel_format[] = {0x55};
	static const uint8_t ram_control[] = {0x00, 0xe8};
	static const uint8_t porch[] = {0x0c, 0x0c, 0x00, 0x33, 0x33};
	static const uint8_t gate[] = {0x75};
	static const uint8_t vcom[] = {0x1a};
	static const uint8_t lcm[] = {0x80};
	static const uint8_t vdv_enable[] = {0x01, 0xff};
	static const uint8_t vrh[] = {0x13};
	static const uint8_t vdv[] = {0x20};
	static const uint8_t frame_rate[] = {0x0f};
	static const uint8_t power[] = {0xa4, 0xa1};
	static const uint8_t gamma_positive[] = {
		0xd0, 0x0d, 0x14, 0x0d, 0x0d, 0x09, 0x38,
		0x44, 0x4e, 0x3a, 0x17, 0x18, 0x2f, 0x30,
	};
	static const uint8_t gamma_negative[] = {
		0xd0, 0x09, 0x0f, 0x08, 0x07, 0x14, 0x37,
		0x44, 0x4d, 0x38, 0x15, 0x16, 0x2c, 0x2e,
	};

	ESP_RETURN_ON_ERROR(lcd_command(0x36, madctl, sizeof(madctl)), TAG, "set LCD orientation");
	ESP_RETURN_ON_ERROR(lcd_command(0x3a, pixel_format, sizeof(pixel_format)), TAG, "set LCD pixel format");
	ESP_RETURN_ON_ERROR(lcd_command(0xb0, ram_control, sizeof(ram_control)), TAG, "set LCD RAM control");
	ESP_RETURN_ON_ERROR(lcd_command(0xb2, porch, sizeof(porch)), TAG, "set LCD porch");
	ESP_RETURN_ON_ERROR(lcd_command(0xb7, gate, sizeof(gate)), TAG, "set LCD gate");
	ESP_RETURN_ON_ERROR(lcd_command(0xbb, vcom, sizeof(vcom)), TAG, "set LCD VCOM");
	ESP_RETURN_ON_ERROR(lcd_command(0xc0, lcm, sizeof(lcm)), TAG, "set LCD control");
	ESP_RETURN_ON_ERROR(lcd_command(0xc2, vdv_enable, sizeof(vdv_enable)), TAG, "enable LCD VDV");
	ESP_RETURN_ON_ERROR(lcd_command(0xc3, vrh, sizeof(vrh)), TAG, "set LCD VRH");
	ESP_RETURN_ON_ERROR(lcd_command(0xc4, vdv, sizeof(vdv)), TAG, "set LCD VDV");
	ESP_RETURN_ON_ERROR(lcd_command(0xc6, frame_rate, sizeof(frame_rate)), TAG, "set LCD frame rate");
	ESP_RETURN_ON_ERROR(lcd_command(0xd0, power, sizeof(power)), TAG, "set LCD power");
	ESP_RETURN_ON_ERROR(lcd_command(0xe0, gamma_positive, sizeof(gamma_positive)), TAG, "set LCD positive gamma");
	ESP_RETURN_ON_ERROR(lcd_command(0xe1, gamma_negative, sizeof(gamma_negative)), TAG, "set LCD negative gamma");
	ESP_RETURN_ON_ERROR(lcd_command(0x21, NULL, 0), TAG, "enable LCD inversion");
	ESP_RETURN_ON_ERROR(lcd_command(0x29, NULL, 0), TAG, "turn LCD on");

	return ESP_OK;
}

static esp_err_t lcd_flush_stripe(int y, int height)
{
	uint16_t x_start = LCD_X_OFFSET;
	uint16_t x_end = LCD_X_OFFSET + LCD_WIDTH - 1;
	uint16_t y_end = y + height - 1;
	uint8_t columns[] = {x_start >> 8, x_start & 0xff, x_end >> 8, x_end & 0xff};
	uint8_t rows[] = {y >> 8, y & 0xff, y_end >> 8, y_end & 0xff};

	ESP_RETURN_ON_ERROR(lcd_command(LCD_CMD_CASET, columns, sizeof(columns)), TAG, "set LCD columns");
	ESP_RETURN_ON_ERROR(lcd_command(LCD_CMD_RASET, rows, sizeof(rows)), TAG, "set LCD rows");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(s_lcd_io, LCD_CMD_RAMWR,
						 s_line_buffer,
						 LCD_WIDTH * height * sizeof(uint16_t)),
			    TAG, "write LCD pixels");
	if (xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}
	return ESP_OK;
}

static void canvas_pixel(canvas_t *canvas, int x, int y, uint16_t color)
{
	if (x < 0 || x >= LCD_WIDTH || y < canvas->stripe_y ||
	    y >= canvas->stripe_y + canvas->stripe_height) {
		return;
	}
	canvas->pixels[(y - canvas->stripe_y) * LCD_WIDTH + x] = color;
}

static void canvas_rect(canvas_t *canvas, int x, int y, int width, int height, uint16_t color)
{
	int top = y > canvas->stripe_y ? y : canvas->stripe_y;
	int bottom = y + height < canvas->stripe_y + canvas->stripe_height ?
		     y + height : canvas->stripe_y + canvas->stripe_height;
	int left = x > 0 ? x : 0;
	int right = x + width < LCD_WIDTH ? x + width : LCD_WIDTH;
	for (int py = top; py < bottom; py++) {
		for (int px = left; px < right; px++) {
			canvas_pixel(canvas, px, py, color);
		}
	}
}

static void canvas_round_rect(canvas_t *canvas, int x, int y, int width, int height,
			      int radius, uint16_t color)
{
	for (int py = y; py < y + height; py++) {
		if (py < canvas->stripe_y || py >= canvas->stripe_y + canvas->stripe_height) {
			continue;
		}
		for (int px = x; px < x + width; px++) {
			int corner_x = px < x + radius ? x + radius : x + width - radius - 1;
			int corner_y = py < y + radius ? y + radius : y + height - radius - 1;
			bool middle = (px >= x + radius && px < x + width - radius) ||
				      (py >= y + radius && py < y + height - radius);
			int dx = px - corner_x;
			int dy = py - corner_y;
			if (middle || dx * dx + dy * dy <= radius * radius) {
				canvas_pixel(canvas, px, py, color);
			}
		}
	}
}

static void canvas_circle(canvas_t *canvas, int center_x, int center_y, int radius, uint16_t color)
{
	for (int y = center_y - radius; y <= center_y + radius; y++) {
		if (y < canvas->stripe_y || y >= canvas->stripe_y + canvas->stripe_height) {
			continue;
		}
		for (int x = center_x - radius; x <= center_x + radius; x++) {
			int dx = x - center_x;
			int dy = y - center_y;
			if (dx * dx + dy * dy <= radius * radius) {
				canvas_pixel(canvas, x, y, color);
			}
		}
	}
}

static const uint8_t *glyph_for(char character)
{
	static const uint8_t digits[][5] = {
		{0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00},
		{0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31},
		{0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
		{0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
		{0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e},
	};
	static const uint8_t letters[][5] = {
		{0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
		{0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
		{0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
		{0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
		{0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01},
		{0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
		{0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
		{0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
		{0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
		{0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
		{0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
		{0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63},
		{0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
	};
	static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
	static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
	static const uint8_t bang[5] = {0x00, 0x00, 0x5f, 0x00, 0x00};
	static const uint8_t percent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};

	if (character >= '0' && character <= '9') return digits[character - '0'];
	if (character >= 'A' && character <= 'Z') return letters[character - 'A'];
	if (character == ':') return colon;
	if (character == '-') return dash;
	if (character == '!') return bang;
	if (character == '%') return percent;
	return NULL;
}

static int text_width(const char *text, int scale)
{
	size_t length = strlen(text);
	return length == 0 ? 0 : (int)(length * 6 - 1) * scale;
}

static void canvas_text(canvas_t *canvas, int x, int y, const char *text,
			int scale, uint16_t color)
{
	for (const char *cursor = text; *cursor; cursor++, x += 6 * scale) {
		const uint8_t *glyph = glyph_for(*cursor);
		if (glyph == NULL) {
			continue;
		}
		for (int column = 0; column < 5; column++) {
			for (int row = 0; row < 7; row++) {
				if (glyph[column] & (1 << row)) {
					canvas_rect(canvas, x + column * scale, y + row * scale,
						    scale, scale, color);
				}
			}
		}
	}
}

static void canvas_centered_text(canvas_t *canvas, int y, const char *text,
				 int scale, uint16_t color)
{
	canvas_text(canvas, (LCD_WIDTH - text_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_ball(canvas_t *canvas, int center_y)
{
	const int center_x = LCD_WIDTH / 2;
	const int radius = 25;
	canvas_circle(canvas, center_x, center_y, radius + 2, COLOR_BLACK);
	canvas_circle(canvas, center_x, center_y, radius, COLOR_WHITE);
	for (int y = center_y - radius; y < center_y; y++) {
		for (int x = center_x - radius; x <= center_x + radius; x++) {
			int dx = x - center_x;
			int dy = y - center_y;
			if (dx * dx + dy * dy <= radius * radius) {
				canvas_pixel(canvas, x, y, COLOR_RED);
			}
		}
	}
	canvas_rect(canvas, center_x - radius, center_y - 3, radius * 2, 6, COLOR_BLACK);
	canvas_circle(canvas, center_x, center_y, 9, COLOR_BLACK);
	canvas_circle(canvas, center_x, center_y, 5, COLOR_WHITE);
}

static void draw_confetti(canvas_t *canvas, int frame)
{
	static const int points[][2] = {
		{22, 66}, {39, 91}, {145, 68}, {130, 101}, {18, 112}, {153, 119},
		{53, 61}, {119, 59}, {32, 123}, {139, 126},
	};
	static const uint16_t colors[] = {COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_RED};
	for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
		int y = points[i][1] + ((frame + (int)i) % 3) * 2;
		canvas_rect(canvas, points[i][0], y, 3, 5, colors[i % 4]);
	}
}

static void draw_card(canvas_t *canvas, int y, const char *label,
		      uint32_t value, uint16_t accent)
{
	canvas_round_rect(canvas, 8, y, 156, 70, 10, accent);
	canvas_round_rect(canvas, 10, y + 2, 152, 66, 8, COLOR_CARD);
	canvas_centered_text(canvas, y + 8, label, 2, accent);
	char value_text[16];
	snprintf(value_text, sizeof(value_text), "%" PRIu32, value);
	canvas_centered_text(canvas, y + 32, value_text, 4, COLOR_WHITE);
}

static void render_dashboard(const display_state_t *state)
{
	if (!state->awake) {
		return;
	}
	for (int stripe_y = 0; stripe_y < LCD_HEIGHT; stripe_y += LCD_STRIPE_HEIGHT) {
		int stripe_height = LCD_HEIGHT - stripe_y;
		if (stripe_height > LCD_STRIPE_HEIGHT) {
			stripe_height = LCD_STRIPE_HEIGHT;
		}
		canvas_t canvas = {
			.stripe_y = stripe_y,
			.stripe_height = stripe_height,
			.pixels = s_line_buffer,
		};

		for (int y = stripe_y; y < stripe_y + stripe_height; y++) {
			uint8_t mix = y * 255 / LCD_HEIGHT;
			uint8_t red = 5 + (7 * mix / 255);
			uint8_t green = 15 + (14 * mix / 255);
			uint8_t blue = 29 + (19 * mix / 255);
			uint16_t color = rgb565(red, green, blue);
			for (int x = 0; x < LCD_WIDTH; x++) {
				s_line_buffer[(y - stripe_y) * LCD_WIDTH + x] = color;
			}
		}

		canvas_circle(&canvas, 21, 19, 12, COLOR_BLACK);
		canvas_circle(&canvas, 21, 19, 10, COLOR_WHITE);
		canvas_rect(&canvas, 11, 17, 20, 4, COLOR_BLACK);
		canvas_circle(&canvas, 21, 19, 4, COLOR_RED);
		canvas_text(&canvas, 40, 11, "GO BUDDY", 2, COLOR_WHITE);

		draw_ball(&canvas, 69 + state->ball_offset);
		if (state->confetti_frame >= 0) {
			draw_confetti(&canvas, state->confetti_frame);
		}

		const char *status_text = "CONNECTED";
		const char *detail_text = pgp_auto_mode_label(state->auto_mode);
		uint16_t status_color = COLOR_GREEN;
		switch (state->toast) {
		case TOAST_WAITING:
			status_text = "PAIR ME UP";
			status_color = COLOR_WHITE;
			break;
		case TOAST_CAUGHT:
			status_text = "NICE CATCH!";
			status_color = COLOR_GREEN;
			break;
		case TOAST_FLED:
			status_text = "SO CLOSE";
			status_color = COLOR_RED;
			break;
		case TOAST_SPUN:
			status_text = "STOP SPUN!";
			status_color = COLOR_BLUE;
			break;
		case TOAST_READY:
		default:
			break;
		}
		canvas_round_rect(&canvas, 8, 103, 156, 43, 10, COLOR_CARD_EDGE);
		canvas_round_rect(&canvas, 10, 105, 152, 39, 8, COLOR_CARD);
		canvas_centered_text(&canvas, 109, status_text, 2, status_color);
		canvas_centered_text(&canvas, 132, detail_text, 1, COLOR_MUTED);

		draw_card(&canvas, 153, "CAUGHT", state->caught, COLOR_GREEN);
		draw_card(&canvas, 230, "STOPS SPUN", state->spun, COLOR_BLUE);

		uint64_t total = (uint64_t)state->caught + state->spun;
		char total_text[24];
		snprintf(total_text, sizeof(total_text), "TOTAL %" PRIu64, total);
		canvas_centered_text(&canvas, 308, total_text, 1, COLOR_MUTED);

		esp_err_t error = lcd_flush_stripe(stripe_y, stripe_height);
		if (error != ESP_OK) {
			ESP_LOGE(TAG, "LCD refresh failed: %s", esp_err_to_name(error));
			return;
		}
	}
}

static void load_stats(nvs_handle_t handle, display_state_t *state)
{
	if (nvs_get_u32(handle, "caught", &state->caught) != ESP_OK) {
		state->caught = 0;
	}
	if (nvs_get_u32(handle, "spun", &state->spun) != ESP_OK) {
		state->spun = 0;
	}
}

static bool save_stats(nvs_handle_t handle, const display_state_t *state)
{
	esp_err_t error = nvs_set_u32(handle, "caught", state->caught);
	if (error == ESP_OK) error = nvs_set_u32(handle, "spun", state->spun);
	if (error == ESP_OK) error = nvs_commit(handle);
	if (error != ESP_OK) {
		ESP_LOGW(TAG, "could not save counters: %s", esp_err_to_name(error));
	}
	return error == ESP_OK;
}

static void display_task(void *context)
{
	(void)context;
	display_state_t state = {
		.auto_mode = PGP_AUTO_MODE_BOTH,
		.awake = true,
		.toast = TOAST_WAITING,
		.confetti_frame = -1,
	};
	nvs_handle_t stats_handle = 0;
	bool nvs_ready = nvs_open("go_stats", NVS_READWRITE, &stats_handle) == ESP_OK;
	if (nvs_ready) {
		load_stats(stats_handle, &state);
	}

	esp_err_t error = lcd_init();
	if (error == ESP_OK) {
		render_dashboard(&state);
		error = set_backlight(true);
	}
	if (error != ESP_OK) {
		ESP_LOGE(TAG, "display initialization failed: %s", esp_err_to_name(error));
		if (nvs_ready) nvs_close(stats_handle);
		vTaskDelete(NULL);
		return;
	}

	int64_t now = esp_timer_get_time();
	portENTER_CRITICAL(&s_backlight_lock);
	s_awake_until = now + LCD_TIMEOUT_US;
	s_display_ready = true;
	portEXIT_CRITICAL(&s_backlight_lock);
	ESP_LOGI(TAG, "dashboard ready (%" PRIu32 " caught, %" PRIu32 " stops spun)",
		 state.caught, state.spun);

	bool dirty = false;
	int64_t last_save = esp_timer_get_time();
	for (;;) {
		display_event_item_t event;
		bool received = xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE;
		bool awake = display_should_be_awake();
		if (awake != state.awake) {
			state.awake = awake;
			if (awake) {
				render_dashboard(&state);
			}
			error = set_backlight(awake);
			if (error == ESP_OK) {
				ESP_LOGI(TAG, "backlight %s", awake ? "on at 25%" : "off after inactivity");
			} else {
				state.awake = !awake; /* Retry on the next iteration. */
				ESP_LOGW(TAG, "backlight update failed: %s", esp_err_to_name(error));
			}
		}
		if (received) {
			switch (event.type) {
			case DISPLAY_EVENT_CONNECTED:
				state.connected = true;
				state.toast = TOAST_READY;
				state.confetti_frame = -1;
				render_dashboard(&state);
				break;
			case DISPLAY_EVENT_DISCONNECTED:
				state.connected = false;
				state.toast = TOAST_WAITING;
				state.confetti_frame = -1;
				render_dashboard(&state);
				if (dirty && nvs_ready) {
					dirty = !save_stats(stats_handle, &state);
					last_save = esp_timer_get_time();
				}
				break;
			case DISPLAY_EVENT_SPUN: {
				state.spun++;
				state.toast = TOAST_SPUN;
				state.confetti_frame = -1;
				static const int hop[] = {0, -7, -11, -5, 0};
				for (size_t i = 0; state.awake && i < sizeof(hop) / sizeof(hop[0]); i++) {
					state.ball_offset = hop[i];
					render_dashboard(&state);
					vTaskDelay(pdMS_TO_TICKS(45));
				}
				dirty = true;
				break;
			}
			case DISPLAY_EVENT_CAUGHT:
				state.caught++;
				state.toast = TOAST_CAUGHT;
				state.ball_offset = 0;
				for (int frame = 0; state.awake && frame < 3; frame++) {
					state.confetti_frame = frame;
					render_dashboard(&state);
					vTaskDelay(pdMS_TO_TICKS(80));
				}
				dirty = true;
				break;
			case DISPLAY_EVENT_FLED:
				state.toast = TOAST_FLED;
				state.ball_offset = 0;
				state.confetti_frame = -1;
				render_dashboard(&state);
				break;
			case DISPLAY_EVENT_AUTO_MODE:
				state.auto_mode = event.auto_mode;
				render_dashboard(&state);
				break;
			case DISPLAY_EVENT_WAKE:
				/* The shared deadline also wakes the display if its queue was full. */
				break;
			}
		}

		int64_t now = esp_timer_get_time();
		if (dirty && nvs_ready && now - last_save >= 60LL * 1000 * 1000) {
			dirty = !save_stats(stats_handle, &state);
			last_save = now;
		}
	}
}

static void queue_event(display_event_t event)
{
	display_event_item_t item = {
		.type = event,
	};
	if (s_event_queue != NULL && xQueueSend(s_event_queue, &item, 0) != pdTRUE) {
		ESP_LOGW(TAG, "display event queue full");
	}
}

bool pgp_display_init(void)
{
	s_event_queue = xQueueCreate(8, sizeof(display_event_item_t));
	if (s_event_queue == NULL) {
		return false;
	}
	if (xTaskCreate(display_task, "go_display", 4096, NULL, 7, NULL) != pdPASS) {
		vQueueDelete(s_event_queue);
		s_event_queue = NULL;
		return false;
	}
	return true;
}

bool pgp_display_wake(void)
{
	int64_t now = esp_timer_get_time();
	portENTER_CRITICAL(&s_backlight_lock);
	if (!s_display_ready) {
		portEXIT_CRITICAL(&s_backlight_lock);
		return false;
	}
	bool was_asleep = now >= s_awake_until;
	s_awake_until = now + LCD_TIMEOUT_US;
	portEXIT_CRITICAL(&s_backlight_lock);
	queue_event(DISPLAY_EVENT_WAKE);
	return was_asleep;
}

void pgp_display_set_connected(bool connected)
{
	queue_event(connected ? DISPLAY_EVENT_CONNECTED : DISPLAY_EVENT_DISCONNECTED);
}

void pgp_display_set_auto_mode(pgp_auto_mode_t mode)
{
	display_event_item_t item = {
		.type = DISPLAY_EVENT_AUTO_MODE,
		.auto_mode = mode,
	};
	if (s_event_queue != NULL && xQueueSend(s_event_queue, &item, 0) != pdTRUE) {
		ESP_LOGW(TAG, "display event queue full");
	}
}

void pgp_display_pokemon_caught(void)
{
	queue_event(DISPLAY_EVENT_CAUGHT);
}

void pgp_display_pokemon_fled(void)
{
	queue_event(DISPLAY_EVENT_FLED);
}

void pgp_display_pokestop_spun(void)
{
	queue_event(DISPLAY_EVENT_SPUN);
}
