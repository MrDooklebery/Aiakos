#include "Arduino.h"
#ifndef ARDUINO_SAM_DUE
#include "cryptoauthlib.h"      //for ATSHA204A chip for TRNG & serial number
#endif
#include "debug.h"

//bool getSerialNumber(byte* bufout, byte length)
#if defined(ARDUINO_STM_NUCLEO_F103RB) || defined(ARDUINO_GENERIC_STM32F103C)
ATCAIfaceCfg *gCfg = &cfg_sha204a_i2c_default;
const byte POWER_PIN=PB7;

bool getSerialNumber(byte* bufout, byte length)
{
    byte buf[11];
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
    if(atcab_init( gCfg ) != ATCA_SUCCESS)
    {
        digitalWrite(POWER_PIN, LOW);
        return false;
    }
    if(atcab_read_serial_number(buf) != ATCA_SUCCESS)
    {
        digitalWrite(POWER_PIN, LOW);
        return false;
    }
    if(atcab_release() != ATCA_SUCCESS)
    {
        digitalWrite(POWER_PIN, LOW);
        return false;
    }
    memcpy(bufout, buf, length > 9 ? 9 : length);
    digitalWrite(POWER_PIN, LOW);
    return true;
}

#elif defined(ARDUINO_SAM_DUE)
bool getSerialNumber(byte* bufout, byte length)
{
    const byte FLASH_ACCESS_MODE_128 = 0;
    Efc* EFC = (Efc*)0x400E0A00U;

    typedef enum flash_rc {
        FLASH_RC_OK = 0,        //!< Operation OK
        FLASH_RC_YES = 1,       //!< Yes
        FLASH_RC_NO = 0,        //!< No
        FLASH_RC_ERROR = 0x10,  //!< General error
        FLASH_RC_INVALID,       //!< Invalid argument input
        FLASH_RC_NOT_SUPPORT = 0xFFFFFFFF    //!< Operation is not supported
    } flash_rc_t;
    uint32_t uid_buf[4];

    if (efc_init(EFC, FLASH_ACCESS_MODE_128, 4) != FLASH_RC_OK)
    {
        return false;
    }
    if (FLASH_RC_OK != efc_perform_read_sequence(EFC, EFC_FCMD_STUI, EFC_FCMD_SPUI, uid_buf, 4))
    {
        return false;
    }

    memcpy(bufout, uid_buf, length > 16 ? 16 : length);
    debug_print("Own id:");
    debug_printArray(bufout, length);
    return true;
}
#else
#error You must define a "getSerialNumber"-function
#endif

//int RNG(uint8_t *dest, unsigned size)
#if defined(ARDUINO_STM_NUCLEO_F103RB) || defined(ARDUINO_GENERIC_STM32F103C)
int RNG(byte *dest, unsigned size)
{
    byte randomnum[RANDOM_RSP_SIZE];
    byte defaultRng[4]={0xff, 0xff, 0x00, 0x00};
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
    if(atcab_init( gCfg ) != ATCA_SUCCESS)
    {
        digitalWrite(POWER_PIN, LOW);
        return 0;
    }
    while(size)
    {
        if(atcab_random( randomnum ) != ATCA_SUCCESS)
        {
            digitalWrite(POWER_PIN, LOW);
            return 0;
        }
        byte nrOfBytes = size > 32 ? 32 : size;
        memcpy(dest,randomnum, nrOfBytes);
        dest+=nrOfBytes;
        size-=nrOfBytes;
    }
    if(atcab_release() != ATCA_SUCCESS)
    {
        digitalWrite(POWER_PIN, LOW);
        return 0;
    }
    digitalWrite(POWER_PIN, LOW);
    return memcmp(randomnum, defaultRng, sizeof(defaultRng));
}
#elif defined(ARDUINO_SAM_DUE)

static byte rngEnabled=false;
void initRng()
{
    pmc_enable_periph_clk(ID_TRNG);
    trng_enable(TRNG);
    delay(10);
    rngEnabled=true;
}

int RNG(byte *dest, unsigned size)
{
    if(!rngEnabled)
    {
        debug_println("RNG not enabled.");
        return 0;
    }
    while (size)
    {
        uint32_t randomnum = trng_read_output_data(TRNG);
        byte nrOfBytes = size > 4 ? 4 : size;
        while (nrOfBytes--)
        {
            *dest++ = randomnum & 0xFF;
            randomnum >>= 8;
            size--;
        }
    }
    return 1;
}

#else

//TODO: replace by safe external RNG
int RNG(uint8_t *dest, unsigned size) {
    // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of
    // random noise). This can take a long time to generate random data if the result of analogRead(0)
    // doesn't change very frequently.
#ifdef ARDUINO_AVR_PROTRINKET3
    byte adcpin=0;
#elif defined(ARDUINO_SAM_DUE) || defined(ARDUINO_STM_NUCLEO_F103RB)
    byte adcpin=A0;
#elif defined(ARDUINO_GENERIC_STM32F103C)
    byte adcpin=PA1;
#endif
    while (size) {
        uint8_t val = 0;
        for (unsigned i = 0; i < 8; ++i) {
            int init = analogRead(adcpin);
            int count = 0;
            while (analogRead(adcpin) == init) {
                ++count;
            }

            if (count == 0) {
                val = (val << 1) | (init & 0x01);
            } else {
                val = (val << 1) | (count & 0x01);
            }
        }
        *dest = val;
        ++dest;
        --size;
    }
    // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
    return 1;
}
#endif
