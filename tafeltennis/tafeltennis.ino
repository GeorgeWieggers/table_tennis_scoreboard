#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HT1632_LedMatrix.h>
#include <MD_UISwitch.h>
#include <LowPower.h>
#include <Fsm.h>


#define BTN_SCORE_LINKS PD2
#define BTN_SET_LINKS PD3
#define BTN_SET_RECHTS PD4
#define BTN_SCORE_RECHTS PD5
#define BTN_CANCEL PD6
#define BTN_OK PD7

#define BTN_LONGPRESS 0x10
#define BTN_SCORE_LINKS_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_LINKS)
#define BTN_SET_LINKS_LONGPRESS (BTN_LONGPRESS | BTN_SET_LINKS)
#define BTN_SET_RECHTS_LONGPRESS (BTN_LONGPRESS | BTN_SET_RECHTSCORE_LINKS)
#define BTN_SCORE_RECHTS_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_RECHTS)
#define BTN_CANCEL_LONGPRESS (BTN_LONGPRESS | BTN_CANCEL)
#define BTN_OK_LONGPRESS (BTN_LONGPRESS | BTN_OK)

const uint8_t DIGITAL_SWITCH_PINS[] = 
{ 
  BTN_SCORE_LINKS, 
  BTN_SET_LINKS, 
  BTN_SET_RECHTS, 
  BTN_SCORE_RECHTS, 
  BTN_CANCEL, 
  BTN_OK 
};

MD_UISwitch_Digital DebouncedSwitches(DIGITAL_SWITCH_PINS, ARRAY_SIZE(DIGITAL_SWITCH_PINS), LOW);
LiquidCrystal_I2C lcd(0x27,2,16);
HT1632_LedMatrix led = HT1632_LedMatrix();

typedef struct {
  uint8_t score_links;
  uint8_t set_links;
  uint8_t set_rechts;
  uint8_t score_rechts;
  bool opslag_bijhouden;
  bool links_is_begonnen;
} score_t;

score_t score;

/*
void printScore()
{
  char buffer[50];
  //lcd.clear();
  lcd.home();
  uint8_t som = score.score_links + score.score_rechts;
  bool speler_links_is_aan_opslag = ((som) / 2) %2 == 0;
  if (som >= 20) {
    speler_links_is_aan_opslag = (som) %2 == 0;
  }
  if (!score.links_is_begonnen) {
    speler_links_is_aan_opslag = !speler_links_is_aan_opslag; // invert results
  }
  
  sprintf(buffer, "%c %-2d (%d  %d) %2d %c", speler_links_is_aan_opslag ? '*':' ', score.score_links, score.set_links, score.set_rechts, score.score_rechts, speler_links_is_aan_opslag ? ' ':'*');
  lcd.print(buffer);
}
*/


char* msg = "Test";
uint8_t crtPos = 0;
int msgx = 1;

void displayScrollingLine()
{
  int y,xmax,ymax;
  led.getXYMax(&xmax,&ymax);
  // shift the whole screen 6 times, one column at a time;
  for (uint8_t x=0; x < 6; x++)
  {
    led.scrollLeft(1);
    msgx--;
    // fit as much as we can on
    
    while (!led.putChar(msgx,0,msg[crtPos]))  // zero return if it all fitted
    {
      led.getXY(&msgx,&y);
      crtPos++; // we got all of the character on!!
      if (crtPos >= strlen(msg))
      {
        crtPos = 0;
      }
    }
    led.putShadowRam();    
  }
}


void new_match()
{
  score.score_links = score.set_links = score.set_rechts = score.score_rechts = 0;
  score.opslag_bijhouden = true;
  score.links_is_begonnen = true;
}

void ask_toss()
{
  lcd.clear();
  lcd.print("Auto toss?");  
  lcd.setCursor(0,1);
  lcd.print("             N/J");
}

void manual_toss()
{
  lcd.clear();
  lcd.print("Wie slaat op?");
  lcd.setCursor(0,1);
  lcd.print("<       >");
}

