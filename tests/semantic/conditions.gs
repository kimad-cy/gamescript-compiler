GAME T {
  ENTITY P {
    PROPERTY health: INTEGER = 10;
    ON START {
      IF health { PRINT 1; }
      WHILE 1 + 1 { PRINT 2; }
      IF health > 5 AND health { PRINT 3; }
    }
  }
}
