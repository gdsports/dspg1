/*
   MIT License

   Copyright (c) 2018 gdsports625@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   */

#include <MIDI.h>       // MIDI Library by Forty Seven Effects
#include <usbh_midi.h>  // https://github.com/gdsports/USB_Host_Library_SAMD

/* 1 turns on debug, 0 off */
#define DBGSERIAL if (0) SERIAL_PORT_MONITOR

/* USB Vendor ID and Product ID for Korg nanoKontrol2 */
#define VID_KORG    (0x0944)
#define PID_KONTROL (0x0117)

USBHost UsbH;
USBH_MIDI MIDIUSBH(&UsbH);

#define MIDI_SERIAL_PORT Serial1

struct MySettings : public midi::DefaultSettings
{
  static const bool Use1ByteParsing = false;
  static const unsigned SysExMaxSize = 1026; // Accept SysEx messages up to 1024 bytes long.
  static const long BaudRate = 31250;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, MIDI_SERIAL_PORT, MIDIUART, MySettings);

/* CC numbers for Korg nanoKontrol */
#define FADER1      (0)
#define FADER2      (1)
#define FADER3      (2)
#define FADER4      (3)
#define FADER5      (4)
#define FADER6      (5)
#define FADER7      (6)
#define FADER8      (7)

#define KNOB1       (16)
#define KNOB2       (17)
#define KNOB3       (18)
#define KNOB4       (19)
#define KNOB5       (20)
#define KNOB6       (21)
#define KNOB7       (22)
#define KNOB8       (23)

#define BUTTON_S3   (34)
#define BUTTON_R3   (66)

/* CC numbers for DSP-G1 */
#define DCO_WAVEFORM    (76)
#define DCO_WRAP        ( 4)
#define DCO_RANGE       (21)
#define DCO_DETUNE      (93)

#define LFO_RATE        (16)
#define LFO_WAVEFORM    (20)

#define DCF_CUTOFF      (74)
#define DCF_RESONANCE   (71)
#define DCF_ATTACK      (82)
#define DCF_DECAY       (83)
#define DCF_SUSTAIN     (28)
#define DCF_RELEASE     (29)
#define DCF_MODULATION  (81)

#define DCA_ATTACK      (73)
#define DCA_DECAY       (75)
#define DCA_SUSTAIN     (31)
#define DCA_RELEASE     (72)

/* Map Korg CC numbers to DSP-G1 CC numbers */
void cc_map(uint8_t *ccmsg)
{
  switch (ccmsg[1]) {
    case FADER1: ccmsg[1] = DCO_RANGE;      break;
    case FADER2: ccmsg[1] = DCO_DETUNE;     break;
    case FADER3: ccmsg[1] = DCF_MODULATION; break;
    case FADER4: ccmsg[1] = DCF_DECAY;      break;
    case FADER5: ccmsg[1] = DCF_RELEASE;    break;
    case FADER6: ccmsg[1] = DCF_RESONANCE;  break;
    case FADER7: ccmsg[1] = DCA_DECAY;      break;
    case FADER8: ccmsg[1] = DCA_RELEASE;    break;
    case KNOB1:  ccmsg[1] = DCO_WAVEFORM;   break;
    case KNOB2:  ccmsg[1] = DCO_WRAP;       break;
    case KNOB3:  ccmsg[1] = LFO_RATE;       break;
    case KNOB4:  ccmsg[1] = DCF_ATTACK;     break;
    case KNOB5:  ccmsg[1] = DCF_SUSTAIN;    break;
    case KNOB6:  ccmsg[1] = DCF_CUTOFF;     break;
    case KNOB7:  ccmsg[1] = DCA_ATTACK;     break;
    case KNOB8:  ccmsg[1] = DCA_SUSTAIN;    break;
    case BUTTON_S3:
                 if (ccmsg[2] == 127) {
                   ccmsg[1] = LFO_WAVEFORM;
                   ccmsg[2] = 127;
                 }
                 break;
    case BUTTON_R3:
                 if (ccmsg[2] == 127) {
                   ccmsg[1] = LFO_WAVEFORM;
                   ccmsg[2] =  0;
                 }
                 break;
    default: break;
  }
}

