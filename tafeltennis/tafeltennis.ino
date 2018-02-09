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

// max undo steps possible
#define MAX_UNDO 5

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

typedef enum {
  tabletennis_sets_11_e,
  tabletennis_sets_21_e,
  no_rules_e
} rules_t;

typedef struct {
  rules_t rules;
  uint8_t points_needed_to_win_set;
  uint8_t best_of;
} settings_t;

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

settings_t settings;
match_t match;

match_t undo_buffer[MAX_UNDO];
uint8_t undo_index;

char buffer[20];

State state_choose_sport(NULL, NULL, NULL);
State state_choose_sport_no_rules(NULL, NULL, NULL);
Fsm fsm(&state_choose_sport);

void drop_first_undo()
{
  for (uint8_t i = 0; i < undo_index - 1; ++i)
  {
    undo_buffer[i] = undo_buffer[i + 1];
  }
  --undo_index;
  /*
  Serial.print("drop_first_undo, index is now: ");
  Serial.println(undo_index);
  */
}

void push_undo()
{
  if (undo_index >= MAX_UNDO) {
    drop_first_undo();
  }
  undo_buffer[undo_index] = match;
  ++undo_index;
  /*
  Serial.print("push_undo, index is now: ");
  Serial.println(undo_index);
  */
}

void pop_undo()
{
  if (undo_index > 0) {
    --undo_index;
    match = undo_buffer[undo_index];
  }
  /*
  Serial.print("pop_undo, index is now: ");
  Serial.println(undo_index);
  */
}

void choose_sport_before()
{
  memset(&match, 0, sizeof(match_t));
  settings.rules = tabletennis_sets_11_e;
  settings.best_of = 5;
  
  lcd.clear();
  lcd.print("Soort wedstrijd?");

  choose_sport_table_tennis_11();
}

void choose_sport_table_tennis_11()
{
  settings.rules = tabletennis_sets_11_e;
  settings.points_needed_to_win_set = 11;
  
  lcd.setCursor(0, 1);
  lcd.print("Tafeltennis 11 ");
  lcd.write(byte(0));
}

void choose_sport_table_tennis_21()
{
  settings.rules = tabletennis_sets_21_e;
  settings.points_needed_to_win_set = 21;
  
  lcd.setCursor(0, 1);
  lcd.print("Tafeltennis 21 ");
  lcd.write(byte(0));
}

void choose_best_of()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Best of....?");
  lcd.setCursor(0, 1);
  lcd.print("5       3");
}

void choose_best_of_3()
{
    settings.best_of = 3;
}

void choose_best_of_5()
{
    settings.best_of = 5;  
}

void choose_sport_no_rules()
{
  settings.rules = no_rules_e;
  lcd.setCursor(0, 1);
  lcd.print("Vrij scorebord ");
  lcd.write(byte(0));
}

void new_match()
{
  match.left.score = match.left.sets = match.right.sets = match.right.score = 0;
  match.log_serve = true;
  match.left.started_match = true;
  match.swapped_during_final_set = false;

  undo_index = 0;
}

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
  match.right.started_match = false;
  new_set();
}

void manual_toss_player_rechts()
{
  match.left.started_match = false;
  match.right.started_match = true;
  new_set();
}

void auto_toss_random_player()
{
  led.clear();
  
  lcd.clear();
  lcd.print("Auto toss.....");
  for (uint8_t i = 0; i < 11; ++i)
  {
    led.putChar(i * 3, 0, '\'');
    
    lcd.setCursor(i, 1);
    lcd.print("*");
    delay(100);
  }
  lcd.clear();
  delay(500);

  match.left.started_match = random(0, 2) == 0;
  match.right.started_match = !match.left.started_match;
  new_set();
}

void print_score_lcd()
{
  lcd.clear();
  lcd.home();
  if (settings.rules == no_rules_e) {
    sprintf(buffer, "  %2d-%-2d    (%d-%d)", match.left.score, match.right.score, match.left.sets, match.right.sets);
    lcd.print(buffer);
    lcd.setCursor(0, 1);
    lcd.print("+ +  +  +     <>");    
  } else {
    sprintf(buffer, "%c %2d-%-2d %c  (%d-%d)", match.left.serves ? '*' : ' ', match.left.score, match.right.score, match.left.serves ? ' ' : '*', match.left.sets, match.right.sets);
    lcd.print(buffer);
    lcd.setCursor(0, 1);
    lcd.print("+       +    ");    
  }
  if (undo_index > 0) {
    lcd.print("<");
  }
}

