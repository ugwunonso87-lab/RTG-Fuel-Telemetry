#define TINY_GSM_MODEM_SIM7600

// ==================================================
// BLYNK
// ==================================================

#define BLYNK_TEMPLATE_ID "TMPL2iDRkAs1a"
#define BLYNK_TEMPLATE_NAME "RTG Fuel Monitor"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// ==================================================
// LIBRARIES
// ==================================================

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#include <Preferences.h>

// ==================================================
// A7670G UART
// ==================================================

// GPIO4  = ESP32 TX -> A7670G RX
// GPIO16 = ESP32 RX <- A7670G TX

#define MODEM_TX 4
#define MODEM_RX 16

// ==================================================
// FUEL SENSOR
// ==================================================

#define SENSOR_PIN 34

// Current calibration
// 0 ADC    = sensor OFF / 0 litres
// 4094.391245 ADC = 2000 litres

const float ADC_EMPTY = 0.0f;
const float ADC_FULL  = 4094.391245f;

const int TANK_CAPACITY = 2000;
const int FUEL_CORRECTION = 209;

// ==================================================
// MODEM
// ==================================================

HardwareSerial SerialAT(1);

TinyGsm modem(SerialAT);

TinyGsmClient client(modem);

Preferences fuelStorage;

int lastValidFuelLitres = 0;
int lastValidFuelPercent = 0;

// ==================================================
// MTN APN
// ==================================================

const char apn[]  = "web.gprs.mtnnigeria.net";
const char user[] = "";
const char pass[] = "";

// ==================================================
// SMS NUMBER
// ==================================================

const char* PHONE_NUMBERS[] = {
    "+234**********",
    "+234**********"
};

const int NUM_RECIPIENTS = 2;

// ==================================================
// BLYNK TIMER
// ==================================================

// Send fuel data every 30 minutes
unsigned long previousBlynk = 0;

const unsigned long BLYNK_INTERVAL = 1800000UL;

// Blynk connection timeout
// IMPORTANT: do not name this BLYNK_TIMEOUT
// because Blynk library already uses that name.
const unsigned long MY_BLYNK_CONNECT_TIMEOUT = 30000UL;

// Allow Blynk time to process the queued writes
const unsigned long BLYNK_FLUSH_TIME = 3000UL;

// Retry after failed transmission
const unsigned long BLYNK_RETRY_DELAY = 10000UL;

// ==================================================
// CLOCK TIMER
// ==================================================

// Check network time every 60 seconds

unsigned long previousClockCheck = 0;

const unsigned long CLOCK_CHECK_INTERVAL = 60000UL;

// ==================================================
// SMS CONTROL
// ==================================================

// Prevent duplicate SMS during the same scheduled hour

int lastSMSDate = -1;
int lastSMSHour = -1;

// ==================================================
// STARTUP SMS REPORTS
// ==================================================

// First startup SMS: 30 minutes after power-up
// Second startup SMS: 90 minutes after power-up

unsigned long startupTime = 0;

const unsigned long STARTUP_SMS_1 = 1800000UL;
const unsigned long STARTUP_SMS_2 = 5400000UL;

bool startupSMS1Sent = false;
bool startupSMS2Sent = false;

// ==================================================
// FUEL VARIABLES
// ==================================================

int fuelLitres = 0;
int fuelPercent = 0;

// ==================================================
// LAST DISPLAYED TIME
// ==================================================

int lastDisplayedHour = -1;
int lastDisplayedMinute = -1;

// ==================================================
// MODEM STATUS
// ==================================================

bool modemInitialized = false;

// ==================================================
// LOW FUEL ALARM
// ==================================================

// Alert below 50%
const int LOW_FUEL_THRESHOLD = 50;

// Alert resets only when fuel rises to 55%
// This prevents repeated alerts around 50%.
const int LOW_FUEL_RESET = 55;

bool lowFuelAlertSent = false;

// ==================================================
// READ FUEL
// ==================================================

