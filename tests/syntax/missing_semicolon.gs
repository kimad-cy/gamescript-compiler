GAME T {
  ENTITY P {
    PROPERTY health: INTEGER = 100;
    ON START {
      health = health - 20
      PRINT health;
    }
  }
}
