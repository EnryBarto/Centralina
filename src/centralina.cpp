/**
 * Autore:  Bartocetti Enrico
 * Data: 11/05/2022
 * Versione: 3.0
**/


/*-------------------- IMPORTAZIONE LIBRERIE --------------------*/

#include <EEPROM.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <LiquidCrystal_I2C.h>
//#define DEBUG yes // Togliere il commento per avere sulla seriale: Tempo Ritardo, Tempo iniezione ON, Tempo iniezione OFF


/*-------------------- ASSEGNAZIONE DEI PIN --------------------*/

#define RELE 27           // Pin del relé
#define BUTTON1 34        // Pin del pulsante MENU
#define BUTTON2 35        // Pin del pulsante +
#define BUTTON3 25        // Pin del pulsante -
#define BUTTON4 26        // Pin del pulsante INVIO
#define PRESSURE_SENSOR 4 // Pin del sensore della pressione


/*-------------------- STATI DEL PROGRAMMA --------------------*/

typedef enum {
    HOME,
    INIETTORE_ON,
    ATTESA_ON,
    INIETTORE_OFF,
    ATTESA_OFF,
    VOCE_RESET_P_MAX,
    VOCE_MODIFICA_TEMPI,
    MODIFICA_OFF,
    MODIFICA_ON,
    CONFERMA_MODIFICA,
    VOCE_SALVA_TEMPI,
    CONFERMA_SALVATAGGIO,
    VOCE_ACCENDI_BLUETOOTH,
    VOCE_SPEGNI_BLUETOOTH,
    VOCE_RIEPILOGO,
    MOSTRA_RIEPILOGO
} Stati;


/*-------------------- DEFINIZIONE COSTANTI --------------------*/

const int T_MIN = 25;   // Tempo minimo assegnabile
const int OFFSET = 25;  // Offset durante la modifica dei tempi
const int NUM_PRESSIONI = 4;    // Numero di pressioni soglia
const float PRESS[NUM_PRESSIONI+1] = {  // Pressioni soglia
    0.009,
    0.1,
    0.2,
    0.3,
    9.9
};


/*-------------------- DEFINIZIONE VARIABILI --------------------*/

bool statoBT = false;            // False: Bluetooth spento, True: Bluetooth acceso
bool schermo_aggiornato = false; // Flag per aggiornare lo schermo solo quando necessario
float pressione;                 // Pressione letta dal sensore
float pressione_max;             // Pressione massima raggiunta
byte indice_soglia;              // Indice per la modifica / visualizzazione di una certa soglia
uint32_t tempi[NUM_PRESSIONI][2];// Matrice che contiene i tempi di iniezione. Riga 0: tempi off, Riga 1: tempi on
uint32_t appoggio_t_on;          // Appoggio usato durante la modifica
uint32_t appoggio_t_off;         // Appoggio usato durante la modifica
uint64_t inizio_iniezione;       // Istante in cui inizia l'iniezione

String messaggioBT;              // Messaggio letto da BluetoothSerial
Stati stato_attuale;             // Stato attuale del programma
BluetoothSerial serialBT;        // Oggetto per la seriale via Bluetooth
LiquidCrystal_I2C lcd(0x27, 16, 2);// Creazione dell'oggetto LCD


/*------------------ PROTOTIPI DELLE FUNZIONI ------------------*/

void leggiSensorePressione();   // Legge la pressione dal sensore

void visualizzaPressione();                          // Visualizza la pressione nella pagina iniziale
void visualizzaOpzioniMenu(String sx, String dx);    // Visualizza le opzioni disponibili per i pulsanti esterni
void visualizzaOpzioniModifica(String sx, String dx);// Visualizza le opzioni disponibili per gli ultimi 3 pulsanti

void aumentaTempo(uint32_t *tempo);     // Aumenta il tempo di iniezione
void diminuisciTempo(uint32_t *tempo);  // Diminuisce il tempo di iniezione

