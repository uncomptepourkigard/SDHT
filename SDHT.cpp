#include "wiring_digital.c"
#include "SDHT.h"

uint8_t SDHT::broadcast(uint8_t pin, uint8_t model) {
  if (model > DHT22) _notice = SDHT_ERROR_MODEL;
  else if ((_port = digitalPinToPort(_pin = pin)) == NOT_A_PIN) _notice = SDHT_ERROR_PIN;
  else if ((_notice = read((model < 2) ? 20 : 1)) == SDHT_LOADED) {

    switch (model) {
      case DHT11:
        _humidity = data[0] + (data[1] * .1);
        _temperature.setCelsius(data[2] + (data[3] * .1));
        break;

      case DHT12:
        _humidity = data[0] + (data[1] * .1);
        _temperature.setCelsius((data[2] + ((data[3] & 0x7F) * .1)) * ((data[3] & 0x80) ? -1 : 1));
        break;

      case DHT21:
      case DHT22:
        _humidity = word(data[0], data[1]) * .1;
        _temperature.setCelsius(word((data[2] & 0x7F), data[3]) * ((data[2] & 0x80) ? -.1 : .1));
        break;
    }

    double heatIndex = (0.5 * (_temperature.fahrenheit + 61.0 + ((_temperature.fahrenheit - 68.0) * 1.2) + (_humidity * 0.094)));

    if (heatIndex > 79) {
      heatIndex =
        - 42.37900000
        + 02.04901523 * _temperature.fahrenheit
        + 10.14333127 * _humidity
        - 00.22475541 * _temperature.fahrenheit * _humidity
        - 00.00683783 * pow(_temperature.fahrenheit, 2)
        - 00.05481717 * pow(_humidity, 2)
        + 00.00122874 * pow(_temperature.fahrenheit, 2) * _humidity
        + 00.00085282 * _temperature.fahrenheit * pow(_humidity, 2)
        - 00.00000199 * pow(_temperature.fahrenheit, 2) * pow(_humidity, 2)
        ;

      if ((_humidity < 13.0) && (_temperature.fahrenheit >= 80.0) && (_temperature.fahrenheit <= 112.0))
        heatIndex -= ((13.0 - _humidity) * 0.25) * sqrt((17.0 - abs(_temperature.fahrenheit - 95.0)) * 0.05882);
      else if ((_humidity > 85.0) && (_temperature.fahrenheit >= 80.0) && (_temperature.fahrenheit <= 87.0))
        heatIndex += ((_humidity - 85.0) * 0.1) * ((87.0 - _temperature.fahrenheit) * 0.2);
    }
    _heat.setFahrenheit(heatIndex);
    _notice = ((data[4] == uint8_t(data[0] + data[1] + data[2] + data[3])) ? SDHT_OK : SDHT_WRONG_PARITY);
  }
  return _notice;
}

uint8_t SDHT::read(uint16_t msDelay)
{
  int buffer;

#if defined(ESP8266)
  yield();
#endif

  noInterrupts();

  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  delay(msDelay);
  digitalWrite(_pin, HIGH);
  pinMode(_pin, INPUT);

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;
  _bit = digitalPinToBitMask(_pin);

  if (pulse(_bit)) return SDHT_ERROR_CONNECT;
  if (pulse(0)) return SDHT_ERROR_REQUEST;
  if (pulse(_bit)) return SDHT_ERROR_RESPONSE;

  for (int i = 0; i < 40; i++) {
    if (pulse(0)) return SDHT_ERROR_WAIT;
    buffer = _signal;
    if (pulse(_bit) < 0) return SDHT_ERROR_VALUE;
    data[i / 8] += data[i / 8] + (_signal > buffer);
  }

  interrupts();
  return SDHT_LOADED;
}

bool SDHT::pulse(uint8_t b) {
  _signal = 0;
  while ((*portInputRegister(_port) & _bit) == b) if (SDHT_CYCLES < _signal++) return true;
  return (_signal == 0);
}