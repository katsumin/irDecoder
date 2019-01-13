#include "IRdecode.h"

M5StackIRdecode::M5StackIRdecode(){};

boolean M5StackIRdecode::isValid(rmt_item32_t item)
{
  if (item.level0 == 1 && item.level1 == 0)
    if (item.duration0 > THRESHOLD_T && item.duration1 > THRESHOLD_T)
      return true;
    else
      return false;
  else
    return false;
}

/**
 * 赤外線データをサーチして、変調単位Tを算出する。
 */
uint32_t M5StackIRdecode::calc_t(rmt_item32_t *item, size_t rxSize, size_t *validSize)
{
  int mode = 0;
  int32_t diff0 = 0;
  int32_t diff1 = 0;
  uint32_t duration0 = 0;
  uint32_t duration1 = 0;
  int count = 0;
  for (int i = 0; i < rxSize; i++)
  {
    // Serial.printf("%4d: %d/%d(lv0:%d/lv1:%d)\n", i, item[i].duration0, item[i].duration1, item[i].level0, item[i].level1);
    if (isValid(item[i]))
    {
      if (duration0 == 0)
      {
        if (200 <= item[i].duration0 && item[i].duration0 <= 900)
        {
          // 基準データの初期化
          duration0 = item[i].duration0;
        }
      }
      else
      {
        // 基準との差分を積算する
        int32_t diff = duration0 - item[i].duration0;
        if (abs(diff * 2) > duration0)
        {
          // duration0で特異値（基準データの0.5倍以上の差分がある値）を検出
          if (mode & 0x01)
            // duration1でも特異値を検出していたので、サーチ終了
            break;
          mode |= 0x02;
        }
        else
        {
          diff0 -= diff;
        }
      }
      if (duration1 == 0)
      {
        if (200 <= item[i].duration1 && item[i].duration1 <= 900)
        {
          // 基準データの初期化
          duration1 = item[i].duration1;
        }
      }
      else
      {
        int32_t diff = duration1 - item[i].duration1;
        if (abs(diff * 2) > duration1)
        {
          // duration1で特異値（基準データの0.5倍以上の差分がある値）を検出
          if (mode & 0x02)
            // duration0でも特異値を検出していたので、サーチ終了
            break;
          mode |= 0x01;
        }
        else
        {
          diff1 -= diff;
        }
      }
      count++;
    }
    else
      break;
  }
  if (count > 0)
  {
    uint32_t t = -1;
    switch (mode)
    {
    case 0:
    case 1:
      // duration1に特異値(bit="1")があった
      t = duration0 + diff0 / count;
      // Serial.printf("duration0,t,count :%d,%d,%d\n", duration0, t, count);
      break;
    case 2:
      // duration0に特異値(bit="1")があった
      t = duration1 + diff1 / count;
      // Serial.printf("duration1,t,count :%d,%d,%d\n", duration1, t, count);
      break;
    default:
      t = -1;
      break;
    }
    *validSize = count;
    return t;
  }
  else
    return -1;
}

boolean M5StackIRdecode::decodeNec(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer)
{
  size_t byte_pos = 0;
  int dbyte = 0;
  for (int i = 0; i < (int)size; i++)
  {
    int dbit = (item[i].duration1 * 2 > t * 3) ? 1 : 0; // duration1に対して、変調単位tの1.5倍を閾値とする
    dbyte >>= 1;
    dbyte |= (dbit == 0) ? 0x00 : 0x80;
    if (i % 8 == 7)
    {
      buffer[byte_pos++] = dbyte;
      // Serial.printf("%X\n", dbyte);
      dbyte = 0;
    }
  }
  if (byte_pos < 4)
    return false;

  boolean parityOk = (buffer[2] ^ buffer[3]) == 0xff;
  if (parityOk)
  {
    _ircode = toString(buffer, byte_pos);
    char buf[4 + 1];
    sprintf(buf, "%02X%02X", buffer[1], buffer[0]);
    _customCode = String(buf);
    sprintf(buf, "%02X", buffer[2]);
    _data = String(buf);
    return true;
  }
  else
  {
    _error = "parity error";
    Serial.printf("parity error :%02X xor %02X\n", buffer[2], buffer[3]);
    return false;
  }
}