uint32_t leggiEeprom(uint8_t i);               // Legge la EEPROM
void scriviEeprom(uint32_t valore, uint8_t i); // Sovrascrive la EEPROM


/*------------- CREAZIONE CARATTERI PER SCHERMO LCD -------------*/

byte freccia_giu[8] = {
  0b00000,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
};

byte freccia_angolo_destra[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b10000,
  0b01001,
  0b00101,
  0b00011,
  0b01111,
};

byte freccia_angolo_sinistra[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00001,
  0b10010,
  0b10100,
  0b11000,
  0b11110,
};

byte simbolo_bt[8] = {
  0b00110,
  0b10101,
  0b01110,
  0b00100,
  0b01110,
  0b10101,
  0b00110,
  0b00100,
};


void setup() {

    #ifdef DEBUG
    Serial.begin(9600);
    #endif

    // INIZIALIZZAZIONE INPUT / OUTPUT
    pinMode(PRESSURE_SENSOR, INPUT);
    pinMode(BUTTON1, INPUT);
    pinMode(BUTTON2, INPUT);
    pinMode(BUTTON3, INPUT);
    pinMode(BUTTON4, INPUT);
    pinMode(RELE, OUTPUT); 

    // LETTURA DEI TEMPI DALLA EEPROM
    EEPROM.begin(512);  

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < NUM_PRESSIONI; j++) {
            tempi[j][i] = leggiEeprom(j + (i*4));
        }
    }

    // INIZIALIZZAZIONE DELL'LCD
    lcd.init();
    lcd.backlight();
    lcd.createChar(0, freccia_giu);
    lcd.createChar(1, freccia_angolo_destra);
    lcd.createChar(2, freccia_angolo_sinistra);
    lcd.createChar(3, simbolo_bt);
    lcd.setCursor(0, 0);
    lcd.print(" ACCENSIONE 3.0 ");
    for (int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.print("-");
        delay(150);
    }

    leggiSensorePressione();
    pressione_max = pressione;

    stato_attuale = HOME;
}

