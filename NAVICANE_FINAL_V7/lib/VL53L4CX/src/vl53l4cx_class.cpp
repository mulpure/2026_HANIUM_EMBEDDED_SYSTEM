
// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/******************************************************************************
 * Copyright (c) 2020, STMicroelectronics - All Rights Reserved

 This file is part of VL53L4CX and is dual licensed,
 either GPL-2.0+
 or 'BSD 3-clause "New" or "Revised" License' , at your option.
 ******************************************************************************
 */

/* Includes */
#include "vl53l4cx_class.h"

#ifndef DEFAULT_I2C_BUFFER_LEN
  #ifdef BUFFER_LENGTH
    #define DEFAULT_I2C_BUFFER_LEN  BUFFER_LENGTH
  #else
    #define DEFAULT_I2C_BUFFER_LEN  32
  #endif
#endif

/* Write and read functions from I2C */

VL53L4CX_Error VL53L4CX::VL53L4CX_WriteMulti(VL53L4CX_DEV Dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
  int  status;

  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, pdata, (uint16_t)count);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_ReadMulti(VL53L4CX_DEV Dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
  int status;

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, pdata, (uint16_t)count);

  return status;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WrByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t data)
{
  int  status;

  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, &data, 1);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_WrWord(VL53L4CX_DEV Dev, uint16_t index, uint16_t data)
{
  int  status;
  uint8_t buffer[2];

  buffer[0] = data >> 8;
  buffer[1] = data & 0x00FF;
  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, (uint8_t *)buffer, 2);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_WrDWord(VL53L4CX_DEV Dev, uint16_t index, uint32_t data)
{
  int  status;
  uint8_t buffer[4];

  buffer[0] = (data >> 24) & 0xFF;
  buffer[1] = (data >> 16) & 0xFF;
  buffer[2] = (data >>  8) & 0xFF;
  buffer[3] = (data >>  0) & 0xFF;
  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, (uint8_t *)buffer, 4);
  return status;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_RdByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t *data)
{
  int  status;

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, data, 1);

  if (status) {
    return -1;
  }

  return 0;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_RdWord(VL53L4CX_DEV Dev, uint16_t index, uint16_t *data)
{
  int  status;
  uint8_t buffer[2] = {0, 0};

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, buffer, 2);
  if (!status) {
    *data = (buffer[0] << 8) + buffer[1];
  }
  return status;

}

VL53L4CX_Error VL53L4CX::VL53L4CX_RdDWord(VL53L4CX_DEV Dev, uint16_t index, uint32_t *data)
{
  int status;
  uint8_t buffer[4] = {0, 0, 0, 0};

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, buffer, 4);
  if (!status) {
    *data = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) + ((uint32_t)buffer[2] << 8) + (uint32_t)buffer[3];
  }
  return status;

}

VL53L4CX_Error VL53L4CX::VL53L4CX_UpdateByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t AndData, uint8_t OrData)
{
  int  status;
  uint8_t buffer = 0;

  /* read data direct onto buffer */
  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, &buffer, 1);
  if (!status) {
    buffer = (buffer & AndData) | OrData;
    status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, &buffer, (uint16_t)1);
  }
  return status;
}

// NAVCANE patch: every failed transaction returns; no unbounded retries.
extern "C" bool navSegmentLock();
extern "C" void navSegmentUnlock();
class NavBusGuard {
 public:
  bool ok = navSegmentLock();
  ~NavBusGuard() { if (ok) navSegmentUnlock(); }
};