void manual_toss_speler_links()
{
  score.links_is_begonnen = true;
}

void manual_toss_speler_rechts()
{
  score.links_is_begonnen = false;
}

void auto_toss()
{
  lcd.clear();
  lcd.print("Auto toss.....");
  lcd.setCursor(0,1);
  lcd.print("* Een moment");
  for (uint8_t i = 0; i < 10; ++i)
  {
    lcd.setCursor(0,1);
    lcd.print(" ");
    lcd.setCursor(15,1);
    lcd.print("*");
    delay(100);
    lcd.setCursor(0,1);
    lcd.print("*");
    lcd.setCursor(15,1);
    lcd.print(" ");
    delay(100);
  }
  lcd.clear();
  delay(500);
  score.links_is_begonnen = random(0, 2) == 0;
}

void during_set()
{
  lcd.clear();
  lcd.home();
  lcd.print("Set bezig");  
}

void confirm_new_match()
{
  lcd.clear();
  lcd.home();
  lcd.print("Nieuw spel");      
  lcd.setCursor(0,1);
  lcd.print("starten?     N/J");
}

// State::State(void (*on_enter)(), void (*on_state)(), void (*on_exit)())
State state_new_match(&new_match, NULL, NULL);
State state_ask_toss(&ask_toss, NULL, NULL);
State state_auto_toss(&auto_toss, NULL, NULL);
State state_manual_toss(&manual_toss, NULL, NULL);
State state_manual_toss_speler_links(&manual_toss_speler_links, NULL, NULL);
State state_manual_toss_speler_rechts(&manual_toss_speler_rechts, NULL, NULL);
State state_during_set(&during_set, NULL, NULL);
State state_confirm_new_match(&confirm_new_match, NULL, NULL);

Fsm fsm(&state_new_match);

void setup()
{
  lcd.init();
  lcd.backlight();
  lcd.clear();

  led.init(2,1);
  led.clear();  
  led.setBrightness(15);

  DebouncedSwitches.begin();
  DebouncedSwitches.enableRepeat(false);
  DebouncedSwitches.enableRepeatResult(false);
  DebouncedSwitches.enableDoublePress(true);
  DebouncedSwitches.setDoublePressTime(300);
  DebouncedSwitches.enableLongPress(true);
  DebouncedSwitches.setLongPressTime(1000);

  fsm.add_timed_transition(&state_new_match, &state_ask_toss, 0, NULL);
  fsm.add_transition(&state_ask_toss, &state_auto_toss, BTN_OK, NULL);
  fsm.add_transition(&state_ask_toss, &state_manual_toss, BTN_CANCEL, NULL);
  
  fsm.add_transition(&state_manual_toss, &state_manual_toss_speler_links, BTN_SCORE_LINKS, NULL);
  fsm.add_transition(&state_manual_toss, &state_manual_toss_speler_rechts, BTN_SCORE_RECHTS, NULL);

  fsm.add_timed_transition(&state_auto_toss, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_manual_toss_speler_links, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_manual_toss_speler_rechts, &state_during_set, 0, NULL);

  fsm.add_transition(&state_during_set, &state_confirm_new_match, BTN_CANCEL_LONGPRESS, NULL);
  fsm.add_transition(&state_confirm_new_match, &state_during_set, BTN_CANCEL, NULL);
  fsm.add_transition(&state_confirm_new_match, &state_new_match, BTN_OK, NULL);

}

void loop(void)
{
  //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF); 
  displayScrollingLine();  
  
  fsm.run_machine();


  switch(DebouncedSwitches.read())
  {
    case MD_UISwitch::KEY_PRESS:
      fsm.trigger(DebouncedSwitches.getKey());
      break;
    case MD_UISwitch::KEY_LONGPRESS:
      fsm.trigger(DebouncedSwitches.getKey() | BTN_LONGPRESS);
      break;
  }
}


//void setup() {}
//void loop() {}

