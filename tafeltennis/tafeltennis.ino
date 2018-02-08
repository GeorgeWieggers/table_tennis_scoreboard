#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HT1632_LedMatrix.h>
#include <MD_UISwitch.h>
//#include <LowPower.h>
#include <Fsm.h>


#define BTN_SCORE_LINKS 8
#define BTN_SET_LINKS 9
#define BTN_SET_RECHTS 10
#define BTN_SCORE_RECHTS 11
#define BTN_CANCEL 12
#define BTN_OK 13

#define EVT_NEXT_SET 0x40
#define EVT_CONTINUE_SET 0x41
#define EVT_MATCH_COMPLETE 0x42
#define EVT_CHANGE_SIDES 0x43

#define BTN_LONGPRESS 0x10
#define BTN_SCORE_LINKS_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_LINKS)
#define BTN_SET_LINKS_LONGPRESS (BTN_LONGPRESS | BTN_SET_LINKS)
#define BTN_SET_RECHTS_LONGPRESS (BTN_LONGPRESS | BTN_SET_RECHTSCORE_LINKS)
#define BTN_SCORE_RECHTS_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_RECHTS)
#define BTN_CANCEL_LONGPRESS (BTN_LONGPRESS | BTN_CANCEL)
#define BTN_OK_LONGPRESS (BTN_LONGPRESS | BTN_OK)

uint8_t DIGITAL_SWITCH_PINS[] = 
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
  bool opslag_links;
} score_t;

score_t score;

char buffer[20];
char* msg = "Test";
uint8_t crtPos = 0;
int msgx = 1;

void new_match()
{
  score.score_links = score.set_links = score.set_rechts = score.score_rechts = 0;
  score.opslag_bijhouden = true;
  score.links_is_begonnen = true;
}

State state_new_match(&new_match, NULL, NULL);
Fsm fsm(&state_new_match);

void ask_toss()
{
  lcd.clear();
  lcd.print("Auto toss?");  
  lcd.setCursor(0,1);
  lcd.print("             x ");
  lcd.write(byte(0));
}

void manual_toss()
{
  lcd.clear();
  lcd.print("Wie slaat op?");
  lcd.setCursor(0,1);
  lcd.print("<       >       ");
}

void manual_toss_speler_links()
{
  score.links_is_begonnen = true;
  new_set();
}

void manual_toss_speler_rechts()
{
  score.links_is_begonnen = false;
  new_set();
}

void auto_toss()
{
  lcd.clear();
  lcd.print("Auto toss.....");
  lcd.setCursor(0,1);
  lcd.print("* Een moment");
  for (uint8_t i = 0; i < 20; ++i)
  {
    lcd.setCursor(0,1);
    lcd.print(i%2 == 0 ? " ": "*");
    lcd.setCursor(15,1);
    lcd.print(i%2 == 0 ? "*": " ");
    delay(100);
  }
  lcd.clear();
  delay(500);
  score.links_is_begonnen = random(0, 2) == 0;
  new_set();
}

void print_score_lcd()
{
  lcd.clear();
  lcd.home();
  sprintf(buffer, "%c %2d-%-2d %c  (%d-%d)", score.opslag_links ? '*':' ', score.score_links, score.score_rechts, score.opslag_links ? ' ':'*', score.set_links, score.set_rechts);
  lcd.print(buffer);
  lcd.setCursor(0,1);
  lcd.print("+       +       ");
}

void print_score_led()
{
  led.clear();
  sprintf(buffer, "%c%02d-%02d%c", score.opslag_links ? ' ':'\'', score.score_rechts, score.score_links, score.opslag_links ? '\'':' ');
  led.putString(0,0,buffer);
  sprintf(buffer, "(%d-%d)", score.set_rechts, score.set_links);
  led.putString(5,8,buffer);
}

void new_set()
{
  score.score_links = 0;
  score.score_rechts = 0;
  score.opslag_links = score.links_is_begonnen;  
}