void readFuel()
{
    // Average 10 readings to reduce ADC noise

    const int NUM_SAMPLES = 10;

    long totalADC = 0;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        totalADC += analogRead(SENSOR_PIN);
        delay(10);
    }

    int adc = totalADC / NUM_SAMPLES;

    // --------------------------------------------------
    // SENSOR OFF
    // ADC = 0 means the fuel sensor is switched off.
    // Keep the last valid fuel value instead of changing it to 0.
    // --------------------------------------------------

    if (adc == 0)
    {
        fuelLitres = lastValidFuelLitres;
        fuelPercent = lastValidFuelPercent;

        Serial.println();
        Serial.println("----------------------------");
        Serial.println("FUEL SENSOR: OFF");
        Serial.println("Retaining last valid reading.");
        Serial.print("Fuel: ");
        Serial.print(fuelLitres);
        Serial.println(" L");
        Serial.print("Level: ");
        Serial.print(fuelPercent);
        Serial.println(" %");
        Serial.println("----------------------------");

        return;
    }

    // --------------------------------------------------
    // SENSOR ON - calculate current fuel level
    // --------------------------------------------------

    if (adc > ADC_FULL)
    {
        adc = (int)ADC_FULL;
    }

    // --------------------------------------------------
    // Convert ADC to litres
    // --------------------------------------------------

    // Calculate raw fuel level from ADC
    int rawFuelLitres =
        (int)round((adc * TANK_CAPACITY) / ADC_FULL);

    // Apply +209 L calibration correction before any output
    fuelLitres = rawFuelLitres + FUEL_CORRECTION;

    // Do not allow corrected value to exceed tank capacity
    fuelLitres = constrain(
        fuelLitres,
        0,
        TANK_CAPACITY
    );

    // --------------------------------------------------
    // Convert CORRECTED fuel level to percentage
    // --------------------------------------------------

    fuelPercent =
        (int)round((fuelLitres * 100.0f) / TANK_CAPACITY);

    fuelPercent = constrain(
        fuelPercent,
        0,
        100
    );

    // --------------------------------------------------
    // Save latest valid reading to non-volatile memory
    // --------------------------------------------------

    lastValidFuelLitres = fuelLitres;
    lastValidFuelPercent = fuelPercent;

    fuelStorage.putInt("litres", lastValidFuelLitres);
    fuelStorage.putInt("percent", lastValidFuelPercent);

    // --------------------------------------------------
    // Serial display
    // --------------------------------------------------

    Serial.println();
    Serial.println("----------------------------");

    Serial.print("ADC: ");
    Serial.println(adc);

    Serial.print("Fuel: ");
    Serial.print(fuelLitres);
    Serial.println(" L");

    Serial.print("Level: ");
    Serial.print(fuelPercent);
    Serial.println(" %");

    Serial.println("----------------------------");
}

// ==================================================
// SEND LOW FUEL ALERT
// ==================================================

void checkLowFuel()
{
    // --------------------------------------------------
    // FUEL BELOW 50%
    // --------------------------------------------------

    if (fuelPercent < LOW_FUEL_THRESHOLD)
    {
        Serial.println();
        Serial.println("================================");
        Serial.println("        LOW FUEL WARNING");
        Serial.println("================================");

        Serial.print("Fuel: ");
        Serial.print(fuelLitres);
        Serial.println(" L");

        Serial.print("Level: ");
        Serial.print(fuelPercent);
        Serial.println(" %");

        // Only send the alert once
        if (!lowFuelAlertSent)
        {
            if (Blynk.connected())
            {
                String message =
                    "RTG14 LOW FUEL: " +
                    String(fuelLitres) +
                    " L (" +
                    String(fuelPercent) +
                    "%)";

                Blynk.logEvent(
                    "low_fuel",
                    message
                );

                lowFuelAlertSent = true;

                Serial.println(
                    "LOW FUEL BLYNK ALERT SENT"
                );
            }
            else
            {
                Serial.println(
                    "BLYNK NOT CONNECTED - ALERT WILL RETRY"
                );
            }
        }
        else
        {
            Serial.println(
                "Low fuel alert already sent."
            );
        }

        Serial.println("================================");
    }

    // --------------------------------------------------
    // RESET LOW FUEL ALERT
    // --------------------------------------------------

    else if (fuelPercent >= LOW_FUEL_RESET)
    {
        if (lowFuelAlertSent)
        {
            Serial.println(
                "Fuel recovered above 55%."
            );

            Serial.println(
                "Low fuel alert reset."
            );
        }

        lowFuelAlertSent = false;
    }
}

