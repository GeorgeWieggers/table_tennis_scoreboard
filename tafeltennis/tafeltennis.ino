#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HT1632_LedMatrix.h>
#include <MD_UISwitch.h>
//#include <LowPower.h>
#include <Fsm.h>

// hardware events
#define BTN_SCORE_LEFT 8
#define BTN_SET_LEFT 9
#define BTN_SET_RIGHT 10
#define BTN_SCORE_RIGHT 11
#define BTN_CANCEL 12
#define BTN_OK 13

// longpress mask
#define BTN_LONGPRESS 0x20
#define BTN_SCORE_LEFT_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_LEFT)
#define BTN_SET_LEFT_LONGPRESS (BTN_LONGPRESS | BTN_SET_LEFT)
#define BTN_SET_RIGHT_LONGPRESS (BTN_LONGPRESS | BTN_SET_RIGHT)
#define BTN_SCORE_RIGHT_LONGPRESS (BTN_LONGPRESS | BTN_SCORE_RIGHT)
#define BTN_CANCEL_LONGPRESS (BTN_LONGPRESS | BTN_CANCEL)
#define BTN_OK_LONGPRESS (BTN_LONGPRESS | BTN_OK)

// software events, triggered from within "match_rules" state
#define EVT_NEXT_SET 0x40
#define EVT_CONTINUE_SET 0x41
#define EVT_MATCH_COMPLETE 0x42
#define EVT_SWAP_SIDES 0x43

uint8_t DIGITAL_SWITCH_PINS[] =
{
  BTN_SCORE_LEFT,
  BTN_SET_LEFT,
  BTN_SET_RIGHT,
  BTN_SCORE_RIGHT,
  BTN_CANCEL,
  BTN_OK
};

MD_UISwitch_Digital DebouncedSwitches(DIGITAL_SWITCH_PINS, ARRAY_SIZE(DIGITAL_SWITCH_PINS), LOW);
LiquidCrystal_I2C lcd(0x27, 2, 16);
HT1632_LedMatrix led = HT1632_LedMatrix();

typedef struct {
  uint8_t score;
  uint8_t sets;
  bool serves;
  bool started_match;
} player_t;

typedef struct {
  player_t left;
  player_t right;
  bool log_serve;
  bool swapped_during_final_set;
} match_t;

match_t match;

#define MAX_UNDO 5
match_t undo_buffer[MAX_UNDO];
uint8_t undo_index;

char buffer[20];

void drop_first_undo()
{
  for (uint8_t i = 0; i < undo_index - 1; ++i)
  {
    undo_buffer[i] = undo_buffer[i + 1];
  }
  --undo_index;
  Serial.print("drop_first_undo, index is now: ");
  Serial.println(undo_index);
}

void push_undo()
{
  if (undo_index >= MAX_UNDO) {
    drop_first_undo();
  }
  undo_buffer[undo_index] = match;
  ++undo_index;
  Serial.print("push_undo, index is now: ");
  Serial.println(undo_index);
}

void pop_undo()
{
  if (undo_index > 0) {
    --undo_index;
    match = undo_buffer[undo_index];
  }
  Serial.print("pop_undo, index is now: ");
  Serial.println(undo_index);
}

void new_match()
{
  match.left.score = match.left.sets = match.right.sets = match.right.score = 0;
  match.log_serve = true;
  match.left.started_match = true;
  match.swapped_during_final_set = false;

  undo_index = 0;
}

State state_new_match(&new_match, NULL, NULL);
Fsm fsm(&state_new_match);

void ask_toss()
{
  lcd.clear();
  lcd.print("Auto toss?");
  lcd.setCursor(0, 1);
  lcd.print("             x ");
  lcd.write(byte(0));
  led.clear();
  led.putString(0, 0, "Toss...");
}

void manual_toss()
{
  lcd.clear();
  lcd.print("Wie slaat op?");
  lcd.setCursor(0, 1);
  lcd.print("<       >");
}

void manual_toss_player_links()
{
  match.left.started_match = true;
  new_set();
}

void manual_toss_player_rechts()
{
  match.left.started_match = false;
  new_set();
}

void auto_toss_random_player()
{
  lcd.clear();
  lcd.print("Auto toss.....");
  lcd.setCursor(0, 1);
  lcd.print("* Een moment");
  for (uint8_t i = 0; i < 20; ++i)
  {
    lcd.setCursor(0, 1);
    lcd.print(i % 2 == 0 ? " " : "*");
    lcd.setCursor(15, 1);
    lcd.print(i % 2 == 0 ? "*" : " ");
    delay(100);
  }
  lcd.clear();
  delay(500);

  match.left.started_match = random(0, 2) == 0;
  new_set();
}

