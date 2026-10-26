#pragma once
#include <cstddef>
#include <cstring>
constexpr int ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2, ESP_OK=0;
struct esp_partition_t { size_t size=256; };
inline bool blankPartition=true;
inline const esp_partition_t *esp_partition_find_first(int,int,const char *) { static esp_partition_t part; return &part; }
inline int esp_partition_read(const esp_partition_t *,size_t,void *buffer,size_t n) { memset(buffer,blankPartition?0xff:0x42,n); return ESP_OK; }
