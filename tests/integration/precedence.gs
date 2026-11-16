// Operator precedence and associativity, verified by execution
GAME Precedence {
  ENTITY T {
    ON START {
      PRINT 2 + 3 * 4;
      PRINT (2 + 3) * 4;
      PRINT 10 - 4 - 3;
      PRINT 100 / 10 / 5;
      PRINT -2 * 3;
      PRINT 7 % 4 * 2;
      PRINT NOT FALSE AND FALSE;
      PRINT TRUE OR FALSE AND FALSE;
      PRINT NOT 1 == 2;
      PRINT 1 + 2 < 4 AND 2 * 2 == 4;
      PRINT 2 - -3;
      PRINT 1 + 2 * 3 - 4 / 2;
    }
  }
}
