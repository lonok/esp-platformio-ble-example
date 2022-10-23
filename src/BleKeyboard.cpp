#include "Arduino.h"
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEDevice.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include <driver/adc.h>
//#include "HIDKeyboardTypes.h"
#include "Print.h"
#include "BleKeyboard.h"

#define KEYBOARD_ID 0x01
#define MEDIA_KEYS_ID 0x02
#define MOUSE_ID 0x03

#define SHIFT 0x80

bool isBleConnected = false;

struct InputReport {
    uint8_t modifiers;	     // bitmask: CTRL = 1, SHIFT = 2, ALT = 4
    uint8_t reserved;        // must be 0
    uint8_t pressedKeys[6];  // up to six concurrenlty pressed keys
};
// Message (report) received when an LED's state changed
struct OutputReport {
    uint8_t leds;            // bitmask: num lock = 1, caps lock = 2, scroll lock = 4, compose = 8, kana = 16
};

// The report map describes the HID device (a keyboard in this case) and
// the messages (reports in HID terms) sent and received.
static const uint8_t REPORT_MAP[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Controls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    // KEYBOARD
    REPORT_ID(1),       KEYBOARD_ID,       //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Keyboard/Keypad
    USAGE_MINIMUM(1),   0xE0,       //   Keyboard Left Control
    USAGE_MAXIMUM(1),   0xE7,       //   Keyboard Right Control
    LOGICAL_MINIMUM(1), 0x00,       //   Each bit is either 0 or 1
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1),    0x08,       //   8 bits for the modifier keys
    REPORT_SIZE(1),     0x01,       
    HIDINPUT(1),        0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   1 byte (unused)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),        0x01,       //   Const, Array, Abs
    REPORT_COUNT(1),    0x06,       //   6 bytes (for up to 6 concurrently pressed keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),        0x00,       //   Data, Array, Abs
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    HIDOUTPUT(1),       0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),       0x01,       //   Const, Array, Abs
    END_COLLECTION(0),               // End application collection
    // MEDIA KEYS
    USAGE_PAGE(1),      0x0C,          // USAGE_PAGE (Consumer)
    USAGE(1),           0x01,          // USAGE (Consumer Control)
    COLLECTION(1),      0x01,          // COLLECTION (Application)
    REPORT_ID(1),       MEDIA_KEYS_ID, //   REPORT_ID (3)
    USAGE_PAGE(1),      0x0C,          //   USAGE_PAGE (Consumer)
    LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
    LOGICAL_MAXIMUM(1), 0x01,          //   LOGICAL_MAXIMUM (1)
    REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
    REPORT_COUNT(1),    0x10,          //   REPORT_COUNT (16)
    USAGE(1),           0xB5,          //   USAGE (Scan Next Track)     ; bit 0: 1
    USAGE(1),           0xB6,          //   USAGE (Scan Previous Track) ; bit 1: 2
    USAGE(1),           0xB7,          //   USAGE (Stop)                ; bit 2: 4
    USAGE(1),           0xCD,          //   USAGE (Play/Pause)          ; bit 3: 8
    USAGE(1),           0xE2,          //   USAGE (Mute)                ; bit 4: 16
    USAGE(1),           0xE9,          //   USAGE (Volume Increment)    ; bit 5: 32
    USAGE(1),           0xEA,          //   USAGE (Volume Decrement)    ; bit 6: 64
    USAGE(2),           0x23, 0x02,    //   Usage (WWW Home)            ; bit 7: 128
    USAGE(2),           0x94, 0x01,    //   Usage (My Computer) ; bit 0: 1
    USAGE(2),           0x92, 0x01,    //   Usage (Calculator)  ; bit 1: 2
    USAGE(2),           0x2A, 0x02,    //   Usage (WWW fav)     ; bit 2: 4
    USAGE(2),           0x21, 0x02,    //   Usage (WWW search)  ; bit 3: 8
    USAGE(2),           0x26, 0x02,    //   Usage (WWW stop)    ; bit 4: 16
    USAGE(2),           0x24, 0x02,    //   Usage (WWW back)    ; bit 5: 32
    USAGE(2),           0x83, 0x01,    //   Usage (Media sel)   ; bit 6: 64
    USAGE(2),           0x8A, 0x01,    //   Usage (Mail)        ; bit 7: 128
    HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    END_COLLECTION(0)                 // END_COLLECTION
};

const InputReport NO_KEY_PRESSED = { };

BleKeyboard::BleKeyboard(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) : hid(0)
{
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connection = new BleConnection();
  BLECharacteristic* inputKeyboard;
  BLECharacteristic* outputKeyboard;
}

