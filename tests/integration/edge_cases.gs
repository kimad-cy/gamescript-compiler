// Edge cases: else-if chains, widening, early return, defaults, escapes, empty blocks
GAME Edge {
  FUNCTION grade(score: INTEGER): STRING {
    IF score >= 90 { RETURN "A"; }
    ELSE IF score >= 80 { RETURN "B"; }
    ELSE { RETURN "C"; }
  }
  FUNCTION widen(x: FLOAT): FLOAT { RETURN x; }
  FUNCTION retWiden(): FLOAT { RETURN 3; }
  FUNCTION firstOver(limit: INTEGER): INTEGER {
    VAR i: INTEGER = 0;
    WHILE TRUE {
      IF i * i > limit { RETURN i; }
      i = i + 1;
    }
    RETURN -1;
  }
  ENTITY Empty { }
  ENTITY T {
    ON START {
      PRINT grade(95);
      PRINT grade(85);
      PRINT grade(10);
      PRINT widen(2);
      PRINT retWiden();
      PRINT firstOver(50);
      PRINT "line1\nline2 \"quoted\" \\ done";
      IF TRUE { }
      WHILE FALSE { }
      VAR s: STRING;
      VAR b: BOOLEAN;
      VAR v: VECTOR2;
      VAR n: INTEGER;
      VAR f: FLOAT;
      PRINT s;
      PRINT b;
      PRINT v;
      PRINT n;
      PRINT f;
      PRINT "a" == "a";
      PRINT VECTOR2(1, 2) == VECTOR2(1.0, 2.0);
      PRINT 3 == 3.0;
    }
  }
}
