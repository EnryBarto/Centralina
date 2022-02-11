/*
 * Autore:  Bartocetti Enrico
 * Versione: 1.1
 */


// IMPORTAZIONE LIBRERIE
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>


LiquidCrystal_I2C lcd(0x27, 16, 2); // Inizia la comunicazione I2C con l'LCD


// ASSEGNAZIONE DEI PIN

#define PRESSURE_SENSOR A5 // Pin del sensore della pressione
#define BUTTON1 5 // Pin del pulsante MENU
#define BUTTON2 6 // Pin del pulsante +
#define BUTTON3 7 // Pin del pulsante -
#define BUTTON4 8 // Pin del pulsante INVIO
#define RELE 10   // Pin del relé


// PROTOTIPI
void menu(int press_iniz, int press_fin); // VISUALIZZA LE PAGINE DEL MENU
void visualizza_pressione(); // VISUALIZZA LA PRESSIONE NELLA PAGINA INIZIALE

float leggi_pressione(); // LEGGE LA PRESSIONE DAL SENSORE
void rele(int t_off, int t_on, int numero); // ACCENDE E SPEGNE IL RELE
void controlla_bottone(); // CONTROLLA SE IL PRIMO PULSANTE VIENE PREMUTO

void modifica_tempi(float press_base, int *t_off, int *t_on); // MODIFICA I TEMPI DI INIEZIONE
void aumenta_tempi(float press_base, int *t); // AUMENTA I TEMPI DI INIEZIONE
void diminuisci_tempi(float press_base, int *t, int t_min); // DIMINUISCE I TEMPI DI INIEZIONE
void visualizza_modifiche(int tempo, float press_base); // DURANTE LA MODIFICA, VISUALIZZA LE PRESSIONI "SOGLIA" E I RELATIVI TEMPI

int leggi_eeprom(int i); // LEGGE LA EEPROM
void scrivi_eeprom(int valore, int i); // SOVRASCRIVE LA EEPROM


// DEFINIZIONE VARIABILI
float pressione;  // Valore della pressione calcolata
bool iniezione = false; // Vero se è in corso l'iniezione
int tempi[4][2];  // Matrice che contiene i tempi di iniezione 0: off 1: on
int funzione;     // Contiene l'indice del menu in cui ci si trova 0: Pagina iniziale
unsigned long tempo_partenza, ritardo, inizio_iniezione; // Contengono il tempo assegnato dalla funzione millis()
const float PRESS[5] = {0.007, 0.1, 0.2, 0.3, 3.0}; // Pressioni soglia
const int T_ON_MIN = 300; // Tempo on minimo selezionabile


void setup() {

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
            tempi[j][i] = leggi_eeprom(j + (i*4));
        }
    }

    // INIZIALIZZAZIONE DELL'LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("ACCENSIONE  v1.1");
    for (int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.print("-");
        delay(150);
    }

    funzione = 0;
    pressione = leggi_pressione();
    visualizza_pressione();
}


