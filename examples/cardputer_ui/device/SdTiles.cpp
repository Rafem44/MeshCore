#include "SdTiles.h"

#if CPADV_MAP
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <target.h>

namespace {
SemaphoreHandle_t g_bus = nullptr;   // SPI bus lock (radio / SD)
File g_tile;

struct BusLock {
  BusLock() { if (g_bus) xSemaphoreTakeRecursive(g_bus, portMAX_DELAY); }
  ~BusLock() { if (g_bus) xSemaphoreGiveRecursive(g_bus); }
};
}  // namespace

bool SdTiles::begin() {
  if (!g_bus) g_bus = xSemaphoreCreateRecursiveMutex();
  BusLock lock;
  // 20 MHz: safe on the shared bus with short Cap wiring; SD CS is GPIO 12
  if (!SD.begin(PIN_SD_CS, radio_spi(), 20000000)) {
    Serial.println("[CPADV] SD: no card (map disabled, everything else works)");
    return false;
  }
  Serial.printf("[CPADV] SD: card %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
  File f = SD.open("/tiles/info.txt", FILE_READ);
  if (!f) {
    Serial.println("[CPADV] SD: no /tiles/info.txt (copy the tiles folder made by tools/tiles/make_tiles.py)");
    return false;
  }
  char buf[64] = {};
  f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  _ok = sscanf(buf, "%lf %lf %d %d", &_lat, &_lon, &_zmin, &_zmax) == 4;
  Serial.printf("[CPADV] SD: map tiles zoom %d-%d around %.4f, %.4f (%s)\n", _zmin, _zmax, _lat, _lon,
                _ok ? "ok" : "info.txt invalid");
  return _ok;
}

bool SdTiles::beginTile(int z, int x, int y) {
  if (!_ok) return false;
  char path[40];
  snprintf(path, sizeof(path), "/tiles/%d/%d/%d.bin", z, x, y);
  BusLock lock;
  g_tile = SD.open(path, FILE_READ);
  return (bool)g_tile;
}

bool SdTiles::readRow(int row, int x0, int w, uint16_t* dst) {
  BusLock lock;
  if (!g_tile || !g_tile.seek((row * 256 + x0) * 2)) return false;
  return g_tile.read((uint8_t*)dst, w * 2) == (size_t)(w * 2);
}

void SdTiles::endTile() {
  BusLock lock;
  if (g_tile) g_tile.close();
}
#endif
