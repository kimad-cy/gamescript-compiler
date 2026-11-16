// valid: shadowing, forward references, recursion, cross-entity access
GAME T {
  FUNCTION isEven(n: INTEGER): BOOLEAN {
    IF n == 0 { RETURN TRUE; }
    RETURN isOdd(n - 1);
  }
  FUNCTION isOdd(n: INTEGER): BOOLEAN {
    IF n == 0 { RETURN FALSE; }
    RETURN isEven(n - 1);
  }
  ENTITY A {
    PROPERTY x: INTEGER = 1;
    FUNCTION bump(by: INTEGER): INTEGER { x = x + by; RETURN x; }
    ON START {
      VAR x: INTEGER = 100;       // shadows property x
      PRINT x;
      PRINT SELF.x;
      PRINT B.y;
      PRINT B.twice(4);
      PRINT bump(2);
      PRINT isEven(10);
    }
  }
  ENTITY B {
    PROPERTY y: INTEGER = 7;
    FUNCTION twice(n: INTEGER): INTEGER { RETURN n * 2; }
    ON UPDATE { A.x = A.x + 1; }
  }
}
