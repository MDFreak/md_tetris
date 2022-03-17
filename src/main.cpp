#include <Arduino.h>
#include <wire.h>
#include <Adafruit_GFX.h> //Grafik Bibliothek
#include <Adafruit_ILI9341.h> // Display Treiber
#include <XPT2046_Touchscreen.h> //Touchscreen Treiber
#include <md_TouchEvent.h> //Auswertung von Touchscreen Ereignissen

//Aussehen
#define BACKGROUND ILI9341_GREENYELLOW //Farbe des Rahmens
#define TOPMARGIN 20                   //Rand oben
#define LEFTMARGIN 12                  //Rand links und rechts
#define COLUMNS 12                     //Anzahl der Spalten
#define ROWS 16                        //Anzahl der Zeilen
#define BLOCKSIZE 18                   //Gr��e eines Blocks in Pixel
#define NOPIECE ILI9341_BLACK          //Farb f�r das leere Spielfeld
#define ALLON ILI9341_DARKGREY         //Farbe f�r alle Bl�cke ein
#define BORDER ILI9341_WHITE           //Farbe f�r den Blockrand

//Unterschiedliche Pin-Belegung fuer ESP32 und D1Mini
#ifdef ESP32
    #define TFT_CS   5
    #define TFT_DC   4
    #define TFT_RST  22
    #define TFT_LED  15
    #define TOUCH_CS 14
    #define LED_ON   0
  #endif
  #ifdef ESP8266
      #define TFT_CS   D1
      #define TFT_DC   D2
      #define TFT_RST  -1
      #define TFT_LED  D8
      #define TOUCH_CS 0
      #define LED_ON 1
    #endif

#define TOUCH_ROTATION 3 //muss f�r 2.4 Zoll Display 1 und f�r 2.8 Zoll Display 3 sein
//Instanzen der Bibliotheken
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);
md_TouchEvent tevent(touch);

//Farben fuer die Bloecke
const uint16_t colorBlock[8] =
  {
    ILI9341_BLACK, ILI9341_YELLOW, ILI9341_RED,
    ILI9341_CYAN,  ILI9341_GREEN,  ILI9341_PURPLE,
    ILI9341_BLUE,  ILI9341_ORANGE
  };

/* Bitmuster fuer die Teile
   0 = Block nicht gesetzt >0 Index der Farbe f�r den Block
  */
const uint8_t piece[20][16] =
    {
      { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  1, 1, 0, 0,  1, 1, 0, 0 },
      { 0, 2, 0, 0,  0, 2, 0, 0,  0, 2, 0, 0,  0, 2, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  2, 2, 2, 2 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  3, 3, 0, 0,  0, 3, 3, 0 },
      { 0, 0, 0, 0,  0, 3, 0, 0,  3, 3, 0, 0,  3, 0, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  0, 4, 4, 0,  4, 4, 0, 0 },
      { 0, 0, 0, 0,  4, 0, 0, 0,  4, 4, 0, 0,  0, 4, 0, 0 },
      { 0, 0, 0, 0,  5, 0, 0, 0,  5, 0, 0, 0,  5, 5, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 5, 0,  5, 5, 5, 0 },
      { 0, 0, 0, 0,  5, 5, 0, 0,  0, 5, 0, 0,  0, 5, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  5, 5, 5, 0,  5, 0, 0, 0 },
      { 0, 0, 0, 0,  0, 6, 0, 0,  0, 6, 0, 0,  6, 6, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  6, 6, 6, 0,  0, 0, 6, 0 },
      { 0, 0, 0, 0,  6, 6, 0, 0,  6, 0, 0, 0,  6, 0, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  6, 0, 0, 0,  6, 6, 6, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  0, 7, 0, 0,  7, 7, 7, 0 },
      { 0, 0, 0, 0,  0, 7, 0, 0,  7, 7, 0, 0,  0, 7, 0, 0 },
      { 0, 0, 0, 0,  0, 0, 0, 0,  7, 7, 7, 0,  0, 7, 0, 0 },
      { 0, 0, 0, 0,  0, 7, 0, 0,  0, 7, 7, 0,  0, 7, 0, 0 }
    };

/* Speicherplatz fuer das Spielfeld
   0 bedeutet Block frei >0 Index der Farbe des belegten Blocks
  */
uint8_t playGround[ROWS][COLUMNS];

//Globale variablen
uint8_t curPiece;  //aktuelles Tetris Teil
int8_t curCol;     //aktuelle Spalte
int8_t curRow;     //aktuelle Zeile
uint32_t score;    //aktueller Score
uint8_t level;     //aktueller Level
uint16_t interval; //aktuelles Zeitintervall f�r die Abw�rtsbewegung
uint32_t last;     //letzter Zeitstempel

