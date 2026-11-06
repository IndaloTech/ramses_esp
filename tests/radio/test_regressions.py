"""Run with python3 tests/radio/test_regressions.py (requires Clang and pthreads).

Production C files are included unchanged against generated ESP-IDF shims.
Only mqtt_publish_rx is extracted, to avoid mocking the entire MQTT client.
Hardware timing is simulated; these tests do not replace a device soak test.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CJSON = ROOT / "managed_components/espressif__cjson/cJSON"

MOCK = r"""
#ifndef MOCK_IDF_H
#define MOCK_IDF_H
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#define CONFIG_CC_SPI2_HOST 1
#define CONFIG_CC_SCK_GPIO 18
#define CONFIG_CC_MOSI_GPIO 11
#define CONFIG_CC_MISO_GPIO 13
#define CONFIG_CC_CSN_GPIO 10
#define CONFIG_CC_GDO0_GPIO 14
#define CONFIG_CC_GDO2_GPIO 9
#define CONFIG_CC_LOG_LEVEL 0
#define CONFIG_FRM_LOG_LEVEL 0
#define CONFIG_UART_LOG_LEVEL 0
#define CONFIG_MSG_LOG_LEVEL 0
#define CONFIG_FRM_MIN_TX_DELAY 50
#define CONFIG_FRM_MAX_CAL_INTERVAL 30
#define CONFIG_N_MSG 16
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGD(tag, ...) ((void)(tag))
#define ESP_LOGV(tag, ...) ((void)(tag))
static inline void esp_log_level_set(const char *tag, int level) {}
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERROR_CHECK(call) assert((call) == ESP_OK)
static inline const char *esp_err_to_name(int err) { return "mock error"; }
#define IRAM_ATTR
static int64_t mock_now = 1000000;
static inline int64_t esp_timer_get_time(void) { return mock_now; }
static inline void esp_rom_delay_us(unsigned us) { mock_now += us; }
typedef int BaseType_t;
typedef unsigned TickType_t;
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) ((ms) / 10)
#define portTICK_PERIOD_MS 10
#define portYIELD_FROM_ISR() ((void)0)
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) assert(pthread_mutex_lock(lock) == 0)
#define portEXIT_CRITICAL(lock) assert(pthread_mutex_unlock(lock) == 0)
static inline void vTaskDelay(unsigned ticks) { mock_now += ticks * 10000; }

typedef int uart_port_t;
enum { UART_DATA, UART_FIFO_OVF, UART_BUFFER_FULL, UART_FRAME_ERR, UART_PARITY_ERR };
typedef struct { int type; size_t size; } uart_event_t;
typedef struct { unsigned count, item_size; uart_event_t events[64]; } MockQueue;
static MockQueue mock_queue;
typedef MockQueue *QueueHandle_t;
static inline QueueHandle_t xQueueCreate(unsigned n, unsigned size) {
  memset(&mock_queue, 0, sizeof(mock_queue)); mock_queue.item_size = size; return &mock_queue;
}
static inline int xQueueReset(QueueHandle_t q) { q->count = 0; return pdTRUE; }
static inline int xQueueReceive(QueueHandle_t q, void *out, unsigned ticks) {
  if (!q->count) { mock_now += ticks * 10000; return pdFALSE; }
  if (out) memcpy(out, q->events, q->item_size);
  --q->count;
  memmove(q->events, q->events + 1, q->count * sizeof(q->events[0]));
  return pdTRUE;
}
static inline int xQueueSendFromISR(QueueHandle_t q, const void *in, int *woken) {
  if (q->count == 64) return pdFALSE;
  if (in) memcpy(q->events + q->count, in, q->item_size);
  ++q->count; return pdTRUE;
}
enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_INTR_NEGEDGE, GPIO_INTR_POSEDGE };
static int mock_gpio_level, mock_gpio_enabled, mock_handler, mock_bad_enable;
static int (*mock_get_level)(int);
static inline int gpio_get_level(int pin) { return mock_get_level ? mock_get_level(pin) : mock_gpio_level; }
static inline int gpio_intr_disable(int pin) { mock_gpio_enabled = 0; return 0; }
static inline int gpio_intr_enable(int pin) {
  if (!mock_handler) ++mock_bad_enable;
  mock_gpio_enabled = 1; return 0;
}
static inline int gpio_set_intr_type(int pin, int type) { return 0; }
static inline int gpio_isr_handler_add(int pin, void (*isr)(void *), void *arg) {
  mock_handler = mock_gpio_enabled = 1; return 0;
}
static inline int gpio_isr_handler_remove(int pin) { mock_handler = mock_gpio_enabled = 0; return 0; }
static inline int gpio_reset_pin(int pin) { return 0; }
static inline int gpio_set_direction(int pin, int mode) { return 0; }
static inline int gpio_pulldown_en(int pin) { return 0; }
static inline int gpio_pullup_dis(int pin) { return 0; }
static inline int gpio_set_level(int pin, int value) { return 0; }
enum { LED_RX, LED_TX };
static inline void led_on(int led) {}
static inline void led_off(int led) {}
#define DEBUG2_ON ((void)0)
#define DEBUG2_OFF ((void)0)
#define DEBUG3_ON ((void)0)
#define DEBUG3_OFF ((void)0)
struct message;
void gateway_radio_rx(struct message **msg);