VL53L4CX_Error VL53L4CX::VL53L4CX_I2CWrite(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToWrite)
{
  uint16_t offset = 0;
  while (offset < NumByteToWrite) {
    uint16_t remaining = NumByteToWrite - offset;
    uint16_t count = remaining > 28 ? 28 : remaining;
    uint16_t reg = RegisterAddr + offset;
    NavBusGuard busGuard;
    if (!busGuard.ok) return VL53L4CX_ERROR_CONTROL_INTERFACE;
    dev_i2c->beginTransmission((DeviceAddr >> 1) & 0x7F);
    if (dev_i2c->write(uint8_t(reg >> 8)) != 1 ||
        dev_i2c->write(uint8_t(reg)) != 1 ||
        dev_i2c->write(pBuffer + offset, count) != count) {
      dev_i2c->endTransmission(true);
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    }
    if (dev_i2c->endTransmission(true) != 0)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    offset += count;
  }
  return VL53L4CX_ERROR_NONE;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_I2CRead(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToRead)
{
  uint16_t offset = 0;
  while (offset < NumByteToRead) {
    uint16_t remaining = NumByteToRead - offset;
    uint8_t count = remaining > 28 ? 28 : remaining;
    uint16_t reg = RegisterAddr + offset;
    NavBusGuard busGuard;
    if (!busGuard.ok) return VL53L4CX_ERROR_CONTROL_INTERFACE;
    dev_i2c->beginTransmission((DeviceAddr >> 1) & 0x7F);
    if (dev_i2c->write(uint8_t(reg >> 8)) != 1 ||
        dev_i2c->write(uint8_t(reg)) != 1) {
      dev_i2c->endTransmission(true);
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    }
    if (dev_i2c->endTransmission(false) != 0)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    if (dev_i2c->requestFrom(uint8_t((DeviceAddr >> 1) & 0x7F), count) != count)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    for (uint8_t n = 0; n < count; ++n) {
      if (!dev_i2c->available()) return VL53L4CX_ERROR_CONTROL_INTERFACE;
      pBuffer[offset + n] = dev_i2c->read();
    }
    offset += count;
  }
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_GetTickCount(
  uint32_t *ptick_count_ms)
{

  /* Returns current tick count in [ms] */

  VL53L4CX_Error status  = VL53L4CX_ERROR_NONE;

  *ptick_count_ms = (uint32_t)millis();

  return status;
}



VL53L4CX_Error VL53L4CX::VL53L4CX_WaitUs(VL53L4CX_Dev_t *pdev, int32_t wait_us)
{
  (void)pdev;
  delayMicroseconds(wait_us);
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WaitMs(VL53L4CX_Dev_t *pdev, int32_t wait_ms)
{
  (void)pdev;
  delay(wait_ms);
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WaitValueMaskEx(
  VL53L4CX_Dev_t *pdev,
  uint32_t      timeout_ms,
  uint16_t      index,
  uint8_t       value,
  uint8_t       mask,
  uint32_t      poll_delay_ms)
{

  /*
   * Platform implementation of WaitValueMaskEx V2WReg script command
   *
   * WaitValueMaskEx(
   *          duration_ms,
   *          index,
   *          value,
   *          mask,
   *          poll_delay_ms);
   */

  VL53L4CX_Error status         = VL53L4CX_ERROR_NONE;
  uint32_t     start_time_ms = 0;
  uint32_t     current_time_ms = 0;
  uint32_t     polling_time_ms = 0;
  uint8_t      byte_value      = 0;
  uint8_t      found           = 0;



  /* calculate time limit in absolute time */

  VL53L4CX_GetTickCount(&start_time_ms);

  /* remember current trace functions and temporarily disable
   * function logging
   */


  /* wait until value is found, timeout reached on error occurred */

  while ((status == VL53L4CX_ERROR_NONE) &&
         (polling_time_ms < timeout_ms) &&
         (found == 0)) {

    if (status == VL53L4CX_ERROR_NONE)
      status = VL53L4CX_RdByte(
                 pdev,
                 index,
                 &byte_value);

    if ((byte_value & mask) == value) {
      found = 1;
    }

    if (status == VL53L4CX_ERROR_NONE  &&
        found == 0 &&
        poll_delay_ms > 0)
      status = VL53L4CX_WaitMs(
                 pdev,
                 poll_delay_ms);

    /* Update polling time (Compare difference rather than absolute to
    negate 32bit wrap around issue) */
    VL53L4CX_GetTickCount(&current_time_ms);
    polling_time_ms = current_time_ms - start_time_ms;

  }


  if (found == 0 && status == VL53L4CX_ERROR_NONE) {
    status = VL53L4CX_ERROR_TIME_OUT;
  }

  return status;
}
