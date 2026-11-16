GAME T {
  ENTITY P {
    PROPERTY a: INTEGER = 1;
    PROPERTY b: INTEGER = a + 1;
    PROPERTY c: FLOAT = 1 / 0;
    PROPERTY d: INTEGER = 5 % 0;
    ON START { PRINT 10 / 0; PRINT 10 / 0.0; PRINT 4 % (0); PRINT 2 / -0; PRINT 1 / (1 - 1); }
  }
}