void print_score_lcd()
{
  lcd.clear();
  lcd.home();
  sprintf(buffer, "%c %2d-%-2d %c  (%d-%d)", match.left.serves ? '*' : ' ', match.left.score, match.right.score, match.left.serves ? ' ' : '*', match.left.sets, match.right.sets);
  lcd.print(buffer);
  lcd.setCursor(0, 1);
  lcd.print("+       +    ");
  if (undo_index > 0) {
    lcd.print("<");
  }
}

void print_score_led()
{
  led.clear();

  buffer[0] = match.left.serves ? ' ' : '\'';
  led.putChar(0, 0, buffer[0]);

  sprintf(buffer, "%2d-%-2d", match.right.score, match.left.score);
  led.putString(3, 0, buffer);

  buffer[0] = match.left.serves ? '\'' : ' ';
  led.putChar(30, 0, buffer[0]);

  buffer[0] = match.left.serves ? ' ' : '\'';
  led.putChar(0, 8, buffer[0]);

  sprintf(buffer, "(%d-%d)", match.right.sets, match.left.sets);
  led.putString(4, 8, buffer);

  buffer[0] = match.left.serves ? '\'' : ' ';
  led.putChar(30, 8, buffer[0]);
}

void new_set()
{
  match.left.score = 0;
  match.right.score = 0;
  match.left.serves = match.left.started_match;
}

void undo()
{
  pop_undo();
}

void during_set()
{
  print_score_lcd();
  print_score_led();
}

void score_left()
{
  ++ match.left.score;
}

void score_right()
{
  ++ match.right.score;
}

bool is_serve_changing() {
  uint8_t som = match.left.score + match.right.score;
  return (som < 20 && som % 2 == 0) || som >= 20;
}

bool did_player_win_set(player_t *check, player_t *other)
{
  return (check->score >= 11 && (check->score - other->score >= 2));
}

bool is_match_complete()
{
  return (match.left.sets == 3 || match.right.sets == 3);
}

bool swap_sides_during_final_set()
{
  uint8_t som_sets = match.left.sets + match.right.sets;
  return (match.swapped_during_final_set == false && som_sets == 4
          && (   (match.left.score == 5 && match.right.score < 5)
                 || (match.left.score < 5 && match.right.score == 5)));
}

void swap_scores() {
  player_t tmp = match.left;
  match.left = match.right;
  match.right = tmp;
}

