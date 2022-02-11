/**
 * Autore:  Bartocetti Enrico
 * Data: 11/02/2022
 * Versione: 1.4
**/


/*-------------------- IMPORTAZIONE LIBRERIE --------------------*/

#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
//#define DEBUG yes // Togliere il commento per avere sulla seriale: Tempo Ritardo, Tempo iniezione ON, Tempo iniezione OFF


/*-------------------- ASSEGNAZIONE DEI PIN --------------------*/

#define PRESSURE_SENSOR A5 // Pin del sensore della pressione
#define BUTTON1 5 // Pin del pulsante MENU
#define BUTTON2 6 // Pin del pulsante +
#define BUTTON3 7 // Pin del pulsante -
#define BUTTON4 8 // Pin del pulsante INVIO
#define RELE 10   // Pin del relé


/*------------------ PROTOTIPI DELLE FUNZIONI ------------------*/

// Funzioni per la grafica
void visualizzaPressione();          // VISUALIZZA LA PRESSIONE NELLA PAGINA INIZIALE
void visualizzaPressioneIniezione(); // VISUALIZZA LA PRESSIONE NELLA PAGINA INIZIALE
void visualizzaPaginaMenu(float press_iniz, float press_fin);               // VISUALIZZA LE PAGINE DEL MENU
void visualizzaModifiche(uint32_t tempo, float press_min, float press_max); // DURANTE LA MODIFICA, VISUALIZZA LE PRESSIONI "SOGLIA" E I RELATIVI TEMPI

// Funzioni per sensori e attuatori
float leggiSensorePressione();  // LEGGE LA PRESSIONE DAL SENSORE
void controlloPulsante();       // CONTROLLA SE IL PRIMO PULSANTE VIENE PREMUTO
void attivaIniezione(uint32_t t_off, uint32_t t_on, uint8_t numero); // ACCENDE E SPEGNE IL RELE

// Funzioni per la modifica dei tempi
void modificaTempi(float press_min, float press_max, uint32_t *t_off, uint32_t *t_on); // MODIFICA I TEMPI DI INIEZIONE
void aumentaTempi(float press_min, float press_max, uint32_t *t);                      // AUMENTA I TEMPI DI INIEZIONE
void diminuisciTempi(float press_min, float press_max, uint32_t *t, uint32_t t_min);   // DIMINUISCE I TEMPI DI INIEZIONE

// Funzioni per la EEPROM
uint32_t leggiEeprom(uint8_t i);               // LEGGE LA EEPROM
void scriviEeprom(uint32_t valore, uint8_t i); // SOVRASCRIVE LA EEPROM


/*-------------------- DEFINIZIONE VARIABILI --------------------*/

bool iniezione = false;     // Flag di stato. Vero se è in corso l'iniezione
bool prima_iniezione = true;// Usato per far pulire l'lcd alla prima iniezione
float pressione;            // Pressione letta dal sensore
float pressione_max;        // Pressione massima raggiunta
uint8_t funzione;           // Contiene l'indice del menu in cui ci si trova. 0: Pagina iniziale
uint32_t tempi[4][2];       // Matrice che contiene i tempi di iniezione. Riga 0: tempi off, Riga 1: tempi on
uint64_t tempo_partenza;    // Tempo usato per calcolare il ritardo 
uint64_t ritardo;           // Ritardo calcolato dato dall'esecuzione del codice durante l'iniezione
uint64_t inizio_iniezione;  // Istante in cui inizia l'iniezione
LiquidCrystal_I2C lcd(0x27, 16, 2); // Creazione dell'oggetto LCD


/*-------------------- DEFINIZIONE COSTANTI --------------------*/

const float PRESS[5] = {  // Pressioni soglia
    0.007,
    0.1,
    0.2,
    0.3,
    3.0
};
const int T_OFF_MIN = 50; // Tempo off minimo selezionabile
const int OFFSET = 25;    // Offset per l'incremento e il decremento dei tempi di iniezione


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


