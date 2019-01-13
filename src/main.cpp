#include <M5Stack.h>

#include "freertos/task.h"
#include "time.h"

#include "IrDecode.h"

const rmt_channel_t channel = RMT_CHANNEL_0;
const gpio_num_t irPin = GPIO_NUM_21;
RingbufHandle_t buffer = NULL;

M5StackIRdecode irDec;

// prototype
void rmt_task(void *arg);

void init_rmt(rmt_channel_t channel, gpio_num_t irPin)
{
  rmt_config_t rmtConfig;

  rmtConfig.rmt_mode = RMT_MODE_RX;
  rmtConfig.channel = channel;
  rmtConfig.clk_div = 80;
  rmtConfig.gpio_num = irPin;
  rmtConfig.mem_block_num = 1;

  rmtConfig.rx_config.filter_en = 1;
  rmtConfig.rx_config.filter_ticks_thresh = 255;
  rmtConfig.rx_config.idle_threshold = 10000;

  rmt_config(&rmtConfig);
  rmt_driver_install(rmtConfig.channel, 2048, 0);

  rmt_get_ringbuf_handle(channel, &buffer);
  rmt_rx_start(channel, 1);
}

TFT_eSprite stext1 = TFT_eSprite(&M5.Lcd); // Sprite object graph1

void setup()
{
  // put your setup code here, to run once:
  M5.begin(true, false, true);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  // Create a sprite for the scrolling numbers
  stext1.setColorDepth(8);
  stext1.createSprite(320, 224);
  stext1.fillSprite(TFT_BLUE);                    // Fill sprite with blue
  stext1.setScrollRect(0, 0, 320, 224, TFT_BLUE); // here we set scroll gap fill color to blue
  stext1.setTextColor(TFT_WHITE);                 // White text, no background
  stext1.setTextDatum(BR_DATUM);                  // Bottom right coordinate datum

  init_rmt(channel, irPin);
  xTaskCreate(rmt_task, "rmt_task", 4096, NULL, 10, NULL);
}

String irData = "";
String type = "";
String customCode = "";
String data = "";
uint16_t t = 0;
char buf[256];
void loop()
{
  M5.update();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("%026d", t++);

  if (irData.length())
  {
    sprintf(buf, " %4s,%s,%s", type.c_str(), customCode.c_str(), data.c_str());
    stext1.drawString(irData, 0, 224 - 16 - 1, 2);
    stext1.drawString(buf, 0, 224 - 1, 2);
    stext1.pushSprite(0, 16);
    stext1.scroll(0, -16 * 2);
    irData = "";
  }

  delay(100);
}

void rmt_task(void *arg)
{
  size_t rxSize = 0;
  rmt_item32_t *item;
  while (1)
  {
    item = (rmt_item32_t *)xRingbufferReceive(buffer, &rxSize, 10000);
    if (item)
    {
      if (irDec.parseData(item, rxSize))
      {
        irData = irDec.getIrData();
        type = irDec.getType();
        customCode = irDec.getCustomCode();
        data = irDec.getData();
      }
      vRingbufferReturnItem(buffer, (void *)item);
    }
  }
}