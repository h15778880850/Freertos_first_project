#include "bsp_oled.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

#define OLED_ADDRESS       (0x3CU << 1)
#define OLED_WIDTH         128U
#define OLED_HEIGHT        64U
#define OLED_PAGES         (OLED_HEIGHT / 8U)
#define OLED_TIMEOUT_MS    100U
#define OLED_TEXT_COLUMNS  21U

extern I2C_HandleTypeDef hi2c1;

static uint8_t s_oled_buffer[OLED_WIDTH * OLED_PAGES];
static bool s_oled_ready;

static const uint8_t *font5x7(char ch)
{
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t zero[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t one[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t two[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t three[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t four[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t five[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t six[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t seven[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t eight[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t nine[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t a[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t b[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t c[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t d[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t e[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t f[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t g[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
  static const uint8_t h[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static const uint8_t i[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t k[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
  static const uint8_t l[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t m[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static const uint8_t n[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
  static const uint8_t o[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t p[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t r[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t s[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t t[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t u[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t v[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
  static const uint8_t w[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
  static const uint8_t y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};

  switch (ch)
  {
    case ' ': return blank;
    case ':': return colon;
    case '.': return dot;
    case '/': return slash;
    case '-': return dash;
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case 'A': return a;
    case 'B': return b;
    case 'C': return c;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'G': return g;
    case 'H': return h;
    case 'I': return i;
    case 'K': return k;
    case 'L': return l;
    case 'M': return m;
    case 'N': return n;
    case 'O': return o;
    case 'P': return p;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'W': return w;
    case 'Y': return y;
    default: return blank;
  }
}

static bool oled_cmd(uint8_t cmd)
{
  uint8_t payload[2] = {0x00U, cmd};
  return HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, payload, sizeof(payload), OLED_TIMEOUT_MS) == HAL_OK;
}

static bool oled_flush(void)
{
  uint8_t payload[1U + OLED_WIDTH];

  for (uint8_t page = 0; page < OLED_PAGES; page++)
  {
    if (!oled_cmd((uint8_t)(0xB0U + page)) || !oled_cmd(0x00U) || !oled_cmd(0x10U))
    {
      return false;
    }

    payload[0] = 0x40U;
    memcpy(&payload[1], &s_oled_buffer[page * OLED_WIDTH], OLED_WIDTH);

    if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, payload, sizeof(payload), OLED_TIMEOUT_MS) != HAL_OK)
    {
      return false;
    }
  }

  return true;
}

static void oled_clear(void)
{
  memset(s_oled_buffer, 0, sizeof(s_oled_buffer));
}

static void oled_puts(uint8_t row, uint8_t column, const char *text)
{
  uint16_t offset;

  if ((row >= OLED_PAGES) || (column >= OLED_TEXT_COLUMNS) || (text == 0))
  {
    return;
  }

  offset = (uint16_t)row * OLED_WIDTH + (uint16_t)column * 6U;

  while ((*text != '\0') && (offset + 5U < sizeof(s_oled_buffer)) &&
         (((offset % OLED_WIDTH) + 5U) < OLED_WIDTH))
  {
    const uint8_t *glyph = font5x7(*text);
    memcpy(&s_oled_buffer[offset], glyph, 5U);
    offset += 6U;
    text++;
  }
}

bool BSP_Oled_Init(void)
{
  HAL_Delay(50U);

  s_oled_ready = oled_cmd(0xAEU) &&
                 oled_cmd(0x20U) && oled_cmd(0x00U) &&
                 oled_cmd(0xB0U) &&
                 oled_cmd(0xC8U) &&
                 oled_cmd(0x00U) &&
                 oled_cmd(0x10U) &&
                 oled_cmd(0x40U) &&
                 oled_cmd(0x81U) && oled_cmd(0x7FU) &&
                 oled_cmd(0xA1U) &&
                 oled_cmd(0xA6U) &&
                 oled_cmd(0xA8U) && oled_cmd(0x3FU) &&
                 oled_cmd(0xA4U) &&
                 oled_cmd(0xD3U) && oled_cmd(0x00U) &&
                 oled_cmd(0xD5U) && oled_cmd(0x80U) &&
                 oled_cmd(0xD9U) && oled_cmd(0xF1U) &&
                 oled_cmd(0xDAU) && oled_cmd(0x12U) &&
                 oled_cmd(0xDBU) && oled_cmd(0x40U) &&
                 oled_cmd(0x8DU) && oled_cmd(0x14U) &&
                 oled_cmd(0xAFU);

  if (s_oled_ready)
  {
    oled_clear();
    s_oled_ready = oled_flush();
  }

  return s_oled_ready;
}

void BSP_Oled_ShowBoot(const AppSelfTest *self_test)
{
  if (!s_oled_ready || (self_test == 0))
  {
    return;
  }

  oled_clear();
  oled_puts(0, 0, "SELFTEST");
  oled_puts(2, 0, self_test->flash_ok ? "FLASH OK" : "FLASH ERR");
  oled_puts(3, 0, self_test->config_restored ? "CFG RESTORED" : "CFG DEFAULT");
  oled_puts(4, 0, self_test->adc_ok ? "ADC OK" : "ADC ERR");
  oled_flush();
}

void BSP_Oled_ShowSample(const AppSample *sample)
{
  char line[22];
  int16_t temperature;
  uint16_t magnitude;

  if (!s_oled_ready || (sample == 0))
  {
    return;
  }

  oled_clear();
  if (sample->source == APP_SAMPLE_SOURCE_DS18B20)
  {
    temperature = sample->temperature_centi;
    magnitude = (uint16_t)(temperature < 0 ? -temperature : temperature);
    snprintf(line, sizeof(line), "T:%s%u.%02uC",
             temperature < 0 ? "-" : "",
             magnitude / 100U,
             magnitude % 100U);
    oled_puts(0, 0, line);
    snprintf(line, sizeof(line), "TH:%u.%02uC", sample->threshold / 100U, sample->threshold % 100U);
    oled_puts(2, 0, line);
  }
  else
  {
    snprintf(line, sizeof(line), "ADC:%04u", sample->raw);
    oled_puts(0, 0, line);
    snprintf(line, sizeof(line), "THR:%04u", sample->threshold);
    oled_puts(2, 0, line);
  }
  oled_puts(4, 0, sample->alarm_active ? "ALARM" : "NORMAL");
  if (sample->k1_triggered)
  {
    oled_puts(6, 0, "K1");
  }
  oled_flush();
}