void during_set()
{
  print_score_lcd();
  print_score_led();
}

void score_links()
{
  ++ score.score_links;
}

void score_rechts()
{
  ++ score.score_rechts;  
}

bool wisselt_opslag() {
  uint8_t som = score.score_links + score.score_rechts;
  return (som < 20 && som %2 == 0) || som >=20;
}

bool set_gewonnen(uint8_t eerste, uint8_t tweede)
{
  return (eerste >= 11 && (eerste - tweede >= 2));
}

bool is_match_complete()
{
  return (score.set_links == 3 || score.set_rechts == 3);
}

void wissel_van_kant() {
  uint8_t tmp_score = score.score_links;
  uint8_t tmp_set = score.set_links;

  score.score_links = score.score_rechts;
  score.set_links = score.set_rechts;

  score.score_rechts = tmp_score;
  score.set_rechts = tmp_set;

  score.opslag_links = !score.opslag_links;
}

void check_spelregels()
{
  bool setScoreChanged = false;

  if (wisselt_opslag()) {
    score.opslag_links = !score.opslag_links;
  }
  
  if (set_gewonnen(score.score_links, score.score_rechts)) {
    ++score.set_links;
    setScoreChanged = true;
  }
  else if (set_gewonnen(score.score_rechts, score.score_links)) {
    ++score.set_rechts;
    setScoreChanged = true;
  }
  
  if (setScoreChanged) {
    if (is_match_complete()) {
      fsm.trigger(EVT_MATCH_COMPLETE);
    } else {
      fsm.trigger(EVT_NEXT_SET);    
    }
  }
  else {
    fsm.trigger(EVT_CONTINUE_SET);
  }  
}

void match_complete()
{
  print_score_lcd();
  print_score_led();

  lcd.setCursor(0,1);
  lcd.print("Eindstand      ");
  lcd.write(byte(0));  
}

void confirm_new_set()
{
  print_score_lcd();
  print_score_led();

  lcd.setCursor(0,1);
  lcd.print("Setstand       ");
  lcd.write(byte(0));  
}

void confirm_new_set_after()
{
  wissel_van_kant();
  new_set();
}

void confirm_new_match()
{
  lcd.clear();
  lcd.home();
  lcd.print("Nieuw spel?");      
  lcd.setCursor(0,1);
  lcd.print("             x ");
  lcd.write(byte(0));
}

void change_sides()
{
  
}

// State::State(void (*on_enter)(), void (*on_state)(), void (*on_exit)())
State state_ask_toss(&ask_toss, NULL, NULL);
State state_auto_toss(&auto_toss, NULL, NULL);
State state_manual_toss(&manual_toss, NULL, NULL);
State state_manual_toss_speler_links(&manual_toss_speler_links, NULL, NULL);
State state_manual_toss_speler_rechts(&manual_toss_speler_rechts, NULL, NULL);
State state_during_set(&during_set, NULL, NULL);
State state_score_links(&score_links, NULL, NULL);
State state_score_rechts(&score_rechts, NULL, NULL);
State state_confirm_new_set(&confirm_new_set, NULL, &confirm_new_set_after);
State state_match_complete(&match_complete, NULL, NULL);
State state_check_spelregels(NULL, &check_spelregels, NULL);
State state_change_sides(&change_sides, NULL, NULL);
State state_confirm_new_match(&confirm_new_match, NULL, NULL);

byte checkChar[8] = {
  B00000,
  B00001,
  B00001,
  B00010,
  B10010,
  B01100,
  B00100,
};


