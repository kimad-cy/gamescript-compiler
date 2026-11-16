// Division by a variable that is zero at run time (not detectable at compile time)
GAME Boom {
  ENTITY T {
    PROPERTY zero: INTEGER = 0;
    ON START { PRINT "before"; }
    ON UPDATE { PRINT 10 / zero; }
  }
}
