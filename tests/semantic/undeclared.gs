GAME T {
  ENTITY P {
    PROPERTY health: INTEGER = 10;
    ON START {
      health = hp + 1;
      PRINT ghost;
      PRINT Missing.x;
      PRINT P.nothing;
      undefinedFn(3);
    }
  }
}