// ==================================================
// SEND TO BLYNK
// ==================================================

bool sendToBlynk()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("Sending fuel data to Blynk...");
    Serial.println("==============================");

    readFuel();

    if (!Blynk.connected())
    {
        Serial.println(
            "Blynk not connected."
        );

        return false;
    }

    // --------------------------------------------------
    // V0 = litres
    // --------------------------------------------------

    Blynk.virtualWrite(
        V0,
        fuelLitres
    );

    // --------------------------------------------------
    // V1 = percentage
    // --------------------------------------------------

    Blynk.virtualWrite(
        V1,
        fuelPercent
    );

    // --------------------------------------------------
    // LOW FUEL ALERT
    // --------------------------------------------------

    checkLowFuel();

    Serial.println(
        "Fuel data queued for Blynk."
    );

    Serial.print("V0 = ");
    Serial.print(fuelLitres);
    Serial.println(" L");

    Serial.print("V1 = ");
    Serial.print(fuelPercent);
    Serial.println(" %");

    // --------------------------------------------------
    // Allow Blynk to process the transmission
    // --------------------------------------------------

    Serial.println(
        "Processing Blynk transmission..."
    );

    unsigned long flushStart = millis();

    while (
        millis() - flushStart <
        BLYNK_FLUSH_TIME
    )
    {
        Blynk.run();
        delay(10);
    }

    // --------------------------------------------------
    // Check connection is still alive
    // --------------------------------------------------

    if (!Blynk.connected())
    {
        Serial.println(
            "Blynk connection lost during transmission."
        );

        return false;
    }

    Serial.println(
        "Blynk transmission completed."
    );

    return true;
}

// ==================================================
// SEND SMS
// ==================================================

void sendFuelSMS()
{
    Serial.println();
    Serial.println("Preparing fuel SMS...");

    readFuel();

    String message =
        "RTG14 FUEL REPORT\n"
        "Fuel: " + String(fuelLitres) + " L\n"
        "Level: " + String(fuelPercent) + " %";

    Serial.println("SMS:");
    Serial.println(message);

    bool allSent = true;

    for (int i = 0; i < NUM_RECIPIENTS; i++)
    {
        Serial.print("Sending SMS to: ");
        Serial.println(PHONE_NUMBERS[i]);

        bool result = modem.sendSMS(
            PHONE_NUMBERS[i],
            message
        );

        if (result)
        {
            Serial.println(
                "SMS SENT SUCCESSFULLY"
            );
        }
        else
        {
            Serial.println(
                "SMS FAILED"
            );

            allSent = false;
        }

        delay(1000);
    }

    if (allSent)
    {
        Serial.println(
            "SMS SENT TO ALL RECIPIENTS"
        );
    }
    else
    {
        Serial.println(
            "ONE OR MORE SMS MESSAGES FAILED"
        );
    }
}

// ==================================================
// DAYS IN MONTH
// ==================================================

int daysInMonth(
    int year,
    int month
)
{
    if (
        month == 1 ||
        month == 3 ||
        month == 5 ||
        month == 7 ||
        month == 8 ||
        month == 10 ||
        month == 12
    )
    {
        return 31;
    }

    if (
        month == 4 ||
        month == 6 ||
        month == 9 ||
        month == 11
    )
    {
        return 30;
    }

    bool leap =
        ((year % 4 == 0 && year % 100 != 0) ||
         (year % 400 == 0));

    return leap ? 29 : 28;
}

