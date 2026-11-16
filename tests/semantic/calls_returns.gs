GAME T {
  FUNCTION add(a: INTEGER, b: INTEGER): INTEGER { RETURN a + b; }
  FUNCTION noValue() { PRINT 1; }
  FUNCTION bad1(): INTEGER { PRINT 1; }
  FUNCTION bad2(x: INTEGER): INTEGER { IF x > 0 { RETURN 1; } }
  FUNCTION bad3(): INTEGER { RETURN "s"; }
  FUNCTION bad4() { RETURN 5; }
  FUNCTION bad5(): INTEGER { RETURN; }
  FUNCTION ok1(x: INTEGER): INTEGER { IF x > 0 { RETURN 1; } ELSE { RETURN 2; } }
  ENTITY P {
    ON START {
      PRINT add(1);
      PRINT add(1, "x");
      PRINT add(1, 2, 3);
      PRINT noValue();
      PRINT 1 + noValue();
      PRINT P.nothing();
      PRINT Q.add(1, 2);
      PRINT add;
      PRINT ok1(1);
    }
  }
}