void BleKeyboard::begin(void)
{
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

// uint8_t USBPutChar(uint8_t c);

extern
const uint8_t _asciimap[128] PROGMEM;

const uint8_t _asciimap[128] =
{
	0x00,             // NUL
	0x00,             // SOH
	0x00,             // STX
	0x00,             // ETX
	0x00,             // EOT
	0x00,             // ENQ
	0x00,             // ACK
	0x00,             // BEL
	0x2a,			// BS	Backspace
	0x2b,			// TAB	Tab
	0x28,			// LF	Enter
	0x00,             // VT
	0x00,             // FF
	0x00,             // CR
	0x00,             // SO
	0x00,             // SI
	0x00,             // DEL
	0x00,             // DC1
	0x00,             // DC2
	0x00,             // DC3
	0x00,             // DC4
	0x00,             // NAK
	0x00,             // SYN
	0x00,             // ETB
	0x00,             // CAN
	0x00,             // EM
	0x00,             // SUB
	0x00,             // ESC
	0x00,             // FS
	0x00,             // GS
	0x00,             // RS
	0x00,             // US

	0x2c,		   //  ' '
	0x1e|SHIFT,	   // !
	0x34|SHIFT,	   // "
	0x20|SHIFT,    // #
	0x21|SHIFT,    // $
	0x22|SHIFT,    // %
	0x24|SHIFT,    // &
	0x34,          // '
	0x26|SHIFT,    // (
	0x27|SHIFT,    // )
	0x25|SHIFT,    // *
	0x2e|SHIFT,    // +
	0x36,          // ,
	0x2d,          // -
	0x37,          // .
	0x38,          // /
	0x27,          // 0
	0x1e,          // 1
	0x1f,          // 2
	0x20,          // 3
	0x21,          // 4
	0x22,          // 5
	0x23,          // 6
	0x24,          // 7
	0x25,          // 8
	0x26,          // 9
	0x33|SHIFT,      // :
	0x33,          // ;
	0x36|SHIFT,      // <
	0x2e,          // =
	0x37|SHIFT,      // >
	0x38|SHIFT,      // ?
	0x1f|SHIFT,      // @
	0x04|SHIFT,      // A
	0x05|SHIFT,      // B
	0x06|SHIFT,      // C
	0x07|SHIFT,      // D
	0x08|SHIFT,      // E
	0x09|SHIFT,      // F
	0x0a|SHIFT,      // G
	0x0b|SHIFT,      // H
	0x0c|SHIFT,      // I
	0x0d|SHIFT,      // J
	0x0e|SHIFT,      // K
	0x0f|SHIFT,      // L
	0x10|SHIFT,      // M
	0x11|SHIFT,      // N
	0x12|SHIFT,      // O
	0x13|SHIFT,      // P
	0x14|SHIFT,      // Q
	0x15|SHIFT,      // R
	0x16|SHIFT,      // S
	0x17|SHIFT,      // T
	0x18|SHIFT,      // U
	0x19|SHIFT,      // V
	0x1a|SHIFT,      // W
	0x1b|SHIFT,      // X
	0x1c|SHIFT,      // Y
	0x1d|SHIFT,      // Z
	0x2f,          // [
	0x31,          // bslash
	0x30,          // ]
	0x23|SHIFT,    // ^
	0x2d|SHIFT,    // _
	0x35,          // `
	0x04,          // a
	0x05,          // b
	0x06,          // c
	0x07,          // d
	0x08,          // e
	0x09,          // f
	0x0a,          // g
	0x0b,          // h
	0x0c,          // i
	0x0d,          // j
	0x0e,          // k
	0x0f,          // l
	0x10,          // m
	0x11,          // n
	0x12,          // o
	0x13,          // p
	0x14,          // q
	0x15,          // r
	0x16,          // s
	0x17,          // t
	0x18,          // u
	0x19,          // v
	0x1a,          // w
	0x1b,          // x
	0x1c,          // y
	0x1d,          // z
	0x2f|SHIFT,    // {
	0x31|SHIFT,    // |
	0x30|SHIFT,    // }
	0x35|SHIFT,    // ~
	0				// DEL
};

size_t BleKeyboard::press(uint8_t k)
{
	uint8_t i;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		k = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers |= (1<<(k-128));
		k = 0;
	} else {				// it's a printing key
		k = pgm_read_byte(_asciimap + k);
		if (!k) {
			setWriteError();
			return 0;
		}
		if (k & 0x80) {						// it's a capital letter or other character reached with shift
			_keyReport.modifiers |= 0x02;	// the left shift modifier
			k &= 0x7F;
		}
	}

	// Add k to the key report only if it's not already present
	// and if there is an empty slot.
	if (_keyReport.keys[0] != k && _keyReport.keys[1] != k &&
		_keyReport.keys[2] != k && _keyReport.keys[3] != k &&
		_keyReport.keys[4] != k && _keyReport.keys[5] != k) {

		for (i=0; i<6; i++) {
			if (_keyReport.keys[i] == 0x00) {
				_keyReport.keys[i] = k;
				break;
			}
		}
		if (i == 6) {
			setWriteError();
			return 0;
		}
	}
	sendReport(&_keyReport);
	return 1;
}
size_t BleKeyboard::press(const MediaKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);

    mediaKeyReport_16 |= k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

	sendReport(&_mediaKeyReport);
	return 1;
}

class OutputCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
		 uint8_t* value = (uint8_t*)(characteristic->getValue().c_str());
  		// ESP_LOGI(LOG_TAG, "special keys: %d", *value);
		/*
        OutputReport* report = (OutputReport*) characteristic->getData();
        Serial.print("LED state: ");
        Serial.print((int) report->leds);
        Serial.println();
		*/
    }
};