// ==================================================
// CHECK NETWORK TIME
// ==================================================

void checkScheduledSMS()
{
    if (!modem.isNetworkConnected())
    {
        return;
    }

    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    float timezone;

    if (!modem.getNetworkTime(
            &year,
            &month,
            &day,
            &hour,
            &minute,
            &second,
            &timezone))
    {
        Serial.println(
            "Network time unavailable."
        );

        return;
    }

    // ==================================================
    // CONVERT TO NIGERIA TIME
    // Nigeria = UTC+1
    // ==================================================

    int nigeriaMinutes =
        (hour * 60) +
        minute +
        (int)round(
            (1.0 - timezone) * 60.0
        );

    // ==================================================
    // PREVIOUS DAY
    // ==================================================

    if (nigeriaMinutes < 0)
    {
        nigeriaMinutes += 1440;

        day--;

        if (day < 1)
        {
            month--;

            if (month < 1)
            {
                month = 12;
                year--;
            }

            day = daysInMonth(
                year,
                month
            );
        }
    }

    // ==================================================
    // NEXT DAY
    // ==================================================

    if (nigeriaMinutes >= 1440)
    {
        nigeriaMinutes -= 1440;

        day++;

        if (
            day >
            daysInMonth(
                year,
                month
            )
        )
        {
            day = 1;

            month++;

            if (month > 12)
            {
                month = 1;
                year++;
            }
        }
    }

    int nigeriaHour =
        nigeriaMinutes / 60;

    int nigeriaMinute =
        nigeriaMinutes % 60;

    // ==================================================
    // PRINT TIME ONLY WHEN MINUTE CHANGES
    // ==================================================

    if (
        nigeriaHour != lastDisplayedHour ||
        nigeriaMinute != lastDisplayedMinute
    )
    {
        Serial.print(
            "Nigeria Time: "
        );

        if (nigeriaHour < 10)
        {
            Serial.print("0");
        }

        Serial.print(nigeriaHour);

        Serial.print(":");

        if (nigeriaMinute < 10)
        {
            Serial.print("0");
        }

        Serial.println(nigeriaMinute);

        lastDisplayedHour =
            nigeriaHour;

        lastDisplayedMinute =
            nigeriaMinute;
    }

    // ==================================================
    // UNIQUE DATE
    // ==================================================

    int dateKey =
        (year * 10000) +
        (month * 100) +
        day;

    // ==================================================
    // 04:00 SMS
    // ==================================================

    if (
        nigeriaHour == 4 &&
        nigeriaMinute == 0
    )
    {
        if (
            lastSMSDate != dateKey ||
            lastSMSHour != 4
        )
        {
            Serial.println();
            Serial.println(
                "===== 04:00 SMS ====="
            );

            sendFuelSMS();

            lastSMSDate = dateKey;
            lastSMSHour = 4;
        }
    }

    // ==================================================
    // 05:00 SMS
    // ==================================================

    if (
        nigeriaHour == 5 &&
        nigeriaMinute == 0
    )
    {
        if (
            lastSMSDate != dateKey ||
            lastSMSHour != 5
        )
        {
            Serial.println();
            Serial.println(
                "===== 05:00 SMS ====="
            );

            sendFuelSMS();

            lastSMSDate = dateKey;
            lastSMSHour = 5;
        }
    }
}

// ==================================================
// CHECK STARTUP SMS
// ==================================================