void loop() {
    switch (stato_attuale) {
        case HOME:
            leggiSensorePressione();
            visualizzaPressione();

            if (pressione >= PRESS[0]) {
                lcd.setCursor(0,1);
                lcd.print("Iniezione attiva");
                stato_attuale = INIETTORE_ON;

            } else if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                stato_attuale = VOCE_RESET_P_MAX;
            }
            break;
        
        case INIETTORE_ON:
            digitalWrite(RELE, HIGH);
            inizio_iniezione = millis();
            stato_attuale = ATTESA_ON;
            break;
        
        case ATTESA_ON:
            leggiSensorePressione();
            lcd.home();
            lcd.printf("Pressione: %1.2f ", pressione);

            if (pressione >= PRESS[3]) {
                if (millis() - inizio_iniezione >= tempi[3][1]) stato_attuale = INIETTORE_OFF;
            } else if (pressione >= PRESS[2]) {
                if (millis() - inizio_iniezione >= tempi[2][1]) stato_attuale = INIETTORE_OFF;
            } else if (pressione >= PRESS[1]) {
                if (millis() - inizio_iniezione >= tempi[1][1]) stato_attuale = INIETTORE_OFF;
            } else if (pressione >= PRESS[0]) {
                if (millis() - inizio_iniezione >= tempi[0][1]) stato_attuale = INIETTORE_OFF;
            } else {
                digitalWrite(RELE, LOW);
                stato_attuale = HOME;
            }

            break;
        
        case INIETTORE_OFF:
            digitalWrite(RELE, LOW);
            inizio_iniezione = millis();
            stato_attuale = ATTESA_OFF;
            break;

        case ATTESA_OFF:
            leggiSensorePressione();
            lcd.home();
            lcd.printf("Pressione: %1.2f ", pressione);

            if (pressione >= PRESS[3]) {
                if (millis() - inizio_iniezione >= tempi[3][0]) stato_attuale = INIETTORE_ON;
            } else if (pressione >= PRESS[2]) {
                if (millis() - inizio_iniezione >= tempi[2][0]) stato_attuale = INIETTORE_ON;
            } else if (pressione >= PRESS[1]) {
                if (millis() - inizio_iniezione >= tempi[1][0]) stato_attuale = INIETTORE_ON;
            } else if (pressione >= PRESS[0]) {
                if (millis() - inizio_iniezione >= tempi[0][0]) stato_attuale = INIETTORE_ON;
            } else {
                stato_attuale = HOME;
            }

            break;

        case VOCE_RESET_P_MAX:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("1)AZZERA PR MAX ");
                visualizzaOpzioniMenu("Next", "Reset");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                indice_soglia = 0;
                stato_attuale = VOCE_MODIFICA_TEMPI;
            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) {
                    lcd.home();
                    lcd.print("AZZERAMENTO     ");
                    lcd.setCursor(0,1);
                    lcd.print("                ");
                }
                pressione_max = -50;
                schermo_aggiornato = false;
                stato_attuale = HOME;
            }
            break;
        
        case VOCE_MODIFICA_TEMPI:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.printf("%d)Da %1.1f a %1.1f  ", (indice_soglia + 2), PRESS[indice_soglia], PRESS[indice_soglia + 1]);
                visualizzaOpzioniMenu("Next", "Set");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                indice_soglia = (indice_soglia < NUM_PRESSIONI) ? (indice_soglia + 1) : 0;
                stato_attuale = (indice_soglia == NUM_PRESSIONI) ? VOCE_SALVA_TEMPI : VOCE_MODIFICA_TEMPI;
            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = MODIFICA_OFF;
            }
            break;

        case MODIFICA_OFF:
            if (!schermo_aggiornato) {
                appoggio_t_off = tempi[indice_soglia][0];
                visualizzaOpzioniModifica("Off", "Set");
                schermo_aggiornato = true;
            }

            lcd.home();
            lcd.printf("%1.1f-%1.1f:  %dms  ", PRESS[indice_soglia], PRESS[indice_soglia+1], appoggio_t_off);

            if (digitalRead(BUTTON2)) {
                aumentaTempo(&appoggio_t_off);

            } else if (digitalRead(BUTTON3)) {
                diminuisciTempo(&appoggio_t_off);

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = MODIFICA_ON;
            }

            break;

        case MODIFICA_ON:
            if (!schermo_aggiornato) {
                appoggio_t_on = tempi[indice_soglia][1];
                visualizzaOpzioniModifica("On", "Set");
                schermo_aggiornato = true;
            }

            lcd.home();
            lcd.printf("%1.1f-%1.1f:  %dms", PRESS[indice_soglia], PRESS[indice_soglia+1], appoggio_t_on);

            if (digitalRead(BUTTON2)) {
                aumentaTempo(&appoggio_t_on);

            } else if (digitalRead(BUTTON3)) {
                diminuisciTempo(&appoggio_t_on);

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = CONFERMA_MODIFICA;
            }

            break;


        case CONFERMA_MODIFICA:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.printf("OFF %d", appoggio_t_off);
                lcd.setCursor(9,0);
                lcd.printf("ON %d ", appoggio_t_on);
                visualizzaOpzioniMenu("Abort", "Save");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = HOME;

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                tempi[indice_soglia][1] = appoggio_t_on;
                tempi[indice_soglia][0] = appoggio_t_off;
                schermo_aggiornato = false;
                stato_attuale = HOME;
            }
            break;

        case VOCE_SALVA_TEMPI:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("6)SALVA I TEMPI  ");
                visualizzaOpzioniMenu("Next", "Save");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = statoBT ? VOCE_SPEGNI_BLUETOOTH : VOCE_ACCENDI_BLUETOOTH;

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = CONFERMA_SALVATAGGIO;
            }

            break;
        
        case CONFERMA_SALVATAGGIO:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("SALVARE?        ");
                visualizzaOpzioniMenu("Cancel", "Save");
                schermo_aggiornato = true;
            }


            // SOVRASCRIVE I DATI NELLA EEPROM
            if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();

                for (int i = 0; i < 2; i++) {
                    for (int j = 0; j < NUM_PRESSIONI; j++) {
                        scriviEeprom(tempi[j][i], j + (i*4));
                    }
                }

                schermo_aggiornato = false;
                stato_attuale = HOME;
            } 
            // ESCE DAL MENU SENZA SALVARE
            else if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) {
                    lcd.home();
                    lcd.print("  SALVATAGGIO   ");
                    lcd.setCursor(0,1);
                    lcd.print("   ANNULLATO    ");
                }
                schermo_aggiornato = false;
                stato_attuale = HOME;
                delay(500);
            }

            break;
        
        case VOCE_ACCENDI_BLUETOOTH:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("7)ACCENDI BT    ");
                lcd.setCursor(13,0);
                lcd.write(byte(3));
                visualizzaOpzioniMenu("Next", "On");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = VOCE_RIEPILOGO;

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                serialBT.begin("Centralina ESP32"); //Bluetooth device name
                statoBT = true;
                schermo_aggiornato = false;
                stato_attuale = HOME;
            }

            break;

        case VOCE_SPEGNI_BLUETOOTH:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("7)SPEGNI BT     ");
                lcd.setCursor(12,0);
                lcd.write(byte(3));
                visualizzaOpzioniMenu("Next", "Off");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = VOCE_RIEPILOGO;

            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                serialBT.end();
                statoBT = false;
                schermo_aggiornato = false;
                stato_attuale = HOME;
            }
            break;

        case VOCE_RIEPILOGO:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.print("8)RIEPILOGO      ");
                visualizzaOpzioniMenu("Next", "Show");
                schermo_aggiornato = true;
            }

            if (digitalRead(BUTTON1)) {
                while (digitalRead(BUTTON1)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = HOME;
            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                indice_soglia = 1;
                stato_attuale = MOSTRA_RIEPILOGO;
            }
            break;
        
        case MOSTRA_RIEPILOGO:
            if (!schermo_aggiornato) {
                lcd.home();
                lcd.printf("Off %d", tempi[indice_soglia-1][0]);
                lcd.setCursor(9,0);
                lcd.printf("On %d ", tempi[indice_soglia-1][1]);
                visualizzaOpzioniModifica("<" + (String)PRESS[indice_soglia], "Esc");
                schermo_aggiornato = true;
            }


            if (digitalRead(BUTTON2)) {
                while (digitalRead(BUTTON2)) lcd.clear();
                schermo_aggiornato = false;
                indice_soglia = (indice_soglia < NUM_PRESSIONI) ? (indice_soglia + 1) : 1; 
            } else if (digitalRead(BUTTON3)) {
                while (digitalRead(BUTTON3)) lcd.clear();
                schermo_aggiornato = false;
                indice_soglia = (indice_soglia > 1) ? (indice_soglia - 1) : NUM_PRESSIONI; 
            } else if (digitalRead(BUTTON4)) {
                while (digitalRead(BUTTON4)) lcd.clear();
                schermo_aggiornato = false;
                stato_attuale = HOME;
            }
            break;
    }

    if (pressione < PRESS[0]) {

        if (serialBT.available()){
            char incomingChar = serialBT.read();
            if (incomingChar != '\n') {
                messaggioBT += String(incomingChar);
            } else {
                messaggioBT = "";
            } 
        }

        if (messaggioBT == "tempi") {

            String risposta;
            StaticJsonDocument<256> doc;

            JsonArray t[NUM_PRESSIONI];

            for (int i = 0; i < NUM_PRESSIONI; i++) {
                t[i] = doc.createNestedArray("t" + (String)i);
                t[i].add(tempi[i][0]);
                t[i].add(tempi[i][1]);
            }

            serializeJson(doc, risposta);
            risposta += "\n";

            #ifdef DEBUG
            Serial.println(messaggioBT);
            Serial.println(risposta);
            #endif 

            serialBT.print(risposta);
            messaggioBT = "";

        } else if (messaggioBT.charAt(0) == '{' && messaggioBT.charAt(messaggioBT.length() - 1) == '}') {

            #ifdef DEBUG
            Serial.println(messaggioBT);
            #endif 

            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, messaggioBT);

            if (error) {
                #ifdef DEBUG
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                #endif
            } else {

                for (int i = 0; i < NUM_PRESSIONI; i++) {
                    for (int j = 0; j < 2; j++) {
                        tempi[i][j] = (doc["t"+(String)i][j] >= 25) ?  doc["t"+(String)i][j] : 25;
                    }
                }
            }

            messaggioBT = "";
        }
    }
}


