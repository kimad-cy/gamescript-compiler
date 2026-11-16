GAME T {
  FUNCTION global(): INTEGER {
    MOVE UP BY 1;
    DESTROY SELF;
    PRINT SELF.position;
    RETURN 1;
  }
  ENTITY P {
    PROPERTY v: VECTOR2 = VECTOR2(1, 2);
    ON START {
      SPAWN P AT 5;
      SPAWN Nobody AT VECTOR2(1, 1);
      MOVE UP BY "far";
      MOVE Nobody LEFT BY 1;
      PLAY SOUND 42;
      v.x = 3.0;
      PRINT v.z;
      PRINT position.x.y;
      PRINT VECTOR2(1, "a");
      SELF = 3;
    }
  }
}
