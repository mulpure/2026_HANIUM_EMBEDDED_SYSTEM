#pragma once
// Hooks called by the vendored ST transport once per complete I2C transaction.
extern "C" bool navSegmentLock();
extern "C" void navSegmentUnlock();