// Legge la pressione dal sensore
void leggiSensorePressione() {
    pressione = (float) ( ((double)analogRead(PRESSURE_SENSOR)) * 0.0013745704467353952) - 0.79579037800687285223;

    if (pressione > pressione_max) {
        pressione_max = pressione;
    }

}


// Visualizza la pressione nella pagina iniziale
void visualizzaPressione() {
    lcd.home();
    lcd.printf("Pressione: %1.2f ", pressione);
    lcd.setCursor(0,1);
    lcd.printf("Press Max: %1.2f ", pressione_max);
}


// Visualizza le opzioni disponibili per i pulsanti esterni
void visualizzaOpzioniMenu(String sx, String dx) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.write(byte(2));
    lcd.setCursor(15, 1);
    lcd.write(byte(1));
    lcd.setCursor(1,1);
    lcd.print(sx);
    lcd.setCursor(15 - dx.length(), 1);
    lcd.print(dx);
}


// Visualizza le opzioni disponibili per gli ultimi 3 pulsanti
void visualizzaOpzioniModifica(String sx, String dx) {
    lcd.setCursor(0,1);
    lcd.print(sx);
    lcd.setCursor(4,1);
    lcd.print("  +  -     ");
    lcd.setCursor(15 - dx.length(), 1);
    lcd.print(dx);
    lcd.setCursor(5, 1);
    lcd.write(byte(0));
    lcd.setCursor(10, 1);
    lcd.write(byte(0));
    lcd.setCursor(15, 1);
    lcd.write(byte(1));
}