void setup() {

    #ifdef DEBUG
    Serial.begin(115200);
    #endif

    // INIZIALIZZAZIONE INPUT / OUTPUT
    pinMode(PRESSURE_SENSOR, INPUT);
    pinMode(BUTTON1, INPUT);
    pinMode(BUTTON2, INPUT);
    pinMode(BUTTON3, INPUT);
    pinMode(BUTTON4, INPUT);
    pinMode(RELE, OUTPUT); 

    // LETTURA DEI TEMPI DALLA EEPROM
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            tempi[j][i] = leggiEeprom(j + (i*4));
        }
    }

    // INIZIALIZZAZIONE DELL'LCD
    lcd.init();
    lcd.backlight();
    lcd.createChar(0, freccia_giu);
    lcd.createChar(1, freccia_angolo_destra);
    lcd.createChar(2, freccia_angolo_sinistra);
    lcd.setCursor(0, 0);
    lcd.print("ACCENSIONE  v1.4");
    for (int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.print("-");
        delay(150);
    }

    funzione = 0;
    pressione = leggiSensorePressione();
    pressione_max = leggiSensorePressione();
    visualizzaPressione();
}


void loop() {

    // VISUALIZZA I MENU SOLO SE NON è IN CORSO L'INIEZIONE
    if (!iniezione) {

        // Azzera la prima iniezione
        if (!prima_iniezione) {
            prima_iniezione = true;
        }
        
        controlloPulsante();

        switch (funzione) {

            case 0:
                visualizzaPressione();
                break;
                
            case 1:
                lcd.setCursor(0,0);
                lcd.print("1)AZZERA PR MAX ");
                lcd.setCursor(0, 1);
                lcd.write(byte(2));
                lcd.print("Next     Reset");
                lcd.setCursor(15, 1);
                lcd.write(byte(1));

                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("AZZERAMENTO     ");
                    }
                    pressione_max = leggiSensorePressione();
                    funzione = 0;
                }
                break;

            case 2:
                visualizzaPaginaMenu(PRESS[0], PRESS[1]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modificaTempi(PRESS[0], PRESS[1], &tempi[0][0], &tempi[0][1]);
                }
                break;                                       

            case 3:
                visualizzaPaginaMenu(PRESS[1], PRESS[2]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modificaTempi(PRESS[1], PRESS[2], &tempi[1][0], &tempi[1][1]);
                }
                break;

            case 4:
                visualizzaPaginaMenu(PRESS[2], PRESS[3]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modificaTempi(PRESS[2], PRESS[3], &tempi[2][0], &tempi[2][1]);
                }
                break;

            case 5:
                visualizzaPaginaMenu(PRESS[3], PRESS[4]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modificaTempi(PRESS[3], PRESS[4], &tempi[3][0], &tempi[3][1]);
                }
                break;

            case 6:
                lcd.setCursor(0,0);
                lcd.print("6)SALVA I TEMPI  ");
                lcd.setCursor(0, 1);
                lcd.write(byte(2));
                lcd.print("Next      Save");
                lcd.setCursor(15, 1);
                lcd.write(byte(1));

                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0, 0);
                        lcd.print("                ");
                    }

                    // CHIEDE LA CONFERMA PER SOVRASCRIVERE I DATI NELLA EEPROM
                    while ((!digitalRead(BUTTON4)) && (!digitalRead(BUTTON1))) {
                        lcd.setCursor(0,0);
                        lcd.print("SALVARE?        ");
                        lcd.setCursor(0, 1);
                        lcd.write(byte(2));
                        lcd.print("Cancel    Save");
                        lcd.setCursor(15, 1);
                        lcd.write(byte(1));
                    }

                    // SOVRASCRIVE I DATI NELLA EEPROM
                    if (digitalRead(BUTTON4)) {
                        while (digitalRead(BUTTON4)) {
                            lcd.setCursor(0,0);
                            lcd.print(" SALVATAGGIO IN ");
                            lcd.setCursor(0,1);
                            lcd.print("    CORSO...    ");
                        }

                        for (int i = 0; i < 2; i++) {
                            for (int j = 0; j < 4; j++) {
                                scriviEeprom(tempi[j][i], j + (i*4));
                            }
                        }

                        delay(2000);
                    }

                    // ESCE DAL MENU SENZA SALVARE
                    else if (digitalRead(BUTTON1)) {
                        while (digitalRead(BUTTON1)) {
                            lcd.setCursor(0,0);
                            lcd.print("  SALVATAGGIO   ");
                            lcd.setCursor(0,1);
                            lcd.print("   ANNULLATO    ");
                        }
                        delay(2000);
                    }
                    funzione = 0;
                }
                break;
        }

        delay(50); // Abbassa il refresh rate dello schermo
    }

    pressione = leggiSensorePressione();

    // AZIONA IL RELE SOLO SE CI SI TROVA NELLA HOME
    if (funzione == 0) {

        // AZIONA IL RELE IN BASE ALLA PRESSIONE
        if (pressione <= PRESS[0]) {
            tempo_partenza = millis();
            iniezione = false;
            digitalWrite(RELE, LOW);
        } else if (pressione < PRESS[1]) {
            iniezione = true;
            attivaIniezione(tempi[0][0], tempi[0][1], 1);
        } else if (pressione < PRESS[2]) {
            iniezione = true;
            attivaIniezione(tempi[1][0], tempi[1][1], 2);
        } else if (pressione < PRESS[3]) {
            iniezione = true;
            attivaIniezione(tempi[2][0], tempi[2][1], 3);
        } else {
            iniezione = true;
            attivaIniezione(tempi[3][0], tempi[3][1], 4);
        }   
    }
}