/* Funktion zeigt in der Kopfleiste den aktuellen Score und den Level an
   Abhaengig vom Score wird der Level hinaufgesetzt und das Intervall verringert
  */
void displayScore()
  {
    if      (score < 10)     {level = 1; interval = 900;}
    else if (score < 100)    {level = 2; interval = 700;}
    else if (score < 1000)   {level = 3; interval = 500;}
    else if (score < 10000)  {level = 4; interval = 300;}
    else if (score < 100000) {level = 5; interval = 100;}
    tft.fillRect(0,0,240,20,BACKGROUND);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(5,4);
    char buf[50];
    sprintf(buf,"SC: %8i LV: %i",score,level);
    tft.print(buf);
  }

/*  Funktion um ein Tetris-Teil zu drehen. Der Parameter ist die Nummer des Teils
   das gedreht werden soll. R�ckgabewert ist der Index des gedrehten Teils
  */
uint8_t rotate(uint8_t pc)
  {
    uint8_t res = 0;
    switch (pc) {
      case 1:  res = 1; break;
      case 2:  res = 3; break;
      case 3:  res = 2; break;
      case 4:  res = 5; break;
      case 5:  res = 4; break;
      case 6:  res = 7; break;
      case 7:  res = 6; break;
      case 8:  res = 9; break;
      case 9:  res = 10; break;
      case 10: res = 11; break;
      case 11: res = 8; break;
      case 12: res = 13; break;
      case 13: res = 14; break;
      case 14: res = 15; break;
      case 15: res = 12; break;
      case 16: res = 17; break;
      case 17: res = 18; break;
      case 18: res = 19; break;
      case 19: res = 16; break;
    }
    return res;
  }

//Funktion testet ob eine Zeile voll belegt ist
boolean rowComplete(int8_t rpg)
  {
    if ((rpg >= 0) && (rpg < ROWS))
      {
        boolean res = true;
        uint8_t c = 0;
        //wenn ein Block nicht belegt ist (Farbe 0), ist die Zeile nicht vollstaendig
        while (res && (c < COLUMNS))
          {
            if (playGround[rpg][c] == 0) res = false;
            c++;
          }
        return res;
      }
  }

/* Funkzion prueft ob es zwischen der Zeile rpc des Tetris-Teils pc und
   der Zeile rpg des Spielfelds ab der Position cpg Kolklisionen gibt.
   Wenn eine Kollision auftritt oder die letzte Zeile des Spielfelds
   erreicht wurde wird falsch zurueckgegeben
  */
boolean checkRow(uint8_t pc, int8_t rpc, int8_t cpg, int8_t rpg)
  {
    boolean res = true;
    if (rpg >= ROWS) return false;
    if (rpg < 0) return true;
    for (uint8_t i = 0; i<4; i++)
      {
        if (piece[pc][rpc*4 + i]>0)
          {
            if (((cpg+i) < 0) || ((cpg+i) >= COLUMNS))
              {
                res = false;
              }
              else
              {
                if (playGround[rpg][cpg+i] > 0) res = false;
              }
          }
      }
    return res;
  }

/* Funktion prueft ob das Tetris Teil pc am Spielfeld an der Position
   Zeile rpg Spalte cpg (linke untere Ecke des Teils) Kollisionen auftreten
  */
boolean checkPiece(uint8_t pc, int8_t cpg, int8_t rpg)
  {
    boolean res = true;
    uint8_t rpc = 0;
    while (res && (rpc < 4))
      {
        res = checkRow(pc,rpc,cpg,rpc+rpg-3);
        //Serial.printf("check %i = %i\n",rpc+rpg-3,res);
        rpc++;
      }
    return res;
  }

/* Funktion zeigt einen Block des Spielfeldes in Zeile y Spalte x mit der Farbe color an
   color ist die Farbe im 565 Format fuer das Display */
void showBlock(uint8_t x, uint8_t y, uint16_t color)
  {
    tft.fillRect(LEFTMARGIN+x*BLOCKSIZE+2,TOPMARGIN+y*BLOCKSIZE+2,BLOCKSIZE-4,BLOCKSIZE-4,color);
    tft.drawRect(LEFTMARGIN+x*BLOCKSIZE+1,TOPMARGIN+y*BLOCKSIZE+1,BLOCKSIZE-2,BLOCKSIZE-2,BORDER);
  }

