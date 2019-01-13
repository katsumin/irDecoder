#ifndef _IRdecpde_h
#define _IRdecode_h

#include <M5Stack.h>
#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "soc/rmt_reg.h"

#define THRESHOLD_T 200

class M5StackIRdecode
{
public:
  M5StackIRdecode();
  boolean parseData(rmt_item32_t *item, size_t rxSize);
  inline String getType() { return _type; }
  inline String getCustomCode() { return _customCode; }
  inline String getData() { return _data; }
  inline String getIrData() { return _ircode; }

private:
  String _error = "";
  String _ircode = "";
  String _type = "UNKNOWN";
  String _customCode = "";
  String _data;
  boolean isValid(rmt_item32_t item);
  uint32_t calc_t(rmt_item32_t *item, size_t rxSize, size_t *validSize);
  boolean decodeNec(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer);
  boolean decodeAeha(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer);
  boolean decodeSony(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer);
  String toString(byte *nibble_buffer, size_t len);
};

#endif