void checkStartupSMS()
{
    unsigned long elapsed =
        millis() - startupTime;

    // ==================================================
    // FIRST SMS: 30 MINUTES AFTER POWER-UP
    // ==================================================

    if (
        !startupSMS1Sent &&
        elapsed >= STARTUP_SMS_1
    )
    {
        Serial.println();
        Serial.println(
            "===== STARTUP SMS 1: 30 MINUTES ====="
        );

        sendFuelSMS();

        startupSMS1Sent = true;

        Serial.println(
            "Startup SMS 1 completed."
        );
    }

    // ==================================================
    // SECOND SMS: 90 MINUTES AFTER POWER-UP
    // ==================================================

    if (
        !startupSMS2Sent &&
        elapsed >= STARTUP_SMS_2
    )
    {
        Serial.println();
        Serial.println(
            "===== STARTUP SMS 2: 90 MINUTES ====="
        );

        sendFuelSMS();

        startupSMS2Sent = true;

        Serial.println(
            "Startup SMS 2 completed."
        );
    }
}

// ==================================================
// INITIALIZE MODEM
// ==================================================
//
// IMPORTANT:
// The modem is restarted ONLY ONCE at startup.
// It is NOT restarted every 30 minutes.
// ==================================================

bool initializeModem()
{
    if (modemInitialized)
    {
        return true;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("INITIALIZING A7670G");
    Serial.println("==============================");

    Serial.println(
        "Restarting modem..."
    );

    if (!modem.restart())
    {
        Serial.println(
            "Modem restart FAILED"
        );

        return false;
    }

    Serial.println(
        "Modem OK"
    );

    Serial.print(
        "Modem Info: "
    );

    Serial.println(
        modem.getModemInfo()
    );

    Serial.print(
        "SIM Status: "
    );

    Serial.println(
        modem.getSimStatus()
    );

    modemInitialized = true;

    return true;
}

// ==================================================
// CONNECT CELLULAR DATA
// ==================================================

bool connectCellular()
{
    if (!initializeModem())
    {
        return false;
    }

    Serial.println();
    Serial.println(
        "Checking MTN network..."
    );

    if (!modem.isNetworkConnected())
    {
        Serial.println(
            "Waiting for network..."
        );

        if (!modem.waitForNetwork(60000L))
        {
            Serial.println(
                "NETWORK FAILED"
            );

            return false;
        }

        Serial.println(
            "NETWORK FOUND"
        );
    }
    else
    {
        Serial.println(
            "NETWORK ALREADY CONNECTED"
        );
    }

    Serial.print(
        "Operator: "
    );

    Serial.println(
        modem.getOperator()
    );

    Serial.print(
        "Signal: "
    );

    Serial.println(
        modem.getSignalQuality()
    );

    // ==================================================
    // CONNECT MTN DATA
    // ==================================================

    if (modem.isGprsConnected())
    {
        Serial.println(
            "GPRS ALREADY CONNECTED"
        );

        return true;
    }

    Serial.println(
        "Connecting MTN data..."
    );

    if (!modem.gprsConnect(
            apn,
            user,
            pass))
    {
        Serial.println(
            "GPRS FAILED"
        );

        return false;
    }

    Serial.println(
        "GPRS CONNECTED"
    );

    Serial.print(
        "IP: "
    );

    Serial.println(
        modem.localIP()
    );

    return true;
}

// ==================================================
// CONNECT BLYNK
// ==================================================

bool connectBlynk()
{
    Serial.println();
    Serial.println(
        "Connecting to Blynk..."
    );

    if (!modem.isGprsConnected())
    {
        Serial.println(
            "GPRS not connected."
        );

        return false;
    }

    Blynk.config(
        modem,
        BLYNK_AUTH_TOKEN
    );

    if (
        Blynk.connect(
            MY_BLYNK_CONNECT_TIMEOUT
        )
    )
    {
        Serial.println(
            "BLYNK CONNECTED"
        );

        return true;
    }

    Serial.println(
        "BLYNK CONNECTION FAILED"
    );

    return false;
}

// ==================================================
// DISCONNECT BLYNK + GPRS
// ==================================================

void disconnectBlynkAndGPRS()
{
    Serial.println();

    // ==================================================
    // DISCONNECT BLYNK
    // ==================================================

    if (Blynk.connected())
    {
        Serial.println(
            "Disconnecting Blynk..."
        );

        Blynk.disconnect();

        delay(300);
    }

    // ==================================================
    // DISCONNECT GPRS
    // ==================================================

    if (modem.isGprsConnected())
    {
        Serial.println(
            "Disconnecting MTN GPRS..."
        );

        modem.gprsDisconnect();

        delay(300);
    }

    Serial.println(
        "BLYNK + GPRS DISCONNECTED"
    );
}

// ==================================================
// COMPLETE BLYNK REPORT
// ==================================================

bool performBlynkReport()
{
    Serial.println();
    Serial.println();
    Serial.println(
        "################################"
    );

    Serial.println(
        "       RTG14 BLYNK REPORT"
    );

    Serial.println(
        "################################"
    );

    // ==================================================
    // READ FUEL
    // ==================================================

    readFuel();

    // ==================================================
    // CONNECT MTN
    // ==================================================

    if (!connectCellular())
    {
        Serial.println(
            "BLYNK REPORT FAILED: GPRS"
        );

        return false;
    }

    // ==================================================
    // CONNECT BLYNK
    // ==================================================

    if (!connectBlynk())
    {
        Serial.println(
            "BLYNK REPORT FAILED: BLYNK"
        );

        disconnectBlynkAndGPRS();

        return false;
    }

    // ==================================================
    // SEND DATA
    // ==================================================

    bool success =
        sendToBlynk();

    // ==================================================
    // DISCONNECT
    // ==================================================

    disconnectBlynkAndGPRS();

    if (success)
    {
        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.println(
            "RTG14 BLYNK REPORT SUCCESSFUL"
        );

        Serial.println(
            "================================"
        );

        return true;
    }

    Serial.println();
    Serial.println(
        "RTG14 BLYNK REPORT FAILED"
    );

    return false;
}

// ==================================================
// SETUP
// ==================================================

void setup()
{
    Serial.begin(115200);

    delay(3000);

    Serial.println();
    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "          RTG FUEL MONITOR"
    );

    Serial.println(
        "========================================"
    );

    Serial.println(
        "ESP32 + A7670G"
    );

    Serial.println(
        "Fuel Sensor: GPIO34"
    );

    Serial.println(
        "ESP32 TX: GPIO4"
    );

    Serial.println(
        "ESP32 RX: GPIO16"
    );

    Serial.println(
        "Blynk report: Every 30 minutes"
    );

    Serial.println(
        "Low fuel alarm: Below 50%"
    );

    Serial.println(
        "========================================"
    );

    // ==================================================
    // ADC
    // ==================================================

    analogReadResolution(12);

    analogSetPinAttenuation(
        SENSOR_PIN,
        ADC_11db
    );

    Serial.println(
        "ADC configured."
    );

    // ==================================================
    // LOAD LAST SAVED FUEL VALUE
    // ==================================================

    fuelStorage.begin("fuel", false);

    lastValidFuelLitres = fuelStorage.getInt("litres", 0);
    lastValidFuelPercent = fuelStorage.getInt("percent", 0);

    Serial.println("Last saved fuel loaded.");
    Serial.print("Fuel: ");
    Serial.print(lastValidFuelLitres);
    Serial.println(" L");
    Serial.print("Level: ");
    Serial.print(lastValidFuelPercent);
    Serial.println(" %");

    // ==================================================
    // A7670G UART
    // ==================================================

    SerialAT.begin(
        115200,
        SERIAL_8N1,
        MODEM_RX,
        MODEM_TX
    );

    Serial.println(
        "A7670G UART configured."
    );

    // ==================================================
    // INITIALIZE MODEM
    // ==================================================

    if (!initializeModem())
    {
        Serial.println(
            "MODEM INITIALIZATION FAILED."
        );

        while (true)
        {
            delay(1000);
        }
    }

    // ==================================================
    // WAIT FOR NETWORK
    // ==================================================

    Serial.println(
        "Waiting for cellular network..."
    );

    if (!modem.waitForNetwork(60000L))
    {
        Serial.println(
            "CELLULAR NETWORK FAILED."
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "CELLULAR NETWORK READY."
    );

    // ==================================================
    // INITIAL BLYNK REPORT
    // ==================================================

    Serial.println();
    Serial.println(
        "Performing initial Blynk report..."
    );

    bool initialReport =
        performBlynkReport();

    if (initialReport)
    {
        Serial.println(
            "Initial Blynk report successful."
        );
    }
    else
    {
        Serial.println(
            "Initial Blynk report failed."
        );
    }

    // ==================================================
    // INITIAL FUEL READING
    // ==================================================

    Serial.println();
    Serial.println(
        "Initial fuel reading:"
    );

    readFuel();

    // ==================================================
    // READY
    // ==================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "SYSTEM READY"
    );

    Serial.println(
        "========================================"
    );

    Serial.println(
        "Blynk: Every 30 minutes"
    );

    Serial.println(
        "Blynk: Connect -> Upload -> Disconnect"
    );

    Serial.println(
        "SMS: 04:00 and 05:00"
    );

    Serial.println(
        "Startup SMS: 30 and 90 minutes"
    );

    Serial.println(
        "Fuel: Integer litres"
    );

    Serial.println(
        "Level: Integer %"
    );

    Serial.println(
        "Low fuel alert: <50%"
    );

    Serial.println(
        "Alert reset: >=55%"
    );

    Serial.println(
        "========================================"
    );

    // ==================================================
    // STARTUP SMS TIMER
    // ==================================================

    startupTime = millis();

    Serial.println();
    Serial.println(
        "STARTUP SMS SCHEDULE"
    );

    Serial.println(
        "SMS 1: 30 minutes after power-up"
    );

    Serial.println(
        "SMS 2: 90 minutes after power-up"
    );

    Serial.println(
        "========================================"
    );

    // ==================================================
    // START 30-MINUTE BLYNK TIMER
    // ==================================================

    previousBlynk = millis();

    // Clock timer
    previousClockCheck = millis();
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    // ==================================================
    // CURRENT TIME
    // ==================================================

    unsigned long currentMillis =
        millis();

    // ==================================================
    // CHECK NETWORK
    // ==================================================

    if (!modem.isNetworkConnected())
    {
        Serial.println();
        Serial.println(
            "NETWORK LOST"
        );

        if (
            modem.waitForNetwork(60000L)
        )
        {
            Serial.println(
                "NETWORK RECONNECTED"
            );
        }
    }

    // ==================================================
    // BLYNK REPORT EVERY 30 MINUTES
    // ==================================================

    if (
        currentMillis -
        previousBlynk >=
        BLYNK_INTERVAL
    )
    {
        previousBlynk =
            currentMillis;

        // ----------------------------------------------
        // First attempt
        // ----------------------------------------------

        bool success =
            performBlynkReport();

        // ----------------------------------------------
        // Retry once if failed
        // ----------------------------------------------

        if (!success)
        {
            Serial.println();
            Serial.println(
                "Blynk report failed."
            );

            Serial.println(
                "Waiting 10 seconds before retry..."
            );

            delay(BLYNK_RETRY_DELAY);

            Serial.println(
                "RETRYING BLYNK REPORT..."
            );

            success =
                performBlynkReport();

            if (success)
            {
                Serial.println(
                    "Blynk retry successful."
                );
            }
            else
            {
                Serial.println(
                    "Blynk retry failed."
                );

                Serial.println(
                    "Next scheduled report in 30 minutes."
                );
            }
        }
    }

    // ==================================================
    // CHECK CLOCK EVERY 1 MINUTE
    // ==================================================

    if (
        currentMillis -
        previousClockCheck >=
        CLOCK_CHECK_INTERVAL
    )
    {
        previousClockCheck =
            currentMillis;

        checkScheduledSMS();
    }

    // ==================================================
    // STARTUP SMS CHECK
    // ==================================================

    checkStartupSMS();

    // ==================================================
    // SMALL LOOP DELAY
    // ==================================================

    delay(100);
}