// Funktion faellt einen Block des Spielfeldes in Zeile y Spalte x mit der Hintergrundfarbe
void hideBlock(uint8_t x, uint8_t y)
  {
    tft.fillRect(LEFTMARGIN+x*BLOCKSIZE,TOPMARGIN+y*BLOCKSIZE,BLOCKSIZE,BLOCKSIZE,NOPIECE);
  }

/* Funktion zeigt das Tetris-Teil pc in Zeile rpg, Spalte cpg (Linke untere Ecke) an
   Die Farbe wird der Definition des Tetris-Teils entnommen
  */
void showPiece(uint8_t pc, uint8_t cpg, uint8_t rpg)
  {
    uint8_t color;
    for (uint8_t r = 0; r<4; r++)
      {
        for (uint8_t c = 0; c<4; c++)
          {
            color = piece[pc][r*4+c];
            if ((color > 0) && ((3-r+rpg) >= 0))
              showBlock(cpg+c,rpg-3+r,colorBlock[color]);
      }
    }
  }

/* Funktion faellt die belegten Bloecke des Tetris-Teil pc in Zeile rpg,
   Spalte cpg (Linke untere Ecke) an mit Hintergrundfarbe
 */
void hidePiece(uint8_t pc, int8_t cpg, int8_t rpg)
  {
    uint8_t color;
    for (uint8_t r = 0; r<4; r++)
      {
        for (uint8_t c = 0; c<4; c++)
          {
            color = piece[pc][r*4+c];
            if ((color > 0) && ((3-r+rpg) >= 0)) hideBlock(cpg+c,rpg-3+r);
          }
      }
  }

/*funktion faellt die Zeile row des Spielfelds mit Hintergrundfarbe und
  loescht alle Eintraege fuer diese Zeile im Spielfeld-Speicher
  */
void deleteRow(int8_t row)
  {
    tft.fillRect(LEFTMARGIN,TOPMARGIN+row*BLOCKSIZE,COLUMNS * BLOCKSIZE,BLOCKSIZE,NOPIECE);
    for (uint8_t i =0; i<COLUMNS; i++) playGround[row][i]=0;
  }

/* Funktion kopiert die Zeile srcrow in die Zeile dstrow
   Die Anzeige der Zielzeile wird vorher geloescht. Beim
   kopieren wird die Quellzeile in der Zielzeile angezeigt
  */
void copyRow(int8_t srcrow, int8_t dstrow)
  {
    uint8_t col;
    deleteRow(dstrow);
    if ((srcrow < dstrow) && (srcrow >=0) && (dstrow < ROWS)) {
      for (uint8_t c = 0; c < COLUMNS; c++) {
        col = playGround[srcrow][c];
        playGround[dstrow][c] = col;
        if (col > 0) showBlock(c,dstrow,colorBlock[col]);
      }
    }
  }

/* Funktion zeigt alle Bloecke des Spielfeldes mit der Farbe ALLON an.
   Nach einer Pause von 500 ms wird das Sielfeld komplett geloescht
  */
void clearBoard()
  {
    for (uint8_t x = 0; x<COLUMNS; x++) {
      for (uint8_t y = 0; y<ROWS; y++) {
        showBlock(x,y,ALLON);
      }
    }
    delay(500);
    for (uint8_t i = 0; i<ROWS; i++) {
      deleteRow(i);
    }
  }

/* Funktion uebertraegt das Tetris-Teil pc in den Spielfeldspeicher in der Zeile
   rpg an der Spalte cpg (linke untere Ecke)
  */
void putPiece(uint8_t pc, int8_t cpg, int8_t rpg)
  {
    uint8_t color;
    for (uint8_t r = 0; r<4; r++)
      {
        for (uint8_t c = 0; c<4; c++)
          {
            color = piece[pc][r*4+c];
            if ((color > 0) && ((3-r+rpg) >= 0)) playGround[rpg-3+r][cpg+c] = color;
          }
      }
  }

/* Ein neues Tetristeil wird am oberen Rand des Spielfeldes eingefuegt.
   Welches Teil und in welcher Spalte wird als Zufallszahl ermittelt
   Hat das neue Teil keinen Platz am Spielfeld, so ist das Spiel zu Ende
  */
boolean newPiece()
  {
    uint8_t pc = random(1,20);
    uint8_t cpg = random(0,COLUMNS-4);
    boolean res = checkPiece(pc,cpg,3);
    curPiece=0;
    if (res)
      {
        curPiece = pc;
        curCol = cpg;
        curRow = 0;
        showPiece(pc,cpg,0);
        score += 4;
        displayScore();
      }
      else
      {
        tft.setTextSize(3);
        tft.setCursor(LEFTMARGIN+COLUMNS*BLOCKSIZE/2-79,TOPMARGIN+ROWS*BLOCKSIZE/2-10);
        tft.setTextColor(ILI9341_BLACK);
        tft.print("GAME OVER");
        tft.setCursor(LEFTMARGIN+COLUMNS*BLOCKSIZE/2-81,TOPMARGIN+ROWS*BLOCKSIZE/2-12);
        tft.setTextColor(ILI9341_YELLOW);
        tft.print("GAME OVER");
      }
  }