// VISUALIZZA LA PRESSIONE NELLA PAGINA INIZIALE
void visualizzaPressione() {
    lcd.setCursor(0,0);
    lcd.print("PRESS:       bar");
    lcd.setCursor(8,0);
    lcd.print(pressione);
    lcd.setCursor(0,1);
    lcd.print("PRESS MAX: ");
    lcd.setCursor(15,1);
    lcd.print("b");
    lcd.setCursor(11,1);
    lcd.print(pressione_max);
}


// VISUALIZZA LA PRESSIONE DURANTE L'INIEZIONE
void visualizzaPressioneIniezione() {
    if (prima_iniezione) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("PRESS:       bar");
        lcd.setCursor(0,1);
        lcd.print("Iniettore acceso");
        prima_iniezione = false;
    }
    lcd.setCursor(8,0);
    lcd.print(pressione);
}


// VISUALIZZA LE PAGINE DEL MENU
void visualizzaPaginaMenu(float press_iniz, float press_fin) {
    lcd.setCursor(0,0);
    lcd.print(" )DA     A    ba");
    lcd.setCursor(0,0);
    lcd.print(funzione);
    lcd.setCursor(5,0);
    lcd.print(press_iniz, 1);
    lcd.setCursor(11,0);
    lcd.print(press_fin, 1);

    lcd.setCursor(0, 1);
    lcd.write(byte(2));
    lcd.print("Next       Set");
    lcd.setCursor(15, 1);
    lcd.write(byte(1));

    delay(50);
}


// DURANTE LA MODIFICA, VISUALIZZA LE PRESSIONI "SOGLIA" E I RELATIVI TEMPI
void visualizzaModifiche(uint32_t tempo, float press_min, float press_max) {
    lcd.setCursor(0, 0);
    lcd.print(press_min, 1);  
    lcd.setCursor(3, 0);
    lcd.print("-");
    lcd.setCursor(4, 0);
    lcd.print(press_max, 1);
    lcd.setCursor(7, 0);
    lcd.print(":");
    lcd.setCursor(12, 0);
    lcd.print("  ms");
    lcd.setCursor(10, 0);
    lcd.print(tempo);

    delay(50);
}


// LEGGE LA PRESSIONE DAL SENSORE
float leggiSensorePressione() {
    float lettura;
    lettura = (float) (analogRead(PRESSURE_SENSOR) - 338) / 292;

    if (lettura > pressione_max) {
        pressione_max = lettura;
    }
    
    return lettura;
}


// CONTROLLA SE IL PRIMO PULSANTE VIENE PREMUTO
void controlloPulsante() {
    if (digitalRead(BUTTON1)) {
        while (digitalRead(BUTTON1)) {
            lcd.setCursor(0,0);
            lcd.print("                ");
        }

        // Alla pressione, aumenta la funzione, se si è alla 5 ritorna alla pagina iniziale
        if (funzione >= 6) {
            funzione = 0;
        } else {
            funzione++;
        }
    }
}


