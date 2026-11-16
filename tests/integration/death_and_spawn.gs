// ON DEATH, DESTROY of another entity, methods called across entities, collision edge-trigger
GAME Life {
  ENTITY Spike {
    PROPERTY hits: INTEGER = 0;
    FUNCTION touch(): INTEGER { hits = hits + 1; RETURN hits; }
    ON START { position = VECTOR2(50, 0); }
    ON UPDATE { MOVE LEFT BY 10; }
  }
  ENTITY Hero {
    PROPERTY hp: INTEGER = 1;
    ON COLLISION WITH Spike {
      hp = hp - Spike.touch();
      PRINT hp;
      IF hp <= 0 { DESTROY SELF; }
    }
    ON DEATH { PRINT "hero died"; Spike.hits = 100; }
  }
}