void BleKeyboard::setDeviceName(std::string device_name) {
    this->deviceName = device_name;
}
void BleKeyboard::setDeviceManufacturer(std::string manufacturer) {
    this->deviceManufacturer = manufacturer;
}

void BleKeyboard::setVendorId(uint16_t id) {
    this->vendor_id = id;
}

void BleKeyboard::setProductId(uint16_t id) {
    this->product_id = id;
}

bool BleKeyboard::isConnected(void) {
  return this->connection->connected;
}

void BleKeyboard::taskServer(void* pvParameter) {
    BleKeyboard* keyboardInstance = (BleKeyboard *) pvParameter;

    BLEDevice::init(keyboardInstance->deviceName);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(keyboardInstance->connection);

    // create an HID device
    keyboardInstance->hid = new BLEHIDDevice(server);

    keyboardInstance->inputKeyboard = keyboardInstance->hid->inputReport(KEYBOARD_ID); // report ID
    keyboardInstance->outputKeyboard = keyboardInstance->hid->outputReport(KEYBOARD_ID); // report ID
    keyboardInstance->inputMediaKeys = keyboardInstance->hid->inputReport(MEDIA_KEYS_ID); // report ID

	keyboardInstance->connection->inputKeyboard = keyboardInstance->inputKeyboard;
	keyboardInstance->connection->outputKeyboard = keyboardInstance->outputKeyboard;

    keyboardInstance->outputKeyboard->setCallbacks(new OutputCallbacks());

    keyboardInstance->hid->manufacturer()->setValue(keyboardInstance->deviceManufacturer); 
    keyboardInstance->hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    keyboardInstance->hid->hidInfo(0x00, 0x01); // flags original one device - 0x02 | combo uses 0x01

    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);

    keyboardInstance->hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    keyboardInstance->hid->startServices();

    // advertise the services
    BLEAdvertising* advertising = server->getAdvertising();
    advertising->addServiceUUID(keyboardInstance->hid->hidService()->getUUID());
    advertising->addServiceUUID(keyboardInstance->hid->deviceInfo()->getUUID());
    advertising->addServiceUUID(keyboardInstance->hid->batteryService()->getUUID());
    advertising->setAppearance(HID_KEYBOARD);
    advertising->start();

    keyboardInstance->hid->setBatteryLevel(100);

    //ESP_LOGD(LOG_TAG, "[Bluetooth] Keyboard advertising started");

    vTaskDelay(portMAX_DELAY);
};

void BleKeyboard::setBatteryLevel(uint8_t level) {
  this->batteryLevel = level;
  if (hid != 0)
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleKeyboard::sendReport(KeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputKeyboard->setValue((uint8_t*)keys, sizeof(KeyReport));
    this->inputKeyboard->notify();
  }
}

void BleKeyboard::sendReport(MediaKeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputMediaKeys->setValue((uint8_t*)keys, sizeof(MediaKeyReport));
    this->inputMediaKeys->notify();
  }
}

// release() takes the specified key out of the persistent key report and
// sends the report.  This tells the OS the key is no longer pressed and that
// it shouldn't be repeated any more.
size_t BleKeyboard::release(uint8_t k)
{
	uint8_t i;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		k = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers &= ~(1<<(k-128));
		k = 0;
	} else {				// it's a printing key
		k = pgm_read_byte(_asciimap + k);
		if (!k) {
			return 0;
		}
		if (k & 0x80) {							// it's a capital letter or other character reached with shift
			_keyReport.modifiers &= ~(0x02);	// the left shift modifier
			k &= 0x7F;
		}
	}

	// Test the key report to see if k is present.  Clear it if it exists.
	// Check all positions in case the key is present more than once (which it shouldn't be)
	for (i=0; i<6; i++) {
		if (0 != k && _keyReport.keys[i] == k) {
			_keyReport.keys[i] = 0x00;
		}
	}

	sendReport(&_keyReport);
	return 1;
}

size_t BleKeyboard::release(const MediaKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);
    mediaKeyReport_16 &= ~k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

	sendReport(&_mediaKeyReport);
	return 1;
}

void BleKeyboard::releaseAll(void)
{
	_keyReport.keys[0] = 0;
	_keyReport.keys[1] = 0;
	_keyReport.keys[2] = 0;
	_keyReport.keys[3] = 0;
	_keyReport.keys[4] = 0;
	_keyReport.keys[5] = 0;
	_keyReport.modifiers = 0;
    _mediaKeyReport[0] = 0;
    _mediaKeyReport[1] = 0;
	sendReport(&_keyReport);
}

size_t BleKeyboard::write(uint8_t c)
{
	uint8_t p = press(c);  // Keydown
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleKeyboard::write(const MediaKeyReport c)
{
	uint16_t p = press(c);  // Keydown
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleKeyboard::write(const uint8_t *buffer, size_t size) {
	size_t n = 0;
	while (size--) {
		if (*buffer != '\r') {
			if (write(*buffer)) {
			  n++;
			} else {
			  break;
			}
		}
		buffer++;
	}
	return n;
}