enum { SPI2_HOST, SPI_DMA_CH_AUTO, SPI_DEVICE_NO_DUMMY = 1,
       SPI_TRANS_USE_TXDATA = 2, SPI_TRANS_USE_RXDATA = 4 };
typedef void *spi_device_handle_t;
typedef struct { int sclk_io_num, mosi_io_num, miso_io_num, quadwp_io_num, quadhd_io_num; } spi_bus_config_t;
typedef struct { int clock_speed_hz, queue_size, mode, spics_io_num, flags; } spi_device_interface_config_t;
typedef struct { unsigned flags, length; uint8_t tx_data[4], rx_data[4]; } spi_transaction_t;
static int (*mock_spi)(spi_transaction_t *);
static inline int spi_device_transmit(void *handle, spi_transaction_t *t) { return mock_spi(t); }
static inline int spi_bus_initialize(int host, const spi_bus_config_t *cfg, int dma) { return 0; }
static inline int spi_bus_add_device(int host, const spi_device_interface_config_t *cfg, void **out) { *out = (void *)1; return 0; }

enum { UART_NUM_1, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1,
       UART_HW_FLOWCTRL_DISABLE, UART_SCLK_DEFAULT, UART_PIN_NO_CHANGE = -1 };
#define UART_INTR_RXFIFO_FULL 1
#define UART_INTR_RXFIFO_TOUT 2
#define UART_INTR_RXFIFO_OVF 4
#define UART_INTR_FRAM_ERR 8
#define UART_INTR_PARITY_ERR 16
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; } uart_config_t;
typedef struct { unsigned intr_enable_mask, rx_timeout_thresh, rxfifo_full_thresh; } uart_intr_config_t;
static uint8_t mock_uart_data[4096];
static size_t mock_uart_bytes;
static int mock_uart_enabled, mock_uart_reads, mock_uart_continuous;
static inline int uart_disable_rx_intr(int port) { mock_uart_enabled = 0; return 0; }
static inline int uart_enable_rx_intr(int port) { mock_uart_enabled = 1; return 0; }
static inline int uart_flush_input(int port) { mock_uart_bytes = 0; return 0; }
static inline int uart_read_bytes(int port, void *out, unsigned length, unsigned ticks) {
  ++mock_uart_reads;
  if (mock_uart_continuous) { memset(out, 0xAA, length); return length; }
  unsigned n = mock_uart_bytes < length ? mock_uart_bytes : length;
  memcpy(out, mock_uart_data, n);
  mock_uart_bytes -= n;
  memmove(mock_uart_data, mock_uart_data + n, mock_uart_bytes);
  if (n) mock_uart_enabled = 1; // Model driver re-arming after a full-buffer read.
  return n;
}
static inline int uart_set_pin(int p, int tx, int rx, int rts, int cts) { return 0; }
static inline int uart_param_config(int p, const uart_config_t *cfg) { return 0; }
static inline int uart_driver_install(int p, int rx, int tx, int n, QueueHandle_t *q, int flags) {
  *q = xQueueCreate(n, sizeof(uart_event_t)); return 0;
}
static inline int uart_intr_config(int p, const uart_intr_config_t *cfg) { return 0; }
#endif
"""

FRAME_TEST = r"""
#include "components/frame/frame.c"
static bool rx_ok = true, prepare_ok = true, start_ok = true, fifo_ok = true;
static bool underflow, empty_signal, stuck_low;
static unsigned fifo_bytes, rx_entries, tx_aborts, tx_completions, rx_ends;
static uint8_t last_error, raw[162];
static bool rx_owned;
bool cc_enter_idle_mode(void) { return true; }
bool cc_enter_rx_mode(void) { ++rx_entries; return rx_ok; }
bool cc_enter_tx_mode(void) { fifo_bytes = 0; underflow = empty_signal = false; return prepare_ok; }
bool cc_start_tx(void) { assert(fifo_bytes > 0); return start_ok; }
bool cc_tx_status(uint8_t *bytes, bool *error) { *bytes = fifo_bytes; *error = underflow; return fifo_ok; }
uint8_t cc_write_fifo(uint8_t byte) { assert(++fifo_bytes <= 64); return 15; }
void cc_fifo_end(void) { empty_signal = true; }
uint8_t cc_read_rssi(void) { return 60; }
void cc_init(void) {}
void uart_init(void) {}
void uart_work(void) {}
void uart_disable(void) { mock_uart_enabled = 0; }
void uart_rx_enable(void) { mock_uart_enabled = 1; }
uint8_t *msg_rx_start(void) { assert(!rx_owned); rx_owned = true; raw[0] = sizeof(raw); return raw; }
uint8_t msg_rx_byte(uint8_t byte) { return MSG_OK; }
void msg_rx_rssi(uint8_t rssi) { assert(rx_owned); }
void msg_rx_end(uint8_t n, uint8_t error) { assert(rx_owned); rx_owned = false; ++rx_ends; last_error = error; }
uint8_t msg_tx_byte(uint8_t *done) { *done = 1; return 0; }
void msg_tx_end(uint8_t n) {}
void msg_tx_done(void) { ++tx_completions; }
void msg_tx_abort(void) { ++tx_aborts; }
static int fifo_level(int pin) { return stuck_low ? 0 : (empty_signal ? underflow : fifo_bytes >= 33); }
static void reset(void) {
  frame_reset(); frame_rx_reset(); frame_tx_reset();
  rx_ok = prepare_ok = start_ok = fifo_ok = true;
  underflow = empty_signal = stuck_low = rx_owned = false;
  fifo_bytes = rx_entries = tx_aborts = tx_completions = rx_ends = 0;
  mock_now = 1000000; last_frm = last_cal = next_rx_attempt = 0;
  mock_bad_enable = mock_handler = mock_gpio_enabled = 0;
  mock_get_level = fifo_level;
  tx_isr_queue = xQueueCreate(1, 0);
  syncWord = 0x00335553;
  frame_rx_enable();
}
static void header(void) {
  const uint8_t bytes[] = {0xFF, 0x00, 0x33, 0x55, 0x53};
  for (unsigned i=0; i<sizeof(bytes); ++i) frame_rx_byte(bytes[i]);
}
static void start_frame(unsigned bytes) {
  txFrm.raw = raw; txFrm.nBytes = bytes; txFrm.state = FRM_TX_READY;
  frame_tx_enable();
}
int main(void) {
  reset(); header(); assert(rxFrm.state == FRM_RX_MESSAGE);
  mock_now += 51000; frame_work();
  assert(rx_ends == 1 && last_error == MSG_TRUNC_ERR && !rx_owned);
  assert(frame.state == FRM_RX && mock_uart_enabled);

  reset(); header();
  for (unsigned i=0; i<5; ++i) { mock_now += 40000; frame_rx_byte(0xAA); frame_work(); }
  assert(rx_ends == 1 && last_error == MSG_TRUNC_ERR); // Overall deadline despite continuing bytes.

  reset(); header(); frame_rx_byte(0x35); header(); frame_rx_byte(0x35); frame_work();
  assert(rx_ends == 2 && !rx_owned && rxFrm.state == FRM_RX_IDLE);
  reset(); header(); frame_rx_error(); frame_work(); assert(rx_ends == 1 && !rx_owned);

  reset(); rx_ok = false; frame_rx_enable();
  assert(frame.state == FRM_IDLE && !mock_uart_enabled);
  unsigned attempts = rx_entries; frame_work(); assert(rx_entries == attempts);
  mock_now += 500000; rx_ok = true; frame_work(); assert(frame.state == FRM_RX);

  reset(); start_frame(100); assert(frame.state == FRM_TX && fifo_bytes >= 33);
  fifo_bytes = 10; // Falling edge was lost: no queued ISR notification.
  frame_work(); assert(fifo_bytes > 10 && tx_aborts == 0);

  reset(); start_frame(0); assert(tx_state == TX_FIFO_WAIT); // Short frame primed without hanging.
  fifo_bytes = 0; underflow = true; frame_work(); frame_work();
  assert(tx_completions == 1 && tx_aborts == 0 && frame.state == FRM_RX);
  assert(!mock_handler && !mock_gpio_enabled && mock_bad_enable == 0);

  reset(); start_frame(100); underflow = true; fifo_bytes = 0; frame_work();
  assert(tx_aborts == 1 && tx_completions == 0 && frame.state == FRM_RX);
  reset(); start_frame(100); mock_now += 501000; frame_work();
  assert(tx_aborts == 1 && frame.state == FRM_RX && mock_uart_enabled);
  reset(); stuck_low = true; start_frame(100);
  assert(tx_aborts == 1 && frame.state == FRM_RX && fifo_bytes <= 64);
  reset(); prepare_ok = false; start_frame(100); assert(tx_aborts == 1 && frame.state == FRM_RX);
  reset(); start_ok = false; start_frame(100); assert(tx_aborts == 1 && frame.state == FRM_RX);
  reset(); start_frame(100); fifo_ok = false; fifo_bytes = 0; frame_work();
  assert(tx_aborts == 1 && frame.state == FRM_RX);
  return 0;
}
"""

CC_TEST = r"""
#include "components/cc1101/cc1101.c"
#include "components/cc1101/cc1101_param.c"
static uint8_t state, txbytes;
static bool stuck, fail, alternate;
static unsigned starts, transfers;
static int transfer(spi_transaction_t *t) {
  ++transfers;
  assert(t->flags == (SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA));
  assert(t->length == 8 || t->length == 16);
  if (fail) return -1;
  t->rx_data[0] = state | 15;
  t->rx_data[1] = alternate ? (transfers & 1) : txbytes;
  if (t->length == 8 && !stuck) {
    switch(t->tx_data[0]) {
      case CC_SIDLE: state = CC_STATE_IDLE; break;
      case CC_SRX: state = CC_STATE_RX; break;
      case CC_STX: state = CC_STATE_TX; ++starts; break;
    }
  }
  return 0;
}
int main(void) {
  mock_spi = transfer;
  struct { uint8_t before, status, after; } guard = {0xA5, 0, 0x5A};
  uint8_t command[2] = {CC_FIFO, 0xAA};
  assert(spi_write_byte(&guard.status, command, 2));
  assert(guard.before == 0xA5 && guard.after == 0x5A && guard.status == 15);
  assert(cc_enter_tx_mode()); assert(starts == 0 && state == CC_STATE_IDLE);
  cc_write_fifo(0xAA); assert(cc_start_tx()); assert(starts == 1);
  assert(cc_enter_rx_mode() && state == CC_STATE_RX);
  stuck = true; mock_now = 0; unsigned before = transfers;
  assert(!cc_enter_idle_mode()); assert(mock_now >= 20000 && transfers-before < 1100);
  stuck = false; fail = true; assert(!cc_enter_rx_mode());
  fail = false; assert(cc_enter_rx_mode()); // A later recovery attempt can succeed.
  uint8_t bytes; bool underflow;
  txbytes = 0x85; assert(cc_tx_status(&bytes, &underflow) && bytes == 5 && underflow);
  alternate = true; before = transfers; assert(!cc_tx_status(&bytes, &underflow));
  assert(transfers-before == 5); alternate = false;
  cc_io_ok = true; state = CC_STATE_TX_UNDERFLOW; cc_write_fifo(0xAA);
  assert(!cc_tx_status(&bytes, &underflow)); // Early underflow cannot become a success echo.
  return 0;
}
"""

UART_TEST = r"""
#include "components/frame/uart.c"
static unsigned received, errors;
void frame_rx_byte(uint8_t byte) { ++received; }
void frame_rx_error(void) { ++errors; }
static void load(unsigned bytes) {
  mock_uart_bytes = bytes; memset(mock_uart_data, 0xAA, bytes);
}
int main(void) {
  uart_init(); uart_rx_enable();
  load(300); mock_uart_enabled = 0; // Buffered data with all notifications lost.
  uart_work(); assert(received == 300 && mock_uart_bytes == 0 && mock_uart_enabled);
  for (unsigned type=UART_FIFO_OVF; type<=UART_PARITY_ERR; ++type) {
    load(100); mock_queue.count = 1; mock_queue.events[0].type = type;
    uart_work(); assert(mock_uart_bytes == 0 && mock_queue.count == 0 && mock_uart_enabled);
  }
  assert(errors == 4 && received == 300);
  load(400); mock_queue.count = 1;
  mock_queue.events[0] = (uart_event_t){UART_DATA, 100000}; // Never use event.size as a buffer length.
  uart_work(); assert(received == 700);
  mock_uart_continuous = 1; mock_uart_reads = 0; unsigned before = received;
  uart_work(); assert(mock_uart_reads == 4 && received-before == 512);
  mock_uart_continuous = 0; uart_disable(); load(100); uart_work(); assert(mock_uart_bytes == 100);
  uart_rx_enable(); assert(mock_uart_bytes == 0 && mock_uart_enabled);
  return 0;
}
"""

POOL_TEST = r"""
#include "components/message/message.c"
#include "components/message/msg.c"
static struct message *entries[CONFIG_N_MSG];
static atomic_uint owners[CONFIG_N_MSG];
void gateway_radio_rx(struct message **msg) { msg_free(msg); }
void frame_tx_start(uint8_t *raw, uint8_t bytes) {}
static unsigned index_of(struct message *msg) {
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) if (entries[i] == msg) return i;
  abort();
}
static void *worker(void *arg) {
  for (unsigned i=0; i<20000; ++i) {
    struct message *msg;
    while (!(msg = msg_alloc())) sched_yield();
    unsigned index = index_of(msg);
    assert(atomic_exchange(&owners[index], 1) == 0);
    if (i % 7 == 0) sched_yield();
    assert(atomic_exchange(&owners[index], 0) == 1);
    msg_free(&msg); assert(!msg);
  }
  return NULL;
}
int main(void) {
  msg_init();
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) { entries[i] = msg_alloc(); assert(entries[i]); }
  assert(!msg_alloc());
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) { struct message *msg=entries[i]; msg_free(&msg); }
  pthread_t threads[6];
  for (unsigned i=0; i<6; ++i) assert(!pthread_create(&threads[i], NULL, worker, NULL));
  for (unsigned i=0; i<6; ++i) assert(!pthread_join(threads[i], NULL));
  struct message *messages[CONFIG_N_MSG];
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) {
    messages[i] = msg_alloc(); assert(messages[i]);
    for (unsigned j=0; j<i; ++j) assert(messages[i] != messages[j]);
    msg_tx_ready(&messages[i]); assert(!messages[i]);
  }
  assert(!msg_alloc());
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) { TxMsg = msg_tx_get(); assert(TxMsg); msg_tx_abort(); assert(!TxMsg); }
  assert(!msg_tx_get());
  for (unsigned i=0; i<CONFIG_N_MSG; ++i) { messages[i]=msg_alloc(); assert(messages[i]); }
  assert(!msg_alloc());
  return 0;
}
"""

MQTT_SETUP = r"""
#include "cJSON.h"
struct mqtt_data { char topic[32]; void *client; };
struct message;
static unsigned allocated, released, calls, fail_after;
static void *allocate(size_t bytes) {
  if (calls++ >= fail_after) return NULL;
  void *p = malloc(bytes); if (p) ++allocated; return p;
}
static void release(void *p) { if (p) ++released; free(p); }
static cJSON *decoded_message_to_json(const struct message *source) { return cJSON_CreateObject(); }
static int esp_mqtt_client_publish(void *client, const char *topic, const char *data, int len, int qos, int retain) {
  assert(!strcmp(topic, "gateway/rx"));
  assert(strstr(data, "payload")); return 1;
}
"""

MQTT_TEST = r"""
int main(void) {
  cJSON_Hooks hooks = {allocate, release}; cJSON_InitHooks(&hooks);
  struct mqtt_data context = {.topic = "gateway"};
  for (unsigned limit=0; limit<30; ++limit) {
    allocated = released = calls = 0; fail_after = limit;
    mqtt_publish_rx(&context, "2026-11-06", "payload\r\n", (void *)1);
    assert(allocated == released);
  }
  cJSON_InitHooks(NULL);
  return 0;
}
"""


class RadioRegressions(unittest.TestCase):
    def compile_run(self, body, extra_sources=()):
        compiler = shutil.which(os.environ.get("CC", "clang"))
        self.assertIsNotNone(compiler, "Install Clang or set CC")
        with tempfile.TemporaryDirectory(prefix="ramses-radio-tests-") as temporary:
            temp = Path(temporary)
            (temp / "mock_idf.h").write_text(MOCK)
            headers = ["driver/uart.h", "driver/gpio.h", "driver/spi_master.h",
                       "hal/uart_hal.h", "freertos/FreeRTOS.h", "freertos/task.h",
                       "freertos/portmacro.h", "esp_log.h", "esp_err.h", "esp_timer.h",
                       "esp_rom_sys.h", "ramses_debug.h", "ramses_led.h", "gateway.h"]
            for name in headers:
                path = temp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "mock_idf.h"\n')
            source = temp / "test.c"
            source.write_text('#include "mock_idf.h"\n' + body)
            binary = temp / "test"
            includes = [temp, ROOT, ROOT / "components/frame", ROOT / "components/frame/include",
                        ROOT / "components/message/include", ROOT / "components/message",
                        ROOT / "components/cc1101/include", CJSON]
            command = [compiler, "-std=gnu11", "-g", "-O1", "-pthread",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wno-format",
                       *[f"-I{path}" for path in includes], str(source),
                       *[str(path) for path in extra_sources], "-o", str(binary)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_frame_deadlines_lost_edges_and_recovery(self):
        self.compile_run(FRAME_TEST)

    def test_spi_buffers_and_bounded_mode_changes(self):
        self.compile_run(CC_TEST)

    def test_uart_notification_loss_overflow_and_work_budget(self):
        self.compile_run(UART_TEST)

    def test_message_pool_concurrent_ownership_and_tx_abort(self):
        self.compile_run(POOL_TEST)

    def test_mqtt_payload_lifetime_and_allocation_failures(self):
        source = (ROOT / "components/ramses-mqtt/ramses-mqtt.c").read_text()
        start = source.index("static void mqtt_publish_rx(")
        end = source.index("\nvoid MQTT_publish_rx(", start)
        self.compile_run(MQTT_SETUP + source[start:end] + MQTT_TEST, [CJSON / "cJSON.c"])


if __name__ == "__main__":
    unittest.main(verbosity=2)

