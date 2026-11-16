GAME T {
  ENTITY P {
    PROPERTY score: INTEGER = 0;
    ON START { WIN; LOSE; }
    ON COLLISION WITH Ghost { PRINT 1; }
    ON KEY_PRESS "FIRE" { PRINT 2; }
    ON START { PRINT 3; }
    ON UPDATE { PRINT 4; }
    ON UPDATE { PRINT 5; }
    ON KEY_PRESS "A" { PRINT 6; }
    ON KEY_PRESS "A" { PRINT 7; }
    ON DEATH { DESTROY SELF; }
  }
  RULE R {
    P.score = 1;
    MOVE P UP BY 1;
    SPAWN P AT VECTOR2(0, 0);
    DESTROY P;
    PLAY SOUND "x.wav";
    IF P.score > 0 { WIN; }
  }
  RULE R { LOSE; }
}