void check_match_rules()
{
  bool set_score_changed = false;

  if (is_serve_changing()) {
    match.left.serves = !match.left.serves;
    match.right.serves = !match.right.serves;
  }

  if (did_player_win_set(&match.left, &match.right)) {
    ++match.left.sets;
    set_score_changed = true;
  }
  else if (did_player_win_set(&match.right, &match.left)) {
    ++match.right.sets;
    set_score_changed = true;
  }

  if (swap_sides_during_final_set()) {
    match.swapped_during_final_set = true;
    fsm.trigger(EVT_SWAP_SIDES);
  }
  else if (set_score_changed) {
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

  lcd.setCursor(0, 1);
  lcd.print("Eindstand    < ");
  lcd.write(byte(0));
}

void confirm_new_set()
{
  print_score_lcd();
  print_score_led();

  lcd.setCursor(0, 1);
  lcd.print("Setstand     < ");
  lcd.write(byte(0));
}

void confirm_new_set_after()
{
  new_set();
}

void confirm_new_match()
{
  lcd.clear();
  lcd.home();
  lcd.print("Nieuw spel?");
  lcd.setCursor(0, 1);
  lcd.print("             x ");
  lcd.write(byte(0));
}

void swap_sides()
{
  swap_scores();

  lcd.clear();
  lcd.home();
  lcd.print("Wissel van");
  lcd.setCursor(0, 1);
  lcd.print("speelhelft   < ");
  lcd.write(byte(0));

  led.clear();
  led.putString(0, 0, "Wissel..");
  led.putString(0, 8, "<<  >>");
}

// State::State(void (*on_enter)(), void (*on_state)(), void (*on_exit)())
State state_ask_toss					(&ask_toss,						NULL, NULL);
State state_auto_toss_random_player		(&auto_toss_random_player,		NULL, NULL);
State state_manual_toss					(&manual_toss,					NULL, NULL);
State state_manual_toss_player_links	(&manual_toss_player_links,		NULL, NULL);
State state_manual_toss_player_rechts	(&manual_toss_player_rechts,	NULL, NULL);
State state_during_set					(&during_set,					NULL, NULL);
State state_undo                (&undo,               NULL, NULL);
State state_score_left					(&score_left,					NULL, NULL);
State state_score_right					(&score_right,					NULL, NULL);
State state_confirm_new_set				(&confirm_new_set,				NULL, &confirm_new_set_after);
State state_match_complete				(&match_complete,				NULL, NULL);
State state_check_match_rules			(NULL,							&check_match_rules, NULL);
State state_swap_sides					(&swap_sides,					NULL, NULL);
State state_confirm_new_match			(&confirm_new_match,			NULL, NULL);


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

  led.init(1, 2);
  led.clear();
  led.setBrightness(15);

  DebouncedSwitches.begin();
  DebouncedSwitches.enableRepeat(false);
  DebouncedSwitches.enableRepeatResult(false);
  DebouncedSwitches.enableDoublePress(true);
  DebouncedSwitches.setDoublePressTime(400);
  DebouncedSwitches.enableLongPress(true);
  DebouncedSwitches.setLongPressTime(750);

  fsm.add_timed_transition(&state_new_match, &state_ask_toss, 0, NULL);

  fsm.add_transition(&state_ask_toss, &state_auto_toss_random_player, BTN_OK, NULL);
  fsm.add_transition(&state_ask_toss, &state_manual_toss, BTN_CANCEL, NULL);

  fsm.add_timed_transition(&state_auto_toss_random_player, &state_during_set, 0, NULL);

  fsm.add_transition(&state_manual_toss, &state_manual_toss_player_links, BTN_SCORE_LEFT, NULL);
  fsm.add_transition(&state_manual_toss, &state_manual_toss_player_rechts, BTN_SCORE_RIGHT, NULL);

  fsm.add_timed_transition(&state_manual_toss_player_links, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_manual_toss_player_rechts, &state_during_set, 0, NULL);

  fsm.add_transition(&state_during_set, &state_score_left, BTN_SCORE_LEFT, &push_undo);
  fsm.add_transition(&state_during_set, &state_score_right, BTN_SCORE_RIGHT, &push_undo);
  fsm.add_transition(&state_during_set, &state_undo, BTN_CANCEL, NULL);
  fsm.add_transition(&state_during_set, &state_confirm_new_match, BTN_CANCEL_LONGPRESS, NULL);

  fsm.add_timed_transition(&state_undo, &state_during_set, 0, NULL);
  fsm.add_timed_transition(&state_score_left, &state_check_match_rules, 0, NULL);
  fsm.add_timed_transition(&state_score_right, &state_check_match_rules, 0, NULL);

  fsm.add_transition(&state_check_match_rules, &state_during_set, EVT_CONTINUE_SET, NULL);
  fsm.add_transition(&state_check_match_rules, &state_confirm_new_set, EVT_NEXT_SET, NULL);
  fsm.add_transition(&state_check_match_rules, &state_swap_sides, EVT_SWAP_SIDES, NULL);
  fsm.add_transition(&state_check_match_rules, &state_match_complete, EVT_MATCH_COMPLETE, NULL);

  fsm.add_transition(&state_confirm_new_set, &state_swap_sides, BTN_OK, NULL);
  fsm.add_transition(&state_confirm_new_set, &state_undo, BTN_CANCEL, NULL);

  fsm.add_transition(&state_swap_sides, &state_during_set, BTN_OK, NULL);
  fsm.add_transition(&state_swap_sides, &state_undo, BTN_CANCEL, NULL);

  fsm.add_transition(&state_match_complete, &state_new_match, BTN_OK, NULL);
  fsm.add_transition(&state_match_complete, &state_undo, BTN_CANCEL, NULL);

  fsm.add_transition(&state_confirm_new_match, &state_during_set, BTN_CANCEL, NULL);
  fsm.add_transition(&state_confirm_new_match, &state_new_match, BTN_OK, NULL);

}

void loop(void)
{
  //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);

  fsm.run_machine();

  switch (DebouncedSwitches.read())
  {
    case MD_UISwitch::KEY_PRESS:
      fsm.trigger(DebouncedSwitches.getKey());
      break;
    case MD_UISwitch::KEY_LONGPRESS:
      fsm.trigger(DebouncedSwitches.getKey() | BTN_LONGPRESS);
      break;
  }

  if (Serial.available() > 0) {
    switch (Serial.read()) {
      case '1': fsm.trigger(BTN_SCORE_LEFT); break;
      case '2': fsm.trigger(BTN_SET_LEFT); break;
      case '3': fsm.trigger(BTN_SET_RIGHT); break;
      case '4': fsm.trigger(BTN_SCORE_RIGHT); break;
      case '5': fsm.trigger(BTN_CANCEL); break;
      case '6': fsm.trigger(BTN_OK); break;
      case '!': fsm.trigger(BTN_SCORE_LEFT | BTN_LONGPRESS); break;
      case '@': fsm.trigger(BTN_SET_LEFT | BTN_LONGPRESS); break;
      case '#': fsm.trigger(BTN_SET_RIGHT | BTN_LONGPRESS); break;
      case '$': fsm.trigger(BTN_SCORE_RIGHT | BTN_LONGPRESS); break;
      case '%': fsm.trigger(BTN_CANCEL | BTN_LONGPRESS); break;
      case '^': fsm.trigger(BTN_OK | BTN_LONGPRESS); break;
    }
  }
}
