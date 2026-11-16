// Coin Chase: collect 4 coins before the Chaser catches you 3 times.
// Controls: arrow keys.  The Chaser speeds up as time passes.
GAME CoinChase {

  // keep a value inside [lo, hi]
  FUNCTION clamp(v: FLOAT, lo: FLOAT, hi: FLOAT): FLOAT {
    IF v < lo { RETURN lo; }
    IF v > hi { RETURN hi; }
    RETURN v;
  }

  ENTITY Coin {
    ON COLLISION WITH Player {
      Player.score = Player.score + 1;
      PRINT "coin collected";
      PLAY SOUND "coin.wav";
      DESTROY SELF;
    }
  }

  ENTITY Chaser {
    PROPERTY speed: FLOAT = 1.0;
    PROPERTY ticks: INTEGER = 0;

    FUNCTION respawn() {
      position = VECTOR2(600, 440);
    }

    ON START { respawn(); }

    ON UPDATE {
      ticks = ticks + 1;
      IF ticks % 300 == 0 {
        speed = speed + 0.25;
        PRINT "the Chaser got faster";
      }
      // walk toward the player, one axis at a time
      IF Player.position.x > position.x { MOVE RIGHT BY speed; }
      IF Player.position.x < position.x { MOVE LEFT BY speed; }
      IF Player.position.y > position.y { MOVE DOWN BY speed; }
      IF Player.position.y < position.y { MOVE UP BY speed; }
    }
  }

  ENTITY Player {
    PROPERTY score: INTEGER = 0;
    PROPERTY lives: INTEGER = 3;
    PROPERTY step: INTEGER = 20;

    FUNCTION keepInside() {
      position = VECTOR2(clamp(position.x, 0, 620), clamp(position.y, 0, 460));
    }

    ON START {
      position = VECTOR2(320, 240);
      Coin.position = VECTOR2(200, 160);
      SPAWN Coin AT VECTOR2(440, 160);
      SPAWN Coin AT VECTOR2(200, 320);
      SPAWN Coin AT VECTOR2(440, 320);
    }

    ON KEY_PRESS "LEFT"  { MOVE LEFT BY step;  keepInside(); }
    ON KEY_PRESS "RIGHT" { MOVE RIGHT BY step; keepInside(); }
    ON KEY_PRESS "UP"    { MOVE UP BY step;    keepInside(); }
    ON KEY_PRESS "DOWN"  { MOVE DOWN BY step;  keepInside(); }

    ON COLLISION WITH Chaser {
      lives = lives - 1;
      PRINT "ouch! lives left:";
      PRINT lives;
      Chaser.respawn();
    }
  }

  RULE Victory { IF Player.score >= 4 { WIN; } }
  RULE Defeat  { IF Player.lives <= 0 { LOSE; } }
}