// Aumenta il tempo di iniezione
void aumentaTempo(uint32_t *tempo) {
    *tempo += OFFSET;
    lcd.setCursor(10,0);
    lcd.printf("%dms", *tempo);
    delay(400);

    while (digitalRead(BUTTON2)) {
        *tempo += OFFSET;
        lcd.setCursor(10,0);
        lcd.printf("%dms", *tempo);
        delay(200);
    }
}


// Diminuisce il tempo di iniezione
void diminuisciTempo(uint32_t *tempo) {
    if ((*tempo - OFFSET) >= T_MIN) *tempo -= OFFSET;
    lcd.setCursor(10,0);
    lcd.printf("%dms  ", *tempo);
    delay(400);

    while (digitalRead(BUTTON3)) {
        if ((*tempo - OFFSET) >= T_MIN) *tempo -= OFFSET;
        lcd.setCursor(10,0);
        lcd.printf("%dms  ", *tempo);
        delay(200);
    }
}


// Legge la EEPROM
uint32_t leggiEeprom(uint8_t i) {
    uint32_t lettura = EEPROM.read(i*2) * 256 + EEPROM.read(i*2+1);
    return lettura;
}


// Sovrascrive la EEPROM
void scriviEeprom(uint32_t valore, uint8_t i) {
    for (int j = 0; j < 2; j++) {
        EEPROM.put(i*2, highByte(valore));
        EEPROM.put(i*2+1, lowByte(valore));
    }  
    EEPROM.commit();
}