void print_score_led()
{
  led.clear();

  if (settings.rules != no_rules_e) {
    buffer[0] = match.left.serves ? ' ' : '\'';
    led.putChar(0, 0, buffer[0]);
  }
  
  sprintf(buffer, "%2d-%-2d", match.right.score, match.left.score);
  led.putString(3, 0, buffer);

  if (settings.rules != no_rules_e) {
    buffer[0] = match.left.serves ? '\'' : ' ';
    led.putChar(30, 0, buffer[0]);
  
    buffer[0] = match.left.serves ? ' ' : '\'';
    led.putChar(0, 8, buffer[0]);
  }

  sprintf(buffer, "(%d-%d)", match.right.sets, match.left.sets);
  led.putString(4, 8, buffer);

  if (settings.rules != no_rules_e) {
    buffer[0] = match.left.serves ? '\'' : ' ';
    led.putChar(30, 8, buffer[0]);
  }
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

void increase_score_left()
{
  push_undo();
  ++ match.left.score;
}

void decrease_score_left()
{
  if (match.left.score > 0) {
    -- match.left.score;
  }
}

void increase_score_right()
{
  push_undo();
  ++ match.right.score;
}

void decrease_score_right()
{
  if (match.right.score > 0) {
    -- match.right.score;
  }
}

void increase_set_left()
{
  ++ match.left.sets;
}

void decrease_set_left()
{
  if (match.left.sets > 0) {
    -- match.left.sets;
  }
}

void increase_set_right()
{
  ++ match.right.sets;
}

void decrease_set_right()
{
  if (match.right.sets > 0) {
   -- match.right.sets;
  }
}

bool is_serve_changing() {
  uint8_t som = match.left.score + match.right.score;
  if (settings.rules == tabletennis_sets_11_e) {
    return (som < 20 && som % 2 == 0) || som >= 20;
  }
  return (som < 40 && som % 5 == 0) || som >= 40;
}

bool did_player_win_set(player_t *check, player_t *other)
{
  return (check->score >= settings.points_needed_to_win_set && (check->score - other->score >= 2));
}

bool is_match_complete()
{
  uint8_t needed_sets = (settings.best_of / 2) + 1;
  return (match.left.sets == needed_sets || match.right.sets == needed_sets);
}

bool swap_sides_during_final_set()
{
  uint8_t half_score = settings.points_needed_to_win_set / 2;
  uint8_t som_sets = match.left.sets + match.right.sets;
  return (match.swapped_during_final_set == false && som_sets == 4
          && (   (match.left.score == half_score && match.right.score < half_score)
                 || (match.left.score < half_score && match.right.score == half_score)));
}

void swap_scores() {
  player_t tmp = match.left;
  match.left = match.right;
  match.right = tmp;
}

void check_match_rules()
{
  if (settings.rules == no_rules_e) {
    return;
  }
  
  bool set_score_changed = false;

  if (is_serve_changing()) {
    match.left.serves = !match.left.serves;
    match.right.serves = !match.right.serves;
  }

  if (did_player_win_set(&match.left, &match.right)) {
    increase_set_left();
    set_score_changed = true;
  }
  else if (did_player_win_set(&match.right, &match.left)) {
    increase_set_right();
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
State state_choose_best_of(&choose_best_of, NULL, NULL);
State state_new_match(&new_match, NULL, NULL);
State state_ask_toss					(&ask_toss,						NULL, NULL);
State state_auto_toss_random_player		(&auto_toss_random_player,		NULL, NULL);
State state_manual_toss					(&manual_toss,					NULL, NULL);
State state_during_set					(&during_set,					NULL, NULL);
State state_during_set_no_rules (&during_set,         NULL, NULL);
State state_undo                (&undo,               NULL, NULL);
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

  fsm.add_transition(&state_choose_sport, &state_choose_sport, BTN_SCORE_LEFT, &choose_sport_table_tennis_11);
  fsm.add_transition(&state_choose_sport, &state_choose_sport, BTN_SET_LEFT, &choose_sport_table_tennis_21);
  fsm.add_transition(&state_choose_sport, &state_choose_sport_no_rules, BTN_SET_RIGHT, &choose_sport_no_rules);
  fsm.add_transition(&state_choose_sport, &state_choose_best_of, BTN_OK, NULL);
  
  fsm.add_transition(&state_choose_sport_no_rules, &state_choose_sport, BTN_SCORE_LEFT, &choose_sport_table_tennis_11);
  fsm.add_transition(&state_choose_sport_no_rules, &state_choose_sport, BTN_SET_LEFT, &choose_sport_table_tennis_21);
  fsm.add_transition(&state_choose_sport_no_rules, &state_during_set_no_rules, BTN_OK, &choose_sport_no_rules);
  
  fsm.add_transition(&state_choose_best_of, &state_new_match, BTN_SCORE_LEFT, &choose_best_of_5);
  fsm.add_transition(&state_choose_best_of, &state_new_match, BTN_SCORE_RIGHT, &choose_best_of_3);

  fsm.add_timed_transition(&state_new_match, &state_ask_toss, 0, NULL);

  fsm.add_transition(&state_ask_toss, &state_auto_toss_random_player, BTN_OK, NULL);
  fsm.add_transition(&state_ask_toss, &state_manual_toss, BTN_CANCEL, NULL);

  fsm.add_timed_transition(&state_auto_toss_random_player, &state_during_set, 0, NULL);

  fsm.add_transition(&state_manual_toss, &state_during_set, BTN_SCORE_LEFT, manual_toss_player_links);
  fsm.add_transition(&state_manual_toss, &state_during_set, BTN_SCORE_RIGHT, manual_toss_player_rechts);

  fsm.add_transition(&state_during_set, &state_check_match_rules, BTN_SCORE_LEFT, &increase_score_left);
  fsm.add_transition(&state_during_set, &state_check_match_rules, BTN_SCORE_RIGHT, &increase_score_right);
  fsm.add_transition(&state_during_set, &state_undo, BTN_CANCEL, NULL);
  fsm.add_transition(&state_during_set, &state_choose_sport, BTN_CANCEL_LONGPRESS, &choose_sport_before);

  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SCORE_LEFT, &increase_score_left);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SET_LEFT, &increase_set_left);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SET_RIGHT, &increase_set_right);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SCORE_RIGHT, &increase_score_right);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SCORE_LEFT_LONGPRESS, &decrease_score_left);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SET_LEFT_LONGPRESS, &decrease_set_left);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SET_RIGHT_LONGPRESS, &decrease_set_right);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_SCORE_RIGHT_LONGPRESS, &decrease_score_right);
  fsm.add_transition(&state_during_set_no_rules, &state_choose_sport, BTN_CANCEL_LONGPRESS, &choose_sport_before);
  fsm.add_transition(&state_during_set_no_rules, &state_during_set_no_rules, BTN_OK, &swap_scores);

  fsm.add_timed_transition(&state_undo, &state_during_set, 0, NULL);

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

  choose_sport_before();
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
