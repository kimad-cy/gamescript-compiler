// compiles fine, but produces warnings
GAME T {
  FUNCTION f(): INTEGER {
    VAR unused: INTEGER = 1;
    RETURN 1;
    PRINT "never runs";
  }
  ENTITY Idle { PROPERTY x: INTEGER = 1; }
  ENTITY Active { ON START { PRINT f(); } }
}