inline uint8_t writeUARTwait(uint8_t *p, uint16_t size)
{
  // Apparently, not needed. write blocks, if needed
  //  while (MIDI_SERIAL_PORT.availableForWrite() < size) {
  //    delay(1);
  //  }
  return MIDI_SERIAL_PORT.write(p, size);
}

uint16_t sysexSize = 0;

void sysex_end(uint8_t i)
{
  sysexSize += i;
  DBGSERIAL.print(F("sysexSize="));
  DBGSERIAL.println(sysexSize);
  sysexSize = 0;
}


void setup() {
  DBGSERIAL.begin(115200);

  /* MIDI thru defaults ON */
  MIDIUART.begin(MIDI_CHANNEL_OMNI);

  if (UsbH.Init()) {
    DBGSERIAL.println(F("USB host failed to start"));
    while (1) delay(1); // halt
  }
}

void USBHost_to_UART()
{
  uint8_t recvBuf[MIDI_EVENT_PACKET_SIZE];
  uint8_t rcode = 0;     //return code
  uint16_t rcvd;
  uint8_t readCount = 0;

  rcode = MIDIUSBH.RecvData( &rcvd, recvBuf);

  if (rcode != 0 || rcvd == 0) return;
  if ( recvBuf[0] == 0 && recvBuf[1] == 0 && recvBuf[2] == 0 && recvBuf[3] == 0 ) {
    return;
  }

  uint8_t *p = recvBuf;
  while (readCount < rcvd)  {
    if (*p == 0 && *(p + 1) == 0) break; //data end
    DBGSERIAL.print(F("USB "));
    DBGSERIAL.print(p[0], DEC);
    DBGSERIAL.print(' ');
    DBGSERIAL.print(p[1], DEC);
    DBGSERIAL.print(' ');
    DBGSERIAL.print(p[2], DEC);
    DBGSERIAL.print(' ');
    DBGSERIAL.println(p[3], DEC);
    uint8_t header = *p & 0x0F;
    p++;
    switch (header) {
      case 0x00:  // Misc. Reserved for future extensions.
        break;
      case 0x01:  // Cable events. Reserved for future expansion.
        break;
      case 0x02:  // Two-byte System Common messages
      case 0x0C:  // Program Change
      case 0x0D:  // Channel Pressure
        writeUARTwait(p, 2);
        break;
      case 0x03:  // Three-byte System Common messages
      case 0x08:  // Note-off
      case 0x09:  // Note-on
      case 0x0A:  // Poly-KeyPress
      case 0x0E:  // PitchBend Change
        writeUARTwait(p, 3);
        break;
      case 0x0B:  // Control Change
        if ((MIDIUSBH.idVendor() == VID_KORG) && (MIDIUSBH.idProduct() == PID_KONTROL)) {
          cc_map(p);
        }
        writeUARTwait(p, 3);
        DBGSERIAL.print(F("CC "));
        DBGSERIAL.print(p[0], DEC);
        DBGSERIAL.print(' ');
        DBGSERIAL.print(p[1], DEC);
        DBGSERIAL.print(' ');
        DBGSERIAL.println(p[2], DEC);
        break;

      case 0x04:  // SysEx starts or continues
        sysexSize += 3;
        writeUARTwait(p, 3);
        break;
      case 0x05:  // Single-byte System Common Message or SysEx ends with the following single byte
        sysex_end(1);
        writeUARTwait(p, 1);
        break;
      case 0x06:  // SysEx ends with the following two bytes
        sysex_end(2);
        writeUARTwait(p, 2);
        break;
      case 0x07:  // SysEx ends with the following three bytes
        sysex_end(3);
        writeUARTwait(p, 3);
        break;
      case 0x0F:  // Single Byte, TuneRequest, Clock, Start, Continue, Stop, etc.
        writeUARTwait(p, 1);
        break;
    }
    p += 3;
    readCount += 4;
  }
}

void loop()
{
  UsbH.Task();

  if (MIDIUSBH) {
    /* MIDI UART Rx -> discard but MIDI thru works automatically */
    MIDIUART.read();

    /* MIDI USB Host -> MIDI UART Tx */
    USBHost_to_UART();
  }
}
