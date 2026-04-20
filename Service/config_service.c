#include "config_service.h"

#include <string.h>

#include "bsp_w25q64.h"

#define CONFIG_MAGIC       0x31474643UL
#define CONFIG_VERSION     1U
#define CONFIG_FLASH_ADDR  0x000000UL
#define CONFIG_MIN_PERIOD  100U
#define CONFIG_MAX_PERIOD  60000U
#define CONFIG_MAX_ADC     4095U

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  AppConfig config;
  uint32_t checksum;
} StoredConfig;

static uint32_t config_checksum(const StoredConfig *record)
{
  uint32_t hash = 2166136261UL;
  const uint8_t *bytes = (const uint8_t *)record;
  size_t length = sizeof(*record) - sizeof(record->checksum);

  for (size_t i = 0; i < length; i++)
  {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }

  return hash;
}

static bool config_is_valid(const AppConfig *config)
{
  return (config->sample_period_ms >= CONFIG_MIN_PERIOD) &&
         (config->sample_period_ms <= CONFIG_MAX_PERIOD) &&
         (config->alarm_threshold <= CONFIG_MAX_ADC) &&
         (config->alarm_hysteresis <= config->alarm_threshold);
}

void ConfigService_Default(AppConfig *config)
{
  if (config == 0)
  {
    return;
  }

  config->sample_period_ms = 1000U;
  config->alarm_threshold = 3000U;
  config->alarm_hysteresis = 100U;
}

bool ConfigService_Load(AppConfig *config)
{
  StoredConfig record;

  if (config == 0)
  {
    return false;
  }

  if (!BSP_W25Q64_Read(CONFIG_FLASH_ADDR, (uint8_t *)&record, sizeof(record)))
  {
    ConfigService_Default(config);
    return false;
  }

  if ((record.magic != CONFIG_MAGIC) ||
      (record.version != CONFIG_VERSION) ||
      (record.checksum != config_checksum(&record)) ||
      !config_is_valid(&record.config))
  {
    ConfigService_Default(config);
    return false;
  }

  memcpy(config, &record.config, sizeof(*config));
  return true;
}

bool ConfigService_Save(const AppConfig *config)
{
  StoredConfig record;

  if (config == 0)
  {
    return false;
  }

  memset(&record, 0, sizeof(record));
  record.magic = CONFIG_MAGIC;
  record.version = CONFIG_VERSION;
  memcpy(&record.config, config, sizeof(record.config));
  record.checksum = config_checksum(&record);

  return BSP_W25Q64_WriteSector(CONFIG_FLASH_ADDR, (const uint8_t *)&record, sizeof(record));
}