/* Funktion ermittelt komplett gef�llte Zeilen am Spielfeld und entfernt diese
   Darueberliegende Zeilen werden nach unten verschoben
 */
void removeComplete()
  {
    uint8_t s=ROWS-1;
    int8_t d= ROWS-1;
    while (d >= 0)
      {
        if (rowComplete(d))
          {
            s--;
            score += 10;
            copyRow(s,d);
          }
          else
          {
            if ((s < d) && (s >=0))
              {
                Serial.printf("copy %i to %i\n",s, d);
                copyRow(s,d);
              }
            s--;
            d--;
          }
      }
   displayScore();
  }

/* Funktion beginnt ein neues Spiel. Der score wird auf 0 gesetzt, das Spielfeld
   geloescht und mit einem neuen Tetris Teil gestartet
 */
void newGame()
  {
    score=0;
    displayScore();
    clearBoard();
    newPiece();
  }

/* Callbackfunktion fuer Touchscreen Ereignis Klick
   Diese Funktion wird immer dann aufgerufen, wenn der Bildschirm
   kurz beruehrt wird. p gibt die Position des Beruehrungspunktes an
 */
void onClick(TS_Point p)
  {
    if (p.y < 80)
      { //Klick im obersten Viertel des Displays
        newGame();
      }
      else if (p.y > 240)
      { //Klick im untersten Viertel
        uint8_t pc = curPiece;
        int8_t c = curCol;
        if (p.x < 80)
          { //Klick im linken Drittel -> nach links schieben
            c--;
          }
          else if (p.x <160)
          { //Klick im mittleren Drittel -> drehen
            pc = rotate(pc);
          }
          else
          { //Klick im rechten Drittel -> nach rechts schieben
            c++;
          }
        /* nach Aenderung der Position wird auf Kollision geprueft
           nur wenn keine Kollision auftritt, wird die Bewegung
           ausgefuehrt
         */
        if (checkPiece(pc,c,curRow))
          {
            hidePiece(curPiece,curCol,curRow);
            curPiece = pc;
            curCol = c;
            showPiece(curPiece,curCol,curRow);
          }
      }
  }

// ----------------------------------------
void setup()
  {
    Serial.begin(115200);
    Serial.println("Start ..");
    //Hintergrundbeleuchtung einschalten
    pinMode(TFT_LED,OUTPUT);
    digitalWrite(TFT_LED, LED_ON);
    //Display initialisieren
    Serial.println(" .. tft");
    tft.begin();
    tft.fillScreen(BACKGROUND);
    //Touchscreen vorbereiten
    Serial.println(" .. touch");
    touch.begin();
    touch.setRotation(TOUCH_ROTATION);
    tevent.setResolution(tft.width(),tft.height());
    tevent.setDrawMode(false);
    //Callback Funktion registrieren
    tevent.registerOnTouchClick(onClick);
    //tft.fillRect(LEFTMARGIN,TOPMARGIN,COLUMNS*BLOCKSIZE,ROWS*BLOCKSIZE,NOPIECE);
    clearBoard();
    //newPiece();
    //Startzeit merken und Spielfeld loeschen
    last = millis();
    score=0;
    displayScore();
    Serial.println("  .. ready");
  }

// ----------------------------------------
// Hauptschleife
void loop()
  {
    //Auf Touch Ereignisse pruefen
    tevent.pollTouchScreen();
    /* Immer wenn die Zeit intterval erreicht ist, wird das aktuelle Tetris-Teil
       um eine Zeile nach unten geschoben falls das moeglich ist.
       Kommt es dabei zu einer Kollision oder wird der untere Rand erreicht,
       so wird das Teil nicht verschoben sondern am Spielfeld verankert.
       Vollstaendige Zeilen werden entfernt und ein neues Teil am oberen
       Rand eingefuegt
     */
    if ((curPiece > 0) && ((millis()-last) > interval))
      {
        last = millis();
        if (checkPiece(curPiece,curCol,curRow+1))
          {
            hidePiece(curPiece,curCol,curRow);
            curRow++;
            showPiece(curPiece,curCol,curRow);
          }
          else
          {
            putPiece(curPiece,curCol,curRow);
            removeComplete();
            newPiece();
          }
      }
  }
