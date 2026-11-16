// Example 3: rules (WIN / LOSE), spawning, entity properties
GAME Coins {

  ENTITY Coin {
    ON COLLISION WITH Player {
      Player.score = Player.score + 10;
      PRINT "coin collected";
      DESTROY SELF;
    }
  }

  ENTITY Player {
    PROPERTY score: INTEGER = 0;
    PROPERTY health: INTEGER = 100;
    ON START {
      Coin.position = VECTOR2(40, 0);
      SPAWN Coin AT VECTOR2(80, 0);
      SPAWN Coin AT VECTOR2(120, 0);
    }
    ON UPDATE {
      MOVE RIGHT BY 4;
      health = health - 1;
    }
  }

  RULE Victory { IF Player.score >= 30 { WIN; } }
  RULE Defeat  { IF Player.health <= 0 { LOSE; } }
}
