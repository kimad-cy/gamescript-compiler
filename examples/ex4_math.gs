// Example 4: loops, recursion, FLOAT arithmetic, strings, vectors
GAME Math {

  FUNCTION fact(n: INTEGER): INTEGER {
    IF n <= 1 { RETURN 1; } ELSE { RETURN n * fact(n - 1); }
  }

  FUNCTION average(a: INTEGER, b: INTEGER): FLOAT {
    RETURN (a + b) / 2;
  }

  FUNCTION half(x: FLOAT): FLOAT {
    RETURN x / 2;
  }

  ENTITY Calculator {
    PROPERTY label: STRING = "factorial of 5 =";

    FUNCTION describe(): STRING {
      RETURN label + " (computed)";
    }

    ON START {
      VAR i: INTEGER = 1;
      WHILE i <= 3 { PRINT i; i = i + 1; }
      PRINT describe();
      PRINT fact(5);
      PRINT average(3, 4);
      PRINT half(5);
      PRINT 7 % 3;
      PRINT NOT (1 < 2) OR TRUE;
      VAR v: VECTOR2 = VECTOR2(1, 2) + VECTOR2(3, 4) * 2;
      PRINT v;
      PRINT v.x + v.y;
    }
  }
}
