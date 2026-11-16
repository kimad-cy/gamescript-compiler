GAME Bad {
  PROPERTY speed: FLOAT = 5.5 @ 2;
  PROPERTY a: INTEGER = 1 # 2;
  IF a > 0 && a < 5 { PRINT "ok"; }
  IF ! alive { WIN; }
  PROPERTY é: STRING = "ok";
}
