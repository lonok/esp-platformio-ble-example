#include <BLEServer.h>
#include "BLE2902.h"
#include "BLECharacteristic.h"

class BleConnection : public BLEServerCallbacks
{
public:
  BleConnection(void);
  bool connected = false;
  void onConnect(BLEServer* pServer);
  void onDisconnect(BLEServer* pServer);
  BLECharacteristic* inputKeyboard;
  BLECharacteristic* outputKeyboard;
  BLECharacteristic* inputMediaKeys;
  // BLECharacteristic* inputMouse;
};