void loop() {

    // Esegue il controllo sul pulsante solo se non è in corso l'iniezione
    if (!iniezione) {
        controlla_bottone();
    } else {
        funzione = 0; // Se si sta visualizzando il menu ed inizia l'iniezione, torna alla pagina iniziale
    }

    // VISUALIZZA I MENU SOLO SE NON è IN CORSO L'INIEZIONE
    if (!iniezione) {

        switch (funzione) {

            case 1:
                menu(PRESS[0], PRESS[1]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modifica_tempi(PRESS[0], &tempi[0][0], &tempi[0][1]);
                }
                break;                                       

            case 2:
                menu(PRESS[1], PRESS[2]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modifica_tempi(PRESS[1], &tempi[1][0], &tempi[1][1]);
                }
                break;

            case 3:
                menu(PRESS[2], PRESS[3]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modifica_tempi(PRESS[2], &tempi[2][0], &tempi[2][1]);
                }
                break;

            case 4:
                menu(PRESS[3], PRESS[4]);
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0,0);
                        lcd.print("MODIFICA        ");
                    }
                    modifica_tempi(PRESS[3], &tempi[3][0], &tempi[3][1]);
                }
                break;

            case 5:
                lcd.setCursor(0,0);
                lcd.print("5)SALVA I TEMPI ");
                if (digitalRead(BUTTON4)) {
                    while (digitalRead(BUTTON4)) {
                        lcd.setCursor(0, 0);
                        lcd.print("                ");
                    }

                    // CHIEDE LA CONFERMA PER SOVRASCRIVERE I DATI NELLA EEPROM
                    while ((!digitalRead(BUTTON4)) && (!digitalRead(BUTTON1))) {
                        lcd.setCursor(0,0);
                        lcd.print("CONFERMI?       ");
                        lcd.setCursor(0,1);
                        lcd.print("BT4 x CONFERMARE");
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
                                scrivi_eeprom(tempi[j][i], j + (i*4));
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
    }

    pressione = leggi_pressione();

    // VISUALIZZA LA PRESSIONE OGNI 20ms NELL'LCD
    if (funzione == 0) {
        visualizza_pressione();
        delay(20);
    }

    // AZIONA IL RELE IN BASE ALLA PRESSIONE
    if (pressione <= PRESS[0]) {
        tempo_partenza = millis();
        iniezione = false;
        lcd.setCursor(0,1);
        lcd.print("INIEZIONE: OFF  ");
        digitalWrite(RELE, LOW);
    } else if (pressione < PRESS[1]) {
        iniezione = true;
        lcd.setCursor(0,1);
        lcd.print("INIEZIONE: ON 1 ");
        rele(tempi[0][0], tempi[0][1], 1);
    } else if (pressione < PRESS[2]) {
        iniezione = true;
        lcd.setCursor(0,1);
        lcd.print("INIEZIONE: ON 2 ");
        rele(tempi[1][0], tempi[1][1], 2);
    } else if (pressione < PRESS[3]) {
        iniezione = true;
        lcd.setCursor(0,1);
        lcd.print("INIEZIONE: ON 3 ");
        rele(tempi[2][0], tempi[2][1], 3);
    } else {
        iniezione = true;
        lcd.setCursor(0,1);
        lcd.print("INIEZIONE: ON 4 ");
        rele(tempi[3][0], tempi[3][1], 4);
    }   
}


// VISUALIZZA LE PAGINE DEL MENU
void menu(float press_iniz, float press_fin) {
    lcd.setCursor(0,0);
    lcd.print(" )DA     A    ba");
    lcd.setCursor(0,0);
    lcd.print(funzione);
    lcd.setCursor(5,0);
    lcd.print(press_iniz, 1);
    lcd.setCursor(11,0);
    lcd.print(press_fin, 1);
    delay(50);
}


// VISUALIZZA LA PRESSIONE NELLA PAGINA INIZIALE
void visualizza_pressione() {
    lcd.setCursor(0,0);
    lcd.print("PRESS:       bar");
    lcd.setCursor(8,0);
    lcd.print(pressione);
}


// LEGGE LA PRESSIONE DAL SENSORE
float leggi_pressione() {
    float lettura;
    lettura = (float) (analogRead(PRESSURE_SENSOR) - 338) / 292;
    return lettura;
}


// ACCENDE E SPEGNE IL RELE
void rele(int t_off, int t_on, int numero) {

    // Calcola il ritardo causato dall'esecuzione del resto del codice
    ritardo = millis() - tempo_partenza;
    t_on -= ritardo;

    // IN CASO DI ERRORI IMPOSTA t_on = 1
    if (t_on < 0) {
        tempo_partenza = millis();
        t_on = 1;
    }

    inizio_iniezione = millis();

    digitalWrite(RELE, LOW);

    // Mentre aspetta il tempo off, controlla se la pressione letta cambia il range
    while (millis() - inizio_iniezione <= t_off) {
        pressione = leggi_pressione();
        visualizza_pressione();

        // ESCE SE IL RANGE DELLA PRESSIONE è CAMBIATO
        if ((pressione > PRESS[numero]) || (pressione < PRESS[numero-1])) {
            return;
        }

        delay(20);
    }

    inizio_iniezione = millis();

    digitalWrite(RELE, HIGH);

    // Mentre aspetta il tempo on, controlla se la pressione letta cambia il range    
    while (millis() - inizio_iniezione <= t_on) {
        pressione = leggi_pressione();
        visualizza_pressione();

        // ESCE SE IL RANGE DELLA PRESSIONE è CAMBIATO
        if ((pressione > PRESS[numero]) || (pressione < PRESS[numero-1])) {
            return;
        }

        delay(20);
    }

    tempo_partenza = millis();
}