void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(0, checkChar);

  led.init(1,2);
  led.clear();  
  led.setBrightness(15);

  DebouncedSwitches.begin();
  DebouncedSwitches.enableRepeat(false);
  DebouncedSwitches.enableRepeatResult(false);
  DebouncedSwitches.enableDoublePress(false);
  //DebouncedSwitches.setDoublePressTime(200);
  DebouncedSwitches.enableLongPress(true);
  DebouncedSwitches.setLongPressTime(750);

  fsm.add_timed_transition(&state_new_match, &state_ask_toss, 0, NULL);
  fsm.add_transition(&state_ask_toss, &state_auto_toss, BTN_OK, NULL);
  fsm.add_transition(&state_ask_toss, &state_manual_toss, BTN_CANCEL, NULL);
  
  fsm.add_transition(&state_manual_toss, &state_manual_toss_speler_links, BTN_SCORE_LINKS, NULL);
  fsm.add_transition(&state_manual_toss, &state_manual_toss_speler_rechts, BTN_SCORE_RECHTS, NULL);

  fsm.add_timed_transition(&state_auto_toss, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_manual_toss_speler_links, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_manual_toss_speler_rechts, &state_during_set, 0, NULL);

  fsm.add_transition(&state_during_set, &state_score_links, BTN_SCORE_LINKS, NULL);
  fsm.add_transition(&state_during_set, &state_score_rechts, BTN_SCORE_RECHTS, NULL);
  fsm.add_transition(&state_during_set, &state_confirm_new_match, BTN_CANCEL_LONGPRESS, NULL);

  fsm.add_transition(&state_confirm_new_set, &state_during_set, BTN_OK, NULL);

  fsm.add_timed_transition(&state_score_links, &state_check_spelregels, 0, NULL);
  fsm.add_timed_transition(&state_score_rechts, &state_check_spelregels, 0, NULL);

  fsm.add_transition(&state_check_spelregels, &state_confirm_new_set, EVT_NEXT_SET, NULL);
  fsm.add_transition(&state_check_spelregels, &state_change_sides, EVT_CHANGE_SIDES, NULL);
  fsm.add_transition(&state_check_spelregels, &state_during_set, EVT_CONTINUE_SET, NULL);
  fsm.add_transition(&state_check_spelregels, &state_match_complete, EVT_MATCH_COMPLETE, NULL);

  fsm.add_transition(&state_change_sides, &state_during_set, BTN_OK, NULL);

  fsm.add_transition(&state_match_complete, &state_new_match, BTN_OK, NULL);

  fsm.add_transition(&state_confirm_new_match, &state_during_set, BTN_CANCEL, NULL);
  fsm.add_transition(&state_confirm_new_match, &state_new_match, BTN_OK, NULL);

}

void loop(void)
{
  //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF); 
  //displayScrollingLine();  
  
  fsm.run_machine();

  switch(DebouncedSwitches.read())
  {
    case MD_UISwitch::KEY_PRESS:
      Serial.println("key_press");
      fsm.trigger(DebouncedSwitches.getKey());
      break;
    case MD_UISwitch::KEY_LONGPRESS:
      Serial.println("long_press");
      fsm.trigger(DebouncedSwitches.getKey() | BTN_LONGPRESS);
      break;
  }

  if (Serial.available() > 0) {
    char incoming = Serial.read();
    switch (incoming) {
      case '1': fsm.trigger(BTN_SCORE_LINKS); break;
      case '2': fsm.trigger(BTN_SET_LINKS); break;
      case '3': fsm.trigger(BTN_SET_RECHTS); break;
      case '4': fsm.trigger(BTN_SCORE_RECHTS); break;
      case '5': fsm.trigger(BTN_CANCEL); break;
      case '6': fsm.trigger(BTN_OK); break;
      case '!': fsm.trigger(BTN_SCORE_LINKS | BTN_LONGPRESS); break;
      case '@': fsm.trigger(BTN_SET_LINKS | BTN_LONGPRESS); break;
      case '#': fsm.trigger(BTN_SET_RECHTS | BTN_LONGPRESS); break;
      case '$': fsm.trigger(BTN_SCORE_RECHTS | BTN_LONGPRESS); break;
      case '%': fsm.trigger(BTN_CANCEL | BTN_LONGPRESS); break;
      case '^': fsm.trigger(BTN_OK | BTN_LONGPRESS); break;
    }
  }
}


//void setup() {}
//void loop() {}

