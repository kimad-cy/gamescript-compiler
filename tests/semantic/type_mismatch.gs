GAME T {
  ENTITY P {
    PROPERTY name: STRING = 10;
    PROPERTY speed: FLOAT = 5;
    PROPERTY count: INTEGER = 2.5;
    PROPERTY alive: BOOLEAN = TRUE;
    ON START {
      VAR i: INTEGER = "text";
      VAR f: FLOAT = 3;
      count = speed;
      alive = 1;
      speed = count;
      PRINT "a" + 1;
      PRINT TRUE + FALSE;
      PRINT 5 % 2.0;
      PRINT NOT 3;
      PRINT -"x";
      PRINT 1 < "a";
      PRINT alive == 1;
      PRINT f;
      PRINT i;
    }
  }
}