// CONTROLLA SE IL PRIMO PULSANTE VIENE PREMUTO
void controlla_bottone() {
    if (digitalRead(BUTTON1)) {
        while (digitalRead(BUTTON1)) {
            lcd.setCursor(0,0);
            lcd.print("                ");
        }

        // Alla pressione, aumenta la funzione, se si è alla 5 ritorna alla pagina iniziale
        if (funzione >= 5) {
            funzione = 0;
        } else {
            funzione++;
        }
    }
}



// MODIFICA I TEMPI DI INIEZIONE
void modifica_tempi(float press_base, int *t_off, int *t_on) {
    lcd.setCursor(0, 0);
    lcd.print("  INIEZIONE ON  ");

    // MODIFICA IL TEMPO DI INIEZIONE APERTO
    while (!digitalRead(BUTTON4)) {

        // AUMENTA IL TEMPO
        if (digitalRead(BUTTON2)) {
            aumenta_tempi(press_base, t_on);
        }

        // DIMINUISCE IL TEMPO
        else if (digitalRead(BUTTON3)) {
            diminuisci_tempi(press_base, t_on, T_ON_MIN);
        }

        visualizza_modifiche(*t_on, press_base);
    }

    while(digitalRead(BUTTON4)) {
        lcd.setCursor(0, 0);
        lcd.print("  INIEZIONE OFF ");
        visualizza_modifiche(*t_off, press_base);
    }

    // MODIFICA IL TEMPO DI INIEZIONE "CHIUSO"
    while (!digitalRead(BUTTON4)) {

        // AUMENTA IL TEMPO
        if (digitalRead(BUTTON2)) {
            aumenta_tempi(press_base, t_off);
        }

        // DIMINUISCE IL TEMPO
        else if (digitalRead(BUTTON3)) {
            diminuisci_tempi(press_base, t_off, 0);
        }

        visualizza_modifiche(*t_off, press_base);
    }

    while (digitalRead(BUTTON4)) {
        lcd.setCursor(0, 0);
        lcd.print("  SALVATAGGIO   ");
    }
    funzione = 0;
}


// AUMENTA I TEMPI DI INIEZIONE
void aumenta_tempi(float press_base, int *t) { 
    *t += 50;
    delay(400);
    while (digitalRead(BUTTON2)) {
        *t += 50;
        visualizza_modifiche(*t, press_base);
        delay(200);
    }
}


// DIMINUISCE I TEMPI DI INIEZIONE
void diminuisci_tempi(float press_base, int *t, int t_min) {
    // t_min: Tempo minimo di iniezione
    if (*t > t_min) {
        *t -= 50;
    }
    delay(400);
    while (digitalRead(BUTTON3)) {
        if (*t > t_min) {
            *t -= 50;
        }
        visualizza_modifiche(*t, press_base);
        delay(200);
    }
}


// DURANTE LA MODIFICA, VISUALIZZA LE PRESSIONI "SOGLIA" E I RELATIVI TEMPI
void visualizza_modifiche(int tempo, float press_base) {
    lcd.setCursor(3, 1);
    lcd.print(press_base, 1);  
    lcd.setCursor(0, 1);
    lcd.print("Da ");
    lcd.setCursor(6, 1);
    lcd.print("bar ");
    lcd.setCursor(12, 1);
    lcd.print("  ms");
    lcd.setCursor(10, 1);
    lcd.print(tempo);
    delay(5);
}


// LEGGE LA EEPROM
int leggi_eeprom(int i) {
    int lettura = EEPROM.read(i*2) * 256 + EEPROM.read(i*2+1);
    return lettura;
}


// SOVRASCRIVE LA EEPROM
void scrivi_eeprom(int valore, int i) {
    EEPROM.update(i*2, highByte(valore));
    EEPROM.update(i*2+1, lowByte(valore));
}