boolean M5StackIRdecode::decodeAeha(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer)
{
  size_t byte_pos = 0;
  int dbyte = 0;
  for (int i = 0; i < (int)size; i++)
  {
    int dbit = (item[i].duration1 * 2 > t * 3) ? 1 : 0; // duration1に対して、変調単位tの1.5倍を閾値とする
    dbyte >>= 1;
    dbyte |= (dbit == 0) ? 0x00 : 0x80;
    if (i % 8 == 7)
    {
      buffer[byte_pos++] = dbyte;
      // Serial.printf("%X\n", dbyte);
      dbyte = 0;
    }
  }
  if (byte_pos < 3)
    return false;

  byte bcc = buffer[0] & 0xf;
  bcc ^= buffer[0] >> 4;
  bcc ^= buffer[1] & 0xf;
  bcc ^= buffer[1] >> 4;
  bcc ^= buffer[2] & 0xf;
  if (bcc == 0x00)
  {
    _ircode = toString(buffer, byte_pos);
    char buf[4 + 1];
    sprintf(buf, "%02X%02X", buffer[1], buffer[0]);
    _customCode = String(buf);
    buffer[2] >>= 4; // Data0
    _data = toString(&buffer[2], byte_pos - 2);
    return true;
  }
  else
  {
    _error = "parity error";
    Serial.printf("parity error :%X xor %X xor %X xor %X xor %X\n", //
                  buffer[0] & 0xf,                                  //
                  buffer[0] >> 4,                                   //
                  buffer[1] & 0xf,                                  //
                  buffer[1] >> 4,                                   //
                  buffer[2] & 0xf);
    return false;
  }
}
boolean M5StackIRdecode::decodeSony(rmt_item32_t *item, uint32_t t, size_t size, byte *buffer)
{
  size_t byte_pos = 0;
  int dbyte = 0;
  int i = 0;
  int remainder = 0;
  for (; i < (int)size; i++)
  {
    int dbit = (item[i].duration0 * 2 > t * 3) ? 1 : 0; // duration0に対して、変調単位tの1.5倍を閾値とする
    dbyte >>= 1;
    dbyte |= (dbit == 0) ? 0x00 : 0x80;
    remainder = 7 - i % 8;
    if (remainder == 0)
    {
      buffer[byte_pos++] = dbyte;
      // Serial.printf("%X\n", dbyte);
      dbyte = 0;
    }
  }
  if (remainder != 0)
    buffer[byte_pos++] = dbyte >> remainder;
  if (byte_pos < 2)
    return false;
  _ircode = toString(buffer, byte_pos);

  // 7bit data
  char buf[4 + 1];
  sprintf(buf, "%02X", buffer[0] & 0x7f);
  _data = String(buf);

  // 5/8/13bit address
  int address = 0;
  for (int i = 1; i < byte_pos; i++)
  {
    address <<= 8;
    address |= buffer[byte_pos - i];
  }
  address <<= 1;
  if (buffer[0] & 0x80)
    address += 1;
  sprintf(buf, "%04X", address);
  _customCode = String(buf);
  return true;
}

String M5StackIRdecode::toString(byte *byte_buffer, size_t len)
{
  char buf[len * 2 + 1];
  for (int i = 0; i < len; i++)
  {
    sprintf(&buf[i * 2], "%02X", byte_buffer[i]);
  }
  String ircode = String(buf);
  return ircode;
}

boolean M5StackIRdecode::parseData(rmt_item32_t *item, size_t rxSize)
{
  size_t validSize = 0;
  uint32_t t = calc_t(&item[1], rxSize - 1, &validSize); // Leaderは除外
  if (t > 0)
  {
    // Leaderの長さでREMOCON_TYPEを判定
    boolean valid = false;
    byte dbytes[256];
    uint32_t leader_l = item[0].duration0 / t;
    uint32_t leader_s = item[0].duration1 / t;
    // Serial.printf("%d/%d -> %d/%d\n", item[0].duration0, item[0].duration1, leader_l, leader_s);
    if (leader_l >= 10 && leader_s >= 5)
    {
      _type = "NEC";
      valid = decodeNec(&item[1], t, validSize, dbytes);
    }
    else if (leader_l >= 6 && leader_s >= 3)
    {
      _type = "AEHA";
      valid = decodeAeha(&item[1], t, validSize, dbytes);
    }
    else if (leader_l >= 2)
    {
      _type = "SONY";
      //Leaderが無発光部分を含まない分、data bitが1bitずれる
      valid = decodeSony(&item[1], t, validSize + 1, dbytes);
    }
    else
    {
      _type = "UNKNOWN";
      _error = "unknown format";
      Serial.println(_error);
      valid = false;
    }
    // Serial.println(_type);
    return valid;
  }
  else
  {
    _error = "calc_t() error";
    return false;
  }
}