// ACCENDE E SPEGNE IL RELE
void attivaIniezione(uint32_t t_off, uint32_t t_on, uint8_t numero) {

    // Calcola il ritardo causato dall'esecuzione del resto del codice
    ritardo = millis() - tempo_partenza;
    t_off -= ritardo;

    #ifdef DEBUG
    Serial.print(ritardo);
    Serial.print(" ");
    #endif

    // IN CASO DI ERRORI IMPOSTA t_off = 1
    if (t_off < 0) {
        tempo_partenza = millis();
        t_off = 1;
    }

    inizio_iniezione = millis();

    digitalWrite(RELE, HIGH);

    // Mentre aspetta il tempo on, controlla se la pressione letta cambia il range
    while (millis() - inizio_iniezione <= t_on) {
        pressione = leggiSensorePressione();
        visualizzaPressioneIniezione();

        // ESCE SE IL RANGE DELLA PRESSIONE è CAMBIATO
        if ((pressione > PRESS[numero]) || (pressione < PRESS[numero-1])) {
            return;
        }
    }

    #ifdef DEBUG
    Serial.print(millis() - inizio_iniezione);
    Serial.print(" ");
    #endif

    inizio_iniezione = millis();

    digitalWrite(RELE, LOW);

    // Mentre aspetta il tempo off, controlla se la pressione letta cambia il range    
    while (millis() - inizio_iniezione <= t_off) {
        pressione = leggiSensorePressione();
        visualizzaPressioneIniezione();

        // ESCE SE IL RANGE DELLA PRESSIONE è CAMBIATO
        if ((pressione > PRESS[numero]) || (pressione < PRESS[numero-1])) {
            return;
        }
    }

    #ifdef DEBUG
    Serial.println(millis() - inizio_iniezione);
    #endif

    tempo_partenza = millis();
}


// MODIFICA I TEMPI DI INIEZIONE
void modificaTempi(float press_min, float press_max, uint32_t *t_off, uint32_t *t_on) {
    lcd.setCursor(0, 1);
    lcd.print("ON    +  -  Set");
    lcd.setCursor(5, 1);
    lcd.write(byte(0));
    lcd.setCursor(10, 1);
    lcd.write(byte(0));
    lcd.setCursor(15, 1);
    lcd.write(byte(1));

    // MODIFICA IL TEMPO DI INIEZIONE APERTO
    while (!digitalRead(BUTTON4)) {

        // AUMENTA IL TEMPO
        if (digitalRead(BUTTON2)) {
            aumentaTempi(press_min, press_max, t_on);
        }

        // DIMINUISCE IL TEMPO
        else if (digitalRead(BUTTON3)) {
            diminuisciTempi(press_min, press_max, t_on, 0);
        }

        visualizzaModifiche(*t_on, press_min, press_max);
    }

    while(digitalRead(BUTTON4)) {
        lcd.setCursor(0, 1);
        lcd.print("OFF");
        visualizzaModifiche(*t_off, press_min, press_max);
    }

    // MODIFICA IL TEMPO DI INIEZIONE "CHIUSO"
    while (!digitalRead(BUTTON4)) {

        // AUMENTA IL TEMPO
        if (digitalRead(BUTTON2)) {
            aumentaTempi(press_min, press_max, t_off);
        }

        // DIMINUISCE IL TEMPO
        else if (digitalRead(BUTTON3)) {
            diminuisciTempi(press_min, press_max, t_off, T_OFF_MIN);
        }

        visualizzaModifiche(*t_off, press_min, press_max);
    }

    while (digitalRead(BUTTON4)) {
        lcd.setCursor(0, 0);
        lcd.print("  SALVATAGGIO   ");
    }
    funzione = 0;
}


// AUMENTA I TEMPI DI INIEZIONE
void aumentaTempi(float press_min, float press_max, uint32_t *t) { 
    *t += OFFSET;
    delay(400);
    while (digitalRead(BUTTON2)) {
        *t += OFFSET;
        visualizzaModifiche(*t, press_min, press_max);
        delay(200);
    }
}


// DIMINUISCE I TEMPI DI INIEZIONE
void diminuisciTempi(float press_min, float press_max, uint32_t *t, uint32_t t_min) {
    // t_min: Tempo minimo di iniezione
    if (*t > t_min) {
        *t -= OFFSET;
    }
    delay(400);
    while (digitalRead(BUTTON3)) {
        if (*t > t_min) {
            *t -= OFFSET;
        }
        visualizzaModifiche(*t, press_min, press_max);
        delay(200);
    }
}


// LEGGE LA EEPROM
uint32_t leggiEeprom(uint8_t i) {
    uint32_t lettura = EEPROM.read(i*2) * 256 + EEPROM.read(i*2+1);
    return lettura;
}


// SOVRASCRIVE LA EEPROM
void scriviEeprom(uint32_t valore, uint8_t i) {
    for (int j = 0; j < 2; j++) {
        EEPROM.update(i*2, highByte(valore));
        EEPROM.update(i*2+1, lowByte(valore));
    }
}