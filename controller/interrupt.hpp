#pragma once
#include <iostream>

class Interrupt {
public:
  Interrupt()
      : b_button_one(false), b_button_two(false), b_button_three(false),
        b_button_four(false), b_button_five(false), b_button_six(false),
        b_button_seven(false), b_button_eight(false), b_button_nine(false) {}
  virtual ~Interrupt() = default;

  virtual void ProcessInterrupt() { _ResetFlags(); };

  void PressOne() { b_button_one = true; }

protected:
  void _ResetFlags() {
    b_button_one = false;
    b_button_two = false;
    b_button_three = false;
    b_button_four = false;
    b_button_five = false;
    b_button_six = false;
    b_button_seven = false;
    b_button_eight = false;
    b_button_nine = false;
  }

  bool b_button_one;
  bool b_button_two;
  bool b_button_three;
  bool b_button_four;
  bool b_button_five;
  bool b_button_six;
  bool b_button_seven;
  bool b_button_eight;
  bool b_button_nine;
};
